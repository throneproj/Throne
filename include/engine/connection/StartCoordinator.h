#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QString>

#include "include/engine/EngineTypes.h"
#include "include/engine/RuntimeOptions.h"

namespace Configs {
    class Profile;
}

namespace Engine {
    class AutoSelectorController;
    class CoreSupervisor;
    class EnginePolicy;
    class EngineState;
    class StatsService;
    class TestRunner;
    class VpnAuthController;

    // Every start, stop and restart of the running profile, the starts parked on a gate and the restart hint. Lives on the main
    // thread and emits only there; main thread unless noted. ConnectionManager owns it and relays its signals.
    class StartCoordinator final : public QObject {
        Q_OBJECT

    public:
        struct ModeHooks {
            // Any thread.
            std::function<void(bool enable)> applySystemProxy;
        };

        StartCoordinator(std::shared_ptr<EngineState> state, EnginePolicy *policy, CoreSupervisor *core, StatsService *stats,
                         const RuntimeOptions &options, ModeHooks hooks, QObject *parent = nullptr);

        void attach(TestRunner *tests, AutoSelectorController *autoSelector, VpnAuthController *vpnAuth);

        // startFinished fires once per request. Synchronous up to the rank check; the body runs on the ops thread.
        void requestStart(const StartRequest &request);

        // Any thread but the ops thread. The intent bump runs on the caller, the decision on the main thread (posted from
        // elsewhere). block waits until the stop it started or joined was torn down, or until it found nothing; during a start,
        // block first waits for that start. Crash stops and stops during exit never wait for another.
        void requestStop(const StopOptions &options);

        // A stop waits for the start in flight to end.
        bool stopPending() const;

        // False when profileId < 0.
        bool restart(int profileId, bool interactive, quint64 serial);

        bool restartResumeTarget(bool interactive, quint64 serial);

        // Before CoreSupervisor::start(): the first coreStarted for request.profileId runs exactly this request.
        void setBootStart(const StartRequest &request);

        // ConnectionManager's coreStarted slot calls noteCoreUp() before its mode restore and takeCoreStart() after it.
        void noteCoreUp();

        void takeCoreStart(int profileId, bool tunRestoreHeld, const QString &tunHoldWhy);

        void onCoreGaveUp(const QString &reason);

        // The kill switch entered Armed.
        void onGuardArmed();

        // A no-op while nothing was started and no start is in flight.
        void noteRestartNeeded(RestartReason reason);

        void clearRestartNeeded();

        QList<RestartReason> restartReasons() const;

        bool restartPending() const;

        void applyPendingRestart();

        int resolveLastProfile() const;

        quint64 nextStartSerial();

    signals:
        void startFinished(quint64 serial, int profileId, Engine::StartOutcome outcome, const QString &error);

        void startDispatching(quint64 serial, int profileId);

        void stopFinished(int profileId);

        void connectionStateChanged(Engine::ConnectionState state);

        void statusChanged();

        void busyChanged();

        void profileRowsChanged(const QList<int> &profileIds);

        void runMarker(qint64 monoMs, Engine::RunMarker kind, const QString &name);

        void restartReasonsChanged();

    private:
        struct PendingStarts {
            StartRequest killSwitch;
            // coreStarted carries only a profile id, so the rest of the request waits here; another id supersedes it.
            StartRequest core;
            // The boot request's serial until the first coreStarted; only that request is refused while the Tun is held.
            quint64 bootSerial = 0;
            QElapsedTimer guardWindow;
        };

        enum class Wake {
            AlreadyComing,
            NotListening,
            Starting,
            Guard,
            Boot,
        };

        // The one stop that waits for the start in flight; later ones merge into it.
        struct HeldStop {
            StopOptions options;
            QList<std::function<void()>> done;
        };

        static constexpr qint64 kGuardRestartWindowMs = 15000;

        EnginePolicy *policy() const { return m_policy; }

        void failStart(const StartRequest &request, StartOutcome outcome, const QString &title, const QString &error);

        // The request slot holds, if any, gets Superseded; then slot holds next.
        void supersedePending(StartRequest &slot, const StartRequest &next);

        // Parks request for the next coreStarted once the core was told to bring it, as wake says; CoreUnavailable otherwise,
        // leaving the slot alone.
        void deferToCore(const StartRequest &request, Wake wake);

        bool guardRestartPending() const;

        // From a start's Busy claim until its end ran on the main thread.
        bool startInFlight() const;

        // done, when set, runs once nothing runs, or once the stop this one started or joined was torn down. During a start, a
        // user stop or a block stop is held until that start ends; crash stops and stops during exit are never held or joined.
        void decideStop(const StopOptions &options, const std::function<void()> &done);

        void holdStop(const StopOptions &options, const std::function<void()> &done, bool userStop);

        // A user stop: no resume target (persisted), no parked start, no core resume.
        void applyUserStop();

        // Something runs and no stop is in flight. switching: a start's stop of the previous profile, marked by that start.
        void acceptStop(const StopOptions &options, bool switching, const std::function<void()> &done);

        // Drops the reasons last noted at or before epoch.
        void clearRestartNotedBy(quint64 epoch);

        void syncState();

        void lookupExitCountry(const std::shared_ptr<Configs::Profile> &ent);

        // A start that failed with GeoAssetsMissing restarts once per profile after the unattended download, unless a start or a
        // manual stop moved token on since that start was requested.
        void retryAfterGeoFetch(int profileId, quint64 token);

        std::shared_ptr<EngineState> m_state;
        EnginePolicy *m_policy;
        CoreSupervisor *m_core;
        StatsService *m_stats;
        TestRunner *m_tests = nullptr;
        AutoSelectorController *m_autoSelector = nullptr;
        VpnAuthController *m_vpnAuth = nullptr;
        RuntimeOptions m_options;
        ModeHooks m_hooks;

        // Main thread only.
        quint64 m_startSerial = 0;
        PendingStarts m_pending;
        // A whenSettled callback is registered for the current Arming wait.
        bool m_killSwitchWaiting = false;
        int m_geoRetriedProfile = -1;
        // From a stop's acceptance until its teardown.
        bool m_stopInFlight = false;
        std::optional<HeldStop> m_heldStop;
        // Stops that joined the one in flight; they run after its teardown.
        QList<std::function<void()>> m_stopWaiters;

        // From a start's Busy check until it returns early or its body ends; the ops thread clears it when a body ends.
        std::atomic<bool> m_startInFlight{false};
        // Any thread: bumped by every start and manual stop; deferred continuations compare it.
        std::atomic<quint64> m_intentToken{0};
    };
} // namespace Engine
