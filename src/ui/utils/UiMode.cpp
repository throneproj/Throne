#include "include/ui/utils/UiMode.h"

#include <algorithm>

#include <QAction>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include "include/global/Configs.hpp"

namespace UiMode {
    namespace {
        constexpr auto kLevelProperty = "uiLevel";
        constexpr auto kWantedProperty = "uiWanted";
        constexpr auto kFoldProperty = "uiFoldInto";

        bool tagged(const QObject *object) {
            return object != nullptr && object->property(kLevelProperty).toString() == QLatin1String("advanced");
        }

        QLayout *layoutHolding(QLayout *layout, QWidget *widget) {
            if (!layout) return nullptr;
            if (layout->indexOf(widget) >= 0) return layout;
            for (int i = 0; i < layout->count(); ++i) {
                if (auto *found = layoutHolding(layout->itemAt(i)->layout(), widget)) return found;
            }
            return nullptr;
        }

        QLayout *layoutOf(const QWidget *widget) {
            QWidget *parent = widget->parentWidget();
            return parent ? layoutHolding(parent->layout(), const_cast<QWidget *>(widget)) : nullptr;
        }

        QFormLayout *formOf(const QWidget *widget) {
            return qobject_cast<QFormLayout *>(layoutOf(widget));
        }

        QWidget *rowLabel(const QWidget *widget) {
            auto *form = formOf(widget);
            return form ? form->labelForField(const_cast<QWidget *>(widget)) : nullptr;
        }

        QWidget *rowField(const QLabel *label) {
            if (label->buddy()) return label->buddy();
            auto *form = formOf(label);
            if (!form) return nullptr;
            int row = -1;
            QFormLayout::ItemRole role;
            form->getWidgetPosition(const_cast<QLabel *>(label), &row, &role);
            if (row < 0 || role != QFormLayout::LabelRole) return nullptr;
            auto *item = form->itemAt(row, QFormLayout::FieldRole);
            return item ? item->widget() : nullptr;
        }

        QTabWidget *tabsOf(QWidget *page) {
            auto *stack = qobject_cast<QStackedWidget *>(page->parentWidget());
            auto *tabs = stack ? qobject_cast<QTabWidget *>(stack->parentWidget()) : nullptr;
            return tabs && tabs->indexOf(page) >= 0 ? tabs : nullptr;
        }

        bool shown(QWidget *widget) {
            if (auto *tabs = tabsOf(widget)) return tabs->isTabVisible(tabs->indexOf(widget));
            return !widget->isHidden();
        }

        // QFormLayout::setRowVisible() is Qt 6.4+, so a form field takes its label along by hand.
        void place(QWidget *widget, bool visible) {
            if (auto *tabs = tabsOf(widget)) {
                tabs->setTabVisible(tabs->indexOf(widget), visible);
                return;
            }
            widget->setVisible(visible);
            if (auto *label = rowLabel(widget)) label->setVisible(visible);
        }

        // An item never passed to SetVisible() keeps the visibility it had when Apply() first saw it.
        bool wanted(QObject *object, bool current) {
            const QVariant value = object->property(kWantedProperty);
            if (value.isValid()) return value.toBool();
            object->setProperty(kWantedProperty, current);
            return current;
        }

        QAction *actionOf(QObject *object) {
            if (auto *menu = qobject_cast<QMenu *>(object)) return menu->menuAction();
            return qobject_cast<QAction *>(object);
        }

        // Ahead of a trailing spacer, so the page keeps packing its rows to the top.
        void appendSection(QLayout *layout, QWidget *section) {
            if (auto *box = qobject_cast<QBoxLayout *>(layout)) {
                int index = box->count();
                while (index > 0 && box->itemAt(index - 1)->spacerItem()) --index;
                box->insertWidget(index, section);
            } else if (auto *grid = qobject_cast<QGridLayout *>(layout)) {
                grid->addWidget(section, grid->rowCount(), 0, 1, qMax(1, grid->columnCount()));
            } else if (auto *form = qobject_cast<QFormLayout *>(layout)) {
                form->addRow(section);
            }
        }

        QWidget *sectionOf(QWidget *page, QTabWidget *tabs) {
            auto *box = new QGroupBox(tabs->tabText(tabs->indexOf(page)));
            tabs->removeTab(tabs->indexOf(page));
            // A page's spacer would otherwise make the box share the target page's spare height.
            box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
            if (page->layout()) page->layout()->setContentsMargins(0, 0, 0, 0);
            (new QVBoxLayout(box))->addWidget(page);
            page->show();
            return box;
        }

        bool hasShownContent(const QWidget *page) {
            const auto children = page->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
            return std::any_of(children.cbegin(), children.cend(), [](const QWidget *child) { return !child->isHidden(); });
        }
    }

    bool Simple() {
        return Configs::dataManager != nullptr && Configs::dataManager->settingsRepo->simple_mode;
    }

    bool IsAdvanced(const QObject *object) {
        if (tagged(object)) return true;
        auto *widget = qobject_cast<const QWidget *>(object);
        if (!widget) return false;
        for (const QWidget *parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
            if (tagged(parent)) return true;
        }
        auto *label = qobject_cast<const QLabel *>(widget);
        return label && tagged(rowField(label));
    }

    bool Allows(const QObject *object) {
        return !Simple() || !IsAdvanced(object);
    }

    void MarkAdvanced(QObject *object) {
        object->setProperty(kLevelProperty, QStringLiteral("advanced"));
    }

    void Unmark(QObject *object) {
        object->setProperty(kLevelProperty, QVariant());
        object->setProperty(kFoldProperty, QVariant());
    }

    QString FoldTarget(const QObject *object) {
        return object->property(kFoldProperty).toString();
    }

    void Fold(QWidget *root) {
        if (!Simple()) return;
        // In tab order, so the sections keep the order of the tabs they came from.
        QList<QWidget *> folds;
        for (auto *tabs : root->findChildren<QTabWidget *>()) {
            for (int i = 0; i < tabs->count(); ++i) {
                QWidget *page = tabs->widget(i);
                auto inPage = page->findChildren<QWidget *>();
                inPage.prepend(page);
                for (QWidget *widget : std::as_const(inPage)) {
                    if (!FoldTarget(widget).isEmpty() && !folds.contains(widget)) folds.append(widget);
                }
            }
        }
        for (QWidget *widget : std::as_const(folds)) {
            const QString into = FoldTarget(widget);
            auto *target = root->findChild<QWidget *>(into);
            if (target == nullptr || target->layout() == nullptr) continue;
            if (auto *tabs = tabsOf(widget)) {
                if (tabs->isTabVisible(tabs->indexOf(widget))) appendSection(target->layout(), sectionOf(widget, tabs));
            } else if (!widget->isHidden()) {
                // Taken out first: adding a widget its old layout still holds makes Qt warn on every open.
                if (auto *layout = layoutOf(widget)) layout->removeWidget(widget);
                appendSection(target->layout(), widget);
            }
        }
        for (auto *tabs : root->findChildren<QTabWidget *>()) {
            for (int i = 0; i < tabs->count(); ++i) {
                if (tabs->isTabVisible(i) && !hasShownContent(tabs->widget(i))) tabs->setTabVisible(i, false);
            }
        }
    }

    void Apply(QWidget *root) {
        const bool simple = Simple();
        for (QObject *object : root->findChildren<QObject *>()) {
            if (!tagged(object)) continue;
            if (auto *action = actionOf(object)) {
                action->setVisible(wanted(action, action->isVisible()) && !simple);
            } else if (auto *widget = qobject_cast<QWidget *>(object)) {
                place(widget, wanted(widget, shown(widget)) && !simple);
            }
        }
    }

    void SetVisible(QWidget *widget, bool visible) {
        widget->setProperty(kWantedProperty, visible);
        place(widget, visible && !(Simple() && tagged(widget)));
    }

    void SetVisible(QAction *action, bool visible) {
        action->setProperty(kWantedProperty, visible);
        action->setVisible(visible && !(Simple() && (tagged(action) || tagged(action->menu()))));
    }
}
