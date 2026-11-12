#include "include/ui/utils/ProfilesTableVerticalHeader.h"
#include "include/ui/utils/ProfilesFilterProxyModel.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include "include/ui/setting/ThemeManager.hpp"
#include <QPainter>
#include <QTableView>
#include <QFontMetrics>

namespace {
    int pinSide(const QFontMetrics &fm) {
        return qMax(8, fm.height() * 3 / 4);
    }

    // A push pin on a 12-unit grid.
    void paintPin(QPainter *painter, const QRectF &box, const QColor &color) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->translate(box.topLeft());
        painter->scale(box.width() / 12.0, box.height() / 12.0);
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawRoundedRect(QRectF(3.4, 0.6, 5.2, 1.7), 0.6, 0.6);
        painter->drawPolygon(QPolygonF{{4.4, 2.3}, {7.6, 2.3}, {8.4, 6.4}, {3.6, 6.4}});
        painter->drawRoundedRect(QRectF(2.0, 6.0, 8.0, 1.6), 0.6, 0.6);
        painter->setPen(QPen(color, 1.3, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(QPointF(6.0, 7.6), QPointF(6.0, 11.3));
        painter->restore();
    }
}

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
        connect(m_model, &ProfilesTableModel::pinnedCountChanged, this, [this]() {
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
    int w = qMax(wNum, wCheck) + 8 + pinRoom();
    setMinimumWidth(w);
    setMaximumWidth(w);
}

int ProfilesTableVerticalHeader::pinRoom() const {
    return m_model && m_model->pinnedCount() > 0 ? pinSide(QFontMetrics(font())) + 4 : 0;
}

void ProfilesTableVerticalHeader::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const {
    const int sourceRow = m_proxy ? m_proxy->toSourceRow(logicalIndex) : logicalIndex;
    const ThemeTokens &tokens = themeManager()->tokens;
    const bool marked = tokens.runningMark.isValid() && m_model && m_model->isStartedRow(sourceRow);
    painter->save();
    painter->fillRect(rect, marked ? tokens.runningMark : palette().color(QPalette::Button));
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
    // Every row's label shifts right while the group has pins, so the numbers stay in one column.
    const int room = pinRoom();
    if (m_model && m_model->isPinnedRow(sourceRow)) {
        const int side = pinSide(QFontMetrics(font()));
        paintPin(painter, QRectF(rect.left() + 3, rect.top() + (rect.height() - side) / 2.0, side, side),
                 marked ? tokens.onRunningMark : tokens.accent);
    }
    painter->setPen(marked ? tokens.onRunningMark : palette().color(foregroundRole()));
    painter->drawText(rect.adjusted(room, 0, 0, 0), Qt::AlignCenter, text);
    const QAbstractItemModel *rows = m_proxy ? static_cast<QAbstractItemModel *>(m_proxy) : m_model;
    if (rows != nullptr && ProfilesTableModel::endsPinnedBlock(rows->index(logicalIndex, 0))) {
        painter->fillRect(QRect(rect.left(), rect.bottom() - 1, rect.width(), 2), tokens.muted);
    }
    painter->restore();
}
