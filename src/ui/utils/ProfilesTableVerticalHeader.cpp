#include "include/ui/utils/ProfilesTableVerticalHeader.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include <QPainter>
#include <QTableView>
#include <QFontMetrics>

ProfilesTableVerticalHeader::ProfilesTableVerticalHeader(QWidget *parent)
    : QHeaderView(Qt::Vertical, parent) {}

void ProfilesTableVerticalHeader::setProfilesModel(ProfilesTableModel *model) {
    if (m_model == model) return;
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }
    m_model = model;
    if (m_model) {
        connect(m_model, &ProfilesTableModel::dataChanged, this, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
            for (int r = topLeft.row(); r <= bottomRight.row(); ++r) {
                updateSection(r);
            }
        });
        connect(m_model, &ProfilesTableModel::modelReset, this, [this]() {
            updateWidthFromRowCount();
            update();
        });
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
    setMinimumWidth(w);
    setMaximumWidth(w);
}

void ProfilesTableVerticalHeader::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const {
    painter->save();
    painter->fillRect(rect, palette().color(QPalette::Button));
    if (rect.width() > 1) {
        painter->setPen(palette().color(QPalette::Mid));
        painter->drawLine(rect.topRight(), rect.bottomRight());
    }
    QString text;
    if (m_model) {
        text = m_model->rowLabel(logicalIndex);
    } else {
        text = QString::number(logicalIndex + 1) + QStringLiteral("  ");
    }
    painter->setPen(palette().color(foregroundRole()));
    painter->drawText(rect, Qt::AlignCenter, text);
    painter->restore();
}
