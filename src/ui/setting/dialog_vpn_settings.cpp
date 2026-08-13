#include "include/ui/setting/dialog_vpn_settings.h"

#include "include/global/GuiUtils.hpp"
#include "include/global/Configs.hpp"
#include "include/ui/mainwindow_interface.h"
#ifdef Q_OS_WIN
#include "include/sys/windows/WinVersion.h"
#endif

#include <QMessageBox>
#include <QHostAddress>


#define ADJUST_SIZE runOnThread([=,this] { adjustSize(); adjustPosition(mainwindow); }, this);

namespace {
    const QString kDefaultTunIPv4CIDR = "172.19.0.1/24";
    const QString kDefaultTunIPv6CIDR = "fdfe:dcba:9876::1/96";

    bool IsValidCIDR(const QString &cidr, const QAbstractSocket::NetworkLayerProtocol protocol) {
        const auto parts = cidr.trimmed().split("/");
        if (parts.size() != 2) return false;

        bool ok = false;
        const int prefix = parts[1].toInt(&ok);
        if (!ok) return false;

        QHostAddress host;
        if (!host.setAddress(parts[0].trimmed())) return false;
        if (host.protocol() != protocol) return false;

        if (protocol == QAbstractSocket::IPv4Protocol) return prefix >= 0 && prefix <= 32;
        if (protocol == QAbstractSocket::IPv6Protocol) return prefix >= 0 && prefix <= 128;
        return false;
    }
}

DialogVPNSettings::DialogVPNSettings(QWidget *parent) : QDialog(parent), ui(new Ui::DialogVPNSettings) {
    ui->setupUi(this);
    ADD_ASTERISK(this);

#ifdef Q_OS_WIN
    if (WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_10_1507)) {
        ui->vpn_implementation->addItems(Configs::VPNImplementation::VPNImplementation);
        ui->vpn_implementation->setCurrentText(Configs::dataManager->settingsRepo->vpn_implementation);
    }
    else {
        ui->vpn_implementation->addItems(Configs::VPNImplementation::VPNImplementation);
        ui->vpn_implementation->setCurrentText("gvisor");
        ui->vpn_implementation->setEnabled(false);
    }
#else
    ui->vpn_implementation->addItems(Configs::VPNImplementation::VPNImplementation);
    ui->vpn_implementation->setCurrentText(Configs::dataManager->settingsRepo->vpn_implementation);
#endif
    ui->vpn_mtu->setCurrentText(Int2String(Configs::dataManager->settingsRepo->vpn_mtu));
    ui->vpn_ipv6->setChecked(Configs::dataManager->settingsRepo->vpn_ipv6);
    ui->strict_route->setChecked(Configs::dataManager->settingsRepo->vpn_strict_route);
    ui->tun_routing->setChecked(Configs::dataManager->settingsRepo->enable_tun_routing);
#ifdef Q_OS_WIN
    ui->kill_switch->setChecked(Configs::dataManager->settingsRepo->kill_switch_enabled);
#else
    ui->kill_switch_widget->hide();
#endif
    ui->tun_ipv4_cidr->setText(Configs::dataManager->settingsRepo->vpn_tun_ipv4_cidr);
    ui->tun_ipv6_cidr->setText(Configs::dataManager->settingsRepo->vpn_tun_ipv6_cidr);
    ui->disable_priv_range->setChecked(Configs::dataManager->settingsRepo->disable_private_range_bypass);
    ui->auto_redirect->setChecked(Configs::dataManager->settingsRepo->vpn_auto_redirect);
#ifndef Q_OS_LINUX
    ui->auto_redirect->hide();
#endif
    ADJUST_SIZE
}

DialogVPNSettings::~DialogVPNSettings() {
    delete ui;
}

void DialogVPNSettings::accept() {
    //
    auto mtu = ui->vpn_mtu->currentText().toInt();
    if (mtu > 10000 || mtu < 1000) mtu = 9000;
    const auto tunIPv4CIDR = ui->tun_ipv4_cidr->text().trimmed();
    const auto tunIPv6CIDR = ui->tun_ipv6_cidr->text().trimmed();
    if (!IsValidCIDR(tunIPv4CIDR, QAbstractSocket::IPv4Protocol)) {
        QMessageBox::warning(this, tr("Invalid Tun Address"), tr("IPv4 CIDR is invalid."));
        return;
    }
    if (!IsValidCIDR(tunIPv6CIDR, QAbstractSocket::IPv6Protocol)) {
        QMessageBox::warning(this, tr("Invalid Tun Address"), tr("IPv6 CIDR is invalid."));
        return;
    }

    struct VpnSettingsValues {
        QString implementation;
        int mtu;
        bool ipv6;
        bool strictRoute;
        bool tunRouting;
        QString tunIPv4CIDR;
        QString tunIPv6CIDR;
        bool disablePrivateRangeBypass;
        bool autoRedirect;
    };

    auto &settings = *Configs::dataManager->settingsRepo;
    const VpnSettingsValues pendingSettings{
        ui->vpn_implementation->currentText(),
        mtu,
        ui->vpn_ipv6->isChecked(),
        ui->strict_route->isChecked(),
        ui->tun_routing->isChecked(),
        tunIPv4CIDR,
        tunIPv6CIDR,
        ui->disable_priv_range->isChecked(),
        ui->auto_redirect->isChecked(),
    };
    const auto applySettings = [&settings](const VpnSettingsValues &values) {
        settings.vpn_implementation = values.implementation;
        settings.vpn_mtu = values.mtu;
        settings.vpn_ipv6 = values.ipv6;
        settings.vpn_strict_route = values.strictRoute;
        settings.enable_tun_routing = values.tunRouting;
        settings.vpn_tun_ipv4_cidr = values.tunIPv4CIDR;
        settings.vpn_tun_ipv6_cidr = values.tunIPv6CIDR;
        settings.disable_private_range_bypass = values.disablePrivateRangeBypass;
        settings.vpn_auto_redirect = values.autoRedirect;
    };

    bool protectedRestartWillApplySettings = false;
#ifdef Q_OS_WIN
    const VpnSettingsValues previousSettings{
        settings.vpn_implementation,
        settings.vpn_mtu,
        settings.vpn_ipv6,
        settings.vpn_strict_route,
        settings.enable_tun_routing,
        settings.vpn_tun_ipv4_cidr,
        settings.vpn_tun_ipv6_cidr,
        settings.disable_private_range_bypass,
        settings.vpn_auto_redirect,
    };
    const bool requestedKillSwitch = ui->kill_switch->isChecked();
    if (requestedKillSwitch != settings.kill_switch_enabled) {
        // A non-elevated first enable restarts the whole application. Persist
        // the pending ordinary fields immediately before launching the helper,
        // because the elevated replacement must read them from the database.
        // If any part of that transition fails, restore both the live and
        // persisted ordinary settings before leaving the dialog open.
        const bool stageForElevatedRestart =
            requestedKillSwitch && !Configs::IsAdmin();
        if (stageForElevatedRestart) {
            applySettings(pendingSettings);
            settings.Save();
        }
        QString error;
        if (!GetMainWindow()->setKillSwitchEnabled(requestedKillSwitch, &error)) {
            if (stageForElevatedRestart) {
                applySettings(previousSettings);
                settings.Save();
            }
            QMessageBox::critical(
                this,
                tr("Kill switch change failed"),
                tr("The requested kill-switch change could not be completed safely. "
                   "Throne retained the safest state it could verify.\n\n%1")
                    .arg(error));
            ui->kill_switch->setChecked(settings.kill_switch_enabled);
            return;
        }
        protectedRestartWillApplySettings = stageForElevatedRestart;
    }
#endif

    // For an ordinary in-process transition, commit these fields only after
    // the kill-switch state change has succeeded. In the elevated-restart case
    // this simply reapplies the already persisted pending snapshot.
    applySettings(pendingSettings);

    if (!protectedRestartWillApplySettings) {
        MW_dialog_message(MwMessage::UpdateSettings, {MwArg::Vpn});
    }

    QDialog::accept();
}

void DialogVPNSettings::on_restore_default_addresses_clicked() {
    ui->tun_ipv4_cidr->setText(kDefaultTunIPv4CIDR);
    ui->tun_ipv6_cidr->setText(kDefaultTunIPv6CIDR);
}

void DialogVPNSettings::on_troubleshooting_clicked() {


    QMessageBox msg(
        QMessageBox::Information,
        tr("Troubleshooting"),
        tr("If you have trouble starting VPN, you can force reset Core process here.\n\n"
            "If still not working, see documentation for more information.\n"
            "https://matsuridayo.github.io/n-configuration/#vpn-tun"),
        QMessageBox::NoButton,
        this
    );
    msg.addButton(tr("Reset"), QMessageBox::ActionRole);
    auto cancel = msg.addButton(tr("Cancel"), QMessageBox::ActionRole);

    msg.setDefaultButton(cancel);
    msg.setEscapeButton(cancel);

    auto r = msg.exec() - 2;
    if (r == 0) {
        GetMainWindow()->StopVPNProcess();
    }
}
