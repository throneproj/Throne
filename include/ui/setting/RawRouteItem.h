#pragma once

#include <QDialog>
#include <QHash>
#include <QPair>
#include <memory>

#include "include/database/entities/RouteProfile.h"
#include "include/ui/widget/json/JsonCodeEdit.h"

class QLineEdit;
class QCheckBox;
class QCompleter;
class QLabel;
class RouteDnsEditor;

namespace JsonEdit {
    class JsonIssueList;
}

namespace Validation {
    class Form;
}

class RawRouteEdit : public JsonEdit::JsonCodeEdit {
    Q_OBJECT

public:
    explicit RawRouteEdit(QWidget* parent = nullptr);
    void setOutboundItems(const QList<QPair<QString, QString>>& items);

protected:
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    void insertOutboundCompletion(const QString& completion);

private:
    [[nodiscard]] bool outboundContext(QString* partial) const;
    void updateCompleter();
    QCompleter* completer = nullptr;
    QHash<QString, QString> outboundIdByDisplay; // popup display text -> id to insert
};

class RawRouteItem : public QDialog {
    Q_OBJECT

public:
    explicit RawRouteItem(QWidget* parent = nullptr, const std::shared_ptr<Configs::RouteProfile>& routeChain = nullptr);

    std::shared_ptr<Configs::RouteProfile> chain;

signals:
    void settingsChanged(std::shared_ptr<Configs::RouteProfile> routingChain);

private slots:
    void accept() override;

private:
    QLineEdit* nameEdit = nullptr;
    RawRouteEdit* jsonEdit = nullptr;
    // Kept to re-run on save: the editor's own check is debounced, so its issues can lag the last keystroke.
    std::shared_ptr<JsonEdit::Validator> routeValidator;
    QCheckBox* preventCheck = nullptr;
    JsonEdit::JsonIssueList* issueList = nullptr;
    QLabel* validateLabel = nullptr;
    RouteDnsEditor* dnsEditor = nullptr;
    Validation::Form* dnsForm = nullptr;
};
