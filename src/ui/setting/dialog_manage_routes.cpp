#include "include/ui/setting/dialog_manage_routes.h"

#include <QClipboard>

#include "include/dataStore/Database.hpp"

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "include/global/GuiUtils.hpp"
#include "include/configs/proxy/Preset.hpp"

#include <QFile>
#include <QMessageBox>
#include <QShortcut>
#include <QTimer>
#include <QToolTip>
#include <include/api/RPC.h>

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

DialogManageRoutes::DialogManageRoutes(QWidget *parent, const std::map<std::string, std::string>& dataMap) : QDialog(parent), ui(new Ui::DialogManageRoutes) {
    ui->setupUi(this);
    auto profiles = Configs::profileManager->routes;
    for (const auto &item: profiles) {
        chainList << item.second;
    }
    if (chainList.empty()) {
        auto defaultChain = Configs::RoutingChain::GetDefaultChain();
        Configs::profileManager->AddRouteChain(defaultChain);
        chainList.append(defaultChain);
    }
    currentRoute = Configs::profileManager->GetRouteChain(Configs::dataStore->routing->current_route_id);
    if (currentRoute == nullptr) currentRoute = chainList[0];

    QStringList qsValue = {""};
    QString dnsHelpDocumentUrl;

    ui->outbound_domain_strategy->addItems(Preset::SingBox::DomainStrategy);
    ui->domainStrategyCombo->addItems(Preset::SingBox::DomainStrategy);
    qsValue += QString("prefer_ipv4 prefer_ipv6 ipv4_only ipv6_only").split(" ");
    ui->dns_object->setPlaceholderText(DecodeB64IfValid("ewogICJzZXJ2ZXJzIjogW10sCiAgInJ1bGVzIjogW10sCiAgImZpbmFsIjogIiIsCiAgInN0cmF0ZWd5IjogIiIsCiAgImRpc2FibGVfY2FjaGUiOiBmYWxzZSwKICAiZGlzYWJsZV9leHBpcmUiOiBmYWxzZSwKICAiaW5kZXBlbmRlbnRfY2FjaGUiOiBmYWxzZSwKICAicmV2ZXJzZV9tYXBwaW5nIjogZmFsc2UsCiAgImZha2VpcCI6IHt9Cn0="));
    dnsHelpDocumentUrl = "https://sing-box.sagernet.org/configuration/dns/";

    ui->direct_dns_strategy->addItems(qsValue);
    ui->remote_dns_strategy->addItems(qsValue);
    ui->enable_fakeip->setChecked(Configs::dataStore->fake_dns);
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
    ui->sniffing_mode->setCurrentIndex(Configs::dataStore->routing->sniffing_mode);
    ui->ruleset_mirror->setCurrentIndex(Configs::dataStore->routing->ruleset_mirror);
    ui->outbound_domain_strategy->setCurrentText(Configs::dataStore->routing->outbound_domain_strategy);
    ui->domainStrategyCombo->setCurrentText(Configs::dataStore->routing->domain_strategy);
    ui->use_dns_object->setChecked(Configs::dataStore->routing->use_dns_object);
    ui->dns_object->setPlainText(Configs::dataStore->routing->dns_object);
    ui->remote_dns->setCurrentText(Configs::dataStore->routing->remote_dns);
    ui->remote_dns_strategy->setCurrentText(Configs::dataStore->routing->remote_dns_strategy);
    ui->direct_dns->setCurrentText(Configs::dataStore->routing->direct_dns);
    ui->direct_dns_strategy->setCurrentText(Configs::dataStore->routing->direct_dns_strategy);
    ui->dns_final_out->setCurrentText(Configs::dataStore->routing->dns_final_out);
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
    ui->dnshijack_enable->setChecked(Configs::dataStore->enable_dns_server);
    set_dns_hijack_enability(Configs::dataStore->enable_dns_server);
    ui->dnshijack_allow_lan->setChecked(Configs::dataStore->dns_server_listen_lan);
    ui->dnshijack_listenport->setValidator(QRegExpValidator_Number);
    ui->dnshijack_listenport->setText(Int2String(Configs::dataStore->dns_server_listen_port));
    ui->dnshijack_v4resp->setText(Configs::dataStore->dns_v4_resp);
    ui->dnshijack_v6resp->setText(Configs::dataStore->dns_v6_resp);
    connect(ui->dnshijack_what, &QPushButton::clicked, this, [=,this] {
        MessageBoxInfo("What is this?", Configs::Information::HijackInfo);
    });

    ruleSetMap = dataMap;
    QStringList ruleItems = {"domain:", "suffix:", "regex:"};
    for (const auto& item : ruleSetMap) {
        ruleItems.append("ruleset:" + QString::fromStdString(item.first));
    }
    rule_editor = new AutoCompleteTextEdit("", ruleItems, this);
    ui->hijack_box->layout()->replaceWidget(ui->dnshijack_rules, rule_editor);
    rule_editor->setPlainText(Configs::dataStore->dns_server_rules.join("\n"));
    ui->dnshijack_rules->hide();
#ifndef Q_OS_LINUX
    ui->dnshijack_listenport->setVisible(false);
    ui->dnshijack_listenport_l->setVisible(false);
#endif

    ui->redirect_enable->setChecked(Configs::dataStore->enable_redirect);
    ui->redirect_listenaddr->setEnabled(Configs::dataStore->enable_redirect);
    ui->redirect_listenaddr->setText(Configs::dataStore->redirect_listen_address);
    ui->redirect_listenport->setEnabled(Configs::dataStore->enable_redirect);
    ui->redirect_listenport->setValidator(QRegExpValidator_Number);
    ui->redirect_listenport->setText(Int2String(Configs::dataStore->redirect_listen_port));

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

    Configs::dataStore->routing->sniffing_mode = ui->sniffing_mode->currentIndex();
    Configs::dataStore->routing->ruleset_mirror = ui->ruleset_mirror->currentIndex();
    Configs::dataStore->routing->domain_strategy = ui->domainStrategyCombo->currentText();
    Configs::dataStore->routing->outbound_domain_strategy = ui->outbound_domain_strategy->currentText();
    Configs::dataStore->routing->use_dns_object = ui->use_dns_object->isChecked();
    Configs::dataStore->routing->dns_object = ui->dns_object->toPlainText();
    Configs::dataStore->routing->remote_dns = ui->remote_dns->currentText();
    Configs::dataStore->routing->remote_dns_strategy = ui->remote_dns_strategy->currentText();
    Configs::dataStore->routing->direct_dns = ui->direct_dns->currentText();
    Configs::dataStore->routing->direct_dns_strategy = ui->direct_dns_strategy->currentText();
    Configs::dataStore->routing->dns_final_out = ui->dns_final_out->currentText();
    Configs::dataStore->fake_dns = ui->enable_fakeip->isChecked();

    Configs::profileManager->UpdateRouteChains(chainList);
    Configs::dataStore->routing->current_route_id = currentRoute->id;

    Configs::dataStore->enable_dns_server = ui->dnshijack_enable->isChecked();
    Configs::dataStore->dns_server_listen_port = ui->dnshijack_listenport->text().toInt();
    Configs::dataStore->dns_v4_resp = ui->dnshijack_v4resp->text();
    Configs::dataStore->dns_v6_resp = ui->dnshijack_v6resp->text();
    auto rawRules = rule_editor->toPlainText().split("\n");
    QStringList dnsRules;
    for (const auto& rawRule : rawRules) {
        if (rawRule.trimmed().isEmpty()) continue;
        dnsRules.append(rawRule.trimmed());
    }
    Configs::dataStore->dns_server_rules = dnsRules;

    Configs::dataStore->dns_server_listen_lan = ui->dnshijack_allow_lan->isChecked();
    Configs::dataStore->enable_redirect = ui->redirect_enable->isChecked();
    Configs::dataStore->redirect_listen_address = ui->redirect_listenaddr->text();
    Configs::dataStore->redirect_listen_port = ui->redirect_listenport->text().toInt();

    //
    QStringList msg{"UpdateDataStore"};
    msg << "RouteChanged";
    MW_dialog_message("", msg.join(","));

    QDialog::accept();
}

void DialogManageRoutes::on_new_route_clicked() {
    routeChainWidget = new RouteItem(this, Configs::ProfileManager::NewRouteChain(), ruleSetMap);
    routeChainWidget->setWindowModality(Qt::ApplicationModal);
    routeChainWidget->show();
    connect(routeChainWidget, &RouteItem::settingsChanged, this, [=,this](const std::shared_ptr<Configs::RoutingChain>& chain) {
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

    auto chainCopy = std::make_shared<Configs::RoutingChain>(*chainList[idx]);
    chainCopy->name = chainCopy->name + " clone";
    chainCopy->id = -1;
    chainCopy->save_control_no_save = false;
    chainList.append(chainCopy);
    reloadProfileItems();
}

void DialogManageRoutes::on_edit_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;

    routeChainWidget = new RouteItem(this, chainList[idx], ruleSetMap);
    routeChainWidget->setWindowModality(Qt::ApplicationModal);
    routeChainWidget->show();
    connect(routeChainWidget, &RouteItem::settingsChanged, this, [=,this](const std::shared_ptr<Configs::RoutingChain>& chain) {
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
