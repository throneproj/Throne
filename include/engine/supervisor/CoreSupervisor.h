#pragma once

#include <atomic>
#include <functional>

#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;
class QTimer;

namespace Configs_sys {
    class CoreProcess;
}

namespace Engine {
    enum class CoreState { NotStarted, Starting, Connected, Backoff, GaveUp, ListenFailed, ShuttingDown };

    // Runs ThroneCore and accepts its IPC connection. Lives on the main thread; the CoreProcess and its handlers live on DS_cores.
    class CoreSupervisor : public QObject {
        Q_OBJECT

    public:
        static constexpr int kConnectTimeoutMs = 20000;
        static constexpr qint64 kStableMs = 60 * 1000;
        static constexpr int kBackoffFirstMs = 500;
        static constexpr int kBackoffCapMs = 30000;
        static constexpr int kGiveUpAfter = 5;
        static constexpr qint64 kProfileCrashWindowMs = 10000;
        static constexpr int kIntentRestartDelayMs = 200;
        static constexpr int kPrivilegeProbeTimeoutMs = 5000;

        explicit CoreSupervisor(QObject *parent = nullptr);
        ~CoreSupervisor() override;

        // Main thread. False when the IPC server cannot listen: then a notice is posted and the core never starts.
        bool listen();
        // Main thread, after listen() and KillSwitch::apply(). startWhenUpProfileId < 0: start nothing once connected.
        void start(int startWhenUpProfileId);
        // Main thread; stops every restart, then waits in a nested loop until the core is killed (CoreProcess::Kill(killWaitMs)).
        void shutdown(int killWaitMs = 30000);

        // Main thread, for a second stop signal during the teardown: ShuttingDown, timers stopped, QProcess::kill() posted to
        // DS_cores without waiting; shutdown() still reaps. A pending Stop RPC fails at once, so the core is gone before
        // KillSwitch::shutdown().
        void killForExit();

        // These restarts are not core failures and leave GaveUp. Main thread unless noted; false when the core cannot run.
        // "Restart Core": crash-stops the running profile on DS_cores, then restarts the core with nothing to resume.
        bool resetCore();
        // The core is not listening: restart it and start profileId once it connects.
        bool restartNotListening(int profileId);
        // Crash-stops the running profile and restarts the core; the profile set by setStartWhenUp() starts once it connects.
        void restartForGuard();
        // Any thread; waits until a running core is killed. The core restarts and resumes the started profile.
        void killForPrivilegeChange();

        // Main thread. The profile to start once the next core connects. False when no core process exists yet.
        bool setStartWhenUp(int profileId);
        // Main thread. A user stop while nothing runs: the profile waiting for the core to come back is not started.
        void cancelResume();
        // Main thread. True from a Linux/macOS accept until its coreStarted, while the privilege probe runs; never on Windows.
        bool awaitingCoreStarted() const;
        // Main thread. The profile the next coreStarted carries; -1 when none.
        int startWhenUp() const;

        // Any thread. False after a listen failure or once shutting down.
        bool canStartCore() const;
        CoreState state() const;
        // Main thread. Why the core cannot run (listen failure, give-up); empty otherwise.
        QString lastFailure() const;
        // Main thread. What the latest core that exited said about it: its fatal line (Go log.Fatal, "panic:", "fatal error:"),
        // the start error, or the connect timeout; one line, at most 300 chars; empty when unknown or after a restart request.
        QString lastCoreError() const;

        qint64 corePid() const;                   // any thread
        // Main thread. Every respawn, counted where they all funnel (postProcessRestart).
        int restartCount() const;
        // Main thread. The failures counted towards giving up.
        int consecutiveFailures() const;
        bool runningWithoutGuardIdentity() const; // any thread; always false on Windows

    signals:
        // Main thread, on every core connection, right after the RPC client was reconnected to it.
        void rpcConnected();
        // Main thread, once a verified core connected and, on Linux/macOS, answered the privilege probe; profileId < 0 when nothing should start.
        void coreStarted(int profileId);
        // Main thread. The core kept exiting and is no longer restarted until a restart is requested.
        void coreGaveUp(const QString &reason);
        // Main thread, from listen().
        void listenFailed(const QString &error);
        // Main thread, for direct connections.
        void stateChanged(Engine::CoreState state);
        // Main thread. Unexpected exits only, after the expected-exit return and before the give-up branch.
        void coreExited(bool wasConnected);
        // Main thread, from cancelResume(): a user stop with nothing running cancelled the resume that waits for the core.
        void resumeCancelled();

    private:
        static constexpr qint64 kLogWindowMs = 1000;
        static constexpr int kPrivilegeProbeRetries = 2;
        static constexpr int kPrivilegeProbeRetryMs = 10000;

        bool verifyCorePid(QLocalSocket *socket) const;
        void onNewConnection();
        void setupRpc(QLocalSocket *socket);
        // attempt > 0 is a retry after a probe that got no answer: it only refreshes the cached answer.
        void startPrivilegeProbe(quint64 gen, int attempt = 0);
        void onPrivilegeProbed(quint64 gen, bool ok, bool privileged, int attempt);
        void emitCoreStarted();
        void attachProcess(Configs_sys::CoreProcess *process);

        void onCoreSpawned(quint64 gen);
        // exitCode: the process exit code, -1 for a crash or a failed start.
        void onCoreExited(quint64 gen, bool failedToStart, int exitCode);
        // Main thread, queued from DS_cores in order with the exit post: a candidate cause line of core generation gen.
        void noteCoreError(quint64 gen, const QString &line);
        void onConnectTimeout();
        void scheduleRestart(int delayMs);
        void postRestart();
        void restartDeadCore(int startWhenUpProfileId);
        void beginRestart();
        void postProcessRestart(const std::function<void()> &before = {});
        bool exiting() const;
        quint64 currentGeneration() const;
        void setState(CoreState state);

        QString m_corePath;
        bool m_debugMode = false;
        QLocalServer *m_server = nullptr;
        mutable QMutex m_processMutex;
        Configs_sys::CoreProcess *m_process = nullptr;
        QTimer *m_restartTimer = nullptr;
        QTimer *m_connectTimer = nullptr;
        std::atomic<CoreState> m_state{CoreState::NotStarted};
        // Written by the DS_cores task right before an intentional Kill(); that exit restarts the core without counting.
        std::atomic<quint64> m_expectedExitGen{0};

        // Main thread only.
        int m_startWhenUp = -1;
        // The accepted core's privilege probe is running, so its coreStarted is still to come.
        bool m_probing = false;
        int m_failures = 0;
        // Consecutive exits of cores that never connected.
        int m_startFailures = 0;
        QString m_lastCoreError;
        quint64 m_lastCoreErrorGen = 0;
        // Exits of generations up to this one are handled or superseded by a restart already posted.
        quint64 m_handledExitGen = 0;
        quint64 m_acceptedGen = 0;
        quint64 m_connectGen = 0;
        // The last generation the connect timeout gave up on; its late connection is refused.
        quint64 m_timedOutGen = 0;
        QElapsedTimer m_connectedAt;
        QElapsedTimer m_profileCrashTimer;
        QString m_lastFailure;
        int m_restartCount = 0;

        // DS_cores only.
        qint64 m_logWindowStartMs = 0;
        int m_logWindowLines = 0;
    };
} // namespace Engine
