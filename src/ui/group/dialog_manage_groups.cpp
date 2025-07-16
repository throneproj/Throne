#include "include/ui/group/dialog_manage_groups.h"

#include "include/dataStore/Database.hpp"
#include "include/configs/sub/GroupUpdater.hpp"
#include "include/global/GuiUtils.hpp"
#include "include/ui/group/GroupItem.h"
#include "include/ui/group/dialog_edit_group.h"

#include <QInputDialog>
#include <QListWidgetItem>
#include <QMessageBox>

#define AddGroupToListIfExist(_id)                       \
    auto __ent = Configs::profileManager->GetGroup(_id); \
    if (__ent != nullptr) {                              \
        auto wI = new QListWidgetItem();                 \
        auto w = new GroupItem(this, __ent, wI);         \
        wI->setData(114514, _id);                        \
        ui->listWidget->addItem(wI);                     \
        ui->listWidget->setItemWidget(wI, w);            \
    }

DialogManageGroups::DialogManageGroups(QWidget *parent) : QDialog(parent), ui(new Ui::DialogManageGroups) {
    ui->setupUi(this);

    for (auto id: Configs::profileManager->groupsTabOrder) {
        AddGroupToListIfExist(id)
    }

    connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, [=](QListWidgetItem *wI) {
        auto w = dynamic_cast<GroupItem *>(ui->listWidget->itemWidget(wI));
        emit w->edit_clicked();
    });
}

DialogManageGroups::~DialogManageGroups() {
    delete ui;
}

void DialogManageGroups::on_add_clicked() {
    auto ent = Configs::ProfileManager::NewGroup();
    auto dialog = new DialogEditGroup(ent, this);
    int ret = dialog->exec();
    dialog->deleteLater();

    if (ret == QDialog::Accepted) {
        Configs::profileManager->AddGroup(ent);
        AddGroupToListIfExist(ent->id);
        MW_dialog_message(Dialog_DialogManageGroups, "refresh-1");
    }
}

void DialogManageGroups::on_update_all_clicked() {
    if (QMessageBox::question(this, tr("Confirmation"), tr("Update all subscriptions?")) == QMessageBox::StandardButton::Yes) {
        UI_update_all_groups();
    }
}
