#pragma once

#include <QDialog>
#include <QList>
#include <QPair>
// dialog_manage_routes.h declares a QShortcut member and takes the class from here.
#include <QShortcut>

#include <memory>

#include "include/database/entities/RouteProfile.h"

class QCheckBox;
class QCloseEvent;
class QKeyEvent;
class QLabel;
class QuickListsView;
class RouteDnsEditor;
class RouteRulesPage;
class RouteTabBadge;

namespace Ui {
    class RouteItem;
}

namespace Validation {
    class Form;
}

class RouteItem : public QDialog {
    Q_OBJECT

public:
    explicit RouteItem(QWidget *parent = nullptr, const std::shared_ptr<Configs::RouteProfile>& routeChain = nullptr);
    ~RouteItem() override;

    std::shared_ptr<Configs::RouteProfile> chain;

public slots:
    void accept() override;

    // Asks before throwing away changes.
    void reject() override;

signals:
    void settingsChanged(std::shared_ptr<Configs::RouteProfile> routingChain);

protected:
    void closeEvent(QCloseEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

private:
    struct EndpointRow {
        int profileId = -1;
        bool innerHops = false;
        QWidget* widget = nullptr;
        QLabel* subtitle = nullptr;
        QCheckBox* toggle = nullptr;
    };

    Ui::RouteItem *ui;

    Validation::Form* form = nullptr;

    // Advanced mode shows every rule, Simple mode only the quick lists; exactly one of them exists.
    RouteRulesPage* rulesPage = nullptr;
    QuickListsView* quickListsView = nullptr;

    RouteDnsEditor* dnsEditor = nullptr;

    RouteTabBadge* rulesBadge = nullptr;
    RouteTabBadge* endpointsBadge = nullptr;
    RouteTabBadge* dnsBadge = nullptr;

    QList<QPair<QString, int>> endpointCandidates;

    QList<EndpointRow> endpointRows;

    // The state Cancel compares against.
    QString openedFingerprint;

    // Set once the user agreed to discard, so closing does not ask twice.
    bool discardConfirmed = false;

    void setupHeader();

    void setupRemoteSection();

    void fetchRemote(bool applyToChain);

    void setupEndpointsSection();

    void refreshEndpointCandidates();

    void addEndpointRow(int profileId, bool innerHops);

    void removeEndpointRow(int profileId);

    [[nodiscard]] QList<int> listedEndpointIDs() const;

    [[nodiscard]] QList<int> listedInnerHopEndpointIDs() const;

    void setEndpointRowInnerHops(int profileId, bool innerHops);

    // One endpointPreferredBy rule per listed endpoint and opened-up inner hop, keeping existing rules where they sit.
    void syncEndpointRules();

    void refreshEndpointSubtitles();

    void reloadRules();

    void updateBadges();

    void setBadge(QWidget* page, RouteTabBadge*& badge, const QString& text);

    void setDefaultOutbound(int outboundID);

    [[nodiscard]] int defaultOutbound() const;

    [[nodiscard]] QString fingerprint() const;

    // True when nothing changed or the user chose to discard.
    bool confirmDiscard();
};
