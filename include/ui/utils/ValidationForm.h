#pragma once

#include "include/global/Validation.hpp"

#include <QObject>
#include <QPointer>

#include <functional>
#include <memory>
#include <vector>

class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QPlainTextEdit;
class QWidget;

namespace Validation {
    struct FormIssue {
        Severity severity = Severity::Error;
        QString label;
        QString message;
        QPointer<QWidget> widget;
        // Model-level issues (e.g. a route rule): brings the editor into view and returns the widget to focus.
        std::function<QWidget *()> reveal;
    };

    // Checks bound widgets on focus-out (never per keystroke) and gates saves with validateAll().
    // Hidden, disabled and when()-inactive fields are skipped; Warning issues are shown but never block.
    class Form : public QObject {
    public:
        class Field {
        public:
            Field &label(const QString &text);
            Field &when(std::function<bool()> active);
            Field &warning();
            Field &raw();

        private:
            friend class Form;
            QString labelText;
            std::function<bool()> active;
            Severity severity = Severity::Error;
            bool trim = true;
        };

        // While any Pause lives, discrete-participant changes and focus-outs run nothing; the last one to end calls refresh().
        class Pause {
        public:
            explicit Pause(Form &form);
            ~Pause();
            Pause(const Pause &) = delete;
            Pause &operator=(const Pause &) = delete;

        private:
            QPointer<Form> target;
        };

        explicit Form(QWidget *scope);
        ~Form() override;

        // owner defaults to the widget; the entry is dropped when either is destroyed.
        Field &bind(QLineEdit *edit, Rule rule, QObject *owner = nullptr);
        Field &bind(QPlainTextEdit *edit, Rule rule, QObject *owner = nullptr);
        Field &bind(QComboBox *editableCombo, Rule rule, QObject *owner = nullptr);
        Field &bindLines(QPlainTextEdit *edit, Rule perLine, QObject *owner = nullptr);

        // Re-run when a participant loses focus, or a check box, combo or unfocused spin box participant changes.
        void addRule(const QList<QWidget *> &participants, std::function<QString()> check, QObject *owner = nullptr,
                     Severity severity = Severity::Error, const QString &label = {});

        void addIssueSource(std::function<QList<FormIssue>()> source, QObject *owner = nullptr);

        // One elided line beside the buttons showing the latest focus-out problem; no layout growth.
        void attachStatus(QDialogButtonBox *buttons);

        // Inline marks for everything currently invalid, without a message box (call after loading data).
        void markAll();

        // Save gate: marks every issue, reveals and focuses the first error, shows one summary box.
        bool validateAll();

        QList<FormIssue> issues();

        void revalidate(QWidget *widget);

        // Re-checks only the currently marked fields (cheap): call after changing what is shown, enabled or when()-active.
        void refresh();

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

    private:
        struct Private;
        std::unique_ptr<Private> d;
    };
}
