#include "include/ui/group/dialog_edit_group.h"

#include "include/ui/mainwindow_interface.h"

#include <QClipboard>
#include <QStringListModel>
#include <QCompleter>

#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"


#define ADJUST_SIZE runOnThread([=,this] { adjustSize(); adjustPosition(mainwindow); }, this);

DialogEditGroup::DialogEditGroup(const std::shared_ptr<Configs::Group> &ent, QWidget *parent) : QDialog(parent), ui(new Ui::DialogEditGroup) {
    ui->setupUi(this);
    this->ent = ent;

    connect(ui->type, &QComboBox::currentIndexChanged, this, [=,this](int index) {
        ui->cat_sub->setHidden(index == 0);
        ADJUST_SIZE
    });

    ui->name->setText(ent->name);
    ui->skip_auto_update->setChecked(ent->skip_auto_update);
    ui->url->setText(ent->url);
    ui->type->setCurrentIndex(ent->url.isEmpty() ? 0 : 1);
    ui->type->currentIndexChanged(ui->type->currentIndex());
    ui->cat_share->setVisible(false);
    if (Configs::dataManager->profilesRepo->GetProfile(ent->front_proxy_id) == nullptr) {
        ent->front_proxy_id = -1;
        Configs::dataManager->groupsRepo->Save(ent);
    }
    if (Configs::dataManager->profilesRepo->GetProfile(ent->landing_proxy_id) == nullptr) {
        ent->landing_proxy_id = -1;
        Configs::dataManager->groupsRepo->Save(ent);
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
    connect(ui->front_proxy, &QComboBox::currentTextChanged, this, [=,this](const QString &txt){
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
    connect(ui->landing_proxy, &QComboBox::currentTextChanged, this, [=,this](const QString &txt){
        LANDING.landing_proxy = get_proxy_id(txt);
    });

    connect(ui->copy_links, &QPushButton::clicked, this, [=,this] {
        QStringList links;
        auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(ent->Profiles());
        for (const auto &profile: profiles) {
            links += profile->outbound->ExportToLink();
        }
        QApplication::clipboard()->setText(links.join("\n"));
        MessageBoxInfo(software_name, tr("Copied"));
    });
    connect(ui->copy_links_nkr, &QPushButton::clicked, this, [=,this] {
        QStringList links;
        auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(ent->Profiles());
        for (const auto &profile: profiles) {
            links += profile->outbound->ExportJsonLink();
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
    ent->skip_auto_update = ui->skip_auto_update->isChecked();
    ent->front_proxy_id = CACHE.front_proxy;
    ent->landing_proxy_id = LANDING.landing_proxy;
    QDialog::accept();
}

QStringList DialogEditGroup::load_proxy_items() {
    return Configs::dataManager->profilesRepo->GetAllProfileNames();
}

int DialogEditGroup::get_proxy_id(QString name) {
    if (auto profile = Configs::dataManager->profilesRepo->GetProfileByName(name)) return profile->id;

    return -1;
}

QString DialogEditGroup::get_proxy_name(int id) {
    if (auto profile = Configs::dataManager->profilesRepo->GetProfile(id)) {
        return profile->name;
    }
    return "None";
}