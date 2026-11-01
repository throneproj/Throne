#include "include/sys/SystemProxy.hpp"
#include "include/sys/macos/ProxyJournal.hpp"
#include "include/sys/macos/ProxyServices.hpp"

#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QMutex>
#include <QStandardPaths>

#include <Security/Authorization.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

namespace {
    using namespace MacSystemProxy;
    QMutex proxyMutex;
    std::unique_ptr<QLockFile> ownerLock;

    template<class T> class Ref {
        T value;
    public:
        explicit Ref(T value = nullptr) : value(value) {}
        ~Ref() { if (value) CFRelease(value); }
        Ref(const Ref &) = delete;
        Ref &operator=(const Ref &) = delete;
        operator T() const { return value; }
    };
    QString scError(const char *operation) {
        return QString("system proxy %1: %2").arg(operation, QString::fromUtf8(SCErrorString(SCError())));
    }
    Ref<SCNetworkServiceRef> service(SCPreferencesRef prefs, const QString &id) {
        const Ref<CFStringRef> value(id.toCFString());
        return Ref<SCNetworkServiceRef>(SCNetworkServiceCopy(prefs, value));
    }
    QString project(CFDictionaryRef dictionary, QJsonObject &result) {
        result = {};
        for (int protocol = 0; protocol != 3; ++protocol) {
            for (const auto &key : keys(protocol)) {
                const Ref<CFStringRef> cfKey(key.toCFString());
                const auto value = dictionary ? CFDictionaryGetValue(dictionary, cfKey) : nullptr;
                if (!value) continue;
                if (CFGetTypeID(value) == CFStringGetTypeID()) result.insert(key, QString::fromCFString(static_cast<CFStringRef>(value)));
                else if (CFGetTypeID(value) == CFBooleanGetTypeID()) result.insert(key, bool(CFBooleanGetValue(static_cast<CFBooleanRef>(value))));
                else if (CFGetTypeID(value) == CFNumberGetTypeID()) {
                    int number;
                    if (!CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberIntType, &number)) return "invalid system proxy number";
                    result.insert(key, number);
                } else return "unsupported system proxy value";
            }
        }
        return {};
    }
    class PreferencesBackend final : public Backend {
        AuthorizationRef authorization = nullptr;
    public:
        ~PreferencesBackend() override { if (authorization) AuthorizationFree(authorization, kAuthorizationFlagDefaults); }
        QString services(QStringList &ids) override {
            QStringList names;
            if (auto error = discoverServices("/usr/sbin/networksetup", names); !error.isEmpty()) return error;
            const Ref<SCPreferencesRef> prefs(SCPreferencesCreate(nullptr, CFSTR("Throne"), nullptr));
            if (!prefs) return scError("open preferences");
            const Ref<SCNetworkSetRef> currentSet(SCNetworkSetCopyCurrent(prefs));
            const Ref<CFArrayRef> all(currentSet ? SCNetworkSetCopyServices(currentSet) : nullptr);
            if (!all) return scError("list services");
            ids.clear();
            for (CFIndex i = 0; i < CFArrayGetCount(all); ++i) {
                const auto s = static_cast<SCNetworkServiceRef>(CFArrayGetValueAtIndex(all, i));
                const auto name = SCNetworkServiceGetName(s);
                if (name && names.contains(QString::fromCFString(name)) && SCNetworkServiceGetEnabled(s))
                    ids << QString::fromCFString(SCNetworkServiceGetServiceID(s));
            }
            return {};
        }
        QString read(const QString &id, QJsonObject &proxy, bool &exists) override {
            const Ref<SCPreferencesRef> prefs(SCPreferencesCreate(nullptr, CFSTR("Throne"), nullptr));
            if (!prefs) return scError("open preferences");
            const auto s = service(prefs, id);
            exists = false;
            if (!s) return SCError() == kSCStatusNoKey ? QString{} : scError("read service");
            const Ref<SCNetworkProtocolRef> protocol(SCNetworkServiceCopyProtocol(s, kSCNetworkProtocolTypeProxies));
            if (!protocol) return SCError() == kSCStatusNoKey ? QString{} : scError("read proxy protocol");
            exists = true;
            return project(SCNetworkProtocolGetConfiguration(protocol), proxy);
        }
        Change change(const Entry &entry, bool restore, QString &error) override {
            // The system authorizes preferences access just as networksetup does; no setuid binary or installed helper.
            if (!authorization && AuthorizationCreate(nullptr, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &authorization) != errAuthorizationSuccess) {
                error = "cannot authorize system proxy preferences";
                return Change::Failed;
            }
            const Ref<SCPreferencesRef> prefs(SCPreferencesCreateWithAuthorization(nullptr, CFSTR("Throne"), nullptr, authorization));
            if (!prefs) { error = scError("open preferences"); return Change::Failed; }
            if (!SCPreferencesLock(prefs, false)) { error = scError("lock preferences"); return Change::Failed; }
            struct Unlock { SCPreferencesRef prefs; ~Unlock() { SCPreferencesUnlock(prefs); } } unlock{prefs};
            // No cached read before the lock: third-party updates must participate in the comparison.
            SCPreferencesSynchronize(prefs);
            const auto s = service(prefs, entry.service);
            if (!s) {
                if (SCError() == kSCStatusNoKey) return Change::Missing;
                error = scError("read service"); return Change::Failed;
            }
            const Ref<SCNetworkProtocolRef> protocol(SCNetworkServiceCopyProtocol(s, kSCNetworkProtocolTypeProxies));
            if (!protocol) {
                if (SCError() == kSCStatusNoKey) return Change::Missing;
                error = scError("read proxy protocol"); return Change::Failed;
            }
            const auto original = SCNetworkProtocolGetConfiguration(protocol);
            QJsonObject current;
            if (error = project(original, current); !error.isEmpty()) return Change::Failed;
            const auto fields = keys(entry.protocol);
            current = select(current, fields);
            const auto &expected = restore ? entry.applied : entry.before;
            const auto &desired = restore ? entry.before : entry.applied;
            if (current != expected) {
                if (!restore || current != desired) return Change::Conflict;
                // Restoration may have committed before an ApplyChanges failure/crash. Retry activation only.
                if (!SCPreferencesApplyChanges(prefs)) { error = scError("activate restored preferences"); return Change::Failed; }
                return Change::Done;
            }
            const Ref<CFMutableDictionaryRef> updated(original ? CFDictionaryCreateMutableCopy(nullptr, 0, original)
                : CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
            for (const auto &key : fields) {
                const Ref<CFStringRef> cfKey(key.toCFString());
                if (!desired.contains(key)) CFDictionaryRemoveValue(updated, cfKey);
                else if (desired.value(key).isBool()) CFDictionarySetValue(updated, cfKey, desired.value(key).toBool() ? kCFBooleanTrue : kCFBooleanFalse);
                else if (desired.value(key).isString()) {
                    const Ref<CFStringRef> value(desired.value(key).toString().toCFString());
                    CFDictionarySetValue(updated, cfKey, value);
                } else {
                    const int number = desired.value(key).toInt();
                    const Ref<CFNumberRef> value(CFNumberCreate(nullptr, kCFNumberIntType, &number));
                    CFDictionarySetValue(updated, cfKey, value);
                }
            }
            if (!SCNetworkProtocolSetConfiguration(protocol, updated) || !SCPreferencesCommitChanges(prefs) || !SCPreferencesApplyChanges(prefs)) {
                error = scError("write preferences");
                return Change::Failed;
            }
            return Change::Done;
        }
    };

    QString journalPath(QString &error) {
        // Global per user, not per --appdata: two installations cannot own the system proxy simultaneously.
        const auto directory = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/Throne/system-proxy";
        if (::geteuid() != ::getuid()) { error = "system proxy recovery cannot run setuid"; return {}; }
        if (!QDir().mkpath(directory)) { error = "cannot create system proxy recovery directory"; return {}; }
        struct stat st{};
        if (::lstat(QFile::encodeName(directory).constData(), &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != ::geteuid() ||
            ::chmod(QFile::encodeName(directory).constData(), 0700) != 0) {
            error = "unsafe system proxy recovery directory";
            return {};
        }
        return directory + "/state.json";
    }
    QString runTransaction(const QString *host = nullptr, int port = 0) {
        QMutexLocker lock(&proxyMutex);
        QString error;
        const auto path = journalPath(error);
        if (!error.isEmpty()) return error;
        if (!host && !QFileInfo::exists(path) && !ownerLock) return {};
        if (!ownerLock) {
            auto candidate = std::make_unique<QLockFile>(path + ".lock");
            candidate->setStaleLockTime(0);
            if (!candidate->tryLock(0)) return "system proxy is owned by another Throne instance";
            ownerLock = std::move(candidate);
        }
        PreferencesBackend backend;
        FileJournal journal(path);
        Transaction transaction(backend, journal);
        error = host ? transaction.apply(*host, port) : transaction.recover();
        if (!QFileInfo::exists(path)) ownerLock.reset();
        return error;
    }
}

QString SystemProxy_Apply(const QString &host, int port, const QString &) { return runTransaction(&host, port); }
QString SystemProxy_Clear() { return runTransaction(); }
QString SystemProxy_Recover() { return runTransaction(); }
