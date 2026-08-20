#include "include/ui/group/GroupItem.h"

#include "include/ui/group/dialog_edit_group.h"
#include "include/global/GuiUtils.hpp"
#include "include/configs/sub/GroupUpdater.hpp"

#include <QMessageBox>

#include "include/database/GroupsRepo.h"
#include "include/ui/mainwindow.h"


QString ParseSubInfo(const QString &info) {
    if (info.trimmed().isEmpty()) return "";

    long long used = 0;
    long long total = 0;
    long long expire = 0;

    static const QRegularExpression re(
        R"((total|upload|download|expire)=([0-9]+))");

    auto it = re.globalMatch(info);

    bool hasTotal = false;

    while (it.hasNext()) {
        const auto match = it.next();
        const QStringView key = match.capturedView(1);
        const long long value = match.capturedView(2).toLongLong();

        if (key == u"total") {
            total = value;
            hasTotal = true;
        } else if (key == u"upload" || key == u"download") {
            used += value;
        } else if (key == u"expire") {
            expire = value;
        }
    }

    if (!hasTotal)
        return {};

    return QObject::tr("Used: %1 Remain: %2 Expire: %3")
        .arg(ReadableSize(used),
             total == 0 ? QString::fromUtf8("\u221E")
                        : ReadableSize(total - used),
             DisplayTime(expire, QLocale::ShortFormat));
}

GroupItem::GroupItem(QWidget *parent, const std::shared_ptr<Configs::Group> &ent, QListWidgetItem *item) : QWidget(parent), ui(new Ui::GroupItem) {
    ui->setupUi(this);
    this->setLayoutDirection(Qt::LeftToRight);

    this->parentWindow = parent;
    this->ent = ent;
    this->item = item;
    if (ent == nullptr) return;

    connect(this, &GroupItem::edit_clicked, this, &GroupItem::on_edit_clicked);
    connect(Subscription::groupUpdater, &Subscription::GroupUpdater::asyncUpdateCallback, this, [=,this](int gid) { if (gid == this->ent->id) refresh_data(); });

    refresh_data();
}

GroupItem::~GroupItem() {
    delete ui;
}

void GroupItem::refresh_data() {
    ui->name->setText(ent->name);

    auto type = ent->url.isEmpty() ? tr("Basic") : tr("Subscription");
    if (ent->archive) type = tr("Archive") + " " + type;
    type += " (" + Int2String(ent->Profiles().length()) + ")";
    ui->type->setText(type);

    if (ent->url.isEmpty()) {
        ui->url->hide();
        ui->subinfo->hide();
        ui->update_sub->hide();
    } else {
        ui->url->setText(ent->url);
        QStringList info;
        if (ent->sub_last_update != 0) {
            info << tr("Last update: %1").arg(DisplayTime(ent->sub_last_update, QLocale::ShortFormat));
        }
        auto subinfo = ParseSubInfo(ent->info);
        if (!ent->info.isEmpty()) {
            info << subinfo;
        }
        if (info.isEmpty()) {
            ui->subinfo->hide();
        } else {
            ui->subinfo->show();
            ui->subinfo->setText(info.join(" | "));
        }
    }
    runOnThread(
        [=,this] {
            adjustSize();
            item->setSizeHint(sizeHint());
            dynamic_cast<QWidget *>(parent())->adjustSize();
        },
        this);
}

void GroupItem::on_update_sub_clicked() {
    Subscription::groupUpdater->AsyncUpdate(ent->url, ent->id, nullptr, true);
}

void GroupItem::on_edit_clicked() {
    auto dialog = new DialogEditGroup(ent, parentWindow);
    connect(dialog, &QDialog::finished, this, [=,this] {
        if (dialog->result() == QDialog::Accepted) {
            Configs::dataManager->groupsRepo->Save(ent);
            refresh_data();
            MW_dialog_message(MwMessage::GroupsChanged, {});
        }
        dialog->deleteLater();
    });
    dialog->show();
}

void GroupItem::on_remove_clicked() {
    if (Configs::dataManager->groupsRepo->GetAllGroupIds().size() <= 1) return;
    if (QMessageBox::question(this, tr("Confirmation"), tr("Remove %1?").arg(ent->name)) ==
        QMessageBox::StandardButton::Yes) {
        GetMainWindow()->profile_stop(false, true, false);
        Configs::dataManager->groupsRepo->DeleteGroup(ent->id);
        MW_dialog_message(MwMessage::GroupsChanged, {});
        delete item;
    }
}
