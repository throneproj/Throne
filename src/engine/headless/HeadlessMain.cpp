#include "include/engine/headless/HeadlessMain.h"

#include <cstdio>
#include <exception>
#include <memory>
#include <utility>

#include <QByteArray>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QList>
#include <QLocalServer>
#include <QProcessEnvironment>

#include <3rdparty/SQLiteCpp/include/sqlite3.h>

#include "NkrVersion.h"
#include "include/api/RPC.h"
#include "include/api/remote/Overlay.hpp"
#include "include/database/Database.h"
#include "include/engine/EngineHost.h"
#include "include/engine/RuntimeOptions.h"
#include "include/engine/app/DataDirLock.h"
#include "include/engine/app/ExitCodes.h"
#include "include/engine/app/InstanceServer.h"
#include "include/engine/headless/HeadlessApi.h"
#include "include/engine/headless/HeadlessCli.h"
#include "include/engine/headless/HeadlessInstance.h"
#include "include/engine/headless/HeadlessRuntime.h"
#include "include/engine/headless/Preflight.h"
#include "include/engine/headless/RuntimeStatus.h"
#include "include/engine/supervisor/CoreSupervisor.h"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#include "include/sys/HostPresence.hpp"
#include "include/sys/KillSwitch.hpp"
#include "include/sys/Process.hpp"
#include "include/sys/ServiceManager.hpp"

#ifdef Q_OS_WIN
#include <QProcess>

#include "include/sys/windows/MiniDump.h"
#endif
#ifdef Q_OS_UNIX
#include <cerrno>
#include <cstring>
#include <vector>

#include <QFile>
#include <QThread>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

// Not declared by <unistd.h> on macOS.
extern char **environ;
#endif

namespace Engine {
    namespace {
        // Private: an exec'd image keeps the restart mode the first one resolved (HeadlessCli::EffectiveRestartMode reads it).
        constexpr char headlessMainInheritedModeVar[] = "THRONE_RESTART_INHERITED";
        // Private: the boot had a terminal and catches SIGHUP. A hang-up during the exec reaches the new image after the tty is gone.
        constexpr char headlessMainInheritedTtyVar[] = "THRONE_RESTART_TTY";
        // Private: set on every exec'd image; its boot then feeds the watchdog the reloading unit keeps armed. "reload" when the
        // predecessor's RELOADING is still open (no READY ended it), else "1".
        constexpr char headlessMainExecedVar[] = "THRONE_RESTART_EXECED";

        void headlessMainPrint(FILE *stream, const QString &text) {
#ifdef Q_OS_WIN
            if (_fileno(stream) < 0) return;
#endif
            std::fputs(text.toLocal8Bit().append('\n').constData(), stream);
            std::fflush(stream);
        }

        QJsonObject headlessMainStartingHealth(qint64 bootMonoMs) {
            return {
                {"ok", false},
                {"mode", "headless"},
                {"ready", false},
                {"state", "starting"},
                {"pid", QCoreApplication::applicationPid()},
                {"uptime_s", (MonotonicMs() - bootMonoMs) / 1000},
                {"version", QStringLiteral(NKR_VERSION)},
                {"headless_dialogs", HeadlessDialogCount()},
            };
        }

        QString headlessMainApiSummary(const RemoteApi::Overlay &overlay) {
            const QString source = overlay.listenSource == RemoteApi::ConfigSource::Flags ? QStringLiteral("flags")
                                                                                            : QStringLiteral("environment");
            if (overlay.enabled.has_value() && !*overlay.enabled) return "off (" + source + ")";
            if (overlay.port.has_value()) return "listener from the " + source;
            return QStringLiteral("from the settings, else 127.0.0.1:9095");
        }

        int headlessMainDbPrimary(const Configs::DbError &error) {
            return (error.extended > 0 ? error.extended : error.code) & 0xff;
        }

        // For an unusable database: READONLY or CANTOPEN (permissions), CORRUPT or NOTADB.
        QString headlessMainDbAdvice(const Configs::DbError &error, const QString &dbPath) {
            const int primary = headlessMainDbPrimary(error);
            if (primary == SQLITE_READONLY || primary == SQLITE_CANTOPEN) {
                return QStringLiteral("make %1 and its -wal and -shm files writable by the user Throne runs as").arg(dbPath);
            }
            return QStringLiteral("move %1 and its -wal and -shm files aside to start with an empty database, or restore them from a "
                                  "file backup; if the API listener came from the settings, pass --api-listen until the restore")
                .arg(dbPath);
        }

        // A return before exec() never emits aboutToQuit: without this the socket file stays, the next boot reports a crash,
        // and once init() ran the core and the guard outlive Throne.
        class HeadlessEarlyExit {
        public:
            explicit HeadlessEarlyExit(InstanceServer &server) : m_server(server) {}

            ~HeadlessEarlyExit() {
                if (!m_armed) return;
                if (m_initRan) {
                    if (auto *engine = host(); engine != nullptr) engine->shutdown(false);
                }
                m_server.close();
                QLocalServer::removeServer(m_server.name());
                Logging::Shutdown();
            }

            HeadlessEarlyExit(const HeadlessEarlyExit &) = delete;
            HeadlessEarlyExit &operator=(const HeadlessEarlyExit &) = delete;

            void initRan() { m_initRan = true; }

            void disarm() { m_armed = false; }

        private:
            InstanceServer &m_server;
            bool m_armed = true;
            bool m_initRan = false;
        };

        // An exec'd image whose predecessor left the unit reloading, exiting before SupervisorLink owns that reload: READY then
        // STOPPING, as SupervisorLink::abandonReload, or systemd <= 248 sits in "reloading" until its timeout.
        class HeadlessInheritedReload {
        public:
            explicit HeadlessInheritedReload(bool armed) : m_armed(armed) {}

            ~HeadlessInheritedReload() {
                if (!m_armed) return;
                Sys::ServiceManager::NotifyReady(QStringLiteral("Stopping"));
                Sys::ServiceManager::NotifyStopping(QStringLiteral("Stopping"));
            }

            HeadlessInheritedReload(const HeadlessInheritedReload &) = delete;
            HeadlessInheritedReload &operator=(const HeadlessInheritedReload &) = delete;

            // Whether it was armed: the caller hands the reload on.
            bool disarm() { return std::exchange(m_armed, false); }

        private:
            bool m_armed;
        };

#ifdef Q_OS_UNIX
        constexpr int headlessMainCoreReapWaitMs = 10 * 1000;
        constexpr int headlessMainCoreReapPollMs = 100;

        bool headlessMainHasTty() {
            const int fd = ::open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
            if (fd < 0) return false;
            ::close(fd);
            return true;
        }

        // ENXIO/EIO: the session lost its controlling terminal (a hang-up), as opposed to never having one.
        bool headlessMainTtyGone() {
            const int fd = ::open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
            if (fd >= 0) {
                ::close(fd);
                return false;
            }
            return errno == ENXIO || errno == EIO;
        }

        // Not SIG_IGN (nohup) or SIG_DFL.
        bool headlessMainHangupCaught() {
            struct sigaction current{};
            if (sigaction(SIGHUP, nullptr, &current) != 0) return false;
            return (current.sa_flags & SA_SIGINFO) != 0 || (current.sa_handler != SIG_IGN && current.sa_handler != SIG_DFL);
        }

        sigset_t headlessMainStopSignals() {
            sigset_t set;
            sigemptyset(&set);
            sigaddset(&set, SIGTERM);
            sigaddset(&set, SIGINT);
            sigaddset(&set, SIGHUP);
            return set;
        }

        // Signals that reached the pipe after exec() returned, judged by the runtime as the loop would have; true once one stops.
        bool headlessMainStopPending(HeadlessRuntime &runtime) {
            const QList<int> pending = Bootstrap::TakePendingSignals();
            for (const int signum : pending) {
                if (runtime.exitCode(ExitCode::Restart) != ExitCode::Restart) break;
                runtime.onSignal(signum);
            }
            return runtime.exitCode(ExitCode::Restart) != ExitCode::Restart;
        }

        // Returns only when the exec did not happen, with the code to exit with.
        int headlessMainExecSelf(const QString &exePath, const QList<QByteArray> &savedArgv, const QDir &launchCwd, DataDirLock &lock,
                                 HeadlessRuntime &runtime, bool inheritMode, bool hadTty) {
            // Blocked, a stop sent from now on stays pending across the exec instead of landing in a pipe nobody reads.
            const sigset_t stops = headlessMainStopSignals();
            sigset_t previous;
            pthread_sigmask(SIG_SETMASK, &stops, &previous);
            const bool hangupCaught = headlessMainHangupCaught();
            // Only a terminal that still opens now can hang up during the restart; one gone earlier (a shell that exited) cannot.
            const bool ttyAtStart = hadTty && hangupCaught && headlessMainHasTty();
            const auto giveUp = [&](int code) {
                // Unblocking delivers what is pending to the handler, which writes it to the pipe.
                pthread_sigmask(SIG_SETMASK, &previous, nullptr);
                return headlessMainStopPending(runtime) ? ExitCode::Ok : code;
            };
            const auto refuse = [&](const QString &why) {
                LOG_ERROR(QString("exec restart refused: %1; exiting with 75 instead").arg(why));
                runtime.noteExecRefused(why);
                return giveUp(ExitCode::Restart);
            };
            // The unit is reloading (SupervisorLink sent RELOADING), which keeps its watchdog armed, and the loop that pinged it has ended.
            const bool feedWatchdog = Sys::ServiceManager::WatchdogUsec() > 0;
            if (feedWatchdog) Sys::ServiceManager::NotifyWatchdog();
            // A live old core keeps passing its parent check against our unchanged PID.
            if (auto *engine = host(); engine != nullptr) {
                // The teardown already sent SIGKILL; DS_cores still runs its loop and reaps the core, which drops corePid() to 0.
                qint64 pid = engine->supervisor()->corePid();
                for (int waitedMs = 0; pid > 0 && waitedMs < headlessMainCoreReapWaitMs; waitedMs += headlessMainCoreReapPollMs) {
                    if (headlessMainStopPending(runtime)) return giveUp(ExitCode::Ok);
                    if (feedWatchdog && waitedMs > 0 && waitedMs % 1000 == 0) Sys::ServiceManager::NotifyWatchdog();
                    QThread::msleep(headlessMainCoreReapPollMs);
                    pid = engine->supervisor()->corePid();
                }
                if (pid > 0) {
                    return refuse(QString("the core (PID %1) is still running %2 s after it was killed")
                                      .arg(pid)
                                      .arg(headlessMainCoreReapWaitMs / 1000));
                }
            }
            // Relative --data-dir / --api-key-file values in the saved argv resolve against it again.
            if (::chdir(QFile::encodeName(launchCwd.absolutePath()).constData()) != 0) {
                const int error = errno;
                return refuse(QString("cannot return to the launch directory %1: %2")
                                  .arg(launchCwd.absolutePath(), QString::fromLocal8Bit(std::strerror(error))));
            }
            lock.release();

            std::vector<char *> args;
            args.reserve(static_cast<size_t>(savedArgv.size()) + 1);
            for (const auto &arg : savedArgv) args.push_back(const_cast<char *>(arg.constData()));
            args.push_back(nullptr);
            // A copy, not setenv(): other threads may still read the live environment. RunHeadless removed the markers from it.
            const QByteArray marker = QByteArray(headlessMainInheritedModeVar) + "=exec";
            const QByteArray ttyMarker = QByteArray(headlessMainInheritedTtyVar) + "=1";
            const QByteArray execedMarker = QByteArray(headlessMainExecedVar) + (runtime.reloadPending() ? "=reload" : "=1");
            std::vector<char *> envp;
            for (char **entry = environ; entry != nullptr && *entry != nullptr; ++entry) envp.push_back(*entry);
            if (inheritMode) envp.push_back(const_cast<char *>(marker.constData()));
            if (hadTty && hangupCaught) envp.push_back(const_cast<char *>(ttyMarker.constData()));
            envp.push_back(const_cast<char *>(execedMarker.constData()));
            envp.push_back(nullptr);
            const QByteArray exe = QFile::encodeName(exePath);
            LOG_INFO(QString("restarting in place: %1").arg(exePath));
            std::fflush(stdout);
            std::fflush(stderr);
            // Last look: another thread may have taken a signal while this one blocked them.
            if (headlessMainStopPending(runtime)) return giveUp(ExitCode::Ok);
            // The terminal closed during the restart; its SIGHUP could still land on a thread that dies with this image.
            if (ttyAtStart && headlessMainTtyGone()) {
                runtime.terminate(ExitCode::Ok, QStringLiteral("the terminal hung up during the restart"));
                return giveUp(ExitCode::Ok);
            }
            if (feedWatchdog) Sys::ServiceManager::NotifyWatchdog();
            ::execve(exe.constData(), args.data(), envp.data());
            const int error = errno;
            const QString why = QString::fromLocal8Bit(std::strerror(error));
            LOG_ERROR(QString("exec restart failed: %1: %2; exiting with 75 instead").arg(exePath, why));
            runtime.noteExecRefused(QString("%1 could not be executed: %2").arg(exePath, why));
            return giveUp(ExitCode::Restart);
        }
#endif

#ifdef Q_OS_WIN
        // WM_ENDSESSION: Windows may end the process as soon as the window procedure returns, so the teardown runs in here.
        void headlessMainSessionEnd() {
            const bool tearingDown = Bootstrap::ExitInProgress();
            // Turns a restart in progress into a stop: nothing may start again while the session ends.
            if (auto *runtime = HeadlessRuntime::current(); runtime != nullptr) {
                runtime->endSession(QStringLiteral("the Windows session is ending"));
            }
            if (auto *engine = host(); engine != nullptr) engine->shutdown(false);
            Logging::Shutdown();
            // The teardown above ran outside runTerminate, which would have ended the loop; a running one ends it by itself.
            if (!tearingDown) QCoreApplication::exit(ExitCode::Ok);
        }

        // No execve here: the new process waits for this one to exit (RelaunchParent) before it claims the data dir.
        int headlessMainStartAgain(const QString &exePath, const QStringList &arguments, const QDir &launchCwd, HeadlessRuntime &runtime) {
            QStringList args = arguments.mid(1);
            args << QStringLiteral("-wait_pid=%1").arg(QCoreApplication::applicationPid());
            qint64 pid = 0;
            if (QProcess::startDetached(exePath, args, launchCwd.absolutePath(), &pid)) {
                LOG_INFO(QString("restarting: started %1 as PID %2").arg(QDir::toNativeSeparators(exePath)).arg(pid));
                return ExitCode::Ok;
            }
            const QString why = QString("%1 could not be started").arg(QDir::toNativeSeparators(exePath));
            LOG_ERROR(QString("restart failed: %1; exiting with 75 instead").arg(why));
            runtime.noteExecRefused(why);
            return ExitCode::Restart;
        }
#endif
    } // namespace

    int RunHeadless(int argc, char **argv, const Bootstrap::Launch &launch) {
        const qint64 bootMonoMs = MonotonicMs();
#ifdef Q_OS_UNIX
        QList<QByteArray> savedArgv;
        for (int i = 0; i < argc; i++) savedArgv << QByteArray(argv[i]);
        const qint64 bootParentPid = ::getppid();
        // A closed journald or docker log pipe must fail the write, not kill Throne.
        ::signal(SIGPIPE, SIG_IGN);
#else
        const qint64 bootParentPid = 0;
#endif
#ifdef Q_OS_WIN
        Windows_SetCrashHandler(false);
        // A service runs in session 0, with no console to attach to.
        if (!launch.service) Bootstrap::AttachParentConsole();
#endif
        const bool client = launch.mode == Bootstrap::Mode::HeadlessClient;
        // Client modes answer on stdout for scripts; stderr carries only problems.
        Logging::EnableStdSink(client ? Logging::Level::Warn : Logging::Level::Info);

        // Declared first so it is released last; the exec restart releases it explicitly.
        DataDirLock lock;
        auto app = std::make_unique<QCoreApplication>(argc, argv);
        QCoreApplication::setApplicationName("Throne");
        QCoreApplication::setApplicationVersion(NKR_VERSION);
        // Now: the cached value survives an in-place upgrade and the working directory moving to <config>.
        const QString exePath = QCoreApplication::applicationFilePath();
        const QDir launchCwd = QDir::current();
        Bootstrap::SetSoftwareNames();

        qputenv("QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES", "1");
        const auto env = QProcessEnvironment::systemEnvironment();
        const QString execedValue = env.value(headlessMainExecedVar);
        const bool execed = !execedValue.isEmpty();
        // The snapshot keeps them for EffectiveRestartMode, hadTty and execed; the core and a later image must not inherit them.
        qunsetenv(headlessMainInheritedModeVar);
        qunsetenv(headlessMainInheritedTtyVar);
        qunsetenv(headlessMainExecedVar);
        // Covers every return before SupervisorLink takes the reload over; a predecessor that was never ready entered none, and
        // a READY from us would pass off a failed start as a success.
        HeadlessInheritedReload inheritedReload(execedValue == QLatin1String("reload") && Sys::ServiceManager::NotifyEnabled());
        QCommandLineParser parser;
        HeadlessCli::AddOptions(parser);
        HeadlessCli::LaunchOptions options;
        QString message;
        switch (HeadlessCli::Parse(parser, QCoreApplication::arguments(), env, launchCwd, launch, &options, &message)) {
            case HeadlessCli::ParseOutcome::Help:
                headlessMainPrint(stdout, HeadlessCli::HelpText(parser).trimmed());
                return ExitCode::Ok;
            case HeadlessCli::ParseOutcome::Version:
                headlessMainPrint(stdout, QStringLiteral("Throne " NKR_VERSION));
                return ExitCode::Ok;
            case HeadlessCli::ParseOutcome::Usage:
                headlessMainPrint(stderr, message + QStringLiteral("\nThe options: Throne --headless --help"));
                return ExitCode::Usage;
            case HeadlessCli::ParseOutcome::Run:
                break;
        }
        Logging::SetStdFormat(options.logFormat);
        if (options.debug) Logging::EnableStdSink(Logging::Level::Debug);

        RemoteApi::Overlay overlay;
        if (const int code = HeadlessApi::ParseOverlay(parser, env, launchCwd, &overlay, &message); code != ExitCode::Ok) {
            headlessMainPrint(stderr, message);
            return code;
        }

        Bootstrap::DataDir dataDir;
        if (const int code = HeadlessCli::ResolveDataDir(&options, env, !client, &dataDir, &message); code != ExitCode::Ok) {
            // --check exits 0 or 1 only: a data dir it cannot resolve is one of its failures.
            if (options.client == HeadlessCli::ClientMode::Check) {
                headlessMainPrint(stdout, QStringLiteral("[FAIL] data dir: ") + message);
                return ExitCode::Fatal;
            }
            headlessMainPrint(stderr, message);
            return code;
        }
        Configs::SetBasePath(dataDir.wd.absolutePath());
        const QString wdText = QDir::toNativeSeparators(dataDir.wd.absolutePath());

        // Client modes never take the lock, open the log or the database, or create anything.
        if (options.client == HeadlessCli::ClientMode::Health) return HeadlessInstance::RunHealth(dataDir.wd);
        if (options.client == HeadlessCli::ClientMode::PrintConnection) return HeadlessInstance::RunPrintConnection(dataDir.wd, overlay);
        if (options.client == HeadlessCli::ClientMode::Check) return Preflight::Run(options, env, overlay, dataDir);

        if (const int code = HeadlessApi::CheckExplicitKeyFile(&overlay, &message); code != ExitCode::Ok) {
            headlessMainPrint(stderr, message);
            return code;
        }

        QStringList arguments = QCoreApplication::arguments();
        const Bootstrap::RelaunchParent relaunchParent(arguments);

        const auto claim = Bootstrap::ClaimDataDir(lock, dataDir, Bootstrap::InstanceServerName(dataDir.wd), "headless", {},
                                                   relaunchParent, false);
        if (claim.outcome == Bootstrap::ClaimOutcome::HandedOff) {
            headlessMainPrint(stderr, QString("%1 is in use by an older Throne, which answered on its instance socket").arg(wdText));
            return ExitCode::DataDirOwned;
        }
        if (claim.outcome != Bootstrap::ClaimOutcome::Owned) {
            headlessMainPrint(stderr, QString("%1 is in use by %2")
                                          .arg(wdText, DataDirLock::DescribeOwner(DataDirLock::ReadOwner(dataDir.configDir))));
            return ExitCode::DataDirOwned;
        }
        if (!lock.isHeld()) {
            headlessMainPrint(stderr, QString("%1: refusing to run without the data dir lock").arg(wdText));
            return ExitCode::Fatal;
        }
        // Logs its own failure; the data-dir lock still covers this dir, so the run goes on.
        Sys::HostPresence::Publish(dataDir.wd.absolutePath());
        Sys::ServiceManager::NotifyStartProgress();

        // Only while holding the lock: QLocalServer's bind-and-rename replaces even a live socket file.
        const QString socketName = HeadlessInstance::SocketName(dataDir.wd, true);
        // The private-dir check already logged why.
        if (socketName.isEmpty()) return ExitCode::Fatal;
        // A LocalSystem pipe with UserAccessOption shuts administrators out, and WorldAccessOption lets any user add instances
        // and serve the name; the default named-pipe ACL gives only SYSTEM and Administrators write access.
        const auto socketAccess = (options.service || Sys::ServiceManager::RunningAsLocalSystem()) ? QLocalServer::NoOptions
                                                                                                   : QLocalServer::UserAccessOption;
        auto server = std::make_unique<InstanceServer>(socketName, socketAccess);
        if (!server->listen()) {
            const QString firstError = QString("%1: %2").arg(socketName, server->errorString());
#ifdef Q_OS_WIN
            // Another account may hold the pipe name; the data-dir lock already makes this process the only owner.
            LOG_WARN(QString("could not listen on the instance pipe %1; running without it, so --health, --print-connection and "
                             "--check cannot reach this process")
                         .arg(firstError));
#else
            // FAT, exFAT, sshfs or CIFS refuse socket nodes with EPERM/EOPNOTSUPP, which Qt reports only as an unknown error.
            const QString fallbackName = QFileInfo(socketName).fileName() == QLatin1String(".instance")
                                             ? HeadlessInstance::FallbackSocketName(dataDir.wd, true)
                                             : QString();
            if (fallbackName.isEmpty()) {
                headlessMainPrint(stderr, QString("could not listen on the instance socket %1").arg(firstError));
                return ExitCode::Fatal;
            }
            server = std::make_unique<InstanceServer>(fallbackName, socketAccess);
            if (!server->listen()) {
                headlessMainPrint(stderr, QString("could not listen on the instance socket %1, nor on %2: %3")
                                              .arg(firstError, fallbackName, server->errorString()));
                return ExitCode::Fatal;
            }
            LOG_WARN(QString("could not listen on the instance socket %1; listening on %2 instead").arg(firstError, fallbackName));
#endif
        }
        HeadlessInstance::Install(*server, [bootMonoMs] {
            if (const auto *runtime = HeadlessRuntime::current(); runtime != nullptr) return runtime->health();
            return headlessMainStartingHealth(bootMonoMs);
        });
        server->installQuitCleanup();
        // Destroyed after earlyExit, so its policy still answers the teardown an early return runs.
        std::unique_ptr<HeadlessRuntime> runtime;
        HeadlessEarlyExit earlyExit(*server);

#ifdef Q_OS_UNIX
        // Probed before the handlers queue a hang-up; across an exec the predecessor's answer counts, as the tty may be gone now.
        const bool hadTty = headlessMainHasTty() || env.value(headlessMainInheritedTtyVar) == QLatin1String("1");
        // The direct fallback would run the teardown in signal context: no pipe, no run.
        if (!Bootstrap::InstallTerminationHandlers(nullptr, {.hangup = true, .keepInheritedIgnore = true})) {
            headlessMainPrint(stderr, QStringLiteral("could not create the signal pipe"));
            return ExitCode::Fatal;
        }
        // An exec restart hands these over blocked, a stop sent during it still pending; the handlers queue them from here.
        const sigset_t stopSignals = headlessMainStopSignals();
        pthread_sigmask(SIG_UNBLOCK, &stopSignals, nullptr);
        // These give us a pty of their own and relay the terminal's hang-up, so our tty still opens after it.
        const bool parentRelaysHangup =
            QStringList{"sudo", "su", "doas", "run0", "script"}.contains(Bootstrap::ProcessName(bootParentPid));
#endif
#ifdef Q_OS_WIN
        // The SCM owns stop and shutdown for a service.
        if (!options.service) Sys::ServiceManager::InstallShutdownSignals(headlessMainSessionEnd);
#endif

        Bootstrap::StartLogging(dataDir);
        QString restartWhy;
        const auto restartMode = HeadlessCli::EffectiveRestartMode(options.restartMode, env, bootParentPid, &restartWhy);
        QStringList bootFields{
            QStringLiteral("Throne " NKR_VERSION " headless"),
            "PID " + QString::number(QCoreApplication::applicationPid()),
            "started by " + options.trigger,
            "data dir " + wdText + " (" + options.dataDirSource + ")",
            "restart mode " + HeadlessCli::RestartModeName(restartMode) + " (" + restartWhy + ")",
            QStringLiteral("resume ") + (options.resume ? "on" : "off"),
        };
        if (options.noKillSwitch) bootFields << QStringLiteral("kill switch overridden");
        bootFields << "API " + headlessMainApiSummary(overlay);
        LOG_INFO(bootFields.join(", "));
        // Logging::Init writes this to the file only; stderr and journald should see it too.
        if (Logging::PreviousSessionCrashed()) {
            LOG_WARN("the previous session did not shut down cleanly (a crash, kill -9 or power loss); HEALTH reports previous_crash");
        }

        // Migrations run synchronously in OpenDataDir; the start timeout must not cut them short.
        if (Sys::ServiceManager::NotifyEnabled()) {
            Sys::ServiceManager::NotifyExtendTimeout(120LL * 1000 * 1000);
            // After an exec restart the unit is reloading with its watchdog still armed; nothing has pinged since before execve. A
            // fresh start must not ping: that would arm the watchdog over the whole synchronous boot.
            if (execed && Sys::ServiceManager::WatchdogUsec() > 0) Sys::ServiceManager::NotifyWatchdog();
        }
        try {
            Bootstrap::OpenDataDir(dataDir, options.openDataDirArgs);
        } catch (const std::exception &e) {
            const auto error = Configs::DescribeDbError(e);
            const int primary = headlessMainDbPrimary(error);
            const QDir configDir(dataDir.configDir);
            QString dbFile = configDir.absoluteFilePath(QStringLiteral("throne.db"));
            bool denied = false;
            if (primary == SQLITE_CANTOPEN || primary == SQLITE_READONLY) {
                // The stats DB recreates itself unless it cannot even be moved aside (a Windows ACL denying DELETE).
                for (const QString &name : {QStringLiteral("throne.db"), QStringLiteral("throne_stats.db")}) {
                    if (const QString file = configDir.absoluteFilePath(name); Configs::DbFilesDenied(file)) {
                        dbFile = file;
                        denied = true;
                        break;
                    }
                }
            }
            // Restarting cannot fix the file itself, so these are not the restart-worthy 1.
            if (Configs::IsFatalDbError(error) || (primary == SQLITE_CANTOPEN && denied)) {
                const QString dbPath = QDir::toNativeSeparators(dbFile);
                LOG_ERROR(QString("EXIT 78: the database %1 is unusable (%2); %3")
                              .arg(dbPath, QString::fromLocal8Bit(e.what()), headlessMainDbAdvice(error, dbPath)));
                return ExitCode::Config;
            }
            LOG_ERROR(QString("EXIT 1: could not open the database in %1: %2")
                          .arg(QDir::toNativeSeparators(dataDir.configDir), QString::fromLocal8Bit(e.what())));
            return ExitCode::Fatal;
        }
        Sys::ServiceManager::NotifyStartProgress();
        Bootstrap::PrepareRuntime();
        HeadlessApi::ResolveKeys(overlay, dataDir.configDir);

        API::defaultClient = new API::Client();
        runtime = std::make_unique<HeadlessRuntime>(options, dataDir, bootMonoMs);
#ifdef Q_OS_UNIX
        runtime->setHadTty(hadTty);
        runtime->setParentRelaysHangup(parentRelaysHangup);
        runtime->setInPlaceRestart(restartMode == HeadlessCli::RestartMode::Exec);
#endif
#ifdef Q_OS_WIN
        const QString stopSource = options.service ? QStringLiteral("service stop")
                                                   : QStringLiteral("stop request (Ctrl+C, closing the console, signing out or taskkill)");
        Sys::ServiceManager::SetStopHandler([stopSource](Sys::ServiceManager::StopKind kind) {
            if (auto *current = HeadlessRuntime::current(); current != nullptr) {
                current->onStopRequest(kind == Sys::ServiceManager::StopKind::Fast, stopSource);
            }
        });
#endif
        Configs_sys::SetIsolatedChildren(true);
        auto *engine = EngineHost::create(RuntimeOptions::Headless(options.resume, options.noKillSwitch));
        runtime->attach(engine);
        const auto mark = [rt = runtime.get()](RuntimeStatus::Step step) {
            if (auto *status = rt->status(); status != nullptr) status->mark(step);
        };

        const auto bind = HeadlessApi::Bind();
        if (bind.exitCode != ExitCode::Ok) {
            LOG_ERROR(QString("EXIT %1: the Throne API listener could not start").arg(bind.exitCode));
            return bind.exitCode;
        }
        if (!bind.retry) mark(RuntimeStatus::ApiStep);

        engine->init(runtime->policy(), nullptr, runtime->bootStart());
        earlyExit.initRan();
        if (engine->supervisor()->state() == CoreState::ListenFailed) {
            LOG_ERROR(QStringLiteral("EXIT 1: the core IPC listener could not start"));
            return ExitCode::Fatal;
        }
        Sys::ServiceManager::NotifyStartProgress();

        Configs::dataManager->RunDeferredMaintenance();
        Sys::KillSwitch::instance()->whenSettled(runtime.get(), [mark](bool) { mark(RuntimeStatus::KillSwitchStep); });

        runtime->beforeExec();
        if (bind.retry) {
            HeadlessApi::StartBindRetry(
                runtime.get(), [mark] { mark(RuntimeStatus::ApiStep); },
                [rt = runtime.get()](int code) {
                    rt->terminate(code, code == ExitCode::Config ? QStringLiteral("the Throne API listener could not start")
                                                                 : QStringLiteral("the Throne API listen address never became available"));
                });
        }
        // ApiStep is already marked for that failure; mark() is idempotent.
        HeadlessApi::StartQuietRetry(runtime.get(), [mark] { mark(RuntimeStatus::ApiStep); });
        HeadlessApi::WatchExplicitAddress(runtime.get());
#ifdef Q_OS_UNIX
        Bootstrap::ArmSignalHandlers([rt = runtime.get()](int signum) { rt->onSignal(signum); });
#elif defined(Q_OS_WIN)
        Sys::ServiceManager::ArmStopHandler();
#endif
        earlyExit.disarm();
        // From here SupervisorLink ends the inherited reload: its READY, or abandonReload below.
        if (inheritedReload.disarm()) runtime->inheritReload();

        const int result = QCoreApplication::exec();

        // A stray quit() that skipped the teardown.
        if (!Bootstrap::ExitInProgress()) {
            LOG_WARN(QStringLiteral("the event loop ended without a shutdown; shutting down now"));
            engine->shutdown(false);
        }
#ifdef Q_OS_WIN
        Sys::ServiceManager::SignalShutdownComplete();
#endif
        const int code = runtime->exitCode(result);
        if (code == ExitCode::Restart && restartMode == HeadlessCli::RestartMode::Exec) {
#ifdef Q_OS_UNIX
            const int failed = headlessMainExecSelf(exePath, savedArgv, launchCwd, lock, *runtime,
                                                    options.restartMode == HeadlessCli::RestartMode::Auto, hadTty);
            runtime->abandonReload();
            return failed;
#elif defined(Q_OS_WIN)
            return headlessMainStartAgain(exePath, arguments, launchCwd, *runtime);
#endif
        }
        // A stop that overrode an in-place restart during the teardown.
        runtime->abandonReload();
        return code;
    }
} // namespace Engine
