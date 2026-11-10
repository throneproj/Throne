#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <QProcess>

namespace Configs_sys {
    // Headless sets it before the engine starts any child: the core and the guard then run in their own process group (Unix; Windows: a new process group without a console window),
    // so a terminal's Ctrl+C or hang-up reaches only Throne, which stops them in order.
    void SetIsolatedChildren(bool enabled);

    // Before start(), with isolation on. Unix: setpgid(0, 0) in the child. Windows: CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW.
    void IsolateChild(QProcess *process);

    // Unix with isolation on: SIGKILL the process group of a running isolated child, so what it spawned dies with it. The
    // process itself still needs QProcess::kill(). A no-op otherwise.
    void KillChildGroup(QProcess *process);

    // The ThroneCore process; Engine::CoreSupervisor connects its handlers and owns the restart policy.
    class CoreProcess : public QProcess
    {
    public:
        QString tag;
        QString program;
        QStringList arguments;

        ~CoreProcess();

        // start & kill is one time

        void Start();

        // Waits up to waitMs for the process to exit, then (Windows) up to 10 s of what is left for the adopted elevated core.
        void Kill(int waitMs = 30000);

        CoreProcess(const QString &core_path, const QString &socketName, bool debugMode);

        // Also waits up to 5 s for the adopted elevated core, so the new one cannot race it for the Tun adapter and ports.
        void Restart();

        // Any thread. The pid the core reported, when it is not this process (Windows: the elevated core behind the stub
        // ThroneCore). Ignored when the generation has moved on; released by the next Start(). True when newly adopted.
        bool AdoptElevatedPid(qint64 pid, quint64 forGeneration);

        // Any thread. The adopted elevated core while it runs, else processId().
        qint64 EffectivePid() const;

        // A core adopts the kill switch group (Linux/macOS) only at startup; this records whether the running one was asked to.
        std::atomic<bool> guard_identity = false;

        // Bumped by each Start(), so an exit or connection can be matched to the attempt that produced it.
        std::atomic<quint64> generation = 0;

        // DS_cores only; failed_to_start is reset by each Start().
        bool failed_to_start = false;
        bool restarting = false;

    private:
        void waitElevated(int waitMs) const;

        QString m_socketName;
        bool m_debugMode = false;
        // Guards the two below; a wait holds its own reference, so an adopt or a release never closes a watch in use.
        mutable std::mutex m_elevatedMutex;
        std::shared_ptr<void> m_elevatedWatch;
        qint64 m_elevatedPid = 0;

    protected:
        bool started = false;
    };
} // namespace Configs_sys
