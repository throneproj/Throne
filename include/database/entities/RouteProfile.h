#pragma once

#include "include/database/entities/RouteRule.h"
#include <QUrl>
#include <QJsonArray>

namespace Configs {
    const int INVALID_ID = -99999;

    enum simpleAction{direct, block, proxy};
    inline QString simpleActionToString(simpleAction action)
    {
        if (action == direct) return {"direct"};
        if (action == block) return {"block"};
        if (action == proxy) return {"proxy"};
        return {"invalid"};
    }

    class RouteProfile {
    public:
        int id = -1;
        QString name = "";
        QList<std::shared_ptr<RouteRule>> Rules;
        int defaultOutboundID = proxyID;

        RouteProfile() = default;

        RouteProfile(const RouteProfile& other);

        static QList<std::shared_ptr<RouteRule>> parseJsonArray(const QJsonArray& arr, QString* parseError);

        QJsonArray get_route_rules(bool forView = false, std::map<int, QString> outboundMap = {});

        static std::shared_ptr<RouteProfile> GetDefaultChain();

        std::shared_ptr<QList<int>> get_used_outbounds();

        std::shared_ptr<QStringList> get_used_rule_sets();

        QStringList get_direct_sites();

        QStringList get_direct_ips();

        QString GetSimpleRules(simpleAction action);

        QString UpdateSimpleRules(const QString& content, simpleAction action);
    private:
        static bool add_simple_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type);

        static bool add_simple_address_rule(const QString& content, const std::shared_ptr<RouteRule>& rule);

        static bool add_simple_process_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type);
    };
} // namespace Configs
