#include "include/engine/supervisor/CoreSupervisor.h"

#include "include/api/RPC.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/Lifecycle.h"
#include "include/engine/ThreadCheck.h"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#include "include/global/Notice.hpp"
#include "include/sys/KillSwitch.hpp"
#include "include/sys/Process.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include <QCoreApplication>
#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMutexLocker>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>

// After every other header, so the protobuf and Qt headers above never see its macros.
#ifdef Q_OS_WIN
#include <windows.h>
// <windows.h> defines SetPort, which under unity builds clobbers Configs::outbound::SetPort.
#undef SetPort
#else
#ifdef Q_OS_LINUX
#include <sys/socket.h>
#endif
#ifdef Q_OS_MACOS
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <unistd.h>
#endif

namespace {
    // Must match egressForwardingMarker in core/internal/rpc/forwarding_windows.go.
    constexpr char kCoreSupervisorEgressMarker[] = "IPv4 forwarding is enabled on the Tun egress adapter: ";

    void coreSupervisorWarnEgressForwarding(const QByteArray &log) {
        const auto at = log.indexOf(kCoreSupervisorEgressMarker);
        if (at < 0) return;
        const auto from = at + qsizetype(sizeof(kCoreSupervisorEgressMarker) - 1);
        const auto end = log.indexOf('\n', from);
        const auto adapter = QString::fromUtf8(log.mid(from, end < 0 ? -1 : end - from)).trimmed();
        PostNotice({NoticeLevel::Warning, QObject::tr("IPv4 forwarding breaks Tun mode"),
                    QObject::tr("IPv4 forwarding is on for the network adapter \"%1\", usually because Mobile Hotspot or Internet Connection Sharing is sharing it.\n\n"
                                "Windows then ignores the adapter binding that keeps Throne's own connections out of the Tun, so they loop back into it and fail.\n\n"
                                "To fix this, share the hotspot from the throne-tun adapter instead of \"%1\" (Settings > Mobile hotspot > Share my internet connection from), or turn the hotspot off while using Tun mode.")
                        .arg(adapter)});
    }

#ifdef Q_OS_MACOS
    // A dead core cannot put back the system DNS it pointed at its lo0 listener; the setuid core replays its own record.
    void coreSupervisorRestoreSystemDns(const QString &corePath) {
        auto *restore = new QProcess;
        restore->setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(restore, &QProcess::finished, restore, [restore](int exitCode, QProcess::ExitStatus exitStatus) {
            const auto output = QString::fromUtf8(restore->readAll()).trimmed();
            if (!output.isEmpty()) MW_show_log(output);
            if (exitStatus != QProcess::NormalExit || exitCode != 0)
                Logging::Write(Logging::Level::Warn, QString("system DNS restore exited: code=%1").arg(exitCode));
            restore->deleteLater();
        });
        QObject::connect(restore, &QProcess::errorOccurred, restore, [restore](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart) return;
            Logging::Write(Logging::Level::Warn, "system DNS restore failed to start: " + restore->errorString());
            restore->deleteLater();
        });
        restore->start(corePath, {QStringLiteral("--restore-dns")});
    }
#endif

    // Main thread: the full reply onPrivilegeProbed is handling (its signature carries only the privileged bit).
    std::optional<API::PrivilegeInfo> coreSupervisorProbeReply;

    constexpr qsizetype kCoreSupervisorErrorMaxChars = 300;
    constexpr qsizetype kCoreSupervisorStderrLineCap = 4096;

    // DS_cores only: one core generation's stderr, reassembled into lines across reads.
    struct CoreSupervisorStderrTail {
        quint64 gen = 0;
        QByteArray partial;
        QString panic;
        QString last;
    };

    bool coreSupervisorIsPanicLine(const QString &line) {
        return line.startsWith(QLatin1String("Core panicked:")) || line.startsWith(QLatin1String("panic:")) ||
               line.startsWith(QLatin1String("fatal error:"));
    }

    QString coreSupervisorOneLine(QString text) {
        for (auto &c : text) {
            if (c.category() == QChar::Other_Control) c = u' ';
        }
        text = text.simplified();
        if (text.size() > kCoreSupervisorErrorMaxChars) text = text.left(kCoreSupervisorErrorMaxChars - 3) + QStringLiteral("...");
        return text;
    }

    // Keeps the first panic line, else the last unindented line that is no goroutine header or register dump.
    void coreSupervisorNoteStderrLine(CoreSupervisorStderrTail &tail, QByteArray line) {
        if (!tail.panic.isEmpty()) return;
        if (line.endsWith('\r')) line.chop(1);
        if (line.isEmpty() || line.startsWith(' ') || line.startsWith('\t')) return;
        static const QRegularExpression goLogStamp(QStringLiteral(R"(^\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}(\.\d+)? )"));
        static const QRegularExpression registerDump(QStringLiteral(R"(^[a-z][a-z0-9]{0,7}\s+0x[0-9a-f]+$)"));
        QString text = QString::fromUtf8(line);
        text.remove(goLogStamp);
        text = coreSupervisorOneLine(text);
        if (text.isEmpty()) return;
        if (coreSupervisorIsPanicLine(text)) {
            tail.panic = text;
            return;
        }
        if (text.startsWith(QLatin1String("goroutine ")) || registerDump.match(text).hasMatch()) return;
        tail.last = text;
    }

    void coreSupervisorFeedStderr(CoreSupervisorStderrTail &tail, quint64 gen, const QByteArray &chunk) {
        if (tail.gen != gen) tail = CoreSupervisorStderrTail{gen};
        if (!tail.panic.isEmpty()) return;
        tail.partial += chunk;
        qsizetype from = 0;
        for (qsizetype end = tail.partial.indexOf('\n'); end >= 0; end = tail.partial.indexOf('\n', from)) {
            coreSupervisorNoteStderrLine(tail, tail.partial.mid(from, end - from));
            from = end + 1;
        }
        tail.partial.remove(0, from);
        if (tail.partial.size() > kCoreSupervisorStderrLineCap) tail.partial.clear();
    }

    // At the exit of generation gen: its cause line, empty when it said nothing usable.
    QString coreSupervisorStderrCause(CoreSupervisorStderrTail &tail, quint64 gen) {
        if (tail.gen != gen) return {};
        if (!tail.partial.isEmpty()) {
            const QByteArray rest = tail.partial;
            tail.partial.clear();
            coreSupervisorNoteStderrLine(tail, rest);
        }
        return tail.panic.isEmpty() ? tail.last : tail.panic;
    }
} // namespace

namespace Engine {
    CoreSupervisor::CoreSupervisor(QObject *parent) : QObject(parent) {
        // Queued to the main thread: the marker is seen on DS_cores.
        connect(EngineEvents::instance(), &EngineEvents::extraCoreExited, this, [] {
            Lifecycle::StopRunningProfile(false, false, false, true);
        });
        m_restartTimer = new QTimer(this);
        m_restartTimer->setSingleShot(true);
        connect(m_restartTimer, &QTimer::timeout, this, [this] { postRestart(); });
        m_connectTimer = new QTimer(this);
        m_connectTimer->setSingleShot(true);
        connect(m_connectTimer, &QTimer::timeout, this, [this] { onConnectTimeout(); });
    }

    CoreSupervisor::~CoreSupervisor() {
        // The process stays alive on DS_cores (never deleted); its handlers capture this object.
        QMutexLocker lock(&m_processMutex);
        if (m_process) QObject::disconnect(m_process, nullptr, m_process, nullptr);
    }

    bool CoreSupervisor::listen() {
        m_corePath = Configs::FindCoreRealPath();

        m_debugMode = (Configs::dataManager->settingsRepo->log_level == "debug");

        Configs::dataManager->settingsRepo->core_socket_name =
            "throneIPC-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_server = new QLocalServer(this);
        m_server->setSocketOptions(QLocalServer::UserAccessOption);
        const bool listening = m_server->listen(Configs::dataManager->settingsRepo->core_socket_name);
        if (!listening) {
            const QString error = m_server->errorString();
            qWarning() << "Failed to start IPC server:" << error;
            m_lastFailure = tr("Throne could not open the local channel the core connects to (%1), so no profile can start.").arg(error);
            setState(CoreState::ListenFailed);
            emit listenFailed(error);
            PostNotice({NoticeLevel::Error, tr("Core unavailable"), m_lastFailure});
        }

        connect(m_server, &QLocalServer::newConnection, this, [this] { onNewConnection(); });
        return listening;
    }

    void CoreSupervisor::start(int startWhenUpProfileId) {
        if (m_state != CoreState::NotStarted || m_server == nullptr) return;
        if (startWhenUpProfileId >= 0) m_startWhenUp = startWhenUpProfileId;
        setState(CoreState::Starting);
        auto socketFullName = m_server->fullServerName();
        runOnThread(
            [this, corePath = m_corePath, socketFullName, debugMode = m_debugMode] {
                QMutexLocker lock(&m_processMutex);
                m_process = new Configs_sys::CoreProcess(corePath, socketFullName, debugMode);
                attachProcess(m_process);
                m_process->Start();
            },
            DS_cores);
    }

    void CoreSupervisor::shutdown(int killWaitMs) {
        // First: a restart firing inside the nested wait below would spawn a core during the exit.
        setState(CoreState::ShuttingDown);
        m_probing = false;
        m_restartTimer->stop();
        m_connectTimer->stop();
        runOnThread([this, killWaitMs] {
            if (m_process) m_process->Kill(killWaitMs);
        }, DS_cores, true);
        if (m_server) m_server->close();
    }

    void CoreSupervisor::killForExit() {
        ENGINE_CHECK_MAIN_THREAD();
        setState(CoreState::ShuttingDown);
        m_probing = false;
        m_restartTimer->stop();
        m_connectTimer->stop();
        runOnThread([this] {
            if (!m_process) return;
            Configs_sys::KillChildGroup(m_process);
            m_process->kill();
        }, DS_cores);
    }

    bool CoreSupervisor::resetCore() {
        if (m_state == CoreState::ListenFailed) MW_show_log("[ERROR] " + m_lastFailure);
        if (!canStartCore() || exiting()) return false;
        const CoreState was = m_state;
        beginRestart();
        if (was == CoreState::Backoff || was == CoreState::GaveUp) {
            // That exit was already handled, so no exit report would restart the core.
            m_startWhenUp = -1;
            Configs::dataManager->settingsRepo->started_id = Configs::NoProfileId;
            MW_show_log("[Warn] " + QObject::tr("Restarting the core ..."));
            postProcessRestart([] { Lifecycle::StopRunningProfile(true, true, true); });
            return true;
        }
        runOnThread([this] {
            if (!m_process) return;
            Lifecycle::StopRunningProfile(true, true, true);
            // With nothing running the stop leaves a crashed profile's started_id, which the exit would resume.
            Configs::dataManager->settingsRepo->started_id = Configs::NoProfileId;
            m_expectedExitGen = m_process->generation.load();
            m_process->Kill();
        }, DS_cores);
        return true;
    }

    bool CoreSupervisor::restartNotListening(int profileId) {
        if (!canStartCore() || exiting()) return false;
        beginRestart();
        m_startWhenUp = profileId;
        postProcessRestart();
        return true;
    }

    void CoreSupervisor::restartForGuard() {
        if (!canStartCore() || exiting()) return;
        beginRestart();
        postProcessRestart([] { Lifecycle::StopRunningProfile(true, true, false); });
    }

    void CoreSupervisor::killForPrivilegeChange() {
        const CoreState current = m_state;
        if (current == CoreState::ListenFailed || current == CoreState::ShuttingDown) return;
        if (current == CoreState::Backoff || current == CoreState::GaveUp) {
            // Nothing to kill, and the restart timer may only be touched on the main thread.
            QMetaObject::invokeMethod(this, [this] {
                restartDeadCore(Configs::dataManager->settingsRepo->started_id);
            }, Qt::QueuedConnection);
            return;
        }
        runOnThread([this] {
            if (!m_process) return;
            m_expectedExitGen = m_process->generation.load();
            m_process->Kill();
        }, DS_cores, true);
    }

    bool CoreSupervisor::setStartWhenUp(int profileId) {
        QMutexLocker lock(&m_processMutex);
        const bool haveCore = m_process != nullptr;
        if (haveCore) m_startWhenUp = profileId;
        return haveCore;
    }

    void CoreSupervisor::cancelResume() {
        ENGINE_CHECK_MAIN_THREAD();
        m_startWhenUp = -1;
        Configs::dataManager->settingsRepo->started_id = Configs::NoProfileId;
        emit resumeCancelled();
    }

    bool CoreSupervisor::awaitingCoreStarted() const {
        return m_probing;
    }

    int CoreSupervisor::startWhenUp() const {
        return m_startWhenUp;
    }

    bool CoreSupervisor::canStartCore() const {
        const CoreState current = m_state;
        return current != CoreState::ListenFailed && current != CoreState::ShuttingDown;
    }

    CoreState CoreSupervisor::state() const {
        return m_state;
    }

    QString CoreSupervisor::lastFailure() const {
        return m_lastFailure;
    }

    QString CoreSupervisor::lastCoreError() const {
        return m_lastCoreError;
    }

    qint64 CoreSupervisor::corePid() const {
        QMutexLocker lock(&m_processMutex);
        return m_process ? m_process->processId() : 0;
    }

    int CoreSupervisor::restartCount() const {
        return m_restartCount;
    }

    int CoreSupervisor::consecutiveFailures() const {
        return m_failures;
    }

    bool CoreSupervisor::runningWithoutGuardIdentity() const {
#ifdef Q_OS_WIN
        return false;
#else
        // A restart for the guard could never give it the identity here.
        if (Sys::KillSwitch::instance()->identityUnsupported()) return false;
        QMutexLocker lock(&m_processMutex);
        return m_process != nullptr && Configs::dataManager->settingsRepo->core_running && !m_process->guard_identity;
#endif
    }

    // Caller must hold m_processMutex (reads m_process lock-free by design).
    bool CoreSupervisor::verifyCorePid(QLocalSocket *socket) const {
        if (!m_process) return false;
        qint64 expectedPid = m_process->processId();
        if (expectedPid <= 0) return false;

#if defined(Q_OS_LINUX)
        struct ucred cred = {};
        socklen_t credLen = sizeof(cred);
        if (getsockopt(static_cast<int>(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &cred, &credLen) == 0) {
            return static_cast<qint64>(cred.pid) == expectedPid;
        }
        return false;
#elif defined(Q_OS_MACOS)
        pid_t pid = 0;
        socklen_t pidLen = sizeof(pid);
        if (getsockopt(static_cast<int>(socket->socketDescriptor()), SOL_LOCAL, LOCAL_PEERPID, &pid, &pidLen) == 0) {
            return static_cast<qint64>(pid) == expectedPid;
        }
        return false;
#elif defined(Q_OS_WIN)
        ULONG pid = 0;
        HANDLE hPipe = reinterpret_cast<HANDLE>(static_cast<qintptr>(socket->socketDescriptor()));
        if (GetNamedPipeClientProcessId(hPipe, &pid)) {
            return static_cast<qint64>(pid) == expectedPid;
        }
        return false;
#else
        Q_UNUSED(socket)
        return true;
#endif
    }

    void CoreSupervisor::onNewConnection() {
        ENGINE_CHECK_MAIN_THREAD();
        auto socket = m_server->nextPendingConnection();
        quint64 gen = 0;
        {
            // Hold m_processMutex: DS_cores may still be constructing m_process.
            QMutexLocker lock(&m_processMutex);
            // Read before the pid check: a restart in between bumps it, and the new pid then fails the check.
            if (m_process) gen = m_process->generation.load();
            if (!verifyCorePid(socket)) {
                MW_show_log("[Warn] IPC connection from unexpected process rejected");
                socket->close();
                socket->deleteLater();
                return;
            }
        }
        if (gen <= m_handledExitGen || gen <= m_timedOutGen) {
            // A restart or kill is already queued for this core; its successor connects instead.
            socket->close();
            socket->deleteLater();
            return;
        }
        setupRpc(socket);
        Configs::dataManager->settingsRepo->core_running = true;
        // A mode restore or resumed profile would outlive the exit that is tearing them down.
        if (exiting()) return;
        if (gen > m_acceptedGen) {
            m_acceptedGen = gen;
            m_connectedAt.start();
        }
        m_connectTimer->stop();
        setState(CoreState::Connected);
#ifdef Q_OS_WIN
        // Local and quick, and it writes windows_set_admin, so it stays here on the main thread.
        LOG_INFO(QString("elevated: %1").arg(Configs::IsAdmin(true) ? "yes" : "no"));
        emitCoreStarted();
#else
        m_probing = true;
        startPrivilegeProbe(m_acceptedGen);
#endif
    }

    void CoreSupervisor::startPrivilegeProbe(quint64 gen, int attempt) {
        const QPointer<CoreSupervisor> self(this);
        runOnNewThread([self, gen, attempt] {
            bool ok = false;
            const API::PrivilegeInfo info = API::defaultClient->Privileges(&ok, kPrivilegeProbeTimeoutMs);
            // Posted to the application object, not to self: QPointer is only safe to dereference on the main thread.
            QMetaObject::invokeMethod(QCoreApplication::instance(), [self, gen, ok, info, attempt] {
                if (!self) return;
                coreSupervisorProbeReply = info;
                self->onPrivilegeProbed(gen, ok, info.privileged, attempt);
                coreSupervisorProbeReply.reset();
            }, Qt::QueuedConnection);
        });
    }

    void CoreSupervisor::onPrivilegeProbed(quint64 gen, bool ok, bool privileged, int attempt) {
        ENGINE_CHECK_MAIN_THREAD();
        if ((attempt == 0 && !m_probing) || gen != m_acceptedGen || m_state != CoreState::Connected || exiting()) return;
        // A dropped channel means the core is exiting (it never redials); its exit report settles the pending start.
        if (!ok && !API::defaultClient->IsConnected()) return;
        // On failure the old answer stays cached, as IsAdmin(true) keeps it.
        if (ok) {
            Configs::SetKnownAdmin(privileged);
            if (coreSupervisorProbeReply) Configs::SetKnownCapabilities(*coreSupervisorProbeReply);
        }
        QString elevated = QString("elevated: %1").arg(ok && privileged ? "yes" : "no");
        if (ok && coreSupervisorProbeReply && !coreSupervisorProbeReply->detail.isEmpty())
            elevated += QString(" (%1)").arg(coreSupervisorProbeReply->detail);
        if (attempt > 0) elevated += QString(" (retry %1)").arg(attempt);
        LOG_INFO(elevated);
        if (!ok && attempt < kPrivilegeProbeRetries) {
            QTimer::singleShot(kPrivilegeProbeRetryMs, this, [this, gen, attempt] {
                if (gen == m_acceptedGen && m_state == CoreState::Connected && !exiting()) startPrivilegeProbe(gen, attempt + 1);
            });
        }
        if (attempt > 0) return;
        // Before the emit: a start from a coreStarted slot would otherwise wait for a second coreStarted.
        m_probing = false;
        emitCoreStarted();
    }

    void CoreSupervisor::emitCoreStarted() {
        // Reset here rather than at the accept, so a core that dies during the probe counts as never connected.
        m_startFailures = 0;
        emit coreStarted(std::exchange(m_startWhenUp, -1));
    }

    void CoreSupervisor::setupRpc(QLocalSocket *socket) {
        API::defaultClient->Reconnect(socket);
        emit rpcConnected();
    }

    void CoreSupervisor::attachProcess(Configs_sys::CoreProcess *process) {
        const QPointer<CoreSupervisor> self(this);
        const auto postExit = [self](quint64 gen, bool failedToStart, int exitCode, const QString &cause) {
            if (auto *supervisor = self.data()) {
                QMetaObject::invokeMethod(supervisor, [supervisor, gen, failedToStart, exitCode, cause] {
                    supervisor->noteCoreError(gen, cause);
                    supervisor->onCoreExited(gen, failedToStart, exitCode);
                }, Qt::QueuedConnection);
            }
        };
        const auto stderrTail = std::make_shared<CoreSupervisorStderrTail>();
        // The process is the context, so every handler runs on DS_cores, where the process lives.
        connect(process, &QProcess::readyReadStandardOutput, process, [this, process] {
            auto log = process->readAllStandardOutput();
            if (log.contains("Extra process exited unexpectedly"))
            {
                MW_show_log("Extra Core exited, stopping profile...");
                EngineEvents::ExtraCoreExited();
            }
            coreSupervisorWarnEgressForwarding(log);
            // The window opens at the first chunk after the last one expired; the pre-add count is compared, so the crossing chunk still shows.
            const qint64 now = MonotonicMs();
            if (now - m_logWindowStartMs >= kLogWindowMs) {
                m_logWindowStartMs = now;
                m_logWindowLines = 0;
            }
            const int linesBefore = m_logWindowLines;
            m_logWindowLines += static_cast<int>(log.count("\n"));
            if (linesBefore > Configs::dataManager->settingsRepo->max_log_line) return;
            MW_show_log(log);
        });
        connect(process, &QProcess::readyReadStandardError, process, [process, stderrTail] {
            const auto log = process->readAllStandardError();
            coreSupervisorFeedStderr(*stderrTail, process->generation.load(), log);
            MW_show_log(log.trimmed());
        });
        connect(process, &QProcess::started, process, [self, process] {
            const quint64 gen = process->generation.load();
            if (auto *supervisor = self.data()) {
                QMetaObject::invokeMethod(supervisor, [supervisor, gen] { supervisor->onCoreSpawned(gen); }, Qt::QueuedConnection);
            }
        });
        connect(process, &QProcess::errorOccurred, process, [process, postExit](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                process->failed_to_start = true;
                MW_show_log("start core error occurred: " + process->errorString() + "\n");
            }
            LOG_ERROR(QString("core process error %1: %2").arg(static_cast<int>(error)).arg(process->errorString()));
            // A start failing inside Restart() on Windows reports only here: stateChanged then still sees restarting.
            if (error == QProcess::FailedToStart) postExit(process->generation.load(), true, -1, coreSupervisorOneLine(process->errorString()));
        });
        connect(process, &QProcess::finished, process, [](int exitCode, QProcess::ExitStatus exitStatus) {
            const bool crashed = exitStatus == QProcess::CrashExit || exitCode != 0;
            Logging::Write(crashed ? Logging::Level::Error : Logging::Level::Info,
                           QString("core process exited: code=%1 status=%2%3")
                               .arg(exitCode)
                               .arg(exitStatus == QProcess::CrashExit ? "crash" : "normal")
                               .arg(Configs::dataManager->settingsRepo->prepare_exit ? " (during shutdown)" : ""));
        });
        const auto runningGen = std::make_shared<quint64>(0);
        connect(process, &QProcess::stateChanged, process, [process, postExit, stderrTail, runningGen](QProcess::ProcessState state) {
            if (state == QProcess::Running) {
                *runningGen = process->generation.load();
                return;
            }
            if (state != QProcess::NotRunning) return;
            // Read first: a Restart() queued behind the blocking cleanup below runs inside its nested loop.
            const quint64 gen = process->generation.load();
            // Whether the process that just stopped reached Running; a core outliving Restart()'s wait is still reported (under the new gen).
            const bool reachedRunning = std::exchange(*runningGen, quint64(0)) != 0;
            const bool failedToStart = process->failed_to_start;
            // QProcess stores the exit status before it reports NotRunning.
            const int exitCode = failedToStart || process->exitStatus() == QProcess::CrashExit ? -1 : process->exitCode();
            const QString cause = failedToStart ? coreSupervisorOneLine(process->errorString()) : coreSupervisorStderrCause(*stderrTail, gen);
            Configs::dataManager->settingsRepo->core_running = false;
            qDebug() << "Core stated changed to not running";

            if (Configs::dataManager->settingsRepo->prepare_exit || process->restarting) return;

            if (!failedToStart) {
                MW_show_log("[Fatal] " + QObject::tr("Core exited, cleaning up..."));

                Lifecycle::StopRunningProfile(true, true, false);
#ifdef Q_OS_MACOS
                coreSupervisorRestoreSystemDns(process->program);
#endif
            }
            // Unix reports a failed exec as NotRunning before errorOccurred(FailedToStart), which posts it with errorString().
            if (!failedToStart && !reachedRunning) return;
            postExit(gen, failedToStart, exitCode, cause);
        });
    }

    void CoreSupervisor::onCoreSpawned(quint64 gen) {
        // The accept can be dispatched before this post arrives.
        if (m_state != CoreState::Starting || gen <= m_acceptedGen || exiting()) return;
        m_connectGen = gen;
        m_connectTimer->start(kConnectTimeoutMs);
    }

    void CoreSupervisor::onConnectTimeout() {
        if (m_state != CoreState::Starting || exiting()) return;
        const quint64 gen = m_connectGen;
        if (gen <= m_acceptedGen || gen != currentGeneration()) return;
        m_timedOutGen = gen;
        MW_show_log("[Warn] " + tr("The core did not connect within %1 s, restarting it.").arg(kConnectTimeoutMs / 1000));
        // Not marked expected, so the exit counts as a failure.
        runOnThread([this, gen] {
            if (m_process && m_process->generation.load() == gen) m_process->Kill();
        }, DS_cores);
    }

    void CoreSupervisor::noteCoreError(quint64 gen, const QString &line) {
        // A replaced core's late report and an intentional kill say nothing about why the core fails.
        if (gen <= m_handledExitGen || gen < m_lastCoreErrorGen || m_expectedExitGen.load() == gen) return;
        m_lastCoreErrorGen = gen;
        m_lastCoreError = gen == m_timedOutGen && !coreSupervisorIsPanicLine(line)
                              ? QStringLiteral("did not connect within %1 s").arg(kConnectTimeoutMs / 1000)
                              : line;
    }

    void CoreSupervisor::onCoreExited(quint64 gen, bool failedToStart, int exitCode) {
        // stateChanged and errorOccurred can both report one exit, in a platform-dependent order.
        if (gen <= m_handledExitGen) return;
        m_handledExitGen = gen;
        // This core never sent coreStarted, so it took no start and is treated as one that never connected.
        const bool wasProbing = m_probing && gen == m_acceptedGen;
        if (wasProbing) m_probing = false;
        if (exiting()) return;
        m_connectTimer->stop();

        const auto &settings = Configs::dataManager->settingsRepo;
        if (m_expectedExitGen.load() == gen) {
            m_failures = 0;
            m_startFailures = 0;
            // A start still waiting for the probing core carries over to the restarted one.
            if (!wasProbing || m_startWhenUp < 0) m_startWhenUp = settings->started_id;
            MW_show_log("[Warn] " + QObject::tr("Restarting the core ..."));
            scheduleRestart(kIntentRestartDelayMs);
            return;
        }

        const bool wasConnected = !failedToStart && gen == m_acceptedGen && !wasProbing;
        auto *killSwitch = Sys::KillSwitch::instance();
        if (!wasConnected && exitCode == Sys::KillSwitch::kCoreIdentityExitCode && !killSwitch->identityUnsupported()) {
            // Not a start failure: the kill switch fails closed instead, and the next spawn omits THRONE_GUARD.
            const QString detail = m_lastCoreErrorGen == gen && !m_lastCoreError.isEmpty()
                                       ? m_lastCoreError
                                       : QStringLiteral("kill switch: cannot adopt the guard group");
            killSwitch->noteIdentityUnsupported(detail);
            LOG_WARN(QStringLiteral("[Core] The core cannot adopt the kill-switch group (%1); restarting it without the guard identity").arg(detail));
            MW_show_log("[Warn] " + QObject::tr("Restarting the core ..."));
            scheduleRestart(kIntentRestartDelayMs);
            return;
        }
        if (wasConnected && m_connectedAt.isValid() && m_connectedAt.elapsed() >= kStableMs) m_failures = 0;
        ++m_failures;
        if (!wasConnected) ++m_startFailures;

        // A core that never connected never took the waiting start, and no profile can have brought it down.
        bool tooFrequent = false;
        if (wasConnected) {
            if (!m_profileCrashTimer.isValid()) {
                m_profileCrashTimer.start();
            } else if (m_profileCrashTimer.restart() < kProfileCrashWindowMs) {
                m_profileCrashTimer.invalidate();
                tooFrequent = true;
            }
        }
        if (wasConnected || m_startWhenUp < 0) {
            if (tooFrequent && settings->started_id >= 0) {
                MW_show_log("[ERROR] " + QObject::tr("Core exits too frequently, stop automatic restart this profile."));
                m_startWhenUp = -1;
                // Declined for good: a later restart that resumes the started profile must not bring it back.
                settings->started_id = Configs::NoProfileId;
            } else {
                m_startWhenUp = settings->started_id;
            }
        }

        emit coreExited(wasConnected);

        if ((m_failures >= kGiveUpAfter && m_startWhenUp < 0) || (!wasConnected && m_startFailures >= kGiveUpAfter)) {
            m_startWhenUp = -1;
            settings->started_id = Configs::NoProfileId;
            if (IsGuiApp()) {
                m_lastFailure = tr("The core keeps exiting, so it is no longer restarted. Use Restart Core to try again.");
            } else {
                m_lastFailure = !wasConnected && m_startFailures >= kGiveUpAfter
                                    ? QStringLiteral("the core exited %1 times without connecting").arg(m_startFailures)
                                    : QStringLiteral("the core kept exiting");
                if (m_lastCoreErrorGen == gen && !m_lastCoreError.isEmpty()) m_lastFailure += QStringLiteral("; last error: ") + m_lastCoreError;
            }
            MW_show_log("[ERROR] " + m_lastFailure);
            setState(CoreState::GaveUp);
            emit coreGaveUp(m_lastFailure);
            return;
        }
        MW_show_log("[Warn] " + QObject::tr("Restarting the core ..."));
        scheduleRestart(qMin(kBackoffFirstMs << qMin(m_failures - 1, 6), kBackoffCapMs));
    }

    void CoreSupervisor::scheduleRestart(int delayMs) {
        setState(CoreState::Backoff);
        m_restartTimer->start(delayMs);
    }

    void CoreSupervisor::postRestart() {
        // A restart requested since the timer was armed has already replaced this one.
        if (m_state != CoreState::Backoff || exiting()) return;
        setState(CoreState::Starting);
        postProcessRestart();
    }

    void CoreSupervisor::restartDeadCore(int startWhenUpProfileId) {
        // Re-checked: a backoff restart may have run since the caller looked.
        if ((m_state != CoreState::Backoff && m_state != CoreState::GaveUp) || exiting()) return;
        beginRestart();
        m_startWhenUp = startWhenUpProfileId;
        MW_show_log("[Warn] " + QObject::tr("Restarting the core ..."));
        postProcessRestart();
    }

    void CoreSupervisor::beginRestart() {
        m_probing = false;
        m_restartTimer->stop();
        m_connectTimer->stop();
        m_failures = 0;
        m_startFailures = 0;
        m_lastFailure.clear();
        m_lastCoreError.clear();
        setState(CoreState::Starting);
    }

    void CoreSupervisor::postProcessRestart(const std::function<void()> &before) {
        // Restart() silences the replaced core's exit, so a report of it still on the way is moot.
        m_handledExitGen = qMax(m_handledExitGen, currentGeneration());
        ++m_restartCount;
        runOnThread([this, before] {
            if (!m_process || Configs::dataManager->settingsRepo->prepare_exit) return;
            if (before) before();
            m_process->Restart();
        }, DS_cores);
    }

    bool CoreSupervisor::exiting() const {
        return m_state == CoreState::ShuttingDown || Configs::dataManager->settingsRepo->prepare_exit;
    }

    quint64 CoreSupervisor::currentGeneration() const {
        QMutexLocker lock(&m_processMutex);
        return m_process ? m_process->generation.load() : 0;
    }

    void CoreSupervisor::setState(CoreState state) {
        if (m_state.exchange(state) != state) emit stateChanged(state);
    }
} // namespace Engine
