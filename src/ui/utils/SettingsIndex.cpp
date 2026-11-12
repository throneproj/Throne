#include "include/ui/utils/SettingsIndex.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QDialog>
#include <QFormLayout>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QPainter>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextDocument>
#include <QTimer>

#include "include/ui/setting/dialog_basic_settings.h"
#include "include/ui/setting/dialog_integration.h"
#include "include/ui/setting/dialog_manage_routes.h"
#include "include/ui/setting/dialog_preset_settings.h"
#include "include/ui/setting/dialog_vpn_settings.h"
#include "include/ui/utils/CommandPalette.h"
#include "include/ui/utils/UiMode.h"

namespace SettingsIndex {
    namespace {
        constexpr int kMaxLabelLength = 48;

        QLayout *layoutHolding(QLayout *layout, QWidget *widget) {
            if (!layout) return nullptr;
            if (layout->indexOf(widget) >= 0) return layout;
            for (int i = 0; i < layout->count(); ++i) {
                if (auto *found = layoutHolding(layout->itemAt(i)->layout(), widget)) return found;
            }
            return nullptr;
        }

        QWidget *firstWidget(QLayoutItem *item) {
            if (!item) return nullptr;
            if (item->widget()) return item->widget();
            if (auto *layout = item->layout()) {
                for (int i = 0; i < layout->count(); ++i) {
                    if (auto *widget = firstWidget(layout->itemAt(i))) return widget;
                }
            }
            return nullptr;
        }

        bool holds(QLayoutItem *item, QWidget *widget) {
            if (!item) return false;
            if (item->widget() == widget) return true;
            if (auto *layout = item->layout()) {
                for (int i = 0; i < layout->count(); ++i) {
                    if (holds(layout->itemAt(i), widget)) return true;
                }
            }
            return false;
        }

        QString labelText(QWidget *widget) {
            auto *label = qobject_cast<QLabel *>(widget);
            return label ? CommandPalette::plainText(label->text()) : QString();
        }

        // The label that names widget's row: earlier in its horizontal box, or in an earlier column of its form or grid row.
        QString rowCaption(QWidget *widget) {
            QWidget *parent = widget->parentWidget();
            for (QLayout *layout = layoutHolding(parent ? parent->layout() : nullptr, widget); layout;
                 layout = qobject_cast<QLayout *>(layout->parent())) {
                int index = -1;
                for (int i = 0; i < layout->count() && index < 0; ++i) {
                    if (holds(layout->itemAt(i), widget)) index = i;
                }
                if (index < 0) return {};

                if (auto *form = qobject_cast<QFormLayout *>(layout)) {
                    int row = -1;
                    QFormLayout::ItemRole role;
                    form->getItemPosition(index, &row, &role);
                    return role == QFormLayout::FieldRole ? labelText(firstWidget(form->itemAt(row, QFormLayout::LabelRole))) : QString();
                }
                if (auto *grid = qobject_cast<QGridLayout *>(layout)) {
                    int row, column, rowSpan, columnSpan;
                    grid->getItemPosition(index, &row, &column, &rowSpan, &columnSpan);
                    for (int c = column - 1; c >= 0; --c) {
                        const QString caption = labelText(firstWidget(grid->itemAtPosition(row, c)));
                        if (!caption.isEmpty()) return caption;
                    }
                    return {};
                }
                auto *box = qobject_cast<QBoxLayout *>(layout);
                if (box && (box->direction() == QBoxLayout::LeftToRight || box->direction() == QBoxLayout::RightToLeft)) {
                    for (int i = index - 1; i >= 0; --i) {
                        const QString caption = labelText(box->itemAt(i)->widget());
                        if (!caption.isEmpty()) return caption;
                    }
                }
            }
            return {};
        }

        // Tabs and group boxes above an item, as the form lays them out and once Simple mode has folded them.
        struct Where {
            QStringList path;
            QStringList simplePath;

            Where operator+(const QString &title) const {
                if (title.isEmpty()) return *this;
                return {path + QStringList{title}, simplePath + QStringList{title}};
            }
        };

        class Collector {
        public:
            Collector(QWidget *root, QString dialogClass, QList<QWidget *> unavailable, QList<Item> &out)
                : root(root), dialogClass(std::move(dialogClass)), unavailable(std::move(unavailable)), out(out) {}

            void walk(QWidget *parent, const Where &where) {
                for (auto *child : parent->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
                    if (!child->isVisibleTo(parent) || unavailable.contains(child)) continue;

                    if (auto *tabs = qobject_cast<QTabWidget *>(child)) {
                        for (int i = 0; i < tabs->count(); ++i) {
                            const QString title = CommandPalette::plainText(tabs->tabText(i));
                            enterSection(tabs->widget(i), title, where);
                        }
                        continue;
                    }
                    if (auto *group = qobject_cast<QGroupBox *>(child)) {
                        enterSection(group, CommandPalette::plainText(group->title()), where);
                        continue;
                    }

                    QString text;
                    if (qobject_cast<QCheckBox *>(child) || qobject_cast<QRadioButton *>(child)) {
                        text = CommandPalette::plainText(static_cast<QAbstractButton *>(child)->text());
                    } else if (auto *button = qobject_cast<QPushButton *>(child)) {
                        // Lone verbs like "Edit" or "Install" mean nothing out of place.
                        text = CommandPalette::plainText(button->text());
                        if (!text.contains(' ')) text.clear();
                    } else if (auto *label = qobject_cast<QLabel *>(child)) {
                        if (!Qt::mightBeRichText(label->text())) text = labelText(label);
                    }
                    if (!text.isEmpty()) {
                        QString caption = rowCaption(child);
                        if (caption.isEmpty() && !where.path.isEmpty()) caption = where.path.last();
                        add(child, text, where, caption == text ? QString() : caption);
                    }
                    walk(child, where);
                }
            }

            // "Enable" or a twice-used "Disable IPv6" means something only with its row's label or its group's title.
            void finish() {
                QHash<QString, int> uses;
                for (const auto &entry : found) ++uses[entry.item.label];
                for (auto &entry : found) {
                    const QString &text = entry.item.label;
                    const bool vague = !text.contains(' ') || uses.value(text) > 1;
                    if (vague && !entry.caption.isEmpty()) entry.item.label = entry.caption + ": " + text;
                    out.append(entry.item);
                }
            }

        private:
            struct Found {
                Item item;
                QString caption;
            };

            // A tab page or group box: listed itself, then walked with its title added, or moved under its fold target.
            void enterSection(QWidget *section, const QString &title, const Where &where) {
                const QString target = UiMode::FoldTarget(section);
                if (target.isEmpty()) {
                    add(section, title, where);
                    walk(section, where + title);
                    return;
                }
                const Where folded{where.path, {pageTitle(root->findChild<QWidget *>(target))}};
                add(section, title, folded);
                walk(section, {(where + title).path, (folded + title).simplePath});
            }

            static QString pageTitle(QWidget *page) {
                auto *stack = page ? qobject_cast<QStackedWidget *>(page->parentWidget()) : nullptr;
                auto *tabs = stack ? qobject_cast<QTabWidget *>(stack->parentWidget()) : nullptr;
                return tabs ? CommandPalette::plainText(tabs->tabText(tabs->indexOf(page))) : QString();
            }

            void add(QWidget *widget, const QString &text, const Where &where, const QString &caption = {}) {
                // Reveal finds the widget by name again, and findChild with an empty name matches any child.
                if (text.isEmpty() || text.size() > kMaxLabelLength || widget->objectName().isEmpty()) return;
                Item item;
                item.dialogClass = dialogClass;
                item.objectName = widget->objectName();
                item.label = text;
                item.path = where.path.join(QStringLiteral(" › "));
                item.simplePath = where.simplePath.join(QStringLiteral(" › "));
                item.keywords = CommandPalette::keywordsFor(widget->objectName());
                item.advanced = UiMode::IsAdvanced(widget);
                found.append({item, caption});
            }

            QWidget *root;
            QString dialogClass;
            QList<QWidget *> unavailable;
            QList<Item> &out;
            QList<Found> found;
        };

        // unavailable lists the widgets the real dialog hides on this system.
        template <typename Form, typename Unavailable>
        void collectForm(const char *dialogClass, QList<Item> &out, Unavailable unavailable) {
            QDialog host;
            Form form;
            form.setupUi(&host);
            Collector collector(&host, QString::fromLatin1(dialogClass), unavailable(&form), out);
            collector.walk(&host, {});
            collector.finish();
        }

        template <typename Form>
        void collectForm(const char *dialogClass, QList<Item> &out) {
            collectForm<Form>(dialogClass, out, [](const Form *) { return QList<QWidget *>(); });
        }

        // The field a label names: its buddy, else the next cell of its form, grid or box row.
        QWidget *fieldOf(QLabel *label) {
            if (label->buddy()) return label->buddy();
            QWidget *parent = label->parentWidget();
            QLayout *layout = layoutHolding(parent ? parent->layout() : nullptr, label);
            if (auto *form = qobject_cast<QFormLayout *>(layout)) {
                int row = -1;
                QFormLayout::ItemRole role;
                form->getWidgetPosition(label, &row, &role);
                if (row >= 0 && role == QFormLayout::LabelRole) return firstWidget(form->itemAt(row, QFormLayout::FieldRole));
            } else if (auto *grid = qobject_cast<QGridLayout *>(layout)) {
                int row, column, rowSpan, columnSpan;
                grid->getItemPosition(grid->indexOf(label), &row, &column, &rowSpan, &columnSpan);
                return firstWidget(grid->itemAtPosition(row, column + columnSpan));
            } else if (auto *box = qobject_cast<QBoxLayout *>(layout)) {
                for (int i = box->indexOf(label) + 1; i < box->count(); ++i) {
                    if (auto *widget = firstWidget(box->itemAt(i))) return widget;
                }
            }
            return nullptr;
        }

        class Outline : public QWidget {
        public:
            explicit Outline(QWidget *parent) : QWidget(parent) {
                setAttribute(Qt::WA_TransparentForMouseEvents);
            }

        protected:
            void paintEvent(QPaintEvent *) override {
                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing);
                const QColor accent = palette().color(QPalette::Highlight);
                QColor fill = accent;
                fill.setAlphaF(0.15);
                painter.setPen(QPen(accent, 2));
                painter.setBrush(fill);
                painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 4, 4);
            }
        };

        void flash(QDialog *dialog, const QList<QPointer<QWidget>> &widgets) {
            QRect area;
            for (const auto &widget : widgets) {
                if (widget && widget->isVisible()) area |= QRect(widget->mapTo(dialog, QPoint(0, 0)), widget->size());
            }
            if (area.isEmpty()) return;

            auto *outline = new Outline(dialog);
            outline->setGeometry(area.adjusted(-5, -3, 5, 3));
            auto *effect = new QGraphicsOpacityEffect(outline);
            outline->setGraphicsEffect(effect);
            outline->show();
            outline->raise();

            auto *fade = new QPropertyAnimation(effect, "opacity", outline);
            fade->setDuration(2200);
            fade->setKeyValueAt(0, 1.0);
            fade->setKeyValueAt(0.6, 1.0);
            fade->setKeyValueAt(1, 0.0);
            QObject::connect(fade, &QPropertyAnimation::finished, outline, &QObject::deleteLater);
            fade->start();
        }
    }

    QList<Item> Build() {
        QList<Item> out;
        collectForm<Ui::DialogBasicSettings>(DialogBasicSettings::staticMetaObject.className(), out,
                                             &DialogBasicSettings::UnavailableWidgets);
        collectForm<Ui::DialogManageRoutes>(DialogManageRoutes::staticMetaObject.className(), out);
        collectForm<Ui::DialogVPNSettings>(DialogVPNSettings::staticMetaObject.className(), out,
                                           &DialogVPNSettings::UnavailableWidgets);
        collectForm<Ui::DialogPresetSettings>(DialogPresetSettings::staticMetaObject.className(), out);
        collectForm<Ui::DialogIntegration>(DialogIntegration::staticMetaObject.className(), out);
        return out;
    }

    void Reveal(QDialog *dialog, const QString &objectName) {
        if (objectName.isEmpty()) return;
        auto *target = dialog->findChild<QWidget *>(objectName);
        if (!target) return;

        // Outermost tab first, so a nested tab widget is already current when its own page switches.
        QList<QPair<QTabWidget *, QWidget *>> tabs;
        for (QWidget *widget = target; widget && widget != dialog; widget = widget->parentWidget()) {
            auto *stack = qobject_cast<QStackedWidget *>(widget->parentWidget());
            if (auto *tabWidget = stack ? qobject_cast<QTabWidget *>(stack->parentWidget()) : nullptr) tabs.prepend({tabWidget, widget});
        }
        for (const auto &tab : tabs) tab.first->setCurrentWidget(tab.second);
        if (!tabs.isEmpty() && tabs.last().second == target) return;
        if (!target->isVisibleTo(dialog)) return;

        QWidget *focus = target;
        QList<QPointer<QWidget>> marked{target};
        if (auto *label = qobject_cast<QLabel *>(target)) {
            focus = fieldOf(label);
            if (focus) marked.append(focus);
        }
        for (QWidget *widget = target->parentWidget(); widget && widget != dialog; widget = widget->parentWidget()) {
            if (auto *scroll = qobject_cast<QScrollArea *>(widget)) scroll->ensureWidgetVisible(target);
        }
        if (focus && focus->isEnabled() && focus->focusPolicy() != Qt::NoFocus) focus->setFocus(Qt::OtherFocusReason);

        // Next turn of the event loop: the dialog has only just been shown and laid out.
        QPointer<QDialog> guard(dialog);
        QTimer::singleShot(0, dialog, [guard, marked] {
            if (guard) flash(guard, marked);
        });
    }
}
