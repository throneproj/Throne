#pragma once

#include <QLayout>
#include <QLineEdit>
#include <QPointer>
#include <QToolButton>

#include <functional>
#include <utility>

#include "include/ui/utils/ValidationForm.h"

// A section folded under a flat arrow button, the route rule editor's disclosure look.
namespace Disclosure {
    // Starts collapsed; changed() runs after every toggle, e.g. so the dialog refits.
    inline void Setup(QToolButton *toggle, QWidget *body, std::function<void()> changed = {}) {
        toggle->setCheckable(true);
        toggle->setChecked(false);
        toggle->setAutoRaise(true);
        toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toggle->setArrowType(Qt::RightArrow);
        if (auto *layout = body->layout()) {
            auto margins = layout->contentsMargins();
            margins.setLeft(toggle->iconSize().width());
            layout->setContentsMargins(margins);
        }
        body->hide();
        QObject::connect(toggle, &QToolButton::toggled, body, [toggle, body, changed = std::move(changed)](bool open) {
            toggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
            body->setVisible(open);
            if (changed) changed();
        });
    }

    struct Check {
        QWidget *field;
        std::function<QString()> message;
    };

    // The rule the form binds to the field, run on its trimmed text.
    inline Check FieldCheck(QLineEdit *edit, Validation::Rule rule) {
        return {edit, [edit, rule = std::move(rule)] { return rule(edit->text().trimmed()); }};
    }

    // The form skips the fields of a collapsed body, so their errors are reported here; revealing one opens the section.
    inline void CheckCollapsed(Validation::Form &form, QToolButton *toggle, QWidget *body, QList<Check> checks, QObject *owner) {
        form.addIssueSource([toggle = QPointer<QToolButton>(toggle), body = QPointer<QWidget>(body), checks = std::move(checks)] {
            QList<Validation::FormIssue> issues;
            if (!toggle || !body || !body->isHidden()) return issues;
            for (const auto &check: checks) {
                // As in the form, a field hidden or disabled inside the section is not checked.
                bool usable = check.field->isEnabled();
                for (QWidget *p = check.field; usable && p != nullptr && p != body; p = p->parentWidget()) usable = !p->isHidden();
                if (!usable) continue;
                if (const QString message = check.message(); !message.isEmpty()) {
                    issues.append({Validation::Severity::Error, {}, message, check.field,
                                   [toggle, field = QPointer<QWidget>(check.field)]() -> QWidget * {
                                       if (toggle) toggle->setChecked(true);
                                       return field;
                                   }});
                }
            }
            return issues;
        }, owner);
    }
}
