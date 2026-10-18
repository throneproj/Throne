#include "include/engine/tests/TestRunner.h"

#include "include/api/RPC.h"
#include "include/configs/generate.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EnginePolicy.h"
#include "include/engine/EngineTr.h"
#include "include/engine/services/AssetUpdates.h"
#include "include/engine/services/ProfileService.h"
#include "include/engine/tests/TestObserver.h"
#include "include/global/Configs.hpp"
#include "include/global/CountryHelper.hpp"
#include "include/stats/traffic/TrafficStatsManager.hpp"

#include <QCoreApplication>
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
            return ent != nullptr && (ent->type == "openvpn" || ent->type == "openconnect");
        }

        // Mirror the core's URLTestTimeout, TunnelStartupTimeout, TunnelHandshakeTimeout and normalizeConcurrency.
        constexpr int kTestRunnerCoreDefaultTimeoutMs = 3000;
        constexpr int kTestRunnerTunnelStartupMs = 5000;
        constexpr int kTestRunnerTunnelHandshakeMs = 10000;
        constexpr int kTestRunnerCoreMaxConcurrency = 100;
        constexpr int kTestRunnerRpcSlackMs = 30000;

        // Test/IPTest answer only once the whole batch is done, so the deadline covers its worst case.
        int testRunnerBatchRpcTimeoutMs(qsizetype tagCount, int requestsPerTag) {
            const auto& settings = Configs::dataManager->settingsRepo;
            int concurrency = settings->test_concurrent;
            if (concurrency <= 0 || concurrency >= 500) concurrency = kTestRunnerCoreMaxConcurrency;
            const qint64 timeoutMs = settings->url_test_timeout_ms > 0 ? settings->url_test_timeout_ms : kTestRunnerCoreDefaultTimeoutMs;
            const qint64 rounds = (qMax<qsizetype>(tagCount, 1) + concurrency - 1) / concurrency;
            const qint64 total = rounds * (kTestRunnerTunnelHandshakeMs + requestsPerTag * timeoutMs + kTestRunnerTunnelStartupMs) + kTestRunnerRpcSlackMs;
            return static_cast<int>(qMin<qint64>(total, std::numeric_limits<int>::max()));
        }

        int testRunnerResolveEntID(const QMap<QString, int>& tag2entID, const std::string& tag, int fallback) {
            if (tag2entID.isEmpty()) return fallback;
            return tag2entID.value(QString::fromStdString(tag), -1);
        }

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
            void testPanelSeeded(TestKind, int) override {}
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

    void TestRunner::applyUrlResult(const std::shared_ptr<Configs::Profile>& ent, const libcore::URLTestResp& res,
                                    const QHash<QString, bool>* vpnConnected) {
        const auto error = QString::fromStdString(res.error.value());
        if (error.isEmpty()) {
            ent->SetLatency(res.latency_ms.value());
        } else if (testRunnerIsTestAborted(error)) {
            ent->SetLatency(0);
        } else if (vpnConnected != nullptr && testRunnerIsVpnProfile(ent)
                   && vpnConnected->value(QString::fromStdString(res.outbound_tag.value()), false)) {
            ent->SetLatency(Configs::kLatencyConnectOnly);
        } else {
            ent->SetLatency(-1);
            MW_show_log(MainWindowTr::tr("[%1] test error: %2").arg(ent->outbound->DisplayTypeAndName(), error));
        }
        Configs::dataManager->profilesRepo->Save(ent);
    }

    void TestRunner::applyIpResult(const std::shared_ptr<Configs::Profile>& ent, const libcore::IPTestRes& res) {
        const auto error = QString::fromStdString(res.error.value());
        if (error.isEmpty()) {
            ent->ip_out = QString::fromStdString(res.ip.value());
            ent->test_country = QString::fromStdString(res.country_code.value());
        } else {
            if (!testRunnerIsTestAborted(error)) {
                MW_show_log(MainWindowTr::tr("[%1] IP test error: %2").arg(ent->outbound->DisplayTypeAndName(), error));
            }
            ent->ip_out.clear();
            ent->test_country.clear();
        }
        Configs::dataManager->profilesRepo->Save(ent);
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

        bool rpcOK = false;
        QString coreError;
        libcore::TestResp result;
        {
            TestRunnerResultPoller poller([this, gen = sessionGen_.load(), tag2entID = target.tag2entID] {
                if (staleGen(gen)) return;
                bool ok = false;
                const auto resp = API::defaultClient->QueryURLTest(&ok);
                // Checked again: this poll can sit in the RPC while its batch ends and the tags are reused.
                if (staleGen(gen)) return;
                if (!ok || resp.results.empty()) return;

                QList<int> updated;
                for (const auto& res : resp.results) {
                    observer_->testProgressed();
                    observer_->testViewChanged(false);
                    const int entid = testRunnerResolveEntID(tag2entID, res.outbound_tag.value(), -1);
                    if (entid == -1) continue;
                    auto ent = Configs::dataManager->profilesRepo->GetProfile(entid);
                    if (ent == nullptr) continue;
                    applyUrlResult(ent, res);
                    updated << entid;
                }
                if (updated.isEmpty()) return;
                observer_->testViewChanged(true);
                runOnUiThread([=, this] { observer_->testedProfilesChanged(updated); });
            }, kTestRunnerLatencyPollIntervalMs);

            result = API::defaultClient->Test(&rpcOK, req, &coreError, testRunnerBatchRpcTimeoutMs(target.outboundTags.size(), 2));
        }

        if (!rpcOK || result.results.empty()) {
            if (!rpcOK) AssetUpdates::HandleXrayGeoError(policy_, coreError, contextName(target.entID), interactive_.load());
            return;
        }

        QHash<QString, bool> vpnConnected;
        for (const auto& st : result.vpn_status) {
            vpnConnected.insert(QString::fromStdString(st.tag.value()), st.connected.value());
        }

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
            applyUrlResult(ent, res, &vpnConnected);
        }
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

        bool rpcOK = false;
        QString coreError;
        libcore::IPTestResp result;
        {
            TestRunnerResultPoller poller([this, gen = sessionGen_.load(), tag2entID = target.tag2entID] {
                if (staleGen(gen)) return;
                bool ok = false;
                const auto resp = API::defaultClient->QueryIPTest(&ok);
                if (staleGen(gen)) return;
                if (!ok || resp.results.empty()) return;

                QList<int> updated;
                for (const auto& res : resp.results) {
                    observer_->testProgressed();
                    observer_->testViewChanged(false);
                    const int entid = testRunnerResolveEntID(tag2entID, res.outbound_tag.value(), -1);
                    if (entid == -1) continue;
                    auto ent = Configs::dataManager->profilesRepo->GetProfile(entid);
                    if (ent == nullptr) continue;
                    applyIpResult(ent, res);
                    updated << entid;
                }
                if (updated.isEmpty()) return;
                observer_->testViewChanged(true);
                runOnUiThread([=, this] { observer_->testedProfilesChanged(updated); });
            }, kTestRunnerLatencyPollIntervalMs);

            result = API::defaultClient->IPTest(&rpcOK, req, &coreError, testRunnerBatchRpcTimeoutMs(target.outboundTags.size(), 1));
        }

        if (!rpcOK || result.results.empty()) {
            if (!rpcOK) AssetUpdates::HandleXrayGeoError(policy_, coreError, contextName(target.entID), interactive_.load());
            return;
        }

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
            applyIpResult(ent, res);
        }
    }

    bool TestRunner::runUrlTests(const QList<int>& profileIDs, const std::function<void()>& onFinished, bool interactive) {
        return runLatencyGroup(LatencyKind::Url, profileIDs, onFinished, false, interactive);
    }

    void TestRunner::queueUrlTests(const QList<int>& profileIDs, const std::function<void()>& onFinished, bool interactive) {
        runOnNewThread([=, this] { runLatencyGroup(LatencyKind::Url, profileIDs, onFinished, true, interactive); });
    }

    bool TestRunner::runIpTests(const QList<int>& profileIDs, const std::function<void()>& onFinished, bool interactive) {
        return runLatencyGroup(LatencyKind::Ip, profileIDs, onFinished, false, interactive);
    }

    bool TestRunner::runLatencyGroup(LatencyKind kind, const QList<int>& requestedIDs,
                                     const std::function<void()>& onFinished, bool waitForSession, bool interactive) {
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
            const auto text = isUrl
                ? MainWindowTr::tr("The last url test did not exit completely, please wait. If it persists, please restart the program.")
                : MainWindowTr::tr("The last test did not exit completely, please wait. If it persists, please restart the program.");
            // Auto-selector ranking calls in from a worker thread, where no widget may be created.
            if (interactive && QThread::currentThread() == QCoreApplication::instance()->thread()) policy_->testSessionBusy(text);
            else MW_show_log(text);
            finish();
            return false;
        }
        sessionGen_.fetch_add(1);
        // Reset here, not on the worker: a stop() that lands before the worker runs must still count.
        stopRequested_.store(false);
        interactive_.store(interactive);

        runOnNewThread([this, profileIDs, panelKind, isUrl, finish]() {
            observer_->testPanelSeeded(panelKind, profileIDs.size());
            observer_->testViewChanged(true);

            auto runBatch = [this, isUrl](const QList<std::shared_ptr<Configs::Profile>>& profileSlice, const QList<int>& ids) {
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

                MW_show_log(isUrl ? "URL test for batch done." : "IP test for batch done.");
                runOnUiThread([=, this] {
                    observer_->testedProfilesChanged(ids);
                });
            };

            std::shared_ptr<Configs::Group> currentGroup;
            QHash<int, QList<int>> tested;
            for (int i = 0; i < profileIDs.length(); i += kTestRunnerBatchSize) {
                if (stopRequested_.load()) break;
                const auto profileIDsSlice = profileIDs.mid(i, kTestRunnerBatchSize);
                auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(profileIDsSlice);
                if (isUrl && !currentGroup && !profiles.isEmpty()) {
                    currentGroup = Configs::dataManager->groupsRepo->GetGroup(profiles[0]->gid);
                }
                for (const auto& profile : profiles) tested[profile->gid] << profile->id;
                runBatch(profiles, profileIDsSlice);
            }
            // Once the run is over, so rows do not jump while results stream in.
            Configs::RepositionLater(tested, Configs::SortInput::TestResult);

            observer_->testPanelsCleared();
            observer_->testViewChanged(true);
            session_.unlock();
            finish();

            if (currentGroup != nullptr && currentGroup->auto_clear_unavailable) {
                MW_show_log("URL test finished, clearing unavailable profiles...");
                runOnUiThread([=] {
                   ProfileService::RemoveUnavailable(profileIDs);
                });
            }
            MW_show_log(isUrl ? MainWindowTr::tr("URL test finished!") : MainWindowTr::tr("IP test finished!"));
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
            const auto text = MainWindowTr::tr("The last test did not finish completely, please wait. If it persists, please restart the program.");
            if (interactive) policy_->testSessionBusy(text);
            else MW_show_log(text);
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
                observer_->testPanelSeeded(TestKind::Speed, profileIDs.size());
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
                const int stepSize = Configs::dataManager->settingsRepo->speed_test_mode == Configs::TestConfig::COUNTRY ? kTestRunnerBatchSize : 1;
                for (int i = 0; i < profileIDs.length(); i += stepSize) {
                    if (stopRequested_.load()) break;
                    const auto profileIDsSlice = profileIDs.mid(i, stepSize);
                    auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(profileIDsSlice);
                    for (const auto& profile : profiles) tested[profile->gid] << profile->id;
                    runBatch(profiles);
                }
            } else
            {
                observer_->testPanelSeeded(TestKind::Speed, 1);
                Target target;
                target.testCurrent = true;
                runSpeedProbe(target);
                testingCurrent_.store(false);
                if (const auto running = runningSnapshot_()) tested[running->gid] << running->id;
            }
            // A speed test also credits the traffic it moved.
            Configs::RepositionLater(tested, Configs::SortInput::TestResult);
            Configs::RepositionLater(tested, Configs::SortInput::Traffic);
            observer_->testPanelsCleared();
            observer_->testViewChanged(true);
            session_.unlock();
            finish();
            runOnUiThread([=,this]{
                observer_->testedProfilesChanged(profileIDs);
                MW_show_log(MainWindowTr::tr("Speedtest finished!"));
            });
        });
        return true;
    }

    void TestRunner::creditTraffic(const std::shared_ptr<Configs::Profile>& profile, const QString& tag, qint64 curUp, qint64 curDown)
    {
        if (profile == nullptr || tag.isEmpty()) return;
        if (Configs::dataManager->settingsRepo->disable_traffic_stats) return;
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
        // value(tag, -1), not operator[]: a const QMap yields 0 for a missing key.
        auto profile = testCurrent ? runningSnapshot_()
                                   : Configs::dataManager->profilesRepo->GetProfile(tag2entID.value(tag, -1));
        if (profile == nullptr)
        {
            return;
        }
        creditTraffic(profile, tag, result.ul_bytes.value(), result.dl_bytes.value());
        runOnUiThread([this, profile, result]
        {
            observer_->speedTestProgress(profile->outbound->name, result);
            observer_->testViewChanged(false);

            if (result.error.value().empty() && !result.cancelled.value())
            {
                if (!result.dl_speed.value().empty()) profile->dl_speed = QString::fromStdString(result.dl_speed.value());
                if (!result.ul_speed.value().empty()) profile->ul_speed = QString::fromStdString(result.ul_speed.value());
                if (profile->latency <= 0 && result.latency.value() > 0) profile->SetLatency(result.latency.value());
                if (!result.server_country.value().empty()) profile->test_country = CountryNameToCode(QString::fromStdString(result.server_country.value()));
                observer_->testedProfilesChanged({profile->id});
            }
        });
    }

    void TestRunner::pollCountryTest(const QMap<QString, int>& tag2entID, bool testCurrent, quint64 gen)
    {
        bool ok = false;
        const auto res = API::defaultClient->QueryCountryTestResults(&ok);
        if (staleGen(gen)) return;
        if (!ok || res.results.empty())
        {
            return;
        }
        for (const auto& result : res.results)
        {
            observer_->testProgressed();
            observer_->testViewChanged(false);
            const auto tag = QString::fromStdString(result.outbound_tag.value());
            auto profile = testCurrent ? runningSnapshot_()
                                       : Configs::dataManager->profilesRepo->GetProfile(tag2entID.value(tag, -1));
            if (profile == nullptr)
            {
                continue;
            }
            runOnUiThread([this, profile, result]
            {
                if (result.error.value().empty() && !result.cancelled.value())
                {
                    if (profile->latency <= 0 && result.latency.value() > 0) profile->SetLatency(result.latency.value());
                    if (!result.server_country.value().empty()) profile->test_country = CountryNameToCode(QString::fromStdString(result.server_country.value()));
                    observer_->testedProfilesChanged({profile->id});
                }
            });
        }
        observer_->testViewChanged(true);
    }

    void TestRunner::runSpeedProbe(const Target& target)
    {
        if (stopRequested_.load()) {
            MW_show_log(MainWindowTr::tr("Profile speed test aborted"));
            return;
        }

        sessionGen_.fetch_add(1);

        const auto speedtestConf = Configs::dataManager->settingsRepo->speed_test_mode;
        libcore::SpeedTestRequest req;
        testRunnerFillCommonTestReq(req, target);
        req.test_download = speedtestConf == Configs::TestConfig::FULL || speedtestConf == Configs::TestConfig::DL;
        req.test_upload = speedtestConf == Configs::TestConfig::FULL || speedtestConf == Configs::TestConfig::UL;
        req.simple_download = speedtestConf == Configs::TestConfig::SIMPLEDL;
        req.simple_download_addr = Configs::dataManager->settingsRepo->simple_dl_url.toStdString();
        req.test_current = target.testCurrent;
        req.timeout_ms = Configs::dataManager->settingsRepo->speed_test_timeout_ms;
        req.only_country = speedtestConf == Configs::TestConfig::COUNTRY;
        req.country_concurrency = Configs::dataManager->settingsRepo->test_concurrent;

        if (speedtestConf != Configs::TestConfig::COUNTRY) {
            observer_->testProgressed();
            observer_->testViewChanged(false);
        }

        const int contextID = target.testCurrent ? runningProfileId() : target.entID;

        bool rpcOK = false;
        QString coreError;
        libcore::SpeedTestResponse result;
        {
            TestRunnerResultPoller poller([this, gen = sessionGen_.load(), tag2entID = target.tag2entID, testCurrent = target.testCurrent, speedtestConf] {
                if (staleGen(gen)) return;
                if (speedtestConf == Configs::TestConfig::COUNTRY) pollCountryTest(tag2entID, testCurrent, gen);
                else pollSpeedTest(tag2entID, testCurrent, gen);
            }, kTestRunnerSpeedPollIntervalMs);

            result = API::defaultClient->SpeedTest(&rpcOK, req, &coreError);
        }

        if (!rpcOK || result.results.empty()) {
            if (!rpcOK) AssetUpdates::HandleXrayGeoError(policy_, coreError, contextName(contextID), interactive_.load());
            return;
        }

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

            if (res.cancelled.value()) continue;

            const auto error = QString::fromStdString(res.error.value());
            if (error.isEmpty()) {
                ent->dl_speed = QString::fromStdString(res.dl_speed.value());
                ent->ul_speed = QString::fromStdString(res.ul_speed.value());
                if (ent->latency <= 0 && res.latency.value() > 0) ent->SetLatency(res.latency.value());
                if (!res.server_country.value().empty()) ent->test_country = CountryNameToCode(QString::fromStdString(res.server_country.value()));
            } else {
                ent->dl_speed = "N/A";
                ent->ul_speed = "N/A";
                ent->SetLatency(-1);
                ent->test_country = "";
                MW_show_log(MainWindowTr::tr("[%1] speed test error: %2").arg(ent->outbound->DisplayTypeAndName(), error));
            }
            Configs::dataManager->profilesRepo->Save(ent);
        }
    }
} // namespace Engine
