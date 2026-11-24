#pragma once

#include <QDialog>
#include <QJsonObject>
#include <functional>
#include <memory>

#include "include/ui/widget/json/JsonValidator.h"

class QLabel;

namespace JsonEdit {
    class JsonCodeEdit;
    class JsonIssueList;

    // OpenEditor() returns an empty object when the document was cleared, the original one when cancelled.
    class JsonEditorDialog : public QDialog {
        Q_OBJECT

    public:
        explicit JsonEditorDialog(const QJsonObject& root, QWidget* parent = nullptr);

        void SetValidator(std::shared_ptr<Validator> validator);

        // Shown while the document is empty.
        void SetPlaceholderText(const QString& text);

        // Runs on OK with the document as typed; any returned line keeps the editor open.
        void SetAcceptCheck(std::function<QStringList(const QString& text)> check);

        QJsonObject OpenEditor();

    private:
        QJsonObject m_original;
        std::function<QStringList(const QString&)> m_acceptCheck;
        JsonCodeEdit* m_editor = nullptr;
        JsonIssueList* m_issues = nullptr;
        QLabel* m_status = nullptr;
    };
}
