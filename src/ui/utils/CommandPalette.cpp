#include "include/ui/utils/CommandPalette.h"
#include "include/ui/utils/UiMode.h"

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
#include <QScrollBar>
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
    setWindowTitle(tr("Search Settings"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    edit = new QLineEdit(this);
    edit->setFrame(false);
    edit->setTextMargins(8, 8, 8, 8);
    edit->setFont(scaledFont(edit->font(), 1.15));
    edit->setPlaceholderText(tr("Search settings"));
    edit->setAccessibleName(tr("Search settings"));
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
    // Scrolling follows only the keyboard (moveSelection); a refresh that moves the selection must not move the view
    // under a pointer that is about to click.
    list->setAutoScroll(false);
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

    connect(edit, &QLineEdit::textChanged, this, [this] { refilter({}); });
    connect(list, &QListView::doubleClicked, this, &CommandPalette::activate);

    connect(list->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] {
        if (!restoring) preferred = currentSelection();
    });

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
    for (const auto &entry : settings) {
        if (entry.action) connect(entry.action, &QAction::changed, this, &CommandPalette::queueRefilter, Qt::UniqueConnection);
    }
    entries.append(std::move(settings));
}

void CommandPalette::present(BlockerSource source) {
    blockerSource = std::move(source);
    refreshCommands();
    refreshBlocked();
    {
        const QSignalBlocker block(edit);
        edit->clear();
    }
    refilter({});
    stale = false;
    show();
    raise();
    activateWindow();
    edit->setFocus(Qt::OtherFocusReason);
}

CommandPalette::Blocker CommandPalette::currentBlocker() const {
    return blockerSource ? blockerSource() : Blocker();
}

void CommandPalette::refreshBlocked() {
    const Blocker blocker = currentBlocker();
    for (auto &entry : entries) entry.blocked = blocker ? blocker(entry.action) : QString();
}

CommandPalette::Selection CommandPalette::keptSelection() const {
    // The entry picked last comes back once it is runnable again; until then a neighbour of the current row stands in.
    Selection keep = preferred;
    keep.row = list->currentIndex().row();
    return keep;
}

void CommandPalette::refreshState() {
    // Something may have changed while another window had the focus: re-read it all, keeping the query and the selection.
    const bool collected = refreshCommands();
    refreshBlocked();
    refilter(keptSelection(), collected);
}

void CommandPalette::blockersChanged() {
    queueRefilter();
}

void CommandPalette::queueRefilter() {
    // A caller flips its state before the dialog it opens exists, and actions change in bursts; read it all once they
    // have returned. Menus' aboutToShow is left alone, so their own state updates cannot loop back here.
    if (refreshQueued || !isVisible()) return;
    refreshQueued = true;
    QTimer::singleShot(0, this, [this] {
        refreshQueued = false;
        if (!isVisible()) return;
        refreshBlocked();
        refilter(keptSelection());
    });
}

bool CommandPalette::refreshCommands() {
    // Each menu's own aboutToShow handler brings its actions' enabled and checked state up to date.
    QSet<QMenu *> refreshed;
    if (!commandsDirty) {
        for (const auto &menu : std::as_const(collectedMenus)) {
            if (!menu) continue;
            emit menu->aboutToShow();
            refreshed.insert(menu);
        }
    }
    if (!commandsDirty) return false;

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
    return true;
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
        // Toggled or disabled from a menu or the tray: an open palette shows it at once.
        connect(action, &QAction::changed, this, &CommandPalette::queueRefilter, Qt::UniqueConnection);
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

QList<CommandPalette::Row> CommandPalette::matchRows() const {
    QList<Row> out;
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
    const bool simple = UiMode::Simple();
    const auto parked = [simple](const Entry &entry) { return simple && entry.advanced; };
    const auto wanted = [&](const Entry &entry) {
        // A parked setting's dialog opener may be one of the actions Simple mode hides.
        if (!parked(entry) && !effective(entry, &QAction::isVisible)) return false;
        if (commandsOnly) return entry.kind == Entry::Command;
        if (settingsOnly) return entry.kind == Entry::Setting;
        return true;
    };
    const auto header = [&out](const QString &title) { out.append({-1, title}); };

    if (terms.isEmpty()) {
        // Browsing: commands grouped by the menu they live in; settings only on request, there are too many.
        QString section;
        for (int i = 0; i < entries.size(); ++i) {
            const auto &entry = entries[i];
            if (!wanted(entry) || parked(entry) || (entry.kind == Entry::Setting && !settingsOnly)) continue;
            if (entry.section != section) {
                section = entry.section;
                header(section);
            }
            out.append(Row{i});
        }
        return out;
    }

    struct Match {
        int index;
        int score;
        int start;
    };
    QList<Match> commands;
    QList<Match> settings;
    QList<Match> advanced;
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
        (entry.kind == Entry::Command ? commands : parked(entry) ? advanced : settings).append({i, score, start});
    }
    const auto byScore = [](const Match &a, const Match &b) { return a.score < b.score; };
    std::stable_sort(commands.begin(), commands.end(), byScore);
    std::stable_sort(settings.begin(), settings.end(), byScore);
    std::stable_sort(advanced.begin(), advanced.end(), byScore);

    const auto matched = [&](const Match &match) {
        out.append({match.index, {}, qMax(match.start, 0), match.start >= 0 ? int(first.size()) : 0});
    };
    if (!commands.isEmpty()) header(tr("Commands"));
    for (const auto &match : commands) matched(match);
    if (!settings.isEmpty()) header(tr("Settings"));
    for (const auto &match : settings) matched(match);
    if (!advanced.isEmpty()) header(tr("Settings in Advanced mode"));
    for (const auto &match : advanced) matched(match);
    if (commands.isEmpty() && settings.isEmpty() && advanced.isEmpty()) header(tr("Nothing found"));
    return out;
}

void CommandPalette::refilter(const Selection &keep, bool rebuild) {
    QList<Row> next = matchRows();
    restoring = true;
    if (!rebuild && !keep.label.isEmpty() && next == rows) {
        // Same rows: update them in place, so a click or double-click in progress keeps its item.
        for (int i = 0; i < rows.size(); ++i) {
            if (rows[i].entry >= 0) fillEntry(model.item(i), rows[i]);
        }
    } else {
        const int scroll = list->verticalScrollBar()->value();
        rows = std::move(next);
        model.clear();
        for (const auto &row : std::as_const(rows)) {
            auto *item = new QStandardItem;
            if (row.entry >= 0) {
                fillEntry(item, row);
            } else {
                item->setText(row.header);
                item->setFlags(Qt::NoItemFlags);
                item->setData(true, HeaderRole);
            }
            model.appendRow(item);
        }
        if (keep.label.isEmpty()) {
            list->scrollToTop();
        } else {
            // The scroll range follows the new rows only after a layout pass.
            list->doItemsLayout();
            list->verticalScrollBar()->setValue(scroll);
        }
    }
    select(keep);
    restoring = false;
    // A new query starts over from its first match.
    if (keep.label.isEmpty()) preferred = currentSelection();
}

void CommandPalette::fillEntry(QStandardItem *item, const Row &row) {
    const auto &entry = entries[row.entry];
    const QAction *action = entry.action;
    item->setText(entry.label);
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
    item->setData(row.entry, EntryRole);
    item->setData(row.matchStart, MatchStartRole);
    item->setData(row.matchLength, MatchLengthRole);
    item->setToolTip(tip);
}

CommandPalette::Selection CommandPalette::currentSelection() const {
    const QModelIndex current = list->currentIndex();
    if (!current.isValid()) return {};
    const auto &entry = entries[current.data(EntryRole).toInt()];
    return {entry.kind, entry.action, entry.label, entry.path, current.row()};
}

void CommandPalette::select(const Selection &keep) {
    int first = -1;
    int match = -1;
    int before = -1;
    int after = -1;
    for (int row = 0; row < model.rowCount(); ++row) {
        const auto *item = model.item(row);
        if (!item->isSelectable()) continue;
        if (first < 0) first = row;
        if (keep.label.isEmpty()) break;
        if (row < keep.row) before = row;
        else if (after < 0) after = row;
        // Several settings share their dialog's action, so the text tells them apart; a menu that recreated
        // its actions leaves only the text to go by.
        const auto &entry = entries[item->data(EntryRole).toInt()];
        if (entry.kind != keep.kind || entry.label != keep.label || entry.path != keep.path) continue;
        if (match < 0 || entry.action == keep.action) match = row;
        if (entry.action == keep.action) break;
    }
    // Without it, the nearest runnable row keeps the view where it was.
    int row = match >= 0 ? match : (after >= 0 ? after : before);
    if (row < 0) row = first;
    if (row < 0) return;
    // The view stays where it is, even if the user scrolled away from the selection.
    list->setCurrentIndex(model.index(row, 0));
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
    // The list may predate a window that opened since; show the reason instead of running into a busy dialog slot.
    const Blocker blocker = currentBlocker();
    if (blocker && !blocker(entries[entryIndex].action).isEmpty()) {
        refreshState();
        return;
    }
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

void CommandPalette::changeEvent(QEvent *event) {
    QDialog::changeEvent(event);
    if (event->type() != QEvent::ActivationChange || !isVisible()) return;
    // Opening activates the palette right after present() filled it, so only a return from another window refreshes.
    if (!isActiveWindow()) {
        stale = true;
    } else if (stale) {
        stale = false;
        refreshState();
    }
}
