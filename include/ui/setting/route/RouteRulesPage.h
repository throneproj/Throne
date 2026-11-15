#pragma once

#include <QWidget>

#include <memory>

#include "include/database/entities/RouteProfile.h"
#include "include/ui/utils/ValidationForm.h"

class EndpointRulePanel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QuickListEditor;
class RouteRuleEditor;
class RouteRuleList;

// The Rules tab: every rule in the order sing-box checks them, with the selected row's editor beside the list.
class RouteRulesPage : public QWidget {
    Q_OBJECT

public:
    // Edits profile->Rules in place: profile is the dialog's working copy.
    explicit RouteRulesPage(std::shared_ptr<Configs::RouteProfile> profile, QWidget* parent = nullptr);
    ~RouteRulesPage() override;

    // After something else replaced or re-synced profile->Rules; keeps the selection when its rule survived.
    void reload();

    // The route the "Everything else" row shows.
    void setDefaultOutbound(int outboundID);

    void selectRule(const std::shared_ptr<Configs::RouteRule>& rule);

    // Every row's problems for the dialog's Form; each reveal selects the row and returns the widget to focus.
    QList<Validation::FormIssue> issues();

signals:
    // Rules added, removed, moved, turned on or off, or edited.
    void changed();
    void openEndpointsRequested();
    // Delete on an endpoint row, confirmed: the dialog removes that endpoint, which drops its rule.
    void endpointRemovalRequested(int profileId);
    // Delete on an inner hop's rule, confirmed: the dialog stops routing to that endpoint's inner hops.
    void innerHopsOffRequested(int ownerProfileId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    [[nodiscard]] std::shared_ptr<Configs::RouteRule> firstRuleOf(int row) const;
    // The row holding rule, which need not be its first one.
    [[nodiscard]] int rowOf(const std::shared_ptr<Configs::RouteRule>& rule) const;
    // Rows from profile->Rules again, then the row holding keep is selected.
    void rebuild(const std::shared_ptr<Configs::RouteRule>& keep, bool recompute);
    void onRowSelected(int row);
    void showPage(QWidget* page);
    void fitDetailWidth();
    void updateActions();
    void rowEdited(int row);
    void scheduleQuickListRebuild();

    void addRule();
    void duplicateRow(int row);
    void deleteRow(int row);
    void deleteRule(int row);
    void clearQuickList(int row);
    void removeEndpoint(int row);
    void moveRow(int from, int to);
    [[nodiscard]] bool rowDisabled(int row) const;
    void setRowDisabled(int row, bool disabled);
    void showRowMenu(int row, const QPoint& globalPos);
    // The first rule of the nearest shown row after row, else before it.
    [[nodiscard]] std::shared_ptr<Configs::RouteRule> neighbourOf(int row) const;

    std::shared_ptr<Configs::RouteProfile> profile;
    // The selected row's first rule: rows are rebuilt, rules keep their identity.
    std::shared_ptr<Configs::RouteRule> selected;

    QPushButton* newButton = nullptr;
    QPushButton* duplicateButton = nullptr;
    QPushButton* deleteButton = nullptr;
    QLineEdit* filterEdit = nullptr;
    RouteRuleList* list = nullptr;
    QScrollArea* detailScroll = nullptr;
    QStackedWidget* stack = nullptr;
    RouteRuleEditor* ruleEditor = nullptr;
    QuickListEditor* quickListEditor = nullptr;
    EndpointRulePanel* endpointPanel = nullptr;
    QWidget* placeholder = nullptr;
    bool quickListRebuildQueued = false;
};
