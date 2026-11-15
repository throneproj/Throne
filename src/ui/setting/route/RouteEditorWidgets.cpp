#include "include/ui/setting/route/RouteEditorWidgets.h"

#include "include/ui/setting/ThemeManager.hpp"

#include <QFontMetrics>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QTextDocument>
#include <QToolTip>

namespace RouteEditor {
    namespace {
        constexpr int kChipPadding = 8;
        constexpr int kChipGap = 6;
        constexpr int kRemoveSize = 16;
        constexpr int kRemoveMargin = 4;

        // The width a chip needs around a value of textWidth.
        int chipWidth(const QFontMetrics& metrics, const QString& kind, bool removable, int textWidth) {
            int width = kChipPadding + textWidth;
            if (!kind.isEmpty()) width += metrics.horizontalAdvance(kind) + kChipGap;
            width += removable ? kChipGap / 2 + kRemoveSize + kRemoveMargin : kChipPadding;
            return width;
        }
    } // namespace

    FlowLayout::FlowLayout(QWidget* parent, int hSpacing, int vSpacing) : QLayout(parent), hSpace(hSpacing), vSpace(vSpacing) {
        setContentsMargins(0, 0, 0, 0);
    }

    FlowLayout::~FlowLayout() {
        while (QLayoutItem* item = takeAt(0)) delete item;
    }

    void FlowLayout::addItem(QLayoutItem* item) {
        items.append(item);
    }

    int FlowLayout::count() const {
        return static_cast<int>(items.size());
    }

    QLayoutItem* FlowLayout::itemAt(int index) const {
        return index >= 0 && index < items.size() ? items.at(index) : nullptr;
    }

    QLayoutItem* FlowLayout::takeAt(int index) {
        return index >= 0 && index < items.size() ? items.takeAt(index) : nullptr;
    }

    Qt::Orientations FlowLayout::expandingDirections() const {
        return {};
    }

    bool FlowLayout::hasHeightForWidth() const {
        return true;
    }

    int FlowLayout::heightForWidth(int width) const {
        return doLayout(QRect(0, 0, width, 0), true);
    }

    QSize FlowLayout::minimumSize() const {
        QSize size;
        for (const QLayoutItem* item : items) {
            if (!item->isEmpty()) size = size.expandedTo(item->minimumSize());
        }
        const QMargins margins = contentsMargins();
        return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    }

    QSize FlowLayout::sizeHint() const {
        return minimumSize();
    }

    void FlowLayout::setGeometry(const QRect& rect) {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

    void FlowLayout::clear() {
        while (QLayoutItem* item = takeAt(0)) {
            // Later, as a chip may be clearing its own row from one of its signals.
            if (QWidget* widget = item->widget()) {
                widget->hide();
                widget->deleteLater();
            }
            delete item;
        }
        invalidate();
    }

    int FlowLayout::doLayout(const QRect& rect, bool testOnly) const {
        const QMargins margins = contentsMargins();
        const QRect area = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
        int y = area.y();
        int lineWidth = 0;
        int lineHeight = 0;
        QList<QPair<QLayoutItem*, QSize>> line;
        // Items of a line are centred on it, so a chip lines up with a taller button beside it.
        const auto flush = [&] {
            int x = area.x();
            if (!testOnly) {
                for (const auto& [item, size] : line) {
                    item->setGeometry(QRect(QPoint(x, y + (lineHeight - size.height()) / 2), size));
                    x += size.width() + hSpace;
                }
            }
            y += lineHeight + vSpace;
            line.clear();
            lineWidth = 0;
            lineHeight = 0;
        };
        for (QLayoutItem* item : items) {
            if (item->isEmpty()) continue;
            const QSize size = item->sizeHint().boundedTo(QSize(qMax(1, area.width()), QWIDGETSIZE_MAX)).expandedTo(item->minimumSize());
            if (!line.isEmpty() && lineWidth + hSpace + size.width() > area.width()) flush();
            lineWidth = line.isEmpty() ? size.width() : lineWidth + hSpace + size.width();
            lineHeight = qMax(lineHeight, size.height());
            line.append({item, size});
        }
        if (!line.isEmpty()) flush();
        const int used = y > area.y() ? y - area.y() - vSpace : 0;
        return used + margins.top() + margins.bottom();
    }

    ValueChip::ValueChip(const QString& text, QWidget* parent) : QWidget(parent), value(text) {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        setAccessibleName(text);
    }

    void ValueChip::setKind(const QString& kind) {
        kindText = kind;
        updateGeometry();
        update();
    }

    void ValueChip::setRemovable(bool on) {
        removable = on;
        hoverRemove = false;
        pressedRemove = false;
        updateGeometry();
        update();
    }

    void ValueChip::setIssue(Validation::Severity severity, const QString& message) {
        issueSeverity = severity;
        issueText = message;
        update();
    }

    QSize ValueChip::sizeHint() const {
        const QFontMetrics metrics(font());
        return {chipWidth(metrics, kindText, removable, metrics.horizontalAdvance(shownValue())),
                qMax(metrics.height() + 8, kRemoveSize + 6)};
    }

    QSize ValueChip::minimumSizeHint() const {
        const QFontMetrics metrics(font());
        const int text = qMin(metrics.horizontalAdvance(shownValue()), metrics.averageCharWidth() * 8);
        return {chipWidth(metrics, kindText, removable, text), sizeHint().height()};
    }

    QRect ValueChip::removeRect() const {
        if (!removable) return {};
        return {width() - kRemoveMargin - kRemoveSize, (height() - kRemoveSize) / 2, kRemoveSize, kRemoveSize};
    }

    QString ValueChip::shownValue() const {
        return value.simplified();
    }

    bool ValueChip::event(QEvent* event) {
        if (event->type() == QEvent::ToolTip) {
            const auto* help = static_cast<QHelpEvent*>(event);
            QString tip;
            if (removable && removeRect().contains(help->pos())) {
                tip = tr("Remove");
            } else {
                const QFontMetrics metrics(font());
                const bool elided = chipWidth(metrics, kindText, removable, metrics.horizontalAdvance(shownValue())) > width();
                tip = elided ? value : QString();
                if (!issueText.isEmpty()) tip = tip.isEmpty() ? issueText : tip + QLatin1Char('\n') + issueText;
            }
            if (tip.isEmpty()) {
                QToolTip::hideText();
                event->ignore();
            } else {
                QToolTip::showText(help->globalPos(), Qt::convertFromPlainText(tip), this);
            }
            return true;
        }
        return QWidget::event(event);
    }

    void ValueChip::paintEvent(QPaintEvent*) {
        const auto& tokens = themeManager()->tokens;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QColor fill = Mix(tokens.onSurface, tokens.surface, 0.08);
        QColor border = Mix(tokens.onSurface, tokens.surface, 0.2);
        QColor text = tokens.onSurface;
        if (!issueText.isEmpty()) {
            const QColor tint = issueSeverity == Validation::Severity::Error ? tokens.danger : tokens.warning;
            fill = Mix(tint, tokens.surface, 0.14);
            border = Mix(tint, tokens.surface, 0.6);
            if (issueSeverity == Validation::Severity::Error) text = tokens.danger;
        }
        if (hasFocus()) border = tokens.accent;
        painter.setPen(QPen(border, 1));
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);

        const QFontMetrics metrics(font());
        int x = kChipPadding;
        if (!kindText.isEmpty()) {
            painter.setPen(tokens.muted);
            const int kindWidth = metrics.horizontalAdvance(kindText);
            painter.drawText(QRect(x, 0, kindWidth, height()), Qt::AlignVCenter | Qt::AlignLeft, kindText);
            x += kindWidth + kChipGap;
        }
        const int right = removable ? removeRect().left() - kChipGap / 2 : width() - kChipPadding;
        const QRect textRect(x, 0, qMax(0, right - x), height());
        painter.setPen(text);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, metrics.elidedText(shownValue(), Qt::ElideMiddle, textRect.width()));

        if (!removable) return;
        const QRect button = removeRect();
        if (hoverRemove) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Mix(tokens.onSurface, fill, pressedRemove ? 0.22 : 0.12));
            painter.drawEllipse(button);
        }
        painter.setPen(QPen(hoverRemove ? tokens.onSurface : tokens.muted, 1.4, Qt::SolidLine, Qt::RoundCap));
        const QRectF cross = QRectF(button).adjusted(5, 5, -5, -5);
        painter.drawLine(cross.topLeft(), cross.bottomRight());
        painter.drawLine(cross.topRight(), cross.bottomLeft());
    }

    void ValueChip::mouseMoveEvent(QMouseEvent* event) {
        const bool over = removable && removeRect().contains(event->position().toPoint());
        if (over != hoverRemove) {
            hoverRemove = over;
            if (over) setCursor(Qt::PointingHandCursor);
            else unsetCursor();
            update();
        }
        QWidget::mouseMoveEvent(event);
    }

    void ValueChip::mousePressEvent(QMouseEvent* event) {
        if (event->button() == Qt::LeftButton && removable && removeRect().contains(event->position().toPoint())) {
            pressedRemove = true;
            update();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void ValueChip::mouseReleaseEvent(QMouseEvent* event) {
        if (event->button() == Qt::LeftButton && pressedRemove) {
            pressedRemove = false;
            update();
            if (removable && removeRect().contains(event->position().toPoint())) emit removeRequested();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void ValueChip::mouseDoubleClickEvent(QMouseEvent* event) {
        if (event->button() == Qt::LeftButton && !(removable && removeRect().contains(event->position().toPoint()))) {
            emit editRequested();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void ValueChip::keyPressEvent(QKeyEvent* event) {
        switch (event->key()) {
            case Qt::Key_Delete:
            case Qt::Key_Backspace:
                if (removable) {
                    emit removeRequested();
                    return;
                }
                break;
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_F2:
                emit editRequested();
                return;
            default: break;
        }
        QWidget::keyPressEvent(event);
    }

    void ValueChip::leaveEvent(QEvent* event) {
        if (hoverRemove) {
            hoverRemove = false;
            unsetCursor();
            update();
        }
        QWidget::leaveEvent(event);
    }

    ThenPill::ThenPill(QWidget* parent) : QWidget(parent) {
        QFont semibold = font();
        semibold.setWeight(QFont::DemiBold);
        setFont(semibold);
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    }

    void ThenPill::setPill(const QString& text, Tone pillTone) {
        label = text;
        tone = pillTone;
        setAccessibleName(text);
        updateGeometry();
        update();
    }

    QSize ThenPill::sizeHint() const {
        return PillSize(fontMetrics(), label);
    }

    QSize ThenPill::minimumSizeHint() const {
        const QFontMetrics metrics = fontMetrics();
        const QSize full = PillSize(metrics, label);
        return {qMin(full.width(), PillSize(metrics, QStringLiteral("Xxxxxxxxxxxx")).width()), full.height()};
    }

    void ThenPill::paintEvent(QPaintEvent*) {
        QPainter painter(this);
        const QSize size = PillSize(fontMetrics(), label);
        const QRect pill(0, (height() - size.height()) / 2, qMin(width(), size.width()), size.height());
        PaintPill(&painter, pill, label, tone);
        // A pill too narrow for its label shows the whole of it on hover.
        const QString tip = pill.width() < size.width() ? Qt::convertFromPlainText(label) : QString();
        if (toolTip() != tip) setToolTip(tip);
    }
} // namespace RouteEditor
