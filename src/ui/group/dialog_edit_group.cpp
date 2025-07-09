#include "include/ui/group/dialog_edit_group.h"

#include "include/dataStore/Database.hpp"
#include "include/ui/mainwindow_interface.h"

#include <QClipboard>
#include <QStringListModel>
#include <QCompleter>

#define ADJUST_SIZE runOnUiThread([=] { adjustSize(); adjustPosition(mainwindow); }, this);

DialogEditGroup::DialogEditGroup(const std::shared_ptr<NekoGui::Group> &ent, QWidget *parent) : QDialog(parent), ui(new Ui::DialogEditGroup) {
    ui->setupUi(this);
    this->ent = ent;

    connect(ui->type, &QComboBox::currentIndexChanged, this, [=](int index) {
        ui->cat_sub->setHidden(index == 0);
        ADJUST_SIZE
    });

    ui->name->setText(ent->name);
    ui->archive->setChecked(ent->archive);
    ui->skip_auto_update->setChecked(ent->skip_auto_update);
    ui->url->setText(ent->url);
    ui->type->setCurrentIndex(ent->url.isEmpty() ? 0 : 1);
    ui->type->currentIndexChanged(ui->type->currentIndex());
    ui->manually_column_width->setChecked(ent->manually_column_width);
    ui->cat_share->setVisible(false);
    if (NekoGui::profileManager->GetProfile(ent->front_proxy_id) == nullptr) {
        ent->front_proxy_id = -1;
        ent->Save();
    }
    if (NekoGui::profileManager->GetProfile(ent->landing_proxy_id) == nullptr) {
        ent->landing_proxy_id = -1;
        ent->Save();
    }
    CACHE.front_proxy = ent->front_proxy_id;
    LANDING.landing_proxy = ent->landing_proxy_id;

    if (ent->id >= 0) { // already a group
        ui->type->setDisabled(true);
        if (!ent->Profiles().isEmpty()) {
            ui->cat_share->setVisible(true);
        }
    }

    auto proxy_items = load_proxy_items();
    ui->front_proxy->addItems(proxy_items);
    ui->front_proxy->setCurrentText(get_proxy_name(CACHE.front_proxy));
    ui->front_proxy->setEditable(true);
    ui->front_proxy->setInsertPolicy(QComboBox::NoInsert);
    auto frontCompleter = new QCompleter(proxy_items, this);
    frontCompleter->setCompletionMode(QCompleter::PopupCompletion);
    frontCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    frontCompleter->setFilterMode(Qt::MatchContains);
    ui->front_proxy->setCompleter(nullptr);
    ui->front_proxy->lineEdit()->setCompleter(frontCompleter);
    connect(ui->front_proxy, &QComboBox::currentTextChanged, this, [=](const QString &txt){
        CACHE.front_proxy = get_proxy_id(txt);
    });

    ui->landing_proxy->addItems(proxy_items);
    ui->landing_proxy->setCurrentText(get_proxy_name(LANDING.landing_proxy));
    ui->landing_proxy->setEditable(true);
    ui->landing_proxy->setInsertPolicy(QComboBox::NoInsert);
    auto landingCompleter = new QCompleter(proxy_items, this);
    landingCompleter->setCompletionMode(QCompleter::PopupCompletion);
    landingCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    landingCompleter->setFilterMode(Qt::MatchContains);
    ui->landing_proxy->setCompleter(nullptr);
    ui->landing_proxy->lineEdit()->setCompleter(frontCompleter);
    connect(ui->landing_proxy, &QComboBox::currentTextChanged, this, [=](const QString &txt){
        LANDING.landing_proxy = get_proxy_id(txt);
    });

    connect(ui->copy_links, &QPushButton::clicked, this, [=] {
        QStringList links;
        for (const auto &[_, profile]: NekoGui::profileManager->profiles) {
            if (profile->gid != ent->id) continue;
            links += profile->bean->ToShareLink();
        }
        QApplication::clipboard()->setText(links.join("\n"));
        MessageBoxInfo(software_name, tr("Copied"));
    });
    connect(ui->copy_links_nkr, &QPushButton::clicked, this, [=] {
        QStringList links;
        for (const auto &[_, profile]: NekoGui::profileManager->profiles) {
            if (profile->gid != ent->id) continue;
            links += profile->bean->ToNekorayShareLink(profile->type);
        }
        QApplication::clipboard()->setText(links.join("\n"));
        MessageBoxInfo(software_name, tr("Copied"));
    });

    ui->name->setFocus();

    ADJUST_SIZE
}

DialogEditGroup::~DialogEditGroup() {
    delete ui;
}

void DialogEditGroup::accept() {
    if (ent->id >= 0) { // already a group
        if (!ent->url.isEmpty() && ui->url->text().isEmpty()) {
            MessageBoxWarning(tr("Warning"), tr("Please input URL"));
            return;
        }
    }
    ent->name = ui->name->text();
    ent->url = ui->url->text();
    ent->archive = ui->archive->isChecked();
    ent->skip_auto_update = ui->skip_auto_update->isChecked();
    ent->manually_column_width = ui->manually_column_width->isChecked();
    ent->front_proxy_id = CACHE.front_proxy;
    ent->landing_proxy_id = LANDING.landing_proxy;
    QDialog::accept();
}

QStringList DialogEditGroup::load_proxy_items() {
    QStringList res = QStringList();
    auto profiles = NekoGui::profileManager->profiles;
    for (const auto &item: profiles) {
        res.push_back(item.second->bean->DisplayName());
    }

    return res;
}

int DialogEditGroup::get_proxy_id(QString name) {
    auto profiles = NekoGui::profileManager->profiles;
    for (const auto &item: profiles) {
        if (item.second->bean->DisplayName() == name) return item.first;
    }

    return -1;
}

QString DialogEditGroup::get_proxy_name(int id) {
    auto profiles = NekoGui::profileManager->profiles;
    return profiles.count(id) == 0 ? "None" : profiles[id]->bean->DisplayName();
}