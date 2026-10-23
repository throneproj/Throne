#pragma once

// Shared internals of src/ui/mainWindow/: everything must be inline or a macro (unity builds).

#include <QDialog>

#include "include/global/Configs.hpp"
#include "include/database/GroupsRepo.h"

// Read freely, but set only through MainWindow::setDialogInUse, which keeps the command palette in step.
inline bool dialog_is_using = false;

inline bool mw_sub_updating = false;

// Expands inside a MainWindow member function: uses `this` and `connect`.
#define USE_DIALOG(a)                                    \
    if (dialog_is_using) return;                         \
    setDialogInUse(true);                                \
    auto dialog = new a(this);                           \
    connect(dialog, &QDialog::finished, this, [=, this] { \
        dialog->deleteLater();                           \
        setDialogInUse(false);                           \
    });                                                  \
    dialog->show();

inline int tabIndex2GroupId(int index) {
    auto tabOrder = Configs::dataManager->groupsRepo->GetGroupsTabOrder();
    if (tabOrder.length() <= index) return -1;
    return tabOrder[index];
}

inline int groupId2TabIndex(int gid) {
    auto tabOrder = Configs::dataManager->groupsRepo->GetGroupsTabOrder();
    for (int key = 0; key < tabOrder.count(); key++) {
        if (tabOrder[key] == gid) return key;
    }
    return 0;
}
