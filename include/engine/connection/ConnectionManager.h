#pragma once

#include <memory>

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
    class StartCoordinator;
    class StatsService;
    class TestRunner;
    class VpnAuthController;

    // Starts, stops and switches the running profile, applies the Tun/system-proxy/route modes and reacts to the kill switch.
    // Lives on the main thread and emits only there; main thread unless noted.
    class ConnectionManager final : public QObject {
        Q_OBJECT

    public:
        ConnectionManager(EnginePolicy *policy, CoreSupervisor *coreSupervisor, StatsService *stats, const RuntimeOptions &options,
                          QObject *parent = nullptr);

        // Before the event loop runs; all three outlive every start and stop.
        void attach(TestRunner *tests, AutoSelectorController *autoSelector, VpnAuthController *vpnAuth);

        // Once, before the first KillSwitch::apply(), so the state changes apply() emits reach the handler.
        void attachKillSwitch();

        // startFinished fires once per request. profileId < 0: NotFound.
        void start(const StartRequest &request);

        // Any thread; decided on the main thread (posted from elsewhere). block: the caller spins a nested loop until the stop it
        // started or joined was torn down, or it found nothing; during a start it waits for that start first. Crash stops and
        // stops during exit never wait for another.
        void stop(const StopOptions &options);

        // A stop waits for the start in flight to end.
        bool stopPending() const;

        void applyPendingRestart();

        // Before CoreSupervisor::start(): the first coreStarted for request.profileId runs exactly this request.
        void setBootStart(const StartRequest &request);

        // KnownAdmin() when known; otherwise options.blockingPrivilegeCheck && IsAdmin().
        bool tunPrivileged();

        // A remembered Tun the boot could not restore; cleared by a Tun disable or a successful enable.
        bool tunRestoreHeld() const { return m_tunRestoreHeld; }

        // Main thread. Empty while Tun is requested here or the running config carries a tun inbound. Headless: Sys::HostTun::Conflict(vpn_tun_ipv4_cidr, idle ? core_dns_in_port
        // : 0), cached for 5 s. GUI: a live headless instance from Sys::HostPresence::OtherHeadless() (translated text).
        QString tunBlockedReason();

        // Any thread. The running config carries a tun inbound.
        bool runningTun() const;

        ConnectionState connectionState() const;

        // Any thread.
        std::shared_ptr<Configs::Profile> runningSnapshot() const;

        int resolveLastProfile() const;

        int lastProfileId() const;

        quint64 nextStartSerial();

        // Any thread. Shared with the Lifecycle resume-target hooks.
        std::shared_ptr<EngineState> state() const;

        // ConnectionModes.cpp. Each returns whether it restarted the profile.
        bool setSystemProxy(bool enable, const ModeChange &change = {});

        bool setTun(bool enable, const ModeChange &change = {});

        bool chooseRoute(int routeId, bool interactive = true, quint64 restartSerial = 0);

        // Any thread. Queued on one worker thread in call order; wait spins a nested loop until this change ran.
        void applySystemProxy(bool enable, bool wait = false);

        bool ensureTunPrivileges(bool interactive);

        void disableKillSwitch();

        // A no-op while nothing was started and no start is in flight.
        void noteRestartNeeded(RestartReason reason);

        void clearRestartNeeded();

        QList<RestartReason> restartReasons() const;

        bool restartPending() const;

    signals:
        // API; EngineHost relays them with Qt::DirectConnection. The types are unregistered.
        void startFinished(quint64 serial, int profileId, Engine::StartOutcome outcome, const QString &error);

        // Main thread, inside start() once the request passed the kill-switch, guard and Busy checks, before its rank and build.
        // Every pass of one request that gets there (rank and kill-switch re-entries) emits it with the same serial. Direct
        // connections only.
        void startDispatching(quint64 serial, int profileId);

        // After the stop's teardown.
        void stopFinished(int profileId);

        void connectionStateChanged(Engine::ConnectionState state);

        // View hints, emitted where the old code refreshed a view; slots must not block, open a dialog, start or stop.
        void statusChanged();

        void busyChanged();

        void profileRowsChanged(const QList<int> &profileIds);

        void runMarker(qint64 monoMs, Engine::RunMarker kind, const QString &name);

        // Only when the set of pending reasons changed.
        void restartReasonsChanged();

    private:
        EnginePolicy *policy() const { return m_policy; }

        void onCoreStarted(int profileId);

        // ConnectionModes.cpp. Why the boot Tun restore was held: the host conflict, the capability blocker, else privileges.
        QString tunRestoreHoldReason();

        // ConnectionModes.cpp.
        void onKillSwitchStateChanged();

        EnginePolicy *m_policy;
        std::shared_ptr<EngineState> m_state;
        StartCoordinator *m_coordinator = nullptr;

        // Main thread only.
        bool m_killSwitchWasFailed = false;
        bool m_killSwitchWasArmed = false;

        RuntimeOptions m_options;
        bool m_tunRestoreHeld = false;
        QString m_tunBlockedCache;
        // The inputs m_tunBlockedCache was computed from (Tun CIDR and probed DNS port); a change recomputes it.
        QString m_tunBlockedKey;
        qint64 m_tunBlockedAtMs = 0;
    };
} // namespace Engine
