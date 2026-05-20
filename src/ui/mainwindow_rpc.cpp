#include "include/ui/mainwindow.h"

#include "include/stats/traffic/TrafficLooper.hpp"
#include "include/api/RPC.h"
#include "include/ui/utils//MessageBoxTimer.h"
#include "3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.hpp"

#include <QInputDialog>
#include <QPushButton>
#include <QDesktopServices>
#include <QMessageBox>
#include <QJsonDocument>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QApplication>
#include <QClipboard>
#include <QScrollBar>
#include <QPointer>
#include <QLabel>

#include "include/configs/generate.h"
#include "include/configs/sub/GroupUpdater.hpp"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"

#include "include/sys/Process.hpp"

#include <algorithm>

#include <memory>

// rpc

using namespace API;

void MainWindow::setup_rpc(QLocalSocket *socket) {
    // The Client is long-lived and never recreated; on core restart we only
    // swap the underlying connection so worker threads holding `defaultClient`
    // never touch freed memory.
    QMutexLocker lock(&defaultClientMutex);
    if (defaultClient == nullptr) {
        defaultClient = new Client();
    }
    defaultClient->Reconnect(socket);

    // Loopers run for the lifetime of the app, start only once
    if (!rpc_started) {
        rpc_started = true;
        runOnNewThread([=] { Stats::trafficLooper->Loop(); });
        runOnNewThread([=] { Stats::connection_lister->Loop(); });
    }
}

void MainWindow::runURLTest(const QString& config, const QString& xrayConfig, bool useDefault, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID) {
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
    auto result = defaultClient->Test(&rpcOK, req);
    done->unlock();
    //
    if (!rpcOK || result.results.empty()) return;

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

void MainWindow::runIPTest(const QString& config, const QString& xrayConfig, bool useDefault, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID) {
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
    auto result = defaultClient->IPTest(&rpcOK, req);
    done->unlock();
    //
    if (!rpcOK || result.results.empty()) return;

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
            auto testCount = buildObject->fullConfigs.size() + (!buildObject->outboundTags.empty());
            for (const auto &entID: buildObject->fullConfigs.keys()) {
                auto configStr = buildObject->fullConfigs[entID];
                auto func = [this, &counter, testCount, configStr, entID]() {
                    runURLTest(configStr, "", true, {}, {}, entID);
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
                    runURLTest(QJsonObject2QString(buildObject->coreConfig, false),xrayConf, false, buildObject->outboundTags, buildObject->tag2entID);
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
            auto testCount = buildObject->fullConfigs.size() + (!buildObject->outboundTags.empty());
            for (const auto &entID: buildObject->fullConfigs.keys()) {
                auto configStr = buildObject->fullConfigs[entID];
                auto func = [this, &counter, testCount, configStr, entID]() {
                    runIPTest(configStr, "", true, {}, {}, entID);
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
                    runIPTest(QJsonObject2QString(buildObject->coreConfig, false), xrayConf, false, buildObject->outboundTags, buildObject->tag2entID);
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
                    runSpeedTest(configStr, "", true, false, {}, {}, entID);
                }

                if (!buildObject->outboundTags.empty()) {
                    auto xrayConf = buildObject->isXrayNeeded ? QJsonObject2QString(buildObject->xrayConfig, true) : "";
                    runSpeedTest(QJsonObject2QString(buildObject->coreConfig, false), xrayConf, false, false, buildObject->outboundTags, buildObject->tag2entID, -1);
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
            runSpeedTest("", "", true, true, {}, {}, -1);
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


void MainWindow::runSpeedTest(const QString& config, const QString& xrayConfig, bool useDefault, bool testCurrent, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID)
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
    auto result = defaultClient->SpeedTest(&rpcOK, req);
    doneMu->unlock();
    //
    if (!rpcOK || result.results.empty()) return;

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

    auto ents = get_now_selected_list();
    auto ent = (_id < 0 && !ents.isEmpty()) ? Configs::dataManager->profilesRepo->GetProfile(ents.first()) : Configs::dataManager->profilesRepo->GetProfile(_id);
    if (ent == nullptr) return;

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
        Stats::trafficLooper->SetEnts(result->outboundEntsForTraffic);
        Stats::trafficLooper->isChain = result->isChained;
        Stats::trafficLooper->loop_enabled = true;
        Stats::connection_lister->suspend = false;

        Configs::dataManager->settingsRepo->UpdateStartedId(ent->id);
        running = ent;
        set_system_proxy(false);

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
    connect(restartMsgbox, &QMessageBox::accepted, this, [=,this] { MW_dialog_message("", "RestartProgram"); });
    auto restartMsgboxTimer = new MessageBoxTimer(this, restartMsgbox, 10000);

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
        runOnUiThread([=] {
            restartMsgboxTimer->cancel();
            restartMsgboxTimer->deleteLater();
            restartMsgbox->deleteLater();
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
        set_system_proxy(true);
        return true;
    };

    if (!mu_stopping.tryLock()) {
        return;
    }

    UpdateConnectionListWithRecreate({});

    runOnNewThread([=, this] {
        Stats::trafficLooper->loop_enabled = false;
        Stats::connection_lister->suspend = true;
        Stats::trafficLooper->loop_mutex.lock();
        Stats::trafficLooper->UpdateAll();
        Stats::trafficLooper->loop_mutex.unlock();

        QMessageBox* restartMsgbox;
        MessageBoxTimer* restartMsgboxTimer;
        runOnUiThread([=, this, &restartMsgbox, &restartMsgboxTimer] {
            restartMsgbox = new QMessageBox(QMessageBox::Question, software_name, tr("If there is no response for a long time, it is recommended to restart the software."),
                             QMessageBox::Yes | QMessageBox::No, this);
            connect(restartMsgbox, &QMessageBox::accepted, this, [=] { MW_dialog_message("", "RestartProgram"); });
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

            refresh_status();
            refresh_proxy_list({id});

            mu_stopping.unlock();
        }, true);
    }, block);
}

// ─── Debug Check All VLess ─────────────────────────────────────────────────

// Indices into the lines list where each profile's block starts.
// Each profile occupies 5 lines:
//   [0] header   "[N/T] Name (type)"
//   [1] IP check
//   [2] TCP check
//   [3] UDP check
//   [4] blank separator

static const int LINES_PER_PROFILE = 5;

// Lines before the first profile block.
static const int HEADER_LINES = 5; // title, timestamp, profile count, sub-update status, blank

static QString pendingBlock(int idx, int total, const QString &name, const QString &type) {
    return QString("[%1/%2] %3 (%4)\n"
                   "  IP Check  : PENDING\n"
                   "  TCP (1MB) : PENDING\n"
                   "  UDP/DNS   : PENDING\n")
        .arg(idx).arg(total).arg(name).arg(type);
}

void MainWindow::check_all_vless_profiles() {
    // ── 1. Collect vless profiles ───────────────────────────────────────────
    auto collectVless = [] {
        QList<std::shared_ptr<Configs::Profile>> result;
        for (int gid : Configs::dataManager->groupsRepo->GetAllGroupIds()) {
            auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
            if (!group) continue;
            for (int pid : group->Profiles()) {
                auto ent = Configs::dataManager->profilesRepo->GetProfile(pid);
                if (ent && (ent->type == "vless" || ent->type == "xrayvless"))
                    result.append(ent);
            }
        }
        return result;
    };

    auto initialProfiles = collectVless();
    if (initialProfiles.isEmpty()) {
        runOnUiThread([] {
            QMessageBox::information(GetMainWindow(), QObject::tr("Debug Check"),
                                     QObject::tr("No VLess profiles found."));
        }, true);
        return;
    }

    // ── 2. Build initial PENDING text and open the dialog immediately ───────
    QString initialText;
    initialText += "=== Throne VLess Debug Check ===\n";
    initialText += QString("Timestamp : %1\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    initialText += QString("Profiles  : %1\n").arg(initialProfiles.size());
    initialText += "Subscriptions : Updating...\n";
    initialText += "\n";
    for (int i = 0; i < initialProfiles.size(); ++i)
        initialText += pendingBlock(i + 1, initialProfiles.size(),
                                    initialProfiles[i]->outbound->name,
                                    initialProfiles[i]->type);
    initialText += "=== Running... ===";

    // Create the dialog on the UI thread and keep safe pointers via QPointer.
    QPointer<QPlainTextEdit> edit;
    QPointer<QPushButton>    copyBtn;
    runOnUiThread([&] {
        auto *dialog = new QDialog(GetMainWindow());
        dialog->setWindowTitle(QObject::tr("Debug Check Results"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->resize(700, 500);

        auto *layout = new QVBoxLayout(dialog);

        edit = new QPlainTextEdit(dialog);
        edit->setReadOnly(true);
        edit->setPlainText(initialText);
        QFont mono("Monospace");
        mono.setStyleHint(QFont::Monospace);
        edit->setFont(mono);
        layout->addWidget(edit);

        auto *supportLabel = new QLabel(
            QObject::tr("After this is complete, copy the output using the button and send it to support.\n"
                        "Debug info does not contain any personal information."),
            dialog);
        supportLabel->setWordWrap(true);
        layout->addWidget(supportLabel);

        auto *buttons = new QDialogButtonBox(dialog);
        copyBtn = buttons->addButton(QObject::tr("Wait..."), QDialogButtonBox::ActionRole);
        copyBtn->setEnabled(false);
        QObject::connect(copyBtn, &QPushButton::clicked, dialog, [edit] {
            if (edit) QApplication::clipboard()->setText(edit->toPlainText());
        });
        layout->addWidget(buttons);

        dialog->setLayout(layout);
        dialog->show();
    }, true); // wait=true so pointers are valid before we continue

    // Helper: replace the full text in the editor from the background thread.
    // QPointer copy is safe — becomes null automatically when the widget is destroyed.
    auto updateEdit = [edit](const QString &text) {
        runOnUiThread([edit, text] {
            if (!edit) return;
            int scrollPos = edit->verticalScrollBar()->value();
            edit->setPlainText(text);
            edit->verticalScrollBar()->setValue(scrollPos);
        });
    };

    // ── 3. Update subscriptions ─────────────────────────────────────────────
    MW_show_log(tr("Debug check: updating subscriptions..."));
    runOnUiThread([&] { UI_update_all_groups(false); }, true);

    // Poll until the subscription updater finishes (max ~60 s).
    for (int waited = 0; waited < 600 && UI_update_all_groups_is_updating(); ++waited)
        QThread::msleep(100);

    // ── 4. Re-collect profiles (subscription may have added/removed some) ───
    auto profiles = collectVless();
    const int total = profiles.size();
    MW_show_log(tr("Debug check: starting checks for %1 VLess profile(s)...").arg(total));

    // Rebuild the text with the fresh profile list, all still PENDING.
    QStringList lines;
    lines << "=== Throne VLess Debug Check ===";
    lines << QString("Timestamp : %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    lines << QString("Profiles  : %1").arg(total);
    lines << "Subscriptions : Updated";
    lines << "";
    for (int i = 0; i < total; ++i) {
        lines << QString("[%1/%2] %3 (%4)").arg(i+1).arg(total)
                     .arg(profiles[i]->outbound->name).arg(profiles[i]->type);
        lines << "  IP Check  : PENDING";
        lines << "  TCP (1MB) : PENDING";
        lines << "  UDP/DNS   : PENDING";
        lines << "";
    }
    lines << "=== Running... ===";
    updateEdit(lines.join("\n"));

    // Helper: overwrite the 3 result lines for profile at position idx (0-based).
    auto setProfileLines = [&](int idx, const QString &ip, const QString &tcp, const QString &udp) {
        int base = HEADER_LINES + idx * LINES_PER_PROFILE;
        // base+0 is the "[N/T] Name" header — keep it; replace +1,+2,+3
        lines[base + 1] = ip;
        lines[base + 2] = tcp;
        lines[base + 3] = udp;
        updateEdit(lines.join("\n"));
    };

    // ── 5. Run checks ───────────────────────────────────────────────────────
    const int timeoutMs = 30000;

    for (int idx = 0; idx < total; ++idx) {
        const auto &ent = profiles[idx];
        const int base  = HEADER_LINES + idx * LINES_PER_PROFILE;

        // Mark as RUNNING
        lines[base + 1] = "  IP Check  : RUNNING";
        lines[base + 2] = "  TCP (1MB) : RUNNING";
        lines[base + 3] = "  UDP/DNS   : RUNNING";
        updateEdit(lines.join("\n"));

        MW_show_log(tr("Debug check [%1/%2]: %3").arg(idx+1).arg(total).arg(ent->outbound->name));

        auto buildObject = Configs::BuildTestConfig({ent});
        if (!buildObject->error.isEmpty()) {
            setProfileLines(idx,
                QString("  ERROR: Could not build test config: %1").arg(buildObject->error),
                "", "");
            continue;
        }

        QString configStr, outboundTag, xrayConfig;
        bool useDefault = false, needXray = false;

        if (buildObject->fullConfigs.contains(ent->id)) {
            configStr  = buildObject->fullConfigs[ent->id];
            useDefault = true;
        } else if (!buildObject->outboundTags.isEmpty()) {
            configStr   = QJsonObject2QString(buildObject->coreConfig, false);
            outboundTag = buildObject->outboundTags.first();
            needXray    = buildObject->isXrayNeeded;
            xrayConfig  = needXray ? QJsonObject2QString(buildObject->xrayConfig, false) : "";
        } else {
            setProfileLines(idx, "  ERROR: Empty test config produced.", "", "");
            continue;
        }

        libcore::DebugCheckRequest req;
        req.config               = configStr.toStdString();
        req.use_default_outbound = useDefault;
        req.outbound_tag         = outboundTag.toStdString();
        req.profile_name         = ent->outbound->name.toStdString();
        req.timeout_ms           = timeoutMs;
        req.need_xray            = needXray;
        req.xray_config          = xrayConfig.toStdString();

        bool rpcOK = false;
        auto result = defaultClient->DebugCheck(&rpcOK, req);

        if (!rpcOK) {
            setProfileLines(idx, "  ERROR: RPC call failed (core not running?)", "", "");
            continue;
        }
        if (!result.error.value().empty()) {
            setProfileLines(idx,
                QString("  ERROR: %1").arg(QString::fromStdString(result.error.value())),
                "", "");
            continue;
        }

        const QString realIP  = QString::fromStdString(result.real_ip.value());
        const QString proxyIP = QString::fromStdString(result.proxy_ip.value());

        QString ipLine, tcpLine, udpLine;

        if (result.ip_changed.value())
            ipLine = QString("  IP Check  : PASS  ([hidden]  →  %1)").arg(proxyIP);
        else if (proxyIP.isEmpty())
            ipLine = QString("  IP Check  : FAIL  (could not reach ipify)");
        else
            ipLine = QString("  IP Check  : WARN  IP unchanged (proxy: %1)").arg(proxyIP);

        if (result.tcp_ok.value())
            tcpLine = QString("  TCP (1MB) : PASS  (%1 bytes)").arg(result.tcp_bytes.value());
        else {
            const QString e = QString::fromStdString(result.tcp_error.value());
            tcpLine = QString("  TCP (1MB) : FAIL  %1").arg(e.isEmpty() ? "(unknown)" : e);
        }

        if (result.udp_ok.value())
            udpLine = "  UDP/DNS   : PASS  (youtube.com DNS via 8.8.8.8:53 over UDP)";
        else {
            const QString e = QString::fromStdString(result.udp_error.value());
            udpLine = QString("  UDP/DNS   : FAIL  %1").arg(e.isEmpty() ? "(unknown)" : e);
        }

        setProfileLines(idx, ipLine, tcpLine, udpLine);
    }

    // ── 6. Mark done ────────────────────────────────────────────────────────
    lines.last() = "=== Done ===";
    updateEdit(lines.join("\n"));
    runOnUiThread([copyBtn] {
        if (!copyBtn) return;
        copyBtn->setText(QObject::tr("Copy to Clipboard"));
        copyBtn->setEnabled(true);
    });
    MW_show_log(tr("Debug check finished."));
}
