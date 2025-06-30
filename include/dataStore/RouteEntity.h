#pragma once

#include <include/configs/proxy/Preset.hpp>

#include "include/global/NekoGui.hpp"

namespace NekoGui {
    enum outboundID {proxyID=-1, directID=-2, blockID=-3, dnsOutID=-4};
    inline QString outboundIDToString(int id)
    {
        if (id == proxyID) return {"proxy"};
        if (id == directID) return {"direct"};
        if (id == blockID) return {"block"};
        if (id == dnsOutID) return {"dns"};
        return {"unknown"};
    }
    inline outboundID stringToOutboundID(const QString& out)
    {
        if (out == "proxy") return proxyID;
        if (out == "direct") return directID;
        if (out == "block") return blockID;
        if (out == "dns_out") return dnsOutID;
        return proxyID;
    }
    enum inputType {trufalse, select, text};
    const int IranBypassChainID = 111111111;
    const int ChinaBypassChainID = 222222222;
    enum ruleType {custom, simpleAddress, simpleProcessName, simpleProcessPath};
    enum simpleAction{direct, block, proxy};
    inline QString simpleActionToString(simpleAction action)
    {
        if (action == direct) return {"direct"};
        if (action == block) return {"block"};
        if (action == proxy) return {"proxy"};
        return {"invalid"};
    }

    inline QString ruleTypeToString(ruleType type)
    {
        if (type == custom) return {"custom"};
        if (type == simpleAddress) return {"Address"};
        if (type == simpleProcessName) return {"Process Name"};
        if (type == simpleProcessPath) return {"Process Path"};
        return {"invalid"};
    }

    class RouteRule : public JsonStore {
    public:
        RouteRule();

        RouteRule(const RouteRule& other);

        QString name = "";
        int type = custom;
        int simpleAction;
        QString ip_version;
        QString network;
        QString protocol;
        QList<QString> inbound;
        QList<QString> domain;
        QList<QString> domain_suffix;
        QList<QString> domain_keyword;
        QList<QString> domain_regex;
        QList<QString> source_ip_cidr;
        bool source_ip_is_private = false;
        QList<QString> ip_cidr;
        bool ip_is_private = false;
        QList<QString> source_port;
        QList<QString> source_port_range;
        QList<QString> port;
        QList<QString> port_range;
        QList<QString> process_name;
        QList<QString> process_path;
        QList<QString> process_path_regex;
        QList<QString> rule_set;
        bool invert = false;
        int outboundID = directID; // -1 is proxy -2 is direct -3 is block -4 is dns_out
        // since sing-box 1.11.0
        QString action = "route";

        // reject options
        QString rejectMethod;
        bool no_drop = false;

        // route options
        QString override_address;
        QString override_port;
        // TODO maybe add some of dial fields?

        // sniff options
        QStringList sniffers;
        bool sniffOverrideDest = false;

        // resolve options
        QString strategy;

        [[nodiscard]] QJsonObject get_rule_json(bool forView = false, const QString& outboundTag = "");
        static QStringList get_attributes();
        static inputType get_input_type(const QString& fieldName);
        static QStringList get_values_for_field(const QString& fieldName);
        static std::shared_ptr<RouteRule> get_processPath_direct_rule(QString processPath);
        QStringList get_current_value_string(const QString& fieldName);
        [[nodiscard]] QString get_current_value_bool(const QString& fieldName) const;
        void set_field_value(const QString& fieldName, const QStringList& value);
        [[nodiscard]] bool isEmpty();
    };

    class RoutingChain : public JsonStore {
    public:
        int id = -1;
        QString name = "";
        QList<std::shared_ptr<RouteRule>> Rules;
        QList<JsonStore*> castedRules;
        int defaultOutboundID = proxyID;

        RoutingChain();

        RoutingChain(const RoutingChain& other);

        static QList<std::shared_ptr<RouteRule>> parseJsonArray(const QJsonArray& arr, QString* parseError);

        bool Save() override;

        void FromJson(QJsonObject object);

        QJsonArray get_route_rules(bool forView = false, std::map<int, QString> outboundMap = {});

        bool isViewOnly() const;

        static std::shared_ptr<RoutingChain> GetDefaultChain();

        static std::shared_ptr<RoutingChain> GetIranDefaultChain();

        static std::shared_ptr<RoutingChain> GetChinaDefaultChain();

        std::shared_ptr<QList<int>> get_used_outbounds();

        std::shared_ptr<QStringList> get_used_rule_sets();

        QStringList get_direct_sites();

        QStringList get_direct_ips();

        QString GetSimpleRules(simpleAction action);

        QString UpdateSimpleRules(const QString& content, simpleAction action);
    private:
        static bool need_add_simple_rule_item(const QString& content, ruleType type);

        static bool add_simple_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type);

        static bool add_simple_address_rule(const QString& content, const std::shared_ptr<RouteRule>& rule);

        static bool add_simple_process_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type);
    };
} // namespace NekoGui