#pragma once

#include <atomic>
#include <memory>

#include <QElapsedTimer>
#include <QList>
#include <QMutex>
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

        // Any thread, a start worker included; runs on the caller. block: the caller spins a nested loop until the main-thread
        // teardown ran.
        void stop(const StopOptions &options);

        bool restartStartedProfile(bool interactive = true, quint64 restartSerial = 0);

        void applyPendingRestart();

        // Before CoreSupervisor::start(): the first coreStarted for request.profileId runs exactly this request (as
        // deferStartToCore does).
        void setBootStart(const StartRequest &request);

        // Windows: IsAdmin(). Elsewhere: KnownAdmin() when known; otherwise options.blockingPrivilegeCheck && IsAdmin().
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

        // ConnectionModes.cpp. Each returns whether it restarted the profile.
        bool setSystemProxy(bool enable, const ModeChange &change = {});

        bool setTun(bool enable, const ModeChange &change = {});

        bool chooseRoute(int routeId, bool interactive = true, quint64 restartSerial = 0);

        // Any thread. Queued on one worker thread in call order; wait spins a nested loop until this change ran.
        void applySystemProxy(bool enable, bool wait = false);

        bool ensureTunPrivileges(bool interactive);

        void disableKillSwitch();

        // A no-op while nothing was started.
        void noteRestartNeeded(RestartReason reason);

        void clearRestartNeeded();

        QList<RestartReason> restartReasons() const;

        bool restartPending() const;

    signals:
        // API; EngineHost relays them with Qt::DirectConnection. The types are unregistered.
        void startFinished(quint64 serial, int profileId, Engine::StartOutcome outcome, const QString &error);

        // Main thread, inside start() once the profile resolved and before its config is built; every pass of one request
        // (rank and kill-switch re-entries) emits it again with the same serial. Direct connections only.
        void startDispatching(quint64 serial, int profileId);

        // After mu_stopping is released.
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
        static constexpr qint64 kGuardRestartWindowMs = 15000;

        EnginePolicy *policy() const { return m_policy; }

        void onCoreStarted(int profileId);

        void onCoreGaveUp(const QString &reason);

        void failStart(const StartRequest &request, StartOutcome outcome, const QString &title, const QString &error);

        void deferStartToCore(const StartRequest &request);

        // tun: the running config carries a tun inbound (generated Tun or a custom config's).
        void setRunning(std::shared_ptr<Configs::Profile> profile, bool tun = false);

        // ConnectionModes.cpp. Why the boot Tun restore was held: the host conflict, the capability blocker, else privileges.
        QString tunRestoreHoldReason();

        void syncState();

        void lookupExitCountry(const std::shared_ptr<Configs::Profile> &ent);

        // ConnectionModes.cpp.
        void onKillSwitchStateChanged();

        bool guardRestartPending() const;

        void restartCoreForGuard(const StartRequest &request);

        // A start that failed with GeoAssetsMissing restarts once per profile after the unattended download, unless a start or a
        // manual stop moved token on since the failure.
        void retryAfterGeoFetch(int profileId, quint64 token);

        EnginePolicy *m_policy;
        CoreSupervisor *m_coreSupervisor;
        StatsService *m_stats;
        TestRunner *m_tests = nullptr;
        AutoSelectorController *m_autoSelector = nullptr;
        VpnAuthController *m_vpnAuth = nullptr;

        // Written by the start/stop workers; read through runningSnapshot().
        mutable QMutex m_runningMutex;
        std::shared_ptr<Configs::Profile> m_running;
        bool m_runningTun = false;

        // Main thread only.
        int m_lastProfileId = -1;
        bool m_profileConnecting = false;
        bool m_profileDisconnecting = false;
        ConnectionState m_lastConnectionState = ConnectionState::Idle;
        quint64 m_startSerial = 0;
        StartRequest m_killSwitchDeferredStart;
        // coreStarted carries only a profile id, so the rest of the request waits here.
        StartRequest m_coreStartRequest;
        QElapsedTimer m_guardCoreRestart;
        bool m_killSwitchWasFailed = false;
        bool m_killSwitchWasArmed = false;
        QList<RestartReason> m_restartReasons;

        // Locked and unlocked on different threads by design; no owner tracking.
        QMutex mu_starting;
        QMutex mu_stopping;
        // Set while start() stops the running profile, so that stop is marked as part of the switch.
        std::atomic<bool> m_switchingProfile{false};

        RuntimeOptions m_options;
        bool m_tunRestoreHeld = false;
        QString m_tunBlockedCache;
        // The inputs m_tunBlockedCache was computed from (Tun CIDR and probed DNS port); a change recomputes it.
        QString m_tunBlockedKey;
        qint64 m_tunBlockedAtMs = 0;
        // The boot request's serial until the first coreStarted; only that request is refused while the Tun is held.
        quint64 m_bootSerial = 0;
        int m_geoRetriedProfile = -1;
        // Any thread: bumped by start() and every manual stop.
        std::atomic<quint64> m_geoRetryToken{0};
    };
} // namespace Engine
