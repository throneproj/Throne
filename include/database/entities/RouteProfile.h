#pragma once

#include "include/database/entities/RouteRule.h"
#include <QUrl>
#include <QJsonArray>
#include <QStringList>

namespace Configs {
    const int INVALID_ID = -99999;

    // Fixed underlying type so headers that only pass it around (mainwindow.h) can forward-declare it.
    enum simpleAction : int {bypass, block, proxy, warpBypass};
    inline QString simpleActionToString(simpleAction action)
    {
        if (action == bypass) return {"direct"};
        if (action == block) return {"block"};
        if (action == proxy) return {"proxy"};
        if (action == warpBypass) return {"warp-bypass"};
        return {"invalid"};
    }

    // Where a simple "prefix:value" line goes: one of the simple lists, or, with serverID set, that server's own rules.
    struct SimpleTarget {
        simpleAction action = proxy;
        int serverID = INVALID_ID;
        [[nodiscard]] bool isServer() const { return serverID >= 0; }
        bool operator==(const SimpleTarget&) const = default;
    };

    class RouteProfile {
    public:
        int id = -1;
        QString name = "";
        QList<std::shared_ptr<RouteRule>> Rules;
        int defaultOutboundID = proxyID;

        // rawRoute is a whole sing-box `route` JSON object; preventModifications uses it verbatim, otherwise Throne still injects its plumbing.
        bool isRaw = false;
        QString rawRoute = "";
        bool preventModifications = false;

        // Remote profiles stay structured and user-editable; an update re-fetches remoteURL and overwrites Rules, keeping the local name.
        bool isRemote = false;
        QString remoteURL = "";
        bool autoUpdate = false;
        qint64 remoteLastUpdate = 0; // epoch seconds

        // Profile ids of openvpn/openconnect profiles run alongside this routing profile.
        QList<int> endpointProfileIDs;

        // Subset of endpointProfileIDs whose inner endpoint hops are routable too.
        QList<int> innerHopEndpointIDs;

        RouteProfile() = default;

        RouteProfile(const RouteProfile& other);

        // A rule with a key the desktop cannot store is dropped into *warnings; an array left with no rule fails.
        static QList<std::shared_ptr<RouteRule>> parseJsonArray(const QJsonArray& arr, QString* parseError, QString* warnings = nullptr);

        QJsonArray get_route_rules(bool forView = false, std::map<int, QString> outboundMap = {});

        // Endpoints travel as whole configs with credentials cleared; one that cannot is skipped into *warnings.
        QJsonObject ToShareObject(QString* warnings = nullptr);
        // ToShareObject() compacted, base64url-encoded, wrapped as throne://route/<...>
        QString ToShareLink(QString* warnings = nullptr);
        // *wasOldArray = legacy bare rule array (no name / default outbound); materializeEndpoints=false matches shared endpoints without creating local ones.
        static std::shared_ptr<RouteProfile> FromShareInput(const QString& input, QString* fatalError, QString* warnings, bool* wasOldArray, bool materializeEndpoints = true);

        // *wasRemoteRouteLink is true even when the payload is invalid; a non-remoteRoute input returns {} with it false so callers can fall through.
        static QList<std::shared_ptr<RouteProfile>> FromRemoteRoutesLink(const QString& input, bool* wasRemoteRouteLink, QString* error);

        static QList<int> CollectRawOutboundIds(const QJsonObject& route);
        static QJsonObject TranslateRawOutbounds(const QJsonObject& route, const std::map<int, QString>& outboundMap);

        static std::shared_ptr<RouteProfile> GetDefaultChain();

        // The positional placeholder paired with an endpoint, correlated by type + outboundID.
        static std::shared_ptr<RouteRule> MakeEndpointRule(int endpointProfileID);

        // Each listed endpoint, followed by its opened-up inner hops.
        QList<int> endpointRuleTargets() const;

        // One endpointPreferredBy rule per endpointRuleTargets() entry; prunes orphans, appends missing. Raw: no-op.
        void SyncEndpointRules();

        std::shared_ptr<QList<int>> get_used_outbounds();

        std::shared_ptr<QStringList> get_used_rule_sets();

        QStringList get_direct_sites();

        QStringList get_proxy_sites();

        QStringList get_direct_ips();

        // CIDRs pulled away from a direct exit, so Tun knows which ranges it must carry; rule-sets aren't resolvable at build time and are left out.
        QStringList get_hijacked_ips();

        bool IsEmpty();

        void ResetRules();

        void ResetSimpleRule(ruleType type);

        QString GetSimpleRules(simpleAction action);

        // label names the Basic-tab box in the returned reasons.
        QString UpdateSimpleRules(const QString& content, simpleAction action, const QString& label = {});

        static RuleIssue CheckSimpleRule(const QString& line);

        // A server's own rules are custom route rules to it with nothing but address lists, or nothing but process lists,
        // so the editor shows them as ordinary rules. One the user built that way counts too.

        // Adds one "prefix:value" line to the matching simple rule (or server rule), creating it if the profile has none yet.
        bool AppendSimpleRule(const QString& rawRule, const SimpleTarget& target, QString* why = nullptr);

        // Whether that "prefix:value" line already sits in the target's rules.
        bool HasSimpleRule(const QString& rawRule, const SimpleTarget& target);

        // Drops that "prefix:value" line from the target's rules, and a rule it leaves empty along with it.
        bool RemoveSimpleRule(const QString& rawRule, const SimpleTarget& target);

        // The servers whose own rules hold that "prefix:value" line.
        QList<int> ServersHoldingSimpleRule(const QString& rawRule);

        // A line from another target's rules that already catches every host a suffix: or keyword: rawRule would,
        // and sits in a rule matched before the target's own, so rawRule there would never apply. Empty when there is none.
        QString CoveringSimpleRule(const QString& rawRule, const SimpleTarget& target, SimpleTarget* coveringTarget);

        void FilterEmptyRules();
    private:
        static QString add_simple_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type);

        static QString add_simple_address_rule(const QString& content, const std::shared_ptr<RouteRule>& rule);

        static QString add_simple_process_rule(const QString& content, const std::shared_ptr<RouteRule>& rule);

        // The list inside rule that a "prefix:value" line lives in, with the value split off into *value; nullptr for an unknown prefix.
        static QList<QString>* simple_rule_values(const QString& content, RouteRule& rule, QString* value);

        std::shared_ptr<RouteRule> get_simple_rule_by_type(ruleType type);

        // Whether rule is a server rule of the kind (address or process) that line content goes in.
        static bool is_server_rule_for(const RouteRule& rule, const QString& content);

        // The rules of target a line like content goes in, in list order.
        QList<std::shared_ptr<RouteRule>> rules_for(const QString& content, const SimpleTarget& target);

        static ruleType get_rule_type(const QString& content, simpleAction action);

        static QList<std::shared_ptr<RouteRule>> get_simple_rules();

        static void reset_simple_rule(std::shared_ptr<RouteRule>& rule);
    };
} // namespace Configs
