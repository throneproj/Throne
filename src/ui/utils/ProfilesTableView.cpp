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
    constexpr char16_t kUploadArrow = u'↑';

    QList<ServiceIcons::Item> profilesTableServiceItems(const QModelIndex &index) {
        if (index.column() != ProfilesTableModel::ColExitIP) return {};
        return index.data(ProfilesTableModel::ServiceItemsRole).value<QList<ServiceIcons::Item>>();
    }

    QStyle *profilesTableCellStyle(const QStyleOptionViewItem &option) {
        return option.widget != nullptr ? option.widget->style() : QApplication::style();
    }

    QRect profilesTableServiceArea(const QStyleOptionViewItem &cell) {
        return profilesTableCellStyle(cell)->subElementRect(QStyle::SE_ItemViewItemDecoration, &cell, cell.widget);
    }

    // QStyleSheetStyle drops Qt::BackgroundRole once a sheet's ::item rule draws, as the feiyang themes' does.
    class RowBackgroundDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            if (const QVariant fill = index.data(Qt::BackgroundRole); fill.canConvert<QBrush>()) {
                painter->fillRect(option.rect, qvariant_cast<QBrush>(fill));
            }
            if (const auto items = profilesTableServiceItems(index); !items.isEmpty()) {
                paintWithServices(painter, option, index, items);
            } else if (!paintSpeed(painter, option, index)) {
                QStyledItemDelegate::paint(painter, option, index);
            }
            if (ProfilesTableModel::endsPinnedBlock(index)) {
                painter->fillRect(QRect(option.rect.left(), option.rect.bottom() - 1, option.rect.width(), 2), themeManager()->tokens.muted);
            }
        }

        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            const auto items = profilesTableServiceItems(index);
            if (items.isEmpty()) return QStyledItemDelegate::sizeHint(option, index);
            const QStyleOptionViewItem cell = serviceCell(option, index, items.size());
            return profilesTableCellStyle(option)->sizeFromContents(QStyle::CT_ItemViewItem, &cell, QSize(), option.widget);
        }

        bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) override {
            if (event != nullptr && view != nullptr && event->type() == QEvent::ToolTip) {
                const auto items = profilesTableServiceItems(index);
                const auto icons = items.isEmpty() ? QList<QRect>()
                                                   : ServiceIcons::StripRects(profilesTableServiceArea(serviceCell(option, index, items.size())),
                                                                              items.size(), kProfilesTableServiceIconSize);
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
        // The strip rides as an empty trailing decoration, so the style lays out, aligns and elides the text in what is left.
        QStyleOptionViewItem serviceCell(const QStyleOptionViewItem &option, const QModelIndex &index, qsizetype count) const {
            QStyleOptionViewItem cell(option);
            initStyleOption(&cell, index);
            cell.features |= QStyleOptionViewItem::HasDecoration;
            cell.icon = QIcon();
            cell.decorationPosition = QStyleOptionViewItem::Right;
            cell.decorationAlignment = Qt::AlignCenter;
            cell.decorationSize = QSize(ServiceIcons::StripWidth(count, kProfilesTableServiceIconSize), kProfilesTableServiceIconSize);
            return cell;
        }

        // The selection and hover panel still spans the whole cell: only the text gives way to the icons.
        void paintWithServices(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index,
                               const QList<ServiceIcons::Item> &items) const {
            const QStyleOptionViewItem cell = serviceCell(option, index, items.size());
            profilesTableCellStyle(option)->drawControl(QStyle::CE_ItemViewItem, &cell, painter, option.widget);

            const auto group = !(option.state & QStyle::State_Enabled) ? QPalette::Disabled
                               : option.state & QStyle::State_Active   ? QPalette::Active
                                                                       : QPalette::Inactive;
            const auto role = option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text;
            painter->save();
            painter->setClipRect(option.rect, Qt::IntersectClip);
            ServiceIcons::PaintStrip(painter, profilesTableServiceArea(cell), items, kProfilesTableServiceIconSize,
                                     option.palette.color(group, role));
            painter->restore();
        }

        // Download and upload wear the speed test panel's colours; a selected row keeps the style's text colour, as Latency does.
        bool paintSpeed(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
            if (index.column() != ProfilesTableModel::ColSpeed || option.state & QStyle::State_Selected) return false;
            QStyleOptionViewItem cell(option);
            initStyleOption(&cell, index);
            const QString text = cell.text;
            if (text.isEmpty()) return false;
            const QWidget *widget = option.widget;
            QStyle *style = profilesTableCellStyle(option);
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
            const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &cell, widget).adjusted(textMargin, 0, -textMargin, 0);
            const QFontMetrics metrics = cell.fontMetrics;
            const QString shown = metrics.elidedText(text, cell.textElideMode, qMax(0, textRect.width()));
            cell.text.clear();
            style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, widget);

            const ThemeTokens &tokens = themeManager()->tokens;
            const QVariant fill = index.data(Qt::BackgroundRole);
            const QColor backdrop = fill.canConvert<QBrush>() ? qvariant_cast<QBrush>(fill).color() : QColor();
            const auto runColor = [&](const QString &run, const QColor &color) {
                const QColor base = run.mid(1).trimmed() == QLatin1String("N/A") ? tokens.muted : color;
                return backdrop.isValid() && backdrop.alpha() == 255 ? ContrastSeparated(base, backdrop, 4.0) : base;
            };
            const qsizetype upAt = text.indexOf(kUploadArrow);
            const QColor downColor = runColor(upAt < 0 ? text : text.left(upAt), tokens.info);
            const QColor upColor = runColor(upAt < 0 ? QString() : text.mid(upAt), tokens.success);
            qsizetype split = shown.indexOf(kUploadArrow);
            if (split < 0) split = upAt == 0 ? 0 : shown.size();

            const QRect area = QStyle::alignedRect(cell.direction, cell.displayAlignment,
                                                   QSize(metrics.horizontalAdvance(shown), metrics.height()), textRect);
            painter->save();
            painter->setClipRect(option.rect, Qt::IntersectClip);
            painter->setFont(cell.font);
            painter->setLayoutDirection(Qt::LeftToRight);
            int x = area.left();
            for (const auto &[run, color] : {std::pair{shown.left(split), downColor}, std::pair{shown.mid(split), upColor}}) {
                painter->setPen(color);
                painter->drawText(QPoint(x, area.top() + metrics.ascent()), run);
                x += metrics.horizontalAdvance(run);
            }
            painter->restore();
            return true;
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
