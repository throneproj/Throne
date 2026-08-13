#include "include/sys/windows/WindowsWfpKillSwitchBackend.h"

#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include <winsock2.h>
#include <windows.h>
#include <fwpmu.h>
#include <netioapi.h>

namespace {

constexpr wchar_t kSubLayerName[] = L"Throne kill switch policy";
constexpr wchar_t kSubLayerDescription[] = L"Blocks direct connections while allowing only controlled bootstrap traffic";
constexpr wchar_t kSessionName[] = L"Throne kill switch runtime allowances";
constexpr wchar_t kSessionDescription[] = L"Dynamic core and TUN allowances; removed when Throne exits";
constexpr std::array<UINT8, 19> kPolicySchemaPrefix = {
    't', 'h', 'r', 'o', 'n', 'e', '-', 'k', 'i', 'l', 'l', '-', 's', 'w', 'i', 't', 'c', 'h', '-',
};
constexpr std::array<UINT8, 20> kPolicySchema = {
    't', 'h', 'r', 'o', 'n', 'e', '-', 'k', 'i', 'l', 'l', '-', 's', 'w', 'i', 't', 'c', 'h', '-', '4',
};
constexpr UINT32 kDhcpV4BroadcastAddress = 0xffffffff;
constexpr std::array<UINT8, FWP_V6_ADDR_SIZE> kDhcpV6ServersMulticastAddress = {
    0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02,
};
constexpr std::array<UINT8, FWP_V6_ADDR_SIZE> kIpv6LinkLocalPrefix = {
    0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// NDP, router solicitation/advertisement, and DAD use link-local-scope
// multicast. Deliberately exclude multicast with broader scopes.
constexpr std::array<UINT8, FWP_V6_ADDR_SIZE> kIpv6LinkLocalMulticastPrefix = {
    0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
constexpr std::array<UINT8, FWP_V6_ADDR_SIZE> kIpv6UnspecifiedAddress = {};
constexpr UINT8 kIpv6LinkLocalPrefixLength = 10;
constexpr UINT8 kIpv6LinkLocalMulticastPrefixLength = 16;

// Throne's Windows build still advertises _WIN32_WINNT=0x0600, which makes
// current SDK headers hide the arrival-interface (Vista SP1+) and next-hop
// interface (Windows 7+) identifiers. Keep the documented keys local instead
// of raising the target for the entire application. Unsupported systems fail
// closed when BFE rejects the dynamic filter.
constexpr GUID kIpArrivalInterfaceConditionKey =
    {0x618a9b6d, 0x386b, 0x4136, {0xad, 0x6e, 0xb5, 0x15, 0x87, 0xcf, 0xb1, 0xcd}};
constexpr GUID kIpNextHopInterfaceConditionKey =
    {0x93ae8f5b, 0x7f6f, 0x4719, {0x98, 0xc8, 0x14, 0xe9, 0x74, 0x29, 0xef, 0x04}};

// Retained only to identify and safely migrate the provider-associated v1-v3
// policy. The v4 policy never creates or references a provider object.
constexpr GUID kLegacyProviderKey =
    {0x3c2bc3c2, 0x3bd8, 0x492b, {0x9d, 0x92, 0xe7, 0xff, 0xf9, 0x7a, 0xce, 0x76}};
constexpr GUID kSubLayerKey =
    {0xecc81b21, 0x2471, 0x41f9, {0xae, 0x7c, 0xab, 0x23, 0x28, 0x77, 0x4f, 0x94}};

constexpr GUID kLoopbackV4FilterKey =
    {0xa4547a78, 0x8878, 0x4215, {0x80, 0x43, 0x93, 0x3a, 0x44, 0x03, 0x52, 0xe4}};
constexpr GUID kLoopbackV6FilterKey =
    {0x48d0e937, 0xf81a, 0x4545, {0xb5, 0xba, 0x93, 0xd7, 0x2c, 0x1a, 0x2e, 0x82}};
constexpr GUID kDhcpV4FilterKey =
    {0x05d67b17, 0x7098, 0x4074, {0x86, 0x76, 0x6e, 0xea, 0xe3, 0x1e, 0x76, 0x8b}};
constexpr GUID kDhcpV6FilterKey =
    {0xbe3e2d94, 0xe626, 0x4404, {0x82, 0x0f, 0x66, 0x2e, 0x55, 0xd1, 0xcd, 0xf2}};
constexpr GUID kIcmpV6ConnectLinkLocalFilterKey =
    {0x213e71d7, 0xe9d6, 0x4e62, {0xb7, 0xbf, 0x06, 0x4f, 0xc8, 0x78, 0xe6, 0x02}};
constexpr GUID kIcmpV6ConnectMulticastFilterKey =
    {0x446f078e, 0x52df, 0x455b, {0xa7, 0x88, 0x92, 0x2b, 0x0c, 0x5e, 0xb7, 0x25}};
constexpr GUID kIcmpV6ReceiveLinkLocalFilterKey =
    {0x0beeea16, 0xaa6d, 0x4685, {0x94, 0x24, 0x55, 0x40, 0x96, 0x9c, 0xfa, 0x67}};
constexpr GUID kIcmpV6ReceiveDadFilterKey =
    {0x4d6209a5, 0x2511, 0x4f4b, {0x9b, 0x35, 0xc2, 0xd2, 0x74, 0xec, 0xa3, 0x8d}};
constexpr GUID kBlockV4FilterKey =
    {0x24741cf9, 0x29a7, 0x4f08, {0x97, 0x8b, 0x9f, 0x03, 0x1c, 0xe3, 0x29, 0x27}};
constexpr GUID kBlockV6FilterKey =
    {0x522c4565, 0x7eca, 0x4933, {0x8b, 0x5b, 0x6d, 0x48, 0x00, 0x29, 0x43, 0x0f}};
constexpr GUID kReceiveLoopbackV4FilterKey =
    {0x0afdfeb1, 0x71a8, 0x4030, {0x88, 0x72, 0x5f, 0x82, 0xac, 0x8e, 0xda, 0x02}};
constexpr GUID kReceiveLoopbackV6FilterKey =
    {0xaf1d5811, 0x8e2e, 0x410d, {0x89, 0x7a, 0xa8, 0x7b, 0x53, 0xcb, 0x48, 0xe5}};
constexpr GUID kReceiveBlockV4FilterKey =
    {0xf77be552, 0xfeac, 0x45dc, {0x98, 0xd3, 0x0a, 0xde, 0x8b, 0x26, 0x76, 0x20}};
constexpr GUID kReceiveBlockV6FilterKey =
    {0xf9612efc, 0x03e2, 0x40a8, {0xaf, 0x6e, 0x9d, 0x67, 0x5a, 0xe9, 0xe6, 0xc8}};

constexpr GUID kCoreV4FilterKey =
    {0xe19b3249, 0xd1d5, 0x4261, {0xae, 0x15, 0xd3, 0xfe, 0x34, 0xae, 0x36, 0x79}};
constexpr GUID kCoreV6FilterKey =
    {0xaced7286, 0x73e0, 0x4282, {0xac, 0x72, 0x66, 0x48, 0xe0, 0xa5, 0xbf, 0x0b}};
constexpr GUID kTunV4FilterKey =
    {0xa45ec644, 0x96b6, 0x4079, {0xb7, 0x8a, 0x2d, 0x99, 0x8d, 0xe9, 0x6e, 0xfc}};
constexpr GUID kTunV6FilterKey =
    {0x257d8918, 0x1116, 0x4883, {0xbb, 0x5c, 0x83, 0x74, 0xf5, 0x62, 0x63, 0xe3}};
constexpr GUID kReceiveTunV4FilterKey =
    {0x4dff6e48, 0xa5ca, 0x40ba, {0xb2, 0xf9, 0xf6, 0xbc, 0x75, 0x94, 0x86, 0x25}};
constexpr GUID kReceiveTunV6FilterKey =
    {0xb7f016d4, 0x25f7, 0x447d, {0xb5, 0xb1, 0x3b, 0xed, 0xda, 0x1a, 0x37, 0x56}};

constexpr std::array<const GUID *, 14> kPersistentFilterKeys = {
    &kLoopbackV4FilterKey,
    &kLoopbackV6FilterKey,
    &kDhcpV4FilterKey,
    &kDhcpV6FilterKey,
    &kIcmpV6ConnectLinkLocalFilterKey,
    &kIcmpV6ConnectMulticastFilterKey,
    &kIcmpV6ReceiveLinkLocalFilterKey,
    &kIcmpV6ReceiveDadFilterKey,
    &kBlockV4FilterKey,
    &kBlockV6FilterKey,
    &kReceiveLoopbackV4FilterKey,
    &kReceiveLoopbackV6FilterKey,
    &kReceiveBlockV4FilterKey,
    &kReceiveBlockV6FilterKey,
};

constexpr std::array<const GUID *, 6> kDynamicFilterKeys = {
    &kCoreV4FilterKey,
    &kCoreV6FilterKey,
    &kTunV4FilterKey,
    &kTunV6FilterKey,
    &kReceiveTunV4FilterKey,
    &kReceiveTunV6FilterKey,
};

constexpr UINT8 kBlockWeight = 0;
constexpr UINT8 kPermitWeight = 15;

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

QString systemErrorMessage(DWORD code)
{
    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                            FORMAT_MESSAGE_FROM_SYSTEM |
                                            FORMAT_MESSAGE_IGNORE_INSERTS,
                                        nullptr,
                                        code,
                                        0,
                                        reinterpret_cast<LPWSTR>(&buffer),
                                        0,
                                        nullptr);
    QString message;
    if (length != 0 && buffer != nullptr) {
        message = QString::fromWCharArray(buffer, static_cast<qsizetype>(length)).trimmed();
    }
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    return message;
}

QString operationError(const QString &operation, DWORD code)
{
    const QString numericCode = QStringLiteral("0x%1").arg(static_cast<qulonglong>(code), 8, 16, QLatin1Char('0'));
    const QString description = systemErrorMessage(code);
    if (description.isEmpty()) {
        return QStringLiteral("%1 failed (%2)").arg(operation, numericCode);
    }
    return QStringLiteral("%1 failed (%2): %3").arg(operation, numericCode, description);
}

wchar_t *mutableText(const wchar_t *text)
{
    // The WFP structures predate const-correct Windows APIs. Fwpm*Add does not
    // modify the supplied display data and copies it before returning.
    return const_cast<wchar_t *>(text);
}

class EngineHandle final
{
public:
    EngineHandle() = default;
    explicit EngineHandle(HANDLE handle) : handle_(handle) {}
    ~EngineHandle() { reset(); }

    EngineHandle(const EngineHandle &) = delete;
    EngineHandle &operator=(const EngineHandle &) = delete;

    EngineHandle(EngineHandle &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    EngineHandle &operator=(EngineHandle &&other) noexcept
    {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] HANDLE get() const { return handle_; }
    [[nodiscard]] bool valid() const { return handle_ != nullptr; }

    DWORD reset(HANDLE handle = nullptr)
    {
        DWORD result = ERROR_SUCCESS;
        if (handle_ != nullptr) {
            result = FwpmEngineClose0(handle_);
        }
        handle_ = handle;
        return result;
    }

private:
    HANDLE handle_ = nullptr;
};

class WfpMemory final
{
public:
    WfpMemory() = default;
    explicit WfpMemory(void *memory) : memory_(memory) {}
    ~WfpMemory() { reset(); }

    WfpMemory(const WfpMemory &) = delete;
    WfpMemory &operator=(const WfpMemory &) = delete;

    WfpMemory(WfpMemory &&other) noexcept : memory_(std::exchange(other.memory_, nullptr)) {}
    WfpMemory &operator=(WfpMemory &&other) noexcept
    {
        if (this != &other) {
            reset();
            memory_ = std::exchange(other.memory_, nullptr);
        }
        return *this;
    }

    void reset(void *memory = nullptr)
    {
        if (memory_ != nullptr) {
            void *toFree = memory_;
            FwpmFreeMemory0(&toFree);
        }
        memory_ = memory;
    }

private:
    void *memory_ = nullptr;
};

class Transaction final
{
public:
    explicit Transaction(HANDLE engine) : engine_(engine) {}
    ~Transaction()
    {
        if (active_) {
            FwpmTransactionAbort0(engine_);
        }
    }

    DWORD begin()
    {
        const DWORD result = FwpmTransactionBegin0(engine_, 0);
        active_ = result == ERROR_SUCCESS;
        return result;
    }

    DWORD commit()
    {
        const DWORD result = FwpmTransactionCommit0(engine_);
        if (result == ERROR_SUCCESS) {
            active_ = false;
        }
        return result;
    }

private:
    HANDLE engine_;
    bool active_ = false;
};

DWORD openEngine(bool dynamic, EngineHandle *engine)
{
    FWPM_SESSION0 session{};
    const FWPM_SESSION0 *sessionPointer = nullptr;
    if (dynamic) {
        session.displayData.name = mutableText(kSessionName);
        session.displayData.description = mutableText(kSessionDescription);
        session.flags = FWPM_SESSION_FLAG_DYNAMIC;
        session.txnWaitTimeoutInMSec = 5000;
        sessionPointer = &session;
    }

    HANDLE handle = nullptr;
    const DWORD result = FwpmEngineOpen0(nullptr, RPC_C_AUTHN_WINNT, nullptr, sessionPointer, &handle);
    if (result == ERROR_SUCCESS) {
        engine->reset(handle);
    }
    return result;
}

enum class Presence
{
    Missing,
    Present,
    Failed,
};

template<typename Object, typename Getter, typename Validator>
Presence getObjectPresence(HANDLE engine,
                           const GUID &key,
                           Getter getter,
                           Validator validator,
                           bool *matchesSchema,
                           DWORD notFound,
                           QString *error,
                           const QString &name)
{
    Object *object = nullptr;
    const DWORD result = getter(engine, &key, &object);
    WfpMemory memory(object);
    if (result == ERROR_SUCCESS) {
        if (matchesSchema != nullptr) {
            *matchesSchema = object != nullptr && validator(*object);
        }
        return Presence::Present;
    }
    if (result == notFound) {
        return Presence::Missing;
    }
    setError(error, operationError(QStringLiteral("Query %1").arg(name), result));
    return Presence::Failed;
}

bool equalGuid(const GUID &left, const GUID &right)
{
    return InlineIsEqualGUID(left, right) != FALSE;
}

bool blobMatchesCurrentSchema(const FWP_BYTE_BLOB &data)
{
    return data.size == kPolicySchema.size() && data.data != nullptr &&
           std::equal(kPolicySchema.begin(), kPolicySchema.end(), data.data);
}

void setCurrentSchema(FWP_BYTE_BLOB *data)
{
    data->size = static_cast<UINT32>(kPolicySchema.size());
    data->data = const_cast<UINT8 *>(kPolicySchema.data());
}

bool providerHasKnownLegacyThroneOwnershipMarker(const FWPM_PROVIDER0 &provider)
{
    // DISABLED is a read-only status bit which BFE may add when returning an
    // older provider after service initialization. It cannot be supplied by an
    // object creator, so accepting it does not broaden the ownership marker.
    constexpr UINT32 allowedFlags = FWPM_PROVIDER_FLAG_PERSISTENT |
                                    FWPM_PROVIDER_FLAG_DISABLED;
    if ((provider.flags & FWPM_PROVIDER_FLAG_PERSISTENT) == 0 ||
        (provider.flags & ~allowedFlags) != 0 || provider.serviceName != nullptr ||
        provider.providerData.size != kPolicySchema.size() || provider.providerData.data == nullptr ||
        !std::equal(kPolicySchemaPrefix.begin(),
                    kPolicySchemaPrefix.end(),
                    provider.providerData.data)) {
        return false;
    }

    // Recognizing only this closed set permits safe in-place migration without
    // treating an arbitrary deterministic-GUID collision as ours.
    const UINT8 version = provider.providerData.data[kPolicySchemaPrefix.size()];
    return version == '1' || version == '2' || version == '3';
}

enum class MutationOwnership
{
    Empty,
    OwnedByThrone,
    Foreign,
    Error,
};

struct MutationOwnershipResult
{
    MutationOwnership ownership = MutationOwnership::Error;
    QString detail;
};

MutationOwnershipResult verifyMutationOwnership(HANDLE engine)
{
    bool anyObjectPresent = false;
    bool legacyProviderPresent = false;

    FWPM_PROVIDER0 *provider = nullptr;
    DWORD result = FwpmProviderGetByKey0(engine, &kLegacyProviderKey, &provider);
    WfpMemory providerMemory(provider);
    if (result == ERROR_SUCCESS) {
        anyObjectPresent = true;
        legacyProviderPresent = provider != nullptr &&
                                providerHasKnownLegacyThroneOwnershipMarker(*provider);
        if (!legacyProviderPresent) {
            return {
                MutationOwnership::Foreign,
                QStringLiteral("Refusing to modify the deterministic legacy WFP provider because it lacks a known Throne v1-v3 ownership marker"),
            };
        }
    } else if (result != FWP_E_PROVIDER_NOT_FOUND) {
        return {
            MutationOwnership::Error,
            operationError(QStringLiteral("Verify ownership of the legacy Throne WFP provider"), result),
        };
    }

    const auto legacyObjectDataIsEmpty = [](const FWP_BYTE_BLOB &data) {
        // RPC unmarshalling is allowed to return an arbitrary pointer for a
        // zero-length blob. The legacy v1-v3 objects never carried object
        // data; ownership is instead proven by their marked provider link.
        return data.size == 0;
    };

    FWPM_SUBLAYER0 *subLayer = nullptr;
    result = FwpmSubLayerGetByKey0(engine, &kSubLayerKey, &subLayer);
    WfpMemory subLayerMemory(subLayer);
    if (result == ERROR_SUCCESS) {
        anyObjectPresent = true;
        const bool owned = subLayer != nullptr &&
                           (legacyProviderPresent
                                ? (subLayer->providerKey != nullptr &&
                                   equalGuid(*subLayer->providerKey, kLegacyProviderKey) &&
                                   legacyObjectDataIsEmpty(subLayer->providerData))
                                : (subLayer->providerKey == nullptr &&
                                   blobMatchesCurrentSchema(subLayer->providerData)));
        if (!owned) {
            return {
                MutationOwnership::Foreign,
                QStringLiteral("Refusing to modify the deterministic WFP sublayer because neither its v4 object marker nor a marked legacy provider proves Throne ownership"),
            };
        }
    } else if (result != FWP_E_SUBLAYER_NOT_FOUND) {
        return {
            MutationOwnership::Error,
            operationError(QStringLiteral("Verify ownership of the Throne WFP sublayer"), result),
        };
    }

    const auto verifyFilter = [&](const GUID &key) -> MutationOwnershipResult {
        FWPM_FILTER0 *filter = nullptr;
        const DWORD filterResult = FwpmFilterGetByKey0(engine, &key, &filter);
        WfpMemory filterMemory(filter);
        if (filterResult == FWP_E_FILTER_NOT_FOUND) {
            return {MutationOwnership::Empty, {}};
        }
        if (filterResult != ERROR_SUCCESS) {
            return {
                MutationOwnership::Error,
                operationError(QStringLiteral("Verify ownership of a deterministic Throne WFP filter"),
                               filterResult),
            };
        }

        anyObjectPresent = true;
        const bool owned = filter != nullptr && equalGuid(filter->subLayerKey, kSubLayerKey) &&
                           (legacyProviderPresent
                                ? (filter->providerKey != nullptr &&
                                   equalGuid(*filter->providerKey, kLegacyProviderKey) &&
                                   legacyObjectDataIsEmpty(filter->providerData))
                                : (filter->providerKey == nullptr &&
                                   blobMatchesCurrentSchema(filter->providerData)));
        if (!owned) {
            return {
                MutationOwnership::Foreign,
                QStringLiteral("Refusing to modify a deterministic WFP filter because neither its v4 object marker nor a marked legacy provider proves Throne ownership"),
            };
        }
        return {MutationOwnership::OwnedByThrone, {}};
    };

    for (const GUID *key : kPersistentFilterKeys) {
        const MutationOwnershipResult filterOwnership = verifyFilter(*key);
        if (filterOwnership.ownership == MutationOwnership::Foreign ||
            filterOwnership.ownership == MutationOwnership::Error) {
            return filterOwnership;
        }
    }
    for (const GUID *key : kDynamicFilterKeys) {
        const MutationOwnershipResult filterOwnership = verifyFilter(*key);
        if (filterOwnership.ownership == MutationOwnership::Foreign ||
            filterOwnership.ownership == MutationOwnership::Error) {
            return filterOwnership;
        }
    }

    return {anyObjectPresent ? MutationOwnership::OwnedByThrone
                             : MutationOwnership::Empty,
            {}};
}

bool subLayerMatchesSchema(const FWPM_SUBLAYER0 &subLayer)
{
    return subLayer.flags == FWPM_SUBLAYER_FLAG_PERSISTENT &&
           subLayer.providerKey == nullptr &&
           blobMatchesCurrentSchema(subLayer.providerData) &&
           subLayer.weight == 0xffff;
}

bool conditionMatches(const FWPM_FILTER_CONDITION0 &condition,
                      const GUID &field,
                      FWP_DATA_TYPE type,
                      UINT32 value)
{
    if (!equalGuid(condition.fieldKey, field) || condition.matchType != FWP_MATCH_EQUAL ||
        condition.conditionValue.type != type) {
        return false;
    }
    switch (type) {
    case FWP_UINT8:
        return condition.conditionValue.uint8 == value;
    case FWP_UINT16:
        return condition.conditionValue.uint16 == value;
    case FWP_UINT32:
        return condition.conditionValue.uint32 == value;
    default:
        return false;
    }
}

bool ipv6MaskConditionMatches(const FWPM_FILTER_CONDITION0 &condition,
                              const GUID &field,
                              const std::array<UINT8, FWP_V6_ADDR_SIZE> &address,
                              UINT8 prefixLength)
{
    return equalGuid(condition.fieldKey, field) &&
           condition.matchType == FWP_MATCH_EQUAL &&
           condition.conditionValue.type == FWP_V6_ADDR_MASK &&
           condition.conditionValue.v6AddrMask != nullptr &&
           condition.conditionValue.v6AddrMask->prefixLength == prefixLength &&
           std::equal(address.begin(),
                      address.end(),
                      condition.conditionValue.v6AddrMask->addr);
}

bool commonFilterMatches(const FWPM_FILTER0 &filter,
                         const GUID &layer,
                         FWP_ACTION_TYPE action,
                         UINT8 weight,
                         UINT32 expectedFlags)
{
    return filter.flags == expectedFlags &&
           filter.providerKey == nullptr && blobMatchesCurrentSchema(filter.providerData) &&
           equalGuid(filter.layerKey, layer) && equalGuid(filter.subLayerKey, kSubLayerKey) &&
           filter.weight.type == FWP_UINT8 && filter.weight.uint8 == weight &&
           filter.action.type == action;
}

const GUID &tunInterfaceCondition(bool receive)
{
    // Match the actual directional interface, not merely the interface owning
    // the selected local address. NEXTHOP is the last interface an outbound
    // packet traverses after weak-host/forwarding decisions; ARRIVAL is the
    // interface on which inbound traffic entered. Thus neither direction can
    // use a physical interface while satisfying a TUN allowance.
    return receive ? kIpArrivalInterfaceConditionKey
                   : kIpNextHopInterfaceConditionKey;
}

bool persistentFilterMatchesSchema(const GUID &key, const FWPM_FILTER0 &filter)
{
    const bool receive = equalGuid(key, kReceiveLoopbackV4FilterKey) ||
                         equalGuid(key, kReceiveLoopbackV6FilterKey) ||
                         equalGuid(key, kIcmpV6ReceiveLinkLocalFilterKey) ||
                         equalGuid(key, kIcmpV6ReceiveDadFilterKey) ||
                         equalGuid(key, kReceiveBlockV4FilterKey) ||
                         equalGuid(key, kReceiveBlockV6FilterKey);
    const bool ipv6 = equalGuid(key, kLoopbackV6FilterKey) ||
                      equalGuid(key, kDhcpV6FilterKey) ||
                      equalGuid(key, kBlockV6FilterKey) ||
                      equalGuid(key, kIcmpV6ConnectLinkLocalFilterKey) ||
                      equalGuid(key, kIcmpV6ConnectMulticastFilterKey) ||
                      equalGuid(key, kIcmpV6ReceiveLinkLocalFilterKey) ||
                      equalGuid(key, kIcmpV6ReceiveDadFilterKey) ||
                      equalGuid(key, kReceiveLoopbackV6FilterKey) ||
                      equalGuid(key, kReceiveBlockV6FilterKey);
    const bool block = equalGuid(key, kBlockV4FilterKey) ||
                       equalGuid(key, kBlockV6FilterKey) ||
                       equalGuid(key, kReceiveBlockV4FilterKey) ||
                       equalGuid(key, kReceiveBlockV6FilterKey);
    const bool loopback = equalGuid(key, kLoopbackV4FilterKey) ||
                          equalGuid(key, kLoopbackV6FilterKey) ||
                          equalGuid(key, kReceiveLoopbackV4FilterKey) ||
                          equalGuid(key, kReceiveLoopbackV6FilterKey);
    const bool dhcp = equalGuid(key, kDhcpV4FilterKey) || equalGuid(key, kDhcpV6FilterKey);
    const bool icmpV6 = equalGuid(key, kIcmpV6ConnectLinkLocalFilterKey) ||
                        equalGuid(key, kIcmpV6ConnectMulticastFilterKey) ||
                        equalGuid(key, kIcmpV6ReceiveLinkLocalFilterKey) ||
                        equalGuid(key, kIcmpV6ReceiveDadFilterKey);
    if (!block && !loopback && !dhcp && !icmpV6) {
        return false;
    }

    const GUID &layer = receive
                            ? (ipv6 ? FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6
                                    : FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4)
                            : (ipv6 ? FWPM_LAYER_ALE_AUTH_CONNECT_V6
                                    : FWPM_LAYER_ALE_AUTH_CONNECT_V4);

    if (block) {
        return commonFilterMatches(filter,
                                   layer,
                                   FWP_ACTION_BLOCK,
                                   kBlockWeight,
                                   FWPM_FILTER_FLAG_PERSISTENT | FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT) &&
               filter.numFilterConditions == 0;
    }

    if (!commonFilterMatches(filter,
                             layer,
                             FWP_ACTION_PERMIT,
                             kPermitWeight,
                             FWPM_FILTER_FLAG_PERSISTENT)) {
        return false;
    }

    if (loopback) {
        if (filter.numFilterConditions != 1 || filter.filterCondition == nullptr) {
            return false;
        }
        const FWPM_FILTER_CONDITION0 &condition = filter.filterCondition[0];
        return equalGuid(condition.fieldKey, FWPM_CONDITION_FLAGS) &&
               condition.matchType == FWP_MATCH_FLAGS_ALL_SET &&
               condition.conditionValue.type == FWP_UINT32 &&
               condition.conditionValue.uint32 == FWP_CONDITION_FLAG_IS_LOOPBACK;
    }

    if (icmpV6) {
        const bool dad = equalGuid(key, kIcmpV6ReceiveDadFilterKey);
        const UINT32 expectedConditionCount = dad ? 4 : 2;
        if (filter.numFilterConditions != expectedConditionCount ||
            filter.filterCondition == nullptr) {
            return false;
        }

        const bool multicast = equalGuid(key, kIcmpV6ConnectMulticastFilterKey);
        bool protocolFound = false;
        bool remoteAddressFound = false;
        bool localAddressFound = !dad;
        bool icmpTypeFound = !dad;
        for (UINT32 index = 0; index < filter.numFilterConditions; ++index) {
            const FWPM_FILTER_CONDITION0 &condition = filter.filterCondition[index];
            protocolFound |= conditionMatches(condition,
                                              FWPM_CONDITION_IP_PROTOCOL,
                                              FWP_UINT8,
                                              IPPROTO_ICMPV6);
            if (dad) {
                remoteAddressFound |= ipv6MaskConditionMatches(condition,
                                                               FWPM_CONDITION_IP_REMOTE_ADDRESS,
                                                               kIpv6UnspecifiedAddress,
                                                               128);
                localAddressFound |= ipv6MaskConditionMatches(condition,
                                                              FWPM_CONDITION_IP_LOCAL_ADDRESS,
                                                              kIpv6LinkLocalMulticastPrefix,
                                                              kIpv6LinkLocalMulticastPrefixLength);
                icmpTypeFound |= conditionMatches(condition,
                                                  FWPM_CONDITION_ICMP_TYPE,
                                                  FWP_UINT16,
                                                  135);
            } else {
                const auto &address = multicast ? kIpv6LinkLocalMulticastPrefix
                                                : kIpv6LinkLocalPrefix;
                const UINT8 prefixLength = multicast ? kIpv6LinkLocalMulticastPrefixLength
                                                     : kIpv6LinkLocalPrefixLength;
                remoteAddressFound |= ipv6MaskConditionMatches(condition,
                                                               FWPM_CONDITION_IP_REMOTE_ADDRESS,
                                                               address,
                                                               prefixLength);
            }
        }
        return protocolFound && remoteAddressFound && localAddressFound &&
               icmpTypeFound;
    }

    if (filter.numFilterConditions != 4 || filter.filterCondition == nullptr) {
        return false;
    }
    const UINT16 localPort = ipv6 ? 546 : 68;
    const UINT16 remotePort = ipv6 ? 547 : 67;
    bool protocolFound = false;
    bool localPortFound = false;
    bool remotePortFound = false;
    bool remoteAddressFound = false;
    for (UINT32 index = 0; index < filter.numFilterConditions; ++index) {
        const FWPM_FILTER_CONDITION0 &condition = filter.filterCondition[index];
        protocolFound |= conditionMatches(condition, FWPM_CONDITION_IP_PROTOCOL, FWP_UINT8, IPPROTO_UDP);
        localPortFound |= conditionMatches(condition, FWPM_CONDITION_IP_LOCAL_PORT, FWP_UINT16, localPort);
        remotePortFound |= conditionMatches(condition, FWPM_CONDITION_IP_REMOTE_PORT, FWP_UINT16, remotePort);
        if (ipv6) {
            remoteAddressFound |= equalGuid(condition.fieldKey, FWPM_CONDITION_IP_REMOTE_ADDRESS) &&
                                  condition.matchType == FWP_MATCH_EQUAL &&
                                  condition.conditionValue.type == FWP_BYTE_ARRAY16_TYPE &&
                                  condition.conditionValue.byteArray16 != nullptr &&
                                  std::equal(kDhcpV6ServersMulticastAddress.begin(),
                                             kDhcpV6ServersMulticastAddress.end(),
                                             condition.conditionValue.byteArray16->byteArray16);
        } else {
            remoteAddressFound |= conditionMatches(condition,
                                                   FWPM_CONDITION_IP_REMOTE_ADDRESS,
                                                   FWP_UINT32,
                                                   kDhcpV4BroadcastAddress);
        }
    }
    return protocolFound && localPortFound && remotePortFound && remoteAddressFound;
}

bool dynamicFilterMatchesSchema(const GUID &key, const FWPM_FILTER0 &filter)
{
    const bool core = equalGuid(key, kCoreV4FilterKey) ||
                      equalGuid(key, kCoreV6FilterKey);
    const bool tun = equalGuid(key, kTunV4FilterKey) ||
                     equalGuid(key, kTunV6FilterKey) ||
                     equalGuid(key, kReceiveTunV4FilterKey) ||
                     equalGuid(key, kReceiveTunV6FilterKey);
    if (!core && !tun) {
        return false;
    }

    const bool receive = equalGuid(key, kReceiveTunV4FilterKey) ||
                         equalGuid(key, kReceiveTunV6FilterKey);
    const bool ipv6 = equalGuid(key, kCoreV6FilterKey) ||
                      equalGuid(key, kTunV6FilterKey) ||
                      equalGuid(key, kReceiveTunV6FilterKey);
    const GUID &layer = receive
                            ? (ipv6 ? FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6
                                    : FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4)
                            : (ipv6 ? FWPM_LAYER_ALE_AUTH_CONNECT_V6
                                    : FWPM_LAYER_ALE_AUTH_CONNECT_V4);
    if (!commonFilterMatches(filter,
                             layer,
                             FWP_ACTION_PERMIT,
                             kPermitWeight,
                             0) ||
        filter.numFilterConditions != 1 || filter.filterCondition == nullptr) {
        return false;
    }

    const FWPM_FILTER_CONDITION0 &condition = filter.filterCondition[0];
    if (core) {
        return equalGuid(condition.fieldKey, FWPM_CONDITION_ALE_APP_ID) &&
               condition.matchType == FWP_MATCH_EQUAL &&
               condition.conditionValue.type == FWP_BYTE_BLOB_TYPE &&
               condition.conditionValue.byteBlob != nullptr &&
               condition.conditionValue.byteBlob->size != 0 &&
               condition.conditionValue.byteBlob->data != nullptr;
    }
    return equalGuid(condition.fieldKey, tunInterfaceCondition(receive)) &&
           condition.matchType == FWP_MATCH_EQUAL &&
           condition.conditionValue.type == FWP_UINT64 &&
           condition.conditionValue.uint64 != nullptr;
}

DWORD deleteFilterIfPresent(HANDLE engine, const GUID &key)
{
    const DWORD result = FwpmFilterDeleteByKey0(engine, &key);
    return result == FWP_E_FILTER_NOT_FOUND ? ERROR_SUCCESS : result;
}

DWORD deleteSubLayerIfPresent(HANDLE engine)
{
    const DWORD result = FwpmSubLayerDeleteByKey0(engine, &kSubLayerKey);
    return result == FWP_E_SUBLAYER_NOT_FOUND ? ERROR_SUCCESS : result;
}

DWORD deleteLegacyProviderIfPresent(HANDLE engine)
{
    const DWORD result = FwpmProviderDeleteByKey0(engine, &kLegacyProviderKey);
    return result == FWP_E_PROVIDER_NOT_FOUND ? ERROR_SUCCESS : result;
}

DWORD deletePersistentObjects(HANDLE engine, QString *operation)
{
    // Dynamic filters normally disappear with their owning WFP session. Delete
    // our exact keys as well so a legacy/non-dynamic implementation cannot
    // leave an exception behind during startup recovery or explicit disable.
    for (const GUID *key : kDynamicFilterKeys) {
        const DWORD result = deleteFilterIfPresent(engine, *key);
        if (result != ERROR_SUCCESS) {
            if (operation != nullptr) {
                *operation = QStringLiteral("Delete a stale Throne dynamic filter");
            }
            return result;
        }
    }
    for (const GUID *key : kPersistentFilterKeys) {
        const DWORD result = deleteFilterIfPresent(engine, *key);
        if (result != ERROR_SUCCESS) {
            if (operation != nullptr) {
                *operation = QStringLiteral("Delete a Throne persistent filter");
            }
            return result;
        }
    }

    DWORD result = deleteSubLayerIfPresent(engine);
    if (result != ERROR_SUCCESS) {
        if (operation != nullptr) {
            *operation = QStringLiteral("Delete the Throne WFP sublayer");
        }
        return result;
    }

    result = deleteLegacyProviderIfPresent(engine);
    if (result != ERROR_SUCCESS && operation != nullptr) {
        *operation = QStringLiteral("Delete the marked legacy Throne WFP provider");
    }
    return result;
}

DWORD addSubLayer(HANDLE engine, QString *operation)
{
    FWPM_SUBLAYER0 subLayer{};
    subLayer.subLayerKey = kSubLayerKey;
    subLayer.displayData.name = mutableText(kSubLayerName);
    subLayer.displayData.description = mutableText(kSubLayerDescription);
    subLayer.flags = FWPM_SUBLAYER_FLAG_PERSISTENT;
    // Providerless persistent objects are explicitly restored by BFE after a
    // service restart. Association with an ordinary provider would instead
    // require a serviceName before Windows re-enumerates the provider's policy.
    setCurrentSchema(&subLayer.providerData);
    // Highest ordinary sublayer weight. Windows security policy may still use
    // its reserved higher range and our soft permits cannot override its blocks.
    subLayer.weight = 0xffff;

    const DWORD result = FwpmSubLayerAdd0(engine, &subLayer, nullptr);
    if (result != ERROR_SUCCESS && operation != nullptr) {
        *operation = QStringLiteral("Add the Throne WFP sublayer");
    }
    return result;
}

FWPM_FILTER0 makeFilter(const GUID &key,
                        const GUID &layer,
                        const wchar_t *name,
                        FWP_ACTION_TYPE action,
                        UINT8 weight,
                        bool persistent,
                        FWPM_FILTER_CONDITION0 *conditions = nullptr,
                        UINT32 conditionCount = 0)
{
    FWPM_FILTER0 filter{};
    filter.filterKey = key;
    filter.displayData.name = mutableText(name);
    filter.flags = persistent ? FWPM_FILTER_FLAG_PERSISTENT : 0;
    // Each deterministic object carries its own ownership marker. This keeps
    // both persistent and dynamic filters providerless and makes exact-object
    // validation possible before any repair or cleanup mutation.
    setCurrentSchema(&filter.providerData);
    filter.layerKey = layer;
    filter.subLayerKey = kSubLayerKey;
    filter.weight.type = FWP_UINT8;
    filter.weight.uint8 = weight;
    filter.numFilterConditions = conditionCount;
    filter.filterCondition = conditions;
    filter.action.type = action;
    return filter;
}

DWORD addFilter(HANDLE engine, FWPM_FILTER0 *filter, QString *operation)
{
    const DWORD result = FwpmFilterAdd0(engine, filter, nullptr, nullptr);
    if (result != ERROR_SUCCESS && operation != nullptr) {
        *operation = QStringLiteral("Add WFP filter '%1'").arg(QString::fromWCharArray(filter->displayData.name));
    }
    return result;
}

DWORD addLoopbackFilter(HANDLE engine, bool ipv6, bool receive, QString *operation)
{
    FWPM_FILTER_CONDITION0 condition{};
    condition.fieldKey = FWPM_CONDITION_FLAGS;
    condition.matchType = FWP_MATCH_FLAGS_ALL_SET;
    condition.conditionValue.type = FWP_UINT32;
    condition.conditionValue.uint32 = FWP_CONDITION_FLAG_IS_LOOPBACK;

    const GUID &key = receive
                          ? (ipv6 ? kReceiveLoopbackV6FilterKey : kReceiveLoopbackV4FilterKey)
                          : (ipv6 ? kLoopbackV6FilterKey : kLoopbackV4FilterKey);
    const GUID &layer = receive
                            ? (ipv6 ? FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6
                                    : FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4)
                            : (ipv6 ? FWPM_LAYER_ALE_AUTH_CONNECT_V6
                                    : FWPM_LAYER_ALE_AUTH_CONNECT_V4);
    const wchar_t *name = receive
                              ? (ipv6 ? L"Throne permit received IPv6 loopback"
                                      : L"Throne permit received IPv4 loopback")
                              : (ipv6 ? L"Throne permit IPv6 loopback"
                                      : L"Throne permit IPv4 loopback");
    FWPM_FILTER0 filter = makeFilter(key,
                                    layer,
                                    name,
                                    FWP_ACTION_PERMIT,
                                    kPermitWeight,
                                    true,
                                    &condition,
                                    1);
    return addFilter(engine, &filter, operation);
}

DWORD addDhcpFilter(HANDLE engine, bool ipv6, QString *operation)
{
    std::array<FWPM_FILTER_CONDITION0, 4> conditions{};
    conditions[0].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
    conditions[0].matchType = FWP_MATCH_EQUAL;
    conditions[0].conditionValue.type = FWP_UINT8;
    conditions[0].conditionValue.uint8 = IPPROTO_UDP;

    conditions[1].fieldKey = FWPM_CONDITION_IP_LOCAL_PORT;
    conditions[1].matchType = FWP_MATCH_EQUAL;
    conditions[1].conditionValue.type = FWP_UINT16;
    conditions[1].conditionValue.uint16 = ipv6 ? 546 : 68;

    conditions[2].fieldKey = FWPM_CONDITION_IP_REMOTE_PORT;
    conditions[2].matchType = FWP_MATCH_EQUAL;
    conditions[2].conditionValue.type = FWP_UINT16;
    conditions[2].conditionValue.uint16 = ipv6 ? 547 : 67;

    // Keep address configuration working without creating a generic UDP
    // escape hatch. DHCPv4 is limited to the all-hosts broadcast, and DHCPv6
    // to the link-local All_DHCP_Relay_Agents_and_Servers multicast group.
    // ALE gives multicast/broadcast request-response state a short lifetime
    // (four seconds by default), enough for the replies. Unicast lease renewal
    // remains blocked and Windows can fall back to broadcast/multicast rebind.
    FWP_BYTE_ARRAY16 dhcpV6Address{};
    conditions[3].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    conditions[3].matchType = FWP_MATCH_EQUAL;
    if (ipv6) {
        std::copy(kDhcpV6ServersMulticastAddress.begin(),
                  kDhcpV6ServersMulticastAddress.end(),
                  dhcpV6Address.byteArray16);
        conditions[3].conditionValue.type = FWP_BYTE_ARRAY16_TYPE;
        conditions[3].conditionValue.byteArray16 = &dhcpV6Address;
    } else {
        conditions[3].conditionValue.type = FWP_UINT32;
        conditions[3].conditionValue.uint32 = kDhcpV4BroadcastAddress;
    }

    FWPM_FILTER0 filter = makeFilter(ipv6 ? kDhcpV6FilterKey : kDhcpV4FilterKey,
                                    ipv6 ? FWPM_LAYER_ALE_AUTH_CONNECT_V6 : FWPM_LAYER_ALE_AUTH_CONNECT_V4,
                                    ipv6 ? L"Throne permit DHCPv6 client" : L"Throne permit DHCPv4 client",
                                    FWP_ACTION_PERMIT,
                                    kPermitWeight,
                                    true,
                                    conditions.data(),
                                    static_cast<UINT32>(conditions.size()));
    return addFilter(engine, &filter, operation);
}

enum class IcmpV6ControlPlanePermit
{
    ConnectLinkLocal,
    ConnectMulticast,
    ReceiveLinkLocal,
    ReceiveDad,
};

DWORD addIcmpV6ControlPlaneFilter(HANDLE engine,
                                  IcmpV6ControlPlanePermit permit,
                                  QString *operation)
{
    const bool receive = permit == IcmpV6ControlPlanePermit::ReceiveLinkLocal ||
                         permit == IcmpV6ControlPlanePermit::ReceiveDad;
    const bool dad = permit == IcmpV6ControlPlanePermit::ReceiveDad;
    const bool multicast = permit == IcmpV6ControlPlanePermit::ConnectMulticast;

    std::array<FWPM_FILTER_CONDITION0, 4> conditions{};
    conditions[0].fieldKey = FWPM_CONDITION_IP_PROTOCOL;
    conditions[0].matchType = FWP_MATCH_EQUAL;
    conditions[0].conditionValue.type = FWP_UINT8;
    conditions[0].conditionValue.uint8 = IPPROTO_ICMPV6;

    FWP_V6_ADDR_AND_MASK remoteAddress{};
    const auto &remotePrefix = dad ? kIpv6UnspecifiedAddress
                                   : (multicast ? kIpv6LinkLocalMulticastPrefix
                                                : kIpv6LinkLocalPrefix);
    std::copy(remotePrefix.begin(), remotePrefix.end(), remoteAddress.addr);
    remoteAddress.prefixLength = dad ? 128
                                     : (multicast ? kIpv6LinkLocalMulticastPrefixLength
                                                  : kIpv6LinkLocalPrefixLength);
    conditions[1].fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
    conditions[1].matchType = FWP_MATCH_EQUAL;
    conditions[1].conditionValue.type = FWP_V6_ADDR_MASK;
    conditions[1].conditionValue.v6AddrMask = &remoteAddress;

    UINT32 conditionCount = 2;
    FWP_V6_ADDR_AND_MASK localMulticastAddress{};
    if (dad) {
        // Duplicate Address Detection is the only accepted unspecified-source
        // receive case: Neighbor Solicitation (type 135) to IPv6 multicast.
        std::copy(kIpv6LinkLocalMulticastPrefix.begin(),
                  kIpv6LinkLocalMulticastPrefix.end(),
                  localMulticastAddress.addr);
        localMulticastAddress.prefixLength = kIpv6LinkLocalMulticastPrefixLength;
        conditions[2].fieldKey = FWPM_CONDITION_IP_LOCAL_ADDRESS;
        conditions[2].matchType = FWP_MATCH_EQUAL;
        conditions[2].conditionValue.type = FWP_V6_ADDR_MASK;
        conditions[2].conditionValue.v6AddrMask = &localMulticastAddress;

        conditions[3].fieldKey = FWPM_CONDITION_ICMP_TYPE;
        conditions[3].matchType = FWP_MATCH_EQUAL;
        conditions[3].conditionValue.type = FWP_UINT16;
        conditions[3].conditionValue.uint16 = 135;
        conditionCount = 4;
    }

    const GUID *key = nullptr;
    const wchar_t *name = nullptr;
    switch (permit) {
    case IcmpV6ControlPlanePermit::ConnectLinkLocal:
        key = &kIcmpV6ConnectLinkLocalFilterKey;
        name = L"Throne permit link-local ICMPv6 control traffic";
        break;
    case IcmpV6ControlPlanePermit::ConnectMulticast:
        key = &kIcmpV6ConnectMulticastFilterKey;
        name = L"Throne permit multicast ICMPv6 control traffic";
        break;
    case IcmpV6ControlPlanePermit::ReceiveLinkLocal:
        key = &kIcmpV6ReceiveLinkLocalFilterKey;
        name = L"Throne permit received link-local ICMPv6 control traffic";
        break;
    case IcmpV6ControlPlanePermit::ReceiveDad:
        key = &kIcmpV6ReceiveDadFilterKey;
        name = L"Throne permit received ICMPv6 duplicate-address detection";
        break;
    }

    FWPM_FILTER0 filter = makeFilter(*key,
                                    receive ? FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6
                                            : FWPM_LAYER_ALE_AUTH_CONNECT_V6,
                                    name,
                                    FWP_ACTION_PERMIT,
                                    kPermitWeight,
                                    true,
                                    conditions.data(),
                                    conditionCount);
    return addFilter(engine, &filter, operation);
}

DWORD addBlockFilter(HANDLE engine, bool ipv6, bool receive, QString *operation)
{
    const GUID &key = receive
                          ? (ipv6 ? kReceiveBlockV6FilterKey : kReceiveBlockV4FilterKey)
                          : (ipv6 ? kBlockV6FilterKey : kBlockV4FilterKey);
    const GUID &layer = receive
                            ? (ipv6 ? FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6
                                    : FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4)
                            : (ipv6 ? FWPM_LAYER_ALE_AUTH_CONNECT_V6
                                    : FWPM_LAYER_ALE_AUTH_CONNECT_V4);
    const wchar_t *name = receive
                              ? (ipv6 ? L"Throne block received direct IPv6"
                                      : L"Throne block received direct IPv4")
                              : (ipv6 ? L"Throne block direct IPv6"
                                      : L"Throne block direct IPv4");
    FWPM_FILTER0 filter = makeFilter(key,
                                    layer,
                                    name,
                                    FWP_ACTION_BLOCK,
                                    kBlockWeight,
                                    true);
    // A hard block prevents lower-priority providers from changing the action.
    filter.flags |= FWPM_FILTER_FLAG_CLEAR_ACTION_RIGHT;
    return addFilter(engine, &filter, operation);
}

DWORD addPersistentFilters(HANDLE engine, QString *operation)
{
    for (const bool ipv6 : {false, true}) {
        DWORD result = addLoopbackFilter(engine, ipv6, false, operation);
        if (result != ERROR_SUCCESS) {
            return result;
        }
        result = addDhcpFilter(engine, ipv6, operation);
        if (result != ERROR_SUCCESS) {
            return result;
        }
        if (ipv6) {
            result = addIcmpV6ControlPlaneFilter(
                engine, IcmpV6ControlPlanePermit::ConnectLinkLocal, operation);
            if (result != ERROR_SUCCESS) {
                return result;
            }
            result = addIcmpV6ControlPlaneFilter(
                engine, IcmpV6ControlPlanePermit::ConnectMulticast, operation);
            if (result != ERROR_SUCCESS) {
                return result;
            }
        }
        result = addBlockFilter(engine, ipv6, false, operation);
        if (result != ERROR_SUCCESS) {
            return result;
        }
        result = addLoopbackFilter(engine, ipv6, true, operation);
        if (result != ERROR_SUCCESS) {
            return result;
        }
        if (ipv6) {
            result = addIcmpV6ControlPlaneFilter(
                engine, IcmpV6ControlPlanePermit::ReceiveLinkLocal, operation);
            if (result != ERROR_SUCCESS) {
                return result;
            }
            result = addIcmpV6ControlPlaneFilter(
                engine, IcmpV6ControlPlanePermit::ReceiveDad, operation);
            if (result != ERROR_SUCCESS) {
                return result;
            }
        }
        result = addBlockFilter(engine, ipv6, true, operation);
        if (result != ERROR_SUCCESS) {
            return result;
        }
    }
    return ERROR_SUCCESS;
}

DWORD addCoreFilter(HANDLE engine, bool ipv6, FWP_BYTE_BLOB *appId, QString *operation)
{
    FWPM_FILTER_CONDITION0 condition{};
    condition.fieldKey = FWPM_CONDITION_ALE_APP_ID;
    condition.matchType = FWP_MATCH_EQUAL;
    condition.conditionValue.type = FWP_BYTE_BLOB_TYPE;
    condition.conditionValue.byteBlob = appId;

    FWPM_FILTER0 filter = makeFilter(ipv6 ? kCoreV6FilterKey : kCoreV4FilterKey,
                                    ipv6 ? FWPM_LAYER_ALE_AUTH_CONNECT_V6 : FWPM_LAYER_ALE_AUTH_CONNECT_V4,
                                    ipv6 ? L"Throne permit core IPv6" : L"Throne permit core IPv4",
                                    FWP_ACTION_PERMIT,
                                    kPermitWeight,
                                    false,
                                    &condition,
                                    1);
    return addFilter(engine, &filter, operation);
}

DWORD addTunFilter(HANDLE engine, bool ipv6, bool receive, UINT64 *luid, QString *operation)
{
    FWPM_FILTER_CONDITION0 condition{};
    condition.fieldKey = tunInterfaceCondition(receive);
    condition.matchType = FWP_MATCH_EQUAL;
    condition.conditionValue.type = FWP_UINT64;
    condition.conditionValue.uint64 = luid;

    const GUID &key = receive
                          ? (ipv6 ? kReceiveTunV6FilterKey : kReceiveTunV4FilterKey)
                          : (ipv6 ? kTunV6FilterKey : kTunV4FilterKey);
    const GUID &layer = receive
                            ? (ipv6 ? FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V6
                                    : FWPM_LAYER_ALE_AUTH_RECV_ACCEPT_V4)
                            : (ipv6 ? FWPM_LAYER_ALE_AUTH_CONNECT_V6
                                    : FWPM_LAYER_ALE_AUTH_CONNECT_V4);
    const wchar_t *name = receive
                              ? (ipv6 ? L"Throne permit received TUN IPv6"
                                      : L"Throne permit received TUN IPv4")
                              : (ipv6 ? L"Throne permit TUN IPv6"
                                      : L"Throne permit TUN IPv4");
    FWPM_FILTER0 filter = makeFilter(key,
                                    layer,
                                    name,
                                    FWP_ACTION_PERMIT,
                                    kPermitWeight,
                                    false,
                                    &condition,
                                    1);
    return addFilter(engine, &filter, operation);
}

DWORD deleteTunFilters(HANDLE engine, QString *operation)
{
    DWORD result = deleteFilterIfPresent(engine, kTunV4FilterKey);
    if (result != ERROR_SUCCESS) {
        if (operation != nullptr) {
            *operation = QStringLiteral("Delete the Throne IPv4 TUN allowance");
        }
        return result;
    }
    result = deleteFilterIfPresent(engine, kTunV6FilterKey);
    if (result != ERROR_SUCCESS) {
        if (operation != nullptr) {
            *operation = QStringLiteral("Delete the Throne IPv6 TUN allowance");
        }
        return result;
    }
    result = deleteFilterIfPresent(engine, kReceiveTunV4FilterKey);
    if (result != ERROR_SUCCESS) {
        if (operation != nullptr) {
            *operation = QStringLiteral("Delete the Throne received IPv4 TUN allowance");
        }
        return result;
    }
    result = deleteFilterIfPresent(engine, kReceiveTunV6FilterKey);
    if (result != ERROR_SUCCESS && operation != nullptr) {
        *operation = QStringLiteral("Delete the Throne received IPv6 TUN allowance");
    }
    return result;
}

} // namespace

class WindowsWfpKillSwitchBackend::Impl
{
public:
    mutable QMutex mutex;
    EngineHandle dynamicEngine;
    QString coreExecutablePath;
};

WindowsWfpKillSwitchBackend::WindowsWfpKillSwitchBackend() : impl_(std::make_unique<Impl>()) {}

WindowsWfpKillSwitchBackend::~WindowsWfpKillSwitchBackend() = default;

WindowsWfpKillSwitchBackend::BaselineStatus WindowsWfpKillSwitchBackend::queryBaseline() const
{
    QMutexLocker lock(&impl_->mutex);
    EngineHandle engine;
    DWORD result = openEngine(false, &engine);
    if (result != ERROR_SUCCESS) {
        return {BaselineState::Error, operationError(QStringLiteral("Open Windows Filtering Platform"), result)};
    }

    bool legacyProviderPresent = false;
    QString error;
    FWPM_PROVIDER0 *legacyProvider = nullptr;
    result = FwpmProviderGetByKey0(engine.get(), &kLegacyProviderKey, &legacyProvider);
    WfpMemory legacyProviderMemory(legacyProvider);
    if (result == ERROR_SUCCESS) {
        legacyProviderPresent = true;
        if (legacyProvider == nullptr ||
            !providerHasKnownLegacyThroneOwnershipMarker(*legacyProvider)) {
            return {
                BaselineState::Error,
                QStringLiteral("A deterministic WFP provider collision is present; Throne will not modify it because it lacks the exact v1-v3 ownership marker"),
            };
        }
    } else if (result != FWP_E_PROVIDER_NOT_FOUND) {
        return {
            BaselineState::Error,
            operationError(QStringLiteral("Query the legacy Throne WFP provider"), result),
        };
    }

    int presentCount = 0;
    constexpr int expectedCount = 1 + static_cast<int>(kPersistentFilterKeys.size());
    bool schemaMatches = true;
    bool staleDynamicFilterPresent = false;
    bool dynamicFilterPresent = false;
    bool objectMatches = false;

    objectMatches = false;
    const Presence subLayer = getObjectPresence<FWPM_SUBLAYER0>(engine.get(),
                                                                kSubLayerKey,
                                                                FwpmSubLayerGetByKey0,
                                                                subLayerMatchesSchema,
                                                                &objectMatches,
                                                                FWP_E_SUBLAYER_NOT_FOUND,
                                                                &error,
                                                                QStringLiteral("Throne WFP sublayer"));
    if (subLayer == Presence::Failed) {
        return {BaselineState::Error, error};
    }
    presentCount += subLayer == Presence::Present ? 1 : 0;
    schemaMatches &= subLayer != Presence::Present || objectMatches;

    for (const GUID *key : kPersistentFilterKeys) {
        objectMatches = false;
        const Presence filter = getObjectPresence<FWPM_FILTER0>(engine.get(),
                                                                *key,
                                                                FwpmFilterGetByKey0,
                                                                [key](const FWPM_FILTER0 &object) {
                                                                    return persistentFilterMatchesSchema(*key, object);
                                                                },
                                                                &objectMatches,
                                                                FWP_E_FILTER_NOT_FOUND,
                                                                &error,
                                                                QStringLiteral("Throne WFP filter"));
        if (filter == Presence::Failed) {
            return {BaselineState::Error, error};
        }
        presentCount += filter == Presence::Present ? 1 : 0;
        schemaMatches &= filter != Presence::Present || objectMatches;
    }

    for (const GUID *key : kDynamicFilterKeys) {
        objectMatches = false;
        const Presence filter = getObjectPresence<FWPM_FILTER0>(engine.get(),
                                                                *key,
                                                                FwpmFilterGetByKey0,
                                                                [key](const FWPM_FILTER0 &object) {
                                                                    return dynamicFilterMatchesSchema(*key, object);
                                                                },
                                                                &objectMatches,
                                                                FWP_E_FILTER_NOT_FOUND,
                                                                &error,
                                                                QStringLiteral("Throne dynamic WFP filter"));
        if (filter == Presence::Failed) {
            return {BaselineState::Error, error};
        }
        dynamicFilterPresent |= filter == Presence::Present;
        schemaMatches &= filter != Presence::Present || objectMatches;
    }
    staleDynamicFilterPresent = dynamicFilterPresent && !impl_->dynamicEngine.valid();

    const bool anyObjectPresent = legacyProviderPresent || presentCount != 0 ||
                                  dynamicFilterPresent;
    if (anyObjectPresent) {
        const MutationOwnershipResult ownership = verifyMutationOwnership(engine.get());
        if (ownership.ownership == MutationOwnership::Foreign ||
            ownership.ownership == MutationOwnership::Error) {
            return {BaselineState::Error, ownership.detail};
        }
    }

    if (!anyObjectPresent) {
        return {BaselineState::Absent, QStringLiteral("No Throne kill-switch objects are installed")};
    }
    if (!legacyProviderPresent && presentCount == expectedCount && schemaMatches &&
        !staleDynamicFilterPresent) {
        return {
            BaselineState::Valid,
            QStringLiteral("The providerless Throne kill-switch v4 baseline is installed"),
        };
    }
    if (legacyProviderPresent) {
        return {
            BaselineState::StaleOrPartial,
            QStringLiteral("A marked provider-associated Throne v1-v3 policy requires migration to providerless schema v4"),
        };
    }
    if (presentCount == expectedCount && schemaMatches && staleDynamicFilterPresent) {
        return {BaselineState::StaleOrPartial,
                QStringLiteral("The baseline is valid, but stale Throne runtime allowances are present")};
    }
    if (presentCount == expectedCount) {
        return {BaselineState::StaleOrPartial,
                QStringLiteral("All Throne kill-switch objects exist, but at least one uses an obsolete or invalid schema")};
    }
    return {BaselineState::StaleOrPartial,
            QStringLiteral("Only %1 of %2 expected Throne kill-switch objects are present")
                .arg(presentCount)
                .arg(expectedCount)};
}

bool WindowsWfpKillSwitchBackend::reconcileBaseline(QString *error)
{
    QMutexLocker lock(&impl_->mutex);
    if (impl_->dynamicEngine.valid()) {
        setError(error, QStringLiteral("Stop the active kill-switch core session before reconciling persistent rules"));
        return false;
    }

    EngineHandle engine;
    DWORD result = openEngine(false, &engine);
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Open Windows Filtering Platform"), result));
        return false;
    }

    Transaction transaction(engine.get());
    result = transaction.begin();
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Begin the kill-switch transaction"), result));
        return false;
    }

    const MutationOwnershipResult ownership = verifyMutationOwnership(engine.get());
    if (ownership.ownership == MutationOwnership::Foreign ||
        ownership.ownership == MutationOwnership::Error) {
        setError(error, ownership.detail);
        return false;
    }

    QString operation;
    result = deletePersistentObjects(engine.get(), &operation);
    if (result == ERROR_SUCCESS) {
        result = addSubLayer(engine.get(), &operation);
    }
    if (result == ERROR_SUCCESS) {
        result = addPersistentFilters(engine.get(), &operation);
    }
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(operation, result));
        return false;
    }

    result = transaction.commit();
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Commit the kill-switch baseline"), result));
        return false;
    }
    setError(error, {});
    return true;
}

Configs_sys::KillSwitchReconcileResult WindowsWfpKillSwitchBackend::reconcile()
{
    const BaselineStatus status = queryBaseline();
    if (status.state == BaselineState::Valid) {
        return {Configs_sys::KillSwitchResult::Success(), {true, false, false}};
    }
    if (status.state == BaselineState::Error) {
        // Failure to query BFE cannot prove that Throne's persistent objects
        // are absent. Conservatively report protection as active so the
        // controller remains enabled and refuses an unverified transition.
        return {Configs_sys::KillSwitchResult::Failure(status.detail), {true, false, false}};
    }
    if (status.state == BaselineState::Absent) {
        return {Configs_sys::KillSwitchResult::Success(), {}};
    }

    QString error;
    if (!reconcileBaseline(&error)) {
        // Some persistent Throne objects were observed. Report conservative
        // active state even if their exact effectiveness could not be proven.
        return {Configs_sys::KillSwitchResult::Failure(error), {true, false, false}};
    }
    return {Configs_sys::KillSwitchResult::Success(), {true, false, false}};
}

Configs_sys::KillSwitchResult WindowsWfpKillSwitchBackend::ensureBaseline()
{
    const BaselineStatus status = queryBaseline();
    if (status.state == BaselineState::Valid) {
        return Configs_sys::KillSwitchResult::Success();
    }
    if (status.state == BaselineState::Error) {
        return Configs_sys::KillSwitchResult::Failure(status.detail);
    }

    QString error;
    if (!reconcileBaseline(&error)) {
        return Configs_sys::KillSwitchResult::Failure(error);
    }
    return Configs_sys::KillSwitchResult::Success();
}

Configs_sys::KillSwitchResult WindowsWfpKillSwitchBackend::startDynamicCore(
    const Configs_sys::KillSwitchTrustedCorePlan &plan)
{
    if (!plan.isValid()) {
        return Configs_sys::KillSwitchResult::Failure(
            QStringLiteral("The trusted core executable plan is empty or invalid"));
    }
    if (plan.executablePaths.size() != 1) {
        return Configs_sys::KillSwitchResult::Failure(
            QStringLiteral("The Windows kill switch currently supports exactly one trusted core executable"));
    }

    QString error;
    if (!startCoreSession(plan.executablePaths.constFirst(), &error)) {
        return Configs_sys::KillSwitchResult::Failure(error);
    }
    return Configs_sys::KillSwitchResult::Success();
}

bool WindowsWfpKillSwitchBackend::startCoreSession(const QString &absoluteCoreExecutablePath, QString *error)
{
    QMutexLocker lock(&impl_->mutex);

    const QFileInfo executable(absoluteCoreExecutablePath);
    if (!executable.isAbsolute() || !executable.exists() || !executable.isFile()) {
        setError(error, QStringLiteral("The core executable path is not an existing absolute file: %1")
                            .arg(absoluteCoreExecutablePath));
        return false;
    }

    const QString canonicalPath = executable.canonicalFilePath();
    if (canonicalPath.isEmpty()) {
        setError(error, QStringLiteral("The core executable path cannot be canonicalized: %1")
                            .arg(absoluteCoreExecutablePath));
        return false;
    }
    if (impl_->dynamicEngine.valid() &&
        QString::compare(impl_->coreExecutablePath, canonicalPath, Qt::CaseInsensitive) == 0) {
        setError(error, {});
        return true;
    }

    const QString nativePath = QDir::toNativeSeparators(canonicalPath);
    FWP_BYTE_BLOB *appId = nullptr;
    DWORD result = FwpmGetAppIdFromFileName0(reinterpret_cast<PCWSTR>(nativePath.utf16()), &appId);
    WfpMemory appIdMemory(appId);
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Resolve the WFP application ID for ThroneCore"), result));
        return false;
    }

    // For a different trusted executable, close the old exception only after
    // the new path has been validated and converted to a WFP app ID. The
    // persistent baseline remains active, so a subsequent failure is closed.
    if (impl_->dynamicEngine.valid()) {
        const DWORD closeResult = impl_->dynamicEngine.reset();
        impl_->coreExecutablePath.clear();
        if (closeResult != ERROR_SUCCESS) {
            setError(error, operationError(QStringLiteral("Close the previous kill-switch session"), closeResult));
            return false;
        }
    }

    EngineHandle dynamicEngine;
    result = openEngine(true, &dynamicEngine);
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Open the dynamic kill-switch session"), result));
        return false;
    }

    Transaction transaction(dynamicEngine.get());
    result = transaction.begin();
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Begin the core-allowance transaction"), result));
        return false;
    }

    QString operation;
    result = addCoreFilter(dynamicEngine.get(), false, appId, &operation);
    if (result == ERROR_SUCCESS) {
        result = addCoreFilter(dynamicEngine.get(), true, appId, &operation);
    }
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(operation, result));
        return false;
    }

    result = transaction.commit();
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Commit the core allowances"), result));
        return false;
    }

    impl_->dynamicEngine = std::move(dynamicEngine);
    impl_->coreExecutablePath = canonicalPath;
    setError(error, {});
    return true;
}

bool WindowsWfpKillSwitchBackend::stopCoreSession(QString *error)
{
    QMutexLocker lock(&impl_->mutex);
    if (!impl_->dynamicEngine.valid()) {
        setError(error, {});
        return true;
    }
    const DWORD result = impl_->dynamicEngine.reset();
    impl_->coreExecutablePath.clear();
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Close the dynamic kill-switch session"), result));
        return false;
    }
    setError(error, {});
    return true;
}

bool WindowsWfpKillSwitchBackend::addTunAllowance(QString *error)
{
    return addTunAllowanceForInterface(tunInterfaceAlias(), true, true, error);
}

Configs_sys::KillSwitchResult WindowsWfpKillSwitchBackend::addTunAllowance(
    const Configs_sys::KillSwitchTunInterface &tunInterface)
{
    if (!tunInterface.isValid()) {
        return Configs_sys::KillSwitchResult::Failure(
            QStringLiteral("The TUN interface identity or address families are invalid"));
    }

    NET_LUID interfaceLuid{};
    bool haveLuid = false;
    if (tunInterface.interfaceIndex != 0) {
        if (tunInterface.interfaceIndex > (std::numeric_limits<NET_IFINDEX>::max)()) {
            return Configs_sys::KillSwitchResult::Failure(
                QStringLiteral("The TUN interface index is outside the Windows NET_IFINDEX range"));
        }
        const NETIO_STATUS result = ConvertInterfaceIndexToLuid(
            static_cast<NET_IFINDEX>(tunInterface.interfaceIndex), &interfaceLuid);
        if (result != NO_ERROR) {
            return Configs_sys::KillSwitchResult::Failure(
                operationError(QStringLiteral("Resolve TUN interface index %1").arg(tunInterface.interfaceIndex),
                               result));
        }
        haveLuid = true;
    }

    if (!tunInterface.name.trimmed().isEmpty()) {
        NET_LUID namedLuid{};
        const std::wstring alias = tunInterface.name.toStdWString();
        const NETIO_STATUS result = ConvertInterfaceAliasToLuid(alias.c_str(), &namedLuid);
        if (result != NO_ERROR) {
            return Configs_sys::KillSwitchResult::Failure(
                operationError(QStringLiteral("Resolve TUN interface '%1'").arg(tunInterface.name), result));
        }
        if (haveLuid && namedLuid.Value != interfaceLuid.Value) {
            return Configs_sys::KillSwitchResult::Failure(
                QStringLiteral("The TUN interface name and index identify different Windows interfaces"));
        }
        interfaceLuid = namedLuid;
        haveLuid = true;
    }

    if (!haveLuid) {
        return Configs_sys::KillSwitchResult::Failure(
            QStringLiteral("The TUN interface could not be resolved to a Windows LUID"));
    }

    QString error;
    if (!addTunAllowanceForLuidValue(interfaceLuid.Value,
                                     tunInterface.ipv4,
                                     tunInterface.ipv6,
                                     &error)) {
        return Configs_sys::KillSwitchResult::Failure(error);
    }
    return Configs_sys::KillSwitchResult::Success();
}

bool WindowsWfpKillSwitchBackend::addTunAllowanceForInterface(const QString &interfaceAlias,
                                                               bool allowIPv4,
                                                               bool allowIPv6,
                                                               QString *error)
{
    if (interfaceAlias.trimmed().isEmpty()) {
        setError(error, QStringLiteral("The TUN interface alias is empty"));
        return false;
    }

    NET_LUID interfaceLuid{};
    const std::wstring alias = interfaceAlias.toStdWString();
    const NETIO_STATUS luidResult = ConvertInterfaceAliasToLuid(alias.c_str(), &interfaceLuid);
    if (luidResult != NO_ERROR) {
        setError(error, operationError(QStringLiteral("Resolve TUN interface '%1'").arg(interfaceAlias), luidResult));
        return false;
    }

    return addTunAllowanceForLuidValue(interfaceLuid.Value, allowIPv4, allowIPv6, error);
}

bool WindowsWfpKillSwitchBackend::addTunAllowanceForLuidValue(quint64 interfaceLuid,
                                                               bool allowIPv4,
                                                               bool allowIPv6,
                                                               QString *error)
{
    QMutexLocker lock(&impl_->mutex);
    if (!impl_->dynamicEngine.valid()) {
        setError(error, QStringLiteral("The dynamic kill-switch core session is not active"));
        return false;
    }
    if (!allowIPv4 && !allowIPv6) {
        setError(error, QStringLiteral("At least one TUN address family must be allowed"));
        return false;
    }

    Transaction transaction(impl_->dynamicEngine.get());
    DWORD result = transaction.begin();
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Begin the TUN-allowance transaction"), result));
        return false;
    }

    QString operation;
    result = deleteTunFilters(impl_->dynamicEngine.get(), &operation);
    UINT64 luidValue = interfaceLuid;
    if (result == ERROR_SUCCESS && allowIPv4) {
        result = addTunFilter(impl_->dynamicEngine.get(), false, false, &luidValue, &operation);
    }
    if (result == ERROR_SUCCESS && allowIPv4) {
        result = addTunFilter(impl_->dynamicEngine.get(), false, true, &luidValue, &operation);
    }
    if (result == ERROR_SUCCESS && allowIPv6) {
        result = addTunFilter(impl_->dynamicEngine.get(), true, false, &luidValue, &operation);
    }
    if (result == ERROR_SUCCESS && allowIPv6) {
        result = addTunFilter(impl_->dynamicEngine.get(), true, true, &luidValue, &operation);
    }
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(operation, result));
        return false;
    }

    result = transaction.commit();
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Commit the TUN allowances"), result));
        return false;
    }
    setError(error, {});
    return true;
}

bool WindowsWfpKillSwitchBackend::removeTunAllowanceImpl(QString *error)
{
    QMutexLocker lock(&impl_->mutex);
    if (!impl_->dynamicEngine.valid()) {
        setError(error, {});
        return true;
    }

    Transaction transaction(impl_->dynamicEngine.get());
    DWORD result = transaction.begin();
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Begin removal of TUN allowances"), result));
        return false;
    }

    QString operation;
    result = deleteTunFilters(impl_->dynamicEngine.get(), &operation);
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(operation, result));
        return false;
    }

    result = transaction.commit();
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Commit removal of TUN allowances"), result));
        return false;
    }
    setError(error, {});
    return true;
}

Configs_sys::KillSwitchResult WindowsWfpKillSwitchBackend::removeTunAllowance()
{
    QString error;
    if (!removeTunAllowanceImpl(&error)) {
        return Configs_sys::KillSwitchResult::Failure(error);
    }
    return Configs_sys::KillSwitchResult::Success();
}

bool WindowsWfpKillSwitchBackend::disableImpl(QString *error)
{
    QMutexLocker lock(&impl_->mutex);
    if (impl_->dynamicEngine.valid()) {
        const DWORD closeResult = impl_->dynamicEngine.reset();
        impl_->coreExecutablePath.clear();
        if (closeResult != ERROR_SUCCESS) {
            setError(error, operationError(QStringLiteral("Close the dynamic kill-switch session"), closeResult));
            return false;
        }
    }

    EngineHandle engine;
    DWORD result = openEngine(false, &engine);
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Open Windows Filtering Platform"), result));
        return false;
    }

    Transaction transaction(engine.get());
    result = transaction.begin();
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Begin removal of the kill-switch baseline"), result));
        return false;
    }

    const MutationOwnershipResult ownership = verifyMutationOwnership(engine.get());
    if (ownership.ownership == MutationOwnership::Foreign ||
        ownership.ownership == MutationOwnership::Error) {
        setError(error, ownership.detail);
        return false;
    }

    QString operation;
    result = deletePersistentObjects(engine.get(), &operation);
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(operation, result));
        return false;
    }

    result = transaction.commit();
    if (result != ERROR_SUCCESS) {
        setError(error, operationError(QStringLiteral("Commit removal of the kill-switch baseline"), result));
        return false;
    }
    setError(error, {});
    return true;
}

Configs_sys::KillSwitchResult WindowsWfpKillSwitchBackend::disable()
{
    QString error;
    if (!disableImpl(&error)) {
        return Configs_sys::KillSwitchResult::Failure(error);
    }
    return Configs_sys::KillSwitchResult::Success();
}

bool WindowsWfpKillSwitchBackend::coreSessionActive() const
{
    QMutexLocker lock(&impl_->mutex);
    return impl_->dynamicEngine.valid();
}

QString WindowsWfpKillSwitchBackend::tunInterfaceAlias()
{
    return QStringLiteral("throne-tun");
}
