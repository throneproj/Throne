#include "include/ui/setting/dialog_vpn_settings.h"

#include "include/ui/utils/GuiUtils.hpp"
#include "include/global/Configs.hpp"
#include "include/global/Validation.hpp"
#include "include/database/SettingsSchema.h"
#include "include/engine/EngineHost.h"
#include "include/engine/services/SettingsService.h"
#include "include/engine/supervisor/CoreSupervisor.h"
#include "include/ui/utils/ValidationForm.h"

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
    snapshot_ = *Configs::dataManager->settingsRepo;
    ui->setupUi(this);
    ADD_ASTERISK(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);

    ui->vpn_mtu->setCurrentText(Int2String(snapshot_.vpn_mtu));
    ui->vpn_ipv6->setChecked(snapshot_.vpn_ipv6);
    ui->strict_route->setChecked(snapshot_.vpn_strict_route);
    ui->tun_routing->setChecked(snapshot_.enable_tun_routing);
    ui->tun_ipv4_cidr->setText(snapshot_.vpn_tun_ipv4_cidr);
    ui->tun_ipv6_cidr->setText(snapshot_.vpn_tun_ipv6_cidr);
    ui->privRangeGroupBox->setChecked(!snapshot_.disable_private_range_bypass);
    QStringList privateRanges;
    for (const auto &range : snapshot_.vpn_private_ranges) privateRanges << WithPrefixLength(range);
    ui->priv_ranges->setPlainText(privateRanges.join("\n"));
    ui->auto_redirect->setChecked(snapshot_.vpn_auto_redirect);
    ui->l3_bridge->setChecked(snapshot_.vpn_l3_bridge);
    for (auto *widget : UnavailableWidgets(ui)) widget->hide();

    using namespace Validation;
    const auto *mtuKey = Configs::SettingsSchema::Find(QStringLiteral("vpn_mtu"));
    const auto mtuRange = mtuKey != nullptr && mtuKey->range ? *mtuKey->range : Configs::SettingsSchema::IntRange{kVpnMinMtu, kVpnMaxMtu};
    form->bind(ui->vpn_mtu, Rules::Int(mtuRange.min, mtuRange.max, Empty::Required)).label(ui->label_10->text());
    form->addRule({ui->vpn_mtu, ui->vpn_ipv6}, [this, minMtu = mtuRange.min] {
        const auto mtu = ToInt(ui->vpn_mtu->currentText());
        if (!ui->vpn_ipv6->isChecked() || !mtu || *mtu >= kVpnMinIPv6Mtu || *mtu < minMtu) return QString();
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

QList<QWidget *> DialogVPNSettings::UnavailableWidgets(const Ui::DialogVPNSettings *ui) {
    Q_UNUSED(ui) // On Linux x86 nothing is unavailable.
    QList<QWidget *> widgets;
#ifndef Q_OS_LINUX
    widgets << ui->auto_redirect;
#endif
    // The core's bridge backend has no implementation for Windows on ARM.
#if defined(Q_OS_WIN) && defined(Q_PROCESSOR_ARM)
    widgets << ui->l3_bridge;
#endif
    return widgets;
}

void DialogVPNSettings::accept() {
    if (!form->validateAll()) return;
    const int mtu = int(*Validation::ToInt(ui->vpn_mtu->currentText()));
    const auto tunIPv4CIDR = ui->tun_ipv4_cidr->text().trimmed();
    const auto tunIPv6CIDR = ui->tun_ipv6_cidr->text().trimmed();
    // The list is disabled while the bypass is off, so it is neither checked nor saved.
    auto privateRanges = snapshot_.vpn_private_ranges;
    if (ui->privRangeGroupBox->isChecked()) privateRanges = Validation::Items(ui->priv_ranges->toPlainText(), Validation::Split::Newline);

    Configs::SettingsValues proposed = snapshot_;
    proposed.vpn_mtu = mtu;
    proposed.vpn_ipv6 = ui->vpn_ipv6->isChecked();
    proposed.vpn_strict_route = ui->strict_route->isChecked();
    proposed.enable_tun_routing = ui->tun_routing->isChecked();
    proposed.vpn_tun_ipv4_cidr = tunIPv4CIDR;
    proposed.vpn_tun_ipv6_cidr = tunIPv6CIDR;
    proposed.disable_private_range_bypass = !ui->privRangeGroupBox->isChecked();
    proposed.vpn_private_ranges = privateRanges;
    proposed.vpn_auto_redirect = ui->auto_redirect->isChecked();
    proposed.vpn_l3_bridge = ui->l3_bridge->isChecked();
    if (const auto error = Engine::SettingsService::ApplyEdit(snapshot_, proposed, {.interactive = true})) {
        MessageBoxWarning(windowTitle(), SettingsApplyErrorText(*error));
        return;
    }
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

void DialogVPNSettings::on_restart_core_clicked() {
    Engine::host()->supervisor()->resetCore();
}
