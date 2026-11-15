#include "include/ui/utils/ProfilesTableView.h"
#include "include/ui/utils/ProfilesTableVerticalHeader.h"
#include "include/ui/utils/ProfilesTableFilterHeader.h"
#include "include/ui/utils/ProfilesFilterProxyModel.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include "include/ui/utils/ServiceIcons.h"
#include "include/ui/setting/ThemeManager.hpp"
#include <QApplication>
#include <QDragMoveEvent>
#include <QHeaderView>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QToolTip>

namespace {
    constexpr int kProfilesTableServiceIconSize = 16;
    constexpr int kProfilesTableServiceGap = 6;

    QList<ServiceIcons::Item> profilesTableServiceItems(const QModelIndex &index) {
        if (index.column() != ProfilesTableModel::ColTestResult) return {};
        return index.data(ProfilesTableModel::ServiceItemsRole).value<QList<ServiceIcons::Item>>();
    }

    int profilesTableServiceReserve(qsizetype count) {
        return ServiceIcons::StripWidth(count, kProfilesTableServiceIconSize) + kProfilesTableServiceGap;
    }

    // Inside the reserve at the cell's trailing edge, with half the gap on either side.
    QRect profilesTableServiceArea(const QStyleOptionViewItem &option, qsizetype count) {
        const int width = ServiceIcons::StripWidth(count, kProfilesTableServiceIconSize);
        const QRect area(option.rect.right() + 1 - kProfilesTableServiceGap / 2 - width, option.rect.top(), width, option.rect.height());
        return QStyle::visualRect(option.direction, option.rect, area);
    }

    // QStyleSheetStyle drops Qt::BackgroundRole once a sheet's ::item rule draws, as the feiyang themes' does.
    class RowBackgroundDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            if (const QVariant fill = index.data(Qt::BackgroundRole); fill.canConvert<QBrush>()) {
                painter->fillRect(option.rect, qvariant_cast<QBrush>(fill));
            }
            if (const auto items = profilesTableServiceItems(index); items.isEmpty()) {
                QStyledItemDelegate::paint(painter, option, index);
            } else {
                paintWithServices(painter, option, index, items);
            }
            if (ProfilesTableModel::endsPinnedBlock(index)) {
                painter->fillRect(QRect(option.rect.left(), option.rect.bottom() - 1, option.rect.width(), 2), themeManager()->tokens.muted);
            }
        }

        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            QSize size = QStyledItemDelegate::sizeHint(option, index);
            if (const auto items = profilesTableServiceItems(index); !items.isEmpty()) size.rwidth() += profilesTableServiceReserve(items.size());
            return size;
        }

        bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) override {
            if (event != nullptr && view != nullptr && event->type() == QEvent::ToolTip) {
                const auto items = profilesTableServiceItems(index);
                const auto icons = ServiceIcons::StripRects(profilesTableServiceArea(option, items.size()), items.size(),
                                                            kProfilesTableServiceIconSize);
                for (qsizetype i = 0; i < items.size(); ++i) {
                    if (!icons[i].contains(event->pos())) continue;
                    // The viewport gets the mouse moves that hide the tip once the pointer leaves the icon.
                    QToolTip::showText(event->globalPos(), items[i].toolTip, view->viewport(), icons[i]);
                    return true;
                }
            }
            return QStyledItemDelegate::helpEvent(event, view, option, index);
        }

    private:
        // The selection and hover panel still spans the whole cell: only the text gives way to the icons.
        void paintWithServices(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index,
                               const QList<ServiceIcons::Item> &items) const {
            QStyleOptionViewItem cell(option);
            initStyleOption(&cell, index);
            const QWidget *widget = option.widget;
            QStyle *style = widget != nullptr ? widget->style() : QApplication::style();
            QStyleOptionViewItem narrowed(cell);
            narrowed.rect.setWidth(cell.rect.width() - profilesTableServiceReserve(items.size()));
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
            const int textWidth = style->subElementRect(QStyle::SE_ItemViewItemText, &narrowed, widget).width() - 2 * textMargin;
            cell.text = cell.fontMetrics.elidedText(cell.text, cell.textElideMode, qMax(0, textWidth));
            style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, widget);

            const auto group = !(option.state & QStyle::State_Enabled) ? QPalette::Disabled
                               : option.state & QStyle::State_Active   ? QPalette::Active
                                                                       : QPalette::Inactive;
            const auto role = option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text;
            painter->save();
            painter->setClipRect(option.rect, Qt::IntersectClip);
            ServiceIcons::PaintStrip(painter, profilesTableServiceArea(option, items.size()), items, kProfilesTableServiceIconSize,
                                     option.palette.color(group, role));
            painter->restore();
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

void ProfilesTableView::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat("application/profile-row-number")) {

        event->accept();
        QTableView::dragEnterEvent(event);
    } else {
        event->ignore();
    }
}

void ProfilesTableView::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasFormat("application/profile-row-number")) {
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
    if (event->source() == this && event->mimeData()->hasFormat("application/profile-row-number")) {
        QByteArray encodedData = event->mimeData()->data("application/profile-row-number");
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        // The proxy maps drag indices to the source, so this is a source row.
        int rowNum;
        stream >> rowNum;

        QPoint pos = event->position().toPoint();
        QModelIndex targetIndex = indexAt(pos);

        // Rows are not drop targets, so the indicator is above or below: the upper or lower half of the row.
        int target = model()->rowCount() - 1;
        bool below = true;
        if (targetIndex.isValid()) {
            target = targetIndex.row();
            below = dropIndicatorPosition() != AboveItem;
        }
        if (m_filterProxy) target = m_filterProxy->toSourceRow(target);
        if (target < 0) return;
        if (rowDropped) rowDropped(rowNum, target, below);
        event->accept();
    } else {
        QTableView::dropEvent(event);
    }
}