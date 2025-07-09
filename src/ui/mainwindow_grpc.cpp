#include "include/ui/mainwindow.h"

#include "include/dataStore/Database.hpp"
#include "include/configs/ConfigBuilder.hpp"
#include "include/stats/traffic/TrafficLooper.hpp"
#include "include/api/gRPC.h"
#include "include/ui/utils//MessageBoxTimer.h"
#include "3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.hpp"

#include <QInputDialog>
#include <QPushButton>
#include <QDesktopServices>
#include <QMessageBox>

// grpc

using namespace NekoGui_rpc;

void MainWindow::setup_grpc() {
    // Setup Connection
    defaultClient = new Client(
        [=](const QString &errStr) {
            MW_show_log("[Error] Core: " + errStr);
        },
        "127.0.0.1:" + Int2String(NekoGui::dataStore->core_port));

    // Looper
    runOnNewThread([=] { NekoGui_traffic::trafficLooper->Loop(); });
    runOnNewThread([=] {NekoGui_traffic::connection_lister->Loop(); });
}

void MainWindow::runURLTest(const QString& config, bool useDefault, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID) {
    if (stopSpeedtest.load()) {
        MW_show_log(tr("Profile test aborted"));
        return;
    }

    libcore::TestReq req;
    for (const auto &item: outboundTags) {
        req.add_outbound_tags(item.toStdString());
    }
    req.set_config(config.toStdString());
    req.set_url(NekoGui::dataStore->test_latency_url.toStdString());
    req.set_use_default_outbound(useDefault);
    req.set_max_concurrency(NekoGui::dataStore->test_concurrent);

    auto done = new QMutex;
    done->lock();
    runOnNewThread([=]
    {
        bool ok;
        while (true)
        {
            QThread::msleep(1500);
            if (done->try_lock()) break;
            auto resp = defaultClient->QueryURLTest(&ok);
            if (!ok || resp.results().empty())
            {
                continue;
            }

            bool needRefresh = false;
            for (const auto& res : resp.results())
            {
                int entid = -1;
                if (!tag2entID.empty()) {
                    entid = tag2entID.count(QString(res.outbound_tag().c_str())) == 0 ? -1 : tag2entID[QString(res.outbound_tag().c_str())];
                }
                if (entid == -1) {
                    continue;
                }
                auto ent = NekoGui::profileManager->GetProfile(entid);
                if (ent == nullptr) {
                    continue;
                }
                if (res.error().empty()) {
                ent->latency = res.latency_ms();
                } else {
                    if (QString(res.error().c_str()).contains("test aborted") ||
                        QString(res.error().c_str()).contains("context canceled")) ent->latency=0;
                    else {
                        ent->latency = -1;
                        MW_show_log(tr("[%1] test error: %2").arg(ent->bean->DisplayTypeAndName(), res.error().c_str()));
                    }
                }
                ent->Save();
                needRefresh = true;
            }
            if (needRefresh)
            {
                runOnUiThread([=]{
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
    if (!rpcOK) return;

    for (const auto &res: result.results()) {
        if (!tag2entID.empty()) {
            entID = tag2entID.count(QString(res.outbound_tag().c_str())) == 0 ? -1 : tag2entID[QString(res.outbound_tag().c_str())];
        }
        if (entID == -1) {
            MW_show_log(tr("Something is very wrong, the subject ent cannot be found!"));
            continue;
        }

        auto ent = NekoGui::profileManager->GetProfile(entID);
        if (ent == nullptr) {
            MW_show_log(tr("Profile manager data is corrupted, try again."));
            continue;
        }

        if (res.error().empty()) {
            ent->latency = res.latency_ms();
        } else {
            if (QString(res.error().c_str()).contains("test aborted") ||
                QString(res.error().c_str()).contains("context canceled")) ent->latency=0;
            else {
                ent->latency = -1;
                MW_show_log(tr("[%1] test error: %2").arg(ent->bean->DisplayTypeAndName(), res.error().c_str()));
            }
        }
        ent->Save();
    }
}

void MainWindow::urltest_current_group(const QList<std::shared_ptr<NekoGui::ProxyEntity>>& profiles) {
    if (profiles.isEmpty()) {
        return;
    }
    if (!speedtestRunning.tryLock()) {
        MessageBoxWarning(software_name, tr("The last url test did not exit completely, please wait. If it persists, please restart the program."));
        return;
    }

    runOnNewThread([this, profiles]() {
        auto buildObject = NekoGui::BuildTestConfig(profiles);
        if (!buildObject->error.isEmpty()) {
            MW_show_log(tr("Failed to build test config: ") + buildObject->error);
            speedtestRunning.unlock();
            return;
        }

        std::atomic<int> counter(0);
        stopSpeedtest.store(false);
        auto testCount = buildObject->fullConfigs.size() + (!buildObject->outboundTags.empty());
        for (const auto &entID: buildObject->fullConfigs.keys()) {
            auto configStr = buildObject->fullConfigs[entID];
            auto func = [this, &counter, testCount, configStr, entID]() {
                MainWindow::runURLTest(configStr, true, {}, {}, entID);
                counter++;
                if (counter.load() == testCount) {
                    speedtestRunning.unlock();
                }
            };
            speedTestThreadPool->start(func);
        }

        if (!buildObject->outboundTags.empty()) {
            auto func = [this, &buildObject, &counter, testCount]() {
                MainWindow::runURLTest(QJsonObject2QString(buildObject->coreConfig, false), false, buildObject->outboundTags, buildObject->tag2entID);
                counter++;
                if (counter.load() == testCount) {
                    speedtestRunning.unlock();
                }
            };
            speedTestThreadPool->start(func);
        }
        if (testCount == 0) speedtestRunning.unlock();

        speedtestRunning.lock();
        speedtestRunning.unlock();
        runOnUiThread([=]{
            refresh_proxy_list();
            MW_show_log(tr("URL test finished!"));
        });
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
    last_test_time = QTime::currentTime();
    ui->label_running->setText(tr("Testing"));

    runOnNewThread([=] {
        libcore::TestReq req;
        req.set_test_current(true);
        req.set_url(NekoGui::dataStore->test_latency_url.toStdString());

        bool rpcOK;
        auto result = defaultClient->Test(&rpcOK, req);
        if (!rpcOK) return;

        auto latency = result.results()[0].latency_ms();
        last_test_time = QTime::currentTime();

        runOnUiThread([=] {
            if (!result.results()[0].error().empty()) {
                MW_show_log(QString("UrlTest error: %1").arg(result.results()[0].error().c_str()));
            }
            if (latency <= 0) {
                ui->label_running->setText(tr("Test Result") + ": " + tr("Unavailable"));
            } else if (latency > 0) {
                ui->label_running->setText(tr("Test Result") + ": " + QString("%1 ms").arg(latency));
            }
        });
    });
}

void MainWindow::speedtest_current_group(const QList<std::shared_ptr<NekoGui::ProxyEntity>>& profiles, bool testCurrent)
{
    if (profiles.isEmpty() && !testCurrent) {
        return;
    }
    if (!speedtestRunning.tryLock()) {
        MessageBoxWarning(software_name, tr("The last speed test did not exit completely, please wait. If it persists, please restart the program."));
        return;
    }

    runOnNewThread([this, profiles, testCurrent]() {
        if (!testCurrent)
        {
            auto buildObject = NekoGui::BuildTestConfig(profiles);
            if (!buildObject->error.isEmpty()) {
                MW_show_log(tr("Failed to build test config: ") + buildObject->error);
                speedtestRunning.unlock();
                return;
            }

            stopSpeedtest.store(false);
            for (const auto &entID: buildObject->fullConfigs.keys()) {
                auto configStr = buildObject->fullConfigs[entID];
                runSpeedTest(configStr, true, false, {}, {}, entID);
            }

            if (!buildObject->outboundTags.empty()) {
                runSpeedTest(QJsonObject2QString(buildObject->coreConfig, false), false, false, buildObject->outboundTags, buildObject->tag2entID);
            }
        } else
        {
            stopSpeedtest.store(false);
            runSpeedTest("", true, true, {}, {});
        }

        speedtestRunning.unlock();
        runOnUiThread([=]{
            refresh_proxy_list();
            MW_show_log(tr("Speedtest finished!"));
        });
    });
}

void MainWindow::runSpeedTest(const QString& config, bool useDefault, bool testCurrent, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID)
{
    if (stopSpeedtest.load()) {
        MW_show_log(tr("Profile speed test aborted"));
        return;
    }

    libcore::SpeedTestRequest req;
    auto speedtestConf = NekoGui::dataStore->speed_test_mode;
    for (const auto &item: outboundTags) {
        req.add_outbound_tags(item.toStdString());
    }
    req.set_config(config.toStdString());
    req.set_use_default_outbound(useDefault);
    req.set_test_download(speedtestConf == NekoGui::TestConfig::FULL || speedtestConf == NekoGui::TestConfig::DL);
    req.set_test_upload(speedtestConf == NekoGui::TestConfig::FULL || speedtestConf == NekoGui::TestConfig::UL);
    req.set_simple_download(speedtestConf == NekoGui::TestConfig::SIMPLEDL);
    req.set_simple_download_addr(NekoGui::dataStore->simple_dl_url.toStdString());
    req.set_test_current(testCurrent);

    // loop query result
    auto doneMu = new QMutex;
    doneMu->lock();
    runOnNewThread([=]
    {
        QDateTime lastProxyListUpdate = QDateTime::currentDateTime();
        bool ok;
        while (true) {
            QThread::msleep(100);
            if (doneMu->tryLock())
            {
                break;
            }
            auto res = defaultClient->QueryCurrentSpeedTests(&ok);
            if (!ok || !res.is_running())
            {
                continue;
            }
            auto profile = testCurrent ? running : NekoGui::profileManager->GetProfile(tag2entID[res.result().outbound_tag().c_str()]);
            if (profile == nullptr)
            {
                continue;
            }
            runOnUiThread([=, &lastProxyListUpdate]
            {
                showSpeedtestData = true;
                currentSptProfileName = profile->bean->name;
                currentTestResult = res.result();
                UpdateDataView();

                if (res.result().error().empty() && !res.result().cancelled() && lastProxyListUpdate.msecsTo(QDateTime::currentDateTime()) >= 500)
                {
                    if (!res.result().dl_speed().empty()) profile->dl_speed = res.result().dl_speed().c_str();
                    if (!res.result().ul_speed().empty()) profile->ul_speed = res.result().ul_speed().c_str();
                    if (profile->latency <= 0 && res.result().latency() > 0) profile->latency = res.result().latency();
                    refresh_proxy_list(profile->id);
                    lastProxyListUpdate = QDateTime::currentDateTime();
                }
            });
        }
        runOnUiThread([=]
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
    if (!rpcOK) return;

    for (const auto &res: result.results()) {
        if (testCurrent) entID = running ? running->id : -1;
        else {
            entID = tag2entID.count(QString(res.outbound_tag().c_str())) == 0 ? -1 : tag2entID[QString(res.outbound_tag().c_str())];
        }
        if (entID == -1) {
            MW_show_log(tr("Something is very wrong, the subject ent cannot be found!"));
            continue;
        }

        auto ent = NekoGui::profileManager->GetProfile(entID);
        if (ent == nullptr) {
            MW_show_log(tr("Profile manager data is corrupted, try again."));
            continue;
        }

        if (res.cancelled()) continue;

        if (res.error().empty()) {
            ent->dl_speed = res.dl_speed().c_str();
            ent->ul_speed = res.ul_speed().c_str();
            if (ent->latency <= 0 && res.latency() > 0) ent->latency = res.latency();
        } else {
            ent->dl_speed = "N/A";
            ent->ul_speed = "N/A";
            ent->latency = -1;
            MW_show_log(tr("[%1] speed test error: %2").arg(ent->bean->DisplayTypeAndName(), res.error().c_str()));
        }
        ent->Save();
    }
}

bool MainWindow::set_system_dns(bool set, bool save_set) {
    if (!NekoGui::dataStore->enable_dns_server) {
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
    if (save_set) NekoGui::dataStore->system_dns_set = set;
    return true;
}

void MainWindow::neko_start(int _id) {
    if (NekoGui::dataStore->prepare_exit) return;
#ifdef Q_OS_LINUX
    if (NekoGui::dataStore->enable_dns_server && NekoGui::dataStore->dns_server_listen_port <= 1024) {
        if (!get_elevated_permissions()) {
            MW_show_log(QString("Failed to get admin access, cannot listen on port %1 without it").arg(NekoGui::dataStore->dns_server_listen_port));
            return;
        }
    }
#endif

    auto ents = get_now_selected_list();
    auto ent = (_id < 0 && !ents.isEmpty()) ? ents.first() : NekoGui::profileManager->GetProfile(_id);
    if (ent == nullptr) return;

    if (select_mode) {
        emit profile_selected(ent->id);
        select_mode = false;
        refresh_status();
        return;
    }

    auto group = NekoGui::profileManager->GetGroup(ent->gid);
    if (group == nullptr || group->archive) return;

    auto result = BuildConfig(ent, false, false);
    if (!result->error.isEmpty()) {
        MessageBoxWarning(tr("BuildConfig return error"), result->error);
        return;
    }

    auto neko_start_stage2 = [=] {
        libcore::LoadConfigReq req;
        req.set_core_config(QJsonObject2QString(result->coreConfig, true).toStdString());
        req.set_disable_stats(NekoGui::dataStore->disable_traffic_stats);
        if (ent->type == "extracore")
        {
            req.set_need_extra_process(true);
            req.set_extra_process_path(result->extraCoreData->path.toStdString());
            req.set_extra_process_args(result->extraCoreData->args.toStdString());
            req.set_extra_process_conf(result->extraCoreData->config.toStdString());
            req.set_extra_process_conf_dir(result->extraCoreData->configDir.toStdString());
            req.set_extra_no_out(result->extraCoreData->noLog);
        }
        //
        bool rpcOK;
        QString error = defaultClient->Start(&rpcOK, req);
        if (!rpcOK) {
            return false;
        }
        if (!error.isEmpty()) {
            if (error.contains("configure tun interface")) {
                runOnUiThread([=] {

                    QMessageBox msg(
                        QMessageBox::Information,
                        tr("Tun device misbehaving"),
                        tr("If you have trouble starting VPN, you can force reset nekobox_core process here and then try starting the profile again. The error is %1").arg(error),
                        QMessageBox::NoButton,
                        this
                    );
                    msg.addButton(tr("Reset"), QMessageBox::ActionRole);
                    auto cancel = msg.addButton(tr("Cancel"), QMessageBox::ActionRole);

                    msg.setDefaultButton(cancel);
                    msg.setEscapeButton(cancel);

                    int r = msg.exec() - 2;
                    if (r == 0) {
                        GetMainWindow()->StopVPNProcess();
                    }
                });
                return false;
            }
            runOnUiThread([=] { MessageBoxWarning("LoadConfig return error", error); });
            return false;
        }
        //
        NekoGui_traffic::trafficLooper->proxy = std::make_shared<NekoGui_traffic::TrafficData>("proxy");
        NekoGui_traffic::trafficLooper->direct = std::make_shared<NekoGui_traffic::TrafficData>("direct");
        NekoGui_traffic::trafficLooper->items = result->outboundStats;
        NekoGui_traffic::trafficLooper->isChain = ent->type == "chain";
        NekoGui_traffic::trafficLooper->loop_enabled = true;
        NekoGui_traffic::connection_lister->suspend = false;

        NekoGui::dataStore->UpdateStartedId(ent->id);
        running = ent;

        runOnUiThread([=] {
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
    if (!NekoGui::dataStore->core_running) {
        runOnUiThread(
            [=] {
                MW_show_log(tr("Try to start the config, but the core has not listened to the grpc port, so restart it..."));
                core_process->start_profile_when_core_is_up = ent->id;
                core_process->Restart();
            },
            DS_cores);
        mu_starting.unlock();
        return; // let CoreProcess call neko_start when core is up
    }

    // timeout message
    auto restartMsgbox = new QMessageBox(QMessageBox::Question, software_name, tr("If there is no response for a long time, it is recommended to restart the software."),
                                         QMessageBox::Yes | QMessageBox::No, this);
    connect(restartMsgbox, &QMessageBox::accepted, this, [=] { MW_dialog_message("", "RestartProgram"); });
    auto restartMsgboxTimer = new MessageBoxTimer(this, restartMsgbox, 5000);

    runOnNewThread([=] {
        // stop current running
        if (running != nullptr) {
            runOnUiThread([=] { neko_stop(false, true, true); });
            sem_stopped.acquire();
        }
        // do start
        MW_show_log(">>>>>>>> " + tr("Starting profile %1").arg(ent->bean->DisplayTypeAndName()));
        if (!neko_start_stage2()) {
            MW_show_log("<<<<<<<< " + tr("Failed to start profile %1").arg(ent->bean->DisplayTypeAndName()));
        }
        mu_starting.unlock();
        // cancel timeout
        runOnUiThread([=] {
            restartMsgboxTimer->cancel();
            restartMsgboxTimer->deleteLater();
            restartMsgbox->deleteLater();
#ifdef Q_OS_LINUX
            // Check systemd-resolved
            if (NekoGui::dataStore->spmode_vpn && NekoGui::dataStore->routing->direct_dns.startsWith("local") && ReadFileText("/etc/resolv.conf").contains("systemd-resolved")) {
                MW_show_log("[Warning] The default Direct DNS may not works with systemd-resolved, you may consider change your DNS settings.");
            }
#endif
        });
    });
}

void MainWindow::neko_set_spmode_system_proxy(bool enable, bool save) {
    if (enable != NekoGui::dataStore->spmode_system_proxy) {
        if (enable) {
            auto socks_port = NekoGui::dataStore->inbound_socks_port;
            SetSystemProxy(socks_port, socks_port, NekoGui::dataStore->proxy_scheme);
        } else {
            ClearSystemProxy();
        }
    }

    if (save) {
        NekoGui::dataStore->remember_spmode.removeAll("system_proxy");
        if (enable && NekoGui::dataStore->remember_enable) {
            NekoGui::dataStore->remember_spmode.append("system_proxy");
        }
        NekoGui::dataStore->Save();
    }

    NekoGui::dataStore->spmode_system_proxy = enable;
    refresh_status();
}

void MainWindow::neko_stop(bool crash, bool sem, bool manual) {
    auto id = NekoGui::dataStore->started_id;
    if (id < 0) {
        if (sem) sem_stopped.release();
        return;
    }

    auto neko_stop_stage2 = [=] {
        if (!crash) {
            bool rpcOK;
            QString error = defaultClient->Stop(&rpcOK);
            if (rpcOK && !error.isEmpty()) {
                runOnUiThread([=] { MessageBoxWarning(tr("Stop return error"), error); });
                return false;
            } else if (!rpcOK) {
                return false;
            }
        }
        return true;
    };

    if (!mu_stopping.tryLock()) {
        if (sem) sem_stopped.release();
        return;
    }

    // timeout message
    auto restartMsgbox = new QMessageBox(QMessageBox::Question, software_name, tr("If there is no response for a long time, it is recommended to restart the software."),
                                         QMessageBox::Yes | QMessageBox::No, this);
    connect(restartMsgbox, &QMessageBox::accepted, this, [=] { MW_dialog_message("", "RestartProgram"); });
    auto restartMsgboxTimer = new MessageBoxTimer(this, restartMsgbox, 5000);

    NekoGui_traffic::trafficLooper->loop_enabled = false;
    NekoGui_traffic::connection_lister->suspend = true;
    UpdateConnectionListWithRecreate({});
    NekoGui_traffic::trafficLooper->loop_mutex.lock();
    NekoGui_traffic::trafficLooper->UpdateAll();
    for (const auto &item: NekoGui_traffic::trafficLooper->items) {
        if (item->id < 0) continue;
        NekoGui::profileManager->GetProfile(item->id)->Save();
        refresh_proxy_list(item->id);
    }
    NekoGui_traffic::trafficLooper->loop_mutex.unlock();

    restartMsgboxTimer->cancel();
    restartMsgboxTimer->deleteLater();
    restartMsgbox->deleteLater();

    runOnNewThread([=] {
        // do stop
        MW_show_log(">>>>>>>> " + tr("Stopping profile %1").arg(running->bean->DisplayTypeAndName()));
        if (!neko_stop_stage2()) {
            MW_show_log("<<<<<<<< " + tr("Failed to stop, please restart the program."));
        }

        if (manual) NekoGui::dataStore->UpdateStartedId(-1919);
        NekoGui::dataStore->need_keep_vpn_off = false;
        running = nullptr;

        if (sem) sem_stopped.release();

        runOnUiThread([=] {
            refresh_status();
            refresh_proxy_list_impl_refresh_data(id, true);

            mu_stopping.unlock();
        });
    });
}