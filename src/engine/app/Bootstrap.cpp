#include "include/engine/app/Bootstrap.h"

#include <cstdio>
#include <cstring>
#include <utility>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>

#include "include/engine/app/DataDirLock.h"
#include "include/engine/app/InstanceServer.h"
#include "include/engine/headless/HeadlessInstance.h"
#include "include/global/Configs.hpp"
#include "include/global/DeviceDetailsHelper.hpp"
#include "include/global/Logger.hpp"
#include "include/stats/traffic/TrafficStatsManager.hpp"

#ifdef Q_OS_WIN
#include "include/sys/windows/MiniDump.h"

#include <windows.h>
// <windows.h> defines SetPort, which under unity builds clobbers Configs::outbound::SetPort.
#undef SetPort
#endif
#ifdef Q_OS_UNIX
#include <QSocketNotifier>

#include <atomic>
#include <cerrno>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef Q_OS_MACOS
#include <libproc.h>
#endif

#define LOCAL_SERVER_PREFIX "throne-"

namespace Engine::Bootstrap {
    namespace {
        constexpr auto FALLBACK_MARKER = "config/.install-dir-unwritable";

        // QFileInfo::isWritable reports the read-only attribute, not what a UAC-filtered token can actually do.
        bool DirIsWritable(const QDir &dir) {
            if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) return false;
            QFile probe(dir.absoluteFilePath(".throne-write-test"));
            if (!probe.open(QIODevice::WriteOnly)) return false;
            probe.close();
            probe.remove();
            return true;
        }

        bool ConfigDirIsUsable(const QDir &configDir) {
            if (!DirIsWritable(configDir)) return false;
            const QString db = configDir.absoluteFilePath("throne.db");
            if (!QFile::exists(db)) return true;
            QFile file(db);
            return file.open(QIODevice::ReadWrite);
        }

        void CopyDirContents(const QString &from, const QString &to) {
            QDir().mkpath(to);
            QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
            while (it.hasNext()) {
                it.next();
                const QString target = QDir(to).absoluteFilePath(it.fileName());
                if (it.fileInfo().isDir()) CopyDirContents(it.filePath(), target);
                else if (!QFile::exists(target)) QFile::copy(it.filePath(), target);
            }
        }

        // An elevated relaunch finds the install dir writable again, so the fallback must stay pinned by the marker.
        bool AdoptUserConfigDir(const QDir &installWd, const QDir &userWd) {
            QFile marker(userWd.absoluteFilePath(FALLBACK_MARKER));
            if (marker.open(QIODevice::ReadOnly)) {
                const bool pinnedHere = QString::fromUtf8(marker.readAll()).trimmed() == installWd.absolutePath();
                marker.close();
                if (pinnedHere) return true;
            }

            const QString installConfig = installWd.absoluteFilePath("config");
            if (ConfigDirIsUsable(QDir(installConfig))) return false;

            const QString userConfig = userWd.absoluteFilePath("config");
            QDir().mkpath(userConfig);
            if (!QFile::exists(userConfig + "/throne.db") && QFile::exists(installConfig + "/throne.db")) {
                CopyDirContents(installConfig, userConfig);
                LOG_WARN(QString("copied existing config from %1").arg(installConfig));
            }
            if (marker.open(QIODevice::WriteOnly)) {
                marker.write(installWd.absolutePath().toUtf8());
                marker.close();
            }
            LOG_WARN(QString("%1 is not writable, using %2").arg(installConfig, userConfig));
            return true;
        }

        // "-name" and "--name" alike; nullptr for anything that is not an option.
        const char *bootstrapOptionName(const char *arg) {
            if (arg == nullptr || arg[0] != '-') return nullptr;
            return arg[1] == '-' ? arg + 2 : arg + 1;
        }

        bool bootstrapEnvTruthy(const char *name) {
            const QByteArray value = qgetenv(name).trimmed().toLower();
            return !value.isEmpty() && value != "0" && value != "false" && value != "no" && value != "off";
        }

        // Any local user can bind the predictable legacy name in the shared temp dir; headless only trusts a socket of its own user.
        bool bootstrapLegacySocketIsOurs(const QString &serverName) {
#ifdef Q_OS_UNIX
            const QString path = QDir::cleanPath(QDir::tempPath()) + '/' + serverName;
            struct stat st{};
            if (::lstat(QFile::encodeName(path).constData(), &st) != 0) return false;
            if (S_ISSOCK(st.st_mode) && st.st_uid == ::geteuid()) return true;
            LOG_WARN(QString("%1 is not a socket of this user, so it is not taken for an older Throne").arg(path));
            return false;
#else
            Q_UNUSED(serverName)
            return false;
#endif
        }

#ifdef Q_OS_UNIX
        // Dir fds opened with O_NOFOLLOW: the data dir's owner may swap temp or a subdir for a link at any moment; a link is only unlinked.
        void bootstrapRemoveTree(int parentFd, const char *name, int depth) {
            const int fd = ::openat(parentFd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (fd < 0) {
                if (errno == ELOOP || errno == ENOTDIR) ::unlinkat(parentFd, name, 0);
                return;
            }
            std::vector<std::string> entries;
            const int listFd = ::dup(fd);
            if (DIR *dir = listFd >= 0 ? ::fdopendir(listFd) : nullptr; dir != nullptr) {
                while (const dirent *entry = ::readdir(dir)) {
                    if (std::strcmp(entry->d_name, ".") != 0 && std::strcmp(entry->d_name, "..") != 0) entries.emplace_back(entry->d_name);
                }
                ::closedir(dir);
            } else if (listFd >= 0) {
                ::close(listFd);
            }
            for (const auto &entry : entries) {
                struct stat st{};
                if (::fstatat(fd, entry.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) continue;
                if (!S_ISDIR(st.st_mode)) ::unlinkat(fd, entry.c_str(), 0);
                else if (depth < 64) bootstrapRemoveTree(fd, entry.c_str(), depth + 1);
            }
            ::close(fd);
            ::unlinkat(parentFd, name, AT_REMOVEDIR);
        }
#endif
    } // namespace

    Launch PreScan(int argc, char **argv) {
        Launch launch;
        bool dumpConfigs = false;
        bool headless = false;
        bool client = false;
        for (int i = 1; i < argc; i++) {
            if (argv[i] != nullptr && std::strcmp(argv[i], "-dump-configs") == 0) dumpConfigs = true;
            const char *name = bootstrapOptionName(argv[i]);
            if (name == nullptr) continue;
            if (std::strcmp(name, "headless") == 0) headless = true;
            else if (std::strcmp(name, "health") == 0 || std::strcmp(name, "print-connection") == 0 || std::strcmp(name, "check") == 0) client = true;
            else if (std::strcmp(name, "service") == 0) launch.service = true;
        }
        if (dumpConfigs) launch.mode = Mode::DumpConfigs;
        else if (client) launch.mode = Mode::HeadlessClient;
        else if (headless || bootstrapEnvTruthy("THRONE_HEADLESS")) launch.mode = Mode::Headless;
        return launch;
    }

    void SetSoftwareNames() {
        software_name = "Throne";
        software_core_name = "sing-box";
    }

    DataDir ResolveDataDir(const QStringList &arguments, const QDir &relativeBase) {
        DataDir res;
        res.wd = QDir(QCoreApplication::applicationDirPath());
        if (arguments.contains("-appdata")) {
            res.useAppdata = true;
            const auto appdataIndex = arguments.indexOf("-appdata");
            if (arguments.size() > appdataIndex + 1 && !arguments.at(appdataIndex + 1).startsWith("-")) {
                res.appdataDir = arguments.at(appdataIndex + 1);
            }
        }
#ifdef NKR_CPP_USE_APPDATA
        res.useAppdata = true;
#endif
        QCoreApplication::setApplicationName("Throne");
        if (res.useAppdata) {
            if (!res.appdataDir.isEmpty()) {
                res.wd.setPath(relativeBase.absoluteFilePath(res.appdataDir));
            } else {
                res.wd.setPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
            }
        } else {
            const QDir userWd(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
            if (AdoptUserConfigDir(res.wd, userWd)) {
                res.wd = userWd;
                res.useAppdata = true;
            }
        }
        if (!res.wd.exists()) res.wd.mkpath(res.wd.absolutePath());
        if (!res.wd.exists("config")) res.wd.mkdir("config");
        res.configDir = res.wd.absoluteFilePath("config");
        return res;
    }

    QString InstanceServerName(const QDir &wd) {
        QByteArray hashBytes = QCryptographicHash::hash(wd.absolutePath().toUtf8(), QCryptographicHash::Md5).toBase64(QByteArray::OmitTrailingEquals);
        hashBytes.replace('+', '0').replace('/', '1');
        return LOCAL_SERVER_PREFIX + QString::fromUtf8(hashBytes);
    }

    void AdoptLegacyGeoAssets(const DataDir &dataDir) {
        if (!dataDir.useAppdata || dataDir.appdataDir.isEmpty()) return;
        const QDir legacy(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        if (legacy.absolutePath() == dataDir.wd.absolutePath()) return;
        for (const QString &name : QStringList{"geoip.dat", "geosite.dat"}) {
            const QString from = legacy.absoluteFilePath(name);
            const QString to = dataDir.wd.absoluteFilePath(name);
            if (!QFile::exists(from) || QFile::exists(to)) continue;
            if (QFile::copy(from, to)) {
                LOG_INFO(QString("copied %1 from %2").arg(name, legacy.absolutePath()));
            } else {
                LOG_WARN(QString("could not copy %1 from %2").arg(name, legacy.absolutePath()));
            }
        }
    }

    RelaunchParent::RelaunchParent(QStringList &arguments) {
        const QString prefix = "-wait_pid=";
        for (qsizetype i = arguments.size() - 1; i >= 1; i--) {
            if (arguments.at(i).startsWith(prefix)) pid = arguments.takeAt(i).mid(prefix.size()).toLongLong();
        }
#ifdef Q_OS_WIN
        if (pid > 0) {
            handle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
            if (handle != nullptr) WaitForSingleObject(handle, 30000);
        }
#endif
    }

    RelaunchParent::~RelaunchParent() {
#ifdef Q_OS_WIN
        if (handle != nullptr) CloseHandle(handle);
#endif
    }

    bool RelaunchParent::gone() const {
        if (pid <= 0) return true;
#ifdef Q_OS_WIN
        return handle == nullptr || WaitForSingleObject(handle, 0) == WAIT_OBJECT_0;
#else
        return ::kill(static_cast<pid_t>(pid), 0) == -1 && errno == ESRCH;
#endif
    }

    void WarnDroppedPayload(const QStringList &payload) {
        if (!payload.isEmpty()) LOG_WARN("dropped the launch payload: " + payload.join(' '));
    }

    Claim ClaimDataDir(DataDirLock &lock, const DataDir &dataDir, const QString &serverName, const QString &lockMode,
                       const QStringList &payload, const RelaunchParent &relaunchParent, bool handOff) {
        const QString &configDir = dataDir.configDir;
        // For up to 60 s a restarted instance never hands off: the owner is its exiting parent, which still answers on the socket.
        const QDeadlineTimer relaunchDeadline(relaunchParent.relaunched() ? 60000 : 0);
        QDeadlineTimer handOffDeadline(QDeadlineTimer::Forever);
        while (true) {
            const auto lockResult = lock.tryAcquire(configDir, lockMode);
            if (lockResult == DataDirLock::Result::Failed) {
                LOG_WARN("running without the data dir lock: " + lock.errorString());
                break;
            }
            if (lockResult == DataDirLock::Result::Acquired) {
                if (!lock.errorString().isEmpty()) LOG_WARN(lock.errorString());
                break;
            }
            if (!relaunchDeadline.hasExpired() && !relaunchParent.gone()) {
                QThread::msleep(100);
                continue;
            }
            if (!handOff) return {ClaimOutcome::HeldByOther, DataDirLock::ReadOwner(configDir).value("pid").toInteger()};
            if (handOffDeadline.isForever()) {
                if (relaunchParent.relaunched() && !relaunchParent.gone()) {
                    LOG_WARN(QString("the data dir is still held after waiting for the restarting PID %1").arg(relaunchParent.pid));
                }
                // A headless owner never takes a hand-off; its socket would only answer REJECT.
                if (const auto owner = DataDirLock::ReadOwner(configDir); owner.value("mode").toString() == "headless") {
                    WarnDroppedPayload(payload);
                    return {ClaimOutcome::HeldByHeadless, owner.value("pid").toInteger()};
                }
                handOffDeadline.setRemainingTime(10000);
            }
            // The owner may still be starting up, or exiting and about to release the lock.
            if (InstanceServer::HandOff(serverName, payload)) return {ClaimOutcome::HandedOff};
            if (handOffDeadline.hasExpired()) {
                // The owner record may be unreadable; a headless owner still answers HELLO on its own socket.
                if (const qint64 headlessPid = HeadlessInstance::Hello(dataDir.wd, "gui", 500); headlessPid > 0) {
                    WarnDroppedPayload(payload);
                    return {ClaimOutcome::HeldByHeadless, headlessPid};
                }
                const auto owner = DataDirLock::ReadOwner(configDir);
                const auto pid = owner.value("pid").toInteger();
                LOG_WARN(QString("the data dir is held by PID %1, which does not answer on %2").arg(QString::number(pid), serverName));
                WarnDroppedPayload(payload);
                return {ClaimOutcome::HeldByOther, pid};
            }
            QThread::msleep(100);
        }
        // A Throne older than the lock only answers on its socket; a restarted one would only find its exiting parent there.
        if (relaunchParent.gone() && (handOff || bootstrapLegacySocketIsOurs(serverName)) && InstanceServer::HandOff(serverName, payload)) {
            return {ClaimOutcome::HandedOff};
        }
        return {};
    }

    void AttachParentConsole() {
#ifdef Q_OS_WIN
        // An inherited pipe or file wins: attaching would steal a redirect.
        const auto usable = [](FILE *stream, DWORD which) {
            const HANDLE handle = GetStdHandle(which);
            return _fileno(stream) >= 0 && handle != nullptr && handle != INVALID_HANDLE_VALUE && GetFileType(handle) != FILE_TYPE_UNKNOWN;
        };
        const bool out = usable(stdout, STD_OUTPUT_HANDLE);
        const bool err = usable(stderr, STD_ERROR_HANDLE);
        if ((out && err) || !AttachConsole(ATTACH_PARENT_PROCESS)) return;
        FILE *console = nullptr;
        if (!out) freopen_s(&console, "CONOUT$", "w", stdout);
        if (!err) freopen_s(&console, "CONOUT$", "w", stderr);
#endif
    }

    void OpenDataDir(const DataDir &dataDir, const QStringList &arguments) {
        const bool useAppdata = dataDir.useAppdata;
        const QString &appdataDir = dataDir.appdataDir;
        const QString &configDir = dataDir.configDir;
        SetDeviceIdStore(configDir);

        QDir::setCurrent(configDir);
#ifdef Q_OS_UNIX
        bootstrapRemoveTree(AT_FDCWD, "temp", 0);
#else
        QDir("temp").removeRecursively();
#endif
        AdoptLegacyGeoAssets(dataDir);

        appStartEpoch = QDateTime::currentSecsSinceEpoch();

        Configs::initDB(QString(QDir::currentPath() + QDir::separator() + "throne.db").toStdString());

        Logging::SetLevel(Logging::LevelFromString(Configs::dataManager->settingsRepo->log_file_level));

        Stats::trafficStatsManager->Init();

        Configs::dataManager->settingsRepo->argv = arguments;
        if (Configs::dataManager->settingsRepo->argv.contains("-many")) Configs::dataManager->settingsRepo->flag_many = true;
        if (Configs::dataManager->settingsRepo->argv.contains("-tray")) Configs::dataManager->settingsRepo->flag_tray = true;
        if (Configs::dataManager->settingsRepo->argv.contains("-debug")) Configs::dataManager->settingsRepo->flag_debug = true;
        if (Configs::dataManager->settingsRepo->argv.contains("-flag_restart_tun_on")) Configs::dataManager->settingsRepo->flag_restart_tun_on = true;
        Configs::dataManager->settingsRepo->flag_use_appdata = useAppdata;
        if(useAppdata && !appdataDir.isEmpty()) Configs::dataManager->settingsRepo->appdataDir = appdataDir;
#ifdef NKR_CPP_DEBUG
        Configs::dataManager->settingsRepo->flag_debug = true;
#endif
    }

    void PrepareRuntime() {
        AddBundledPluginPath();

        DS_cores = new QThread;
        DS_cores->start();
    }

    void AddBundledPluginPath() {
#ifdef Q_OS_LINUX
        QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() + "/usr/plugins");
#endif
    }

    void StartLogging(const DataDir &dataDir) {
        Logging::Init(dataDir.configDir);
        LOG_INFO(QString("appdata mode: %1").arg(dataDir.useAppdata ? "yes" : "no"));
#ifdef Q_OS_WIN
        Windows_SetCrashDumpPath();
        Windows_ConfigureWER();
#endif
    }

    bool ExitInProgress() {
        return Configs::dataManager != nullptr && Configs::dataManager->settingsRepo->prepare_exit;
    }

#ifdef Q_OS_UNIX
    namespace {
        int bootstrapSignalPipe[2] = {-1, -1};
        // Set before the handlers are installed; an exec keeps both, so a restarted image sees the same values.
        std::atomic<pid_t> bootstrapBootParent{0};
        std::atomic<pid_t> bootstrapBootSession{0};
        static_assert(std::atomic<pid_t>::is_always_lock_free, "read from signal context");

        // Async-signal-safe: atomic loads and the self-pipe write(); teardown runs from the notifier on the main thread.
        void bootstrapPosixSignalHandler(int signum, siginfo_t *info, void *) {
            const int savedErrno = errno;
            int value = signum;
            if (signum == SIGHUP && info != nullptr) {
                const pid_t sender = info->si_pid;
                // A pty relay (sudo, su) forwards the terminal's hang-up with its own kill().
                if (sender > 0 && sender == bootstrapBootParent.load(std::memory_order_relaxed)) value |= kSignalFromParent;
                // The kernel's hang-up carries si_pid 0; a shell of our session relays it to its jobs. getsid() is a bare syscall
                // and still answers for a zombie sender.
                const pid_t bootSession = bootstrapBootSession.load(std::memory_order_relaxed);
                bool fromSession = sender == 0 || (sender > 0 && (sender == bootSession || ::getsid(sender) == bootSession));
#ifdef SI_KERNEL
                fromSession = fromSession || info->si_code == SI_KERNEL;
#endif
                if (fromSession) value |= kSignalFromSession;
            }
            const auto byte = static_cast<char>(value);
            [[maybe_unused]] const ssize_t written = ::write(bootstrapSignalPipe[1], &byte, 1);
            errno = savedErrno;
        }

        void bootstrapCatchSignal(int signum, const struct sigaction &action, bool keepInheritedIgnore) {
            if (keepInheritedIgnore) {
                struct sigaction current{};
                if (sigaction(signum, nullptr, &current) == 0 && current.sa_handler == SIG_IGN) return;
            }
            sigaction(signum, &action, nullptr);
        }

        // The pipe is non-blocking, so this stops at the first empty read.
        QByteArray bootstrapDrainSignalPipe() {
            QByteArray pending;
            if (bootstrapSignalPipe[0] < 0) return pending;
            char chunk[16];
            while (true) {
                const ssize_t n = ::read(bootstrapSignalPipe[0], chunk, sizeof(chunk));
                if (n > 0) pending.append(chunk, n);
                else if (n < 0 && errno == EINTR) continue;
                else break;
            }
            return pending;
        }
    } // namespace

    // Signals caught from here on wait in the pipe until ArmTerminationHandlers() / ArmSignalHandlers().
    bool InstallTerminationHandlers(void (*directFallback)(int), SignalSet set) {
        bootstrapBootParent.store(::getppid(), std::memory_order_relaxed);
        bootstrapBootSession.store(::getsid(0), std::memory_order_relaxed);
        if (::pipe(bootstrapSignalPipe) != 0) {
            bootstrapSignalPipe[0] = bootstrapSignalPipe[1] = -1;
            if (directFallback != nullptr) {
                signal(SIGTERM, directFallback);
                signal(SIGINT, directFallback);
            }
            return false;
        }
        for (const int fd : bootstrapSignalPipe) {
            ::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD) | FD_CLOEXEC);
            // Non-blocking: a full pipe must fail the write, never block in signal context.
            ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
        }

        struct sigaction sa{};
        sa.sa_sigaction = bootstrapPosixSignalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART | SA_SIGINFO;
        // nohup and background jobs ignore only SIGHUP/SIGINT; an inherited ignored SIGTERM would just turn every stop into SIGKILL.
        bootstrapCatchSignal(SIGTERM, sa, false);
        bootstrapCatchSignal(SIGINT, sa, set.keepInheritedIgnore);
        if (set.hangup) bootstrapCatchSignal(SIGHUP, sa, set.keepInheritedIgnore);
        return true;
    }

    void ArmTerminationHandlers(std::function<void()> onSignal) {
        if (bootstrapSignalPipe[0] < 0) return;
        auto *notifier = new QSocketNotifier(bootstrapSignalPipe[0], QSocketNotifier::Read, QCoreApplication::instance());
        QObject::connect(notifier, &QSocketNotifier::activated, QCoreApplication::instance(), [notifier, onSignal = std::move(onSignal)] {
            char drain[16];
            while (::read(bootstrapSignalPipe[0], drain, sizeof(drain)) > 0) {}
            if (ExitInProgress()) return;
            notifier->setEnabled(false);
            onSignal();
        });
    }

    void ArmSignalHandlers(std::function<void(int signum)> onSignal) {
        if (bootstrapSignalPipe[0] < 0) return;
        auto *notifier = new QSocketNotifier(bootstrapSignalPipe[0], QSocketNotifier::Read, QCoreApplication::instance());
        QObject::connect(notifier, &QSocketNotifier::activated, QCoreApplication::instance(), [onSignal = std::move(onSignal)] {
            // Drained first, so a handler that re-enters the loop never sees the bytes out of order.
            const QByteArray pending = bootstrapDrainSignalPipe();
            for (const char byte : pending) onSignal(static_cast<unsigned char>(byte));
        });
    }

    QList<int> TakePendingSignals() {
        QList<int> signums;
        const QByteArray pending = bootstrapDrainSignalPipe();
        for (const char byte : pending) signums << static_cast<unsigned char>(byte);
        return signums;
    }
#endif

    QString ProcessName(qint64 pid) {
        if (pid <= 0) return {};
#if defined(Q_OS_WIN)
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
        if (process == nullptr) return {};
        QString name;
        DWORD exitCode = 0;
        // A handle still opens for an exited process that something keeps referenced.
        const bool exited = GetExitCodeProcess(process, &exitCode) && exitCode != STILL_ACTIVE;
        if (!exited) {
            QString path(32767, QChar());
            DWORD length = static_cast<DWORD>(path.size());
            if (QueryFullProcessImageNameW(process, 0, reinterpret_cast<wchar_t *>(path.data()), &length)) {
                path.truncate(static_cast<qsizetype>(length));
                name = QFileInfo(path).fileName();
            }
        }
        CloseHandle(process);
        return name;
#else
#if defined(Q_OS_LINUX)
        QFile file(QString("/proc/%1/comm").arg(pid));
        if (file.open(QIODevice::ReadOnly)) return QString::fromUtf8(file.readAll()).trimmed();
#elif defined(Q_OS_MACOS)
        char name[64] = {};
        if (proc_name(static_cast<int>(pid), name, static_cast<uint32_t>(sizeof(name))) > 0) return QString::fromUtf8(name).trimmed();
#endif
        return {};
#endif
    }
} // namespace Engine::Bootstrap
