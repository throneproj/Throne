#pragma once

#include <QDialog>
#include <QHash>
#include <QMap>

#include "include/configs/RouteRuleOrigin.h"

namespace Engine {
    class ConnectionManager;
}

namespace libcore {
    struct TestRouteRequest;
    struct TestRouteResponse;
    struct TestRouteStep;
}

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTreeWidget;

class DialogRuleTester : public QDialog {
    Q_OBJECT

public:
    DialogRuleTester(Engine::ConnectionManager *connection, int preferredProfileId, QWidget *parent = nullptr);

    void setPreferredProfile(int profileId);

private:
    struct RuleNames {
        QList<Configs::RouteRuleOrigin> origins;
        QHash<QString, QString> outboundNames;
        QMap<QString, int> vpnEndpointProfiles;
        bool rawProfile = false;
        int routeId = -1;
        // The profile an offline config was built from; -1 for the running config.
        int builtWith = -1;
    };

    [[nodiscard]] bool isRunning() const;

    void updateHeader(bool running);

    void fillRouteCombo(int selectRouteId);

    void pickProcess();

    [[nodiscard]] bool buildRequest(libcore::TestRouteRequest &request, QString *error) const;

    void runTest();

    void testOffline(libcore::TestRouteRequest request);

    void send(const libcore::TestRouteRequest &request, const RuleNames &names, bool fallBackOffline);

    void finish(const libcore::TestRouteRequest &request, const RuleNames &names, bool fallBackOffline, bool rpcOK,
                const QString &coreError, const libcore::TestRouteResponse &reply);

    void showResult(const libcore::TestRouteResponse &reply, const RuleNames &names);

    void showMessage(const QString &text);

    void setBusy(bool busy);

    [[nodiscard]] QString profileName(int profileId) const;

    [[nodiscard]] QString outboundLabel(const QString &tag, const RuleNames &names) const;

    [[nodiscard]] QString originLabel(const Configs::RouteRuleOrigin &origin, const RuleNames &names) const;

    [[nodiscard]] QString ruleLabel(int index, const QString &ruleText, const RuleNames &names) const;

    [[nodiscard]] QString actionText(const libcore::TestRouteStep &step, const RuleNames &names) const;

    [[nodiscard]] QString detailsText(const libcore::TestRouteStep &step) const;

    Engine::ConnectionManager *m_connection = nullptr;
    int m_preferredProfileId = -1;
    bool m_busy = false;

    QLabel *m_scope = nullptr;
    QWidget *m_routeRow = nullptr;
    QComboBox *m_route = nullptr;
    QLabel *m_offlineHint = nullptr;

    QLineEdit *m_process = nullptr;
    QLineEdit *m_domain = nullptr;
    QLineEdit *m_ip = nullptr;
    QSpinBox *m_port = nullptr;
    QComboBox *m_network = nullptr;
    QComboBox *m_protocol = nullptr;
    QPushButton *m_test = nullptr;

    QLabel *m_headline = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_skipped = nullptr;
    QTreeWidget *m_steps = nullptr;
};
