#include "include/ui/mainwindow.h"

#include <QMessageBox>
#include <QPushButton>

#include <memory>
#include <utility>

#include "include/engine/app/ExitController.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/global/Logger.hpp"
#include "include/sys/CoreElevation.hpp"
#include "include/sys/KillSwitch.hpp"
#include "include/ui/profile/dialog_vpn_auth.h"
#include "include/ui/utils/RestartPrompt.h"

#ifdef Q_OS_LINUX
#include "include/sys/linux/LinuxCap.h"
#endif
#ifdef Q_OS_MACOS
#include "include/sys/macos/MacOS.h"
#endif

namespace {
    using PolicyBoxes = QHash<QString, QPointer<QMessageBox>>;

    // A closed box lingers until its deferred delete, so only a visible one counts as open.
    QMessageBox *policyOpenBox(const PolicyBoxes &boxes, const char *method) {
        const auto box = boxes.value(QString::fromLatin1(method));
        return box && box->isVisible() ? box.data() : nullptr;
    }

    void policyNotice(PolicyBoxes &boxes, const char *method, QWidget *parent, QMessageBox::Icon icon, const QString &title,
                      const QString &text) {
        if (auto *open = policyOpenBox(boxes, method)) {
            if (open->icon() != icon) open->setIcon(icon);
            if (open->windowTitle() != title) open->setWindowTitle(title);
            if (open->text() != text) open->setText(text);
            return;
        }
        auto *box = new QMessageBox(icon, title, text, QMessageBox::Ok, parent);
        box->setAttribute(Qt::WA_DeleteOnClose);
        boxes.insert(QString::fromLatin1(method), box);
        box->open();
    }

    // answered runs exactly once: true when accept was clicked, false however else the box goes away.
    void policyAsk(PolicyBoxes &boxes, const char *method, QMessageBox *box, const QAbstractButton *accept,
                   std::function<void(bool)> answered) {
        const auto pending = std::make_shared<std::function<void(bool)>>(std::move(answered));
        const auto answer = [pending](bool value) {
            if (const auto callback = std::exchange(*pending, {})) callback(value);
        };
        QObject::connect(box, &QMessageBox::finished, box, [box, accept, answer] { answer(box->clickedButton() == accept); });
        // A box deleted with its parent never emits finished.
        QObject::connect(box, &QObject::destroyed, [answer] { answer(false); });
        box->setAttribute(Qt::WA_DeleteOnClose);
        boxes.insert(QString::fromLatin1(method), box);
        box->open();
    }
} // namespace

#ifdef Q_OS_WIN
namespace {
    bool policyOfferElevatedRestart(QWidget *parent, const QString &text) {
        QMessageBox box(QMessageBox::Warning, software_name, text, QMessageBox::NoButton, parent);
        auto *restart = box.addButton(MainWindow::tr("Restart as administrator"), QMessageBox::AcceptRole);
        auto *cancel = box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(restart);
        box.setEscapeButton(cancel);
        box.exec();
        return box.clickedButton() == restart;
    }
} // namespace
#endif

void MainWindow::geoAssetMissingCategory(const QString &contextName, const QString &needed, const QString &whichFile) {
    policyNotice(m_policyBoxes, __func__, this, QMessageBox::Warning,
        tr("Geo asset missing category"),
        tr("The Xray config \"%1\" needs \"%2\", but the installed %3 does "
           "not contain it.\n\n"
           "Re-downloading from the same source will not fix this — the data "
           "file does not include that category. Set the GeoIP/GeoSite asset "
           "URL in Settings to a source that provides \"%2\", then delete %3 "
           "from the app folder and download it again.")
            .arg(contextName, needed, whichFile));
}

void MainWindow::confirmGeoAssetDownload(const QString &contextName, std::function<void(bool download)> answered) {
    if (policyOpenBox(m_policyBoxes, __func__) != nullptr) {
        answered(false);
        return;
    }
    auto *box = new QMessageBox(QMessageBox::Question, tr("Geo asset files required"),
            tr("The Xray config \"%1\" uses geoip/geosite routing rules, but the "
               "required data files (geoip.dat / geosite.dat) are not installed.\n\n"
               "Download them now?").arg(contextName),
            QMessageBox::Yes | QMessageBox::No, this);
    policyAsk(m_policyBoxes, __func__, box, box->button(QMessageBox::Yes), std::move(answered));
}

void MainWindow::geoAssetDownloadFinished(const QString &error) {
    if (!error.isEmpty()) {
        policyNotice(m_policyBoxes, __func__, this, QMessageBox::Warning, tr("Geo asset download failed"), error);
    } else {
        policyNotice(m_policyBoxes, __func__, this, QMessageBox::Information, tr("Geo assets installed"),
            tr("Geo assets were downloaded successfully.\n\n"
               "Please try again."));
    }
}

void MainWindow::killSwitchProblem() {
    auto *killSwitch = Sys::KillSwitch::instance();
    if (killSwitch->state() != Sys::KillSwitch::State::Failed) return;
    const bool privilege = killSwitch->failedForPrivileges();
    const bool identity = killSwitch->failedForIdentity();
    if (m_killSwitchDialog) {
        if (m_killSwitchDialog->property("privilege").toBool() == privilege && m_killSwitchDialog->property("identity").toBool() == identity) {
            m_killSwitchDialog->setText(killSwitch->failureText());
            return;
        }
        m_killSwitchDialog->close();
    }

    auto *box = new QMessageBox(QMessageBox::Warning, identity ? tr("Kill switch is blocking traffic") : tr("Kill switch is not active"),
                                killSwitch->failureText(), QMessageBox::NoButton, this);
    box->setTextFormat(Qt::PlainText);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setProperty("privilege", privilege);
    box->setProperty("identity", identity);
    // The guard still blocks and the core cannot join its group: neither a retry nor more privileges changes that.
    QPushButton *fix = nullptr;
#ifdef Q_OS_WIN
    // Only an administrator's own account can grant the core, and a grant that did not take effect is not fixed by another.
    const bool restartElevated = !Sys::CoreElevation::CanGrant() || Sys::CoreElevation::GrantIneffective();
#else
    const bool restartElevated = false;
#endif
    if (!identity) {
        fix = box->addButton(!privilege ? tr("Retry") : restartElevated ? tr("Restart as administrator") : tr("Grant privileges"),
                             QMessageBox::AcceptRole);
    }
    auto *disable = box->addButton(tr("Disable kill switch"), QMessageBox::DestructiveRole);
    auto *cancel = box->addButton(QMessageBox::Cancel);
    box->setDefaultButton(fix != nullptr ? fix : cancel);
    box->setEscapeButton(cancel);
    m_killSwitchDialog = box;

    // Custom-button result codes changed in Qt 6.5, so only the clicked button pointer is trusted.
    connect(box, &QMessageBox::finished, this, [this, box, fix, disable, privilege, restartElevated] {
        const auto *clicked = box->clickedButton();
        if (m_killSwitchDialog == box) m_killSwitchDialog = nullptr;
        if (clicked == disable) {
            connection()->disableKillSwitch();
            return;
        }
        if (fix == nullptr || clicked != fix) return;
        if (!privilege) {
            Sys::KillSwitch::instance()->apply();
            return;
        }
        if (restartElevated) {
            exitController()->requestExit(ExitReason::RestartElevated, false);
            return;
        }
        // Otherwise the core restarts once it is privileged, and onCoreStarted retries.
        if (connection()->ensureTunPrivileges(true, true)) Sys::KillSwitch::instance()->apply();
    });
    box->open();
}

void MainWindow::killSwitchProblemResolved() {
    if (m_killSwitchDialog) m_killSwitchDialog->close();
}

void MainWindow::extraCoreNotPermitted() {
    ShowPassiveWarning(tr("Kill switch"),
                       tr("The kill switch could not be updated to allow this profile's extra core, so the profile was not started. See the log for details."));
}

void MainWindow::startFailed(const StartRequest &, StartOutcome, const QString &title, const QString &error) {
    policyNotice(m_policyBoxes, __func__, this, QMessageBox::Warning, title, error);
}

void MainWindow::strictRouteUnavailable(const QString &error) {
    policyNotice(m_policyBoxes, __func__, this, QMessageBox::Warning,
        tr("Strict routing unavailable"),
        tr("Windows could not enable strict routing. Open Tun Settings, "
           "disable Strict route, and start the profile again.\n\n"
           "Disabling Strict route may cause DNS leaks.\n\nError: %1").arg(error));
}

void MainWindow::offerCoreReset(const QString &error, std::function<void(bool reset)> answered) {
    if (policyOpenBox(m_policyBoxes, __func__) != nullptr) {
        answered(false);
        return;
    }
    auto *msg = new QMessageBox(
        QMessageBox::Information,
        tr("Tun device misbehaving"),
        tr("If you have trouble starting VPN, you can force reset Core process here and then try starting the profile again. The error is %1").arg(error),
        QMessageBox::NoButton,
        this
    );
    auto reset = msg->addButton(tr("Reset"), QMessageBox::ActionRole);
    auto cancel = msg->addButton(tr("Cancel"), QMessageBox::ActionRole);

    msg->setDefaultButton(cancel);
    msg->setEscapeButton(cancel);

    policyAsk(m_policyBoxes, __func__, msg, reset, std::move(answered));
}

void MainWindow::offerLeftoverTunAdapterRetry(const QString &error, std::function<void(bool retry)> answered) {
    if (policyOpenBox(m_policyBoxes, __func__) != nullptr) {
        answered(false);
        return;
    }
    auto *msg = new QMessageBox(
        QMessageBox::Information,
        tr("Leftover Tun adapter"),
        tr("Windows was still holding the Tun adapter from an earlier session that did not close cleanly. "
           "Starting the profile again usually works; if it keeps failing, restart Windows.\n\nError: %1").arg(error),
        QMessageBox::NoButton,
        this
    );
    auto startAgain = msg->addButton(tr("Start again"), QMessageBox::AcceptRole);
    auto cancel = msg->addButton(tr("Cancel"), QMessageBox::RejectRole);

    msg->setDefaultButton(startAgain);
    msg->setEscapeButton(cancel);

    policyAsk(m_policyBoxes, __func__, msg, startAgain, std::move(answered));
}

void MainWindow::stopFailed(const QString &error) {
    policyNotice(m_policyBoxes, __func__, this, QMessageBox::Warning, tr("Stop return error"), error);
}

Engine::SlowOperation MainWindow::slowOperation(Engine::SlowOperationKind, int delayMs) {
    const QPointer<RestartPrompt> restartPrompt =
        new RestartPrompt(this, tr("If there is no response for a long time, it is recommended to restart the software."), delayMs);
    return Engine::SlowOperation([restartPrompt] { if (restartPrompt) restartPrompt->dismiss(); });
}

bool MainWindow::requestCorePrivileges(bool confirmed) {
#ifdef NKR_ELEVATION_HINT
    MessageBoxWarning(software_name, tr("This installation cannot grant the core privileges by itself.") + "\n\n" + NKR_ELEVATION_HINT);
    return false;
#endif
#ifdef Q_OS_LINUX
    if (!Linux_HavePkexec()) {
        MessageBoxWarning(software_name, "Please install \"pkexec\" first.");
        return false;
    }
    if (confirmed || QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please give the core root privileges"),
                                          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
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
    if (Sys::CoreElevation::GrantIneffective()) {
        if (policyOfferElevatedRestart(GetMessageBoxParent(),
                                       tr("The core did not get administrator rights, although they were granted. The log shows why.\n\n"
                                          "Restart Throne as administrator to use Tun mode and the kill switch."))) {
            this->exit_reason = ExitReason::RestartWithTun;
            on_menu_exit_triggered();
        }
        return false;
    }
    if (Sys::CoreElevation::CanGrant()) {
        if (!confirmed) {
            QMessageBox box(QMessageBox::Warning, software_name,
                            tr("Tun mode and the kill switch need the core to run with administrator rights.\n\n"
                               "Grant them once with a Windows confirmation; Throne itself keeps running as a standard user."),
                            QMessageBox::NoButton, GetMessageBoxParent());
            auto *grant = box.addButton(tr("Grant privileges"), QMessageBox::AcceptRole);
            auto *cancel = box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(grant);
            box.setEscapeButton(cancel);
            box.exec();
            if (box.clickedButton() != grant) return false;
        }
        Sys::CoreElevation::GrantAsync(this, [this](int code) {
            const auto result = Sys::CoreElevation::ResultText(code);
            if (code == Sys::CoreElevation::HelperOk) {
                LOG_INFO("core elevation grant: " + result);
                // The core restarts elevated, like after the Linux grant.
                StopVPNProcess();
            } else if (code == -1) {
                LOG_INFO("core elevation grant: " + result);
            } else {
                LOG_WARN("core elevation grant: " + result);
                if (policyOfferElevatedRestart(GetMessageBoxParent(), result + "\n\n" + tr("Throne can restart as administrator instead."))) {
                    this->exit_reason = ExitReason::RestartWithTun;
                    on_menu_exit_triggered();
                }
            }
        });
        return false;
    }
    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please run Throne as admin"), QMessageBox::Yes | QMessageBox::No);
    if (n == QMessageBox::Yes) {
        this->exit_reason = ExitReason::RestartWithTun;
        on_menu_exit_triggered();
    }
#endif

#ifdef Q_OS_MACOS
    if (Configs::isSetuidSet(Configs::FindCoreRealPath().toStdString()))
    {
        StopVPNProcess();
        return true;
    }
    if (confirmed || QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please give the core root privileges"),
                                          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
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

void MainWindow::systemProxyNeedsMixedInbound() {
    policyNotice(m_policyBoxes, __func__, this, QMessageBox::Warning, "Invalid Operation",
                 "Cannot set system proxy when mixed inbound is disabled.");
}

bool MainWindow::vpnPromptOpen() const {
    return m_vpnAuthDialog != nullptr;
}

void MainWindow::vpnChallenge(const VpnAuthChallenge &challenge) {
    auto *dialog = new DialogVpnAuth(this, vpnAuth(), challenge);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_vpnAuthDialog = dialog;
    dialog->show();
    ActivateWindow(dialog);
}

void MainWindow::vpnCredentialsRejected(int, const VpnAuthChallenge &challenge,
                                        const std::function<void(const QString &username, const QString &password)> &retryWith) {
    auto *dialog = new DialogVpnAuth(this, vpnAuth(), challenge, true);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_vpnAuthDialog = dialog;
    connect(dialog, &QDialog::accepted, this, [dialog, retryWith] {
        retryWith(dialog->enteredUsername(), dialog->enteredPassword());
    });
    dialog->show();
    ActivateWindow(dialog);
}

void MainWindow::offerVpnSignInRestart(const QString &endpointName, const QString &error,
                                       std::function<void(bool restart)> answered) {
    if (policyOpenBox(m_policyBoxes, __func__) != nullptr) {
        answered(false);
        return;
    }
    auto *msg = new QMessageBox(
        QMessageBox::Warning,
        tr("VPN sign-in refused"),
        tr("%1 refused its sign-in, so traffic routed through it fails. Restarting the profile signs it in again with a new "
           "one-time code, but drops every open connection.\n\nError: %2").arg(endpointName, error),
        QMessageBox::NoButton,
        this
    );
    auto restart = msg->addButton(tr("Restart profile"), QMessageBox::AcceptRole);
    auto later = msg->addButton(tr("Not now"), QMessageBox::RejectRole);

    // Not a default restart: the box can pop up while the user is typing elsewhere.
    msg->setDefaultButton(later);
    msg->setEscapeButton(later);

    policyAsk(m_policyBoxes, __func__, msg, restart, std::move(answered));
}

void MainWindow::vpnChallengesCleared() {
    if (m_vpnAuthDialog != nullptr) m_vpnAuthDialog->close();
    // The question belongs to the run that just ended.
    if (auto *box = policyOpenBox(m_policyBoxes, "offerVpnSignInRestart")) box->close();
}

void MainWindow::settingsNeedTunRestart() {
    MessageBoxWarning(tr("Tun Settings changed"), tr("Restart Tun to take effect."));
}

bool MainWindow::confirmRestartProgram() {
    return QMessageBox::warning(GetMessageBoxParent(), tr("Settings changed"), tr("Restart the program to take effect."), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

void MainWindow::testSessionBusy(const QString &text) {
    MessageBoxWarning(software_name, text);
}

void MainWindow::exitProgram(ExitReason reason, bool unattended) {
    if (unattended) {
        request_unattended_exit(reason);
        return;
    }
    exit_reason = reason;
    on_menu_exit_triggered();
}
