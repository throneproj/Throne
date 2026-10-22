#include "include/ui/utils/CommandPalette.h"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

namespace {
    enum Role {
        PathRole = Qt::UserRole + 1,
        ShortcutRole,
        StateRole,
        HeaderRole,
        EntryRole,
        MatchStartRole,
        MatchLengthRole,
    };

    constexpr int kPageStep = 10;
    constexpr int kSidePadding = 12;

    QFont scaledFont(QFont font, qreal factor) {
        if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * factor);
        else if (font.pixelSize() > 0) font.setPixelSize(qRound(font.pixelSize() * factor));
        return font;
    }

    QColor mutedColor(const QPalette &palette, bool selected) {
        QColor color = palette.color(QPalette::Active, selected ? QPalette::HighlightedText : QPalette::Text);
        color.setAlphaF(selected ? 0.75 : 0.6);
        return color;
    }

    // True when the entry's action and every submenu it sits in have the state.
    bool effective(const CommandPalette::Entry &entry, bool (QAction::*state)() const) {
        if (!entry.action || !(entry.action.data()->*state)()) return false;
        return std::all_of(entry.submenus.cbegin(), entry.submenus.cend(),
                           [state](const QPointer<QAction> &menu) { return menu && (menu.data()->*state)(); });
    }

    // Leaves the window's ShortcutOverride for the action unaccepted, so no focused widget can claim the key.
    class ShortcutGuard : public QObject {
    public:
        ShortcutGuard(QAction *action, QWidget *window) : QObject(action), action(action), window(window) {}

        bool eventFilter(QObject *obj, QEvent *event) override {
            if (event->type() != QEvent::ShortcutOverride || !obj->isWidgetType() || !window) return false;
            if (static_cast<QWidget *>(obj)->window() != window || action->shortcut().isEmpty()) return false;
            const auto *key = static_cast<QKeyEvent *>(event);
            return QKeySequence(key->keyCombination()) == action->shortcut();
        }

    private:
        QAction *action;
        QPointer<QWidget> window;
    };

    class PaletteDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            const int lineHeight = option.fontMetrics.height();
            return {0, index.data(HeaderRole).toBool() ? lineHeight + 12 : lineHeight + 14};
        }

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);
            painter->save();
            const QRect area = opt.rect.adjusted(kSidePadding, 0, -kSidePadding, 0);

            if (index.data(HeaderRole).toBool()) {
                QFont font = scaledFont(opt.font, 0.9);
                font.setBold(true);
                painter->setFont(font);
                painter->setPen(mutedColor(opt.palette, false));
                painter->drawText(area.adjusted(0, 4, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, index.data().toString());
                painter->restore();
                return;
            }

            const bool selected = opt.state & QStyle::State_Selected;
            const bool enabled = index.flags() & Qt::ItemIsEnabled;
            const QColor accent = opt.palette.color(QPalette::Active, QPalette::Highlight);
            const QColor onAccent = opt.palette.color(QPalette::Active, QPalette::HighlightedText);
            painter->setRenderHint(QPainter::Antialiasing);

            // Painted here rather than by the style: some styles draw nothing for a list that never takes focus.
            if (selected) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(accent);
                painter->drawRoundedRect(QRectF(opt.rect.adjusted(4, 1, -4, -1)), 4, 4);
            }

            QColor text = selected ? onAccent : opt.palette.color(QPalette::Active, QPalette::Text);
            QColor muted = mutedColor(opt.palette, selected);
            if (!enabled) {
                text.setAlphaF(0.45);
                muted.setAlphaF(0.35);
            }

            int right = area.right();
            const QFont small = scaledFont(opt.font, 0.85);
            const QFontMetrics smallMetrics(small);

            // Right side: a switch for a checkable command, then the shortcut as key caps.
            const QVariant state = index.data(StateRole);
            if (state.isValid()) {
                const bool on = state.toBool();
                const qreal h = smallMetrics.height();
                const QRectF track(right - 2 * h + 1, area.center().y() - h / 2, 2 * h, h);
                QColor trackColor = on ? (selected ? onAccent : accent) : muted;
                if (!on) trackColor.setAlphaF(trackColor.alphaF() * 0.6);
                painter->setPen(Qt::NoPen);
                painter->setBrush(trackColor);
                painter->drawRoundedRect(track, h / 2, h / 2);
                const qreal knob = h - 4;
                painter->setBrush(on ? (selected ? accent : onAccent) : text);
                painter->drawEllipse(QRectF(on ? track.right() - 2 - knob : track.left() + 2, track.top() + 2, knob, knob));
                right = int(track.left()) - 12;
            }
            const QString shortcut = index.data(ShortcutRole).toString();
            if (!shortcut.isEmpty()) {
                QStringList keys = shortcut.split('+', Qt::SkipEmptyParts);
                if (shortcut.endsWith("++")) keys.append("+");
                painter->setFont(small);
                for (int i = int(keys.size()) - 1; i >= 0; --i) {
                    const int w = smallMetrics.horizontalAdvance(keys[i]) + 10;
                    const int h = smallMetrics.height() + 4;
                    const QRect cap(right - w + 1, area.center().y() - h / 2, w, h);
                    painter->setPen(muted);
                    painter->setBrush(Qt::NoBrush);
                    painter->drawRoundedRect(QRectF(cap).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
                    painter->setPen(text);
                    painter->drawText(cap, Qt::AlignCenter, keys[i]);
                    right = cap.left() - 4;
                }
                right -= 8;
            }

            // Label, with the matched part in bold.
            painter->setFont(opt.font);
            const QFontMetrics metrics(opt.font);
            QFont bold = opt.font;
            bold.setBold(true);
            const QFontMetrics boldMetrics(bold);

            const QString label = index.data().toString();
            const int start = index.data(MatchStartRole).toInt();
            const int length = index.data(MatchLengthRole).toInt();
            const QString pre = label.left(start);
            const QString hit = length > 0 ? label.mid(start, length) : QString();
            const QString post = label.mid(start + length);
            int x = area.left();
            const int available = right - x;
            const int fullWidth = metrics.horizontalAdvance(pre) + boldMetrics.horizontalAdvance(hit) + metrics.horizontalAdvance(post);
            painter->setPen(text);
            if (length <= 0 || fullWidth > available) {
                const QString elided = metrics.elidedText(label, Qt::ElideRight, available);
                painter->drawText(QRect(x, area.top(), available, area.height()), Qt::AlignLeft | Qt::AlignVCenter, elided);
                x += metrics.horizontalAdvance(elided);
            } else {
                const auto drawPart = [&](const QString &part, const QFont &font, const QFontMetrics &fm) {
                    painter->setFont(font);
                    const int w = fm.horizontalAdvance(part);
                    painter->drawText(QRect(x, area.top(), w, area.height()), Qt::AlignLeft | Qt::AlignVCenter, part);
                    x += w;
                };
                drawPart(pre, opt.font, metrics);
                drawPart(hit, bold, boldMetrics);
                drawPart(post, opt.font, metrics);
                painter->setFont(opt.font);
            }

            const QString path = index.data(PathRole).toString();
            x += 12;
            if (!path.isEmpty() && right - x > 40) {
                painter->setPen(muted);
                painter->drawText(QRect(x, area.top(), right - x, area.height()), Qt::AlignLeft | Qt::AlignVCenter,
                                  metrics.elidedText(path, Qt::ElideMiddle, right - x));
            }
            painter->restore();
        }
    };
}

CommandPalette::CommandPalette(QWidget *parent, QList<QPair<QString, QMenu *>> menus, const QAction *skip)
    : QDialog(parent), roots(std::move(menus)), skipAction(skip) {
    setWindowTitle(tr("Search Commands and Settings"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    edit = new QLineEdit(this);
    edit->setFrame(false);
    edit->setTextMargins(8, 8, 8, 8);
    edit->setFont(scaledFont(edit->font(), 1.15));
    edit->setPlaceholderText(tr("Search commands and settings"));
    edit->setAccessibleName(tr("Search commands and settings"));
    edit->installEventFilter(this);
    layout->addWidget(edit);

    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    list = new QListView(this);
    list->setFrameShape(QFrame::NoFrame);
    list->setModel(&model);
    list->setItemDelegate(new PaletteDelegate(list));
    list->setFocusPolicy(Qt::NoFocus);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(list, 1);

    hint = new QLabel(tr("↑↓ move · Enter run · Esc close · > commands only · @ settings only"), this);
    hint->setContentsMargins(10, 5, 10, 6);
    hint->setFont(scaledFont(hint->font(), 0.9));
    QPalette hintPalette = hint->palette();
    QColor hintColor = hintPalette.color(QPalette::WindowText);
    hintColor.setAlphaF(0.65);
    hintPalette.setColor(QPalette::WindowText, hintColor);
    hint->setPalette(hintPalette);
    layout->addWidget(hint);

    connect(edit, &QLineEdit::textChanged, this, &CommandPalette::refilter);
    connect(list, &QListView::doubleClicked, this, &CommandPalette::activate);

    const int lineHeight = fontMetrics().height();
    resize(lineHeight * 36, lineHeight * 28);
}

QString CommandPalette::plainText(QString text) {
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        if (text[i] == '&') {
            if (i + 1 < text.size() && text[i + 1] == '&') {
                out += '&';
                ++i;
            }
            continue;
        }
        out += text[i];
    }
    out = out.trimmed();
    if (out.endsWith("...")) out.chop(3);
    else if (out.endsWith(QChar(0x2026))) out.chop(1);
    else if (out.endsWith(':')) out.chop(1);
    return out.trimmed();
}

QString CommandPalette::keywordsFor(const QString &objectName) {
    static const QRegularExpression generated("^(tab|widget|label|groupBox|checkBox|radioButton|pushButton)(_\\d+)?$");
    static const QRegularExpression prefix("^(action|menu)_?");
    if (generated.match(objectName).hasMatch()) return {};
    QString words = objectName;
    words.remove(prefix);
    words.replace('_', ' ');
    return words.trimmed();
}

void CommandPalette::keepShortcutFromTextFields(QAction *action, QWidget *window) {
    qApp->installEventFilter(new ShortcutGuard(action, window));
}

void CommandPalette::setSettings(QList<Entry> settings) {
    entries.resize(commandCount);
    entries.append(std::move(settings));
}

void CommandPalette::present(const Blocker &blocker) {
    refreshCommands();
    for (auto &entry : entries) entry.blocked = blocker ? blocker(entry.action) : QString();
    {
        const QSignalBlocker block(edit);
        edit->clear();
    }
    refilter();
    show();
    raise();
    activateWindow();
    edit->setFocus(Qt::OtherFocusReason);
}

void CommandPalette::refreshCommands() {
    // Each menu's own aboutToShow handler brings its actions' enabled and checked state up to date.
    QSet<QMenu *> refreshed;
    if (!commandsDirty) {
        for (const auto &menu : std::as_const(collectedMenus)) {
            if (!menu) continue;
            emit menu->aboutToShow();
            refreshed.insert(menu);
        }
    }
    if (!commandsDirty) return;

    collecting = true;
    for (const auto &menu : std::as_const(collectedMenus)) {
        if (menu) menu->removeEventFilter(this);
    }
    collectedMenus.clear();
    QList<Entry> commands;
    QSet<const QAction *> seen;
    for (const auto &[title, menu] : std::as_const(roots)) {
        const QString section = plainText(title);
        collect(menu, section, section, {}, refreshed, seen, commands);
    }
    collecting = false;
    commandsDirty = false;

    const qsizetype count = commands.size();
    commands.append(entries.mid(commandCount));
    entries = std::move(commands);
    commandCount = count;
}

void CommandPalette::collect(QMenu *menu, const QString &section, const QString &path, const QList<QPointer<QAction>> &submenus,
                             const QSet<QMenu *> &refreshed, QSet<const QAction *> &seen, QList<Entry> &out) {
    if (collectedMenus.contains(menu)) return;
    collectedMenus.append(menu);
    menu->installEventFilter(this);
    // Menus that fill their actions on demand.
    if (!refreshed.contains(menu)) emit menu->aboutToShow();
    for (QAction *action : menu->actions()) {
        if (action->isSeparator() || action == skipAction) continue;
        if (auto *sub = action->menu()) {
            collect(sub, section, path + " › " + plainText(sub->title()), submenus + QList<QPointer<QAction>>{action}, refreshed, seen, out);
            continue;
        }
        const QString label = plainText(action->text());
        if (label.isEmpty() || seen.contains(action)) continue;
        seen.insert(action);
        connect(action, &QObject::destroyed, this, &CommandPalette::markCommandsDirty, Qt::UniqueConnection);

        Entry entry;
        entry.kind = Entry::Command;
        entry.label = label;
        entry.path = path;
        entry.section = section;
        entry.keywords = keywordsFor(action->objectName());
        entry.action = action;
        entry.submenus = submenus;
        entry.run = [guard = entry.action] {
            if (guard && guard->isEnabled()) guard->trigger();
        };
        out.append(entry);
    }
}

void CommandPalette::markCommandsDirty() {
    commandsDirty = true;
}

void CommandPalette::refilter() {
    model.clear();

    QString text = edit->text().trimmed();
    bool commandsOnly = false;
    bool settingsOnly = false;
    if (text.startsWith('>')) {
        commandsOnly = true;
        text = text.mid(1).trimmed();
    } else if (text.startsWith('@')) {
        settingsOnly = true;
        text = text.mid(1).trimmed();
    }
    const QStringList terms = text.toLower().split(' ', Qt::SkipEmptyParts);
    const auto wanted = [&](const Entry &entry) {
        if (!effective(entry, &QAction::isVisible)) return false;
        if (commandsOnly) return entry.kind == Entry::Command;
        if (settingsOnly) return entry.kind == Entry::Setting;
        return true;
    };

    if (terms.isEmpty()) {
        // Browsing: commands grouped by the menu they live in; settings only on request, there are too many.
        QString section;
        for (int i = 0; i < entries.size(); ++i) {
            const auto &entry = entries[i];
            if (!wanted(entry) || (entry.kind == Entry::Setting && !settingsOnly)) continue;
            if (entry.section != section) {
                section = entry.section;
                addHeader(section);
            }
            addEntry(i, 0, 0);
        }
    } else {
        struct Match {
            int index;
            int score;
            int start;
        };
        QList<Match> commands;
        QList<Match> settings;
        const QString &first = terms.first();
        for (int i = 0; i < entries.size(); ++i) {
            const auto &entry = entries[i];
            if (!wanted(entry)) continue;
            const QString label = entry.label.toLower();
            const QString haystack = label + ' ' + entry.path.toLower() + ' ' + entry.keywords.toLower();
            if (!std::all_of(terms.begin(), terms.end(), [&](const QString &term) { return haystack.contains(term); })) continue;

            const int start = label.indexOf(first);
            int score = 4;
            if (start == 0) score = 0;
            else if (start > 0 && !label[start - 1].isLetterOrNumber()) score = 1;
            else if (start > 0) score = 2;
            else if (entry.path.toLower().contains(first)) score = 3;
            (entry.kind == Entry::Command ? commands : settings).append({i, score, start});
        }
        const auto byScore = [](const Match &a, const Match &b) { return a.score < b.score; };
        std::stable_sort(commands.begin(), commands.end(), byScore);
        std::stable_sort(settings.begin(), settings.end(), byScore);

        if (!commands.isEmpty()) addHeader(tr("Commands"));
        for (const auto &match : commands) addEntry(match.index, match.start, match.start >= 0 ? int(first.size()) : 0);
        if (!settings.isEmpty()) addHeader(tr("Settings"));
        for (const auto &match : settings) addEntry(match.index, match.start, match.start >= 0 ? int(first.size()) : 0);
        if (commands.isEmpty() && settings.isEmpty()) addHeader(tr("Nothing found"));
    }

    selectFirst();
}

void CommandPalette::addHeader(const QString &title) {
    auto *item = new QStandardItem(title);
    item->setFlags(Qt::NoItemFlags);
    item->setData(true, HeaderRole);
    model.appendRow(item);
}

void CommandPalette::addEntry(int entryIndex, int matchStart, int matchLength) {
    const auto &entry = entries[entryIndex];
    const QAction *action = entry.action;
    auto *item = new QStandardItem(entry.label);
    const bool enabled = entry.blocked.isEmpty() && effective(entry, &QAction::isEnabled);
    item->setFlags(enabled ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags);
    const QString path = entry.blocked.isEmpty() ? entry.path : entry.path + " · " + entry.blocked;
    item->setData(path, PathRole);
    QString tip = entry.label;
    if (entry.kind == Entry::Command) {
        item->setData(action->shortcut().toString(QKeySequence::NativeText), ShortcutRole);
        if (action->isCheckable()) {
            item->setData(action->isChecked(), StateRole);
            tip += action->isChecked() ? tr(" (on)") : tr(" (off)");
        }
    }
    if (!path.isEmpty()) tip += "\n" + path;
    item->setData(entryIndex, EntryRole);
    item->setData(qMax(matchStart, 0), MatchStartRole);
    item->setData(matchStart >= 0 ? matchLength : 0, MatchLengthRole);
    item->setToolTip(tip);
    model.appendRow(item);
}

void CommandPalette::selectFirst() {
    list->scrollToTop();
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.item(row)->isSelectable()) {
            list->setCurrentIndex(model.index(row, 0));
            return;
        }
    }
}

void CommandPalette::moveSelection(int delta) {
    const int rows = model.rowCount();
    const int current = list->currentIndex().isValid() ? list->currentIndex().row() : -1;
    const int step = delta > 0 ? 1 : -1;
    int remaining = qAbs(delta);
    int target = current;
    for (int row = current + step; row >= 0 && row < rows && remaining > 0; row += step) {
        if (!model.item(row)->isSelectable()) continue;
        target = row;
        --remaining;
    }
    if (target < 0 || target == current) return;
    list->setCurrentIndex(model.index(target, 0));
    // Keep a section header in view when stepping up to its first entry.
    if (step < 0 && target > 0 && !model.item(target - 1)->isSelectable()) list->scrollTo(model.index(target - 1, 0));
    list->scrollTo(model.index(target, 0));
}

void CommandPalette::activate(const QModelIndex &index) {
    if (!index.isValid() || !(index.flags() & Qt::ItemIsSelectable)) return;
    const int entryIndex = index.data(EntryRole).toInt();
    if (entryIndex < 0 || entryIndex >= entries.size()) return;
    const auto run = entries[entryIndex].run;
    hide();
    // After the palette is gone, so whatever the entry opens gets the focus.
    if (run) QTimer::singleShot(0, parentWidget(), run);
}

bool CommandPalette::eventFilter(QObject *obj, QEvent *event) {
    if ((event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved) && qobject_cast<QMenu *>(obj)) {
        if (!collecting) commandsDirty = true;
        return false;
    }
    if (obj == edit && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        switch (key->key()) {
        case Qt::Key_Down:
        case Qt::Key_Tab:
            moveSelection(1);
            return true;
        case Qt::Key_Up:
        case Qt::Key_Backtab:
            moveSelection(-1);
            return true;
        case Qt::Key_PageDown:
            moveSelection(kPageStep);
            return true;
        case Qt::Key_PageUp:
            moveSelection(-kPageStep);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            activate(list->currentIndex());
            return true;
        case Qt::Key_Escape:
            reject();
            return true;
        default:
            break;
        }
    }
    return QDialog::eventFilter(obj, event);
}
