#include "include/sys/HostPresence.hpp"

#include "include/global/Logger.hpp"
#include "NkrVersion.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstring>

#include <windows.h>
#include <sddl.h>
// <windows.h> defines SetPort, which under unity builds clobbers Configs::outbound::SetPort.
#undef SetPort

namespace Sys::HostPresence {
    namespace {
        // Global\ needs SeCreateGlobalPrivilege (elevated or a service); Local\ is seen only inside this Windows session.
        constexpr const wchar_t *kHostPresenceWinNames[] = {L"Global\\ThroneHeadless", L"Local\\ThroneHeadless"};
        constexpr int kHostPresenceWinGlobal = 0;
        constexpr int kHostPresenceWinLocal = 1;
        constexpr DWORD kHostPresenceWinMinSize = 4096;
        // The first try plus 3 retries: a GUI probe keeps an exited headless's mapping alive for a moment.
        constexpr int kHostPresenceWinAttempts = 4;
        constexpr DWORD kHostPresenceWinRetryMs = 50;
        constexpr const wchar_t *kHostPresenceWinSddl = L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;AU)";

        // Never closed: the mapping is the marker, and it dies with its last handle, so it cannot outlive this process.
        HANDLE g_hostPresenceWinMapping = nullptr;
        int g_hostPresenceWinPublished = -1;

        enum class HostPresenceWinOutcome { Created, Taken, Denied, Failed };

        QString hostPresenceWinName(int index) {
            return QString::fromWCharArray(kHostPresenceWinNames[index]);
        }

        Holder hostPresenceWinParse(const QByteArray &data) {
            Holder holder;
            const auto doc = QJsonDocument::fromJson(data.trimmed());
            if (!doc.isObject()) return holder;
            const auto object = doc.object();
            holder.pid = object.value(QStringLiteral("pid")).toInteger();
            holder.installDir = object.value(QStringLiteral("install_dir")).toString();
            holder.dataDir = object.value(QStringLiteral("data_dir")).toString();
            holder.version = object.value(QStringLiteral("version")).toString();
            holder.started = object.value(QStringLiteral("started")).toInteger();
            return holder;
        }

        // A record still being written reads as zeros or a torn JSON, so a holder with pid 0: the mapping alone proves a live headless.
        Holder hostPresenceWinRead(HANDLE mapping) {
            const void *view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
            if (view == nullptr) return {};
            MEMORY_BASIC_INFORMATION info;
            ZeroMemory(&info, sizeof(info));
            const SIZE_T region = VirtualQuery(view, &info, sizeof(info)) != 0 ? info.RegionSize : 0;
            const char *data = static_cast<const char *>(view);
            const void *nul = region > 0 ? std::memchr(data, '\0', region) : nullptr;
            const SIZE_T length = nul != nullptr ? static_cast<SIZE_T>(static_cast<const char *>(nul) - data) : region;
            const Holder holder = hostPresenceWinParse(QByteArray(data, static_cast<qsizetype>(length)));
            UnmapViewOfFile(view);
            return holder;
        }

        std::optional<Holder> hostPresenceWinPeek(int index) {
            const HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, kHostPresenceWinNames[index]);
            if (mapping == nullptr) {
                // It exists but this user may not read it: still a live headless, with an unknown record.
                if (GetLastError() == ERROR_ACCESS_DENIED) return Holder{};
                return std::nullopt;
            }
            const Holder holder = hostPresenceWinRead(mapping);
            CloseHandle(mapping);
            return holder;
        }

        HostPresenceWinOutcome hostPresenceWinCreate(int index, SECURITY_ATTRIBUTES *attributes, DWORD size, Holder *holder,
                                                     DWORD *error) {
            for (int attempt = 0; attempt < kHostPresenceWinAttempts; ++attempt) {
                if (attempt > 0) Sleep(kHostPresenceWinRetryMs);
                SetLastError(ERROR_SUCCESS);
                const HANDLE mapping =
                    CreateFileMappingW(INVALID_HANDLE_VALUE, attributes, PAGE_READWRITE, 0, size, kHostPresenceWinNames[index]);
                const DWORD code = GetLastError();
                if (mapping == nullptr) {
                    *error = code;
                    return code == ERROR_ACCESS_DENIED ? HostPresenceWinOutcome::Denied : HostPresenceWinOutcome::Failed;
                }
                if (code != ERROR_ALREADY_EXISTS) {
                    g_hostPresenceWinMapping = mapping;
                    g_hostPresenceWinPublished = index;
                    return HostPresenceWinOutcome::Created;
                }
                *holder = hostPresenceWinRead(mapping);
                CloseHandle(mapping);
            }
            return HostPresenceWinOutcome::Taken;
        }

        bool hostPresenceWinWrite(HANDLE mapping, const QByteArray &record) {
            void *view = MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, 0);
            if (view == nullptr) return false;
            std::memcpy(view, record.constData(), static_cast<size_t>(record.size()));
            static_cast<char *>(view)[record.size()] = '\0';
            UnmapViewOfFile(view);
            return true;
        }

        QString hostPresenceWinDescribe(const Holder &holder) {
            return holder.pid > 0 ? QStringLiteral("PID %1, data dir %2").arg(holder.pid).arg(holder.dataDir)
                                  : QStringLiteral("its PID is unknown");
        }
    } // namespace

    bool Publish(const QString &dataDir) {
        if (g_hostPresenceWinMapping != nullptr) return true;
        const QByteArray record = QJsonDocument(QJsonObject{
                                                    {"pid", QCoreApplication::applicationPid()},
                                                    {"install_dir", QCoreApplication::applicationDirPath()},
                                                    {"data_dir", dataDir},
                                                    {"version", QStringLiteral(NKR_VERSION)},
                                                    {"started", QDateTime::currentSecsSinceEpoch()},
                                                })
                                      .toJson(QJsonDocument::Compact);
        const DWORD needed = static_cast<DWORD>(record.size()) + 1;
        const DWORD size = needed > kHostPresenceWinMinSize ? needed : kHostPresenceWinMinSize;

        PSECURITY_DESCRIPTOR descriptor = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(kHostPresenceWinSddl, SDDL_REVISION_1, &descriptor, nullptr)) {
            descriptor = nullptr;
        }
        SECURITY_ATTRIBUTES attributes;
        ZeroMemory(&attributes, sizeof(attributes));
        attributes.nLength = sizeof(attributes);
        attributes.lpSecurityDescriptor = descriptor;
        // Never inheritable: a child holding the handle would keep the marker alive after this process exits.
        attributes.bInheritHandle = FALSE;

        Holder holder;
        DWORD error = ERROR_SUCCESS;
        int index = kHostPresenceWinGlobal;
        auto outcome = hostPresenceWinCreate(index, &attributes, size, &holder, &error);
        if (outcome == HostPresenceWinOutcome::Denied) {
            if (const auto global = hostPresenceWinPeek(kHostPresenceWinGlobal)) {
                LOG_INFO(QStringLiteral("host presence: another headless Throne (%1) publishes %2; this one publishes in %3")
                             .arg(hostPresenceWinDescribe(*global), hostPresenceWinName(kHostPresenceWinGlobal),
                                  hostPresenceWinName(kHostPresenceWinLocal)));
            } else {
                LOG_INFO(QStringLiteral("host presence: %1 needs elevation; publishing in %2, which only a GUI in this Windows session sees")
                             .arg(hostPresenceWinName(kHostPresenceWinGlobal), hostPresenceWinName(kHostPresenceWinLocal)));
            }
            index = kHostPresenceWinLocal;
            outcome = hostPresenceWinCreate(index, &attributes, size, &holder, &error);
        }
        if (descriptor != nullptr) LocalFree(descriptor);

        const QString name = hostPresenceWinName(index);
        if (outcome == HostPresenceWinOutcome::Created) {
            if (!hostPresenceWinWrite(g_hostPresenceWinMapping, record)) {
                // The mapping alone still marks this instance; readers then report an unknown PID.
                LOG_WARN(QStringLiteral("host presence: could not write %1: error %2").arg(name).arg(GetLastError()));
            }
            LOG_INFO(QStringLiteral("host presence: published as %1").arg(name));
            return true;
        }
        if (outcome == HostPresenceWinOutcome::Taken) {
            LOG_WARN(QStringLiteral("host presence: another headless Throne (%1) already publishes %2; a GUI on this computer sees "
                                    "that one, not this one")
                         .arg(hostPresenceWinDescribe(holder), name));
            return false;
        }
        LOG_WARN(QStringLiteral("host presence: could not create %1: error %2; a GUI on this computer cannot see this instance")
                     .arg(name)
                     .arg(error));
        return false;
    }

    std::optional<Holder> OtherHeadless() {
        for (const int index : {kHostPresenceWinGlobal, kHostPresenceWinLocal}) {
            if (index == g_hostPresenceWinPublished) continue;
            auto holder = hostPresenceWinPeek(index);
            if (holder && holder->pid != QCoreApplication::applicationPid()) return holder;
        }
        return std::nullopt;
    }
} // namespace Sys::HostPresence
