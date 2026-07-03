#pragma once

#include "include/database/entities/RouteRule.h"
#include <QUrl>
#include <QJsonArray>

namespace Configs {
    const int INVALID_ID = -99999;

    enum simpleAction{bypass, block, proxy, warpBypass};
    inline QString simpleActionToString(simpleAction action)
    {
        if (action == bypass) return {"direct"};
        if (action == block) return {"block"};
        if (action == proxy) return {"proxy"};
        if (action == warpBypass) return {"warp-bypass"};
        return {"invalid"};
    }

    class RouteProfile {
    public:
        int id = -1;
        QString name = "";
        QList<std::shared_ptr<RouteRule>> Rules;
        int defaultOutboundID = proxyID;

        // Raw profiles carry a full sing-box `route` JSON object (as text) instead of
        // structured Rules. When preventModifications is set we use it verbatim (after
        // outbound-id translation); otherwise Throne still injects its internal plumbing.
        bool isRaw = false;
        QString rawRoute = "";
        bool preventModifications = false;

        // Remote profiles fetch their rules from a URL (content may be a throne://route deep
        // link, its base64, or the JSON share object). The profile is a normal *structured*
        // profile locally and stays user-editable; a manual/auto update re-fetches from
        // remoteURL and overwrites the rules (the local name is kept). Raw remote profiles
        // are intentionally unsupported for now.
        bool isRemote = false;
        QString remoteURL = "";
        bool autoUpdate = false;
        qint64 remoteLastUpdate = 0; // epoch seconds of the last successful remote fetch

        RouteProfile() = default;

        RouteProfile(const RouteProfile& other);

        static QList<std::shared_ptr<RouteRule>> parseJsonArray(const QJsonArray& arr, QString* parseError, QString* warnings = nullptr);

        QJsonArray get_route_rules(bool forView = false, std::map<int, QString> outboundMap = {});

        // Lossless share schema: a tagged JSON object carrying the profile name, default
        // outbound and every rule (with its simple/advanced type).
        QJsonObject ToShareObject();
        // ToShareObject() compacted, base64url-encoded, wrapped as throne://route?data=<...>
        QString ToShareLink();
        // Parse any shared form: a throne://route link, a base64 blob, a raw share object,
        // or a legacy bare rule array. Returns nullptr and fills *fatalError on failure;
        // non-fatal notes (e.g. outbound fallbacks) go to *warnings. *wasOldArray is set
        // true when the input was a legacy array (no name / default outbound to import).
        static std::shared_ptr<RouteProfile> FromShareInput(const QString& input, QString* fatalError, QString* warnings, bool* wasOldArray);

        // Parse a throne://remoteRoute?data=<...> deep link into unsaved remote route profiles
        // (id=-1, isRemote, remoteURL, autoUpdate, name defaulting to the URL host). *wasRemoteRouteLink
        // is set true when the input is a remoteRoute link at all (even if its payload is invalid);
        // on a bad payload the list is empty and *error explains why. Returns {} with
        // *wasRemoteRouteLink=false when the input isn't a remoteRoute link, so callers can fall
        // through to other formats.
        static QList<std::shared_ptr<RouteProfile>> FromRemoteRoutesLink(const QString& input, bool* wasRemoteRouteLink, QString* error);

        // Raw-profile helpers: recursively collect referenced outbound ids (from `outbound`
        // and top-level `final` fields) and translate those numeric ids to sing-box tags.
        static QList<int> CollectRawOutboundIds(const QJsonObject& route);
        static QJsonObject TranslateRawOutbounds(const QJsonObject& route, const std::map<int, QString>& outboundMap);

        static std::shared_ptr<RouteProfile> GetDefaultChain();

        std::shared_ptr<QList<int>> get_used_outbounds();

        std::shared_ptr<QStringList> get_used_rule_sets();

        QStringList get_direct_sites();

        QStringList get_proxy_sites();

        QStringList get_direct_ips();

        bool IsEmpty();

        void ResetRules();

        void ResetSimpleRule(ruleType type);

        QString GetSimpleRules(simpleAction action);

        QString UpdateSimpleRules(const QString& content, simpleAction action);

        void FilterEmptyRules();
    private:
        static bool add_simple_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type);

        static bool add_simple_address_rule(const QString& content, const std::shared_ptr<RouteRule>& rule);

        static bool add_simple_process_rule(const QString& content, const std::shared_ptr<RouteRule>& rule);

        std::shared_ptr<RouteRule> get_simple_rule_by_type(ruleType type);

        static ruleType get_rule_type(const QString& content, simpleAction action);

        static QList<std::shared_ptr<RouteRule>> get_simple_rules();

        static void reset_simple_rule(std::shared_ptr<RouteRule>& rule);
    };
} // namespace Configs
