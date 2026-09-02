#include "include/ui/utils/ConnectionCloseDelegate.h"
#include "include/ui/utils/ConnectionsTableModel.h"

#include <QMouseEvent>
#include <QPainter>

ConnectionCloseDelegate::ConnectionCloseDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void ConnectionCloseDelegate::setIcon(const QIcon &icon) {
    m_icon = icon;
}

void ConnectionCloseDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const {
    QStyledItemDelegate::paint(painter, option, index);
    if (m_icon.isNull()) return;

    if (option.state & QStyle::State_MouseOver) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        QColor tint = option.palette.color(QPalette::ButtonText);
        tint.setAlpha(38);
        painter->setBrush(tint);
        painter->drawRoundedRect(option.rect.adjusted(2, 2, -2, -2), 3, 3);
        painter->restore();
    }

    QRect iconRect(0, 0, IconSize, IconSize);
    iconRect.moveCenter(option.rect.center());
    m_icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal, QIcon::Off);
}

QSize ConnectionCloseDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const {
    return {ColumnWidth, IconSize};
}

bool ConnectionCloseDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                          const QStyleOptionViewItem &option, const QModelIndex &index) {
    const auto type = event->type();
    if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease
        && type != QEvent::MouseButtonDblClick) {
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

    auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() != Qt::LeftButton || !option.rect.contains(me->position().toPoint())) return false;

    // Swallowed so the cell never starts a selection drag; the click itself lands on release.
    if (type != QEvent::MouseButtonRelease) return true;

    const auto id = index.data(ConnectionsTableModel::ConnIdRole).toString();
    if (!id.isEmpty()) emit closeRequested(id);
    return true;
}
