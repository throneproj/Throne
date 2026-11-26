#include "include/ui/setting/route/RouteEditorWidgets.h"

#include "include/ui/setting/ThemeManager.hpp"

#include <QAbstractButton>
#include <QBoxLayout>
#include <QClipboard>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QStyle>
#include <QTextDocument>
#include <QToolTip>

namespace RouteEditor {
    namespace {
        struct ChipMetrics {
            int padding;
            int gap;
            int removeSize;
            int removeMargin;
            int heightPad;
            int removePad;
            qreal crossInset;
        };
        constexpr ChipMetrics kChipRegular{8, 6, 16, 4, 8, 6, 5.0};
        constexpr ChipMetrics kChipCompact{7, 4, 14, 3, 4, 4, 4.5};

        const ChipMetrics& chipMetrics(bool compact) {
            return compact ? kChipCompact : kChipRegular;
        }

        // The width a chip needs around a value of textWidth.
        int chipWidth(const ChipMetrics& chip, const QFontMetrics& metrics, const QString& kind, bool removable, int textWidth) {
            int width = chip.padding + textWidth;
            if (!kind.isEmpty()) width += TextWidth(metrics, kind) + chip.gap;
            width += removable ? chip.gap / 2 + chip.removeSize + chip.removeMargin : chip.padding;
            return width;
        }

        constexpr int kTreeGap = 5;
        constexpr int kTreeNoteGap = 3;
        // Counted from the spine's pixel column: the elbow covers kTreeElbow columns, and the branch starts kTreeIndent away.
        constexpr int kTreeElbow = 15;
        constexpr int kTreeIndent = 20;
        constexpr qreal kTreeCorner = 6;
        constexpr double kTreeLineAmount = 0.28;
        constexpr int kTreeBadgePadding = 7;
        // Leaf margins, the 1 px border included.
        constexpr int kTreeLeafLead = 9;
        constexpr int kTreeLeafTop = 4;
        constexpr int kTreeLeafTrail = 5;
        constexpr int kTreeLeafBottom = 4;
        constexpr int kTreeLeafLabelGap = 8;
        constexpr int kTreeLeafValueGap = 4;

        QFont treeScaledFont(const QFont& base, qreal scale) {
            QFont font = base;
            if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * scale);
            else if (font.pixelSize() > 0) font.setPixelSize(qMax(8, qRound(font.pixelSize() * scale)));
            return font;
        }

        QFont treeLabelFont(const QFont& base) {
            return treeScaledFont(base, 0.92);
        }

        QFont treeBadgeFont(const QFont& base) {
            QFont font = treeScaledFont(base, 0.8);
            font.setWeight(QFont::Bold);
            font.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
            return font;
        }

        QSize treeBadgeSize(const QFont& base, const QString& text) {
            const QFontMetrics metrics(treeBadgeFont(base));
            return {TextWidth(metrics, text) + 2 * kTreeBadgePadding + 2, metrics.height() + 5};
        }

        // In the branch's own coordinates.
        int treeAnchorOf(const QWidget* item) {
            if (const auto* leaf = qobject_cast<const TreeLeaf*>(item)) return leaf->anchorY();
            if (const auto* node = qobject_cast<const TreeNode*>(item)) return node->anchorY();
            return item->height() / 2;
        }

        // A tree node's operator in an outlined pill; its fonts derive from the widget's, so they follow font changes.
        class TreeBadge : public QWidget {
        public:
            TreeBadge(const QString& text, QWidget* parent) : QWidget(parent), label(text) {
                setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                setAccessibleName(text);
            }

            [[nodiscard]] QSize sizeHint() const override { return treeBadgeSize(font(), label); }
            [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

        protected:
            void paintEvent(QPaintEvent*) override {
                const auto& tokens = themeManager()->tokens;
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing, true);
                const QRectF pill = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
                painter.setPen(QPen(Mix(tokens.onSurface, tokens.surface, 0.3), 1));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(pill, pill.height() / 2, pill.height() / 2);
                painter.setFont(treeBadgeFont(font()));
                painter.setPen(Mix(tokens.onSurface, tokens.surface, 0.75));
                painter.drawText(rect(), Qt::AlignCenter, label);
            }

        private:
            QString label;
        };
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
        if (!stacked) return minimumSize();
        // Stacked lines don't wrap at the widest one's width.
        int widest = 0;
        int lineWidth = -1;
        for (const QLayoutItem* item : items) {
            if (item->isEmpty()) continue;
            const int width = item->sizeHint().expandedTo(item->minimumSize()).width();
            const bool ownLine = qobject_cast<QAbstractButton*>(item->widget()) == nullptr;
            lineWidth = (lineWidth < 0 || ownLine) ? width : lineWidth + hSpace + width;
            widest = qMax(widest, lineWidth);
        }
        const QMargins margins = contentsMargins();
        const int width = widest + margins.left() + margins.right();
        return {width, heightForWidth(width)};
    }

    void FlowLayout::setGeometry(const QRect& rect) {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

    void FlowLayout::setStacked(bool on) {
        stacked = on;
        invalidate();
    }

    int FlowLayout::firstLineHeight(int width) const {
        int first = 0;
        doLayout(QRect(0, 0, width, 0), true, &first);
        return first;
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

    int FlowLayout::doLayout(const QRect& rect, bool testOnly, int* firstLine) const {
        const QMargins margins = contentsMargins();
        const QRect area = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
        const QWidget* owner = parentWidget();
        const bool mirrored = owner != nullptr && owner->isRightToLeft();
        int y = area.y();
        int lineWidth = 0;
        int lineHeight = 0;
        bool first = true;
        QList<QPair<QLayoutItem*, QSize>> line;
        // Items of a line are centred on it, so a chip lines up with a taller button beside it.
        const auto flush = [&] {
            int x = area.x();
            if (!testOnly) {
                for (const auto& [item, size] : line) {
                    const QRect geometry(QPoint(x, y + (lineHeight - size.height()) / 2), size);
                    item->setGeometry(mirrored ? QStyle::visualRect(Qt::RightToLeft, rect, geometry) : geometry);
                    x += size.width() + hSpace;
                }
            }
            if (firstLine != nullptr && first) *firstLine = lineHeight;
            first = false;
            y += lineHeight + vSpace;
            line.clear();
            lineWidth = 0;
            lineHeight = 0;
        };
        for (QLayoutItem* item : items) {
            if (item->isEmpty()) continue;
            const QSize size = item->sizeHint().boundedTo(QSize(qMax(1, area.width()), QWIDGETSIZE_MAX)).expandedTo(item->minimumSize());
            const bool ownLine = stacked && qobject_cast<QAbstractButton*>(item->widget()) == nullptr;
            if (!line.isEmpty() && (ownLine || lineWidth + hSpace + size.width() > area.width())) flush();
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

    void ValueChip::setCompact(bool on) {
        compact = on;
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
        const ChipMetrics& chip = chipMetrics(compact);
        return {chipWidth(chip, metrics, kindText, removable, TextWidth(metrics, shownValue())),
                qMax(metrics.height() + chip.heightPad, chip.removeSize + chip.removePad)};
    }

    QSize ValueChip::minimumSizeHint() const {
        const QFontMetrics metrics(font());
        const int text = qMin(TextWidth(metrics, shownValue()), metrics.averageCharWidth() * 8);
        return {chipWidth(chipMetrics(compact), metrics, kindText, removable, text), sizeHint().height()};
    }

    QRect ValueChip::removeRect() const {
        if (!removable) return {};
        const ChipMetrics& chip = chipMetrics(compact);
        return {width() - chip.removeMargin - chip.removeSize, (height() - chip.removeSize) / 2, chip.removeSize, chip.removeSize};
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
                const bool elided = chipWidth(chipMetrics(compact), metrics, kindText, removable, TextWidth(metrics, shownValue())) > width();
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
        // Copy must reach keyPressEvent even when a Ctrl+C shortcut is set further up.
        if (event->type() == QEvent::ShortcutOverride && static_cast<QKeyEvent*>(event)->matches(QKeySequence::Copy)) {
            event->accept();
            return true;
        }
        return QWidget::event(event);
    }

    void ValueChip::paintEvent(QPaintEvent*) {
        const auto& tokens = themeManager()->tokens;
        const ChipMetrics& chip = chipMetrics(compact);
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
        int x = chip.padding;
        if (!kindText.isEmpty()) {
            painter.setPen(tokens.muted);
            const int kindWidth = TextWidth(metrics, kindText);
            painter.drawText(QRect(x, 0, kindWidth, height()), Qt::AlignVCenter | Qt::AlignLeft, kindText);
            x += kindWidth + chip.gap;
        }
        const int right = removable ? removeRect().left() - chip.gap / 2 : width() - chip.padding;
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
        const QRectF cross = QRectF(button).adjusted(chip.crossInset, chip.crossInset, -chip.crossInset, -chip.crossInset);
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
        if (event->matches(QKeySequence::Copy)) {
            QGuiApplication::clipboard()->setText(value);
            event->accept();
            return;
        }
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

    TreeNode::TreeNode(const QString& op, QWidget* parent) : QWidget(parent), operatorText(op) {
        auto* column = new QVBoxLayout(this);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(kTreeGap);
        headLayout = new QHBoxLayout;
        headLayout->setSpacing(8);
        badge = new TreeBadge(op, this);
        headLayout->addWidget(badge);
        headLayout->addStretch(1);
        column->addLayout(headLayout);
        auto* body = new QHBoxLayout;
        body->setSpacing(0);
        // A spacer, not a margin: box layouts mirror their items right to left, but not their margins.
        indentSpacer = new QSpacerItem(indent(), 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        body->addSpacerItem(indentSpacer);
        kids = new QVBoxLayout;
        kids->setSpacing(kTreeGap);
        body->addLayout(kids, 1);
        column->addLayout(body);
        connect(themeManager(), &ThemeManager::themeChanged, this, [this] { update(); });
    }

    void TreeNode::addBranch(QWidget* item, QWidget* note) {
        auto* slot = new QWidget(this);
        auto* slotLayout = new QVBoxLayout(slot);
        slotLayout->setContentsMargins(0, 0, 0, 0);
        slotLayout->setSpacing(kTreeNoteGap);
        slotLayout->addWidget(item, 0, Qt::AlignLeading);
        if (note != nullptr) slotLayout->addWidget(note);
        kids->insertWidget(tail != nullptr ? kids->indexOf(tail.data()) : -1, slot);
        slot->installEventFilter(this);
        item->installEventFilter(this);
        branches.append(Branch{slot, item});
        update();
    }

    void TreeNode::setTail(QWidget* item) {
        tail = item;
        kids->addWidget(item, 0, Qt::AlignLeading);
        item->installEventFilter(this);
        update();
    }

    void TreeNode::clearBranches() {
        for (const Branch& branch : std::as_const(branches)) {
            if (branch.slot == nullptr) continue;
            kids->removeWidget(branch.slot);
            branch.slot->hide();
            branch.slot->deleteLater();
        }
        branches.clear();
        update();
    }

    void TreeNode::setLinesShown(bool shown) {
        linesShown = shown;
        badge->setVisible(shown);
        update();
    }

    int TreeNode::anchorY() const {
        const QRect line = badge->isHidden() ? headLayout->geometry() : badge->geometry();
        return line.top() + line.height() / 2;
    }

    int TreeNode::spineOffset() const {
        return (treeBadgeSize(font(), operatorText).width() - 1) / 2;
    }

    int TreeNode::spineX() const {
        return isRightToLeft() ? width() - 1 - spineOffset() : spineOffset();
    }

    int TreeNode::indent() const {
        return spineOffset() + kTreeIndent;
    }

    bool TreeNode::event(QEvent* event) {
        const bool handled = QWidget::event(event);
        // The layout has moved the branches by now.
        if (event->type() == QEvent::LayoutRequest) update();
        return handled;
    }

    void TreeNode::changeEvent(QEvent* event) {
        if (event->type() == QEvent::FontChange) indentSpacer->changeSize(indent(), 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        QWidget::changeEvent(event);
    }

    bool TreeNode::eventFilter(QObject* watched, QEvent* event) {
        switch (event->type()) {
            case QEvent::Move:
            case QEvent::Resize:
            case QEvent::Show:
            case QEvent::Hide:
            case QEvent::LayoutRequest: update(); break;
            default: break;
        }
        return QWidget::eventFilter(watched, event);
    }

    void TreeNode::paintEvent(QPaintEvent*) {
        if (!linesShown) return;
        QList<int> anchors;
        int dashFrom = -1;
        for (const Branch& branch : std::as_const(branches)) {
            if (branch.slot == nullptr || branch.item == nullptr || branch.slot->isHidden()) continue;
            anchors << branch.item->mapTo(this, QPoint(0, treeAnchorOf(branch.item))).y();
            dashFrom = branch.slot->geometry().bottom() + 1;
        }
        const bool dashed = tail != nullptr && !tail->isHidden();
        if (dashed) anchors << tail->geometry().top() + treeAnchorOf(tail);
        if (anchors.isEmpty()) return;

        const auto& tokens = themeManager()->tokens;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(Qt::NoBrush);
        QPen pen(Mix(tokens.onSurface, tokens.surface, kTreeLineAmount), 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
        // Half-pixel centres keep the 1 px lines sharp.
        const qreal dir = isRightToLeft() ? -1 : 1;
        const qreal x = spineX() + 0.5;
        const qreal reach = x + dir * (kTreeElbow - 0.5);
        const qreal top = badge->geometry().bottom() + 1;
        const qreal last = anchors.last() + 0.5;
        const qreal radius = qMin(kTreeCorner, qMax<qreal>(0, last - top));
        const qreal dashTop = dashFrom >= 0 ? dashFrom : top;
        const auto turn = [&](QPainterPath* path) {
            // Bezier handles for a quarter circle.
            constexpr qreal kappa = 0.5523;
            path->lineTo(x, last - radius);
            path->cubicTo(x, last - radius * (1 - kappa), x + dir * radius * (1 - kappa), last, x + dir * radius, last);
            path->lineTo(reach, last);
        };

        QPainterPath solid;
        solid.moveTo(x, top);
        if (dashed) solid.lineTo(x, dashTop);
        else turn(&solid);
        for (int i = 0; i + 1 < anchors.size(); ++i) {
            solid.moveTo(x, anchors.at(i) + 0.5);
            solid.lineTo(reach, anchors.at(i) + 0.5);
        }
        painter.setPen(pen);
        painter.drawPath(solid);
        if (!dashed) return;

        QPainterPath ghost;
        ghost.moveTo(x, dashTop);
        turn(&ghost);
        pen.setDashPattern({3.0, 3.0});
        painter.setPen(pen);
        painter.drawPath(ghost);
    }

    TreeLeaf::TreeLeaf(QWidget* parent) : QWidget(parent) {
        values = new FlowLayout(this, kTreeLeafValueGap, kTreeLeafValueGap);
        values->setStacked(true);
        updateMargins();
    }

    void TreeLeaf::setLabel(const QString& text, const QString& tip) {
        label = text;
        labelTip = tip;
        setAccessibleName(tip.isEmpty() ? text : tip);
        updateMargins();
        update();
    }

    void TreeLeaf::setLabelColumn(const QStringList& labels) {
        column = labels;
        updateMargins();
        update();
    }

    int TreeLeaf::anchorY() const {
        const int line = qMax(values->firstLineHeight(width()), QFontMetrics(treeLabelFont(font())).height());
        return kTreeLeafTop + line / 2;
    }

    int TreeLeaf::labelWidth() const {
        const QFontMetrics metrics(treeLabelFont(font()));
        int widest = TextWidth(metrics, label);
        for (const QString& text : column) widest = qMax(widest, TextWidth(metrics, text));
        return widest;
    }

    QRect TreeLeaf::labelRect() const {
        const int height = QFontMetrics(treeLabelFont(font())).height();
        return QStyle::visualRect(layoutDirection(), rect(), QRect(kTreeLeafLead, anchorY() - height / 2, labelWidth(), height));
    }

    // The label is painted in the values' leading margin.
    void TreeLeaf::updateMargins() {
        values->setContentsMargins(kTreeLeafLead + labelWidth() + kTreeLeafLabelGap, kTreeLeafTop, kTreeLeafTrail, kTreeLeafBottom);
    }

    bool TreeLeaf::event(QEvent* event) {
        if (event->type() == QEvent::ToolTip) {
            const auto* help = static_cast<QHelpEvent*>(event);
            const QRect area = labelRect();
            if (!labelTip.isEmpty() && area.contains(help->pos())) {
                QToolTip::showText(help->globalPos(), Qt::convertFromPlainText(labelTip), this, area);
            } else {
                QToolTip::hideText();
                event->ignore();
            }
            return true;
        }
        return QWidget::event(event);
    }

    void TreeLeaf::changeEvent(QEvent* event) {
        if (event->type() == QEvent::FontChange) updateMargins();
        QWidget::changeEvent(event);
    }

    void TreeLeaf::paintEvent(QPaintEvent*) {
        const auto& tokens = themeManager()->tokens;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(Mix(tokens.onSurface, tokens.surface, 0.14), 1));
        painter.setBrush(Mix(tokens.onSurface, tokens.surface, 0.02));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
        painter.setFont(treeLabelFont(font()));
        painter.setPen(tokens.muted);
        painter.drawText(labelRect(), QStyle::visualAlignment(layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter), label);
    }
} // namespace RouteEditor
