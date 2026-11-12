#include "include/ui/utils/ValidationForm.h"

#include "include/ui/setting/ThemeManager.hpp"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHelpEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QTimer>
#include <QToolTip>

#include <algorithm>
#include <climits>
#include <list>
#include <utility>

namespace Validation {
    // Outside the anonymous namespace: Form::Private's members use them, which GCC flags in unity builds.
    enum class FormKind { Line, Plain, Lines, Combo };

    enum class FormMark { None, Warning, Error };

    namespace {
        constexpr int formLineMarkProperty = QTextFormat::UserProperty + 0x5646;
        constexpr int formSummaryLines = 8;

        // Paints its text elided so a long message never widens the button row.
        class ValidationStatusLabel final : public QLabel {
        public:
            explicit ValidationStatusLabel(QWidget *parent) : QLabel(parent) {
                setTextFormat(Qt::PlainText);
                setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
                setProperty("colorRole", QStringLiteral("danger"));
            }

        protected:
            void paintEvent(QPaintEvent *) override {
                if (text().isEmpty()) return;
                QPainter painter(this);
                const QRect area = contentsRect();
                const QString shown = fontMetrics().elidedText(text(), Qt::ElideRight, area.width());
                style()->drawItemText(&painter, area, QStyle::visualAlignment(layoutDirection(), alignment()), palette(),
                                      isEnabled(), shown, foregroundRole());
            }

            // Not setToolTip(): ADD_ASTERISK appends '*' to every label that carries a tooltip.
            bool event(QEvent *event) override {
                if (event->type() == QEvent::ToolTip) {
                    if (text().isEmpty()) QToolTip::hideText();
                    else QToolTip::showText(static_cast<QHelpEvent *>(event)->globalPos(), text(), this);
                    return true;
                }
                return QLabel::event(event);
            }
        };

        QString formCleanLabel(const QString &raw) {
            const QString text = Qt::mightBeRichText(raw) ? QTextDocumentFragment::fromHtml(raw).toPlainText() : raw;
            QString out;
            out.reserve(text.size());
            for (qsizetype i = 0; i < text.size(); ++i) {
                if (text.at(i) != QLatin1Char('&')) {
                    out += text.at(i);
                } else if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('&')) {
                    out += QLatin1Char('&');
                    ++i;
                }
            }
            out = out.simplified();
            while (out.endsWith(QLatin1Char('*')) || out.endsWith(QLatin1Char(':')) || out.endsWith(QChar(0xFF1A))) {
                out.chop(1);
                out = out.trimmed();
            }
            return out;
        }

        QString formIssueText(const QString &label, const QString &message) {
            if (label.isEmpty()) return message;
            return QCoreApplication::translate("Validation", "%1: %2").arg(label, message);
        }

        QString formElide(const QString &text, qsizetype max) {
            if (text.size() <= max) return text;
            qsizetype cut = max - 1;
            if (cut > 0 && text.at(cut - 1).isHighSurrogate()) --cut;
            return text.left(cut) + QChar(0x2026);
        }

        // (layout, item index) pairs from `layout` down to the item holding `widget`.
        bool formLayoutPath(QLayout *layout, const QWidget *widget, QList<QPair<QLayout *, int>> &path) {
            for (int i = 0; i < layout->count(); ++i) {
                QLayoutItem *item = layout->itemAt(i);
                if (item == nullptr) continue;
                if (item->widget() == widget) {
                    path.append({layout, i});
                    return true;
                }
                if (QLayout *sub = item->layout()) {
                    path.append({layout, i});
                    if (formLayoutPath(sub, widget, path)) return true;
                    path.removeLast();
                }
            }
            return false;
        }

        QWidget *formTextTarget(QWidget *widget) {
            if (auto *combo = qobject_cast<QComboBox *>(widget); combo && combo->lineEdit()) return combo->lineEdit();
            return widget;
        }

        bool formIsTextWidget(QObject *object) {
            return qobject_cast<QLineEdit *>(object) || qobject_cast<QPlainTextEdit *>(object) ||
                   qobject_cast<QTextEdit *>(object) || qobject_cast<QComboBox *>(object) ||
                   qobject_cast<QAbstractSpinBox *>(object);
        }

        void formRestyle(QWidget *widget) {
            // Polishing under a style sheet re-resolves the font and drops a widget's own setFont().
            const bool ownFont = widget->testAttribute(Qt::WA_SetFont);
            const QFont font = widget->font();
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
            if (ownFont) widget->setFont(font);
            widget->update();
        }

        void formRepolish(QWidget *widget, FormMark mark) {
            widget->setProperty("invalid", mark == FormMark::Error     ? QVariant(QStringLiteral("true"))
                                           : mark == FormMark::Warning ? QVariant(QStringLiteral("warning"))
                                                                       : QVariant());
            formRestyle(widget);
        }

        // Cached: every bound line edit takes one, and an uncached load costs about 9 ms.
        QIcon formMarkIcon(FormMark mark) {
            return themeManager()->standardIcon(mark == FormMark::Error ? QStyle::SP_MessageBoxCritical : QStyle::SP_MessageBoxWarning);
        }

        QColor formMarkColor(FormMark mark) {
            const QColor color = mark == FormMark::Error ? themeManager()->tokens.danger : themeManager()->tokens.muted;
            if (color.isValid()) return color;
            return mark == FormMark::Error ? QColor(Qt::red) : QColor(Qt::darkGray);
        }

        // Extra selections stay off the undo stack and leave document()->isModified() alone.
        void formSetLineMarks(QPlainTextEdit *edit, const QList<int> &blocks, const QColor &color) {
            auto selections = edit->extraSelections();
            selections.erase(std::remove_if(selections.begin(), selections.end(),
                                            [](const QTextEdit::ExtraSelection &s) { return s.format.boolProperty(formLineMarkProperty); }),
                             selections.end());
            if (!blocks.isEmpty()) {
                QTextCharFormat format;
                format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
                format.setUnderlineColor(color);
                format.setProperty(formLineMarkProperty, true);
                for (const int number: blocks) {
                    const QTextBlock block = edit->document()->findBlockByNumber(number);
                    if (!block.isValid()) continue;
                    QTextEdit::ExtraSelection selection;
                    selection.cursor = QTextCursor(block);
                    selection.cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
                    selection.format = format;
                    selections.append(selection);
                }
            }
            edit->setExtraSelections(selections);
        }

        QAction *formAddMarkAction(QLineEdit *edit, QObject *context) {
            auto *action = edit->addAction(formMarkIcon(FormMark::Error), QLineEdit::TrailingPosition);
            action->setVisible(false);
            QObject::connect(action, &QAction::triggered, context, [action, edit] {
                QWidget *anchor = edit;
                for (QObject *object: action->associatedObjects()) {
                    if (auto *button = qobject_cast<QWidget *>(object); button && button != edit && button->isVisible()) {
                        anchor = button;
                        break;
                    }
                }
                QToolTip::showText(anchor->mapToGlobal(anchor->rect().bottomLeft()), action->toolTip(), anchor);
            });
            return action;
        }

        bool formHasAncestor(const QWidget *widget, const QObject *ancestor) {
            for (const QWidget *p = widget->parentWidget(); p != nullptr; p = p->parentWidget()) {
                if (p == ancestor) return true;
            }
            return false;
        }
    } // namespace

    Form::Field &Form::Field::label(const QString &text) {
        labelText = text;
        return *this;
    }

    Form::Field &Form::Field::when(std::function<bool()> predicate) {
        active = std::move(predicate);
        return *this;
    }

    Form::Field &Form::Field::warning() {
        severity = Severity::Warning;
        return *this;
    }

    Form::Field &Form::Field::raw() {
        trim = false;
        return *this;
    }

    struct Form::Private {
        struct CrossRule;

        struct Message {
            QString text;
            QString label;
            const CrossRule *rule = nullptr;
            Severity severity = Severity::Error;
        };

        struct State {
            QPointer<QWidget> widget;
            QPointer<QAction> action;
            QMap<const void *, Message> messages;
            QList<int> badBlocks;
            QString baseToolTip;
            FormMark mark = FormMark::None;
            bool toolTipSaved = false;
            bool lineMarks = false;
        };

        struct Entry {
            Field field;
            FormKind kind = FormKind::Line;
            const QObject *key = nullptr;
            QPointer<QWidget> widget;
            const QObject *owner = nullptr;
            Rule rule;
        };

        struct CrossRule {
            QList<const QObject *> keys;
            QList<QPointer<QWidget>> participants;
            std::function<QString()> check;
            const QObject *owner = nullptr;
            Severity severity = Severity::Error;
            QString label;
        };

        struct Source {
            std::function<QList<FormIssue>()> collect;
            const QObject *owner = nullptr;
        };

        Form *q = nullptr;
        QWidget *scope = nullptr;
        std::list<Entry> entries;
        std::list<CrossRule> rules;
        std::list<Source> sources;
        QHash<const QObject *, State> states;
        QSet<const QObject *> watched;
        QSet<const QObject *> pending;
        bool flushQueued = false;
        bool refreshQueued = false;
        int pauses = 0;
        QPointer<QLabel> status;
        const QObject *statusKey = nullptr;
        Field detached;

        bool liveWindow() const { return scope->window()->isVisible(); }

        // isVisibleTo() treats every non-current tab page as hidden; a page still in its stack can be switched to,
        // unless its tab itself is hidden.
        bool revealable(QWidget *widget) const {
            for (QWidget *p = widget; p != nullptr && p != scope; p = p->parentWidget()) {
                if (!p->isHidden()) continue;
                auto *stack = qobject_cast<QStackedWidget *>(p->parentWidget());
                if (stack == nullptr || stack->indexOf(p) < 0) return false;
                auto *tabs = qobject_cast<QTabWidget *>(stack->parentWidget());
                if (tabs != nullptr && !tabs->isTabVisible(tabs->indexOf(p))) return false;
            }
            return true;
        }

        bool usable(QWidget *widget) const { return revealable(widget) && widget->isEnabledTo(scope); }

        static bool whenHolds(const Entry &e) { return !e.field.active || e.field.active(); }

        bool entryActive(const Entry &e) const { return e.widget && usable(e.widget) && whenHolds(e); }

        bool participantSkipped(QWidget *widget) const {
            if (!usable(widget)) return true;
            return std::any_of(entries.begin(), entries.end(),
                               [widget](const Entry &e) { return e.key == widget && !whenHolds(e); });
        }

        static FormMark markOf(const State &state) {
            if (state.messages.isEmpty()) return FormMark::None;
            const bool error = std::any_of(state.messages.cbegin(), state.messages.cend(),
                                           [](const Message &m) { return m.severity == Severity::Error; });
            return error ? FormMark::Error : FormMark::Warning;
        }

        void watch(QObject *object) {
            if (object == nullptr || watched.contains(object)) return;
            watched.insert(object);
            QObject::connect(object, &QObject::destroyed, q, [this, object] { forget(object); });
        }

        void hook(QWidget *widget) {
            if (widget == nullptr || states.contains(widget)) return;
            State state;
            state.widget = widget;
            // On an editable combo, not its line edit: the combo forwards focus events to it by direct call, past filters.
            widget->installEventFilter(q);
            if (auto *combo = qobject_cast<QComboBox *>(widget)) {
                if (combo->isEditable()) {
                    QObject::connect(combo, &QComboBox::editTextChanged, q, [this, widget] { edited(widget); });
                    QObject::connect(combo, &QComboBox::activated, q, [this, widget] { committed(widget); });
                    if (combo->lineEdit() != nullptr) state.action = formAddMarkAction(combo->lineEdit(), q);
                } else {
                    QObject::connect(combo, &QComboBox::currentIndexChanged, q, [this, widget] { committed(widget); });
                }
            } else if (auto *edit = qobject_cast<QLineEdit *>(widget)) {
                QObject::connect(edit, &QLineEdit::textChanged, q, [this, widget] { edited(widget); });
                state.action = formAddMarkAction(edit, q);
            } else if (auto *plain = qobject_cast<QPlainTextEdit *>(widget)) {
                QObject::connect(plain, &QPlainTextEdit::textChanged, q, [this, widget] { edited(widget); });
            } else if (auto *rich = qobject_cast<QTextEdit *>(widget)) {
                QObject::connect(rich, &QTextEdit::textChanged, q, [this, widget] { edited(widget); });
            } else if (auto *spin = qobject_cast<QSpinBox *>(widget)) {
                QObject::connect(spin, &QSpinBox::valueChanged, q, [this, spin] { spinChanged(spin); });
            } else if (auto *doubleSpin = qobject_cast<QDoubleSpinBox *>(widget)) {
                QObject::connect(doubleSpin, &QDoubleSpinBox::valueChanged, q, [this, doubleSpin] { spinChanged(doubleSpin); });
            } else if (auto *button = qobject_cast<QAbstractButton *>(widget)) {
                QObject::connect(button, &QAbstractButton::toggled, q, [this, widget] { committed(widget); });
            }
            states.insert(widget, state);
            watch(widget);
        }

        Entry &addEntry(FormKind kind, QWidget *widget, Rule rule, QObject *owner) {
            auto &e = entries.emplace_back();
            e.kind = kind;
            e.key = widget;
            e.widget = widget;
            e.owner = owner != nullptr ? owner : widget;
            e.rule = std::move(rule);
            hook(widget);
            watch(owner);
            return e;
        }

        QString linesMessage(const Entry &e, QPlainTextEdit *edit, QList<int> *bad) const {
            QString first;
            int count = 0;
            for (QTextBlock block = edit->document()->begin(); block.isValid(); block = block.next()) {
                const QString line = e.field.trim ? block.text().trimmed() : block.text();
                if (line.trimmed().isEmpty()) continue;
                const QString message = e.rule(line);
                if (message.isEmpty()) continue;
                bad->append(block.blockNumber());
                if (count++ == 0) {
                    first = QCoreApplication::translate("Validation", "Line %1 (\"%2\"): %3")
                                .arg(QString::number(block.blockNumber() + 1), formElide(line.trimmed(), 40), message);
                }
            }
            if (count > 1) {
                first += QLatin1Char(' ') + QCoreApplication::translate("Validation", "%n more line(s) have problems.", nullptr, count - 1);
            }
            return first;
        }

        QString evaluate(const Entry &e, QList<int> *bad) const {
            if (!e.rule) return {};
            QString text;
            switch (e.kind) {
                case FormKind::Line:
                    if (auto *edit = qobject_cast<QLineEdit *>(e.widget)) text = edit->text();
                    break;
                case FormKind::Combo:
                    if (auto *combo = qobject_cast<QComboBox *>(e.widget)) text = combo->currentText();
                    break;
                case FormKind::Plain:
                    if (auto *plain = qobject_cast<QPlainTextEdit *>(e.widget)) text = plain->toPlainText();
                    break;
                case FormKind::Lines:
                    if (auto *plain = qobject_cast<QPlainTextEdit *>(e.widget)) return linesMessage(e, plain, bad);
                    return {};
            }
            return e.rule(e.field.trim ? text.trimmed() : text);
        }

        static void setMessage(State &state, const void *key, const QString &text, const QString &label, const CrossRule *rule,
                               Severity severity) {
            if (text.isEmpty()) state.messages.remove(key);
            else state.messages.insert(key, {text, label, rule, severity});
        }

        QString runEntry(Entry &e, bool apply) {
            QList<int> bad;
            const QString message = entryActive(e) ? evaluate(e, &bad) : QString();
            if (apply) {
                if (auto it = states.find(e.key); it != states.end()) {
                    setMessage(*it, &e, message, e.field.labelText, nullptr, e.field.severity);
                    if (e.kind == FormKind::Lines) it->badBlocks = message.isEmpty() ? QList<int>() : bad;
                }
            }
            return message;
        }

        // Runs while any participant is active; the issue belongs to the first active one and marks only active ones.
        QString runRule(CrossRule &r, bool apply, QSet<const QObject *> *touched, QWidget **first) {
            QList<bool> active;
            QWidget *anchor = nullptr;
            for (const auto &p: r.participants) {
                active << (p && !participantSkipped(p));
                if (anchor == nullptr && active.last()) anchor = p;
            }
            const QString message = anchor != nullptr && r.check ? r.check() : QString();
            if (apply) {
                for (qsizetype k = 0; k < r.keys.size(); ++k) {
                    if (auto it = states.find(r.keys[k]); it != states.end()) {
                        setMessage(*it, &r, active.value(k) ? message : QString(), r.label, &r, r.severity);
                        if (touched != nullptr) touched->insert(r.keys[k]);
                    }
                }
            }
            if (first != nullptr) *first = anchor;
            return message;
        }

        void applyMark(State &state) {
            QWidget *widget = state.widget;
            if (widget == nullptr) return;
            const FormMark mark = markOf(state);
            const bool marked = mark != FormMark::None;
            if (mark != state.mark) {
                state.mark = mark;
                formRepolish(widget, mark);
                if (state.action && marked) state.action->setIcon(formMarkIcon(mark));
            }
            QStringList texts;
            for (const auto &m: std::as_const(state.messages)) {
                if (!texts.contains(m.text)) texts << m.text;
            }
            const QString tip = texts.join(QLatin1Char('\n'));
            if (state.action) {
                state.action->setToolTip(tip);
                state.action->setVisible(marked);
            } else if (qobject_cast<QPlainTextEdit *>(widget) || qobject_cast<QTextEdit *>(widget) || qobject_cast<QAbstractSpinBox *>(widget)) {
                if (marked) {
                    if (!state.toolTipSaved) {
                        state.baseToolTip = widget->toolTip();
                        state.toolTipSaved = true;
                    }
                    widget->setToolTip(state.baseToolTip.isEmpty() ? tip : tip + QStringLiteral("\n\n") + state.baseToolTip);
                } else if (state.toolTipSaved) {
                    widget->setToolTip(state.baseToolTip);
                    state.toolTipSaved = false;
                }
            }
            if (auto *plain = qobject_cast<QPlainTextEdit *>(widget)) {
                const QList<int> blocks = marked ? state.badBlocks : QList<int>();
                if (!blocks.isEmpty() || state.lineMarks) {
                    formSetLineMarks(plain, blocks, formMarkColor(mark));
                    state.lineMarks = !blocks.isEmpty();
                }
            }
        }

        void applyMarks(const QSet<const QObject *> &keys) {
            for (const QObject *key: keys) {
                if (auto it = states.find(key); it != states.end()) applyMark(*it);
            }
        }

        QString labelFor(QWidget *widget, const QString &explicitLabel) const {
            if (!explicitLabel.isEmpty()) return formCleanLabel(explicitLabel);
            const auto textOf = [](QWidget *labelWidget) -> QString {
                if (auto *label = qobject_cast<QLabel *>(labelWidget)) return formCleanLabel(label->text());
                if (auto *button = qobject_cast<QAbstractButton *>(labelWidget)) return formCleanLabel(button->text());
                return {};
            };
            for (QLabel *label: scope->findChildren<QLabel *>()) {
                if (label->buddy() != widget) continue;
                if (const QString text = textOf(label); !text.isEmpty()) return text;
            }
            if (QWidget *parent = widget->parentWidget(); parent != nullptr && parent->layout() != nullptr) {
                QList<QPair<QLayout *, int>> path;
                if (formLayoutPath(parent->layout(), widget, path)) {
                    for (auto i = path.size() - 1; i >= 0; --i) {
                        QWidget *labelWidget = nullptr;
                        if (auto *form = qobject_cast<QFormLayout *>(path[i].first)) {
                            int row = -1;
                            QFormLayout::ItemRole role{};
                            form->getItemPosition(path[i].second, &row, &role);
                            if (row >= 0 && role == QFormLayout::FieldRole) {
                                if (QLayoutItem *item = form->itemAt(row, QFormLayout::LabelRole)) labelWidget = item->widget();
                            }
                        } else if (auto *grid = qobject_cast<QGridLayout *>(path[i].first)) {
                            int row = 0, column = 0, rowSpan = 0, columnSpan = 0;
                            grid->getItemPosition(path[i].second, &row, &column, &rowSpan, &columnSpan);
                            if (column > 0) {
                                if (QLayoutItem *item = grid->itemAtPosition(row, column - 1)) labelWidget = item->widget();
                            }
                            if (!qobject_cast<QLabel *>(labelWidget) && !qobject_cast<QCheckBox *>(labelWidget)) labelWidget = nullptr;
                        }
                        if (labelWidget == nullptr) continue;
                        if (const QString text = textOf(labelWidget); !text.isEmpty()) return text;
                    }
                }
            }
            if (auto *button = qobject_cast<QAbstractButton *>(widget)) {
                if (const QString text = formCleanLabel(button->text()); !text.isEmpty()) return text;
            }
            QString placeholder;
            if (auto *edit = qobject_cast<QLineEdit *>(formTextTarget(widget))) placeholder = edit->placeholderText();
            else if (auto *plain = qobject_cast<QPlainTextEdit *>(widget)) placeholder = plain->placeholderText();
            else if (auto *rich = qobject_cast<QTextEdit *>(widget)) placeholder = rich->placeholderText();
            return formCleanLabel(placeholder);
        }

        QHash<const QObject *, int> focusOrder() const {
            QHash<const QObject *, int> order;
            QWidget *widget = scope;
            for (int i = 0; widget != nullptr && !order.contains(widget); ++i) {
                order.insert(widget, i);
                widget = widget->nextInFocusChain();
            }
            return order;
        }

        void sortByFocus(QList<FormIssue> &issues) const {
            const auto order = focusOrder();
            std::stable_sort(issues.begin(), issues.end(), [&order](const FormIssue &a, const FormIssue &b) {
                return order.value(a.widget.data(), INT_MAX) < order.value(b.widget.data(), INT_MAX);
            });
        }

        // Errors before warnings, the field's own rule before a cross rule.
        static const Message &statusMessage(const State &state) {
            const Message *chosen = &state.messages.first();
            int best = INT_MAX;
            for (const auto &m: state.messages) {
                const int rank = (m.severity == Severity::Error ? 0 : 2) + (m.rule == nullptr ? 0 : 1);
                if (rank < best) {
                    best = rank;
                    chosen = &m;
                }
            }
            return *chosen;
        }

        bool shownInStatus(const State &state) const { return !state.messages.isEmpty() && state.widget && usable(state.widget); }

        void setStatus(const QObject *key, const State &state) {
            statusKey = key;
            const Message &message = statusMessage(state);
            QString line = formIssueText(labelFor(state.widget, message.label), message.text);
            line.replace(QLatin1Char('\n'), QLatin1Char(' '));
            const QString role = message.severity == Severity::Error ? QStringLiteral("danger") : QStringLiteral("muted");
            if (status->property("colorRole").toString() != role) {
                status->setProperty("colorRole", role);
                formRestyle(status);
            }
            status->setText(line);
        }

        void showStatus(QWidget *widget) {
            if (!status) return;
            const auto it = states.constFind(widget);
            if (it == states.constEnd() || !shownInStatus(*it)) {
                refreshStatus();
                return;
            }
            setStatus(widget, *it);
        }

        void refreshStatus() {
            if (!status) return;
            if (statusKey != nullptr) {
                if (const auto it = states.constFind(statusKey); it != states.constEnd() && shownInStatus(*it)) {
                    setStatus(statusKey, *it);
                    return;
                }
            }
            statusKey = nullptr;
            QList<const QObject *> candidates;
            for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
                if (shownInStatus(*it)) candidates << it.key();
            }
            if (candidates.isEmpty()) {
                status->clear();
                return;
            }
            const auto order = focusOrder();
            const auto rank = [this, &order](const QObject *key) {
                return std::pair(markOf(*states.constFind(key)) == FormMark::Error ? 0 : 1, order.value(key, INT_MAX));
            };
            const auto next = *std::min_element(candidates.begin(), candidates.end(),
                                                [&rank](const QObject *a, const QObject *b) { return rank(a) < rank(b); });
            setStatus(next, *states.constFind(next));
        }

        void validateWidget(QWidget *widget, bool fromUser) {
            QSet<const QObject *> touched{widget};
            for (Entry &e: entries) {
                if (e.key == widget && e.widget) runEntry(e, true);
            }
            for (CrossRule &r: rules) {
                if (r.keys.contains(widget)) runRule(r, true, &touched, nullptr);
            }
            applyMarks(touched);
            const auto it = states.constFind(widget);
            if (fromUser && it != states.constEnd() && it->mark != FormMark::None) showStatus(widget);
            else refreshStatus();
        }

        void focusLeft(QObject *watchedObject) {
            if (pauses > 0) return;
            const auto it = states.constFind(watchedObject);
            if (it == states.constEnd() || !it->widget) return;
            // A widget being destroyed still sends FocusOut, by then only a QWidget.
            if (!formIsTextWidget(watchedObject) || !liveWindow()) return;
            validateWidget(it->widget, true);
        }

        void committed(QWidget *widget) {
            if (pauses > 0 || !liveWindow() || !states.contains(widget)) return;
            validateWidget(widget, true);
        }

        // Typing emits per keystroke and the wheel focuses first, so only an unfocused (programmatic) change counts as committed.
        void spinChanged(QAbstractSpinBox *spin) {
            if (spin->hasFocus()) edited(spin);
            else committed(spin);
        }

        void edited(QWidget *widget) {
            const auto it = states.find(widget);
            if (it == states.end()) return;
            QSet<const QObject *> touched;
            if (it->mark != FormMark::None || !it->messages.isEmpty()) {
                it->messages.clear();
                it->badBlocks.clear();
                touched.insert(widget);
            }
            // Through the rules, not the widget's own messages: a skipped participant carries none of its rule's marks.
            for (const CrossRule &r: rules) {
                if (!r.keys.contains(widget)) continue;
                for (const QObject *key: r.keys) {
                    if (auto other = states.find(key); other != states.end() && other->messages.remove(&r)) touched.insert(key);
                }
            }
            if (touched.isEmpty()) return;
            applyMarks(touched);
            refreshStatus();
        }

        // Hide and EnabledChange reach every descendant, so a marked field learns that a tab, row or editor went away.
        void activityChanged(QObject *object) {
            if (pauses > 0 || refreshQueued || !liveWindow()) return;
            const auto it = states.constFind(object);
            if (it == states.constEnd() || it->messages.isEmpty()) return;
            refreshQueued = true;
            QTimer::singleShot(0, q, [this] {
                refreshQueued = false;
                refresh();
            });
        }

        void refresh() {
            if (pauses > 0) return;
            QList<const QObject *> marked;
            for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
                if (it->widget && !it->messages.isEmpty()) marked << it.key();
            }
            QSet<const QObject *> touched;
            QSet<const CrossRule *> done;
            for (const QObject *key: marked) {
                touched.insert(key);
                for (Entry &e: entries) {
                    if (e.key == key && e.widget) runEntry(e, true);
                }
                for (CrossRule &r: rules) {
                    if (!r.keys.contains(key) || done.contains(&r)) continue;
                    done.insert(&r);
                    runRule(r, true, &touched, nullptr);
                }
                if (auto it = states.find(key); it != states.end() && it->widget && !usable(it->widget)) {
                    for (const Source &s: sources) it->messages.remove(&s);
                }
            }
            applyMarks(touched);
            refreshStatus();
        }

        QList<FormIssue> collect(bool apply) {
            QList<FormIssue> out;
            QSet<const QObject *> touched;
            if (apply) touched.unite(std::exchange(pending, {}));
            for (Entry &e: entries) {
                if (!e.widget) continue;
                if (const QString message = runEntry(e, apply); !message.isEmpty()) {
                    out.append({e.field.severity, labelFor(e.widget, e.field.labelText), message, e.widget, {}});
                }
                touched.insert(e.key);
            }
            for (CrossRule &r: rules) {
                QWidget *first = nullptr;
                if (const QString message = runRule(r, apply, &touched, &first); !message.isEmpty()) {
                    out.append({r.severity, labelFor(first, r.label), message, first, {}});
                }
            }
            for (Source &s: sources) {
                if (apply) {
                    for (auto it = states.begin(); it != states.end(); ++it) {
                        if (it->messages.remove(&s)) touched.insert(it.key());
                    }
                }
                const auto found = s.collect ? s.collect() : QList<FormIssue>();
                for (FormIssue issue: found) {
                    if (issue.widget && issue.label.isEmpty()) issue.label = labelFor(issue.widget, {});
                    if (apply && issue.widget) {
                        hook(issue.widget);
                        State &state = states[issue.widget.data()];
                        if (auto m = state.messages.find(&s); m != state.messages.end()) {
                            m->text += QLatin1Char('\n') + issue.message;
                            if (issue.severity == Severity::Error) m->severity = Severity::Error;
                        } else {
                            state.messages.insert(&s, {issue.message, issue.label, nullptr, issue.severity});
                        }
                        touched.insert(issue.widget.data());
                    }
                    out.append(issue);
                }
            }
            if (apply) applyMarks(touched);
            return out;
        }

        // Page switches run user slots that may delete pages or the target, so every widget is re-checked after one.
        void reveal(const FormIssue &issue) {
            QPointer<QWidget> target;
            if (issue.reveal) target = issue.reveal();
            if (!target) target = issue.widget;
            if (!target) return;
            QList<QPointer<QWidget>> chain;
            for (QWidget *p = target; p != nullptr && p != scope; p = p->parentWidget()) chain.prepend(p);
            for (const QPointer<QWidget> &page: std::as_const(chain)) {
                if (!page) continue;
                auto *stack = qobject_cast<QStackedWidget *>(page->parentWidget());
                if (stack == nullptr || stack->indexOf(page) < 0) continue;
                if (auto *tabs = qobject_cast<QTabWidget *>(stack->parentWidget()); tabs != nullptr && tabs->indexOf(page) >= 0) {
                    tabs->setCurrentWidget(page);
                } else {
                    stack->setCurrentWidget(page);
                }
                if (!target) return;
            }
            if (scope->layout() != nullptr) scope->layout()->activate();
            const auto ensureVisible = [stop = scope](QWidget *widget) {
                for (QWidget *p = widget->parentWidget(); p != nullptr && p != stop; p = p->parentWidget()) {
                    if (auto *area = qobject_cast<QScrollArea *>(p)) area->ensureWidgetVisible(widget);
                }
            };
            ensureVisible(target);
            target->setFocus(Qt::OtherFocusReason);
            // A page shown just now is laid out by its own pending layout request.
            QTimer::singleShot(0, q, [widget = target, ensureVisible] {
                if (widget) ensureVisible(widget);
            });
        }

        void summarize(const QList<FormIssue> &errors, const QList<FormIssue> &warnings) {
            const auto bullet = [](const FormIssue &issue) {
                return QStringLiteral("• ") + formIssueText(issue.label, issue.message);
            };
            QStringList lines;
            for (int i = 0; i < errors.size() && i < formSummaryLines; ++i) lines << bullet(errors.at(i));
            QString text = QCoreApplication::translate("Validation", "Fix these before saving:") + QStringLiteral("\n\n") + lines.join(QLatin1Char('\n'));
            if (errors.size() > formSummaryLines) {
                text += QLatin1Char('\n') + QCoreApplication::translate("Validation", "…and %n more", nullptr, int(errors.size() - formSummaryLines));
            }
            auto *box = new QMessageBox(QMessageBox::Warning, QCoreApplication::translate("Validation", "Invalid input"), text,
                                        QMessageBox::Ok, scope->window());
            box->setTextFormat(Qt::PlainText);
            if (errors.size() > formSummaryLines || !warnings.isEmpty()) {
                QStringList details;
                for (const auto &issue: errors) details << bullet(issue);
                if (!warnings.isEmpty()) {
                    details << QString() << QCoreApplication::translate("Validation", "Warnings:");
                    for (const auto &issue: warnings) details << bullet(issue);
                }
                box->setDetailedText(details.join(QLatin1Char('\n')));
            }
            box->setAttribute(Qt::WA_DeleteOnClose);
            box->open();
        }

        void schedule(const QObject *key) {
            pending.insert(key);
            if (flushQueued) return;
            flushQueued = true;
            // Deferred: destroyed() fires while the dying parent's child list holds nulls, so findChildren() there crashes.
            QTimer::singleShot(0, q, [this] {
                flushQueued = false;
                applyMarks(std::exchange(pending, {}));
                refreshStatus();
            });
        }

        void forget(QObject *object) {
            watched.remove(object);
            const auto drop = [this, object](const QObject *key, const void *source) {
                const auto it = states.find(key);
                if (it == states.end() || !it->messages.remove(source)) return;
                if (it->widget && !formHasAncestor(it->widget, object)) schedule(key);
            };
            for (auto it = entries.begin(); it != entries.end();) {
                if (it->key != object && it->owner != object) {
                    ++it;
                    continue;
                }
                if (it->key != object) drop(it->key, &*it);
                it = entries.erase(it);
            }
            for (auto it = rules.begin(); it != rules.end();) {
                if (it->owner != object && !it->keys.contains(object)) {
                    ++it;
                    continue;
                }
                for (const QObject *key: it->keys) {
                    if (key != object) drop(key, &*it);
                }
                it = rules.erase(it);
            }
            for (auto it = sources.begin(); it != sources.end();) {
                if (it->owner != object) {
                    ++it;
                    continue;
                }
                for (auto s = states.begin(); s != states.end(); ++s) {
                    if (s->messages.contains(&*it)) drop(s.key(), &*it);
                }
                it = sources.erase(it);
            }
            states.remove(object);
            pending.remove(object);
            if (statusKey == object) {
                statusKey = nullptr;
                schedule(nullptr);
            }
        }
    };

    Form::Pause::Pause(Form &form) : target(&form) {
        ++form.d->pauses;
    }

    Form::Pause::~Pause() {
        if (target && --target->d->pauses == 0) target->refresh();
    }

    Form::Form(QWidget *scope) : QObject(scope), d(std::make_unique<Private>()) {
        d->q = this;
        d->scope = scope;
    }

    Form::~Form() = default;

    Form::Field &Form::bind(QLineEdit *edit, Rule rule, QObject *owner) {
        if (edit == nullptr) return d->detached;
        return d->addEntry(FormKind::Line, edit, std::move(rule), owner).field;
    }

    Form::Field &Form::bind(QPlainTextEdit *edit, Rule rule, QObject *owner) {
        if (edit == nullptr) return d->detached;
        return d->addEntry(FormKind::Plain, edit, std::move(rule), owner).field;
    }

    Form::Field &Form::bind(QComboBox *editableCombo, Rule rule, QObject *owner) {
        if (editableCombo == nullptr) return d->detached;
        return d->addEntry(FormKind::Combo, editableCombo, std::move(rule), owner).field;
    }

    Form::Field &Form::bindLines(QPlainTextEdit *edit, Rule perLine, QObject *owner) {
        if (edit == nullptr) return d->detached;
        return d->addEntry(FormKind::Lines, edit, std::move(perLine), owner).field;
    }

    void Form::addRule(const QList<QWidget *> &participants, std::function<QString()> check, QObject *owner,
                       Severity severity, const QString &label) {
        auto &r = d->rules.emplace_back();
        for (QWidget *widget: participants) {
            if (widget == nullptr || r.keys.contains(widget)) continue;
            r.keys << widget;
            r.participants << widget;
            d->hook(widget);
        }
        r.check = std::move(check);
        r.owner = owner;
        r.severity = severity;
        r.label = label;
        d->watch(owner);
    }

    void Form::addIssueSource(std::function<QList<FormIssue>()> source, QObject *owner) {
        auto &s = d->sources.emplace_back();
        s.collect = std::move(source);
        s.owner = owner;
        d->watch(owner);
    }

    void Form::attachStatus(QDialogButtonBox *buttons) {
        if (buttons == nullptr || d->status) return;
        QWidget *host = buttons->parentWidget();
        if (host == nullptr || host->layout() == nullptr) return;
        auto *row = new QWidget(host);
        // Keeps the button box's cell, stretch and alignment, in a QGridLayout too.
        QLayoutItem *old = host->layout()->replaceWidget(buttons, row);
        if (old == nullptr) {
            delete row;
            return;
        }
        delete old;
        row->setSizePolicy(buttons->sizePolicy());
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto *label = new ValidationStatusLabel(row);
        rowLayout->addWidget(label, 1);
        rowLayout->addWidget(buttons);
        d->status = label;
    }

    void Form::markAll() {
        auto all = d->collect(true);
        d->sortByFocus(all);
        const auto error = std::find_if(all.cbegin(), all.cend(), [](const FormIssue &i) { return i.severity == Severity::Error; });
        const auto first = error != all.cend() ? error : all.cbegin();
        if (first != all.cend() && first->widget) d->showStatus(first->widget);
        else d->refreshStatus();
    }

    bool Form::validateAll() {
        auto all = d->collect(true);
        d->sortByFocus(all);
        QList<FormIssue> errors, warnings;
        for (const auto &issue: all) (issue.severity == Severity::Error ? errors : warnings) << issue;
        if (errors.isEmpty()) {
            d->refreshStatus();
            return true;
        }
        d->reveal(errors.first());
        if (errors.first().widget) d->showStatus(errors.first().widget);
        else d->refreshStatus();
        d->summarize(errors, warnings);
        return false;
    }

    QList<FormIssue> Form::issues() {
        auto all = d->collect(false);
        d->sortByFocus(all);
        return all;
    }

    void Form::revalidate(QWidget *widget) {
        if (widget == nullptr || !d->states.contains(widget)) return;
        d->validateWidget(widget, false);
    }

    void Form::refresh() {
        d->refresh();
    }

    bool Form::eventFilter(QObject *watched, QEvent *event) {
        switch (event->type()) {
            case QEvent::FocusOut:
                if (static_cast<QFocusEvent *>(event)->reason() != Qt::PopupFocusReason) d->focusLeft(watched);
                break;
            case QEvent::Hide:
            case QEvent::EnabledChange:
                d->activityChanged(watched);
                break;
            default:
                break;
        }
        return QObject::eventFilter(watched, event);
    }
}
