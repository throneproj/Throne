#include "include/engine/tests/TestRunner.h"

#include "include/api/RPC.h"
#include "include/configs/common/DnsTunnel.h"
#include "include/configs/common/Protocols.h"
#include "include/configs/generate.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EnginePolicy.h"
#include "include/engine/EngineTr.h"
#include "include/engine/services/AssetUpdates.h"
#include "include/engine/services/ProfileService.h"
#include "include/engine/tests/TestObserver.h"
#include "include/global/Configs.hpp"
#include "include/stats/traffic/TrafficStatsManager.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QSemaphore>
#include <QSet>
#include <QThread>
#include <QThreadPool>

#include <limits>
#include <utility>

namespace Engine {
    namespace {
        constexpr int kTestRunnerBatchSize = 100;
        constexpr int kTestRunnerLatencyPollIntervalMs = 200;
        constexpr int kTestRunnerSpeedPollIntervalMs = 100;
        constexpr int kTestRunnerPoolThreads = 10;

        QList<int> testRunnerWithoutAutoSelectors(const QList<int>& profileIDs) {
            const auto selectors = Configs::dataManager->profilesRepo->GetProfileIdsByType("autoselector");
            if (selectors.isEmpty()) return profileIDs;
            const QSet<int> skip(selectors.begin(), selectors.end());
            QList<int> filtered;
            filtered.reserve(profileIDs.size());
            for (int id : profileIDs) {
                if (!skip.contains(id)) filtered << id;
            }
            return filtered;
        }

        bool testRunnerIsTestAborted(const QString& error) {
            return error.contains("test aborted") || error.contains("context canceled");
        }

        bool testRunnerIsVpnProfile(const std::shared_ptr<Configs::Profile>& ent) {
            return ent != nullptr && Configs::Protocols::Is(ent->type, Configs::Protocols::VpnEndpoint);
        }

        // Mirror the core's URLTestTimeout, TunnelStartupTimeout, TunnelHandshakeTimeout and normalizeConcurrency.
        constexpr int kTestRunnerCoreDefaultTimeoutMs = 3000;
        constexpr int kTestRunnerTunnelStartupMs = 5000;
        constexpr int kTestRunnerTunnelHandshakeMs = 10000;
        constexpr int kTestRunnerCoreMaxConcurrency = 100;
        constexpr int kTestRunnerRpcSlackMs = 30000;

        // Test/IPTest answer only once the whole batch is done, so the deadline covers its worst case.
        int testRunnerBatchRpcTimeoutMs(qsizetype tagCount, int requestsPerTag, int startupAllowanceMs) {
            const auto& settings = Configs::dataManager->settingsRepo;
            int concurrency = settings->test_concurrent;
            if (concurrency <= 0 || concurrency >= 500) concurrency = kTestRunnerCoreMaxConcurrency;
            const qint64 timeoutMs = settings->url_test_timeout_ms > 0 ? settings->url_test_timeout_ms : kTestRunnerCoreDefaultTimeoutMs;
            const qint64 rounds = (qMax<qsizetype>(tagCount, 1) + concurrency - 1) / concurrency;
            const qint64 total = rounds * (kTestRunnerTunnelHandshakeMs + requestsPerTag * timeoutMs + kTestRunnerTunnelStartupMs + startupAllowanceMs) +
                                 kTestRunnerRpcSlackMs;
            return static_cast<int>(qMin<qint64>(total, std::numeric_limits<int>::max()));
        }

        int testRunnerStartupAllowanceMs(const QMap<QString, int>& tag2entID, int entID) {
            int allowance = 0;
            const auto add = [&allowance](int id) {
                if (id >= 0) allowance = qMax(allowance, Configs::DnsTunnelStartupAllowanceMs(Configs::dataManager->profilesRepo->GetProfile(id)));
            };
            add(entID);
            for (const int id : tag2entID) add(id);
            return allowance;
        }

        int testRunnerResolveEntID(const QMap<QString, int>& tag2entID, const std::string& tag, int fallback) {
            if (tag2entID.isEmpty()) return fallback;
            return tag2entID.value(QString::fromStdString(tag), -1);
        }

        // Mirror the core's FetchServersTimeout, its server pings and speedtestTimeout; the slack covers building and closing
        // the test box.
        constexpr int kTestRunnerSpeedServerListMs = 8000;
        constexpr int kTestRunnerSpeedPingMs = 4000;
        constexpr int kTestRunnerSpeedPhaseMs = 5000;
        constexpr int kTestRunnerSpeedSlackMs = 20000;
        constexpr int kTestRunnerSpeedStopWaitMs = 5000;

        // A deadline short of the core's own leaves it testing under the tag the next profile reuses.
        int testRunnerSpeedRpcTimeoutMs(const libcore::SpeedTestRequest& req) {
            const bool simple = req.simple_download.value();
            const qint64 phaseMs = req.timeout_ms.value() > 0 ? req.timeout_ms.value()
                                   : simple                   ? kTestRunnerCoreDefaultTimeoutMs
                                                              : kTestRunnerSpeedPhaseMs;
            qint64 total = kTestRunnerSpeedSlackMs;
            if (simple) {
                total += phaseMs;
            } else {
                total += kTestRunnerSpeedServerListMs + kTestRunnerSpeedPingMs;
                if (req.test_download.value()) total += phaseMs;
                if (req.test_upload.value()) total += phaseMs;
            }
            return static_cast<int>(qMin<qint64>(total, std::numeric_limits<int>::max()));
        }

        // For a speed test the GUI gave up on: cancels it and waits, bounded, until the core reports it over.
        void testRunnerStopSpeedTest() {
            bool ok = false;
            API::defaultClient->StopTests(&ok);
            if (!ok) return;
            const QDeadlineTimer deadline(kTestRunnerSpeedStopWaitMs);
            while (!deadline.hasExpired()) {
                bool queried = false;
                const auto state = API::defaultClient->QueryCurrentSpeedTests(&queried);
                if (!queried || !state.is_running.value()) return;
                QThread::msleep(kTestRunnerSpeedPollIntervalMs);
            }
        }

        constexpr int kTestRunnerResultsWaitMs = 15000;

        // Returns once the results queued to the main thread before the call are set and saved; bounded, as a main thread
        // that is shutting down never gets to them.
        void testRunnerAwaitResults() {
            auto* app = QCoreApplication::instance();
            if (app == nullptr || QThread::currentThread() == app->thread()) return;
            const auto reached = std::make_shared<QSemaphore>();
            runOnUiThread([reached] { reached->release(); });
            if (!reached->tryAcquire(1, kTestRunnerResultsWaitMs)) return;
            // What runs after a test may load a profile from the database instead of the identity map.
            Configs::dataManager->profilesRepo->WaitForWrites(kTestRunnerResultsWaitMs);
        }

        // A profile's new result fields, decided on a test thread and set on the main thread.
        struct TestRunnerUpdate {
            std::shared_ptr<Configs::Profile> ent;
            std::function<void(Configs::Profile&)> set;
        };

        using TestRunnerUpdates = QList<TestRunnerUpdate>;

        // Queued, never waited for: a pool thread blocked on the main thread can deadlock the pool's shutdown.
        void testRunnerApply(TestObserver* observer, TestRunnerUpdates updates, Configs::ProfileColumns columns,
                             const TestRunner::ResultHook& onResult = {}) {
            if (updates.isEmpty()) return;
            runOnUiThread([observer, updates = std::move(updates), columns, onResult] {
                QList<std::shared_ptr<Configs::Profile>> applied;
                QList<int> ids;
                for (const auto& [ent, set] : updates) {
                    set(*ent);
                    applied << ent;
                    ids << ent->id;
                }
                Configs::dataManager->profilesRepo->SaveColumns(applied, columns);
                observer->testedProfilesChanged(ids);
                if (!onResult) return;
                for (const auto& ent : applied) onResult(ent);
            });
        }

        // For a result that is not aborted; a failed lookup clears the old exit.
        TestRunnerUpdate testRunnerIpUpdate(const std::shared_ptr<Configs::Profile>& ent, const libcore::IPTestRes& res) {
            if (const auto error = QString::fromStdString(res.error.value()); !error.isEmpty()) {
                MW_show_log(MainWindowTr::tr("[%1] Exit IP test error: %2").arg(ent->outbound->DisplayTypeAndName(), error));
                return {ent, [](Configs::Profile& p) {
                    p.ip_out.clear();
                    p.test_country.clear();
                }};
            }
            return {ent, [ip = QString::fromStdString(res.ip.value()), country = QString::fromStdString(res.country_code.value())](Configs::Profile& p) {
                p.ip_out = ip;
                p.test_country = country;
            }};
        }

        // For a check that is not aborted.
        TestRunnerUpdate testRunnerServiceUpdate(const std::shared_ptr<Configs::Profile>& ent, const libcore::ServiceTestResult& res,
                                                 const QList<ServiceChecks::Category>& categories) {
            if (const auto error = QString::fromStdString(res.error.value()); !error.isEmpty()) {
                MW_show_log(MainWindowTr::tr("[%1] service check error: %2").arg(ent->outbound->DisplayTypeAndName(), error));
            }
            return {ent, [json = ServiceChecks::ResultJson(res, categories, QDateTime::currentSecsSinceEpoch())](Configs::Profile& p) {
                p.service_checks = json;
            }};
        }

        constexpr int kTestRunnerServiceBatchSize = 25;
        constexpr int kTestRunnerServiceConcurrency = 4;
        constexpr int kTestRunnerServiceRequestTimeoutMs = 10000;
        constexpr qint64 kTestRunnerServiceRoundMs = 200000;
        constexpr qint64 kTestRunnerServiceSlackMs = 60000;

        int testRunnerServiceRpcTimeoutMs(qsizetype tagCount) {
            const qint64 rounds = (qMax<qsizetype>(tagCount, 1) + kTestRunnerServiceConcurrency - 1) / kTestRunnerServiceConcurrency;
            return static_cast<int>(qMin<qint64>(rounds * kTestRunnerServiceRoundMs + kTestRunnerServiceSlackMs, std::numeric_limits<int>::max()));
        }

        // A poll and the final reply can both carry a finished profile; whichever comes first applies it.
        class TestRunnerClaimedTags {
        public:
            bool claim(const QString& tag) {
                QMutexLocker locker(&mu_);
                if (tags_.contains(tag)) return false;
                tags_.insert(tag);
                return true;
            }

        private:
            QMutex mu_;
            QSet<QString> tags_;
        };

        // Target is deduced, not named: access control applies to naming a private type.
        template <typename Req, typename Target>
        void testRunnerFillCommonTestReq(Req& req, const Target& target) {
            for (const auto& tag : target.outboundTags) req.outbound_tags.push_back(tag.toStdString());
            req.config = target.coreConfig.toStdString();
            req.use_default_outbound = target.useDefaultOutbound;
            req.xray_config = target.xrayConfig.toStdString();
            req.need_xray = !target.xrayConfig.isEmpty();
            req.xray_outbound_dns_strategy = target.xrayDnsStrategy.toStdString();
            for (const auto& xc : target.xrayFullConfigs) req.xray_full_configs.push_back(xc.toStdString());
        }

        // Stopping does not join: the poll may itself sit in a 30s RPC and must not stall the batch.
        class TestRunnerResultPoller {
        public:
            TestRunnerResultPoller(std::function<void()> tick, int intervalMs)
                : stop_(std::make_shared<std::atomic<bool>>(false)) {
                runOnNewThread([stop = stop_, tick = std::move(tick), intervalMs] {
                    while (!stop->load()) {
                        QThread::msleep(intervalMs);
                        if (stop->load()) break;
                        tick();
                    }
                });
            }

            ~TestRunnerResultPoller() { stop_->store(true); }

            TestRunnerResultPoller(const TestRunnerResultPoller&) = delete;
            TestRunnerResultPoller& operator=(const TestRunnerResultPoller&) = delete;

        private:
            std::shared_ptr<std::atomic<bool>> stop_;
        };

        class TestRunnerNullObserver final : public TestObserver {
        public:
            void testPanelSeeded(TestKind, int, const QList<int>&) override {}
            void testProgressed() override {}
            void testPanelsCleared() override {}
            void testViewChanged(bool) override {}
            void speedTestProgress(const QString&, const libcore::SpeedTestResult&) override {}
            void testedProfilesChanged(const QList<int>&) override {}
        };

        TestObserver* testRunnerNullObserver() {
            static TestRunnerNullObserver observer;
            return &observer;
        }
    } // namespace

    TestRunner::TestRunner(EnginePolicy* policy, TestObserver* observer, RunningSnapshot runningSnapshot)
        : policy_(policy),
          observer_(observer != nullptr ? observer : testRunnerNullObserver()),
          runningSnapshot_(std::move(runningSnapshot)) {
        pool_.setMaxThreadCount(kTestRunnerPoolThreads);
    }

    bool TestRunner::isRunning() {
        if (!session_.tryLock()) return true;
        session_.unlock();
        return false;
    }

    void TestRunner::stop() {
        stopRequested_.store(true);
        bool ok;
        API::defaultClient->StopTests(&ok);

        if (!ok) {
            MW_show_log(MainWindowTr::tr("Failed to stop tests"));
        }
    }

    QString TestRunner::contextName(int entID) const {
        if (entID != -1) {
            if (auto e = Configs::dataManager->profilesRepo->GetProfile(entID)) return e->outbound->DisplayTypeAndName();
        }
        return MainWindowTr::tr("a tested profile");
    }

    int TestRunner::runningProfileId() const {
        const auto running = runningSnapshot_();
        return running ? running->id : -1;
    }

    void TestRunner::reportSessionBusy(bool interactive) const {
        const auto text = MainWindowTr::tr("Another test is still running. Wait for it to finish, or stop it first.");
        // Auto-selector ranking calls in from a worker thread, where no widget may be created.
        if (interactive && QThread::currentThread() == QCoreApplication::instance()->thread()) policy_->testSessionBusy(text);
        else MW_show_log(text);
    }

    void TestRunner::reportProbeFailure(const QString& failedText, const QString& coreError, int contextID) const {
        if (testRunnerIsTestAborted(coreError)) return;
        if (AssetUpdates::HandleXrayGeoError(policy_, coreError, contextName(contextID), interactive_.load())) return;
        MW_show_log(failedText.arg(coreError.isEmpty() ? MainWindowTr::tr("The core did not answer the test") : coreError));
    }

    void TestRunner::runUrlProbe(const Target& target) {
        if (stopRequested_.load()) {
            MW_show_log(MainWindowTr::tr("Profile test aborted"));
            return;
        }

        libcore::TestReq req;
        testRunnerFillCommonTestReq(req, target);
        req.url = Configs::dataManager->settingsRepo->test_latency_url.toStdString();
        req.max_concurrency = Configs::dataManager->settingsRepo->test_concurrent;
        req.test_timeout_ms = Configs::dataManager->settingsRepo->url_test_timeout_ms;

        // The test box dies with the RPC, so the verdict has to be asked for up front.
        for (auto it = target.tag2entID.cbegin(); it != target.tag2entID.cend(); ++it) {
            if (testRunnerIsVpnProfile(Configs::dataManager->profilesRepo->GetProfile(it.value()))) {
                req.vpn_endpoint_tags.push_back(it.key().toStdString());
            }
        }

        const auto claimed = std::make_shared<TestRunnerClaimedTags>();
        bool rpcOK = false;
        QString coreError;
        libcore::TestResp result;
        {
            TestRunnerResultPoller poller([this, gen = sessionGen_.load(), tag2entID = target.tag2entID, onResult = target.onResult, claimed] {
                if (staleGen(gen)) return;
                bool ok = false;
                const auto resp = API::defaultClient->QueryURLTest(&ok);
                // Checked again: this poll can sit in the RPC while its batch ends and the tags are reused.
                if (staleGen(gen)) return;
                if (!ok || resp.results.empty()) return;

                TestRunnerUpdates updates;
                for (const auto& res : resp.results) {
                    observer_->testProgressed();
                    observer_->testViewChanged(false);
                    // Only the final reply tells a failed profile from a failed local network.
                    if (!res.error.value().empty()) continue;
                    const int entid = testRunnerResolveEntID(tag2entID, res.outbound_tag.value(), -1);
                    if (entid == -1) continue;
                    auto ent = Configs::dataManager->profilesRepo->GetProfile(entid);
                    if (ent == nullptr || !claimed->claim(QString::fromStdString(res.outbound_tag.value()))) continue;
                    updates.append({ent, [ms = res.latency_ms.value()](Configs::Profile& p) { p.SetLatency(ms); }});
                }
                if (updates.isEmpty()) return;
                observer_->testViewChanged(true);
                testRunnerApply(observer_, std::move(updates), Configs::ProfileColumn::Latency, onResult);
            }, kTestRunnerLatencyPollIntervalMs);

            result = API::defaultClient->Test(&rpcOK, req, &coreError,
                                              testRunnerBatchRpcTimeoutMs(target.outboundTags.size(), 2,
                                                                          testRunnerStartupAllowanceMs(target.tag2entID, target.entID)));
        }

        if (!rpcOK) {
            reportProbeFailure(MainWindowTr::tr("URL test failed: %1"), coreError, target.entID);
            return;
        }

        QHash<QString, bool> vpnConnected;
        for (const auto& st : result.vpn_status) {
            vpnConnected.insert(QString::fromStdString(st.tag.value()), st.connected.value());
        }

        TestRunnerUpdates updates;
        TestRunnerUpdates inconclusive;
        for (const auto& res : result.results) {
            const int entid = testRunnerResolveEntID(target.tag2entID, res.outbound_tag.value(), target.entID);
            if (entid == -1) {
                MW_show_log(MainWindowTr::tr("Something is very wrong, the subject ent cannot be found!"));
                continue;
            }
            auto ent = Configs::dataManager->profilesRepo->GetProfile(entid);
            if (ent == nullptr) {
                MW_show_log(MainWindowTr::tr("Profile manager data is corrupted, try again."));
                continue;
            }
            const auto tag = QString::fromStdString(res.outbound_tag.value());
            const auto error = QString::fromStdString(res.error.value());
            if (testRunnerIsTestAborted(error) || !claimed->claim(tag)) continue;
            int latency = -1;
            if (error.isEmpty()) {
                latency = res.latency_ms.value();
            } else if (testRunnerIsVpnProfile(ent) && vpnConnected.value(tag, false)) {
                latency = Configs::kLatencyConnectOnly;
            } else if (res.inconclusive.value()) {
                inconclusive.append({ent, [](Configs::Profile& p) { p.SetLatency(Configs::kLatencyInconclusive); }});
                continue;
            } else {
                MW_show_log(MainWindowTr::tr("[%1] test error: %2").arg(ent->outbound->DisplayTypeAndName(), error));
            }
            updates.append({ent, [latency](Configs::Profile& p) { p.SetLatency(latency); }});
        }
        inconclusive_.fetch_add(static_cast<int>(inconclusive.size()));
        // Without the hook: a ranking sweep must not count a no-network result as measured.
        testRunnerApply(observer_, std::move(inconclusive), Configs::ProfileColumn::Latency);
        testRunnerApply(observer_, std::move(updates), Configs::ProfileColumn::Latency, target.onResult);
    }

    void TestRunner::runIpProbe(const Target& target) {
        if (stopRequested_.load()) {
            MW_show_log(MainWindowTr::tr("Profile test aborted"));
            return;
        }

        libcore::IPTestRequest req;
        testRunnerFillCommonTestReq(req, target);
        req.max_concurrency = Configs::dataManager->settingsRepo->test_concurrent;
        req.test_timeout_ms = Configs::dataManager->settingsRepo->url_test_timeout_ms;
        req.provider = Configs::dataManager->settingsRepo->exit_lookup_provider.toStdString();

        const auto claimed = std::make_shared<TestRunnerClaimedTags>();
        bool rpcOK = false;
        QString coreError;
        libcore::IPTestResp result;
        {
            TestRunnerResultPoller poller([this, gen = sessionGen_.load(), tag2entID = target.tag2entID, claimed] {
                if (staleGen(gen)) return;
                bool ok = false;
                const auto resp = API::defaultClient->QueryIPTest(&ok);
                if (staleGen(gen)) return;
                if (!ok || resp.results.empty()) return;

                TestRunnerUpdates updates;
                for (const auto& res : resp.results) {
                    observer_->testProgressed();
                    observer_->testViewChanged(false);
                    if (testRunnerIsTestAborted(QString::fromStdString(res.error.value()))) continue;
                    const int entid = testRunnerResolveEntID(tag2entID, res.outbound_tag.value(), -1);
                    if (entid == -1) continue;
                    auto ent = Configs::dataManager->profilesRepo->GetProfile(entid);
                    if (ent == nullptr || !claimed->claim(QString::fromStdString(res.outbound_tag.value()))) continue;
                    updates << testRunnerIpUpdate(ent, res);
                }
                if (updates.isEmpty()) return;
                observer_->testViewChanged(true);
                testRunnerApply(observer_, std::move(updates), Configs::ProfileColumn::ExitIp);
            }, kTestRunnerLatencyPollIntervalMs);

            result = API::defaultClient->IPTest(&rpcOK, req, &coreError,
                                                testRunnerBatchRpcTimeoutMs(target.outboundTags.size(), 1,
                                                                            testRunnerStartupAllowanceMs(target.tag2entID, target.entID)));
        }

        if (!rpcOK) {
            reportProbeFailure(MainWindowTr::tr("Exit IP test failed: %1"), coreError, target.entID);
            return;
        }

        TestRunnerUpdates updates;
        for (const auto& res : result.results) {
            const int entid = testRunnerResolveEntID(target.tag2entID, res.outbound_tag.value(), target.entID);
            if (entid == -1) {
                MW_show_log(MainWindowTr::tr("Something is very wrong, the subject ent cannot be found!"));
                continue;
            }
            auto ent = Configs::dataManager->profilesRepo->GetProfile(entid);
            if (ent == nullptr) {
                MW_show_log(MainWindowTr::tr("Profile manager data is corrupted, try again."));
                continue;
            }
            if (testRunnerIsTestAborted(QString::fromStdString(res.error.value()))) continue;
            if (!claimed->claim(QString::fromStdString(res.outbound_tag.value()))) continue;
            updates << testRunnerIpUpdate(ent, res);
        }
        testRunnerApply(observer_, std::move(updates), Configs::ProfileColumn::ExitIp);
    }

    void TestRunner::runServiceProbe(const Target& target, const std::shared_ptr<const ServiceChecks::Definitions>& definitions) {
        if (stopRequested_.load()) {
            MW_show_log(MainWindowTr::tr("Profile test aborted"));
            return;
        }

        libcore::ServiceTestRequest req;
        testRunnerFillCommonTestReq(req, target);
        req.max_concurrency = kTestRunnerServiceConcurrency;
        req.test_timeout_ms = kTestRunnerServiceRequestTimeoutMs;
        req.checks = definitions->text.toStdString();
        const auto only = ServiceChecks::Enabled(*definitions);
        // Turned off mid-run: an empty list would run every check.
        if (only.isEmpty()) return;
        for (const auto& id : only) req.only.push_back(id.toStdString());
        req.test_current = target.testCurrent;

        const auto claimed = std::make_shared<TestRunnerClaimedTags>();
        bool rpcOK = false;
        QString coreError;
        libcore::ServiceTestResponse result;
        {
            TestRunnerResultPoller poller([this, gen = sessionGen_.load(), tag2entID = target.tag2entID, definitions, claimed] {
                if (staleGen(gen)) return;
                bool ok = false;
                const auto resp = API::defaultClient->QueryServiceTest(&ok);
                if (staleGen(gen)) return;
                if (!ok || resp.results.empty()) return;

                TestRunnerUpdates updates;
                for (const auto& res : resp.results) {
                    const int entid = testRunnerResolveEntID(tag2entID, res.outbound_tag.value(), -1);
                    if (entid == -1) continue;
                    auto ent = Configs::dataManager->profilesRepo->GetProfile(entid);
                    if (ent == nullptr || !claimed->claim(QString::fromStdString(res.outbound_tag.value()))) continue;
                    observer_->testProgressed();
                    observer_->testViewChanged(false);
                    // An aborted check says nothing about the profile, so its previous one stays.
                    if (testRunnerIsTestAborted(QString::fromStdString(res.error.value()))) continue;
                    updates << testRunnerServiceUpdate(ent, res, definitions->categories);
                }
                if (updates.isEmpty()) return;
                observer_->testViewChanged(true);
                testRunnerApply(observer_, std::move(updates), Configs::ProfileColumn::ServiceChecks);
            }, kTestRunnerLatencyPollIntervalMs);

            result = API::defaultClient->ServiceTest(&rpcOK, req, &coreError, testRunnerServiceRpcTimeoutMs(target.outboundTags.size()));
        }

        if (!rpcOK) {
            reportProbeFailure(MainWindowTr::tr("Service check failed: %1"), coreError, target.entID);
            return;
        }

        // The live instance changed under the check, so its answers may describe another exit.
        if (target.testCurrent && runningProfileId() != target.entID) return;
        TestRunnerUpdates updates;
        for (const auto& res : result.results) {
            const int entid = testRunnerResolveEntID(target.tag2entID, res.outbound_tag.value(), target.entID);
            if (entid == -1) {
                MW_show_log(MainWindowTr::tr("Something is very wrong, the subject ent cannot be found!"));
                continue;
            }
            auto ent = Configs::dataManager->profilesRepo->GetProfile(entid);
            if (ent == nullptr) {
                MW_show_log(MainWindowTr::tr("Profile manager data is corrupted, try again."));
                continue;
            }
            if (!claimed->claim(QString::fromStdString(res.outbound_tag.value()))) continue;
            observer_->testProgressed();
            if (testRunnerIsTestAborted(QString::fromStdString(res.error.value()))) continue;
            updates << testRunnerServiceUpdate(ent, res, definitions->categories);
        }
        testRunnerApply(observer_, std::move(updates), Configs::ProfileColumn::ServiceChecks);
    }

    bool TestRunner::runUrlTests(const QList<int>& profileIDs, const std::function<void()>& onFinished, bool interactive,
                                 const ResultHook& onResult) {
        return runLatencyGroup(LatencyKind::Url, profileIDs, onFinished, false, interactive, onResult);
    }

    void TestRunner::queueUrlTests(const QList<int>& profileIDs, const std::function<void()>& onFinished, bool interactive) {
        runOnNewThread([=, this] { runLatencyGroup(LatencyKind::Url, profileIDs, onFinished, true, interactive); });
    }

    bool TestRunner::runIpTests(const QList<int>& profileIDs, const std::function<void()>& onFinished, bool interactive) {
        return runLatencyGroup(LatencyKind::Ip, profileIDs, onFinished, false, interactive);
    }

    void TestRunner::queueIpTests(const QList<int>& profileIDs, const std::function<void()>& onFinished, bool interactive) {
        runOnNewThread([=, this] { runLatencyGroup(LatencyKind::Ip, profileIDs, onFinished, true, interactive); });
    }

    bool TestRunner::runChainStepTest(const QList<int>& hops, int gid, const std::function<void(const ChainStepReport&)>& onDone) {
        if (!session_.tryLock()) return false;
        sessionGen_.fetch_add(1);
        stopRequested_.store(false);
        interactive_.store(true);

        runOnNewThread([this, hops, gid, onDone] {
            ChainStepReport report;
            const auto build = Configs::BuildChainStepTestConfig(hops, gid);
            report.invalidHops = build.invalidHops;
            report.hasLanding = build.hasLanding;
            if (!build.error.isEmpty()) {
                report.error = build.error;
            } else if (build.build != nullptr && !build.build->outboundTags.isEmpty()) {
                Target target;
                target.coreConfig = QJsonObject2QString(build.build->coreConfig, false);
                target.xrayConfig = build.build->isXrayNeeded ? QJsonObject2QString(build.build->xrayConfig, false) : "";
                target.xrayFullConfigs = build.build->xrayFullConfigs;
                target.outboundTags = build.build->outboundTags;
                target.xrayDnsStrategy = build.build->xrayDnsStrategy;

                libcore::TestReq req;
                testRunnerFillCommonTestReq(req, target);
                req.url = Configs::dataManager->settingsRepo->test_latency_url.toStdString();
                req.max_concurrency = Configs::dataManager->settingsRepo->test_concurrent;
                req.test_timeout_ms = Configs::dataManager->settingsRepo->url_test_timeout_ms;
                for (const auto& tag : build.vpnEndpointTags) req.vpn_endpoint_tags.push_back(tag.toStdString());

                int allowance = 0;
                for (const int id : hops) {
                    allowance = qMax(allowance, Configs::DnsTunnelStartupAllowanceMs(Configs::dataManager->profilesRepo->GetProfile(id)));
                }
                bool rpcOK = false;
                QString coreError;
                const auto result = API::defaultClient->Test(&rpcOK, req, &coreError,
                                                             testRunnerBatchRpcTimeoutMs(target.outboundTags.size(), 2, allowance));
                // Test also queues every result for QueryURLTest; drain them so a later run's poller never sees these tags.
                bool drained = false;
                API::defaultClient->QueryURLTest(&drained);

                if (!rpcOK) {
                    report.error = coreError.isEmpty() ? MainWindowTr::tr("The core did not answer the test") : coreError;
                } else {
                    QHash<QString, bool> vpnConnected;
                    for (const auto& st : result.vpn_status) vpnConnected.insert(QString::fromStdString(st.tag.value()), st.connected.value());
                    for (const auto& res : result.results) {
                        const auto tag = QString::fromStdString(res.outbound_tag.value());
                        if (!build.tag2probe.contains(tag)) continue;
                        const auto probe = build.tag2probe.value(tag);
                        ChainStepResult step{probe.hop, probe.alone};
                        const auto error = QString::fromStdString(res.error.value());
                        if (error.isEmpty()) {
                            step.latency = res.latency_ms.value();
                        } else if (testRunnerIsTestAborted(error)) {
                            report.aborted = true;
                            continue;
                        } else if (vpnConnected.value(tag, false)) {
                            step.latency = Configs::kLatencyConnectOnly;
                        } else {
                            step.latency = -1;
                            step.error = error;
                        }
                        report.results << step;
                    }
                }
            }
            session_.unlock();
            if (onDone) onDone(report);
        });
        return true;
    }

    bool TestRunner::runLatencyGroup(LatencyKind kind, const QList<int>& requestedIDs,
                                     const std::function<void()>& onFinished, bool waitForSession, bool interactive,
                                     const ResultHook& onResult) {
        const bool isUrl = kind == LatencyKind::Url;
        const auto panelKind = isUrl ? TestKind::Latency : TestKind::Ip;
        // Must fire on every exit path — a caller may be blocked on it.
        const auto finish = [onFinished] { if (onFinished) onFinished(); };

        const auto profileIDs = testRunnerWithoutAutoSelectors(requestedIDs);
        if (profileIDs.isEmpty()) {
            finish();
            return false;
        }
        if (waitForSession) {
            session_.lock();
        } else if (!session_.tryLock()) {
            reportSessionBusy(interactive);
            finish();
            return false;
        }
        sessionGen_.fetch_add(1);
        // Reset here, not on the worker: a stop() that lands before the worker runs must still count.
        stopRequested_.store(false);
        interactive_.store(interactive);

        runOnNewThread([this, profileIDs, panelKind, isUrl, finish, onResult]() {
            observer_->testPanelSeeded(panelKind, profileIDs.size(), profileIDs);
            observer_->testViewChanged(true);

            auto runBatch = [this, isUrl, &onResult](const QList<std::shared_ptr<Configs::Profile>>& profileSlice, const QList<int>& ids) {
                // Per batch, not per probe: a batch's probes drain each other's results, and tags restart each batch.
                sessionGen_.fetch_add(1);
                auto buildObject = Configs::BuildTestConfig(profileSlice);
                if (!buildObject->error.isEmpty()) {
                    MW_show_log(MainWindowTr::tr("Failed to build test config for batch: ") + buildObject->error);
                    return;
                }

                const int testCount = buildObject->fullConfigs.size() + (buildObject->outboundTags.empty() ? 0 : 1);
                if (testCount == 0) return;

                QSemaphore batchDone;
                const auto probe = [this, isUrl, &batchDone](const Target& target) {
                    pool_.start([this, isUrl, target, &batchDone] {
                        const QSemaphoreReleaser releaser(batchDone);
                        if (isUrl) runUrlProbe(target);
                        else runIpProbe(target);
                    });
                };

                for (const auto& entID : buildObject->fullConfigs.keys()) {
                    Target target;
                    target.coreConfig = buildObject->fullConfigs[entID];
                    target.useDefaultOutbound = true;
                    target.entID = entID;
                    target.onResult = onResult;
                    probe(target);
                }
                if (!buildObject->outboundTags.empty()) {
                    Target target;
                    target.coreConfig = QJsonObject2QString(buildObject->coreConfig, false);
                    target.xrayConfig = buildObject->isXrayNeeded ? QJsonObject2QString(buildObject->xrayConfig, false) : "";
                    target.xrayFullConfigs = buildObject->xrayFullConfigs;
                    target.outboundTags = buildObject->outboundTags;
                    target.tag2entID = buildObject->tag2entID;
                    target.xrayDnsStrategy = buildObject->xrayDnsStrategy;
                    target.onResult = onResult;
                    probe(target);
                }
                batchDone.acquire(testCount);

                MW_show_log(isUrl ? "URL test for batch done." : "Exit IP test for batch done.");
                runOnUiThread([=, this] {
                    observer_->testedProfilesChanged(ids);
                });
            };

            QHash<int, QList<int>> tested;
            for (int i = 0; i < profileIDs.length(); i += kTestRunnerBatchSize) {
                if (stopRequested_.load()) break;
                const auto profileIDsSlice = profileIDs.mid(i, kTestRunnerBatchSize);
                auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(profileIDsSlice);
                for (const auto& profile : profiles) tested[profile->gid] << profile->id;
                runBatch(profiles, profileIDsSlice);
            }
            // Results are set on the main thread, and everything below reads them.
            testRunnerAwaitResults();
            // Once the run is over, so rows do not jump while results stream in.
            Configs::RepositionLater(tested, Configs::SortInput::TestResult);

            const int inconclusive = inconclusive_.exchange(0);

            // Read before unlocking: the next session resets stopRequested_. A hook means ranking, which never deletes members.
            QList<int> clearable;
            if (isUrl && !onResult && !stopRequested_.load()) {
                for (auto it = tested.cbegin(); it != tested.cend(); ++it) {
                    const auto group = Configs::dataManager->groupsRepo->GetGroup(it.key());
                    if (group == nullptr || !group->auto_clear_unavailable) continue;
                    clearable << it.value();
                }
            }

            observer_->testPanelsCleared();
            observer_->testViewChanged(true);
            session_.unlock();
            finish();

            // Never with an empty list, which RemoveUnavailable reads as the whole current group.
            if (!clearable.isEmpty()) {
                MW_show_log("URL test finished, clearing unavailable profiles...");
                runOnUiThread([clearable] {
                    ProfileService::RemoveUnavailable(clearable);
                });
            }
            if (inconclusive > 0) {
                MW_show_log(MainWindowTr::tr("%n profile(s) could not be tested because the network was unavailable.", nullptr, inconclusive));
            }
            MW_show_log(isUrl ? MainWindowTr::tr("URL test finished!") : MainWindowTr::tr("Exit IP test finished!"));
        });
        return true;
    }

    bool TestRunner::runServiceChecks(const QList<int>& requestedIDs, const std::function<void()>& onFinished, bool interactive) {
        return runServiceSession(requestedIDs, false, onFinished, interactive);
    }

    bool TestRunner::runCurrentServiceCheck(const std::function<void()>& onFinished, bool interactive) {
        const auto running = runningSnapshot_();
        if (running == nullptr) {
            if (onFinished) onFinished();
            return false;
        }
        return runServiceSession({running->id}, true, onFinished, interactive);
    }

    bool TestRunner::runServiceSession(const QList<int>& requestedIDs, bool testCurrent, const std::function<void()>& onFinished,
                                       bool interactive) {
        const auto finish = [onFinished] { if (onFinished) onFinished(); };
        const auto profileIDs = testRunnerWithoutAutoSelectors(requestedIDs);
        if (profileIDs.isEmpty()) {
            finish();
            return false;
        }
        // The menus ask first; this also holds for API callers.
        if (profileIDs.size() > kMaxServiceCheckProfiles) {
            MW_show_log(MainWindowTr::tr("A service check can test up to 100 profiles at a time. Select fewer profiles."));
            finish();
            return false;
        }
        if (!session_.tryLock()) {
            reportSessionBusy(interactive);
            finish();
            return false;
        }
        sessionGen_.fetch_add(1);
        stopRequested_.store(false);
        interactive_.store(interactive);

        runOnNewThread([this, profileIDs, testCurrent, finish] {
            observer_->testPanelSeeded(TestKind::Service, profileIDs.size(), profileIDs);
            observer_->testViewChanged(true);

            const auto definitions = std::make_shared<const ServiceChecks::Definitions>(ServiceChecks::Prepare());
            if (!definitions->error.isEmpty()) MW_show_log(MainWindowTr::tr("Service check failed: %1").arg(definitions->error));
            const bool runnable = definitions->error.isEmpty() && !ServiceChecks::Enabled(*definitions).isEmpty();
            if (definitions->error.isEmpty() && !runnable) {
                MW_show_log(MainWindowTr::tr("No services are selected for the service check. Choose them in Basic Settings."));
            }

            if (testCurrent && runnable) {
                Target target;
                target.testCurrent = true;
                target.entID = profileIDs.first();
                runServiceProbe(target, definitions);
                runOnUiThread([=, this] { observer_->testedProfilesChanged(profileIDs); });
            }
            for (int i = 0; !testCurrent && runnable && i < profileIDs.length(); i += kTestRunnerServiceBatchSize) {
                if (stopRequested_.load()) break;
                const auto ids = profileIDs.mid(i, kTestRunnerServiceBatchSize);
                sessionGen_.fetch_add(1);
                const auto buildObject = Configs::BuildTestConfig(Configs::dataManager->profilesRepo->GetProfileBatch(ids));
                if (!buildObject->error.isEmpty()) {
                    MW_show_log(MainWindowTr::tr("Failed to build test config for batch: ") + buildObject->error);
                    continue;
                }
                const int testCount = buildObject->fullConfigs.size() + (buildObject->outboundTags.empty() ? 0 : 1);
                if (testCount == 0) continue;

                QSemaphore batchDone;
                const auto probe = [this, &definitions, &batchDone](const Target& target) {
                    pool_.start([this, target, definitions, &batchDone] {
                        const QSemaphoreReleaser releaser(batchDone);
                        runServiceProbe(target, definitions);
                    });
                };
                for (const auto& entID : buildObject->fullConfigs.keys()) {
                    Target target;
                    target.coreConfig = buildObject->fullConfigs[entID];
                    target.useDefaultOutbound = true;
                    target.entID = entID;
                    probe(target);
                }
                if (!buildObject->outboundTags.empty()) {
                    Target target;
                    target.coreConfig = QJsonObject2QString(buildObject->coreConfig, false);
                    target.xrayConfig = buildObject->isXrayNeeded ? QJsonObject2QString(buildObject->xrayConfig, false) : "";
                    target.xrayFullConfigs = buildObject->xrayFullConfigs;
                    target.outboundTags = buildObject->outboundTags;
                    target.tag2entID = buildObject->tag2entID;
                    target.xrayDnsStrategy = buildObject->xrayDnsStrategy;
                    probe(target);
                }
                batchDone.acquire(testCount);
                runOnUiThread([=, this] { observer_->testedProfilesChanged(ids); });
            }

            // Results are set on the main thread, and onFinished may read them.
            testRunnerAwaitResults();
            observer_->testPanelsCleared();
            observer_->testViewChanged(true);
            session_.unlock();
            finish();
            if (runnable) MW_show_log(MainWindowTr::tr("Service check finished!"));
        });
        return true;
    }

    bool TestRunner::runSpeedTests(const QList<int>& requestedIDs, bool testCurrent,
                                   const std::function<void()>& onFinished, bool interactive)
    {
        const auto finish = [onFinished] { if (onFinished) onFinished(); };
        // A live-connection test stays valid for a selector: it measures whichever member carries traffic.
        const auto profileIDs = testCurrent ? requestedIDs : testRunnerWithoutAutoSelectors(requestedIDs);
        if (profileIDs.isEmpty() && !testCurrent) {
            finish();
            return false;
        }
        if (!session_.tryLock()) {
            reportSessionBusy(interactive);
            finish();
            return false;
        }
        sessionGen_.fetch_add(1);
        stopRequested_.store(false);
        interactive_.store(interactive);

        testingCurrent_.store(testCurrent);

        runOnNewThread([this, profileIDs, testCurrent, finish]() {
            { QMutexLocker lk(&creditMu_); credited_.clear(); }
            QHash<int, QList<int>> tested;
            if (!testCurrent)
            {
                observer_->testPanelSeeded(TestKind::Speed, profileIDs.size(), profileIDs);
                observer_->testViewChanged(true);
                auto runBatch = [this](const QList<std::shared_ptr<Configs::Profile>>& profileSlice) {
                    auto buildObject = Configs::BuildTestConfig(profileSlice);
                    if (!buildObject->error.isEmpty()) {
                        MW_show_log(MainWindowTr::tr("Failed to build batch test config: ") + buildObject->error);
                        return;
                    }

                    for (auto it = buildObject->fullConfigs.cbegin(); it != buildObject->fullConfigs.cend(); ++it) {
                        Target target;
                        target.coreConfig = it.value();
                        target.useDefaultOutbound = true;
                        target.entID = it.key();
                        runSpeedProbe(target);
                    }

                    if (!buildObject->outboundTags.empty()) {
                        Target target;
                        target.coreConfig = QJsonObject2QString(buildObject->coreConfig, false);
                        target.xrayConfig = buildObject->isXrayNeeded ? QJsonObject2QString(buildObject->xrayConfig, true) : "";
                        target.xrayFullConfigs = buildObject->xrayFullConfigs;
                        target.outboundTags = buildObject->outboundTags;
                        target.tag2entID = buildObject->tag2entID;
                        target.xrayDnsStrategy = buildObject->xrayDnsStrategy;
                        runSpeedProbe(target);
                    }
                };
                for (const int id : profileIDs) {
                    if (stopRequested_.load()) break;
                    const auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch({id});
                    for (const auto& profile : profiles) tested[profile->gid] << profile->id;
                    runBatch(profiles);
                }
            } else
            {
                const int currentId = runningProfileId();
                observer_->testPanelSeeded(TestKind::Speed, 1, currentId >= 0 ? QList<int>{currentId} : QList<int>{});
                observer_->testViewChanged(true);
                Target target;
                target.testCurrent = true;
                runSpeedProbe(target);
                testingCurrent_.store(false);
                if (const auto running = runningSnapshot_()) tested[running->gid] << running->id;
            }
            // Results are set on the main thread, and everything below reads them.
            testRunnerAwaitResults();
            // A speed test also credits the traffic it moved.
            Configs::RepositionLater(tested, Configs::SortInput::TestResult);
            Configs::RepositionLater(tested, Configs::SortInput::Traffic);
            observer_->testPanelsCleared();
            observer_->testViewChanged(true);
            session_.unlock();
            finish();
            runOnUiThread([=,this]{
                observer_->testedProfilesChanged(profileIDs);
                MW_show_log(MainWindowTr::tr("Speed test finished!"));
            });
        });
        return true;
    }

    void TestRunner::creditTraffic(const std::shared_ptr<Configs::Profile>& profile, const QString& tag, qint64 curUp, qint64 curDown)
    {
        if (profile == nullptr || tag.isEmpty()) return;
        QMutexLocker lk(&creditMu_);
        auto& base = credited_[tag];
        const qint64 dUp = curUp >= base.first ? curUp - base.first : curUp;
        const qint64 dDown = curDown >= base.second ? curDown - base.second : curDown;
        base = qMakePair(curUp, curDown);
        if (dUp <= 0 && dDown <= 0) return;

        Stats::trafficStatsManager->AddConfigDelta(profile->id, dUp, dDown);
        Stats::trafficStatsManager->AddAppDelta(Stats::SPEEDTEST_APP_NAME, "", dUp, dDown);

        profile->traffic_uplink += dUp;
        profile->traffic_downlink += dDown;
        Configs::dataManager->profilesRepo->SaveTraffic(profile);
    }

    void TestRunner::pollSpeedTest(const QMap<QString, int>& tag2entID, bool testCurrent, quint64 gen)
    {
        bool ok = false;
        const auto res = API::defaultClient->QueryCurrentSpeedTests(&ok);
        if (staleGen(gen)) return;
        if (!ok || !res.is_running.value())
        {
            return;
        }
        const libcore::SpeedTestResult result = res.result.value();
        const auto tag = QString::fromStdString(result.outbound_tag.value());
        // value(tag, -1), not operator[]: a const QMap yields 0 for a missing key. The repo's object by id, as the result
        // loop uses: once an edit replaces it, the running snapshot is a stale twin.
        auto profile = Configs::dataManager->profilesRepo->GetProfile(testCurrent ? runningProfileId() : tag2entID.value(tag, -1));
        if (profile == nullptr)
        {
            return;
        }
        creditTraffic(profile, tag, result.ul_bytes.value(), result.dl_bytes.value());
        runOnUiThread([this, profile, result, gen]
        {
            if (staleGen(gen)) return;
            observer_->speedTestProgress(profile->outbound->name, result);
            observer_->testViewChanged(false);

            if (result.error.value().empty() && !result.cancelled.value())
            {
                if (!speedBefore_.contains(profile->id)) speedBefore_.insert(profile->id, {profile->dl_speed, profile->ul_speed});
                if (!result.dl_speed.value().empty()) profile->dl_speed = QString::fromStdString(result.dl_speed.value());
                if (!result.ul_speed.value().empty()) profile->ul_speed = QString::fromStdString(result.ul_speed.value());
                observer_->testedProfilesChanged({profile->id});
            }
        });
    }

    void TestRunner::runSpeedProbe(const Target& target)
    {
        if (stopRequested_.load()) {
            MW_show_log(MainWindowTr::tr("Profile speed test aborted"));
            return;
        }

        sessionGen_.fetch_add(1);

        auto speedtestConf = Configs::dataManager->settingsRepo->speed_test_mode;
        // Also catches the removed "Only Country" mode (4).
        if (speedtestConf < Configs::TestConfig::FULL || speedtestConf > Configs::TestConfig::SIMPLEDL) speedtestConf = Configs::TestConfig::FULL;
        libcore::SpeedTestRequest req;
        testRunnerFillCommonTestReq(req, target);
        req.test_download = speedtestConf == Configs::TestConfig::FULL || speedtestConf == Configs::TestConfig::DL;
        req.test_upload = speedtestConf == Configs::TestConfig::FULL || speedtestConf == Configs::TestConfig::UL;
        req.simple_download = speedtestConf == Configs::TestConfig::SIMPLEDL;
        req.simple_download_addr = Configs::dataManager->settingsRepo->simple_dl_url.toStdString();
        req.test_current = target.testCurrent;
        req.timeout_ms = Configs::dataManager->settingsRepo->speed_test_timeout_ms;

        observer_->testProgressed();
        observer_->testViewChanged(false);

        const int contextID = target.testCurrent ? runningProfileId() : target.entID;

        bool rpcOK = false;
        QString coreError;
        libcore::SpeedTestResponse result;
        {
            TestRunnerResultPoller poller([this, gen = sessionGen_.load(), tag2entID = target.tag2entID, testCurrent = target.testCurrent] {
                if (staleGen(gen)) return;
                pollSpeedTest(tag2entID, testCurrent, gen);
            }, kTestRunnerSpeedPollIntervalMs);

            result = API::defaultClient->SpeedTest(&rpcOK, req, &coreError, testRunnerSpeedRpcTimeoutMs(req));
        }
        // A tick still in flight must not show partial speeds after the result.
        sessionGen_.fetch_add(1);

        if (!rpcOK) {
            testRunnerStopSpeedTest();
            restorePolledSpeeds({});
            reportProbeFailure(MainWindowTr::tr("Speed test failed: %1"), coreError, contextID);
            return;
        }

        TestRunnerUpdates updates;
        QSet<int> stored;
        for (const auto& res : result.results) {
            const int entid = target.testCurrent
                                  ? runningProfileId()
                                  : testRunnerResolveEntID(target.tag2entID, res.outbound_tag.value(), target.entID);
            if (entid == -1) {
                MW_show_log(MainWindowTr::tr("Something is very wrong, the subject ent cannot be found!"));
                continue;
            }

            auto ent = Configs::dataManager->profilesRepo->GetProfile(entid);
            if (ent == nullptr) {
                MW_show_log(MainWindowTr::tr("Profile manager data is corrupted, try again."));
                continue;
            }

            creditTraffic(ent, QString::fromStdString(res.outbound_tag.value()),
                          res.ul_bytes.value(), res.dl_bytes.value());

            const auto error = QString::fromStdString(res.error.value());
            // A stop during the server lookup comes back with neither an error nor a speed.
            const bool measured = !error.isEmpty() || !res.dl_speed.value().empty() || !res.ul_speed.value().empty();
            if (res.cancelled.value() || testRunnerIsTestAborted(error) || !measured) continue;
            QString dl = QStringLiteral("N/A");
            QString ul = dl;
            if (error.isEmpty()) {
                dl = QString::fromStdString(res.dl_speed.value());
                ul = QString::fromStdString(res.ul_speed.value());
            } else {
                MW_show_log(MainWindowTr::tr("[%1] speed test error: %2").arg(ent->outbound->DisplayTypeAndName(), error));
            }
            updates.append({ent, [dl, ul](Configs::Profile& p) {
                p.dl_speed = dl;
                p.ul_speed = ul;
            }});
            stored.insert(entid);
        }
        testRunnerApply(observer_, std::move(updates), Configs::ProfileColumn::Speed);
        restorePolledSpeeds(stored);
    }

    void TestRunner::restorePolledSpeeds(const QSet<int>& stored)
    {
        // Queued behind the poll's own updates, so nothing it showed outlives this.
        runOnUiThread([this, stored] {
            QList<int> restored;
            for (auto it = speedBefore_.cbegin(); it != speedBefore_.cend(); ++it) {
                if (stored.contains(it.key())) continue;
                if (const auto ent = Configs::dataManager->profilesRepo->GetProfile(it.key())) {
                    ent->dl_speed = it.value().first;
                    ent->ul_speed = it.value().second;
                    restored << it.key();
                }
            }
            speedBefore_.clear();
            if (!restored.isEmpty()) observer_->testedProfilesChanged(restored);
        });
    }
} // namespace Engine
