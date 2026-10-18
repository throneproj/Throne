#pragma once

#include <QWidget>
#include <QDialog>
#include <QMenu>
#include <QTableWidgetItem>

#include <optional>

#include "ui_dialog_manage_groups.h"
#include "include/engine/services/ServiceTypes.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogManageGroups;
}
QT_END_NAMESPACE

class DialogManageGroups : public QDialog {
    Q_OBJECT

public:
    explicit DialogManageGroups(QWidget *parent = nullptr);

    ~DialogManageGroups() override;

    // GroupService::CheckDelete (checkOnly) or Delete.
    std::optional<Engine::ServiceError> deleteGroup(int groupId, bool checkOnly = false);

    // Drops the rows of groups that no longer exist and appends the groups added elsewhere.
    void refreshGroups();

private:
    Ui::DialogManageGroups *ui;

    void addGroupItem(int groupId);

private slots:

    void on_add_clicked();

    void on_update_all_clicked();
};
