#include "include/ui/group/GroupItem.h"

#include "include/ui/group/dialog_edit_group.h"
#include "include/global/GuiUtils.hpp"
#include "include/configs/sub/GroupUpdater.hpp"

#include <QMessageBox>

#include "include/database/GroupsRepo.h"
#include "include/ui/mainwindow.h"
#include "include/ui/stats/dialog_endpoint_details.h"
#include "include/ui/widget/SubscriptionInfoCard.hpp"

namespace {
    QString ParseSubInfo(const Configs::Group &group) {
        const auto &sub = group.sub_info;
        QStringList parts;
        if (sub.valid && sub.has_quota) {
            QString remainStr = (sub.total > 0) ? ReadableSize(sub.remaining()) : QString::fromUtf8("\u221E");
            QString expireStr = (sub.expire > 0) ? DisplayTime(sub.expire, QLocale::ShortFormat) : QObject::tr("None");
            parts << QObject::tr("Used: %1 Remain: %2 Expire: %3").arg(ReadableSize(sub.used()), remainStr, expireStr);
        }
        if (sub.valid && sub.expire > 0) {
            parts << SubscriptionInfoCard::expiryText(sub.expire);
        }
        if (const auto plan = Subscription::ResolveAutoUpdate(group); plan.interval > 0) {
            parts << QObject::tr("Auto-update: every %1").arg(Stats::HumanizeDuration(plan.interval));
        }
        return parts.join(" | ");
    }
}

GroupItem::GroupItem(QWidget *parent, const std::shared_ptr<Configs::Group> &ent, QListWidgetItem *item) : QWidget(parent), ui(new Ui::GroupItem) {
    ui->setupUi(this);
    this->setLayoutDirection(Qt::LeftToRight);

    this->parentWindow = parent;
    this->ent = ent;
    this->item = item;
    if (ent == nullptr) return;

    ui->subinfo->setTextFormat(Qt::PlainText);

    connect(this, &GroupItem::edit_clicked, this, &GroupItem::on_edit_clicked);
    connect(Subscription::updater(), &Subscription::GroupUpdater::asyncUpdateCallback, this, [=,this](int gid) { if (gid == this->ent->id) refresh_data(); });

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
        auto subinfo = ParseSubInfo(*ent);
        if (!subinfo.isEmpty()) {
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
    Subscription::updater()->RefreshGroup(ent->id, nullptr, true);
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
