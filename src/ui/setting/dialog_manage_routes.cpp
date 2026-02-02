#include "include/ui/setting/dialog_manage_routes.h"

#include <QClipboard>

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "include/global/GuiUtils.hpp"

#include <QFile>
#include <QMessageBox>
#include <QShortcut>
#include <QTimer>
#include <QToolTip>
#include <include/api/RPC.h>

#include "include/database/RoutesRepo.h"



void DialogManageRoutes::reloadProfileItems() {
    if (chainList.empty()) {
        MessageBoxWarning(tr("Invalid state"), tr("The list of routing profiles is empty, this should be an unreachable state, crashes may occur now"));
        return;
    }

    QSignalBlocker blocker = QSignalBlocker(ui->route_prof); // apparently the currentIndexChanged will make us crash if we clear the QComboBox
    ui->route_prof->clear();

    ui->route_profiles->clear();
    bool selectedChainGone = true;
    int i=0;
    for (const auto &item: chainList) {
        ui->route_prof->addItem(item->name);
        ui->route_profiles->addItem(item->name);
        if (item == currentRoute) {
            ui->route_prof->setCurrentIndex(i);
            selectedChainGone=false;
        }
        i++;
    }
    if (selectedChainGone) {
        currentRoute=chainList[0];
        ui->route_prof->setCurrentIndex(0);
    }
    blocker.unblock();
}

void DialogManageRoutes::set_dns_hijack_enability(const bool enable) const {
    ui->dnshijack_allow_lan->setEnabled(enable);
    ui->dnshijack_listenport->setEnabled(enable);
    ui->dnshijack_rules->setEnabled(enable);
    ui->dnshijack_v4resp->setEnabled(enable);
    ui->dnshijack_v6resp->setEnabled(enable);
}

bool DialogManageRoutes::validate_dns_rules(const QString &rawString) {
    auto rules = rawString.split("\n");
    for (const auto& rule : rules) {
        if (!rule.trimmed().isEmpty() && !rule.startsWith("ruleset:") && !rule.startsWith("domain:") && !rule.startsWith("suffix:") && !rule.startsWith("regex:")) return false;
    }
    return true;
}

DialogManageRoutes::DialogManageRoutes(QWidget *parent) : QDialog(parent), ui(new Ui::DialogManageRoutes) {
    ui->setupUi(this);
    auto profiles = Configs::dataManager->routesRepo->GetAllRouteProfiles();
    for (const auto &item: profiles) {
        chainList << item;
    }
    if (chainList.empty()) {
        auto defaultChain = Configs::RouteProfile::GetDefaultChain();
        Configs::dataManager->routesRepo->AddRouteProfile(defaultChain);
        chainList.append(defaultChain);
    }
    currentRoute = Configs::dataManager->routesRepo->GetRouteProfile(Configs::dataManager->settingsRepo->current_route_id);
    if (currentRoute == nullptr) currentRoute = chainList[0];

    QStringList qsValue = {""};
    QString dnsHelpDocumentUrl;

    ui->outbound_domain_strategy->addItems(Configs::DomainStrategy::DomainStrategy);
    ui->domainStrategyCombo->addItems(Configs::DomainStrategy::DomainStrategy);
    qsValue += QString("prefer_ipv4 prefer_ipv6 ipv4_only ipv6_only").split(" ");
    ui->dns_object->setPlaceholderText(DecodeB64IfValid("ewogICJzZXJ2ZXJzIjogW10sCiAgInJ1bGVzIjogW10sCiAgImZpbmFsIjogIiIsCiAgInN0cmF0ZWd5IjogIiIsCiAgImRpc2FibGVfY2FjaGUiOiBmYWxzZSwKICAiZGlzYWJsZV9leHBpcmUiOiBmYWxzZSwKICAiaW5kZXBlbmRlbnRfY2FjaGUiOiBmYWxzZSwKICAicmV2ZXJzZV9tYXBwaW5nIjogZmFsc2UsCiAgImZha2VpcCI6IHt9Cn0="));
    dnsHelpDocumentUrl = "https://sing-box.sagernet.org/configuration/dns/";

    ui->direct_dns_strategy->addItems(qsValue);
    ui->remote_dns_strategy->addItems(qsValue);
    ui->local_override->setText(Configs::dataManager->settingsRepo->core_box_underlying_dns);
    ui->enable_fakeip->setChecked(Configs::dataManager->settingsRepo->fake_dns);
    //
    connect(ui->use_dns_object, &QCheckBox::stateChanged, this, [=,this](int state) {
        auto useDNSObject = state == Qt::Checked;
        ui->simple_dns_box->setDisabled(useDNSObject);
        ui->dns_object->setDisabled(!useDNSObject);
    });
    ui->use_dns_object->stateChanged(Qt::Unchecked); // uncheck to uncheck
    connect(ui->dns_document, &QPushButton::clicked, this, [=,this] {
        MessageBoxInfo("DNS", dnsHelpDocumentUrl);
    });
    connect(ui->format_dns_object, &QPushButton::clicked, this, [=,this] {
        auto obj = QString2QJsonObject(ui->dns_object->toPlainText());
        if (obj.isEmpty()) {
            MessageBoxInfo("DNS", "invaild json");
        } else {
            ui->dns_object->setPlainText(QJsonObject2QString(obj, false));
        }
    });
    ui->sniffing_mode->setCurrentIndex(Configs::dataManager->settingsRepo->sniffing_mode);
    ui->ruleset_mirror->setCurrentIndex(Configs::dataManager->settingsRepo->ruleset_mirror);
    ui->outbound_domain_strategy->setCurrentText(Configs::dataManager->settingsRepo->outbound_domain_strategy);
    ui->domainStrategyCombo->setCurrentText(Configs::dataManager->settingsRepo->domain_strategy);
    ui->use_dns_object->setChecked(Configs::dataManager->settingsRepo->use_dns_object);
    ui->dns_object->setPlainText(Configs::dataManager->settingsRepo->dns_object);
    ui->remote_dns->setCurrentText(Configs::dataManager->settingsRepo->remote_dns);
    ui->remote_dns_strategy->setCurrentText(Configs::dataManager->settingsRepo->remote_dns_strategy);
    ui->direct_dns->setCurrentText(Configs::dataManager->settingsRepo->direct_dns);
    ui->direct_dns_strategy->setCurrentText(Configs::dataManager->settingsRepo->direct_dns_strategy);
    ui->dns_final_out->setCurrentText(Configs::dataManager->settingsRepo->dns_final_out);
    reloadProfileItems();

    connect(ui->route_profiles, &QListWidget::itemDoubleClicked, this, [=,this](const QListWidgetItem* item){
        on_edit_route_clicked();
    });

    connect(ui->route_prof, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCurrentRouteProfile(int)));

    deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);

    connect(deleteShortcut, &QShortcut::activated, this, [=,this]{
        on_delete_route_clicked();
    });

    // hijack
    ui->dnshijack_enable->setChecked(Configs::dataManager->settingsRepo->enable_dns_server);
    set_dns_hijack_enability(Configs::dataManager->settingsRepo->enable_dns_server);
    ui->dnshijack_allow_lan->setChecked(Configs::dataManager->settingsRepo->dns_server_listen_lan);
    ui->dnshijack_listenport->setValidator(QRegExpValidator_Number);
    ui->dnshijack_listenport->setText(Int2String(Configs::dataManager->settingsRepo->dns_server_listen_port));
    ui->dnshijack_v4resp->setText(Configs::dataManager->settingsRepo->dns_v4_resp);
    ui->dnshijack_v6resp->setText(Configs::dataManager->settingsRepo->dns_v6_resp);
    connect(ui->dnshijack_what, &QPushButton::clicked, this, [=,this] {
        MessageBoxInfo("What is this?", Configs::Information::HijackInfo);
    });

    QStringList ruleItems = {"domain:", "suffix:", "regex:"};
    for (const auto& item : ruleSetMap) {
        ruleItems.append("ruleset:" + QString::fromStdString(item.first));
    }
    rule_editor = new AutoCompleteTextEdit("", ruleItems, this);
    ui->hijack_box->layout()->replaceWidget(ui->dnshijack_rules, rule_editor);
    rule_editor->setPlainText(Configs::dataManager->settingsRepo->dns_server_rules.join("\n"));
    ui->dnshijack_rules->hide();
#ifndef Q_OS_LINUX
    ui->dnshijack_listenport->setVisible(false);
    ui->dnshijack_listenport_l->setVisible(false);
#endif

    ui->redirect_enable->setChecked(Configs::dataManager->settingsRepo->enable_redirect);
    ui->redirect_listenaddr->setEnabled(Configs::dataManager->settingsRepo->enable_redirect);
    ui->redirect_listenaddr->setText(Configs::dataManager->settingsRepo->redirect_listen_address);
    ui->redirect_listenport->setEnabled(Configs::dataManager->settingsRepo->enable_redirect);
    ui->redirect_listenport->setValidator(QRegExpValidator_Number);
    ui->redirect_listenport->setText(Int2String(Configs::dataManager->settingsRepo->redirect_listen_port));

    connect(ui->dnshijack_enable, &QCheckBox::stateChanged, this, [=,this](bool state) {
        set_dns_hijack_enability(state);
    });
    connect(ui->redirect_enable, &QCheckBox::stateChanged, this, [=,this](bool state) {
        ui->redirect_listenaddr->setEnabled(state);
        ui->redirect_listenport->setEnabled(state);
    });

    ADD_ASTERISK(this)
}

void DialogManageRoutes::updateCurrentRouteProfile(int idx) {
    currentRoute = chainList[idx];
}

DialogManageRoutes::~DialogManageRoutes() {
    delete ui;
}

void DialogManageRoutes::accept() {
    if (chainList.empty()) {
        MessageBoxInfo(tr("Invalid settings"), tr("Routing profile cannot be empty"));
        return;
    }
    if (!validate_dns_rules(rule_editor->toPlainText())) {
        MessageBoxInfo(tr("Invalid settings"), tr("DNS Rules are not valid"));
        return;
    }

    Configs::dataManager->settingsRepo->sniffing_mode = ui->sniffing_mode->currentIndex();
    Configs::dataManager->settingsRepo->ruleset_mirror = ui->ruleset_mirror->currentIndex();
    Configs::dataManager->settingsRepo->domain_strategy = ui->domainStrategyCombo->currentText();
    Configs::dataManager->settingsRepo->outbound_domain_strategy = ui->outbound_domain_strategy->currentText();
    Configs::dataManager->settingsRepo->use_dns_object = ui->use_dns_object->isChecked();
    Configs::dataManager->settingsRepo->dns_object = ui->dns_object->toPlainText();
    Configs::dataManager->settingsRepo->remote_dns = ui->remote_dns->currentText();
    Configs::dataManager->settingsRepo->remote_dns_strategy = ui->remote_dns_strategy->currentText();
    Configs::dataManager->settingsRepo->direct_dns = ui->direct_dns->currentText();
    Configs::dataManager->settingsRepo->direct_dns_strategy = ui->direct_dns_strategy->currentText();
    Configs::dataManager->settingsRepo->core_box_underlying_dns = ui->local_override->text().trimmed();
    Configs::dataManager->settingsRepo->dns_final_out = ui->dns_final_out->currentText();
    Configs::dataManager->settingsRepo->fake_dns = ui->enable_fakeip->isChecked();

    Configs::dataManager->routesRepo->UpdateRouteProfiles(chainList);
    Configs::dataManager->settingsRepo->current_route_id = currentRoute->id;

    Configs::dataManager->settingsRepo->enable_dns_server = ui->dnshijack_enable->isChecked();
    Configs::dataManager->settingsRepo->dns_server_listen_port = ui->dnshijack_listenport->text().toInt();
    Configs::dataManager->settingsRepo->dns_v4_resp = ui->dnshijack_v4resp->text();
    Configs::dataManager->settingsRepo->dns_v6_resp = ui->dnshijack_v6resp->text();
    auto rawRules = rule_editor->toPlainText().split("\n");
    QStringList dnsRules;
    for (const auto& rawRule : rawRules) {
        if (rawRule.trimmed().isEmpty()) continue;
        dnsRules.append(rawRule.trimmed());
    }
    Configs::dataManager->settingsRepo->dns_server_rules = dnsRules;

    Configs::dataManager->settingsRepo->dns_server_listen_lan = ui->dnshijack_allow_lan->isChecked();
    Configs::dataManager->settingsRepo->enable_redirect = ui->redirect_enable->isChecked();
    Configs::dataManager->settingsRepo->redirect_listen_address = ui->redirect_listenaddr->text();
    Configs::dataManager->settingsRepo->redirect_listen_port = ui->redirect_listenport->text().toInt();

    //
    QStringList msg{"UpdateConfigs::dataManager->settingsRepo"};
    msg << "RouteChanged";
    MW_dialog_message("", msg.join(","));

    QDialog::accept();
}

void DialogManageRoutes::on_new_route_clicked() {
    routeChainWidget = new RouteItem(this, Configs::dataManager->routesRepo->NewRouteProfile());
    routeChainWidget->setWindowModality(Qt::ApplicationModal);
    routeChainWidget->show();
    connect(routeChainWidget, &RouteItem::settingsChanged, this, [=,this](const std::shared_ptr<Configs::RouteProfile>& chain) {
        chainList << chain;
        reloadProfileItems();
    });
}

void DialogManageRoutes::on_export_route_clicked()
{
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;

    QJsonArray arr = chainList[idx]->get_route_rules(true, {});
    QStringList res;
    for (int i = 0; i < arr.count(); i++)
    {
        res.append(QJsonObject2QString(arr[i].toObject(), false));
    }
    QApplication::clipboard()->setText("[" + res.join(",") + "]");

    QToolTip::showText(QCursor::pos(), "Copied!", this);
    int r = ++tooltipID;
    QTimer::singleShot(1500, [=,this] {
        if (tooltipID != r) return;
        QToolTip::hideText();
    });
}

void DialogManageRoutes::on_clone_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;

    auto chainCopy = std::make_shared<Configs::RouteProfile>(*chainList[idx]);
    chainCopy->name = chainCopy->name + " clone";
    chainCopy->id = -1;
    chainList.append(chainCopy);
    reloadProfileItems();
}

void DialogManageRoutes::on_edit_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;

    routeChainWidget = new RouteItem(this, chainList[idx]);
    routeChainWidget->setWindowModality(Qt::ApplicationModal);
    routeChainWidget->show();
    connect(routeChainWidget, &RouteItem::settingsChanged, this, [=,this](const std::shared_ptr<Configs::RouteProfile>& chain) {
        if (currentRoute == chainList[idx]) currentRoute = chain;
        chainList[idx] = chain;
        reloadProfileItems();
    });

}

void DialogManageRoutes::on_delete_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;
    if (chainList.size() == 1) {
        MessageBoxWarning(tr("Invalid operation"), tr("Routing Profiles cannot be empty, try adding another profile or editing this one"));
        return;
    }

    auto profileToDel = chainList[idx];
    chainList.removeAt(idx);
    if (profileToDel == currentRoute) {
        currentRoute = chainList[0];
    }
    reloadProfileItems();
}
