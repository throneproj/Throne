#pragma once

#include <QWidget>
#include <QListWidgetItem>
#include <QDialog>
#include <QEvent>
#include <QHash>
#include <QPointer>
#include <QShortcut>

#include "3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.hpp"
#include "ui_RouteItem.h"
#include "include/database/entities/RouteProfile.h"
#include "include/ui/utils/ValidationForm.h"

class QListWidget;
class QStandardItemModel;
class RouteDnsEditor;

class RouteItem : public QDialog {
    Q_OBJECT

public:
    explicit RouteItem(QWidget *parent = nullptr, const std::shared_ptr<Configs::RouteProfile>& routeChain = nullptr);
    ~RouteItem() override;

    std::shared_ptr<Configs::RouteProfile> chain;
signals:
    void settingsChanged(std::shared_ptr<Configs::RouteProfile> routingChain);

private:
    Ui::RouteItem *ui;
    int currentIndex = -1;

    int lastNum = 0;

    QStringList geo_items;

    QShortcut* deleteShortcut;

    // Shared by every rule's outbound combo: refilling and re-measuring thousands of profiles froze each rule switch.
    QStandardItemModel* outboundsModel = nullptr;

    int outboundsPopupWidth = -1;

    AutoCompleteTextEdit* simpleDirect;

    AutoCompleteTextEdit* simpleBlock;

    AutoCompleteTextEdit* simpleProxy;

    AutoCompleteTextEdit* simpleWarpBypass;

    QListWidget* ruleAttrPlusList = nullptr;

    QWidget* lastTabPage = nullptr;

    RouteDnsEditor* dnsEditor = nullptr;

    Validation::Form* form = nullptr;

    // Owns the selected rule's Form registrations that live outside its attribute pages; replaced with the pages.
    QObject* ruleScope = nullptr;

    QHash<QString, QPointer<QWidget>> attrEditors;

    // The rule the attribute pages were last built for.
    std::shared_ptr<Configs::RouteRule> shownRule;

    // Worst issue severity per rule, kept so rebuilding the list does not re-validate every rule.
    QHash<const Configs::RouteRule*, int> ruleMarks;

    bool ruleCheckQueued = false;

    // Basic-tab lines that could not be added, put back when the editors are refilled from the rules.
    QHash<int, QStringList> rejectedSimpleLines;

    // The simple rules as the Basic tab last left them; unchanged means the editors still match.
    QStringList simpleTextsAtLeave;

    [[nodiscard]] std::shared_ptr<Configs::RouteRule> currentRule() const;

    [[nodiscard]] QList<QPair<AutoCompleteTextEdit*, Configs::simpleAction>> simpleEditors() const;

    [[nodiscard]] QStringList simpleRuleTexts() const;

    void loadSimpleEditors();

    QString applySimpleEditors();

    [[nodiscard]] QWidget* attributeEditor(const QString& attr) const;

    void bindAttributeEditor(const QString& attr, QWidget* editor, QWidget* page);

    void bindCurrentRule();

    QList<Validation::FormIssue> otherRuleIssues();

    QWidget* revealRuleIssue(const std::shared_ptr<Configs::RouteRule>& rule, const QString& attr);

    void refreshIssueIcons();

    void applyRuleMarks();

    // Deferred so it runs after the Form handled the same focus-out or change.
    void scheduleRuleCheck();

    void setupRemoteSection();

    void fetchRemote(bool applyToChain);

    QList<QPair<QString, int>> endpointCandidates;

    void setupEndpointsSection();

    void refreshEndpointCandidates() const;

    void addEndpointRow(int profileId, bool innerHops);

    void removeEndpointRow(int profileId);

    [[nodiscard]] QList<int> listedEndpointIDs() const;

    [[nodiscard]] QList<int> listedInnerHopEndpointIDs() const;

    // The listed endpoint profileId is an opened-up inner hop of, or -1.
    [[nodiscard]] int innerHopOwner(int profileId) const;

    void setEndpointRowInnerHops(int profileId, bool innerHops);

    // One endpointPreferredBy rule per listed endpoint and opened-up inner hop, keeping existing rules where they sit.
    void syncEndpointRules();

    [[nodiscard]] bool currentRuleIsEndpoint() const;

    void applyRuleEditLock();

    void reloadRuleViewsFromChain();

    void ensurePlusTabBuiltOnce();

    void removeAllAttributeTabsExceptPlus();

    void syncPlusListCheckStatesFromRule();

    void persistCurrentRuleAttrTabLabel();

    void applyStoredRuleAttrTabSelection();

    void syncRuleActionCombo();

    void rebuildRuleAttributeTabs();

    [[nodiscard]] QWidget* makeAttributeEditorPage(const QString& attr);

    void updateRuleSection();

    void updateRulePreview();

    void updateRouteItemsView();

    void applyAttributeVisibilityChange(const QString& attr, bool visible);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void accept() override;

    void on_new_route_item_clicked();
    void on_moveup_route_item_clicked();
    void on_movedown_route_item_clicked();
    void on_delete_route_item_clicked();
};
