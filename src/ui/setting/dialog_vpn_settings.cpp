#include "include/ui/setting/dialog_vpn_settings.h"

#include "include/ui/utils/GuiUtils.hpp"
#include "include/global/Configs.hpp"
#include "include/global/Validation.hpp"
#include "include/engine/EngineHost.h"
#include "include/engine/supervisor/CoreSupervisor.h"
#include "include/ui/utils/ValidationForm.h"

#include <QMessageBox>
#include <QHostAddress>


#define ADJUST_SIZE runOnThread([=,this] { adjustSize(); adjustPosition(mainwindow); }, this);

namespace {
    const QString kDefaultTunIPv4CIDR = "172.19.0.1/24";
    const QString kDefaultTunIPv6CIDR = "fdfe:dcba:9876::1/96";
    constexpr int kVpnMinMtu = 1000;
    constexpr int kVpnMaxMtu = 10000;
    constexpr int kVpnMinIPv6Mtu = 1280;

    // Older builds saved bare addresses, which the core cannot read as a prefix (routeExcludePrefix widens them the same way).
    QString WithPrefixLength(const QString &saved) {
        const auto range = saved.trimmed();
        if (range.contains(u'/')) return range;
        if (Validation::Ip(range, Validation::Family::V4).isEmpty()) return range + "/32";
        if (Validation::Ip(range, Validation::Family::V6).isEmpty()) return range + "/128";
        return range;
    }
}

DialogVPNSettings::DialogVPNSettings(QWidget *parent) : QDialog(parent), ui(new Ui::DialogVPNSettings) {
    ui->setupUi(this);
    ADD_ASTERISK(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);

    ui->vpn_mtu->setCurrentText(Int2String(Configs::dataManager->settingsRepo->vpn_mtu));
    ui->vpn_ipv6->setChecked(Configs::dataManager->settingsRepo->vpn_ipv6);
    ui->strict_route->setChecked(Configs::dataManager->settingsRepo->vpn_strict_route);
    ui->tun_routing->setChecked(Configs::dataManager->settingsRepo->enable_tun_routing);
    ui->tun_ipv4_cidr->setText(Configs::dataManager->settingsRepo->vpn_tun_ipv4_cidr);
    ui->tun_ipv6_cidr->setText(Configs::dataManager->settingsRepo->vpn_tun_ipv6_cidr);
    ui->privRangeGroupBox->setChecked(!Configs::dataManager->settingsRepo->disable_private_range_bypass);
    QStringList privateRanges;
    for (const auto &range : Configs::dataManager->settingsRepo->vpn_private_ranges) privateRanges << WithPrefixLength(range);
    ui->priv_ranges->setPlainText(privateRanges.join("\n"));
    ui->auto_redirect->setChecked(Configs::dataManager->settingsRepo->vpn_auto_redirect);
#ifndef Q_OS_LINUX
    ui->auto_redirect->hide();
#endif
    ui->l3_bridge->setChecked(Configs::dataManager->settingsRepo->vpn_l3_bridge);
    // The core's bridge backend has no implementation for Windows on ARM.
#if defined(Q_OS_WIN) && defined(Q_PROCESSOR_ARM)
    ui->l3_bridge->hide();
#endif

    using namespace Validation;
    form->bind(ui->vpn_mtu, Rules::Int(kVpnMinMtu, kVpnMaxMtu, Empty::Required)).label(ui->label_10->text());
    form->addRule({ui->vpn_mtu, ui->vpn_ipv6}, [this] {
        const auto mtu = ToInt(ui->vpn_mtu->currentText());
        if (!ui->vpn_ipv6->isChecked() || !mtu || *mtu >= kVpnMinIPv6Mtu || *mtu < kVpnMinMtu) return QString();
        return tr("IPv6 needs an MTU of at least %1; below that the system may drop IPv6 from the Tun.").arg(kVpnMinIPv6Mtu);
    }, this, Severity::Warning, ui->label_10->text());
    form->bind(ui->tun_ipv4_cidr, Rules::Cidr(Family::V4));
    form->bind(ui->tun_ipv6_cidr, Rules::Cidr(Family::V6));
    form->bindLines(ui->priv_ranges, [](const QString &range) {
        if (auto problem = Cidr(range); !problem.isEmpty()) return problem;
        // Excluding a default route leaves the Tun with no routes at all.
        if (QHostAddress::parseSubnet(range).second == 0) return tr("This covers every address, which would stop Tun from routing anything.");
        return QString();
    }).label(ui->privRangeGroupBox->title());
    form->markAll();

    ADJUST_SIZE
}

DialogVPNSettings::~DialogVPNSettings() {
    delete ui;
}

void DialogVPNSettings::accept() {
    if (!form->validateAll()) return;
    const int mtu = int(*Validation::ToInt(ui->vpn_mtu->currentText()));
    const auto tunIPv4CIDR = ui->tun_ipv4_cidr->text().trimmed();
    const auto tunIPv6CIDR = ui->tun_ipv6_cidr->text().trimmed();
    // The list is disabled while the bypass is off, so it is neither checked nor saved.
    auto privateRanges = Configs::dataManager->settingsRepo->vpn_private_ranges;
    if (ui->privRangeGroupBox->isChecked()) privateRanges = Validation::Items(ui->priv_ranges->toPlainText(), Validation::Split::Newline);

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
    MW_dialog_message(MwMessage::UpdateSettings, {MwArg::Vpn});
    QDialog::accept();
}

void DialogVPNSettings::on_restore_default_addresses_clicked() {
    ui->tun_ipv4_cidr->setText(kDefaultTunIPv4CIDR);
    ui->tun_ipv6_cidr->setText(kDefaultTunIPv6CIDR);
    form->revalidate(ui->tun_ipv4_cidr);
    form->revalidate(ui->tun_ipv6_cidr);
}

void DialogVPNSettings::on_restore_default_ranges_clicked() {
    ui->priv_ranges->setPlainText(Configs::defaultTunPrivateRanges().join("\n"));
    form->revalidate(ui->priv_ranges);
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
    auto reset = msg.addButton(tr("Reset"), QMessageBox::ActionRole);
    auto cancel = msg.addButton(tr("Cancel"), QMessageBox::ActionRole);

    msg.setDefaultButton(cancel);
    msg.setEscapeButton(cancel);

    msg.exec();
    if (msg.clickedButton() == reset) {
        Engine::host()->supervisor()->resetCore();
    }
}
