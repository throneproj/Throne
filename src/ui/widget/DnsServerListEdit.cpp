#include "include/ui/widget/DnsServerListEdit.h"

#include "include/configs/generate.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/utils/ReorderListWidget.h"
#include "include/ui/utils/ValidationForm.h"

#include <QAbstractButton>
#include <QAction>
#include <QDynamicPropertyChangeEvent>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace {
    constexpr int dnsListVisibleRows = 6;
    // Below this many rows the list never scrolls, however tight the dialog.
    constexpr int dnsListMinimumRows = 4;
    constexpr int dnsListRecentLimit = 8;

    QFont dnsListScaledFont(QFont font, const qreal factor) {
        if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * factor);
        else if (font.pixelSize() > 0) font.setPixelSize(qMax(1, qRound(font.pixelSize() * factor)));
        return font;
    }

    QFont dnsListFixedFont(const QFont &base) {
        QFont fixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        if (base.pointSizeF() > 0) fixed.setPointSizeF(base.pointSizeF());
        else if (base.pixelSize() > 0) fixed.setPixelSize(base.pixelSize());
        return fixed;
    }

    // horizontalAdvance() rounded up: elidedText() elides a text even a fraction of a pixel wider than its width.
    int dnsListTextWidth(const QFontMetrics &metrics, const QString &text) {
        return static_cast<int>(std::ceil(QFontMetricsF(metrics).horizontalAdvance(text)));
    }

    // Escaped: a tooltip that merely looks like HTML is rendered as HTML.
    QString dnsListTip(const QString &text) {
        return text.isEmpty() ? QString() : Qt::convertFromPlainText(text, Qt::WhiteSpaceNormal);
    }

    QColor dnsListColor(const QWidget *widget, const QColor &token, const QPalette::ColorRole fallback) {
        if (!widget->isEnabled()) return widget->palette().color(QPalette::Disabled, QPalette::Text);
        return token.isValid() ? token : widget->palette().color(fallback);
    }

    void dnsListShowProblem(QLineEdit *edit, const QString &problem) {
        auto *mark = edit->findChild<QAction *>(QStringLiteral("dnsListMark"), Qt::FindDirectChildrenOnly);
        if (mark == nullptr && !problem.isEmpty()) {
            mark = edit->addAction(themeManager()->standardIcon(QStyle::SP_MessageBoxCritical), QLineEdit::TrailingPosition);
            mark->setObjectName(QStringLiteral("dnsListMark"));
        }
        if (mark != nullptr) {
            mark->setToolTip(dnsListTip(problem));
            mark->setVisible(!problem.isEmpty());
        }
        edit->setToolTip(dnsListTip(problem));
        const QVariant invalid = problem.isEmpty() ? QVariant() : QVariant(QStringLiteral("true"));
        if (edit->property("invalid") == invalid) return;
        edit->setProperty("invalid", invalid);
        edit->style()->unpolish(edit);
        edit->style()->polish(edit);
        edit->update();
    }

    QStringList dnsListWithEdit(QStringList addresses, const qsizetype row, const QString &text) {
        if (row < 0 || row >= addresses.size()) return addresses;
        addresses.removeAt(row);
        qsizetype at = row;
        for (const auto &address : Configs::SplitDnsAddresses(text)) {
            if (!addresses.contains(address)) addresses.insert(at++, address);
        }
        return addresses;
    }

    // Without a meta-object of its own, a QWidget subclass gets a style sheet's QWidget background (qdarkstyle) over the row.
    class DnsListPiece : public QWidget {
    public:
        explicit DnsListPiece(QWidget *parent) : QWidget(parent) { setAttribute(Qt::WA_NoSystemBackground); }
    };

    class DnsListGrip : public DnsListPiece {
    public:
        explicit DnsListGrip(QWidget *parent) : DnsListPiece(parent) {
            setToolTip(DnsServerListEdit::tr("Drag to reorder"));
            setCursor(Qt::OpenHandCursor);
            QSizePolicy policy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            policy.setRetainSizeWhenHidden(true);
            setSizePolicy(policy);
        }

        [[nodiscard]] QSize sizeHint() const override {
            const int dot = dotSize();
            return {dot * 5, dot * 7};
        }

    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(dnsListColor(this, themeManager()->tokens.muted, QPalette::PlaceholderText));
            const int dot = dotSize();
            const QPointF origin((width() - dot * 3) / 2.0, (height() - dot * 5) / 2.0);
            for (int column = 0; column < 2; ++column) {
                for (int row = 0; row < 3; ++row) painter.drawEllipse(QRectF(origin + QPointF(column * dot * 2, row * dot * 2), QSizeF(dot, dot)));
            }
        }

    private:
        [[nodiscard]] int dotSize() const { return qMax(2, fontMetrics().height() / 7); }
    };

    class DnsListAddress : public DnsListPiece {
    public:
        DnsListAddress(QWidget *parent, QString address_, QString note_, const bool invalid_)
            : DnsListPiece(parent), address(std::move(address_)), note(std::move(note_)), invalid(invalid_) {
            setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        }

        [[nodiscard]] QSize sizeHint() const override {
            const QFontMetrics fixed(dnsListFixedFont(font()));
            return {dnsListTextWidth(fixed, address), fixed.height()};
        }

    protected:
        // The note sits at the end of the address's line, so every row is one line tall.
        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            const QFont fixedFont = dnsListFixedFont(font());
            const QFontMetrics fixed(fixedFont);
            const int addressWidth = qMin(dnsListTextWidth(fixed, address), width());
            const int space = noteSpace(addressWidth);
            const QRect line(0, (height() - fixed.height()) / 2, space > 0 ? addressWidth : width(), fixed.height());
            const int leading = int(QStyle::visualAlignment(layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter)) | Qt::TextSingleLine;
            painter.setFont(fixedFont);
            painter.setPen(invalid ? dnsListColor(this, themeManager()->tokens.danger, QPalette::Text) : palette().color(QPalette::Text));
            painter.drawText(QStyle::visualRect(layoutDirection(), rect(), line), leading | Qt::TextForceLeftToRight,
                             fixed.elidedText(address, Qt::ElideRight, line.width()));
            if (space <= 0) return;
            const QFont smaller = noteFont();
            const QFontMetrics metrics(smaller);
            const QRect noteRect(width() - space, (height() - metrics.height()) / 2, space, metrics.height());
            const int trailing = int(QStyle::visualAlignment(layoutDirection(), Qt::AlignRight | Qt::AlignVCenter)) | Qt::TextSingleLine;
            painter.setFont(smaller);
            painter.setPen(dnsListColor(this, themeManager()->tokens.muted, QPalette::PlaceholderText));
            painter.drawText(QStyle::visualRect(layoutDirection(), rect(), noteRect), trailing, metrics.elidedText(note, Qt::ElideRight, space));
        }

        void resizeEvent(QResizeEvent *event) override {
            DnsListPiece::resizeEvent(event);
            const int addressWidth = dnsListTextWidth(QFontMetrics(dnsListFixedFont(font())), address);
            QStringList tip;
            if (addressWidth > width()) tip << address;
            if (!note.isEmpty() && noteSpace(addressWidth) < dnsListTextWidth(QFontMetrics(noteFont()), note)) tip << note;
            setToolTip(dnsListTip(tip.join(QLatin1Char('\n'))));
        }

    private:
        [[nodiscard]] QFont noteFont() const { return dnsListScaledFont(font(), 0.9); }

        // Zero when the note has no room worth drawing in; its tooltip carries it then.
        [[nodiscard]] int noteSpace(const int addressWidth) const {
            if (note.isEmpty()) return 0;
            const QFontMetrics metrics(noteFont());
            const int space = width() - addressWidth - 2 * metrics.averageCharWidth();
            return space >= 8 * metrics.averageCharWidth() ? space : 0;
        }

        QString address;
        QString note;
        bool invalid = false;
    };

    class DnsListMark : public DnsListPiece {
    public:
        DnsListMark(QWidget *parent, const QString &problem) : DnsListPiece(parent) {
            setToolTip(dnsListTip(problem));
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        }

        [[nodiscard]] QSize sizeHint() const override {
            const int side = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
            return {side, side};
        }

    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            themeManager()->standardIcon(QStyle::SP_MessageBoxCritical).paint(&painter, rect(), Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled);
        }
    };

    class DnsListRemove : public QToolButton {
    public:
        explicit DnsListRemove(QWidget *parent) : QToolButton(parent) {
            setAutoRaise(true);
            setFocusPolicy(Qt::NoFocus);
            setToolTip(DnsServerListEdit::tr("Remove"));
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        }

        [[nodiscard]] QSize sizeHint() const override {
            ensurePolished();
            QStyleOptionToolButton option;
            initStyleOption(&option);
            option.iconSize = QSize(glyphBox(), glyphBox());
            return style()->sizeFromContents(QStyle::CT_ToolButton, &option, option.iconSize, this);
        }

        [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

    protected:
        void paintEvent(QPaintEvent *event) override {
            QToolButton::paintEvent(event);
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const QColor color = isEnabled() && underMouse() ? palette().color(QPalette::ButtonText)
                                                              : dnsListColor(this, themeManager()->tokens.muted, QPalette::ButtonText);
            const qreal half = glyphBox() / 4.0;
            painter.setPen(QPen(color, qMax(1.5, half / 2.5), Qt::SolidLine, Qt::RoundCap));
            const QPointF center = QRectF(rect()).center();
            painter.drawLine(center + QPointF(-half, -half), center + QPointF(half, half));
            painter.drawLine(center + QPointF(half, -half), center + QPointF(-half, half));
        }

    private:
        [[nodiscard]] int glyphBox() const { return qMax(style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this), fontMetrics().height()); }
    };

    // The row tints itself instead: a style's selected background sits under text colored for an unselected row.
    class DnsListDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            QStyleOptionViewItem plain(option);
            plain.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
            QStyledItemDelegate::paint(painter, plain, index);
        }
    };

    class DnsListRow : public DnsListPiece {
    public:
        DnsListRow(QWidget *parent, const QString &address, const QString &note, const bool draggable) : DnsListPiece(parent) {
            const QString problem = Configs::DnsAddressListProblem(address);
            auto *layout = new QHBoxLayout(this);
            layout->setContentsMargins(4, 2, 2, 2);
            grip = new DnsListGrip(this);
            if (!draggable) grip->hide();
            layout->addWidget(grip);
            text = new DnsListAddress(this, address, note, !problem.isEmpty());
            layout->addWidget(text, 1);
            if (!problem.isEmpty()) layout->addWidget(new DnsListMark(this, problem));
            removeButton = new DnsListRemove(this);
            layout->addWidget(removeButton);
        }

        QLineEdit *startEdit(const QString &address) {
            auto *editor = new QLineEdit(address, this);
            delete layout()->replaceWidget(text, editor);
            text->hide();
            editor->show();
            return editor;
        }

        void setSelected(const bool on) {
            if (selected == on) return;
            selected = on;
            update();
        }

        // From the leading edge, once laid out: where the address starts and where the grip's middle is.
        [[nodiscard]] int addressInset() const { return QStyle::visualRect(layoutDirection(), rect(), text->geometry()).left(); }

        [[nodiscard]] qreal gripCenter() const { return QRectF(QStyle::visualRect(layoutDirection(), rect(), grip->geometry())).center().x(); }

        DnsListRemove *removeButton = nullptr;

    protected:
        void paintEvent(QPaintEvent *) override {
            if (!selected) return;
            QPainter painter(this);
            const QColor accent = themeManager()->tokens.accent;
            QColor tint = accent.isValid() ? accent : palette().color(QPalette::Highlight);
            tint.setAlphaF(isEnabled() ? 0.2f : 0.1f);
            painter.fillRect(rect(), tint);
        }

    private:
        DnsListGrip *grip = nullptr;
        DnsListAddress *text = nullptr;
        bool selected = false;
    };
}

class DnsServerListEdit::Rows : public ReorderListWidget {
public:
    explicit Rows(DnsServerListEdit *owner_) : ReorderListWidget(owner_), owner(owner_) {
        setSelectionMode(QAbstractItemView::SingleSelection);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        setItemDelegate(new DnsListDelegate(this));
        // Ends the scroll bar where the rows end, beside the add row.
        barEnd = new DnsListPiece(this);
        addScrollBarWidget(barEnd, Qt::AlignBottom);
    }

    [[nodiscard]] QSize sizeHint() const override { return {ReorderListWidget::sizeHint().width(), heightFor(dnsListVisibleRows)}; }

    [[nodiscard]] QSize minimumSizeHint() const override { return {ReorderListWidget::minimumSizeHint().width(), heightFor(dnsListMinimumRows)}; }

    // A band under the rows, inside the frame, that never scrolls: the add row's place.
    void setFooterHeight(const int height) {
        if (height == viewportMargins().bottom()) return;
        barEnd->setFixedHeight(height);
        setViewportMargins(0, 0, 0, height);
        updateGeometry();
    }

    [[nodiscard]] QRect footerRect() const {
        const QRect area = viewport()->geometry();
        return {area.left(), area.bottom() + 1, area.width(), viewportMargins().bottom()};
    }

protected:
    bool event(QEvent *event) override {
        // Where the viewport alone fills the list's background, the band outside it needs the same fill.
        if (event->type() == QEvent::Paint && viewport()->autoFillBackground()) {
            QPainter painter(this);
            const QRect inner = contentsRect();
            const int top = viewport()->geometry().bottom() + 1;
            painter.fillRect(QRect(inner.left(), top, inner.width(), inner.bottom() + 1 - top), viewport()->palette().brush(viewport()->backgroundRole()));
        }
        return ReorderListWidget::event(event);
    }

    // The viewport's resize: it also follows the scroll bar coming and going.
    void resizeEvent(QResizeEvent *event) override {
        ReorderListWidget::resizeEvent(event);
        owner->layoutFooter();
    }

    void paintEvent(QPaintEvent *event) override {
        ReorderListWidget::paintEvent(event);
        if (count() > 0) return;
        const auto &tokens = themeManager()->tokens;
        const bool invalid = owner->property("invalid").toString() == QLatin1String("true");
        QPainter painter(viewport());
        painter.setPen(invalid ? dnsListColor(this, tokens.danger, QPalette::Text) : dnsListColor(this, tokens.muted, QPalette::PlaceholderText));
        const QRect area = viewport()->rect().adjusted(6, 0, -6, 0);
        const int align = int(QStyle::visualAlignment(layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter)) | Qt::TextSingleLine;
        painter.drawText(area, align, fontMetrics().elidedText(DnsServerListEdit::tr("Add at least one server."), Qt::ElideRight, area.width()));
    }

private:
    [[nodiscard]] int heightFor(const int shown) const {
        ensurePolished();
        int height = count() == 0 ? owner->rowHeight : 0;
        for (int row = 0; row < qMin(shown, count()); ++row) height += item(row)->sizeHint().height();
        const QMargins inner = viewportMargins();
        return height + 2 * frameWidth() + inner.top() + inner.bottom();
    }

    DnsServerListEdit *owner;
    QWidget *barEnd = nullptr;
};

// The list's last row, pinned in the band under the rows; it opens the add menu.
class DnsServerListEdit::AddRow : public QAbstractButton {
public:
    AddRow(QListWidget *list_, QWidget *parent) : QAbstractButton(parent), list(list_) {
        setFocusPolicy(Qt::TabFocus);
        setText(DnsServerListEdit::tr("Add server"));
        setAccessibleName(text());
    }

    // Both from the leading edge: the plus sits in the grip's column, the label lines up with the addresses.
    void setInsets(const qreal glyph, const int label) {
        glyphCenter = glyph;
        labelInset = label;
        update();
    }

    void setOpen(const bool on) {
        open = on;
        update();
    }

protected:
    bool event(QEvent *event) override {
        // Kept from the dialogs, which bind Delete to deleting something else.
        if (event->type() == QEvent::ShortcutOverride) {
            const auto *key = static_cast<QKeyEvent *>(event);
            if ((key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier && (key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace)) {
                event->accept();
                return true;
            }
        }
        return QAbstractButton::event(event);
    }

    // Opens on press, as a menu button does.
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) {
            QAbstractButton::mousePressEvent(event);
            return;
        }
        event->accept();
        click();
    }

    void keyPressEvent(QKeyEvent *event) override {
        const int key = event->key();
        if ((event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier &&
            (key == Qt::Key_Space || key == Qt::Key_Select || key == Qt::Key_Return || key == Qt::Key_Enter)) {
            // Repeats are taken too: an Enter reaching the dialog would press its default button.
            event->accept();
            if (!event->isAutoRepeat()) click();
            return;
        }
        QAbstractButton::keyPressEvent(event);
    }

    void enterEvent(QEnterEvent *event) override {
        QAbstractButton::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent *event) override {
        QAbstractButton::leaveEvent(event);
        update();
    }

    // Scrolls the rows above; left unaccepted at either end, it carries on to the parents.
    void wheelEvent(QWheelEvent *event) override { QCoreApplication::sendEvent(list->viewport(), event); }

    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        const bool lit = isEnabled() && (underMouse() || hasFocus() || open);
        if (lit) {
            // The hover the style gives the rows above.
            QStyleOptionViewItem option;
            option.initFrom(list);
            option.rect = rect();
            option.styleObject = nullptr;
            option.widget = list;
            // With no text to fit, only this spans the hover and the focus frame over the whole row.
            option.showDecorationSelected = true;
            option.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
            option.state |= QStyle::State_MouseOver;
            if (hasFocus()) option.state |= QStyle::State_HasFocus;
            list->style()->drawControl(QStyle::CE_ItemViewItem, &option, &painter, list);
        }
        const QColor color = lit ? list->palette().color(QPalette::Text) : dnsListColor(this, themeManager()->tokens.muted, QPalette::PlaceholderText);

        // Whole device pixels for the stroke, centred to match, keep the plus sharp at any scale.
        const qreal ratio = devicePixelRatioF();
        const qreal half = fontMetrics().height() * 0.22;
        const qreal stroke = qMax(1.0, std::round(qMax(1.2, half / 2.7) * ratio)) / ratio;
        const qreal offset = std::fmod(std::round(stroke * ratio), 2.0) / 2.0;
        const auto snap = [ratio, offset](const qreal at) { return (std::round(at * ratio - offset) + offset) / ratio; };
        const QPointF center(snap(isRightToLeft() ? width() - glyphCenter : glyphCenter), snap(height() / 2.0));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(color, stroke, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(center - QPointF(half, 0), center + QPointF(half, 0));
        painter.drawLine(center - QPointF(0, half), center + QPointF(0, half));

        const QRect label = QStyle::visualRect(layoutDirection(), rect(), rect().adjusted(labelInset, 0, -2, 0));
        const int align = int(QStyle::visualAlignment(layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter)) | Qt::TextSingleLine;
        painter.drawText(label, align, fontMetrics().elidedText(text(), Qt::ElideRight, label.width()));
    }

private:
    QListWidget *list;
    qreal glyphCenter = 0;
    int labelInset = 0;
    bool open = false;
};

DnsServerListEdit::DnsServerListEdit(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    rows = new Rows(this);
    layout->addWidget(rows);

    // Both lie over the band under the rows, one at a time; the edit comes first in the focus chain, so closing it hands focus to the add row.
    customEdit = new QLineEdit(this);
    customEdit->setPlaceholderText(tr("Address, e.g. tls://1.1.1.1"));
    customEdit->hide();
    addRow = new AddRow(rows, this);
    addMenu = new QMenu(addRow);
    // A press on the row while its menu is open only closes the menu.
    addMenu->setNoReplayFor(addRow);

    raceHint = new QLabel(tr("Queried together; the fastest answer wins."), this);
    raceHint->setProperty("colorRole", QStringLiteral("muted"));
    raceHint->setWordWrap(true);
    raceHint->hide();
    layout->addWidget(raceHint);

    setFocusProxy(rows);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    rows->installEventFilter(this);
    customEdit->installEventFilter(this);
    // Queued: the drop arrives inside the drag's own event loop, under rows a rebuild would delete.
    connect(rows, &ReorderListWidget::reorderRequested, this, &DnsServerListEdit::moveServer, Qt::QueuedConnection);
    connect(rows, &QListWidget::itemDoubleClicked, this, [this](const QListWidgetItem *item) { editServer(rows->row(item)); });
    connect(rows, &QListWidget::itemSelectionChanged, this, &DnsServerListEdit::syncSelection);
    connect(customEdit, &QLineEdit::textEdited, this, [this] { dnsListShowProblem(customEdit, {}); });
    connect(addRow, &QAbstractButton::clicked, this, &DnsServerListEdit::openAddMenu);
    connect(addMenu, &QMenu::aboutToHide, addRow, [this] { addRow->setOpen(false); });
    connect(themeManager(), &ThemeManager::themeChanged, this, &DnsServerListEdit::queueMetrics);

    rebuildRows(-1);
}

void DnsServerListEdit::setServers(const QString &commaList) {
    if (!customEdit->isHidden()) closeCustom();
    addresses = Configs::SplitDnsAddresses(commaList);
    rebuildRows(-1);
}

QString DnsServerListEdit::servers() const {
    QStringList list = addresses;
    // Valid text still in an editor counts: a keyboard OK saves without taking focus from it.
    if (rowEditor && Configs::DnsAddressListProblem(rowEditor->text()).isEmpty()) list = dnsListWithEdit(list, editedRow, rowEditor->text());
    if (!customEdit->isHidden() && Configs::DnsAddressListProblem(customEdit->text()).isEmpty()) {
        for (const auto &address : Configs::SplitDnsAddresses(customEdit->text())) {
            if (!list.contains(address)) list << address;
        }
    }
    return Configs::NormalizeDnsAddressList(list.join(QLatin1Char(',')));
}

void DnsServerListEdit::setPresets(const QStringList &presets) {
    presetServers = Configs::SplitDnsAddresses(presets.join(QLatin1Char(',')));
}

void DnsServerListEdit::setRecent(const QStringList &history) {
    recentServers = Configs::SplitDnsAddresses(history.join(QLatin1Char(',')));
}

void DnsServerListEdit::setPrimaryNote(const bool shown) {
    if (primaryNote == shown) return;
    primaryNote = shown;
    rebuildRows(selectedRow());
}

void DnsServerListEdit::bindTo(Validation::Form *target, const QString &label) {
    form = target;
    target->addRule({this}, [this] { return problem(); }, this, Validation::Severity::Error, label);
}

bool DnsServerListEdit::event(QEvent *event) {
    if (rows != nullptr) {
        if (event->type() == QEvent::DynamicPropertyChange &&
            static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == "invalid")
            rows->viewport()->update();
        else if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange || event->type() == QEvent::LayoutDirectionChange)
            queueMetrics();
    }
    return QWidget::event(event);
}

bool DnsServerListEdit::eventFilter(QObject *watched, QEvent *event) {
    const auto type = event->type();
    const bool keyEvent = type == QEvent::ShortcutOverride || type == QEvent::KeyPress;
    QLineEdit *edit = watched == customEdit ? customEdit : watched == rowEditor.data() ? rowEditor.data() : nullptr;
    if (keyEvent && (watched == rows || edit != nullptr)) {
        const auto *key = static_cast<QKeyEvent *>(event);
        const bool plain = (key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
        if (watched == rows) {
            const bool remove = key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace;
            if (plain && (remove || key->key() == Qt::Key_F2)) {
                // Taken even with nothing selected: the dialogs bind Delete to deleting something else.
                event->accept();
                const int row = selectedRow();
                if (type == QEvent::KeyPress && row >= 0) {
                    if (remove) removeAddress(addresses.value(row), true);
                    else editServer(row);
                }
                return true;
            }
        } else {
            const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
            if (plain && (enter || key->key() == Qt::Key_Escape)) {
                // Kept from the dialog, whose default button and reject() would take them.
                event->accept();
                if (type == QEvent::KeyPress) {
                    if (edit == customEdit) {
                        if (enter) commitCustom();
                        else cancelCustom();
                    } else {
                        if (enter) commitRowEdit(false);
                        else cancelRowEdit();
                    }
                }
                return true;
            }
        }
    } else if (type == QEvent::FocusOut && edit != nullptr && !edit->isHidden()) {
        const auto reason = static_cast<QFocusEvent *>(event)->reason();
        if (reason != Qt::PopupFocusReason && reason != Qt::ActiveWindowFocusReason) {
            if (edit == customEdit) commitCustom();
            else commitRowEdit(false);
        }
    } else if (type == QEvent::Move && watched == rows) {
        layoutFooter();
    }
    return QWidget::eventFilter(watched, event);
}

QString DnsServerListEdit::problem() const {
    if (rowEditor) {
        if (QString why = Configs::DnsAddressListProblem(rowEditor->text()); !why.isEmpty()) return why;
    }
    if (!customEdit->isHidden() && !customEdit->text().trimmed().isEmpty()) {
        if (QString why = Configs::DnsAddressListProblem(customEdit->text()); !why.isEmpty()) return why;
    }
    return Configs::DnsAddressListProblem(servers());
}

int DnsServerListEdit::selectedRow() const {
    const auto selected = rows->selectedItems();
    return selected.isEmpty() ? -1 : rows->row(selected.first());
}

void DnsServerListEdit::rebuildRows(int select) {
    rowEditor = nullptr;
    editedRow = -1;
    const int scroll = rows->verticalScrollBar()->value();
    rows->setUpdatesEnabled(false);
    rows->clear();
    const QString note = tr("Also resolves proxy server addresses");
    for (qsizetype i = 0; i < addresses.size(); ++i) {
        const QString address = addresses[i];
        auto *row = new DnsListRow(rows, address, primaryNote && i == 0 ? note : QString(), addresses.size() > 1);
        connect(row->removeButton, &QToolButton::clicked, this, [this, address] { removeAddress(address, false); });
        auto *item = new QListWidgetItem(rows);
        rows->setItemWidget(item, row);
    }
    select = qMin(select, rows->count() - 1);
    if (select >= 0) rows->setCurrentRow(select);
    refreshMetrics();
    rows->doItemsLayout();
    rows->verticalScrollBar()->setValue(scroll);
    rows->setUpdatesEnabled(true);
    raceHint->setVisible(addresses.size() > 1);
    syncSelection();
    rows->RefreshHover();
}

void DnsServerListEdit::refreshMetrics() {
    metricsQueued = false;
    for (int row = 0; row < rows->count(); ++row) {
        auto *item = rows->item(row);
        if (auto *widget = rows->itemWidget(item)) {
            widget->ensurePolished();
            item->setSizeHint(QSize(0, widget->sizeHint().height()));
        }
    }
    // The empty list, the add row and the custom address all take their measures from a stand-in row.
    DnsListRow probe(rows, QStringLiteral("1.1.1.1"), {}, false);
    probe.ensurePolished();
    probe.resize(probe.sizeHint());
    probe.layout()->activate();
    rowHeight = probe.height();
    addressInset = probe.addressInset();
    addRow->setInsets(probe.gripCenter(), addressInset);
    layoutFooter();
    rows->updateGeometry();
    rows->viewport()->update();
}

void DnsServerListEdit::queueMetrics() {
    if (metricsQueued) return;
    metricsQueued = true;
    QTimer::singleShot(0, this, &DnsServerListEdit::refreshMetrics);
}

void DnsServerListEdit::layoutFooter() {
    if (addRow == nullptr) return;
    // The custom address takes more room than a row when it needs to, as an edited row does.
    const int editHeight = customEdit->sizeHint().height();
    rows->setFooterHeight(customEdit->isHidden() ? rowHeight : qMax(rowHeight, editHeight + 4));
    const QRect band = rows->footerRect().translated(rows->pos());
    addRow->setGeometry(band);
    const QRect edit(band.left() + addressInset, band.top() + (band.height() - editHeight) / 2, qMax(0, band.width() - addressInset - 2), editHeight);
    customEdit->setGeometry(QStyle::visualRect(layoutDirection(), band, edit));
}

void DnsServerListEdit::syncSelection() {
    for (int row = 0; row < rows->count(); ++row) {
        auto *item = rows->item(row);
        if (auto *widget = static_cast<DnsListRow *>(rows->itemWidget(item))) widget->setSelected(item->isSelected());
    }
}

void DnsServerListEdit::openAddMenu() {
    if (addMenu->isVisible()) return;
    fillAddMenu();
    const QSize size = addMenu->sizeHint();
    const QRect row(addRow->mapToGlobal(QPoint(0, 0)), addRow->size());
    QPoint at(addRow->isRightToLeft() ? row.right() + 1 - size.width() : row.left(), row.bottom() + 1);
    // Above the row when the screen has no room below it, as a menu button does.
    if (const QScreen *screen = addRow->screen()) {
        const QRect area = screen->availableGeometry();
        if (at.y() + size.height() > area.bottom() + 1 && row.top() - size.height() >= area.top()) at.setY(row.top() - size.height());
    }
    addRow->setOpen(true);
    addMenu->popup(at);
}

void DnsServerListEdit::fillAddMenu() {
    addMenu->clear();
    QStringList offered;
    const auto offer = [this, &offered](const QString &address) {
        offered << address;
        auto *action = addMenu->addAction(QString(address).replace(QLatin1Char('&'), QStringLiteral("&&")));
        connect(action, &QAction::triggered, this, [this, address] { addServers({address}); });
    };
    for (const auto &preset : std::as_const(presetServers)) {
        if (!addresses.contains(preset)) offer(preset);
    }
    QStringList recent;
    for (const auto &address : std::as_const(recentServers)) {
        if (recent.size() == dnsListRecentLimit) break;
        if (addresses.contains(address) || offered.contains(address) || !Configs::DnsAddressListProblem(address).isEmpty()) continue;
        recent << address;
    }
    if (!recent.isEmpty()) {
        addMenu->addSeparator();
        addMenu->addAction(tr("Recent"))->setEnabled(false);
        for (const auto &address : std::as_const(recent)) offer(address);
    }
    addMenu->addSeparator();
    connect(addMenu->addAction(tr("Custom address")), &QAction::triggered, this, &DnsServerListEdit::openCustom);
}

void DnsServerListEdit::addServers(const QStringList &entered) {
    commitRowEdit(true);
    const qsizetype before = addresses.size();
    for (const auto &address : entered) {
        if (!addresses.contains(address)) addresses << address;
    }
    if (addresses.size() == before) return;
    rebuildRows(int(addresses.size()) - 1);
    rows->scrollToBottom();
    notifyChanged();
}

void DnsServerListEdit::removeAddress(const QString &address, const bool selectNext) {
    if (rowEditor && addresses.value(editedRow) == address) closeRowEditor(false);
    else commitRowEdit(true);
    const int row = int(addresses.indexOf(address));
    if (row < 0) {
        rebuildRows(selectedRow());
        return;
    }
    const QString kept = addresses.value(selectedRow());
    addresses.removeAt(row);
    rebuildRows(selectNext ? qMin(row, int(addresses.size()) - 1) : int(addresses.indexOf(kept)));
    notifyChanged();
}

void DnsServerListEdit::moveServer(const int from, const int to) {
    if (from < 0 || from >= addresses.size() || to < 0 || to >= addresses.size() || from == to) return;
    const QString moved = addresses[from];
    const QString anchor = addresses[to];
    commitRowEdit(true);
    const int source = int(addresses.indexOf(moved));
    const int target = int(addresses.indexOf(anchor));
    if (source < 0 || target < 0 || source == target) return;
    addresses.move(source, target);
    rebuildRows(target);
    notifyChanged();
}

void DnsServerListEdit::editServer(int row) {
    if (!isEnabled() || row < 0 || row >= addresses.size() || row == editedRow) return;
    if (rowEditor) {
        const QString address = addresses[row];
        commitRowEdit(true);
        row = int(addresses.indexOf(address));
        if (row < 0) return;
    }
    auto *item = rows->item(row);
    auto *widget = static_cast<DnsListRow *>(rows->itemWidget(item));
    if (widget == nullptr) return;
    rows->setCurrentRow(row);
    auto *editor = widget->startEdit(addresses[row]);
    rowEditor = editor;
    editedRow = row;
    editor->installEventFilter(this);
    connect(editor, &QLineEdit::textEdited, this, [editor] { dnsListShowProblem(editor, {}); });
    item->setSizeHint(QSize(0, widget->sizeHint().height()));
    rows->scrollToItem(item);
    editor->setFocus(Qt::OtherFocusReason);
    editor->selectAll();
}

bool DnsServerListEdit::closeRowEditor(const bool apply) {
    if (!rowEditor) return true;
    if (apply) {
        const QString text = rowEditor->text();
        if (const QString why = Configs::DnsAddressListProblem(text); !why.isEmpty()) {
            dnsListShowProblem(rowEditor, why);
            return false;
        }
        addresses = dnsListWithEdit(addresses, editedRow, text);
    }
    const bool hadFocus = rowEditor->hasFocus();
    rowEditor = nullptr;
    editedRow = -1;
    if (hadFocus) rows->setFocus(Qt::OtherFocusReason);
    return true;
}

void DnsServerListEdit::commitRowEdit(const bool dropInvalid) {
    if (!rowEditor) return;
    const int row = editedRow;
    const QStringList before = addresses;
    if (!closeRowEditor(true)) {
        if (!dropInvalid) {
            revalidate();
            return;
        }
        closeRowEditor(false);
    }
    rebuildRows(row);
    if (addresses != before) notifyChanged();
    else revalidate();
}

void DnsServerListEdit::cancelRowEdit() {
    if (!rowEditor) return;
    const int row = editedRow;
    closeRowEditor(false);
    rebuildRows(row);
    revalidate();
}

void DnsServerListEdit::openCustom() {
    commitRowEdit(true);
    customEdit->show();
    layoutFooter();
    // Before the add row hides, or its focus would move on down the dialog.
    customEdit->setFocus(Qt::OtherFocusReason);
    addRow->hide();
    // After the menu has closed and handed focus back.
    QTimer::singleShot(0, customEdit, [this] {
        if (!customEdit->isHidden()) customEdit->setFocus(Qt::OtherFocusReason);
    });
}

void DnsServerListEdit::closeCustom() {
    const bool hadFocus = customEdit->hasFocus();
    addRow->show();
    // Hidden first, so its focus out commits nothing; the focus passes to the add row, next in the chain.
    customEdit->hide();
    if (hadFocus && !addRow->hasFocus()) addRow->setFocus(Qt::OtherFocusReason);
    customEdit->clear();
    dnsListShowProblem(customEdit, {});
    layoutFooter();
}

void DnsServerListEdit::commitCustom() {
    const QString text = customEdit->text();
    const QStringList entered = Configs::SplitDnsAddresses(text);
    if (!entered.isEmpty()) {
        if (const QString why = Configs::DnsAddressListProblem(text); !why.isEmpty()) {
            dnsListShowProblem(customEdit, why);
            revalidate();
            return;
        }
    }
    closeCustom();
    addServers(entered);
    revalidate();
}

void DnsServerListEdit::cancelCustom() {
    closeCustom();
    revalidate();
}

void DnsServerListEdit::notifyChanged() {
    emit changed();
    revalidate();
}

void DnsServerListEdit::revalidate() {
    if (form) form->revalidate(this);
}
