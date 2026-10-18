#include "include/engine/EngineHost.h"

#include <QPointer>

#include <atomic>
#include <memory>

#include "include/api/remote/Overlay.hpp"
#include "include/api/remote/Router.hpp"
#include "include/api/remote/Server.hpp"
#include "include/configs/generate.h"
#include "include/configs/sub/GroupUpdater.hpp"
#include "include/configs/sub/RouteUpdater.hpp"
#include "include/engine/EngineEvents.h"
#include "include/engine/EnginePolicy.h"
#include "include/engine/EngineTr.h"
#include "include/engine/Lifecycle.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/app/ExitController.h"
#include "include/engine/autoselector/AutoSelectorController.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/engine/headless/HeadlessPolicy.h"
#include "include/engine/services/AssetUpdates.h"
#include "include/engine/services/Dashboard.h"
#include "include/engine/services/ProfileService.h"
#include "include/engine/services/StatsService.h"
#include "include/engine/supervisor/CoreSupervisor.h"
#include "include/engine/tests/TestObserver.h"
#include "include/engine/tests/TestRunner.h"
#include "include/engine/vpn/VpnAuthController.h"
#include "include/global/Configs.hpp"
#include "include/global/DeviceDetailsHelper.hpp"
#include "include/global/Logger.hpp"
#include "include/global/PeriodicRunner.hpp"
#include "include/scanner/IpListUpdater.h"
#include "include/sys/AutoRun.hpp"
#include "include/sys/KillSwitch.hpp"

namespace Engine {
    namespace {
        EngineHost *engineHostInstance = nullptr;
    } // namespace

    class EngineHost::PolicyRelay final : public EnginePolicy {
    public:
        void bind(EnginePolicy *policy) { m_target = policy; }

        void geoAssetMissingCategory(const QString &contextName, const QString &needed, const QString &whichFile) override {
            target(__func__)->geoAssetMissingCategory(contextName, needed, whichFile);
        }

        bool confirmGeoAssetDownload(const QString &contextName) override {
            return target(__func__)->confirmGeoAssetDownload(contextName);
        }

        void geoAssetDownloadFinished(const QString &error) override {
            target(__func__)->geoAssetDownloadFinished(error);
        }

        void killSwitchProblem() override {
            target(__func__)->killSwitchProblem();
        }

        void killSwitchProblemResolved() override {
            target(__func__)->killSwitchProblemResolved();
        }

        void extraCoreNotPermitted() override {
            target(__func__)->extraCoreNotPermitted();
        }

        void startFailed(const StartRequest &request, StartOutcome outcome, const QString &title, const QString &error) override {
            target(__func__)->startFailed(request, outcome, title, error);
        }

        void strictRouteUnavailable(const QString &error) override {
            target(__func__)->strictRouteUnavailable(error);
        }

        bool offerCoreReset(const QString &error) override {
            return target(__func__)->offerCoreReset(error);
        }

        void stopFailed(const QString &error) override {
            target(__func__)->stopFailed(error);
        }

        SlowOperation slowOperation(SlowOperationKind kind, int delayMs) override {
            return target(__func__)->slowOperation(kind, delayMs);
        }

        bool requestCorePrivileges() override {
            return target(__func__)->requestCorePrivileges();
        }

        void systemProxyNeedsMixedInbound() override {
            target(__func__)->systemProxyNeedsMixedInbound();
        }

        bool vpnPromptOpen() const override {
            return target(__func__)->vpnPromptOpen();
        }

        void vpnChallenge(const VpnAuthChallenge &challenge) override {
            target(__func__)->vpnChallenge(challenge);
        }

        void vpnCredentialsRejected(int profileId, const VpnAuthChallenge &challenge,
                                    const std::function<void(const QString &username, const QString &password)> &retryWith) override {
            target(__func__)->vpnCredentialsRejected(profileId, challenge, retryWith);
        }

        void vpnChallengesCleared() override {
            target(__func__)->vpnChallengesCleared();
        }

        void settingsNeedTunRestart() override {
            target(__func__)->settingsNeedTunRestart();
        }

        bool confirmRestartProgram() override {
            return target(__func__)->confirmRestartProgram();
        }

        void testSessionBusy(const QString &text) override {
            target(__func__)->testSessionBusy(text);
        }

        void exitProgram(ExitReason reason, bool unattended) override {
            target(__func__)->exitProgram(reason, unattended);
        }

    private:
        EnginePolicy *target(const char *method) const {
            if (m_target != nullptr) return m_target;
            if (!m_warnedUnbound.exchange(true)) {
                LOG_WARN(QStringLiteral("POLICY-UNBOUND %1 got the headless answer: no front end is bound").arg(QLatin1String(method)));
            }
            return &m_fallback;
        }

        EnginePolicy *m_target = nullptr;
        mutable HeadlessPolicy m_fallback;
        mutable std::atomic<bool> m_warnedUnbound{false};
    };

    class EngineHost::ObserverRelay final : public TestObserver {
    public:
        void bind(TestObserver *observer) { m_target.store(observer); }

        void testPanelSeeded(TestKind kind, int totalProfiles) override {
            if (auto *target = m_target.load()) target->testPanelSeeded(kind, totalProfiles);
        }

        void testProgressed() override {
            if (auto *target = m_target.load()) target->testProgressed();
        }

        void testPanelsCleared() override {
            if (auto *target = m_target.load()) target->testPanelsCleared();
        }

        void testViewChanged(bool force) override {
            if (auto *target = m_target.load()) target->testViewChanged(force);
        }

        void speedTestProgress(const QString &profileName, const libcore::SpeedTestResult &result) override {
            if (auto *target = m_target.load()) target->speedTestProgress(profileName, result);
        }

        void testedProfilesChanged(const QList<int> &profileIds) override {
            if (auto *target = m_target.load()) target->testedProfilesChanged(profileIds);
        }

    private:
        std::atomic<TestObserver *> m_target{nullptr};
    };

    EngineHost::EngineHost(const RuntimeOptions &options) : m_options(options) {}

    EngineHost::~EngineHost() {
        engineHostInstance = nullptr;
        detachFrontEnd();
        Subscription::updater()->SetUrlTester(nullptr);
        Lifecycle::SetStopHandler({});
        delete m_router;
        m_autoSelector.reset();
        m_tests.reset();
        delete m_exit;
        delete m_vpnAuth;
        delete m_connection;
        delete m_coreSupervisor;
        delete m_stats;
    }

    EngineHost *EngineHost::create(const RuntimeOptions &options) {
        ENGINE_CHECK_MAIN_THREAD();
        if (engineHostInstance != nullptr) {
            LOG_WARN("EngineHost::create() called again; keeping the existing host");
            return engineHostInstance;
        }
        Sys::KillSwitch::instance();
        Throne::PeriodicRunner::instance();
        Subscription::updater();
        EngineEvents::instance();
        AssetUpdates::SetUnattendedFetch(options.geoAutoDownload);
        auto *created = new EngineHost(options);
        engineHostInstance = created;
        created->construct();
        return created;
    }

    void EngineHost::construct() {
        m_policy = std::make_unique<PolicyRelay>();
        m_observer = std::make_unique<ObserverRelay>();
        m_stats = new StatsService(this);
        m_coreSupervisor = new CoreSupervisor(this);
        m_connection = new ConnectionManager(policy(), m_coreSupervisor, m_stats, m_options, this);
        connect(m_connection, &ConnectionManager::startFinished, this, &EngineControl::startFinished, Qt::DirectConnection);
        connect(m_connection, &ConnectionManager::stopFinished, this, &EngineControl::stopFinished, Qt::DirectConnection);
        connect(m_connection, &ConnectionManager::connectionStateChanged, this, &EngineControl::connectionStateChanged, Qt::DirectConnection);
        m_vpnAuth = new VpnAuthController(policy(), this, this);
        Lifecycle::SetStopHandler([c = m_connection](bool crash, bool block, bool manual, bool interactive) { c->stop({crash, block, manual, interactive}); });
        m_tests = std::make_unique<TestRunner>(policy(), m_observer.get(), [c = m_connection] { return c->runningSnapshot(); });
        m_autoSelector = std::make_unique<AutoSelectorController>(this, m_tests.get());
        m_connection->attach(m_tests.get(), m_autoSelector.get(), m_vpnAuth);
        // In this order: the traffic and connection loops spawn before the selector monitor, as before.
        connect(m_coreSupervisor, &CoreSupervisor::rpcConnected, m_stats, &StatsService::startLoops);
        connect(m_coreSupervisor, &CoreSupervisor::rpcConnected, m_autoSelector.get(), &AutoSelectorController::startMonitorLoop);
        m_exit = new ExitController(policy(), m_connection, m_coreSupervisor, this);
        if (m_options.headless) m_exit->setShutdownOptions(ShutdownOptions::Headless());

        connect(EngineEvents::instance(), &EngineEvents::subscriptionFinished, this, [](int importedCount, bool quiet) {
            if (quiet) return;
            // The job's own count: the global may already belong to the next job.
            MW_show_log(MainWindowTr::tr("Imported %1 profile(s)").arg(importedCount >= 0 ? importedCount : Configs::dataManager->settingsRepo->imported_count));
        });
        // Queued and connected before the window's model refresh, so posted-event order clears the cache first.
        auto *ipLists = Scanner::IpListUpdater::instance();
        connect(ipLists, &Scanner::IpListUpdater::listsChanged, this, [] { Configs::InvalidateEndpointDisplayCache(); }, Qt::QueuedConnection);
        connect(ipLists, &Scanner::IpListUpdater::listUpdated, this, [] { Configs::InvalidateEndpointDisplayCache(); }, Qt::QueuedConnection);

        runOnNewThread([] { GetDeviceDetails(); });

        Subscription::updater()->SetUrlTester([this](const QList<int> &profileIDs, bool interactive, const Subscription::GroupUpdater::Finish &done) {
            m_tests->queueUrlTests(profileIDs, done, interactive);
        });

        Dashboard::SeedClashUi();
        // Leaving the dir non-empty marks it user-provided, disabling the core's own updater.
        Dashboard::SeedApi();

        registerPeriodicJobs();

        m_router = new RemoteApi::Router(this, this);
        const QPointer<RemoteApi::Router> apiRouter = m_router;
        const QPointer<EngineHost> self = this;
        RemoteApi::Server::instance()->setDispatcher([apiRouter, self](const RemoteApi::Request &request, const RemoteApi::Responder &respond) {
            if (!apiRouter || !self) respond(RemoteApi::Response::Error(503, "exiting", "Throne is shutting down"));
            // A start dispatched before the core supervisor started would strand the core.
            else if (!self->m_apiReady) respond(RemoteApi::Response::Error(503, "starting", "Throne is starting"));
            else apiRouter->dispatch(request, respond);
        });
    }

    void EngineHost::init(EnginePolicy *frontEnd, TestObserver *testObserver, std::optional<BootStart> bootStart) {
        ENGINE_CHECK_MAIN_THREAD();
        if (m_initialized) {
            LOG_WARN("EngineHost::init() called again; ignored");
            return;
        }
        m_initialized = true;
        m_policy->bind(frontEnd);
        m_observer->bind(testObserver);

        // Not in construct(): MkPort's failure line needs the front end's log sink; the emit repaints the port the window drew.
        if (Configs::dataManager->settingsRepo->random_inbound_port)
        {
            Configs::dataManager->settingsRepo->inbound_socks_port = MkPort(Configs::dataManager->settingsRepo->inbound_address);
            emit statusChanged();
        }

        // On failure the supervisor posts the notice, and start() and every profile start refuse to run.
        m_coreSupervisor->listen();

        // Before apply(): a state change inside apply() must reach the handler.
        m_connection->attachKillSwitch();

        if (m_options.headless) {
            Sys::KillSwitch::instance()->setOverride(m_options.killSwitchOverride);
            if (m_options.killSwitchOverride && Configs::dataManager->settingsRepo->kill_switch) {
                LOG_WARN(QStringLiteral("[Kill switch] kill_switch is on, but --no-kill-switch (THRONE_KILL_SWITCH) keeps it off for this run; "
                                        "profiles stay refused until kill_switch is turned off"));
            }
        }

        // Bound before the guard's first arm, so it already carries api_port; requests get 503 "starting" until m_apiReady.
        RemoteApi::Server::instance()->applyAndWait(RemoteApi::EffectiveConfig().config);

        // Must precede the first coreStarted, or a remembered profile would start before the guard is even arming.
        Sys::KillSwitch::instance()->apply();

        if (bootStart) {
            if (bootStart->profileId >= 0) m_connection->setBootStart(StartRequest{bootStart->profileId, false, bootStart->serial});
            m_coreSupervisor->start(bootStart->profileId);
        } else {
            const auto &startSettings = Configs::dataManager->settingsRepo;
            m_coreSupervisor->start(startSettings->remember_enable && startSettings->remember_id >= 0 ? startSettings->remember_id : -1);
        }

        m_apiReady = true;
    }

    QJsonObject EngineHost::statusJson() const {
        return m_router != nullptr ? m_router->statusJson() : QJsonObject();
    }

    void EngineHost::setResumeSuppressed(bool suppressed) {
        m_resumeSuppressed = suppressed;
    }

    void EngineHost::detachFrontEnd() {
        if (m_policy) m_policy->bind(nullptr);
        if (m_observer) m_observer->bind(nullptr);
    }

    void EngineHost::shutdown(bool interactive) {
        ENGINE_CHECK_MAIN_THREAD();
        m_exit->shutdown(interactive);
    }

    void EngineHost::applySettingsChange(const QStringList &flags, bool interactive) {
        ENGINE_CHECK_MAIN_THREAD();
        const auto changed = [&](const QString &flag) { return flags.contains(flag); };
        auto &settings = Configs::dataManager->settingsRepo;

        if (changed(MwArg::ChoosePort)) {
            settings->inbound_socks_port = MkPort(settings->inbound_address);
            if (settings->spmode_system_proxy) {
                const ModeChange change{.interactive = interactive};
                m_connection->setSystemProxy(false, change);
                m_connection->setSystemProxy(true, change);
            }
        }
        if (changed(MwArg::DisableAdmin)) {
            AutoRun_FixTaskIfNeeded();
        }
        auto suggestRestartProxy = settings->Save();
        Throne::PeriodicRunner::instance()->CheckNow();
        if (changed(MwArg::KillSwitch) || changed(MwArg::Vpn)) Sys::KillSwitch::instance()->apply();
        if (changed(MwArg::RemoteApi)) RemoteApi::Server::instance()->apply(RemoteApi::EffectiveConfig().config);
        if (changed(MwArg::Route)) {
            settings->Save();
            suggestRestartProxy = true;
        }
        if (changed(MwArg::NeedRestart)) {
            suggestRestartProxy = false;
        }
        if (changed(MwArg::Vpn) && settings->spmode_vpn) {
            if (interactive) policy()->settingsNeedTunRestart();
            else MW_show_log(MainWindowTr::tr("Tun Settings changed") + ": " + MainWindowTr::tr("Restart Tun to take effect."));
        }
        if (changed(MwArg::ChoosePort) || suggestRestartProxy) {
            m_connection->noteRestartNeeded(changed(MwArg::Route) ? Engine::RestartReason::Routing : Engine::RestartReason::Settings);
        }
        emit statusChanged();
        if (changed(MwArg::NeedRestart)) {
            if (!interactive) {
                MW_show_log(MainWindowTr::tr("Settings changed") + ": " + MainWindowTr::tr("Restart the program to take effect."));
            } else if (policy()->confirmRestartProgram()) {
                m_exit->requestExit(ExitReason::Restart, false);
            }
        }
    }

    EnginePolicy *EngineHost::policy() const {
        return m_policy.get();
    }

    ConnectionState EngineHost::connectionState() const {
        return m_connection->connectionState();
    }

    std::shared_ptr<Configs::Profile> EngineHost::runningSnapshot() const {
        return m_connection->runningSnapshot();
    }

    int EngineHost::resolveLastProfile() {
        return m_connection->resolveLastProfile();
    }

    quint64 EngineHost::nextStartSerial() {
        return m_connection->nextStartSerial();
    }

    void EngineHost::start(const StartRequest &request) {
        m_connection->start(request);
    }

    void EngineHost::stop(const StopOptions &options) {
        m_connection->stop(options);
    }

    bool EngineHost::setSystemProxy(bool enable, const ModeChange &change) {
        return m_connection->setSystemProxy(enable, change);
    }

    bool EngineHost::setTun(bool enable, const ModeChange &change) {
        return m_connection->setTun(enable, change);
    }

    bool EngineHost::chooseRoute(int routeId, bool interactive, quint64 serial) {
        return m_connection->chooseRoute(routeId, interactive, serial);
    }

    int EngineHost::clearTestResults(const QList<int> &profileIds) {
        QList<int> gids;
        const int n = ProfileService::ClearTestResults(profileIds, &gids);
        if (n > 0) emit testResultsCleared(gids);
        return n;
    }

    std::optional<LiveRates> EngineHost::freshLiveRates(qint64 maxAgeMs) const {
        if (m_connection->runningSnapshot() == nullptr) return std::nullopt;
        return m_stats->freshLiveRates(maxAgeMs);
    }

    bool EngineHost::testsRunning() {
        return m_tests->isRunning();
    }

    bool EngineHost::testsStopRequested() const {
        return m_tests->stopRequested();
    }

    bool EngineHost::runTests(TestKind kind, const QList<int> &profileIds, const std::function<void()> &onFinished) {
        switch (kind) {
            case Engine::TestKind::Latency: return m_tests->runUrlTests(profileIds, onFinished, false);
            case Engine::TestKind::Ip: return m_tests->runIpTests(profileIds, onFinished, false);
            case Engine::TestKind::Speed: break;
        }
        return m_tests->runSpeedTests(profileIds, false, onFinished, false);
    }

    void EngineHost::queueUrlTests(const QList<int> &profileIds, const std::function<void()> &onFinished) {
        m_tests->queueUrlTests(profileIds, onFinished, false);
    }

    void EngineHost::stopTests() {
        m_tests->stop();
    }

    void EngineHost::noteRestartNeeded(RestartReason reason) {
        m_connection->noteRestartNeeded(reason);
    }

    bool EngineHost::restartPending() const {
        return m_connection->restartPending();
    }

    bool EngineHost::requestExit(ExitReason reason) {
        ENGINE_CHECK_MAIN_THREAD();
        return m_exit->requestExit(reason);
    }

    QString EngineHost::degradedReason() const {
        const auto &settings = Configs::dataManager->settingsRepo;
        const auto killSwitchFailed = [&settings] {
            return settings->kill_switch && Sys::KillSwitch::instance()->state() == Sys::KillSwitch::State::Failed;
        };
        if (!m_options.headless) {
            if (!settings->core_running) return QStringLiteral("core_unavailable");
            if (killSwitchFailed()) return QStringLiteral("kill_switch_failed");
            return {};
        }
        if (m_coreSupervisor->state() == CoreState::GaveUp) return QStringLiteral("core_gave_up");
        if (!settings->core_running) return QStringLiteral("core_unavailable");
        if (killSwitchFailed()) return QStringLiteral("kill_switch_failed");
        if (RemoteApi::EffectiveConfig().config.enabled &&
            RemoteApi::Server::instance()->lastResult().state != RemoteApi::ApplyResult::State::Listening) {
            return QStringLiteral("api_unavailable");
        }
        if (m_connection->tunRestoreHeld()) return QStringLiteral("tun_unavailable");
        if (m_resumeSuppressed) return QStringLiteral("resume_suppressed");
        if (m_vpnAuth->authFailed()) return QStringLiteral("vpn_auth_failed");
        if (!m_vpnAuth->pendingChallenges().isEmpty()) return QStringLiteral("vpn_challenge_pending");
        return {};
    }

    bool EngineHost::systemProxySupported() const {
        return m_options.systemProxy;
    }

    bool EngineHost::tunPrivileged() {
        return m_connection->tunPrivileged();
    }

    bool EngineHost::resumesOnBoot() const {
        return m_options.resume && (m_options.headless || Configs::dataManager->settingsRepo->remember_enable);
    }

    bool EngineHost::tunRestoreHeld() const {
        return m_connection->tunRestoreHeld();
    }

    QString EngineHost::tunBlockedReason() {
        return m_connection->tunBlockedReason();
    }

    bool EngineHost::runningTun() const {
        return m_connection->runningTun();
    }

    void EngineHost::registerPeriodicJobs() {
        auto* runner = Throne::PeriodicRunner::instance();
        // Interval is sign-encoded in settings (negative = disabled); < 30 min counts as off.
        const auto minutesOf = [](int v) { return v >= 30 ? v : 0; };
        // Every poll while enabled: each group keeps its own schedule, persisted as its sub_last_update.
        runner->Add({
            {},
            [minutesOf] { return minutesOf(Configs::dataManager->settingsRepo->sub_auto_update) > 0 ? 1 : 0; },
            nullptr,
            nullptr,
            [] { Subscription::updater()->CheckAutoUpdate(); },
        });
        runner->Add({
            MainWindowTr::tr("routing profiles"),
            [minutesOf] { return minutesOf(Configs::dataManager->settingsRepo->route_auto_update); },
            [] { return Configs::dataManager->settingsRepo->route_auto_update_last; },
            [](qint64 t) {
                Configs::dataManager->settingsRepo->route_auto_update_last = t;
                Configs::dataManager->settingsRepo->Save();
            },
            [] { UI_update_all_remote_routes(true); },
        });
        runner->Add({
            {},
            [] { return 1; },
            nullptr,
            nullptr,
            [] { Scanner::IpListUpdater::instance()->CheckAutoUpdate(); },
        });
    }

    EngineHost *host() {
        return engineHostInstance;
    }
} // namespace Engine
