#pragma once

#include <memory>
#include <optional>

#include <QJsonObject>
#include <QList>
#include <QStringList>

#include "include/engine/EngineControl.h"
#include "include/engine/RuntimeOptions.h"

namespace RemoteApi {
    class Router;
}

namespace Engine {
    class AutoSelectorController;
    class ConnectionManager;
    class CoreSupervisor;
    class EnginePolicy;
    class ExitController;
    class StatsService;
    class TestObserver;
    class TestRunner;
    class VpnAuthController;

    // Owns and wires every engine object, and is the EngineControl the Throne API drives. Main thread unless noted.
    class EngineHost final : public EngineControl {
        Q_OBJECT

    public:
        // Once, after initDB, DS_cores, the translator, Logging::Init and API::defaultClient; builds and wires, starts nothing.
        // Never destroyed: ~TestRunner joins its pool and the core process outlives the app object.
        static EngineHost *create(const RuntimeOptions &options = {});

        ~EngineHost() override;

        const RuntimeOptions &options() const { return m_options; }

        struct BootStart {
            int profileId = -1;
            quint64 serial = 0;
        };

        // Once the front end exists: binds its answers, then the random inbound port, the core IPC listener, the kill switch,
        // the core and the API. observer may be null. Without bootStart the remembered profile starts as the GUI always did
        // (remember_enable && remember_id >= 0); with it, exactly that request starts once the core is up. The API answers
        // 503 "starting" until this returns.
        void init(EnginePolicy *policy, TestObserver *observer, std::optional<BootStart> bootStart = std::nullopt);

        // The Throne API's status object; main thread. For the headless HEALTH reply.
        QJsonObject statusJson() const;

        // The boot resume was skipped by the crash-loop guard (headless): degraded resume_suppressed until cleared.
        void setResumeSuppressed(bool suppressed);

        // Back to the headless answers, for a front end that dies first.
        void detachFrontEnd();

        // Idempotent; ends with KillSwitch::shutdown(). interactive false: a failing or slow stop is logged, never shown.
        void shutdown(bool interactive = true);

        // Engine half of MwMessage::UpdateSettings; its dialogs appear only when interactive.
        void applySettingsChange(const QStringList &flags, bool interactive);

        // Stable for the process: forwards to the bound policy, or to the headless answers while none is bound.
        EnginePolicy *policy() const;

        ConnectionManager *connection() const { return m_connection; }

        CoreSupervisor *supervisor() const { return m_coreSupervisor; }

        StatsService *stats() const { return m_stats; }

        TestRunner *tests() const { return m_tests.get(); }

        AutoSelectorController *autoSelector() const { return m_autoSelector.get(); }

        VpnAuthController *vpnAuth() const { return m_vpnAuth; }

        ExitController *exitController() const { return m_exit; }

        ConnectionState connectionState() const override;

        std::shared_ptr<Configs::Profile> runningSnapshot() const override;

        int resolveLastProfile() override;

        quint64 nextStartSerial() override;

        void start(const StartRequest &request) override;

        void stop(const StopOptions &options) override;

        bool setSystemProxy(bool enable, const ModeChange &change) override;

        bool setTun(bool enable, const ModeChange &change) override;

        bool chooseRoute(int routeId, bool interactive, quint64 serial) override;

        int clearTestResults(const QList<int> &profileIds) override;

        std::optional<LiveRates> freshLiveRates(qint64 maxAgeMs) const override;

        bool testsRunning() override;

        bool testsStopRequested() const override;

        bool runTests(TestKind kind, const QList<int> &profileIds, const std::function<void()> &onFinished) override;

        void queueUrlTests(const QList<int> &profileIds, const std::function<void()> &onFinished) override;

        void stopTests() override;

        void noteRestartNeeded(RestartReason reason) override;

        bool restartPending() const override;

        bool requestExit(ExitReason reason) override;

        QString degradedReason() const override;

        bool systemProxySupported() const override;

        bool tunPrivileged() override;

        bool resumesOnBoot() const override;

        bool tunRestoreHeld() const override;

        QString tunBlockedReason() override;

        // Any thread. The running config carries a tun inbound (HEALTH tun_running).
        bool runningTun() const;

    signals:
        // Where applySettingsChange used to refresh the status bar; slots run before its restart prompt.
        void statusChanged();

        // After clearTestResults() cleared at least one profile; the groups of the cleared profiles.
        void testResultsCleared(const QList<int> &groupIds);

    private:
        class PolicyRelay;
        class ObserverRelay;

        explicit EngineHost(const RuntimeOptions &options);

        void construct();

        void registerPeriodicJobs();

        std::unique_ptr<PolicyRelay> m_policy;
        std::unique_ptr<ObserverRelay> m_observer;
        StatsService *m_stats = nullptr;
        CoreSupervisor *m_coreSupervisor = nullptr;
        ConnectionManager *m_connection = nullptr;
        VpnAuthController *m_vpnAuth = nullptr;
        std::unique_ptr<TestRunner> m_tests;
        // After m_tests, so it is destroyed first.
        std::unique_ptr<AutoSelectorController> m_autoSelector;
        ExitController *m_exit = nullptr;
        RemoteApi::Router *m_router = nullptr;
        bool m_initialized = false;
        RuntimeOptions m_options;
        bool m_resumeSuppressed = false;
        // Set when init() returns; until then the API answers 503 "starting".
        bool m_apiReady = false;
    };

    // Null before EngineHost::create().
    EngineHost *host();
} // namespace Engine
