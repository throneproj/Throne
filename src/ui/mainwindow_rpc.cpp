#include "include/ui/mainwindow.h"

#include "include/stats/traffic/TrafficLooper.hpp"
#include "include/api/RPC.h"
#include "include/ui/utils//MessageBoxTimer.h"
#include "3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.hpp"

#include <QInputDialog>
#include <QPushButton>
#include <QDesktopServices>
#include <QMessageBox>

#include "include/configs/generate.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"

#include "include/sys/Process.hpp"

// rpc

using namespace API;

void MainWindow::setup_rpc() {
    // Setup Connection
    defaultClient = new Client(
        [=](const QString &errStr) {
            MW_show_log("[Error] Core: " + errStr);
        },
        "127.0.0.1", Configs::dataManager->settingsRepo->core_port);

    // Looper
    runOnNewThread([=] { Stats::trafficLooper->Loop(); });
    runOnNewThread([=] {Stats::connection_lister->Loop(); });
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
            for (const auto& res : resp.results)
            {
                int entid = -1;
                if (!tag2entID.empty()) {
                    entid = tag2entID.count(QString::fromStdString(res.outbound_tag.value())) == 0 ? -1 : tag2entID[QString::fromStdString(res.outbound_tag.value())];
                }
                if (entid == -1) {
                    continue;
                }
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
                runOnUiThread([=,this]{
                    refresh_proxy_list();
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
        auto speedTestFunc = [=, this](const QList<std::shared_ptr<Configs::Profile>>& profileSlice) {
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
                    runURLTest(QJsonObject2QString(buildObject->coreConfig, false),QJsonObject2QString(buildObject->xrayConfig, false), false, buildObject->outboundTags, buildObject->tag2entID);
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
                refresh_proxy_list();
            });
        };
        for (int i=0;i<profileIDs.length();i+=100) {
            if (stopSpeedtest.load()) break;
            auto profileIDsSlice = profileIDs.mid(i, 100);
            auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(profileIDsSlice);
            speedTestFunc(profiles);
        }
        speedtestRunning.unlock();
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

void MainWindow::speedtest_current_group(const QList<int>& profileIDs, bool testCurrent)
{
    if (profileIDs.isEmpty() && !testCurrent) {
        return;
    }
    if (!speedtestRunning.tryLock()) {
        MessageBoxWarning(software_name, tr("The last speed test did not exit completely, please wait. If it persists, please restart the program."));
        return;
    }

    runOnNewThread([this, profileIDs, testCurrent]() {
        stopSpeedtest.store(false);
        if (!testCurrent)
        {
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
                    runSpeedTest(QJsonObject2QString(buildObject->coreConfig, false), QJsonObject2QString(buildObject->xrayConfig, false), false, false, buildObject->outboundTags, buildObject->tag2entID);
                }
            };
            for (int i=0;i<profileIDs.length();i+=100) {
                if (stopSpeedtest.load()) break;
                auto profileIDsSlice = profileIDs.mid(i, 100);
                auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(profileIDsSlice);
                speedTestFunc(profiles);
            }
        } else
        {
            runSpeedTest("", "", true, true, {}, {});
        }

        speedtestRunning.unlock();
        runOnUiThread([=,this]{
            refresh_proxy_list();
            MW_show_log(tr("Speedtest finished!"));
        });
    });
}

void MainWindow::querySpeedtest(QDateTime lastProxyListUpdate, const QMap<QString, int>& tag2entID, bool testCurrent)
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
    runOnUiThread([=, this, &lastProxyListUpdate]
    {
        showSpeedtestData = true;
        currentSptProfileName = profile->outbound->name;
        currentTestResult = res.result.value();
        UpdateDataView();

        if (res.result.value().error.value().empty() && !res.result.value().cancelled.value() && lastProxyListUpdate.msecsTo(QDateTime::currentDateTime()) >= 500)
        {
            if (!res.result.value().dl_speed.value().empty()) profile->dl_speed = QString::fromStdString(res.result.value().dl_speed.value());
            if (!res.result.value().ul_speed.value().empty()) profile->ul_speed = QString::fromStdString(res.result.value().ul_speed.value());
            if (profile->latency <= 0 && res.result.value().latency.value() > 0) profile->latency = res.result.value().latency.value();
            if (!res.result->server_country.value().empty()) profile->test_country = CountryNameToCode(QString::fromStdString(res.result.value().server_country.value()));
            refresh_proxy_list(profile->id);
            lastProxyListUpdate = QDateTime::currentDateTime();
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
                refresh_proxy_list(profile->id);
            }
        });
    }
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

    // loop query result
    auto doneMu = new QMutex;
    doneMu->lock();
    runOnNewThread([=,this]
    {
        QDateTime lastProxyListUpdate = QDateTime::currentDateTime();
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
                querySpeedtest(lastProxyListUpdate, tag2entID, testCurrent);
            }
        }
        runOnUiThread([=, this]
        {
            showSpeedtestData = false;
            UpdateDataView(true);
        });
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
        req.disable_stats = Configs::dataManager->settingsRepo->disable_traffic_stats;
        req.xray_config = QJsonObject2QString(result->xrayConfig, true).toStdString();
        req.need_xray = !result->xrayConfig.isEmpty();
        if (ent->type == "extracore")
        {
            req.need_extra_process = true;
            req.extra_process_path = result->extraCoreData->path.toStdString();
            req.extra_process_args = result->extraCoreData->args.toStdString();
            req.extra_process_conf = result->extraCoreData->config.toStdString();
            req.extra_process_conf_dir = result->extraCoreData->configDir.toStdString();
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
            runOnUiThread([=,this] { MessageBoxWarning("LoadConfig return error", error); });
            return false;
        }
        //
        Stats::trafficLooper->proxy = std::make_shared<Stats::TrafficData>("proxy");
        Stats::trafficLooper->direct = std::make_shared<Stats::TrafficData>("direct");
        Stats::trafficLooper->items = result->outboundStats;
        Stats::trafficLooper->isChain = ent->type == "chain";
        Stats::trafficLooper->loop_enabled = true;
        Stats::connection_lister->suspend = false;

        Configs::dataManager->settingsRepo->UpdateStartedId(ent->id);
        running = ent;

        runOnUiThread([=, this] {
            refresh_status();
            refresh_proxy_list(ent->id);
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
            runOnUiThread([=, this]
            {
                profile_stop(false, false, true);
            }, true);
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
        runOnUiThread([=,this] {
            restartMsgboxTimer->cancel();
            restartMsgboxTimer->deleteLater();
            restartMsgbox->deleteLater();
        });
    });
}

void MainWindow::set_spmode_system_proxy(bool enable, bool save) {
    if (enable != Configs::dataManager->settingsRepo->spmode_system_proxy) {
        if (enable) {
            auto socks_port = Configs::dataManager->settingsRepo->inbound_socks_port;
            SetSystemProxy(socks_port, socks_port, Configs::dataManager->settingsRepo->proxy_scheme);
        } else {
            ClearSystemProxy();
        }
    }

    if (save) {
        Configs::dataManager->settingsRepo->remember_spmode.removeAll("system_proxy");
        if (enable && Configs::dataManager->settingsRepo->remember_enable) {
            Configs::dataManager->settingsRepo->remember_spmode.append("system_proxy");
        }
        Configs::dataManager->settingsRepo->Save();
    }

    Configs::dataManager->settingsRepo->spmode_system_proxy = enable;
    refresh_status();
}

void MainWindow::profile_stop(bool crash, bool block, bool manual) {
    if (running == nullptr) {
        return;
    }
    auto id = running->id;

    auto profile_stop_stage2 = [=,this] {
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
        return true;
    };

    if (!mu_stopping.tryLock()) {
        return;
    }
    QMutex blocker;
    if (block) blocker.lock();

    // timeout message
    auto restartMsgbox = new QMessageBox(QMessageBox::Question, software_name, tr("If there is no response for a long time, it is recommended to restart the software."),
                                         QMessageBox::Yes | QMessageBox::No, this);
    connect(restartMsgbox, &QMessageBox::accepted, this, [=,this] { MW_dialog_message("", "RestartProgram"); });
    auto restartMsgboxTimer = new MessageBoxTimer(this, restartMsgbox, 5000);

    Stats::trafficLooper->loop_enabled = false;
    Stats::connection_lister->suspend = true;
    UpdateConnectionListWithRecreate({});
    Stats::trafficLooper->loop_mutex.lock();
    Stats::trafficLooper->UpdateAll();
    for (const auto &item: Stats::trafficLooper->items) {
        if (item->id < 0) continue;
        Configs::dataManager->profilesRepo->Save(Configs::dataManager->profilesRepo->GetProfile(item->id));
        refresh_proxy_list(item->id);
    }
    Stats::trafficLooper->loop_mutex.unlock();

    restartMsgboxTimer->cancel();
    restartMsgboxTimer->deleteLater();
    restartMsgbox->deleteLater();

    runOnNewThread([=, this, &blocker] {
        // do stop
        MW_show_log(">>>>>>>> " + tr("Stopping profile %1").arg(running->outbound->DisplayTypeAndName()));
        if (!profile_stop_stage2()) {
            MW_show_log("<<<<<<<< " + tr("Failed to stop, please restart the program."));
        }

        if (manual) Configs::dataManager->settingsRepo->UpdateStartedId(-1919);
        Configs::dataManager->settingsRepo->need_keep_vpn_off = false;
        running = nullptr;

        if (block) blocker.unlock();

        runOnUiThread([=, this] {
            refresh_status();
            refresh_proxy_list_impl_refresh_data(id, true);

            mu_stopping.unlock();
        });
    });

    if (block)
    {
        blocker.lock();
        blocker.unlock();
    }
}
