#pragma once

#include <atomic>
#include <memory>
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

        // Waits up to waitMs for the process to exit.
        void Kill(int waitMs = 30000);

        CoreProcess(const QString &core_path, const QString &socketName, bool debugMode);

        void Restart();

        // A core adopts the kill switch group (Linux/macOS) only at startup; this records whether the running one was asked to.
        std::atomic<bool> guard_identity = false;

        // Bumped by each Start(), so an exit or connection can be matched to the attempt that produced it.
        std::atomic<quint64> generation = 0;

        // DS_cores only; failed_to_start is reset by each Start().
        bool failed_to_start = false;
        bool restarting = false;

    private:
        QString m_socketName;
        bool m_debugMode = false;

    protected:
        bool started = false;
    };
} // namespace Configs_sys
