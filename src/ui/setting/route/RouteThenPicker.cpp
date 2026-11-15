#include "include/ui/setting/route/RouteThenPicker.h"

#include "include/global/Configs.hpp"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/route/RouteEditorCommon.h"

#include <QFrame>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScreen>
#include <QScrollBar>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace {
    enum class ThenPickerKind { Section, Option, Note };

    constexpr int thenPickerKindRole = Qt::UserRole;
    constexpr int thenPickerThenRole = Qt::UserRole + 1;
    constexpr int thenPickerProfileRole = Qt::UserRole + 2;
    constexpr int thenPickerHintRole = Qt::UserRole + 3;
    constexpr int thenPickerCurrentRole = Qt::UserRole + 4;
    constexpr int thenPickerMaxProfiles = 8;
    constexpr int thenPickerPad = 8;
    constexpr int thenPickerArrowGap = 8;
    constexpr int thenPickerPageStep = 5;

    QFont thenPickerSmallFont(const QFont& base) {
        QFont font = base;
        if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * 0.88);
        else if (font.pixelSize() > 0) font.setPixelSize(qMax(8, qRound(font.pixelSize() * 0.88)));
        return font;
    }

    ThenPickerKind thenPickerKindOf(const QModelIndex& index) {
        return static_cast<ThenPickerKind>(index.data(thenPickerKindRole).toInt());
    }

    ThenPickerKind thenPickerKindOf(const QListWidgetItem* item) {
        return static_cast<ThenPickerKind>(item->data(thenPickerKindRole).toInt());
    }

    class ThenPickerDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
            const auto& tokens = themeManager()->tokens;
            const ThenPickerKind kind = thenPickerKindOf(index);
            const QString text = index.data(Qt::DisplayRole).toString();
            const QRect area = option.rect.adjusted(thenPickerPad, 0, -thenPickerPad, 0);
            painter->save();
            if (kind != ThenPickerKind::Option) {
                QFont font = thenPickerSmallFont(option.font);
                if (kind == ThenPickerKind::Section) font.setWeight(QFont::DemiBold);
                const QFontMetrics metrics(font);
                painter->setFont(font);
                painter->setPen(tokens.muted);
                const Qt::Alignment align = Qt::AlignLeft | (kind == ThenPickerKind::Section ? Qt::AlignBottom : Qt::AlignVCenter);
                painter->drawText(area.adjusted(0, 0, 0, -2), align, metrics.elidedText(text, Qt::ElideRight, area.width()));
                painter->restore();
                return;
            }

            painter->setRenderHint(QPainter::Antialiasing, true);
            if (option.state & QStyle::State_Selected) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(RouteEditor::Mix(tokens.accent, tokens.surface, 0.16));
                painter->drawRoundedRect(QRectF(option.rect).adjusted(2.5, 0.5, -2.5, -0.5), 5, 5);
            }
            const QFontMetrics metrics(option.font);
            const QFont smallFont = thenPickerSmallFont(option.font);
            const QFontMetrics small(smallFont);
            const int mark = metrics.height();
            const QRect textArea = area.adjusted(0, 5, -(mark + 6), -5);
            const QRect labelRect(textArea.left(), textArea.top(), textArea.width(), metrics.height());
            painter->setFont(option.font);
            painter->setPen(tokens.onSurface);
            painter->drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(text, Qt::ElideRight, labelRect.width()));
            const QString hint = index.data(thenPickerHintRole).toString();
            if (!hint.isEmpty()) {
                const QRect hintRect(textArea.left(), labelRect.bottom() + 2, textArea.width(), small.height());
                painter->setFont(smallFont);
                painter->setPen(tokens.muted);
                painter->drawText(hintRect, Qt::AlignLeft | Qt::AlignVCenter, small.elidedText(hint, Qt::ElideRight, hintRect.width()));
            }
            if (index.data(thenPickerCurrentRole).toBool()) {
                const QRectF box(area.right() - mark + 1, labelRect.top(), mark, mark);
                QPainterPath check;
                check.moveTo(box.left() + box.width() * 0.2, box.top() + box.height() * 0.52);
                check.lineTo(box.left() + box.width() * 0.42, box.top() + box.height() * 0.74);
                check.lineTo(box.left() + box.width() * 0.8, box.top() + box.height() * 0.3);
                painter->setPen(QPen(tokens.accent, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter->setBrush(Qt::NoBrush);
                painter->drawPath(check);
            }
            painter->restore();
        }

        [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
            const QFontMetrics small(thenPickerSmallFont(option.font));
            switch (thenPickerKindOf(index)) {
                case ThenPickerKind::Section: return {0, small.height() + 10};
                case ThenPickerKind::Note: return {0, small.height() + 8};
                case ThenPickerKind::Option: break;
            }
            const QFontMetrics metrics(option.font);
            const bool hint = !index.data(thenPickerHintRole).toString().isEmpty();
            return {0, metrics.height() + (hint ? small.height() + 2 : 0) + 10};
        }
    };

    // The Then picker's popup: a search field over sections of options; the search field keeps the focus throughout.
    class ThenPickerPopup : public QFrame {
    public:
        using Pick = std::function<void(RouteEditor::Then, int)>;

        ThenPickerPopup(QWidget* anchorWidget, const Configs::RouteRule& rule, Pick onPick)
            : QFrame(anchorWidget, Qt::Popup), anchor(anchorWidget), pick(std::move(onPick)) {
            setAttribute(Qt::WA_DeleteOnClose);
            setFrameShape(QFrame::StyledPanel);
            current = RouteEditor::ThenOf(rule);
            currentProfile = current == RouteEditor::Then::Profile ? rule.outboundID : -1;
            offerBypass = getOS() == Linux || current == RouteEditor::Then::Bypass;

            auto* layout = new QVBoxLayout(this);
            layout->setContentsMargins(6, 6, 6, 6);
            layout->setSpacing(4);
            search = new QLineEdit(this);
            search->setPlaceholderText(RouteThenPicker::tr("Search profiles"));
            search->setClearButtonEnabled(true);
            search->installEventFilter(this);
            layout->addWidget(search);

            list = new QListWidget(this);
            list->setFrameShape(QFrame::NoFrame);
            list->setFocusPolicy(Qt::NoFocus);
            list->setMouseTracking(true);
            list->viewport()->setMouseTracking(true);
            list->setSelectionMode(QAbstractItemView::SingleSelection);
            list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            list->setItemDelegate(new ThenPickerDelegate(list));
            layout->addWidget(list, 1);

            connect(search, &QLineEdit::textChanged, this, [this] {
                rebuild();
                fit();
            });
            connect(list, &QListWidget::entered, this, [this](const QModelIndex& index) {
                if (thenPickerKindOf(index) == ThenPickerKind::Option) list->setCurrentRow(index.row());
            });
            connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { pickItem(item); });
            rebuild();
        }

        void popUp() {
            fit();
            show();
            search->setFocus(Qt::PopupFocusReason);
        }

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override {
            if (watched == search && event->type() == QEvent::KeyPress) {
                switch (static_cast<QKeyEvent*>(event)->key()) {
                    case Qt::Key_Up: moveCurrent(-1); return true;
                    case Qt::Key_Down: moveCurrent(1); return true;
                    case Qt::Key_PageUp: moveCurrent(-thenPickerPageStep); return true;
                    case Qt::Key_PageDown: moveCurrent(thenPickerPageStep); return true;
                    case Qt::Key_Return:
                    case Qt::Key_Enter: pickItem(list->currentItem()); return true;
                    case Qt::Key_Escape: close(); return true;
                    default: break;
                }
            }
            return QFrame::eventFilter(watched, event);
        }

        void mousePressEvent(QMouseEvent* event) override {
            // A press on the picker that opened the popup only closes it, rather than reopening it.
            if (anchor != nullptr && !rect().contains(event->position().toPoint())) {
                if (anchor->rect().contains(anchor->mapFromGlobal(event->globalPosition().toPoint()))) setAttribute(Qt::WA_NoMouseReplay);
            }
            QFrame::mousePressEvent(event);
        }

    private:
        void addSection(const QString& title) {
            auto* item = new QListWidgetItem(title, list);
            item->setFlags(Qt::NoItemFlags);
            item->setData(thenPickerKindRole, static_cast<int>(ThenPickerKind::Section));
        }

        void addNote(const QString& text) {
            auto* item = new QListWidgetItem(text, list);
            item->setFlags(Qt::NoItemFlags);
            item->setData(thenPickerKindRole, static_cast<int>(ThenPickerKind::Note));
        }

        void addOption(const QString& label, RouteEditor::Then then, int profileId, const QString& hint) {
            auto* item = new QListWidgetItem(label, list);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setData(thenPickerKindRole, static_cast<int>(ThenPickerKind::Option));
            item->setData(thenPickerThenRole, static_cast<int>(then));
            item->setData(thenPickerProfileRole, profileId);
            item->setData(thenPickerHintRole, hint);
            const bool isCurrent = then == current && (then != RouteEditor::Then::Profile || profileId == currentProfile);
            item->setData(thenPickerCurrentRole, isCurrent);
            if (isCurrent) item->setData(Qt::AccessibleDescriptionRole, RouteThenPicker::tr("Current choice"));
        }

        void rebuild() {
            using RouteEditor::Then;
            const QString needle = search->text().trimmed();
            const auto matches = [&needle](const QString& text) { return needle.isEmpty() || text.contains(needle, Qt::CaseInsensitive); };
            list->clear();

            const QString next = RouteThenPicker::tr("Continues with the next rule");
            QList<QPair<Then, QString>> others = {{Then::HijackDns, {}}, {Then::Sniff, next}, {Then::Resolve, next}, {Then::RouteOptions, next}};
            if (offerBypass) others.append({Then::Bypass, RouteThenPicker::tr("Linux auto-redirect only")});
            const QList<QPair<QString, QList<QPair<Then, QString>>>> sections = {
                {RouteThenPicker::tr("Send through"),
                 {{Then::Proxy, RouteThenPicker::tr("The selected profile")},
                  {Then::Direct, RouteThenPicker::tr("No proxy")},
                  {Then::NoWarp, RouteThenPicker::tr("Proxy minus the WARP or Tor layer")}}},
                {RouteThenPicker::tr("Stop it"), {{Then::Block, {}}}},
                {RouteThenPicker::tr("Other actions"), others},
            };
            for (const auto& [title, offers] : sections) {
                bool headed = false;
                for (const auto& [then, hint] : offers) {
                    const QString label = RouteEditor::ThenLabel(then);
                    if (!matches(label)) continue;
                    if (!headed) addSection(title);
                    headed = true;
                    addOption(label, then, -1, hint);
                }
            }

            QList<QPair<QString, int>> profiles;
            int more = 0;
            if (currentProfile >= 0) {
                const QString label = RouteEditor::ProfileLabel(currentProfile);
                if (matches(label)) profiles.append({label, currentProfile});
            }
            for (const auto& [label, id] : RouteEditor::ProfileChoices()) {
                if (id == currentProfile || !matches(label)) continue;
                if (profiles.size() < thenPickerMaxProfiles) profiles.append({label, id});
                else ++more;
            }
            if (!profiles.isEmpty()) {
                addSection(RouteThenPicker::tr("A specific profile"));
                for (const auto& [label, id] : profiles) addOption(label, Then::Profile, id, {});
            }
            if (more > 0) addNote(RouteThenPicker::tr("%n more profiles: type to find them.", nullptr, more));
            if (list->count() == 0) addNote(RouteThenPicker::tr("Nothing matches."));

            int row = -1;
            for (int i = 0; i < list->count(); ++i) {
                const auto* item = list->item(i);
                if (thenPickerKindOf(item) != ThenPickerKind::Option) continue;
                if (row < 0 || item->data(thenPickerCurrentRole).toBool()) row = i;
                if (item->data(thenPickerCurrentRole).toBool()) break;
            }
            if (row >= 0) list->setCurrentRow(row);
        }

        void moveCurrent(int step) {
            const int count = list->count();
            if (count == 0 || step == 0) return;
            const int direction = step > 0 ? 1 : -1;
            int row = list->currentRow();
            if (row < 0) row = direction > 0 ? -1 : count;
            int target = -1;
            int remaining = qAbs(step);
            for (int i = row + direction; i >= 0 && i < count && remaining > 0; i += direction) {
                if (thenPickerKindOf(list->item(i)) != ThenPickerKind::Option) continue;
                target = i;
                --remaining;
            }
            if (target >= 0) list->setCurrentRow(target);
        }

        void pickItem(const QListWidgetItem* item) {
            if (item == nullptr || thenPickerKindOf(item) != ThenPickerKind::Option) return;
            const auto then = static_cast<RouteEditor::Then>(item->data(thenPickerThenRole).toInt());
            const int profileId = item->data(thenPickerProfileRole).toInt();
            const Pick callback = pick;
            close();
            if (callback) callback(then, profileId);
        }

        void fit() {
            if (anchor == nullptr) return;
            const QRect avail = anchor->screen()->availableGeometry();
            const QFontMetrics metrics(list->font());
            const QFontMetrics small(thenPickerSmallFont(list->font()));
            int textWidth = 0;
            int listHeight = 2 * list->frameWidth() + 2;
            for (int i = 0; i < list->count(); ++i) {
                const auto* item = list->item(i);
                const ThenPickerKind kind = thenPickerKindOf(item);
                int width = (kind == ThenPickerKind::Option ? metrics : small).horizontalAdvance(item->text());
                if (kind == ThenPickerKind::Option) {
                    width += metrics.height() + 6;
                    width = qMax(width, small.horizontalAdvance(item->data(thenPickerHintRole).toString()));
                }
                textWidth = qMax(textWidth, width);
                listHeight += list->sizeHintForRow(i);
            }
            const QMargins margins = layout()->contentsMargins();
            const int chromeWidth = 2 * thenPickerPad + list->verticalScrollBar()->sizeHint().width() + margins.left() + margins.right() + 2 * frameWidth() + 4;
            const int widest = qMax(anchor->width(), avail.width() / 3);
            const int width = qMin(qBound(anchor->width(), textWidth + chromeWidth, widest), avail.width());
            int height = margins.top() + margins.bottom() + layout()->spacing() + search->sizeHint().height() + 2 * frameWidth() + listHeight;

            const QPoint below = anchor->mapToGlobal(QPoint(0, anchor->height()));
            const QPoint top = anchor->mapToGlobal(QPoint(0, 0));
            const int spaceBelow = avail.bottom() - below.y();
            const int spaceAbove = top.y() - avail.top();
            if (!placed) {
                above = height > spaceBelow && spaceAbove > spaceBelow;
                placed = true;
            }
            height = qMin(height, above ? spaceAbove : spaceBelow);
            const int x = qBound(avail.left(), below.x(), qMax(avail.left(), avail.right() - width + 1));
            const int y = above ? top.y() - height : below.y();
            setGeometry(x, y, width, height);
        }

        QPointer<QWidget> anchor;
        Pick pick;
        QLineEdit* search = nullptr;
        QListWidget* list = nullptr;
        RouteEditor::Then current = RouteEditor::Then::Other;
        int currentProfile = -1;
        bool offerBypass = false;
        bool placed = false;
        bool above = false;
    };
} // namespace

RouteThenPicker::RouteThenPicker(QWidget* parent) : QPushButton(parent) {
    setAutoDefault(false);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(this, &QPushButton::clicked, this, &RouteThenPicker::openPopup);
    refresh();
}

void RouteThenPicker::setRule(const std::shared_ptr<Configs::RouteRule>& value) {
    rule = value;
    refresh();
}

void RouteThenPicker::refresh() {
    setEnabled(rule != nullptr);
    if (rule == nullptr) {
        setAccessibleName({});
        setToolTip({});
    } else {
        setAccessibleName(RouteEditor::ThenLabel(*rule));
        const bool profile = RouteEditor::ThenOf(*rule) == RouteEditor::Then::Profile;
        setToolTip(profile ? RouteEditor::ProfileLabel(rule->outboundID) : QString());
    }
    updateGeometry();
    update();
}

QSize RouteThenPicker::contentSize(bool minimum) const {
    QFont pillFont = font();
    pillFont.setWeight(QFont::DemiBold);
    const QFontMetrics pillMetrics(pillFont);
    const QString label = rule != nullptr ? RouteEditor::ThenLabel(*rule) : RouteEditor::ThenLabel(RouteEditor::Then::Proxy);
    QSize pill = RouteEditor::PillSize(pillMetrics, label);
    if (minimum) pill.setWidth(qMin(pill.width(), RouteEditor::PillSize(pillMetrics, QString(10, QLatin1Char('x'))).width()));
    const int arrowWidth = fontMetrics().horizontalAdvance(QChar(0x25BE));
    return {pill.width() + thenPickerArrowGap + arrowWidth, qMax(pill.height(), fontMetrics().height())};
}

QSize RouteThenPicker::sizeHint() const {
    QStyleOptionButton option;
    initStyleOption(&option);
    return style()->sizeFromContents(QStyle::CT_PushButton, &option, contentSize(false), this);
}

QSize RouteThenPicker::minimumSizeHint() const {
    QStyleOptionButton option;
    initStyleOption(&option);
    return style()->sizeFromContents(QStyle::CT_PushButton, &option, contentSize(true), this);
}

void RouteThenPicker::paintEvent(QPaintEvent*) {
    QStylePainter painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    option.text.clear();
    option.icon = QIcon();
    painter.drawControl(QStyle::CE_PushButtonBevel, option);
    if (rule == nullptr) return;

    const QRect contents = style()->subElementRect(QStyle::SE_PushButtonContents, &option, this);
    const QString label = RouteEditor::ThenLabel(*rule);
    QFont pillFont = font();
    pillFont.setWeight(QFont::DemiBold);
    const QSize pill = RouteEditor::PillSize(QFontMetrics(pillFont), label);
    const QString arrow(QChar(0x25BE));
    const int arrowWidth = fontMetrics().horizontalAdvance(arrow);
    const int pillWidth = qMax(0, qMin(pill.width(), contents.width() - thenPickerArrowGap - arrowWidth));
    const QRect pillRect(contents.left(), contents.center().y() - pill.height() / 2, pillWidth, pill.height());
    painter.setFont(pillFont);
    RouteEditor::PaintPill(&painter, pillRect, label, RouteEditor::ToneOf(*rule));
    painter.setFont(font());
    painter.setPen(themeManager()->tokens.muted);
    painter.drawText(QRect(pillRect.right() + 1 + thenPickerArrowGap, contents.top(), arrowWidth, contents.height()), Qt::AlignCenter, arrow);
}

void RouteThenPicker::openPopup() {
    if (rule == nullptr) return;
    auto* popup = new ThenPickerPopup(this, *rule, [guard = QPointer<RouteThenPicker>(this)](RouteEditor::Then then, int profileId) {
        if (guard == nullptr || guard->rule == nullptr) return;
        Configs::RouteRule& target = *guard->rule;
        const RouteEditor::Then was = RouteEditor::ThenOf(target);
        if (then == was && (then != RouteEditor::Then::Profile || profileId == target.outboundID)) return;
        RouteEditor::SetThen(target, then, profileId);
        guard->refresh();
        emit guard->picked();
    });
    popup->popUp();
}
