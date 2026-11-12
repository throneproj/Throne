#include "include/ui/profile/dialog_edit_profile.h"

#include "include/configs/common/xrayStreamSetting.h"
#include "include/ui/utils/ValidationForm.h"
#include "include/ui/widget/ResolverSourceWidget.h"

#include <QCoreApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QRegularExpressionValidator>

#include <array>

namespace {
struct KCPEdit {
    QLineEdit *edit;
    qint64 Configs::xrayKCP::*field;
    qint64 xrayDefault;
};

std::array<KCPEdit, 6> kcpEdits(const Ui::DialogEditProfile *ui) {
    using Configs::xrayKCP;
    return {{
        {ui->xray_kcp_mtu, &xrayKCP::mtu, xrayKCP::DefaultMtu},
        {ui->xray_kcp_tti, &xrayKCP::tti, xrayKCP::DefaultTti},
        {ui->xray_kcp_uplink_capacity, &xrayKCP::uplinkCapacity, xrayKCP::DefaultUplinkCapacity},
        {ui->xray_kcp_downlink_capacity, &xrayKCP::downlinkCapacity, xrayKCP::DefaultDownlinkCapacity},
        {ui->xray_kcp_cwnd_multiplier, &xrayKCP::cwndMultiplier, xrayKCP::DefaultCwndMultiplier},
        {ui->xray_kcp_max_sending_window, &xrayKCP::maxSendingWindow, xrayKCP::DefaultMaxSendingWindow},
    }};
}

QString finalmaskIssue(const QJsonObject &finalmask, Validation::Severity severity, bool resolversReplaced) {
    Validation::Issues issues;
    Configs::Checks::XrayFinalmask(issues, {}, finalmask, resolversReplaced);
    for (const auto &issue: issues) {
        if (issue.severity == severity) return QCoreApplication::translate("Validation", "%1: %2").arg(issue.field, issue.message);
    }
    return {};
}
}

void DialogEditProfile::setupXrayKCPControls() {
    for (auto *grid: {ui->gridLayout_xray_kcp, ui->gridLayout_xray_kcp_tuning}) {
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(3, 1);
    }
    ui->verticalLayout_xray_kcp->setAlignment(Qt::AlignTop);

    auto *digits = new QRegularExpressionValidator(QRegularExpression("^[0-9]*$"), this);
    for (const auto &value: kcpEdits(ui)) {
        value.edit->setValidator(digits);
        value.edit->setPlaceholderText(QString::number(value.xrayDefault));
    }

    setXrayXHTTPHelp(ui->xray_kcp_mtu_l, ui->xray_kcp_mtu, tr("MTU"), "mtu",
                     tr("Largest packet mKCP sends, in bytes, from 21 to 8192. Default: 1350."));
    setXrayXHTTPHelp(ui->xray_kcp_tti_l, ui->xray_kcp_tti, tr("TTI"), "tti",
                     tr("How often mKCP sends, in milliseconds, from 10 to 1000. Default: 50."));
    setXrayXHTTPHelp(ui->xray_kcp_uplink_capacity_l, ui->xray_kcp_uplink_capacity, tr("Uplink Capacity"), "uplinkCapacity",
                     tr("Most data sent per second, in MB/s; 0 means very little. Default: 5."));
    setXrayXHTTPHelp(ui->xray_kcp_downlink_capacity_l, ui->xray_kcp_downlink_capacity, tr("Downlink Capacity"),
                     "downlinkCapacity", tr("Most data received per second, in MB/s; 0 means very little. Default: 20."));
    setXrayXHTTPHelp(ui->xray_kcp_cwnd_multiplier_l, ui->xray_kcp_cwnd_multiplier, tr("CWND Multiplier"), "cwndMultiplier",
                     tr("Multiplies how many packets may be in flight at once. Default: 1."));
    setXrayXHTTPHelp(ui->xray_kcp_max_sending_window_l, ui->xray_kcp_max_sending_window, tr("Max Sending Window"),
                     "maxSendingWindow",
                     tr("Most data waiting to be sent or acknowledged, in bytes; at least the MTU. Default: 2097152."));

    xdnsResolvers = new ResolverSourceWidget(ui->xray_kcp_box);
    xdnsResolvers->setFlavor(Configs::ResolverFlavor::Xdns);
    xdnsResolversLabel = new QLabel(tr("Resolvers"), ui->xray_kcp_box);
    xdnsResolversLabel->setBuddy(xdnsResolvers->linesEdit());
    ui->gridLayout_xray_kcp->addWidget(xdnsResolversLabel, 1, 0, Qt::AlignTop);
    ui->gridLayout_xray_kcp->addWidget(xdnsResolvers, 1, 1, 1, 3);
    // outerTabOrder was collected before this row existed, and the inner editor's chain is rebuilt from it.
    if (const auto at = outerTabOrder.indexOf(ui->xray_kcp_tti); at >= 0) {
        QList<QWidget *> chain;
        for (auto *w = xdnsResolvers->nextInFocusChain(); w != nullptr && xdnsResolvers->isAncestorOf(w); w = w->nextInFocusChain()) {
            if (w->focusPolicy() & Qt::TabFocus) chain << w;
        }
        for (qsizetype i = 0; i < chain.size(); ++i) outerTabOrder.insert(at + 1 + i, chain.at(i));
        if (innerTabOrderIndex > at) innerTabOrderIndex += chain.size();
    }
    connect(xdnsResolvers, &ResolverSourceWidget::changed, this, [this] {
        if (form != nullptr) form->revalidate(ui->xray_finalmask_edit);
    });
}

void DialogEditProfile::loadXrayKCP(const Configs::xrayKCP &kcp) {
    for (const auto &value: kcpEdits(ui)) {
        const auto number = kcp.*value.field;
        value.edit->setText(number == value.xrayDefault ? QString() : QString::number(number));
    }
}

void DialogEditProfile::saveXrayKCP(Configs::xrayKCP &kcp) {
    for (const auto &value: kcpEdits(ui)) {
        const auto text = value.edit->text().trimmed();
        kcp.*value.field = text.isEmpty() ? value.xrayDefault : text.toLongLong();
    }
}

void DialogEditProfile::bindXrayKCP() {
    using Validation::Severity;
    namespace Checks = Configs::Checks;

    form->bind(ui->xray_kcp_mtu, Checks::XrayKCPMtu);
    form->bind(ui->xray_kcp_tti, Checks::XrayKCPTti);
    form->bind(ui->xray_kcp_uplink_capacity, Checks::XrayKCPCapacity);
    form->bind(ui->xray_kcp_downlink_capacity, Checks::XrayKCPCapacity);
    form->bind(ui->xray_kcp_cwnd_multiplier, Checks::XrayKCPCwndMultiplier);
    form->addRule({ui->xray_kcp_max_sending_window, ui->xray_kcp_mtu}, [this] {
        return Checks::XrayKCPMaxSendingWindow(ui->xray_kcp_max_sending_window->text(), ui->xray_kcp_mtu->text());
    }, nullptr, Severity::Error, ui->xray_kcp_max_sending_window_l->text());

    const auto replaced = [this] { return ui->xray_network->currentText() == "kcp" && !xdnsResolvers->save().IsEmpty(); };
    form->addRule({ui->xray_finalmask_edit}, [this, replaced] { return finalmaskIssue(CACHE.XrayFinalmask, Severity::Error, replaced()); });
    form->addRule({ui->xray_finalmask_edit}, [this, replaced] { return finalmaskIssue(CACHE.XrayFinalmask, Severity::Warning, replaced()); },
                  nullptr, Severity::Warning);
    xdnsResolvers->bindValidation(*form);
}
