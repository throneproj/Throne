#include "include/ui/setting/RouteItem.h"
#include "include/dataStore/RouteEntity.h"
#include "include/dataStore/Database.hpp"
#include "include/api/gRPC.h"

void adjustComboBoxWidth(const QComboBox *comboBox) {
    int maxWidth = 0;

    // Iterate over all items and calculate the width required
    for (int i = 0; i < comboBox->count(); ++i) {
        QFontMetrics fontMetrics(comboBox->font());
        int itemWidth = fontMetrics.horizontalAdvance(comboBox->itemText(i));
        maxWidth = qMax(maxWidth, itemWidth);
    }

    // Add some padding to the width to avoid text being too close to the edge
    maxWidth += 30;

    // Set the minimum width for the drop-down menu
    comboBox->view()->setMinimumWidth(maxWidth);
}

int RouteItem::getIndexOf(const QString& name) const {
    for (int i=0;i<chain->Rules.size();i++) {
        if (chain->Rules[i]->name == name) return i;
    }

    return -1;
}

QString get_outbound_name(int id) {
    // -1 is proxy -2 is direct -3 is block -4 is dns-out
    if (id == -1) return "proxy";
    if (id == -2) return "direct";
    auto profiles = Configs::profileManager->profiles;
    if (profiles.count(id)) return profiles[id]->bean->name;
    return "INVALID OUTBOUND";
}

QStringList get_all_outbounds() {
    QStringList res;
    auto profiles = Configs::profileManager->profiles;
    for (const auto &item: profiles) {
        res.append(item.second->bean->DisplayName());
    }

    return res;
}

RouteItem::RouteItem(QWidget *parent, const std::shared_ptr<Configs::RoutingChain>& routeChain)
    : QDialog(parent), ui(new Ui::RouteItem) {
    ui->setupUi(this);

    // make a copy
    chain = std::make_shared<Configs::RoutingChain>(*routeChain);

    // add the default rule if empty
    if (chain->Rules.empty()) {
        auto routeItem = std::make_shared<Configs::RouteRule>();
        routeItem->name = "dns-hijack";
        routeItem->protocol = "dns";
        routeItem->action = "hijack-dns";
        chain->Rules << routeItem;
    }

    // setup rule set helper
    bool ok; // for now we discard this
    auto geoIpList = API::defaultClient->GetGeoList(&ok, API::GeoRuleSetType::ip, Configs::GetCoreAssetDir("geoip.db"));
    auto geoSiteList = API::defaultClient->GetGeoList(&ok, API::GeoRuleSetType::site, Configs::GetCoreAssetDir("geosite.db"));
    geo_items << geoIpList << geoSiteList;
    rule_set_editor = new AutoCompleteTextEdit("", geo_items, this);
    ui->rule_attr_data->layout()->addWidget(rule_set_editor);
    ui->rule_attr_data->adjustSize();
    rule_set_editor->hide();
    connect(rule_set_editor, &QPlainTextEdit::textChanged, this, [=]{
        if (currentIndex == -1) return;
        auto currentVal = rule_set_editor->toPlainText().split('\n');
        chain->Rules[currentIndex]->set_field_value(ui->rule_attr->currentText(), currentVal);
        updateRulePreview();
    });

    std::map<QString, int> valueMap;
    for (auto &item: chain->Rules) {
        auto baseName = item->name;
        int randPart;
        if (baseName == "") {
            randPart = int(GetRandomUint64()%1000);
            baseName = "rule_" + Int2String(randPart);
            lastNum = std::max(lastNum, randPart);
        }
        while (true) {
            valueMap[baseName]++;
            if (valueMap[baseName] > 1) {
                valueMap[baseName]--;
                randPart = int(GetRandomUint64()%1000);
                baseName = "rule_" + Int2String(randPart);
                lastNum = std::max(lastNum, randPart);
                continue;
            }
            item->name = baseName;
            break;
        }
        ui->route_items->addItem(item->name);
    }

    outbounds = {"proxy", "direct"};
    outbounds << get_all_outbounds();
    // init outbound map
    outboundMap[0] = -1;
    outboundMap[1] = -2;
    for (const auto& item: Configs::profileManager->profiles) {
        outboundMap[outboundMap.size()] = item.second->id;
    }

    ui->route_name->setText(chain->name);
    ui->rule_attr->addItems(Configs::RouteRule::get_attributes());
    adjustComboBoxWidth(ui->rule_attr);
    ui->rule_attr_text->hide();
    ui->rule_attr_data->setTitle("");
    ui->rule_preview->setReadOnly(true);
    updateRuleSection();

    ui->def_out->setCurrentText(Configs::outboundIDToString(chain->defaultOutboundID));

    // simple rules setup
    QStringList ruleItems = {"domain:", "suffix:", "regex:", "keyword:", "ip:", "processName:", "processPath:", "ruleset:"};
    for (const auto& geoIP : geoIpList) {
        ruleItems.append("ruleset:"+geoIP);
    }
    for (const auto& geoSite: geoSiteList) {
        ruleItems.append("ruleset:"+geoSite);
    }
    simpleDirect = new AutoCompleteTextEdit("", ruleItems, this);
    simpleBlock = new AutoCompleteTextEdit("", ruleItems, this);
    simpleProxy = new AutoCompleteTextEdit("", ruleItems, this);

    ui->simple_direct_box->layout()->replaceWidget(ui->simple_direct, simpleDirect);
    ui->simple_block_box->layout()->replaceWidget(ui->simple_block, simpleBlock);
    ui->simple_proxy_box->layout()->replaceWidget(ui->simple_proxy, simpleProxy);
    ui->simple_direct->hide();
    ui->simple_block->hide();
    ui->simple_proxy->hide();

    simpleDirect->setPlainText(chain->GetSimpleRules(Configs::direct));
    simpleBlock->setPlainText(chain->GetSimpleRules(Configs::block));
    simpleProxy->setPlainText(chain->GetSimpleRules(Configs::proxy));

    connect(ui->tabWidget->tabBar(), &QTabBar::currentChanged, this, [=]()
    {
        if (ui->tabWidget->tabBar()->currentIndex() == 1)
        {
            QString res;
            res += chain->UpdateSimpleRules(simpleDirect->toPlainText(), Configs::direct);
            res += chain->UpdateSimpleRules(simpleBlock->toPlainText(), Configs::block);
            res += chain->UpdateSimpleRules(simpleProxy->toPlainText(), Configs::proxy);
            if (!res.isEmpty())
            {
                runOnUiThread([=]
                {
                    MessageBoxWarning(tr("Invalid rules"), tr("Some rules could not be added:\n") + res);
                });
            }
            currentIndex  = -1;
            updateRouteItemsView();
            updateRuleSection();
        } else
        {
            // reload
            updateRouteItemsView();
            updateRuleSection();
            simpleDirect->setPlainText(chain->GetSimpleRules(Configs::direct));
            simpleBlock->setPlainText(chain->GetSimpleRules(Configs::block));
            simpleProxy->setPlainText(chain->GetSimpleRules(Configs::proxy));
        }
    });

    connect(ui->howtouse_button, &QPushButton::clicked, this, [=]()
    {
        runOnUiThread([=]
        {
            MessageBoxInfo(tr("Simple rule manual"), Configs::Information::SimpleRuleInfo);
        });
    });

    connect(ui->route_import_json, &QPushButton::clicked, this, [=] {
        auto w = new QDialog(this);
        w->setWindowTitle("Import JSON Array");
        w->setWindowModality(Qt::ApplicationModal);

        auto line = 0;
        auto layout = new QGridLayout;
        w->setLayout(layout);

        auto *tEdit = new QTextEdit;
        tEdit->setPlaceholderText("[\n"
            "      {\n"
            "        \"action\": \"hijack-dns\",\n"
            "        \"protocol\": \"dns\"\n"
            "      },\n"
            "      {\n"
            "        \"action\": \"reject\",\n"
            "        \"protocol\": \"udp\"\n"
            "      }\n"
            "    ]");
        layout->addWidget(tEdit, line++, 0);

        auto *buttons = new QDialogButtonBox;
        buttons->setOrientation(Qt::Horizontal);
        buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons, line, 0);

        connect(buttons, &QDialogButtonBox::accepted, w, [=]{
           auto err = new QString;
           auto parsed = Configs::RoutingChain::parseJsonArray(QString2QJsonArray(tEdit->toPlainText()), err);
           if (!err->isEmpty()) {
               MessageBoxInfo(tr("Invalid JSON Array"), tr("The provided input cannot be parsed to a valid route rule array:\n") + *err);
               return;
           }
           chain->Rules.clear();
           chain->Rules << parsed;
           updateRouteItemsView();
           updateRuleSection();

           w->accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

        w->exec();
        w->deleteLater();
    });

    connect(ui->rule_name, &QLineEdit::textChanged, this, [=](const QString& text) {
        if (currentIndex == -1) return;
        chain->Rules[currentIndex]->name = QString(text);
        updateRouteItemsView();
    });

    connect(ui->rule_attr_selector, &QComboBox::currentTextChanged, this, [=](const QString& text){
       if (currentIndex == -1) return;
       if (ui->rule_attr->currentText() == "outbound")
       {
           chain->Rules[currentIndex]->set_field_value("outbound", {QString::number(outboundMap[ui->rule_attr_selector->currentIndex()])});
           updateRulePreview();
           return;
       }
       chain->Rules[currentIndex]->set_field_value(ui->rule_attr->currentText(), {QString(text)});
       updateRulePreview();
    });

    connect(ui->rule_attr_text, &QPlainTextEdit::textChanged, this, [=] {
        if (currentIndex == -1) return;
        auto currentVal = ui->rule_attr_text->toPlainText().split('\n');
        chain->Rules[currentIndex]->set_field_value(ui->rule_attr->currentText(), currentVal);
        updateRulePreview();
    });

    connect(ui->route_items, &QListWidget::currentRowChanged, this, [=](const int idx) {
        if (idx == -1) return;
        currentIndex = idx;
        updateRuleSection();
    });

    connect(ui->rule_attr, &QComboBox::currentTextChanged, this, [=](const QString& text){
        updateRuleSection();
    });

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [=]{
        accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, [=]{
       QDialog::reject();
    });

    deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);

    connect(deleteShortcut, &QShortcut::activated, this, [=]{
        on_delete_route_item_clicked();
    });

    adjustSize();
}

RouteItem::~RouteItem() {
    delete ui;
}

void RouteItem::accept() {
    chain->name = ui->route_name->text();

    if (chain->name == "") {
        MessageBoxWarning(tr("Invalid operation"), tr("Cannot create Route Profile with empty name"));
        return;
    }

    QList<std::shared_ptr<Configs::RouteRule>> tmpChain;
    for (const auto& item: chain->Rules) {
        if (!item->isEmpty()) {
            tmpChain << item;
        }
    }
    chain->Rules.clear();
    for (const auto& item: tmpChain) {
        chain->Rules << item;
    }

    if (chain->Rules.empty()) {
        MessageBoxInfo(tr("Empty Route Profile"), tr("No valid rules are in the profile"));
        return;
    }

    QString res;
    res += chain->UpdateSimpleRules(simpleDirect->toPlainText(), Configs::direct);
    res += chain->UpdateSimpleRules(simpleBlock->toPlainText(), Configs::block);
    res += chain->UpdateSimpleRules(simpleProxy->toPlainText(), Configs::proxy);
    if (!res.isEmpty())
    {
        runOnUiThread([=]
        {
            MessageBoxWarning(tr("Invalid rules"), tr("Some rules could not be added, fix them before saving:\n") + res);
        });
        return;
    }
    chain->defaultOutboundID = Configs::stringToOutboundID(ui->def_out->currentText());

    emit settingsChanged(chain);

    QDialog::accept();
}

void RouteItem::updateRouteItemsView() {
    ui->route_items->clear();
    if (chain->Rules.empty()) return;

    for (const auto& item: chain->Rules) {
        ui->route_items->addItem(item->name);
    }
    if (currentIndex != -1) ui->route_items->setCurrentRow(currentIndex);
}

void RouteItem::updateRuleSection() {
    if (currentIndex == -1) return;

    auto ruleItem = chain->Rules[currentIndex];
    auto currentAttr = ui->rule_attr->currentText();
    switch (ruleItem->get_input_type(currentAttr)) {
        case Configs::trufalse: {
            QStringList items = {"false", "true"};
            QString currentVal = ruleItem->get_current_value_bool(currentAttr);
            showSelectItem(items, currentVal);
            break;
        }
        case Configs::select: {
            if (currentAttr == "outbound")
            {
                // due to the need for mapping, we handle this in a different way...
                showSelectItem(outbounds, get_outbound_name(ruleItem->outboundID));
                break;
            }
            auto items = Configs::RouteRule::get_values_for_field(currentAttr);
            auto currentVal = ruleItem->get_current_value_string(currentAttr)[0];
            showSelectItem(items, currentVal);
            break;
        }
        case Configs::text: {
            auto currentItems = ruleItem->get_current_value_string(currentAttr);
            showTextEnterItem(currentItems, currentAttr == "rule_set");
            break;
        }
    }
    ui->rule_name->setText(ruleItem->name);

    updateRulePreview();
}

void RouteItem::updateRulePreview() {
    if (currentIndex == -1) return;

    ui->rule_preview->setText(QJsonObject2QString(chain->Rules[currentIndex]->get_rule_json(true), false));
}

void RouteItem::setDefaultRuleData(const QString& currentData) {
    ui->rule_attr->setCurrentText("ip_version");
    ui->rule_attr_data->setTitle("ip_version");
    showSelectItem(Configs::RouteRule::get_values_for_field("ip_version"), currentData);
}

void RouteItem::showSelectItem(const QStringList& items, const QString& currentItem) {
    ui->rule_attr_text->hide();
    rule_set_editor->hide();
    ui->rule_attr_selector->clear();
    ui->rule_attr_selector->show();
    ui->rule_attr_selector->addItems(items);
    ui->rule_attr_selector->setCurrentText(currentItem);
    adjustComboBoxWidth(ui->rule_attr_selector);
    adjustSize();
}

void RouteItem::showTextEnterItem(const QStringList& items, bool isRuleSet) {
    ui->rule_attr_selector->hide();
    if (isRuleSet) {
        ui->rule_attr_text->hide();
        rule_set_editor->clear();
        rule_set_editor->show();
        rule_set_editor->setPlainText(items.join('\n'));
    } else {
        rule_set_editor->hide();
        ui->rule_attr_text->clear();
        ui->rule_attr_text->show();
        ui->rule_attr_text->setPlainText(items.join('\n'));
    }
    adjustSize();
}

void RouteItem::on_new_route_item_clicked() {
    auto routeItem = std::make_shared<Configs::RouteRule>();
    routeItem->name = "rule_" + Int2String(++lastNum);
    chain->Rules << routeItem;
    currentIndex = chain->Rules.size() - 1;
    ui->rule_name->setText(routeItem->name);
    currentIndex = chain->Rules.size()-1;

    updateRouteItemsView();
    updateRuleSection();
}

void RouteItem::on_moveup_route_item_clicked() {
    if (currentIndex == -1 || currentIndex == 0) return;
    auto curr = chain->Rules[currentIndex];
    chain->Rules[currentIndex] = chain->Rules[currentIndex-1];
    chain->Rules[currentIndex-1] = curr;
    currentIndex--;
    updateRouteItemsView();
}

void RouteItem::on_movedown_route_item_clicked() {
    if (currentIndex == -1 || currentIndex == chain->Rules.size() - 1) return;
    auto curr = chain->Rules[currentIndex];
    chain->Rules[currentIndex] = chain->Rules[currentIndex+1];
    chain->Rules[currentIndex+1] = curr;
    currentIndex++;
    updateRouteItemsView();
}

void RouteItem::on_delete_route_item_clicked() {
    if (currentIndex == -1) return;
    chain->Rules.removeAt(currentIndex);
    if (chain->Rules.empty()) currentIndex = -1;
    else {
        currentIndex--;
        if (currentIndex == -1) currentIndex = 0;
    }
    updateRouteItemsView();
    updateRuleSection();
}
