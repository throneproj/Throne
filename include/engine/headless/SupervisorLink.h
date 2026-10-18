#pragma once

#include <functional>

#include <QObject>
#include <QString>

class QTimer;

namespace Engine {
    class ExitController;
    class RuntimeStatus;

    // The service manager's protocol for a headless process (systemd notify today): READY, STATUS, STOPPING, WATCHDOG, and
    // EXTEND_TIMEOUT_USEC while not ready. Main thread only; every call is a no-op without a usable NOTIFY_SOCKET.
    class SupervisorLink final : public QObject {
        Q_OBJECT

    public:
        explicit SupervisorLink(QObject *parent = nullptr);

        // From HeadlessRuntime::attach(): readyChanged -> READY=1 with summary() (once), summaryChanged -> STATUS (deduplicated,
        // before and after READY), shutdownStarted -> STOPPING=1.
        void attach(RuntimeStatus *status, ExitController *exit);

        // From HeadlessRuntime::beforeExec(): WATCHDOG=1 every max(1 s, WATCHDOG_USEC / 3) from a PreciseTimer whatever the state
        // (the tick itself proves the main loop runs); while not ready and not stopping, EXTEND_TIMEOUT_USEC=60 s every 20 s.
        void start();

        // From HeadlessRuntime: the restart mode is exec, so a teardown for exit 75 sends RELOADING instead of STOPPING.
        // restarting() is asked at shutdownStarted.
        void setInPlaceRestart(std::function<bool()> restarting);

        // The exec restart did not happen after RELOADING went out: READY then STOPPING (two datagrams), so systemd <= 248 leaves
        // the reload state at once instead of waiting for its timeout before it sees the exit. No-op when no reload was sent.
        void abandonReload();

        bool reloadPending() const { return m_reloadSent; }

        // An exec'd image whose predecessor sent RELOADING: READY clears it, an early exit abandons it.
        void inheritReload() { m_reloadSent = true; }

    private:
        void onReady();

        void onSummary(const QString &summary);

        void onStopping();

        RuntimeStatus *m_status = nullptr;
        QTimer *m_watchdog = nullptr;
        QTimer *m_extend = nullptr;
        QString m_lastStatus;
        bool m_ready = false;
        bool m_stopping = false;
        std::function<bool()> m_inPlaceRestart;
        bool m_reloadSent = false;
    };
} // namespace Engine
