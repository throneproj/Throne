#pragma once

#include <QApplication>
#include <QFontMetricsF>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QtMath>

// Qt 6.12.1's windows11 style moves a table cell's text and decoration right by a margin its size hint leaves out, so a
// column sized to its contents elides them and wrapped text needs a line more than its row got. Style sheet ::item
// padding narrows wrapped text the same way.
namespace ItemViewFit {
    inline QStyle *styleOf(const QStyleOptionViewItem &cell) {
        return cell.widget != nullptr ? cell.widget->style() : QApplication::style();
    }

    // The width the style takes back from a cell of this size.
    inline int ClippedWidth(const QStyleOptionViewItem &cell, const QSize &size) {
        QStyleOptionViewItem probe(cell);
        probe.rect = QRect(QPoint(0, 0), size);
        const QWidget *widget = cell.widget;
        QStyle *style = styleOf(cell);
        int clipped = 0;
        if (probe.features & QStyleOptionViewItem::HasDisplay && !probe.text.isEmpty()) {
            const QFontMetricsF metrics(probe.fontMetrics);
            qreal widest = 0;
            for (const auto &line : probe.text.split(QChar::LineSeparator)) widest = qMax(widest, metrics.horizontalAdvance(line));
            const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
            clipped = qCeil(widest) + 2 * textMargin - style->subElementRect(QStyle::SE_ItemViewItemText, &probe, widget).width();
        }
        if (probe.features & QStyleOptionViewItem::HasDecoration) {
            clipped = qMax(clipped, probe.decorationSize.width() - style->subElementRect(QStyle::SE_ItemViewItemDecoration, &probe, widget).width());
        }
        return qMax(0, clipped);
    }

    // For a cell initStyleOption already filled in.
    inline QSize SizeHint(const QStyleOptionViewItem &cell) {
        QStyle *style = styleOf(cell);
        const bool wraps = cell.features & QStyleOptionViewItem::WrapText && cell.rect.width() > 0;
        QStyleOptionViewItem measured(cell);
        // Only plain text spans the whole cell, so only there is the shortfall known.
        if (wraps && !(cell.features & (QStyleOptionViewItem::HasDecoration | QStyleOptionViewItem::HasCheckIndicator))) {
            const int taken = cell.rect.width() - style->subElementRect(QStyle::SE_ItemViewItemText, &cell, cell.widget).width();
            if (taken > 0) measured.rect.setWidth(cell.rect.width() - taken);
        }
        QSize size = style->sizeFromContents(QStyle::CT_ItemViewItem, &measured, QSize(), cell.widget);
        // Wrapped text is as wide as its rect allows, so only text kept on one line can be clipped.
        QStyleOptionViewItem flat(cell);
        flat.features &= ~QStyleOptionViewItem::WrapText;
        if (!wraps || style->sizeFromContents(QStyle::CT_ItemViewItem, &flat, QSize(), cell.widget).width() <= size.width()) {
            size.rwidth() += ClippedWidth(cell, size);
        }
        return size;
    }

    class Delegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            if (const QVariant hint = index.data(Qt::SizeHintRole); hint.isValid()) return hint.toSize();
            QStyleOptionViewItem cell(option);
            initStyleOption(&cell, index);
            return SizeHint(cell);
        }
    };
}
