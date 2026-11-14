#include "include/ui/utils/ProfilesTableVerticalHeader.h"
#include "include/ui/utils/ProfilesFilterProxyModel.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include "include/ui/setting/ThemeManager.hpp"
#include <QPainter>
#include <QTableView>
#include <QFontMetrics>
#include <QIcon>

ProfilesTableVerticalHeader::ProfilesTableVerticalHeader(QWidget *parent)
    : QHeaderView(Qt::Vertical, parent) {
    setSectionsClickable(true);
}

void ProfilesTableVerticalHeader::setProfilesModel(ProfilesTableModel *model, ProfilesFilterProxyModel *proxy) {
    if (m_model == model && m_proxy == proxy) return;
    if (m_model) disconnect(m_model, nullptr, this, nullptr);
    if (m_proxy) disconnect(m_proxy, nullptr, this, nullptr);
    m_model = model;
    m_proxy = proxy;
    if (m_model) {
        connect(m_model, &ProfilesTableModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
            for (int r = topLeft.row(); r <= bottomRight.row(); ++r) {
                const int section = m_proxy ? m_proxy->toProxyRow(r) : r;
                if (section >= 0) updateSection(section);
            }
        });
        connect(m_model, &ProfilesTableModel::modelReset, this, [this]() {
            updateWidthFromRowCount();
            update();
        });
    }
    if (m_proxy) {
        connect(m_proxy, &QAbstractItemModel::rowsInserted, this, [this] { update(); });
        connect(m_proxy, &QAbstractItemModel::rowsRemoved, this, [this] { update(); });
    }
    updateWidthFromRowCount();
    update();
}

void ProfilesTableVerticalHeader::updateWidthFromRowCount() {
    const QFontMetrics fm(font());
    int rows = m_model ? m_model->rowCount() : 0;
    QString maxNum = (rows <= 0) ? QStringLiteral("1  ") : (QString::number(rows) + QStringLiteral("  "));
    int wNum = fm.horizontalAdvance(maxNum);
    int wCheck = fm.horizontalAdvance(QStringLiteral("✓"));
    int w = qMax(wNum, wCheck) + 8;
    if (m_pinnedVisible) w += fontMetrics().height() + 6;
    setMinimumWidth(w);
    setMaximumWidth(w);
}

bool ProfilesTableVerticalHeader::setPinnedVisible(bool visible) {
    if (m_pinnedVisible == visible) return false;
    m_pinnedVisible = visible;
    updateWidthFromRowCount();
    updateGeometry();
    update();
    return true;
}

void ProfilesTableVerticalHeader::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const {
    const int sourceRow = m_proxy ? m_proxy->toSourceRow(logicalIndex) : logicalIndex;
    const ThemeTokens &tokens = themeManager()->tokens;
    const bool marked = tokens.runningMark.isValid() && m_model && m_model->isStartedRow(sourceRow);
    painter->save();
    painter->fillRect(rect, marked ? tokens.runningMark : palette().color(QPalette::Button));
    const int pinWidth = m_pinnedVisible ? fontMetrics().height() + 6 : 0;
    if (m_pinnedVisible && m_model && m_model->isPinnedRow(sourceRow)) {
        static const QIcon keepIcon(":/icon/material/keep.png");
        const QRect iconRect(rect.left() + 3, rect.center().y() - fontMetrics().height() / 2, fontMetrics().height(), fontMetrics().height());
        keepIcon.paint(painter, iconRect);
    }
    if (rect.width() > 1) {
        painter->setPen(palette().color(QPalette::Mid));
        painter->drawLine(rect.topRight(), rect.bottomRight());
    }
    QString text;
    if (m_model) {
        text = m_model->rowLabel(sourceRow, logicalIndex);
    } else {
        text = QString::number(logicalIndex + 1) + QStringLiteral("  ");
    }
    painter->setPen(marked ? tokens.onRunningMark : palette().color(foregroundRole()));
    painter->drawText(rect.adjusted(pinWidth, 0, 0, 0), Qt::AlignCenter, text);
    const QAbstractItemModel *rows = m_proxy ? static_cast<QAbstractItemModel *>(m_proxy) : m_model;
    if (rows != nullptr && ProfilesTableModel::endsPinnedBlock(rows->index(logicalIndex, 0))) {
        painter->fillRect(QRect(rect.left(), rect.bottom() - 1, rect.width(), 2), tokens.muted);
    }
    painter->restore();
}
