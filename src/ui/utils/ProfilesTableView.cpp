#include "include/ui/utils/ProfilesTableView.h"
#include "include/ui/utils/ProfilesTableVerticalHeader.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>

ProfilesTableView::ProfilesTableView(QWidget *parent)
    : QTableView(parent) {
    setDragDropMode(InternalMove);
    setDropIndicatorShown(true);
    setSelectionBehavior(SelectRows);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDefaultDropAction(Qt::MoveAction);

    m_verticalHeader = new ProfilesTableVerticalHeader(this);
    setVerticalHeader(m_verticalHeader);
}

void ProfilesTableView::setModel(QAbstractItemModel *model) {
    QTableView::setModel(model);
    if (auto *pm = qobject_cast<ProfilesTableModel*>(model)) {
        m_verticalHeader->setProfilesModel(pm);
    } else {
        m_verticalHeader->setProfilesModel(nullptr);
    }
}