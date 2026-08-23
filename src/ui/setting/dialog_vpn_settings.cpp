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

    // parseSubnet alone would accept Qt's abbreviated forms, where "10" means 10.0.0.0/8.
    bool IsValidRange(const QString &range) {
        if (!range.contains('.') && !range.contains(':')) return false;
        return QHostAddress::parseSubnet(range).second >= 0;
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
    ui->tun_ipv4_cidr->setText(Configs::dataManager->settingsRepo->vpn_tun_ipv4_cidr);
    ui->tun_ipv6_cidr->setText(Configs::dataManager->settingsRepo->vpn_tun_ipv6_cidr);
    ui->privRangeGroupBox->setChecked(!Configs::dataManager->settingsRepo->disable_private_range_bypass);
    ui->priv_ranges->setPlainText(Configs::dataManager->settingsRepo->vpn_private_ranges.join("\n"));
    ui->auto_redirect->setChecked(Configs::dataManager->settingsRepo->vpn_auto_redirect);
#ifndef Q_OS_LINUX
    ui->auto_redirect->hide();
#endif
    ui->l3_bridge->setChecked(Configs::dataManager->settingsRepo->vpn_l3_bridge);
    // The core's bridge backend has no implementation for Windows on ARM.
#if defined(Q_OS_WIN) && defined(Q_PROCESSOR_ARM)
    ui->l3_bridge->hide();
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
    QStringList privateRanges;
    for (const auto &line : ui->priv_ranges->toPlainText().split("\n")) {
        const auto range = line.trimmed();
        if (range.isEmpty()) continue;
        if (!IsValidRange(range)) {
            QMessageBox::warning(this, tr("Invalid Private Range"),
                                 tr("\"%1\" is not a valid address or CIDR.").arg(range));
            return;
        }
        // Excluding a default route leaves the Tun with no routes at all.
        if (QHostAddress::parseSubnet(range).second == 0) {
            QMessageBox::warning(this, tr("Invalid Private Range"),
                                 tr("\"%1\" covers every address, which would stop Tun from routing anything.").arg(range));
            return;
        }
        privateRanges << range;
    }

    Configs::dataManager->settingsRepo->vpn_implementation = ui->vpn_implementation->currentText();
    Configs::dataManager->settingsRepo->vpn_mtu = mtu;
    Configs::dataManager->settingsRepo->vpn_ipv6 = ui->vpn_ipv6->isChecked();
    Configs::dataManager->settingsRepo->vpn_strict_route = ui->strict_route->isChecked();
    Configs::dataManager->settingsRepo->enable_tun_routing = ui->tun_routing->isChecked();
    Configs::dataManager->settingsRepo->vpn_tun_ipv4_cidr = tunIPv4CIDR;
    Configs::dataManager->settingsRepo->vpn_tun_ipv6_cidr = tunIPv6CIDR;
    Configs::dataManager->settingsRepo->disable_private_range_bypass = !ui->privRangeGroupBox->isChecked();
    Configs::dataManager->settingsRepo->vpn_private_ranges = privateRanges;
    Configs::dataManager->settingsRepo->vpn_auto_redirect = ui->auto_redirect->isChecked();
    Configs::dataManager->settingsRepo->vpn_l3_bridge = ui->l3_bridge->isChecked();
    //
    MW_dialog_message(MwMessage::UpdateSettings, {MwArg::Vpn});
    QDialog::accept();
}

void DialogVPNSettings::on_restore_default_addresses_clicked() {
    ui->tun_ipv4_cidr->setText(kDefaultTunIPv4CIDR);
    ui->tun_ipv6_cidr->setText(kDefaultTunIPv6CIDR);
}

void DialogVPNSettings::on_restore_default_ranges_clicked() {
    ui->priv_ranges->setPlainText(Configs::defaultTunPrivateRanges().join("\n"));
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
