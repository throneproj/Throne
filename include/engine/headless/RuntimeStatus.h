#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include "include/engine/EngineTypes.h"

namespace Engine {
    class EngineHost;

    // Readiness and the HEALTH fields of a headless process; 4B reads summary()/readyChanged for sd_notify. Main thread only.
    class RuntimeStatus final : public QObject {
        Q_OBJECT

    public:
        enum Step : quint8 { ApiStep = 1, CoreStep = 2, KillSwitchStep = 4, ResumeStep = 8 };

        static constexpr qint64 kStuckConnectingMs = 90000;
        static constexpr qint64 kStuckStoppingMs = 45000;

        // Build after EngineHost::create(): its coreStarted slot must run after the ConnectionManager's. bootMonoMs from MonotonicMs().
        RuntimeStatus(EngineHost *host, qint64 bootMonoMs, QObject *parent = nullptr);

        // Idempotent; readyChanged() once all four are set.
        void mark(Step step);

        bool ready() const;

        // "api", "core", "kill_switch", "resume".
        QStringList pendingSteps() const;

        // Supervisor Connected and API::defaultClient->IsConnected().
        bool coreUsable() const;

        // Seconds past kStuck* of the current Connecting/Stopping, else 0.
        qint64 stuckSeconds() const;

        // How long the current Connecting / Stopping has lasted; 0 when not in it.
        qint64 connectingMs() const;

        qint64 stoppingMs() const;

        // How long the core has been unusable: CoreSupervisor not in {GaveUp, ListenFailed, ShuttingDown} and not (Connected with
        // API::defaultClient->IsConnected()); 0 while usable or in those states. Measured from the later of the moment it became
        // unusable and its last CoreSupervisor state change, so each bounded state (connect timeout, backoff) counts on its own.
        qint64 coreUnusableMs();

        // ready && coreUsable && !stuck && (kill switch != Failed || failedPersistently()) && supervisor != GaveUp.
        bool healthy() const;

        // One line, control characters replaced: "Starting" | "Idle" | "Connecting X" | "Connected X (Tun)" | "Stopping X" |
        // "Starting core" | "Core restarting n/5" | "Degraded: <code>" | "Stopping".
        QString summary() const;

        // {ok, mode:"headless", ready, pid, uptime_s, stuck_s, core_restarts, previous_crash, headless_dialogs}.
        QJsonObject healthFields() const;

    signals:
        void readyChanged();

        // Deduplicated.
        void summaryChanged(const QString &summary);

    private:
        void refreshSummary();

        EngineHost *m_host;
        qint64 m_bootMonoMs;
        quint8 m_steps = 0;
        bool m_shuttingDown = false;
        // MonotonicMs() when the current Connecting/Stopping began; 0 otherwise.
        qint64 m_transitionStartMs = 0;
        ConnectionState m_transitionState = ConnectionState::Idle;
        // MonotonicMs() when the core became unusable or last changed state while unusable; 0 while usable.
        qint64 m_coreUnusableSinceMs = 0;
        QString m_summary;
    };
} // namespace Engine
