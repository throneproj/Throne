#include "include/ui/utils/ProfilesTableView.h"
#include "include/ui/utils/ProfilesTableVerticalHeader.h"
#include "include/ui/utils/ProfilesTableFilterHeader.h"
#include "include/ui/utils/ProfilesFilterProxyModel.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include "include/ui/setting/ThemeManager.hpp"
#include <QDragMoveEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>

namespace {
    // QStyleSheetStyle drops Qt::BackgroundRole once a sheet's ::item rule draws, as the feiyang themes' does.
    class RowBackgroundDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            if (const QVariant fill = index.data(Qt::BackgroundRole); fill.canConvert<QBrush>()) {
                painter->fillRect(option.rect, qvariant_cast<QBrush>(fill));
            }
            QStyledItemDelegate::paint(painter, option, index);
            if (ProfilesTableModel::endsPinnedBlock(index)) {
                painter->fillRect(QRect(option.rect.left(), option.rect.bottom() - 1, option.rect.width(), 2), themeManager()->tokens.muted);
            }
        }
    };
}

ProfilesTableView::ProfilesTableView(QWidget *parent)
    : QTableView(parent) {
    setDragDropMode(InternalMove);
    setDropIndicatorShown(true);
    setSelectionBehavior(SelectRows);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDefaultDropAction(Qt::MoveAction);
    setItemDelegate(new RowBackgroundDelegate(this));

    m_verticalHeader = new ProfilesTableVerticalHeader(this);
    setVerticalHeader(m_verticalHeader);
    m_filterHeader = new ProfilesTableFilterHeader(this);
    setHorizontalHeader(m_filterHeader);
}

void ProfilesTableView::setModel(QAbstractItemModel *model) {
    QTableView::setModel(model);
    m_filterProxy = qobject_cast<ProfilesFilterProxyModel*>(model);
    auto *pm = m_filterProxy ? m_filterProxy->profilesModel()
                             : qobject_cast<ProfilesTableModel*>(model);
    m_verticalHeader->setProfilesModel(pm, m_filterProxy);
    if (pm) {
        connect(pm, &QAbstractItemModel::modelReset, this, &ProfilesTableView::updatePinnedIndicator);
        connect(pm, &ProfilesTableModel::pinnedCountChanged, this, &ProfilesTableView::updatePinnedIndicator);
    }
    updatePinnedIndicator();
}

void ProfilesTableView::updatePinnedIndicator() {
    auto *source = m_filterProxy ? m_filterProxy->profilesModel()
                                 : qobject_cast<ProfilesTableModel *>(model());
    if (m_verticalHeader->setPinnedVisible(source && source->pinnedCount() > 0)) {
        updateGeometries();
        horizontalHeader()->updateGeometry();
        viewport()->update();
    }
}

int ProfilesTableView::firstVisibleRow() {
    QRect rect = this->viewport()->rect();

    QModelIndex topIndex = indexAt(rect.topLeft());

    if (!topIndex.isValid()) return 0;

    int startRow = topIndex.row();
    return startRow;
}


void ProfilesTableView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && selectionModel() && selectionModel()->hasSelection()) {
        clearSelection();
        selectionModel()->clearCurrentIndex();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Up && m_filterHeader->filtersVisible()
        && currentIndex().isValid() && currentIndex().row() == 0) {
        m_filterHeader->focusLastFilterField();
        event->accept();
        return;
    }
    QTableView::keyPressEvent(event);
}

void ProfilesTableView::startDrag(Qt::DropActions supportedActions) {
    if (m_filterProxy && m_filterProxy->hasActiveFilter()) return;
    QTableView::startDrag(supportedActions);
}

void ProfilesTableView::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat("application/profile-id")) {
        if (m_filterProxy && m_filterProxy->hasActiveFilter()) { event->ignore(); return; }

        event->accept();
        QTableView::dragEnterEvent(event);
    } else {
        event->ignore();
    }
}

void ProfilesTableView::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasFormat("application/profile-id")) {
        if (m_filterProxy && m_filterProxy->hasActiveFilter()) { event->ignore(); return; }
        QPoint pos = event->position().toPoint();
        QModelIndex targetIndex = indexAt(pos);
        if (!targetIndex.isValid()) {
            QModelIndex lastRowIndex = model()->index(model()->rowCount() - 1, 0);
            QRect rect = visualRect(lastRowIndex);
            if (event->pos().y() > rect.bottom()) {
                QPoint fakePos(rect.center().x(), rect.bottom() - 5);

                QDragMoveEvent fakeEvent(
                    fakePos,
                    event->possibleActions(),
                    event->mimeData(),
                    event->mouseButtons(),
                    event->keyboardModifiers()
                );
                QTableView::dragMoveEvent(&fakeEvent);
                event->accept();
                return;
            }
        }
        event->accept();
        QTableView::dragMoveEvent(event);
    } else {
        event->ignore();
    }
}

void ProfilesTableView::dropEvent(QDropEvent *event) {
    if (event->source() == this && event->mimeData()->hasFormat("application/profile-id")) {
        if (m_filterProxy && m_filterProxy->hasActiveFilter()) { event->ignore(); return; }
        QByteArray encodedData = event->mimeData()->data("application/profile-id");
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        int profileId = -1;
        stream >> profileId;
        // Sorting can move the dragged profile while the drag is in progress.
        auto *source = m_filterProxy ? m_filterProxy->profilesModel() : qobject_cast<ProfilesTableModel *>(model());
        const int rowNum = source ? source->indexOfProfile(profileId) : -1;

        QPoint pos = event->position().toPoint();
        QModelIndex targetIndex = indexAt(pos);

        int target = model()->rowCount();
        bool below = true;
        if (targetIndex.isValid()) {
            target = m_filterProxy ? m_filterProxy->toSourceRow(targetIndex.row()) : targetIndex.row();
            below = dropIndicatorPosition() != AboveItem;
        }
        if (rowNum < 0 || rowNum >= model()->rowCount() || target < 0) {
            event->ignore();
            return;
        }
        if (rowDropped) rowDropped(rowNum, target, below);
        event->accept();
    } else {
        QTableView::dropEvent(event);
    }
}
