#include "include/ui/group/dialog_manage_groups.h"

#include "include/configs/sub/GroupUpdater.hpp"
#include "include/ui/utils/GuiUtils.hpp"
#include "include/ui/group/GroupItem.h"
#include "include/ui/group/dialog_edit_group.h"

#include <QInputDialog>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QSet>

#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/services/GroupService.h"


DialogManageGroups::DialogManageGroups(QWidget *parent) : QDialog(parent), ui(new Ui::DialogManageGroups) {
    ui->setupUi(this);

    for (auto id: Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
        addGroupItem(id);
    }

    connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, [=,this](QListWidgetItem *wI) {
        auto w = dynamic_cast<GroupItem *>(ui->listWidget->itemWidget(wI));
        emit w->edit_clicked();
    });
    // Queued: never remove a row inside the GroupItem handler whose delete emitted it.
    connect(Engine::EngineEvents::instance(), &Engine::EngineEvents::groupsChanged, this, &DialogManageGroups::refreshGroups, Qt::QueuedConnection);
}

DialogManageGroups::~DialogManageGroups() {
    delete ui;
}

void DialogManageGroups::addGroupItem(int groupId) {
    auto ent = Configs::dataManager->groupsRepo->GetGroup(groupId);
    if (ent == nullptr) return;
    auto wI = new QListWidgetItem();
    auto w = new GroupItem(this, ent, wI);
    wI->setData(114514, groupId);
    ui->listWidget->addItem(wI);
    ui->listWidget->setItemWidget(wI, w);
}

void DialogManageGroups::refreshGroups() {
    QSet<int> listed;
    for (int row = ui->listWidget->count() - 1; row >= 0; --row) {
        auto wI = ui->listWidget->item(row);
        const int id = wI->data(114514).toInt();
        if (Configs::dataManager->groupsRepo->GetGroup(id) == nullptr) {
            delete wI;
            continue;
        }
        listed << id;
        if (auto w = dynamic_cast<GroupItem *>(ui->listWidget->itemWidget(wI))) w->refresh_data();
    }
    for (auto id: Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
        if (!listed.contains(id)) addGroupItem(id);
    }
}

std::optional<Engine::ServiceError> DialogManageGroups::deleteGroup(int groupId, bool checkOnly) {
    return checkOnly ? Engine::GroupService::CheckDelete(groupId) : Engine::GroupService::Delete(groupId);
}

void DialogManageGroups::on_add_clicked() {
    auto ent = Configs::dataManager->groupsRepo->NewGroup();
    auto dialog = new DialogEditGroup(ent, this);
    int ret = dialog->exec();
    dialog->deleteLater();

    if (ret == QDialog::Accepted) {
        Configs::dataManager->groupsRepo->AddGroup(ent);
        addGroupItem(ent->id);
        MW_dialog_message(MwMessage::GroupsChanged, {});
    }
}

void DialogManageGroups::on_update_all_clicked() {
    if (QMessageBox::question(this, tr("Confirmation"), tr("Update all subscriptions?")) == QMessageBox::StandardButton::Yes) {
        Subscription::updater()->RefreshAll();
    }
}
