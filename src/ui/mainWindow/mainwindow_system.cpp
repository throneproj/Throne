#include "include/ui/mainwindow.h"
#include "NkrVersion.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>

#include "3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.hpp"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/global/Logger.hpp"
#include "include/sys/Process.hpp"
#include "include/ui/mainWindow/MainWindowInternal.h"

#include "include/ui/group/dialog_manage_groups.h"
#include "include/ui/setting/dialog_basic_settings.h"
#include "include/ui/setting/dialog_hotkey.h"
#include "include/ui/setting/dialog_manage_routes.h"
#include "include/ui/setting/dialog_vpn_settings.h"

#ifdef Q_OS_WIN
#include "3rdparty/WinCommander.hpp"
#include "include/sys/windows/WinVersion.h"
#endif
#ifdef Q_OS_LINUX
#include "include/sys/linux/LinuxCap.h"
#endif
#ifdef Q_OS_MACOS
#include "include/sys/macos/MacOS.h"
#endif

qint64 MainWindow::GetCorePid() {
    QMutexLocker lock(&coreProcessMutex);
    return core_process ? core_process->processId() : 0;
}

QString MainWindow::GetRunningConfigName() {
    auto ent = running;
    if (ent == nullptr || ent->outbound == nullptr) return {};
    return ent->outbound->DisplayTypeAndName();
}

void MainWindow::on_menu_basic_settings_triggered() {
    USE_DIALOG(DialogBasicSettings)
}

void MainWindow::on_menu_manage_groups_triggered() {
    USE_DIALOG(DialogManageGroups)
}

void MainWindow::on_menu_routing_settings_triggered() {
    if (dialog_is_using) return;
    dialog_is_using = true;
    auto dialog = new DialogManageRoutes(this);
    connect(dialog, &QDialog::finished, this, [=,this] {
        dialog->deleteLater();
        dialog_is_using = false;
    });
    dialog->show();
}

void MainWindow::on_menu_vpn_settings_triggered() {
    USE_DIALOG(DialogVPNSettings)
}

void MainWindow::on_menu_hotkey_settings_triggered() {
    if (dialog_is_using) return;
    dialog_is_using = true;
    auto dialog = new DialogHotkey(this, getActionsForShortcut());
    connect(dialog, &QDialog::finished, this, [=,this]
    {
        dialog->deleteLater();
        dialog_is_using = false;
    });
    dialog->show();
}

void MainWindow::on_commitDataRequest() {
    qDebug() << "Start of data save";

    auto* settings = Configs::dataManager->settingsRepo.get();

    settings->mainWindowGeometry = this->saveGeometry().toBase64(QByteArray::Base64Encoding);
    if (!isMaximized()) {
        auto news = QString("%1x%2").arg(size().width()).arg(size().height());
        if (settings->mw_size != news) settings->mw_size = news;
    }
    settings->splitter_state = ui->splitter->saveState().toBase64();

    // Snapshot on exit rather than per toggle, so "remember last proxy" is independent
    // of the order the user flipped the proxy/tun modes.
    if (settings->remember_enable) {
        if (settings->started_id >= 0) settings->remember_id = settings->started_id;
        settings->remember_system_proxy = settings->spmode_system_proxy;
        settings->remember_tun = settings->spmode_vpn;
    } else {
        settings->remember_system_proxy = false;
        settings->remember_tun = false;
    }

    settings->Save();
    qDebug() << "End of data save";
}

bool MainWindow::prepare_exit()
{
    qDebug() << "prepare for exit...";
    mu_exit.lock();
    if (Configs::dataManager->settingsRepo->prepare_exit)
    {
        qDebug() << "prepare exit had already succeeded, ignoring...";
        mu_exit.unlock();
        return true;
    }

    QString killSwitchError;
    if (!prepareKillSwitchExit(&killSwitchError)) {
        mu_exit.unlock();
        MW_show_log(tr("Exit cancelled because fail-closed protection could not be prepared: %1")
                        .arg(killSwitchError));
        MessageBoxWarning(
            tr("Kill switch blocked exit"),
            tr("Throne could not safely remove the active TUN allowance before exit. "
               "The application will stay open.\n\n%1")
                .arg(killSwitchError));
        return false;
    }
    Configs::dataManager->settingsRepo->prepare_exit = true;
    LOG_INFO("prepare_exit started, tearing down proxy/tun/core");
    //
    if (Configs::dataManager->settingsRepo->spmode_system_proxy) set_system_proxy(false);
    if (Configs::dataManager->settingsRepo->system_dns_set) set_system_dns(false, false);
    RegisterHiddenMenuShortcuts(true);
    RegisterHotkey(true);
    //
    on_commitDataRequest();
    //
    Configs::dataManager->settingsRepo->noSave = true; // don't change Configs::dataManager->settingsRepo after this line
    profile_stop(false, true);

    runOnThread([=, this]()
    {
        core_process->Kill();
    }, DS_cores, true);
    HideWindow(this);
    tray->hide();

    mu_exit.unlock();
    qDebug() << "prepare exit done!";
    return true;
}

void MainWindow::on_menu_exit_triggered() {
    if (!prepare_exit()) return;
    //
    if (exit_reason == ExitReason::RunUpdater) {
        QDir::setCurrent(QApplication::applicationDirPath());
#ifdef Q_OS_WIN
        QFile::copy("./updater.exe", "./updater.old");
        QProcess::startDetached("./updater.old", QStringList{});
#else
        QProcess::startDetached("./updater", QStringList{});
#endif
    } else if (exit_reason == ExitReason::Restart || exit_reason == ExitReason::RestartWithTun || exit_reason == ExitReason::RestartWithDns) {
        QDir::setCurrent(QApplication::applicationDirPath());

        auto arguments = Configs::dataManager->settingsRepo->argv;
        if (arguments.length() > 0) {
            arguments.removeFirst();
            arguments.removeAll("-tray");
            arguments.removeAll("-flag_restart_tun_on");
            arguments.removeAll("-flag_restart_dns_set");
        }
        auto program = QApplication::applicationFilePath();

        if (exit_reason == ExitReason::RestartWithTun || exit_reason == ExitReason::RestartWithDns) {
            if (exit_reason == ExitReason::RestartWithTun) arguments << "-flag_restart_tun_on";
            if (exit_reason == ExitReason::RestartWithDns) arguments << "-flag_restart_dns_set";
#ifdef Q_OS_WIN
            WinCommander::runProcessElevated(program, arguments, "", 1, false);
#else
            QProcess::startDetached(program, arguments);
#endif
        } else {
            QProcess::startDetached(program, arguments);
        }
    }
    QCoreApplication::quit();
}

void MainWindow::toggle_system_proxy() {
    auto currentState = Configs::dataManager->settingsRepo->spmode_system_proxy;
    if (currentState) {
        set_spmode_system_proxy(false);
    } else {
        set_spmode_system_proxy(true);
    }
}

bool MainWindow::get_elevated_permissions(ExitReason reason) {
    if (Configs::dataManager->settingsRepo->disable_privilege_req)
    {
        MW_show_log(tr("User opted for no privilege req, some features may not work"));
        return true;
    }
    if (Configs::IsAdmin()) return true;
#ifdef Q_OS_LINUX
    if (!Linux_HavePkexec()) {
        MessageBoxWarning(software_name, "Please install \"pkexec\" first.");
        return false;
    }
    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please give the core root privileges"), QMessageBox::Yes | QMessageBox::No);
    if (n == QMessageBox::Yes) {
        runOnNewThread([=,this]
        {
            auto chownArgs = QString("root:root " + Configs::FindCoreRealPath());
            auto ret = Linux_Run_Command("chown", chownArgs);
            if (ret != 0) {
                MW_show_log(QString("Failed to run chown %1 code is %2").arg(chownArgs).arg(ret));
            }
            auto chmodArgs = QString("u+s " + Configs::FindCoreRealPath());
            ret = Linux_Run_Command("chmod", chmodArgs);
            if (ret == 0) {
                StopVPNProcess();
            } else {
                MW_show_log(QString("Failed to run chmod %1").arg(chmodArgs));
            }
        });
        return false;
    }
#endif
#ifdef Q_OS_WIN
    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please run Throne as admin"), QMessageBox::Yes | QMessageBox::No);
    if (n == QMessageBox::Yes) {
        this->exit_reason = reason;
        on_menu_exit_triggered();
    }
#endif

#ifdef Q_OS_MACOS
    if (Configs::isSetuidSet(Configs::FindCoreRealPath().toStdString()))
    {
        StopVPNProcess();
        return true;
    }
    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please give the core root privileges"), QMessageBox::Yes | QMessageBox::No);
    if (n == QMessageBox::Yes)
    {
        auto Command = QString("sudo chown root:wheel '%1' && sudo chmod u+s '%1'").arg(Configs::FindCoreRealPath());
        auto ret = Mac_Run_Command(Command);
        if (ret == 0) {
            MessageBoxInfo(tr("Requesting permission"), tr("Please Enter your password in the opened terminal, then try again"));
            return false;
        } else {
            MW_show_log(QString("Failed to run %1 with %2").arg(Command).arg(ret));
            return false;
        }
    }
#endif
    return false;
}

void MainWindow::set_system_proxy(bool enable) {
    if (enable) {
        auto socks_port = Configs::dataManager->settingsRepo->inbound_socks_port;
        SetSystemProxy(socks_port, socks_port, Configs::dataManager->settingsRepo->proxy_scheme);
    } else {
        ClearSystemProxy();
    }
}

void MainWindow::set_spmode_system_proxy(bool enable, bool save) {
    if (enable && Configs::dataManager->settingsRepo->disable_mixed_inbound) {
        runOnUiThread([=, this] {
           MessageBoxWarning("Invalid Operation", "Cannot set system proxy when mixed inbound is disabled.");
        });
        ui->checkBox_SystemProxy->setChecked(false);
        return;
    }
    Configs::dataManager->settingsRepo->spmode_system_proxy = enable;
    if (running) {
        set_system_proxy(enable);
        if (!enable && Configs::dataManager->settingsRepo->reset_proxy_on_disable_sp) {
            profile_start(running->id);
        }
    }

    if (save) {
        Configs::dataManager->settingsRepo->Save();
    }

    refresh_status();
}

void MainWindow::set_spmode_vpn(bool enable, bool save) {
    if (enable == Configs::dataManager->settingsRepo->spmode_vpn) return;

    if (enable) {
        bool requestPermission = !Configs::IsAdmin();
        if (requestPermission) {
            if (!get_elevated_permissions()) {
                refresh_status();
                return;
            }
        }
    }

    if (save) {
        Configs::dataManager->settingsRepo->Save();
    }

    Configs::dataManager->settingsRepo->spmode_vpn = enable;
    refresh_status();

    if (Configs::dataManager->settingsRepo->started_id >= 0) profile_start(Configs::dataManager->settingsRepo->started_id);
}

bool MainWindow::StopVPNProcess(const bool clearStartedProfile) {
    // Serialize a force-reset with the complete profile build-to-ready
    // interval. Otherwise a core could be killed after a config was built but
    // before its protected operation acquired the TUN allowance.
    if (!mu_starting.tryAcquire()) {
        MW_show_log(tr("Core reset refused while a profile is starting."));
        return false;
    }

    bool safelyPrepared = true;
    if (running != nullptr) {
        // A crash-style stop avoids relying on an unresponsive core RPC, but
        // still verifies the persistent baseline and removes the TUN allowance
        // before the process owning the adapter is terminated.
        safelyPrepared = profile_stop(true, true, clearStartedProfile);
    } else {
        QString killSwitchError;
        safelyPrepared = prepareKillSwitchProfileStop(&killSwitchError);
        if (safelyPrepared) {
            finishKillSwitchProfileStop();
            if (clearStartedProfile) {
                Configs::dataManager->settingsRepo->UpdateStartedId(Configs::NoProfileId);
            }
        } else {
            MW_show_log(tr("Core reset refused by the kill switch: %1")
                            .arg(killSwitchError));
        }
    }

    if (!safelyPrepared) {
        mu_starting.release();
        return false;
    }

    runOnThread([=, this]
    {
        core_process->Kill();
    }, DS_cores, true);

    mu_starting.release();
    return true;
}

namespace {

bool isNewer(QString assetName) {
    if (QString(NKR_VERSION).isEmpty()) return false;
    assetName = assetName.mid(7); // take out Throne-
    QString version;
    auto spl = assetName.split('-');
    version += spl[0]; // version: 1.2.3
    if (spl[1].contains("beta") || spl[1].contains("alpha") || spl[1].contains("rc")) version += "."+spl[1]; // .beta.13
    auto parts = version.split("."); // [1,2,3,beta,13]
    auto currentParts = QString(NKR_VERSION).replace("-", ".").split('.');
    if (parts.size() < 3 || currentParts.size() < 3)
    {
        MW_show_log("Version strings seem to be invalid" + QString(NKR_VERSION) + " and " + version);
        return false;
    }
    std::vector<int> verNums;
    std::vector<int> currNums;
    // add base version first
    verNums.push_back(parts[0].toInt());
    verNums.push_back(parts[1].toInt());
    verNums.push_back(parts[2].toInt());
    if (parts.size() > 3)
    {
        if (parts[3] == "alpha") verNums.push_back(1);
        if (parts[3] == "beta") verNums.push_back(2);
        if (parts[3] == "rc") verNums.push_back(3);
        if (parts.size() > 4) verNums.push_back(parts[4].toInt());
    }

    currNums.push_back(currentParts[0].toInt());
    currNums.push_back(currentParts[1].toInt());
    currNums.push_back(currentParts[2].toInt());
    if (currentParts.size() > 3)
    {
        if (currentParts[3] == "alpha") currNums.push_back(1);
        if (currentParts[3] == "beta") currNums.push_back(2);
        if (currentParts[3] == "rc") currNums.push_back(3);
        if (currentParts.size() > 4) currNums.push_back(currentParts[4].toInt());
    }

    if (verNums.size() < 3 || currNums.size() < 3)
    {
        MW_show_log("Version strings seem to be invalid" + QString(NKR_VERSION) + " and " + version);
        return false;
    }

    for (int i=0;i<3;i++)
    {
        if (verNums[i] > currNums[i]) return true;
        if (verNums[i] < currNums[i]) return false;
    }

    // equal base version, check beta-ness
    if (verNums.size() == 5 && currNums.size() == 3) return false;
    if (verNums.size() == 3 && currNums.size() == 5) return true;
    if (verNums.size() == 5 && currNums.size() == 5)
    {
        for (int i=3;i<5;i++)
        {
            if (verNums[i] > currNums[i]) return true;
            if (verNums[i] < currNums[i]) return false;
        }
    } else
    {
		MW_show_log("There are no updates. You have the latest version - " + QString(NKR_VERSION));
        return false;
    }
    return false;
}

} // namespace

void MainWindow::CheckUpdate() {
    QString search;
#ifdef Q_OS_WIN
#  ifdef Q_PROCESSOR_ARM_64
    search = "windows-arm64";
#  else
#    ifdef Q_OS_WIN64
        if (WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_10_1809))
            search = "windows64";
        else
	        search = "windowslegacy64";
#    else
	    search = "windows32";
#    endif
#  endif
#endif
#ifdef Q_OS_LINUX
#  ifdef Q_PROCESSOR_X86_64
    search = "linux-amd64";
#  else
    search = "linux-arm64";
#  endif
#endif
#ifdef Q_OS_MACOS
#  ifdef Q_PROCESSOR_X86_64
	search = "macos-amd64";
#  else
	search = "macos-arm64";
#  endif
#endif
    if (search.isEmpty()) {
        runOnUiThread([=,this] {
            MessageBoxWarning(QObject::tr("Update"), QObject::tr("Not official support platform"));
        });
        return;
    }

    auto resp = NetworkRequestHelper::HttpGet("https://api.github.com/repos/throneproj/Throne/releases");
    if (!resp.error.isEmpty()) {
        runOnUiThread([=,this] {
            MessageBoxWarning(QObject::tr("Update"), QObject::tr("Requesting update error: %1").arg(resp.error + "\n" + resp.data));
        });
        return;
    }

    QString assets_name, release_download_url, release_url, release_note, note_pre_release;
    bool exitFlag = false;
    QJsonArray array = QString2QJsonArray(resp.data);
    for (const QJsonValue value : array) {
        QJsonObject release = value.toObject();
        if (release["prerelease"].toBool() && !Configs::dataManager->settingsRepo->allow_beta_update) continue;
        for (const QJsonValue asset : release["assets"].toArray()) {
            if (asset["name"].toString().contains(search) && asset["name"].toString().section('.', -1) == QString("zip")) {
                note_pre_release = release["prerelease"].toBool() ? " (Pre-release)" : "";
                release_url = release["html_url"].toString();
                release_note = release["body"].toString();
                assets_name = asset["name"].toString();
                release_download_url = asset["browser_download_url"].toString();
                exitFlag = true;
                break;
            }
        }
        if (exitFlag) break;
    }

    if (release_download_url.isEmpty() || !isNewer(assets_name)) {
        runOnUiThread([=,this] {
            MessageBoxInfo(QObject::tr("Update"), QObject::tr("No update"));
        });
        return;
    }

    runOnUiThread([=,this] {
        auto allow_updater = !Configs::dataManager->settingsRepo->flag_use_appdata;
        QMessageBox box(QMessageBox::Question, QObject::tr("Update") + note_pre_release,
                        QObject::tr("Update found: %1\nRelease note:\n%2").arg(assets_name, release_note));
        //
        QAbstractButton *btn1 = nullptr;
        if (allow_updater) {
            btn1 = box.addButton(QObject::tr("Update"), QMessageBox::AcceptRole);
        }
        QAbstractButton *btn2 = box.addButton(QObject::tr("Open in browser"), QMessageBox::AcceptRole);
        box.addButton(QObject::tr("Close"), QMessageBox::RejectRole);
        box.exec();
        //
        if (btn1 == box.clickedButton() && allow_updater) {
            // Download Update
            runOnNewThread([=,this] {
                if (!mu_download_update.tryLock()) {
                    runOnUiThread([=,this](){
                        MessageBoxWarning(tr("Cannot start"), tr("Last download request has not finished yet"));
                    });
                    return;
                }
                QString errors;
                if (!release_download_url.isEmpty()) {
                    auto res = NetworkRequestHelper::DownloadAsset(release_download_url, "Throne.zip");
                    if (!res.isEmpty()) {
                        errors += res;
                    }
                }
                mu_download_update.unlock();
                runOnUiThread([=,this] {
                    if (errors.isEmpty()) {
                        auto q = QMessageBox::question(nullptr, QObject::tr("Update"),
                                                       QObject::tr("Update is ready, restart to install?"));
                        if (q == QMessageBox::StandardButton::Yes) {
                            this->exit_reason = ExitReason::RunUpdater;
                            on_menu_exit_triggered();
                        }
                    } else {
                        MessageBoxWarning(tr("Failed to download update assets"), errors);
                    }
                });
            });
        } else if (btn2 == box.clickedButton()) {
            QDesktopServices::openUrl(QUrl(release_url));
        }
    });
}
