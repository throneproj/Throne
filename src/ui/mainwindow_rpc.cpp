#include "include/ui/mainwindow.h"

#include "include/stats/traffic/TrafficLooper.hpp"
#include "include/stats/traffic/TrafficStatsManager.hpp"
#include "include/api/RPC.h"
#include "include/ui/utils//MessageBoxTimer.h"
#include "3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.hpp"

#include <QInputDialog>
#include <QPushButton>
#include <QDesktopServices>
#include <QMessageBox>
#include <QJsonDocument>
#include <QFile>
#include <QRegularExpression>

#include "include/configs/generate.h"
#include "include/configs/common/xrayStreamSetting.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"

#include "include/sys/Process.hpp"

#include <algorithm>

#include <memory>

// rpc

using namespace API;

void MainWindow::setup_rpc(QLocalSocket *socket) {
    // The Client is constructed once at startup and never recreated; on core
    // restart we only swap the underlying connection, so worker threads holding
    // `defaultClient` never touch freed memory.
    defaultClient->Reconnect(socket);

    // Loopers run for the lifetime of the app, start only once
    if (!rpc_started) {
        rpc_started = true;
        runOnNewThread([=] { Stats::trafficLooper->Loop(); });
        runOnNewThread([=] { Stats::connection_lister->Loop(); });
    }
}

void MainWindow::runURLTest(const QString& config, const QString& xrayConfig, const QStringList& xrayFullConfigs, bool useDefault, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID) {
    if (stopSpeedtest.load()) {
        MW_show_log(tr("Profile test aborted"));
        return;
    }

    libcore::TestReq req;
    for (const auto &item: outboundTags) {
        req.outbound_tags.push_back(item.toStdString());
    }
    req.config = config.toStdString();
    req.url = Configs::dataManager->settingsRepo->test_latency_url.toStdString();
    req.use_default_outbound = useDefault;
    req.max_concurrency = Configs::dataManager->settingsRepo->test_concurrent;
    req.test_timeout_ms = Configs::dataManager->settingsRepo->url_test_timeout_ms;
    req.xray_config = xrayConfig.toStdString();
    req.need_xray = !xrayConfig.isEmpty();
    for (const auto &xc : xrayFullConfigs) req.xray_full_configs.push_back(xc.toStdString());

    auto done = new QMutex;
    done->lock();
    runOnNewThread([=,this]
    {
        bool ok;
        while (true)
        {
            QThread::msleep(200);
            if (done->try_lock()) break;
            auto resp = defaultClient->QueryURLTest(&ok);
            if (!ok || resp.results.empty())
            {
                continue;
            }

            bool needRefresh = false;
            QList<int> profileIDs;
            for (const auto& res : resp.results)
            {
                dataViewHtmlGenerator_.addTestProgress();
                UpdateDataView();
                int entid = -1;
                if (!tag2entID.empty()) {
                    entid = tag2entID.count(QString::fromStdString(res.outbound_tag.value())) == 0 ? -1 : tag2entID[QString::fromStdString(res.outbound_tag.value())];
                }
                if (entid == -1) {
                    continue;
                }
                profileIDs << entID;
                auto ent = Configs::dataManager->profilesRepo->GetProfile(entid);
                if (ent == nullptr) {
                    continue;
                }
                if (res.error.value().empty()) {
                ent->latency = res.latency_ms.value();
                } else {
                    if (QString::fromStdString(res.error.value()).contains("test aborted") ||
                        QString::fromStdString(res.error.value()).contains("context canceled")) ent->latency=0;
                    else {
                        ent->latency = -1;
                        MW_show_log(tr("[%1] test error: %2").arg(ent->outbound->DisplayTypeAndName(), QString::fromStdString(res.error.value())));
                    }
                }
                Configs::dataManager->profilesRepo->Save(ent);
                needRefresh = true;
            }
            if (needRefresh)
            {
                UpdateDataView(true);
                runOnUiThread([=,this]{
                    refresh_proxy_list(profileIDs);
                });
            }
        }
        done->unlock();
        delete done;
    });
    bool rpcOK;
    QString coreError;
    auto result = defaultClient->Test(&rpcOK, req, &coreError);
    done->unlock();
    //
    if (!rpcOK || result.results.empty()) {
        // A failed Test RPC (e.g. an Xray full config that needs geoip.dat /
        // geosite.dat) never yields per-result errors, so inspect the RPC error here
        // to offer the geo-asset download — the same flow profile start uses.
        if (!rpcOK) {
            QString ctxName = tr("a tested profile");
            if (entID != -1) {
                if (auto e = Configs::dataManager->profilesRepo->GetProfile(entID)) ctxName = e->outbound->DisplayTypeAndName();
            }
            handleXrayGeoAssetError(coreError, ctxName);
        }
        return;
    }

    for (const auto &res: result.results) {
        if (!tag2entID.empty()) {
            entID = tag2entID.count(QString::fromStdString(res.outbound_tag.value())) == 0 ? -1 : tag2entID[QString::fromStdString(res.outbound_tag.value())];
        }
        if (entID == -1) {
            MW_show_log(tr("Something is very wrong, the subject ent cannot be found!"));
            continue;
        }

        auto ent = Configs::dataManager->profilesRepo->GetProfile(entID);
        if (ent == nullptr) {
            MW_show_log(tr("Profile manager data is corrupted, try again."));
            continue;
        }

        if (res.error.value().empty()) {
            ent->latency = res.latency_ms.value();
        } else {
            if (QString::fromStdString(res.error.value()).contains("test aborted") ||
                QString::fromStdString(res.error.value()).contains("context canceled")) ent->latency=0;
            else {
                ent->latency = -1;
                MW_show_log(tr("[%1] test error: %2").arg(ent->outbound->DisplayTypeAndName(), QString::fromStdString(res.error.value())));
            }
        }
        Configs::dataManager->profilesRepo->Save(ent);
    }
}

void MainWindow::runIPTest(const QString& config, const QString& xrayConfig, const QStringList& xrayFullConfigs, bool useDefault, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID) {
    if (stopSpeedtest.load()) {
        MW_show_log(tr("Profile test aborted"));
        return;
    }

    libcore::IPTestRequest req;
    for (const auto &item: outboundTags) {
        req.outbound_tags.push_back(item.toStdString());
    }
    req.config = config.toStdString();
    req.use_default_outbound = useDefault;
    req.max_concurrency = Configs::dataManager->settingsRepo->test_concurrent;
    req.test_timeout_ms = Configs::dataManager->settingsRepo->url_test_timeout_ms;
    req.xray_config = xrayConfig.toStdString();
    req.need_xray = !xrayConfig.isEmpty();
    for (const auto &xc : xrayFullConfigs) req.xray_full_configs.push_back(xc.toStdString());

    auto done = new QMutex;
    done->lock();
    runOnNewThread([=,this]
    {
        bool ok;
        while (true)
        {
            QThread::msleep(200);
            if (done->try_lock()) break;
            auto resp = defaultClient->QueryIPTest(&ok);
            if (!ok || resp.results.empty())
            {
                continue;
            }

            bool needRefresh = false;
            QList<int> profileIDs;
            for (const auto& res : resp.results)
            {
                dataViewHtmlGenerator_.addTestProgress();
                UpdateDataView();
                int entid = -1;
                if (!tag2entID.empty()) {
                    entid = tag2entID.count(QString::fromStdString(res.outbound_tag.value())) == 0 ? -1 : tag2entID[QString::fromStdString(res.outbound_tag.value())];
                }
                if (entid == -1) {
                    continue;
                }
                profileIDs << entid;
                auto ent = Configs::dataManager->profilesRepo->GetProfile(entid);
                if (ent == nullptr) {
                    continue;
                }
                if (res.error.value().empty()) {
                    ent->ip_out = QString::fromStdString(res.ip.value());
                    ent->test_country = QString::fromStdString(res.country_code.value());
                } else {
                    if (!QString::fromStdString(res.error.value()).contains("test aborted") &&
                        !QString::fromStdString(res.error.value()).contains("context canceled")) {
                        MW_show_log(tr("[%1] IP test error: %2").arg(ent->outbound->DisplayTypeAndName(), QString::fromStdString(res.error.value())));
                    }
                    ent->ip_out.clear();
                    ent->test_country.clear();
                }
                Configs::dataManager->profilesRepo->Save(ent);
                needRefresh = true;
            }
            if (needRefresh)
            {
                UpdateDataView(true);
                runOnUiThread([=,this]{
                    refresh_proxy_list(profileIDs);
                });
            }
        }
        done->unlock();
        delete done;
    });
    bool rpcOK;
    QString coreError;
    auto result = defaultClient->IPTest(&rpcOK, req, &coreError);
    done->unlock();
    //
    if (!rpcOK || result.results.empty()) {
        // Detect missing Xray geo assets from a failed IPTest RPC (see runURLTest).
        if (!rpcOK) {
            QString ctxName = tr("a tested profile");
            if (entID != -1) {
                if (auto e = Configs::dataManager->profilesRepo->GetProfile(entID)) ctxName = e->outbound->DisplayTypeAndName();
            }
            handleXrayGeoAssetError(coreError, ctxName);
        }
        return;
    }

    for (const auto &res: result.results) {
        if (!tag2entID.empty()) {
            entID = tag2entID.count(QString::fromStdString(res.outbound_tag.value())) == 0 ? -1 : tag2entID[QString::fromStdString(res.outbound_tag.value())];
        }
        if (entID == -1) {
            MW_show_log(tr("Something is very wrong, the subject ent cannot be found!"));
            continue;
        }

        auto ent = Configs::dataManager->profilesRepo->GetProfile(entID);
        if (ent == nullptr) {
            MW_show_log(tr("Profile manager data is corrupted, try again."));
            continue;
        }

        if (res.error.value().empty()) {
            ent->ip_out = QString::fromStdString(res.ip.value());
            ent->test_country = QString::fromStdString(res.country_code.value());
        } else {
            if (!QString::fromStdString(res.error.value()).contains("test aborted") &&
                !QString::fromStdString(res.error.value()).contains("context canceled")) {
                MW_show_log(tr("[%1] IP test error: %2").arg(ent->outbound->DisplayTypeAndName(), QString::fromStdString(res.error.value())));
            }
            ent->ip_out.clear();
            ent->test_country.clear();
        }
        Configs::dataManager->profilesRepo->Save(ent);
    }
}

void MainWindow::urltest_current_group(const QList<int>& profileIDs) {
    if (profileIDs.isEmpty()) {
        return;
    }
    if (!speedtestRunning.tryLock()) {
        MessageBoxWarning(software_name, tr("The last url test did not exit completely, please wait. If it persists, please restart the program."));
        return;
    }

    runOnNewThread([this, profileIDs]() {
        stopSpeedtest.store(false);
        dataViewHtmlGenerator_.seedLatencyTest(DataViewHtmlGenerator::LatencyTestPanelState::Kind::Url, profileIDs.size());
        UpdateDataView(true);
        auto speedTestFunc = [=, this](const QList<std::shared_ptr<Configs::Profile>>& profileSlice, const QList<int>& ids) {
            auto buildObject = Configs::BuildTestConfig(profileSlice);
            if (!buildObject->error.isEmpty()) {
                MW_show_log(tr("Failed to build test config for batch: ") + buildObject->error);
                return;
            }

            std::atomic<int> counter(0);
            // xray-full configs are folded into the single outboundTags test box
            // (their tags live in outboundTags), so they add no separate tests.
            auto testCount = buildObject->fullConfigs.size() + (!buildObject->outboundTags.empty());
            for (const auto &entID: buildObject->fullConfigs.keys()) {
                auto configStr = buildObject->fullConfigs[entID];
                auto func = [this, &counter, testCount, configStr, entID]() {
                    runURLTest(configStr, "", {}, true, {}, {}, entID);
                    ++counter;
                    if (counter.load() == testCount) {
                        speedtestRunning.unlock();
                    }
                };
                parallelCoreCallPool->start(func);
            }

            if (!buildObject->outboundTags.empty()) {
                auto func = [this, &buildObject, &counter, testCount]() {
                    auto xrayConf = buildObject->isXrayNeeded ? QJsonObject2QString(buildObject->xrayConfig, false) : "";
                    runURLTest(QJsonObject2QString(buildObject->coreConfig, false),xrayConf, buildObject->xrayFullConfigs, false, buildObject->outboundTags, buildObject->tag2entID);
                    ++counter;
                    if (counter.load() == testCount) {
                        speedtestRunning.unlock();
                    }
                };
                parallelCoreCallPool->start(func);
            }
            if (testCount == 0) speedtestRunning.unlock();

            speedtestRunning.lock();
            MW_show_log("URL test for batch done.");
            runOnUiThread([=,this]{
                refresh_proxy_list(ids);
            });
        };
        std::shared_ptr<Configs::Group> currentGroup;
        for (int i=0;i<profileIDs.length();i+=100) {
            if (stopSpeedtest.load()) break;
            auto profileIDsSlice = profileIDs.mid(i, 100);
            auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(profileIDsSlice);
            if (!currentGroup && !profiles.isEmpty()) {
                currentGroup = Configs::dataManager->groupsRepo->GetGroup(profiles[0]->gid);
            }
            speedTestFunc(profiles, profileIDsSlice);
        }
        dataViewHtmlGenerator_.clearTestSections();
        UpdateDataView(true);
        speedtestRunning.unlock();
        if (currentGroup->auto_clear_unavailable) {
            MW_show_log("URL test finished, clearing unavailable profiles...");
            runOnUiThread([=, this] {
               clearUnavailableProfiles(false, profileIDs);
            });
        }
        MW_show_log(tr("URL test finished!"));
    });
}

void MainWindow::stopTests() {
    stopSpeedtest.store(true);
    bool ok;
    defaultClient->StopTests(&ok);

    if (!ok) {
        MW_show_log(tr("Failed to stop tests"));
    }
}

void MainWindow::url_test_current() {
    last_test_time = QDateTime::currentSecsSinceEpoch();
    ui->label_running->setText(tr("Testing"));

    runOnNewThread([=,this] {
        libcore::TestReq req;
        req.test_current = true;
        req.url = Configs::dataManager->settingsRepo->test_latency_url.toStdString();

        bool rpcOK;
        auto result = defaultClient->Test(&rpcOK, req);
        if (!rpcOK || result.results.empty()) return;

        auto latency = result.results[0].latency_ms.value();
        last_test_time = QDateTime::currentSecsSinceEpoch();

        runOnUiThread([=,this] {
            if (!result.results[0].error.value().empty()) {
                MW_show_log(QString("UrlTest error: %1").arg(QString::fromStdString(result.results[0].error.value())));
            }
            if (latency <= 0) {
                ui->label_running->setText(tr("Test Result") + ": " + tr("Unavailable"));
            } else if (latency > 0) {
                ui->label_running->setText(tr("Test Result") + ": " + QString("%1 ms").arg(latency));
            }
        });
    });
}

void MainWindow::iptest_current_group(const QList<int>& profileIDs) {
    if (profileIDs.isEmpty()) {
        return;
    }
    if (!speedtestRunning.tryLock()) {
        MessageBoxWarning(software_name, tr("The last test did not exit completely, please wait. If it persists, please restart the program."));
        return;
    }

    runOnNewThread([this, profileIDs]() {
        stopSpeedtest.store(false);
        dataViewHtmlGenerator_.seedLatencyTest(DataViewHtmlGenerator::LatencyTestPanelState::Kind::Ip, profileIDs.size());
        UpdateDataView(true);
        auto ipTestFunc = [=, this](const QList<std::shared_ptr<Configs::Profile>>& profileSlice, const QList<int>& ids) {
            auto buildObject = Configs::BuildTestConfig(profileSlice);
            if (!buildObject->error.isEmpty()) {
                MW_show_log(tr("Failed to build test config for batch: ") + buildObject->error);
                return;
            }

            std::atomic<int> counter(0);
            // xray-full configs are folded into the single outboundTags test box
            // (their tags live in outboundTags), so they add no separate tests.
            auto testCount = buildObject->fullConfigs.size() + (!buildObject->outboundTags.empty());
            for (const auto &entID: buildObject->fullConfigs.keys()) {
                auto configStr = buildObject->fullConfigs[entID];
                auto func = [this, &counter, testCount, configStr, entID]() {
                    runIPTest(configStr, "", {}, true, {}, {}, entID);
                    ++counter;
                    if (counter.load() == testCount) {
                        speedtestRunning.unlock();
                    }
                };
                parallelCoreCallPool->start(func);
            }

            if (!buildObject->outboundTags.empty()) {
                auto func = [this, &buildObject, &counter, testCount]() {
                    auto xrayConf = buildObject->isXrayNeeded ? QJsonObject2QString(buildObject->xrayConfig, false) : "";
                    runIPTest(QJsonObject2QString(buildObject->coreConfig, false), xrayConf, buildObject->xrayFullConfigs, false, buildObject->outboundTags, buildObject->tag2entID);
                    ++counter;
                    if (counter.load() == testCount) {
                        speedtestRunning.unlock();
                    }
                };
                parallelCoreCallPool->start(func);
            }
            if (testCount == 0) speedtestRunning.unlock();

            speedtestRunning.lock();
            MW_show_log("IP test for batch done.");
            runOnUiThread([=,this]{
                refresh_proxy_list(ids);
            });
        };
        for (int i = 0; i < profileIDs.length(); i += 100) {
            if (stopSpeedtest.load()) break;
            auto profileIDsSlice = profileIDs.mid(i, 100);
            auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(profileIDsSlice);
            ipTestFunc(profiles, profileIDsSlice);
        }
        dataViewHtmlGenerator_.clearTestSections();
        UpdateDataView(true);
        speedtestRunning.unlock();
        MW_show_log(tr("IP test finished!"));
    });
}

void MainWindow::speedtest_current_group(const QList<int>& profileIDs, bool testCurrent)
{
    if (profileIDs.isEmpty() && !testCurrent) {
        return;
    }
    if (!speedtestRunning.tryLock()) {
        MessageBoxWarning(software_name, tr("The last test did not finish completely, please wait. If it persists, please restart the program."));
        return;
    }

    currentUnderTest.store(testCurrent);

    runOnNewThread([this, profileIDs, testCurrent]() {
        stopSpeedtest.store(false);
        // Fresh per-tag byte baselines for this speed-test session.
        { QMutexLocker lk(&speedtestCreditMu_); speedtestCredited_.clear(); }
        if (!testCurrent)
        {
            dataViewHtmlGenerator_.seedSpeedTest(profileIDs.size());
            UpdateDataView(true);
            auto speedTestFunc = [=, this](const QList<std::shared_ptr<Configs::Profile>>& profileSlice) {
                auto buildObject = Configs::BuildTestConfig(profileSlice);
                if (!buildObject->error.isEmpty()) {
                    MW_show_log(tr("Failed to build batch test config: ") + buildObject->error);
                    return;
                }

                for (const auto &entID: buildObject->fullConfigs.keys()) {
                    auto configStr = buildObject->fullConfigs[entID];
                    runSpeedTest(configStr, "", {}, true, false, {}, {}, entID);
                }

                if (!buildObject->outboundTags.empty()) {
                    auto xrayConf = buildObject->isXrayNeeded ? QJsonObject2QString(buildObject->xrayConfig, true) : "";
                    runSpeedTest(QJsonObject2QString(buildObject->coreConfig, false), xrayConf, buildObject->xrayFullConfigs, false, false, buildObject->outboundTags, buildObject->tag2entID, -1);
                }
            };
            int stepSize = Configs::dataManager->settingsRepo->speed_test_mode == Configs::TestConfig::COUNTRY ? 100 : 1;
            for (int i=0;i<profileIDs.length();i+=stepSize) {
                if (stopSpeedtest.load()) break;
                auto profileIDsSlice = profileIDs.mid(i, stepSize);
                auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(profileIDsSlice);
                speedTestFunc(profiles);
            }
        } else
        {
            dataViewHtmlGenerator_.seedSpeedTest(1);
            runSpeedTest("", "", {}, false, true, {}, {}, -1);
            currentUnderTest.store(false);
        }
        dataViewHtmlGenerator_.clearTestSections();
        UpdateDataView(true);
        speedtestRunning.unlock();
        runOnUiThread([=,this]{
            refresh_proxy_list(profileIDs);
            MW_show_log(tr("Speedtest finished!"));
        });
    });
}

void MainWindow::creditSpeedtestTraffic(const std::shared_ptr<Configs::Profile>& profile, const QString& tag, qint64 curUp, qint64 curDown)
{
    if (profile == nullptr || tag.isEmpty()) return;
    if (Configs::dataManager->settingsRepo->disable_traffic_stats) return;
    QMutexLocker lk(&speedtestCreditMu_);
    auto& base = speedtestCredited_[tag];
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

void MainWindow::querySpeedtest(const QMap<QString, int>& tag2entID, bool testCurrent)
{
    bool ok;
    auto res = defaultClient->QueryCurrentSpeedTests(&ok);
    if (!ok || !res.is_running.value())
    {
        return;
    }
    auto profile = testCurrent ? running : Configs::dataManager->profilesRepo->GetProfile(tag2entID[QString::fromStdString(res.result.value().outbound_tag.value())]);
    if (profile == nullptr)
    {
        return;
    }
    creditSpeedtestTraffic(profile, QString::fromStdString(res.result.value().outbound_tag.value()),
                           res.result.value().ul_bytes.value(), res.result.value().dl_bytes.value());
    runOnUiThread([=, this]
    {
        dataViewHtmlGenerator_.setSpeedtestProgress(profile->outbound->name, res.result.value());
        UpdateDataView();

        if (res.result.value().error.value().empty() && !res.result.value().cancelled.value())
        {
            if (!res.result.value().dl_speed.value().empty()) profile->dl_speed = QString::fromStdString(res.result.value().dl_speed.value());
            if (!res.result.value().ul_speed.value().empty()) profile->ul_speed = QString::fromStdString(res.result.value().ul_speed.value());
            if (profile->latency <= 0 && res.result.value().latency.value() > 0) profile->latency = res.result.value().latency.value();
            if (!res.result->server_country.value().empty()) profile->test_country = CountryNameToCode(QString::fromStdString(res.result.value().server_country.value()));
            refresh_proxy_list({profile->id});
        }
    });
}

void MainWindow::queryCountryTest(const QMap<QString, int>& tag2entID, bool testCurrent)
{
    bool ok;
    auto res = defaultClient->QueryCountryTestResults(&ok);
    if (!ok || res.results.empty())
    {
        return;
    }
    for (const auto& result : res.results)
    {
        dataViewHtmlGenerator_.addTestProgress();
        UpdateDataView();
        auto profile = testCurrent ? running : Configs::dataManager->profilesRepo->GetProfile(tag2entID[QString::fromStdString(result.outbound_tag.value())]);
        if (profile == nullptr)
        {
            return;
        }
        runOnUiThread([=, this]
        {
            if (result.error.value().empty() && !result.cancelled.value())
            {
                if (profile->latency <= 0 && result.latency.value() > 0) profile->latency = result.latency.value();
                if (!result.server_country.value().empty()) profile->test_country = CountryNameToCode(QString::fromStdString(result.server_country.value()));
                refresh_proxy_list({profile->id});
            }
        });
    }
    UpdateDataView(true);
}


void MainWindow::runSpeedTest(const QString& config, const QString& xrayConfig, const QStringList& xrayFullConfigs, bool useDefault, bool testCurrent, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID)
{
    if (stopSpeedtest.load()) {
        MW_show_log(tr("Profile speed test aborted"));
        return;
    }

    libcore::SpeedTestRequest req;
    auto speedtestConf = Configs::dataManager->settingsRepo->speed_test_mode;
    for (const auto &item: outboundTags) {
        req.outbound_tags.push_back(item.toStdString());
    }
    req.config = config.toStdString();
    req.use_default_outbound = useDefault;
    req.test_download = speedtestConf == Configs::TestConfig::FULL || speedtestConf == Configs::TestConfig::DL;
    req.test_upload = speedtestConf == Configs::TestConfig::FULL || speedtestConf == Configs::TestConfig::UL;
    req.simple_download = speedtestConf == Configs::TestConfig::SIMPLEDL;
    req.simple_download_addr = Configs::dataManager->settingsRepo->simple_dl_url.toStdString();
    req.test_current = testCurrent;
    req.timeout_ms = Configs::dataManager->settingsRepo->speed_test_timeout_ms;
    req.only_country = speedtestConf == Configs::TestConfig::COUNTRY;
    req.country_concurrency = Configs::dataManager->settingsRepo->test_concurrent;
    req.xray_config = xrayConfig.toStdString();
    req.need_xray = !xrayConfig.isEmpty();
    for (const auto &xc : xrayFullConfigs) req.xray_full_configs.push_back(xc.toStdString());

    if (speedtestConf != Configs::TestConfig::COUNTRY) {
        dataViewHtmlGenerator_.addTestProgress();
        UpdateDataView();
    }

    // loop query result
    auto doneMu = new QMutex;
    doneMu->lock();
    runOnNewThread([=,this]
    {
        while (true) {
            QThread::msleep(100);
            if (doneMu->tryLock())
            {
                break;
            }
            if (speedtestConf == Configs::TestConfig::COUNTRY)
            {
                queryCountryTest(tag2entID, testCurrent);
            } else
            {
                querySpeedtest(tag2entID, testCurrent);
            }
        }
        doneMu->unlock();
        delete doneMu;
    });
    bool rpcOK;
    QString coreError;
    auto result = defaultClient->SpeedTest(&rpcOK, req, &coreError);
    doneMu->unlock();
    //
    if (!rpcOK || result.results.empty()) {
        // Detect missing Xray geo assets from a failed SpeedTest RPC (see runURLTest).
        if (!rpcOK) {
            QString ctxName = tr("a tested profile");
            int nameId = testCurrent ? (running ? running->id : -1) : entID;
            if (nameId != -1) {
                if (auto e = Configs::dataManager->profilesRepo->GetProfile(nameId)) ctxName = e->outbound->DisplayTypeAndName();
            }
            handleXrayGeoAssetError(coreError, ctxName);
        }
        return;
    }

    for (const auto &res: result.results) {
        if (testCurrent) entID = running ? running->id : -1;
        else {
            entID = tag2entID.count(QString::fromStdString(res.outbound_tag.value())) == 0 ? -1 : tag2entID[QString::fromStdString(res.outbound_tag.value())];
        }
        if (entID == -1) {
            MW_show_log(tr("Something is very wrong, the subject ent cannot be found!"));
            continue;
        }

        auto ent = Configs::dataManager->profilesRepo->GetProfile(entID);
        if (ent == nullptr) {
            MW_show_log(tr("Profile manager data is corrupted, try again."));
            continue;
        }

        creditSpeedtestTraffic(ent, QString::fromStdString(res.outbound_tag.value()),
                               res.ul_bytes.value(), res.dl_bytes.value());

        if (res.cancelled.value()) continue;

        if (res.error.value().empty()) {
            ent->dl_speed = QString::fromStdString(res.dl_speed.value());
            ent->ul_speed = QString::fromStdString(res.ul_speed.value());
            if (ent->latency <= 0 && res.latency.value() > 0) ent->latency = res.latency.value();
            if (!res.server_country.value().empty()) ent->test_country = CountryNameToCode(QString::fromStdString(res.server_country.value()));
        } else {
            ent->dl_speed = "N/A";
            ent->ul_speed = "N/A";
            ent->latency = -1;
            ent->test_country = "";
            MW_show_log(tr("[%1] speed test error: %2").arg(ent->outbound->DisplayTypeAndName(), QString::fromStdString(res.error.value())));
        }
        Configs::dataManager->profilesRepo->Save(ent);
    }
}

bool MainWindow::set_system_dns(bool set, bool save_set) {
    if (!Configs::dataManager->settingsRepo->enable_dns_server) {
        MW_show_log(tr("You need to enable hijack DNS server first"));
        return false;
    }
    if (!get_elevated_permissions(4)) {
        return false;
    }
    bool rpcOK;
    QString res;
    if (set) {
        res = defaultClient->SetSystemDNS(&rpcOK, false);
    } else {
        res = defaultClient->SetSystemDNS(&rpcOK, true);
    }
    if (!rpcOK) {
        MW_show_log(tr("Failed to set system dns: ") + res);
        return false;
    }
    if (save_set) Configs::dataManager->settingsRepo->system_dns_set = set;
    return true;
}

int MainWindow::get_profile_to_start() {
    auto ents = get_now_selected_list();
    if (ents.size() == 1) {
        return ents.first();
    }
    if (ents.isEmpty()) {
        if (last_running_profile_id >= 0 && Configs::dataManager->profilesRepo->GetProfile(last_running_profile_id) != nullptr) {
            return last_running_profile_id;
        }
        int rememberId = Configs::dataManager->settingsRepo->remember_id;
        if (rememberId >= 0 && Configs::dataManager->profilesRepo->GetProfile(rememberId) != nullptr) {
            return rememberId;
        }
        auto currentGroup = Configs::dataManager->groupsRepo->CurrentGroup();
        if (currentGroup) {
            auto profiles = currentGroup->Profiles();
            if (!profiles.isEmpty()) {
                int firstId = profiles.first();
                if (Configs::dataManager->profilesRepo->GetProfile(firstId) != nullptr) {
                    return firstId;
                }
            }
        }
    }
    return -1;
}

bool MainWindow::handleXrayGeoAssetError(const QString& error, const QString& contextName) {
    // The Xray config's routing referenced geoip:/geosite: rules. Two distinct
    // failures surface here (both when starting a profile via an in-band Start
    // error and when testing one via an RPC error payload). XRAY_LOCATION_ASSET
    // points at GetBasePath():
    //   1. The .dat asset isn't installed  -> "failed to open geoip.dat: ..."
    //   2. The .dat is installed but lacks the referenced category
    //                                       -> "failed to load code cn from geoip.dat: EOF"
    const bool refGeoip = error.contains("geoip.dat");
    const bool refGeosite = error.contains("geosite.dat");
    if (!refGeoip && !refGeosite) return false;

    runOnUiThread([=, this] {
        // A batch test can raise this for many profiles at once — only act once.
        if (m_xrayGeoAssetBusy) return;
        m_xrayGeoAssetBusy = true;
        // Small delay so any in-flight UI teardown (e.g. Connecting -> idle)
        // settles before the modal prompt appears.
        setTimeout([=, this] {
            const QString base = Configs::GetBasePath();
            const bool haveGeoip = QFile::exists(base + "/geoip.dat");
            const bool haveGeosite = QFile::exists(base + "/geosite.dat");

            const bool geoipLacksCategory = refGeoip && haveGeoip;
            const bool geositeLacksCategory = refGeosite && haveGeosite;
            if (geoipLacksCategory || geositeLacksCategory) {
                const QString whichFile = geositeLacksCategory ? "geosite.dat" : "geoip.dat";
                const QString ruleType = geositeLacksCategory ? "geosite" : "geoip";

                QString category;
                QRegularExpression re(QStringLiteral("code\\s+(\\S+)\\s+from"));
                const auto m = re.match(error);
                if (m.hasMatch()) category = m.captured(1);
                const QString needed = category.isEmpty()
                    ? tr("a required category")
                    : QStringLiteral("%1:%2").arg(ruleType, category);

                MessageBoxWarning(
                    tr("Geo asset missing category"),
                    tr("The Xray config \"%1\" needs \"%2\", but the installed %3 does "
                       "not contain it.\n\n"
                       "Re-downloading from the same source will not fix this — the data "
                       "file does not include that category. Set the GeoIP/GeoSite asset "
                       "URL in Settings to a source that provides \"%2\", then delete %3 "
                       "from the app folder and download it again.")
                        .arg(contextName, needed, whichFile));
                m_xrayGeoAssetBusy = false;
                return;
            }

            // Case 1: the referenced asset file is missing -> offer to download it.
            if (QMessageBox::question(this, tr("Geo asset files required"),
                    tr("The Xray config \"%1\" uses geoip/geosite routing rules, but the "
                       "required data files (geoip.dat / geosite.dat) are not installed.\n\n"
                       "Download them now?").arg(contextName)) != QMessageBox::Yes) {
                m_xrayGeoAssetBusy = false;
                return;
            }

            runOnNewThread([=, this] {
                QString dlErr;
                if (!haveGeoip) {
                    auto e = NetworkRequestHelper::DownloadAsset(Configs::dataManager->settingsRepo->xray_geoip_url, "geoip.dat");
                    if (!e.isEmpty()) dlErr += "geoip.dat: " + e + "\n";
                }
                if (!haveGeosite) {
                    auto e = NetworkRequestHelper::DownloadAsset(Configs::dataManager->settingsRepo->xray_geosite_url, "geosite.dat");
                    if (!e.isEmpty()) dlErr += "geosite.dat: " + e + "\n";
                }
                runOnUiThread([=, this] {
                    m_xrayGeoAssetBusy = false;
                    if (!dlErr.isEmpty()) {
                        MessageBoxWarning(tr("Geo asset download failed"), dlErr);
                    } else {
                        MW_show_log(tr("Downloaded Xray geo asset files."));
                        QMessageBox::information(this, tr("Geo assets installed"),
                            tr("Geo data files were downloaded successfully.\n\n"
                               "Please try again."));
                    }
                });
            });
        }, this, 300);
    });
    return true;
}

void MainWindow::profile_start(int _id) {
    if (Configs::dataManager->settingsRepo->prepare_exit) return;
#ifdef Q_OS_LINUX
    if (Configs::dataManager->settingsRepo->enable_dns_server && Configs::dataManager->settingsRepo->dns_server_listen_port <= 1024) {
        if (!get_elevated_permissions()) {
            MW_show_log(QString("Failed to get admin access, cannot listen on port %1 without it").arg(Configs::dataManager->settingsRepo->dns_server_listen_port));
            return;
        }
    }
#endif

    std::shared_ptr<Configs::Profile> ent = nullptr;
    if (_id >= 0) {
        ent = Configs::dataManager->profilesRepo->GetProfile(_id);
    } else {
        int startId = get_profile_to_start();
        if (startId >= 0) {
            ent = Configs::dataManager->profilesRepo->GetProfile(startId);
        }
    }
    if (ent == nullptr) return;

    last_running_profile_id = ent->id;

    if (select_mode) {
        emit profile_selected(ent->id);
        select_mode = false;
        refresh_status();
        return;
    }

    auto group = Configs::dataManager->groupsRepo->GetGroup(ent->gid);
    if (group == nullptr || group->archive) return;

    auto result = Configs::BuildSingBoxConfig(ent);
    if (!result->error.isEmpty()) {
        MessageBoxWarning(tr("BuildConfig return error"), result->error);
        return;
    }

    auto profile_start_stage2 = [=, this] {
        libcore::LoadConfigReq req;
        req.core_config = QJsonObject2QString(result->coreConfig, true).toStdString();
        req.tun_ipv4_cidr = result->tunIPv4CIDR.toStdString();
        req.disable_stats = Configs::dataManager->settingsRepo->disable_traffic_stats;
        req.xray_config = QJsonObject2QString(result->xrayConfig, true).toStdString();
        req.need_xray = !result->xrayConfig.isEmpty();
        if (req.need_xray) {
            // Outbound server-domain resolution for the live Xray instance is
            // wired in the core (ThroneWiring), not baked into the config: point
            // it at sing-box's loopback DNS-in with the user's direct-DNS
            // strategy and let the instance build the resolver internally. Test
            // instances build their own req and leave these empty.
            req.xray_outbound_dns_address = ("127.0.0.1:" + QString::number(Configs::dataManager->settingsRepo->core_dns_in_port)).toStdString();
            req.xray_outbound_dns_strategy = Configs::getXrayOutboundDomainStrategy().toStdString();
        }
        if (!result->extraCoreData->path.isEmpty())
        {
            req.need_extra_process = true;
            req.extra_process_path = result->extraCoreData->path.toStdString();
            req.extra_process_args = result->extraCoreData->args.toStdString();
            req.extra_process_conf = result->extraCoreData->config.toStdString();
            req.extra_no_out = result->extraCoreData->noLog;
        }
        //
        bool rpcOK;
        QString error = defaultClient->Start(&rpcOK, req);
        if (!rpcOK) {
            return false;
        }
        if (!error.isEmpty()) {
            // The Xray config's routing referenced geoip:/geosite: tags but the .dat
            // asset(s) aren't installed. Handle this out-of-band: fail this start
            // attempt right away — blocking here to download would trip the "no
            // response" restart prompt — while handleXrayGeoAssetError asynchronously
            // prompts and downloads. We deliberately don't auto-start; the env var is
            // already set on the live core, so starting the profile again picks the
            // assets up with no core restart.
            if (handleXrayGeoAssetError(error, ent->outbound->DisplayTypeAndName())) {
                return false;
            }
            if (error.contains("Fwpm", Qt::CaseInsensitive)) {
                runOnUiThread([=, this] {
                    MessageBoxWarning(
                        tr("Strict routing unavailable"),
                        tr("Windows could not enable strict routing. Open Tun Settings, "
                           "disable Strict Route, and start the profile again.\n\n"
                           "Disabling Strict Route may cause DNS leaks.\n\nError: %1").arg(error));
                });
                return false;
            }
            if (error.contains("configure tun interface")) {
                runOnUiThread([=, this] {

                    QMessageBox msg(
                        QMessageBox::Information,
                        tr("Tun device misbehaving"),
                        tr("If you have trouble starting VPN, you can force reset Core process here and then try starting the profile again. The error is %1").arg(error),
                        QMessageBox::NoButton,
                        this
                    );
                    msg.addButton(tr("Reset"), QMessageBox::ActionRole);
                    auto cancel = msg.addButton(tr("Cancel"), QMessageBox::ActionRole);

                    msg.setDefaultButton(cancel);
                    msg.setEscapeButton(cancel);

                    int r = msg.exec() - 2;
                    if (r == 0) {
                        StopVPNProcess();
                    }
                });
                return false;
            }
            runOnUiThread([=] { MessageBoxWarning("LoadConfig return error", error); });
            return false;
        }
        //
        Stats::trafficLooper->SetChainGroups(result->chainGroups);
        Stats::trafficLooper->loop_enabled = true;
        Stats::connection_lister->suspend = false;

        Configs::dataManager->settingsRepo->UpdateStartedId(ent->id);
        running = ent;
        if (Configs::dataManager->settingsRepo->spmode_system_proxy) set_system_proxy(true);

        runOnUiThread([=, this] {
            refresh_status();
            refresh_proxy_list({ent->id});

            auto resp = NetworkRequestHelper::HttpGet("http://ip-api.com/json/", false, true);
            if (resp.error.isEmpty()) {
                QJsonDocument doc = QJsonDocument::fromJson(resp.data);
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();
                    QString city = obj["city"].toString();
                    QString countryName = obj["country"].toString();
                    QString countryCode = obj["countryCode"].toString();
                    if (running) running->runningCountryInfo = QString("%1 %2, %3").arg(CountryCodeToFlag(countryCode), countryName, city);
                    refresh_status();
                }
            }
        });

        return true;
    };

    if (!mu_starting.tryLock()) {
        MessageBoxWarning(software_name, tr("Another profile is starting..."));
        return;
    }
    if (!mu_stopping.tryLock()) {
        MessageBoxWarning(software_name, tr("Another profile is stopping..."));
        mu_starting.unlock();
        return;
    }
    mu_stopping.unlock();

    // check core state
    if (!Configs::dataManager->settingsRepo->core_running) {
        runOnThread(
            [=, this] {
                MW_show_log(tr("Try to start the config, but the core has not listened to the RPC port, so restart it..."));
                core_process->start_profile_when_core_is_up = ent->id;
                core_process->Restart();
            },
            DS_cores);
        mu_starting.unlock();
        return; // let CoreProcess call profile_start when core is up
    }

    // timeout message
    auto restartMsgbox = new QMessageBox(QMessageBox::Question, software_name, tr("If there is no response for a long time, it is recommended to restart the software."),
                                         QMessageBox::Yes | QMessageBox::No, this);
    connect(restartMsgbox, &QMessageBox::accepted, this, [=,this] { MW_dialog_message(MwMessage::RestartProgram, {}); });
    auto restartMsgboxTimer = new MessageBoxTimer(this, restartMsgbox, 10000);

    // Show the "Connecting" state until the start resolves below.
    runOnUiThread([this] {
        m_profileConnecting = true;
        refresh_startstop_button();
    });

    runOnNewThread([=, this] {
        // stop current running
        if (running != nullptr) {
            profile_stop(false, false, true);
            mu_stopping.lock();
            mu_stopping.unlock();
        }
        // do start
        MW_show_log(">>>>>>>> " + tr("Starting profile %1").arg(ent->outbound->DisplayTypeAndName()));
        if (!profile_start_stage2()) {
            MW_show_log("<<<<<<<< " + tr("Failed to start profile %1").arg(ent->outbound->DisplayTypeAndName()));
        }
        mu_starting.unlock();
        // cancel timeout
        runOnUiThread([=, this] {
            restartMsgboxTimer->cancel();
            restartMsgboxTimer->deleteLater();
            restartMsgbox->deleteLater();
            // Start has resolved (success or failure); leave the Connecting state.
            m_profileConnecting = false;
            refresh_startstop_button();
        });
    });
}

void MainWindow::profile_stop(bool crash, bool block, bool manual) {
    if (running == nullptr) {
        return;
    }
    auto id = running->id;

    auto profile_stop_stage2 = [=,this] {
        if (currentUnderTest.load()) {
            bool ok;
            defaultClient->StopTests(&ok);
            if (!ok) MW_show_log("Failed to stop profile tests!");
        }
        if (!crash) {
            bool rpcOK;
            QString error = defaultClient->Stop(&rpcOK);
            if (rpcOK && !error.isEmpty()) {
                runOnUiThread([=,this] { MessageBoxWarning(tr("Stop return error"), error); });
                return false;
            } else if (!rpcOK) {
                return false;
            }
        }
        if (Configs::dataManager->settingsRepo->spmode_system_proxy) set_system_proxy(false);
        return true;
    };

    if (!mu_stopping.tryLock()) {
        return;
    }

    UpdateConnectionListWithRecreate({});

    // Show a "Disconnecting" spinner immediately; the stop itself can lag.
    runOnUiThread([this] {
        m_profileDisconnecting = true;
        refresh_startstop_button();
    });

    runOnNewThread([=, this] {
        Stats::trafficLooper->loop_enabled = false;
        Stats::connection_lister->suspend = true;
        Stats::trafficLooper->loop_mutex.lock();
        Stats::trafficLooper->UpdateAll();
        Stats::trafficLooper->loop_mutex.unlock();
        // Flush the final per-profile totals (only persisted every few seconds
        // during the session) and the partial minute bucket before going down.
        Stats::trafficLooper->PersistTraffic();
        Stats::trafficStatsManager->Flush();

        QMessageBox* restartMsgbox;
        MessageBoxTimer* restartMsgboxTimer;
        runOnUiThread([=, this, &restartMsgbox, &restartMsgboxTimer] {
            restartMsgbox = new QMessageBox(QMessageBox::Question, software_name, tr("If there is no response for a long time, it is recommended to restart the software."),
                             QMessageBox::Yes | QMessageBox::No, this);
            connect(restartMsgbox, &QMessageBox::accepted, this, [=] { MW_dialog_message(MwMessage::RestartProgram, {}); });
            restartMsgboxTimer = new MessageBoxTimer(this, restartMsgbox, 5000);
        }, true);

        // do stop
        MW_show_log(">>>>>>>> " + tr("Stopping profile %1").arg(running->outbound->DisplayTypeAndName()));
        if (!profile_stop_stage2()) {
            MW_show_log("<<<<<<<< " + tr("Failed to stop, please restart the program."));
        }

        if (manual) Configs::dataManager->settingsRepo->UpdateStartedId(-1919);
        running = nullptr;

        runOnUiThread([=, this, &restartMsgboxTimer, &restartMsgbox] {
            restartMsgboxTimer->cancel();
            restartMsgboxTimer->deleteLater();
            restartMsgbox->deleteLater();

            m_profileDisconnecting = false;
            refresh_status();
            refresh_proxy_list({id});

            mu_stopping.unlock();
        }, true);
    }, block);
}
