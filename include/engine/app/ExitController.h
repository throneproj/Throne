#pragma once

#include <QMutex>
#include <QObject>

#include "include/engine/EngineTypes.h"

namespace Engine {
    class ConnectionManager;
    class CoreSupervisor;
    class EnginePolicy;

    // The defaults are the GUI's teardown.
    struct ShutdownOptions {
        // 0: the channel default.
        int stopRpcTimeoutMs = 0;
        bool stopTests = true;
        bool clearSystemProxy = true;
        int coreKillWaitMs = 30000;
        // remember_system_proxy/remember_tun from the live modes.
        bool persistModes = true;

        static ShutdownOptions Headless() {
            ShutdownOptions options;
            options.stopRpcTimeoutMs = 10000;
            options.stopTests = false;
            options.clearSystemProxy = false;
            options.coreKillWaitMs = 3000;
            options.persistModes = false;
            return options;
        }
    };

    // The shutdown both front ends share, and the queued unattended exit the Throne API asks for. Main thread only.
    class ExitController final : public QObject {
        Q_OBJECT

    public:
        ExitController(EnginePolicy *policy, ConnectionManager *connection, CoreSupervisor *coreSupervisor, QObject *parent = nullptr);

        // Queued, so an API caller replies before the teardown shuts the API down, and a dialog unwinds before a UAC pump can free
        // it (#1961). Unattended takes only None and Restart. False while one is queued or an exit is in progress.
        bool requestExit(ExitReason reason, bool unattended = true);

        // Idempotent; prepare_exit's order, KillSwitch::shutdown() last.
        void shutdown(bool interactive = true);

        // remember_* from the live state, then Save(); a no-op for the database once noSave is set.
        void persistSessionState();

        // Set across the window's exit, whose UAC wait pumps messages.
        bool exitInProgress() const { return m_exitInProgress; }

        void setExitInProgress(bool inProgress) { m_exitInProgress = inProgress; }

        void setShutdownOptions(const ShutdownOptions &options) { m_shutdownOptions = options; }

    signals:
        // Right after prepare_exit is set.
        void shutdownStarted();

        // Before persistSessionState() in shutdown(); the GUI releases its shortcuts and writes its geometry into the settings.
        void sessionPersisting();

        // The core is gone and the kill switch is still up; the GUI hides the window and the tray, which would veto quit().
        void coreShutDown();

    private:
        EnginePolicy *m_policy;
        ConnectionManager *m_connection;
        CoreSupervisor *m_coreSupervisor;
        QMutex mu_exit;
        bool m_exitInProgress = false;
        bool m_exitQueued = false;
        ShutdownOptions m_shutdownOptions;
    };
} // namespace Engine
