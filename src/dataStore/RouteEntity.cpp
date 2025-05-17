#include <QJsonObject>
#include <QJsonArray>
#include "include/dataStore/RouteEntity.h"
#include "include/dataStore/Database.hpp"
#include "include/configs/proxy/Preset.hpp"
#include <iostream>

namespace NekoGui {
    QJsonArray get_as_array(const QList<QString>& str, bool castToNum = false) {
        QJsonArray res;
        for (const auto &item: str) {
            if (castToNum) res.append(item.toInt());
            else res.append(item);
        }
        return res;
    }

    bool isValidStrArray(const QStringList& arr) {
        for (const auto& item: arr) {
            if (!item.trimmed().isEmpty()) return true;
        }
        return false;
    }

    RouteRule::RouteRule(const RouteRule& other) {
        name = other.name;
        ip_version = other.ip_version;
        network = other.network;
        protocol = other.protocol;
        inbound << other.inbound;
        domain << other.domain;
        domain_suffix << other.domain_suffix;
        domain_keyword << other.domain_keyword;
        domain_regex << other.domain_regex;
        source_ip_cidr << other.source_ip_cidr;
        source_ip_is_private = other.source_ip_is_private;
        ip_cidr << other.ip_cidr;
        ip_is_private = other.ip_is_private;
        source_port << other.source_port;
        source_port_range << other.source_port_range;
        port << other.port;
        port_range << other.port_range;
        process_name << other.process_name;
        process_path << other.process_path;
        process_path_regex << other.process_path_regex;
        rule_set << other.rule_set;
        invert = other.invert;
        outboundID = other.outboundID;
        action = other.action;
        rejectMethod = other.rejectMethod;
        no_drop = other.no_drop;
        override_address = other.override_address;
        override_port = other.override_port;
        sniffers << other.sniffers;
        sniffOverrideDest = other.sniffOverrideDest;
        strategy = other.strategy;
        type = other.type;
        simpleAction = other.simpleAction;

        _add(new configItem("name", &name, itemType::string));
        _add(new configItem("ip_version", &ip_version, itemType::string));
        _add(new configItem("network", &network, itemType::string));
        _add(new configItem("protocol", &protocol, itemType::string));
        _add(new configItem("inbound", &inbound, itemType::stringList));
        _add(new configItem("domain", &domain, itemType::stringList));
        _add(new configItem("domain_suffix", &domain_suffix, itemType::stringList));
        _add(new configItem("domain_keyword", &domain_keyword, itemType::stringList));
        _add(new configItem("domain_regex", &domain_regex, itemType::stringList));
        _add(new configItem("source_ip_cidr", &source_ip_cidr, itemType::stringList));
        _add(new configItem("source_ip_is_private", &source_ip_is_private, itemType::boolean));
        _add(new configItem("ip_cidr", &ip_cidr, itemType::stringList));
        _add(new configItem("ip_is_private", &ip_is_private, itemType::boolean));
        _add(new configItem("source_port", &source_port, itemType::stringList));
        _add(new configItem("source_port_range", &source_port_range, itemType::stringList));
        _add(new configItem("port", &port, itemType::stringList));
        _add(new configItem("port_range", &port_range, itemType::stringList));
        _add(new configItem("process_name", &process_name, itemType::stringList));
        _add(new configItem("process_path", &process_path, itemType::stringList));
        _add(new configItem("process_path_regex", &process_path_regex, itemType::stringList));
        _add(new configItem("rule_set", &rule_set, itemType::stringList));
        _add(new configItem("invert", &invert, itemType::boolean));
        _add(new configItem("outboundID", &outboundID, itemType::integer));
        _add(new configItem("actionType", &action, itemType::string));
        _add(new configItem("rejectMethod", &rejectMethod, itemType::string));
        _add(new configItem("noDrop", &no_drop, itemType::boolean));
        _add(new configItem("override_address", &override_address, itemType::string));
        _add(new configItem("override_port", &override_port, itemType::integer));
        _add(new configItem("sniffers", &sniffers, itemType::stringList));
        _add(new configItem("sniffOverrideDest", &sniffOverrideDest, itemType::boolean));
        _add(new configItem("strategy", &strategy, itemType::string));
        _add(new configItem("type", &type, itemType::integer));
        _add(new configItem("simple_action", &simpleAction, itemType::integer));
    }

    QJsonObject RouteRule::get_rule_json(bool forView, const QString& outboundTag) {
        QJsonObject obj;

        if (!ip_version.isEmpty()) obj["ip_version"] = ip_version.toInt();
        if (!network.isEmpty()) obj["network"] = network;
        if (!protocol.isEmpty()) obj["protocol"] = protocol;
        if (isValidStrArray(inbound)) obj["inbound"] = get_as_array(inbound);
        if (isValidStrArray(domain)) obj["domain"] = get_as_array(domain);
        if (isValidStrArray(domain_suffix)) obj["domain_suffix"] = get_as_array(domain_suffix);
        if (isValidStrArray(domain_keyword)) obj["domain_keyword"] = get_as_array(domain_keyword);
        if (isValidStrArray(domain_regex)) obj["domain_regex"] = get_as_array(domain_regex);
        if (isValidStrArray(source_ip_cidr)) obj["source_ip_cidr"] = get_as_array(source_ip_cidr);
        if (source_ip_is_private) obj["source_ip_is_private"] = source_ip_is_private;
        if (isValidStrArray(ip_cidr)) obj["ip_cidr"] = get_as_array(ip_cidr);
        if (ip_is_private) obj["ip_is_private"] = ip_is_private;
        if (isValidStrArray(source_port)) obj["source_port"] = get_as_array(source_port, true);
        if (isValidStrArray(source_port_range)) obj["source_port_range"] = get_as_array(source_port_range);
        if (isValidStrArray(port)) obj["port"] = get_as_array(port, true);
        if (isValidStrArray(port_range)) obj["port_range"] = get_as_array(port_range);
        if (isValidStrArray(process_name)) obj["process_name"] = get_as_array(process_name);
        if (isValidStrArray(process_path)) obj["process_path"] = get_as_array(process_path);
        if (isValidStrArray(process_path_regex)) obj["process_path_regex"] = get_as_array(process_path_regex);
        if (isValidStrArray(rule_set)) obj["rule_set"] = get_as_array(rule_set);
        if (invert) obj["invert"] = invert;
        // fix action type
        if (action == "route")
        {
            if (outboundID == -3) action = "reject";
            if (outboundID == -4) action = "hijack-dns";
        }
        obj["action"] = action;

        if (action == "reject")
        {
            if (!rejectMethod.isEmpty()) obj["reject_method"] = rejectMethod;
            if (no_drop) obj["no_drop"] = no_drop;
        }
        if (action == "route" || action == "route-options")
        {
            if (!override_address.isEmpty()) obj["override_address"] = override_address;
            if (override_port.toInt() > 0) obj["override_port"] = override_port.toInt();

            if (action == "route")
            {
                if (forView) {
                    switch (outboundID) { // TODO use constants
                    case -1:
                        obj["outbound"] = "proxy";
                        break;
                    case -2:
                        obj["outbound"] = "direct";
                        break;
                    default:
                        auto prof = NekoGui::profileManager->GetProfile(outboundID);
                        if (prof == nullptr) {
                            MW_show_log("The outbound described in the rule chain is missing, maybe your data is corrupted");
                            return {};
                        }
                        obj["outbound"] = prof->bean->DisplayName();
                    }
                } else {
                    if (!outboundTag.isEmpty()) obj["outbound"] = outboundTag;
                    else obj["outbound"] = outboundID;
                }
            }
        }
        if (action == "sniff")
        {
            //if (isValidStrArray(sniffers)) obj["sniffers"] = get_as_array(sniffers); TODO maybe allow customization?
            if (sniffOverrideDest) obj["override_destination"] = sniffOverrideDest;
        }
        if (action == "resolve")
        {
            if (!strategy.isEmpty()) obj["strategy"] = strategy;
        }

        return obj;
    }

    // TODO use constant for field names
    QStringList RouteRule::get_attributes()
    {
        return {
            "ip_version",
            "network",
            "protocol",
            "inbound",
            "domain",
            "domain_suffix",
            "domain_keyword",
            "domain_regex",
            "source_ip_cidr",
            "source_ip_is_private",
            "ip_cidr",
            "ip_is_private",
            "source_port",
            "source_port_range",
            "port",
            "port_range",
            "process_name",
            "process_path",
            "process_path_regex",
            "rule_set",
            "invert",
            "action",
            "outbound",
            "override_address",
            "override_port",
            "method",
            "no_drop",
            "override_destination",
            "strategy",
    };
    }

    inputType RouteRule::get_input_type(const QString& fieldName) {
        if (fieldName == "invert" ||
            fieldName == "source_ip_is_private" ||
            fieldName == "ip_is_private" ||
            fieldName == "no_drop" ||
            fieldName == "override_destination") return trufalse;

        if (fieldName == "ip_version" ||
            fieldName == "network" ||
            fieldName == "protocol" ||
            fieldName == "action" ||
            fieldName == "method" ||
            fieldName == "strategy" ||
            fieldName == "outbound") return select;

        return text;
    }

    QStringList RouteRule::get_values_for_field(const QString& fieldName) {
        if (fieldName == "ip_version") {
            return {"", "4", "6"};
        }
        if (fieldName == "network") {
            return {"", "tcp", "udp"};
        }
        if (fieldName == "protocol") {
            auto resp = Preset::SingBox::SniffProtocols;
            resp.prepend("");
            return resp;
        }
        if (fieldName == "action")
        {
            return Preset::SingBox::ActionTypes;
        }
        if (fieldName == "method")
        {
            auto resp = Preset::SingBox::rejectMethods;
            resp.prepend("");
            return resp;
        }
        if (fieldName == "strategy")
        {
            auto resp = Preset::SingBox::DomainStrategy;
            resp.prepend("");
            return resp;
        }
        return {};
    }

    QStringList RouteRule::get_current_value_string(const QString& fieldName) {
        if (fieldName == "ip_version") {
            return {ip_version};
        }
        if (fieldName == "network") {
            return {network};
        }
        if (fieldName == "protocol") {
            return {protocol};
        }
        if (fieldName == "action")
        {
            return {action};
        }
        if (fieldName == "method")
        {
            return {rejectMethod};
        }
        if (fieldName == "strategy")
        {
            return {strategy};
        }
        if (fieldName == "override_address")
        {
            return {override_address};
        }
        if (fieldName == "override_port")
        {
            return {override_port};
        }
        if (fieldName == "outbound")
        {
            return {Int2String(outboundID)};
        }
        if (fieldName == "inbound") return inbound;
        if (fieldName == "domain") return domain;
        if (fieldName == "domain_suffix") return domain_suffix;
        if (fieldName == "domain_keyword") return domain_keyword;
        if (fieldName == "domain_regex") return domain_regex;
        if (fieldName == "source_ip_cidr") return source_ip_cidr;
        if (fieldName == "ip_cidr") return ip_cidr;
        if (fieldName == "source_port") return source_port;
        if (fieldName == "source_port_range") return source_port_range;
        if (fieldName == "port") return port;
        if (fieldName == "port_range") return port_range;
        if (fieldName == "process_name") return process_name;
        if (fieldName == "process_path") return process_path;
        if (fieldName == "process_path_regex") return process_path_regex;
        if (fieldName == "rule_set") return rule_set;
        return {};
    }

    QString RouteRule::get_current_value_bool(const QString& fieldName) const {
        if (fieldName == "source_ip_is_private") {
            return source_ip_is_private? "true":"false";
        }
        if (fieldName == "ip_is_private") {
            return ip_is_private? "true":"false";
        }
        if (fieldName == "invert") {
            return invert? "true":"false";
        }
        if (fieldName == "no_drop")
        {
            return no_drop? "true":"false";
        }
        if (fieldName == "override_destination")
        {
            return sniffOverrideDest? "true":"false";
        }
        return nullptr;
    }

    QStringList filterEmpty(const QStringList& base) {
        QStringList res;
        for (const auto& item: base) {
            if (item.trimmed().isEmpty()) continue;
            res << item.trimmed();
        }
        return res;
    }

    void RouteRule::set_field_value(const QString& fieldName, const QStringList& value) {
        if (fieldName == "ip_version") {
            ip_version = value[0];
        }
        if (fieldName == "network") {
            network = value[0];
        }
        if (fieldName == "protocol") {
            protocol = value[0];
        }
        if (fieldName == "inbound") {
            inbound = filterEmpty(value);
        }
        if (fieldName == "domain") {
            domain = filterEmpty(value);
        }
        if (fieldName == "domain_suffix") {
            domain_suffix = filterEmpty(value);
        }
        if (fieldName == "domain_keyword") {
            domain_keyword = filterEmpty(value);
        }
        if (fieldName == "domain_regex") {
            domain_regex = filterEmpty(value);
        }
        if (fieldName == "source_ip_cidr") {
            source_ip_cidr = filterEmpty(value);
        }
        if (fieldName == "source_ip_is_private") {
            source_ip_is_private = value[0]=="true";
        }
        if (fieldName == "ip_cidr") {
            ip_cidr = filterEmpty(value);
        }
        if (fieldName == "ip_is_private") {
            ip_is_private = value[0]=="true";
        }
        if (fieldName == "source_port") {
            source_port = filterEmpty(value);
        }
        if (fieldName == "source_port_range") {
            source_port_range = filterEmpty(value);
        }
        if (fieldName == "port") {
            port = filterEmpty(value);
        }
        if (fieldName == "port_range") {
            port_range = filterEmpty(value);
        }
        if (fieldName == "process_name") {
            process_name = filterEmpty(value);
        }
        if (fieldName == "process_path") {
            process_path = filterEmpty(value);
        }
        if (fieldName == "process_path_regex") {
            process_path_regex = filterEmpty(value);
        }
        if (fieldName == "rule_set") {
            rule_set = filterEmpty(value);
        }
        if (fieldName == "invert") {
            invert = value[0]=="true";
        }
        if (fieldName == "action")
        {
            action = value[0];
        }
        if (fieldName == "method")
        {
            rejectMethod = value[0];
        }
        if (fieldName == "no_drop")
        {
            no_drop = value[0]=="true";
        }
        if (fieldName == "override_address")
        {
            override_address = value[0];
        }
        if (fieldName == "override_port")
        {
            override_port = value[0];
        }
        if (fieldName == "override_destination")
        {
            sniffOverrideDest = value[0]=="true";
        }
        if (fieldName == "strategy")
        {
            strategy = value[0];
        }
        if (fieldName == "outbound")
        {
            outboundID = value[0].toInt();
        }
    }

    bool RouteRule::isEmpty() {
        auto ruleJson = get_rule_json();
        if (action == "route" || action == "route-options" || action == "hijack-dns") return ruleJson.keys().length() <= 1;
        if (action == "sniff" || action == "resolve" || action == "reject") return ruleJson.keys().length() < 1;
        return false;
    }

    std::shared_ptr<RouteRule> RouteRule::get_processPath_direct_rule(QString processPath)
    {
        auto res = std::make_shared<RouteRule>();
        res->name = "AutoGenerated direct extra core";
        res->action = "route";
        res->outboundID = -2;
        res->process_path = {processPath};

        return res;
    }


    bool isOutboundIDValid(int id) {
        switch (id) {
            case -1:
                return true;
            case -2:
                return true;
            default:
                return profileManager->profiles.count(id) > 0;
        }
    }

    int getOutboundID(const QString& name) {
        if (name == "proxy") return -1;
        if (name == "direct") return -2;
        for (const auto& item: profileManager->profiles) {
            if (item.second->bean->name == name) return item.first;
        }

        return INVALID_ID;
    }

    QList<std::shared_ptr<RouteRule>> RoutingChain::parseJsonArray(const QJsonArray& arr, QString* parseError) {
        if (arr.empty()) {
            parseError->append("Input is not a valid json array");
            return {};
        }

        auto rules = QList<std::shared_ptr<RouteRule>>();
        auto ruleID = 1;
        for (const auto& item: arr) {
            if (!item.isObject()) {
                parseError->append(QString("expected array of json objects but have member of type '%1'").arg(item.type()));
                return {};
            }

            auto obj = item.toObject();
            auto rule = std::make_shared<RouteRule>();
            for (const auto& key: obj.keys()) {
                auto val = obj.value(key);
                if (key == "outbound") {
                    if (val.isDouble()) {
                        if (!isOutboundIDValid(val.toInt())) {
                            parseError->append(QString("outbound id %1 is not valid").arg(val.toInt()));
                            return {};
                        }
                        rule->outboundID = val.toInt();
                    } else if (val.isString()) {
                        auto id = getOutboundID(val.toString());
                        if (id == INVALID_ID) {
                            parseError->append(QString("outbound with name %1 does not exist").arg(val.toString()));
                            return {};
                        }
                        rule->outboundID = id;
                    }
                } else if (val.isArray()) {
                    rule->set_field_value(key, QJsonArray2QListString(val.toArray()));
                } else if (val.isString()) {
                    rule->set_field_value(key, {val.toString()});
                } else if (val.isBool()) {
                    rule->set_field_value(key, {val.toBool() ? "true":"false"});
                }
            }
            rule->name = "imported rule #" + Int2String(ruleID++);
            rules << rule;
        }

        return rules;
    }

    QJsonArray RoutingChain::get_route_rules(bool forView, std::map<int, QString> outboundMap) {
        QJsonArray res;
        for (const auto &item: Rules) {
            auto outboundTag = QString();
            if (outboundMap.count(item->outboundID)) outboundTag = outboundMap[item->outboundID];
            auto rule_json = item->get_rule_json(forView, outboundTag);
            if (rule_json.empty()) {
                MW_show_log("Aborted generating routing section, an error has occurred");
                return {};
            }
            res += rule_json;
        }

        return res;
    }

    bool RoutingChain::isViewOnly() const {
        return id == IranBypassChainID ||
        id == ChinaBypassChainID;
    }

    std::shared_ptr<RoutingChain> RoutingChain::GetDefaultChain() {
        auto defaultChain = std::make_shared<RoutingChain>();
        defaultChain->name = "Default";
        auto defaultRule = std::make_shared<RouteRule>();
        defaultRule->name = "Route DNS";
        defaultRule->action = "hijack-dns";
        defaultRule->protocol = "dns";
        defaultChain->Rules << defaultRule;
        return defaultChain;
    }

    std::shared_ptr<RoutingChain> RoutingChain::GetIranDefaultChain() {
        auto chain = std::make_shared<RoutingChain>();
        chain->name = "Bypass Iran";
        chain->id = IranBypassChainID;
        chain->save_control_no_save = true;

        auto rule0 = std::make_shared<RouteRule>();
        rule0->name = "Route DNS";
        rule0->action = "hijack-dns";
        rule0->protocol = "dns";
        chain->Rules << rule0;

        auto rule1 = std::make_shared<RouteRule>();
        rule1->rule_set << QString("ir_IP") << QString("category-ir_SITE");
        rule1->name = "Bypass Iran IPs and Domains";
        rule1->outboundID = -2;
        chain->Rules << rule1;

        auto rule2 = std::make_shared<RouteRule>();
        rule2->name = "Bypass Private IPs";
        rule2->ip_is_private = true;
        rule1->outboundID = -2;
        chain->Rules << rule2;

        return chain;
    }

    std::shared_ptr<RoutingChain> RoutingChain::GetChinaDefaultChain() {
        auto chain = std::make_shared<RoutingChain>();
        chain->name = "Bypass China";
        chain->id = ChinaBypassChainID;
        chain->save_control_no_save = true;

        auto rule0 = std::make_shared<RouteRule>();
        rule0->name = "Route DNS";
        rule0->action = "hijack-dns";
        rule0->protocol = "dns";
        chain->Rules << rule0;

        auto rule1 = std::make_shared<RouteRule>();
        rule1->name = "Bypass Chinese IPs and Domains";
        rule1->rule_set << QString("cn_IP") << QString("geolocation-cn_SITE") << QString("cn_SITE");
        rule1->outboundID = -2;
        chain->Rules << rule1;

        auto rule2 = std::make_shared<RouteRule>();
        rule2->name = "Bypass Private IPs";
        rule2->ip_is_private = true;
        rule1->outboundID = -2;
        chain->Rules << rule2;

        return chain;
    }

    std::shared_ptr<QList<int>> RoutingChain::get_used_outbounds() {
        auto res = std::make_shared<QList<int>>();
        for (const auto& item: Rules) {
            res->push_back(item->outboundID);
        }
        return res;
    }

    std::shared_ptr<QStringList> RoutingChain::get_used_rule_sets() {
        auto res = std::make_shared<QStringList>();
        for (const auto& item: Rules) {
            for (const auto& ruleItem: item->rule_set) {
                res->push_back(ruleItem);
            }
        }
        return res;
    }

    QStringList RoutingChain::get_direct_sites() {
        auto res = QStringList();
        for (const auto& item: Rules) {
            if (item->outboundID == -2) {
                for (const auto& rset: item->rule_set) {
                    if (rset.endsWith("_SITE")) res << QString("ruleset:" + rset);
                }
                for (const auto& domain: item->domain) {
                    res << QString("domain:" + domain);
                }
                for (const auto& suffix: item->domain_suffix) {
                    res << QString("suffix:" + suffix);
                }
                for (const auto& keyword: item->domain_keyword) {
                    res << QString("keyword:" + keyword);
                }
                for (const auto& regex: item->domain_regex) {
                    res << QString("regex:" + regex);
                }
            }
        }
        return res;
    }

    QStringList RoutingChain::get_direct_ips()
    {
        auto res = QStringList();
        for (const auto& item: Rules) {
            if (item->outboundID == -2) {
                for (const auto& rset: item->rule_set) {
                    if (rset.endsWith("_IP")) res << QString("ruleset:" + rset);
                }
                for (const auto& domain: item->ip_cidr) {
                    res << QString("ip:" + domain);
                }
            }
        }
        return res;
    }

    QString RoutingChain::GetSimpleRules(simpleAction action)
    {
        QString res;
        for (const auto& item: Rules)
        {
            if (item->type != custom && item->simpleAction == action)
            {
                for (const auto& domain : item->domain) res += QString("domain:" + domain + "\n");
                for (const auto& domain_suffix : item->domain_suffix) res += QString("suffix:" + domain_suffix + "\n");
                for (const auto& domain_keyword : item->domain_keyword) res += QString("keyword:" + domain_keyword + "\n");
                for (const auto& domain_regex : item->domain_regex) res += QString("regex:" + domain_regex + "\n");
                for (const auto& rule_set : item->rule_set) res += QString("ruleset:" + rule_set + "\n");
                for (const auto& ip_cidr : item->ip_cidr) res += QString("ip:" + ip_cidr + "\n");
                for (const auto& process_name : item->process_name) res += QString("processName:" + process_name + "\n");
                for (const auto& process_path : item->process_path) res += QString("processPath:" + process_path + "\n");
            }
        }
        return res;
    }


    QString RoutingChain::UpdateSimpleRules(const QString& content, simpleAction action)
    {
        QString res;
        auto items = content.split("\n");

        QList<std::shared_ptr<RouteRule>> toRemove;
        for (int idx=0;idx<Rules.length();idx++)
        {
            auto route_rule = Rules[idx];
            if (route_rule->type != custom && route_rule->simpleAction == action)
            {
                toRemove.append(route_rule);
            }
        }
        for (const auto& rule : toRemove)
        {
            Rules.removeOne(rule);
        }

        QSet<int> added_items;
        for (const auto type : {simpleAddress, simpleProcessName, simpleProcessPath})
        {
            auto rule = std::make_shared<RouteRule>();
            rule->name = QString("Simple ") + ruleTypeToString(type) + " -> " + simpleActionToString(action);
            rule->type = type;
            rule->simpleAction = action;

            if (action == block)
            {
                rule->action = "reject";
            } else if (action == direct)
            {
                rule->action = "route";
                rule->outboundID = -2;
            } else if (action == proxy)
            {
                rule->action = "route";
                rule->outboundID = -1;
            }
            auto ruleEmpty = true;
            for (int i=0;i<items.size();i++)
            {
                auto item = items[i];
                item = item.trimmed();
                if (item.isEmpty()) continue;
                if (add_simple_rule(item, rule, type))
                {
                    added_items.insert(i);
                    ruleEmpty = false;
                }
            }
            if (!ruleEmpty) Rules.append(rule);
        }
        for (int i=0;i<items.size();i++)
        {
            if (items[i].trimmed().isEmpty()) continue;
            if (!added_items.contains(i)) res += "Could not add " + items[i];
        }
        return res;
    }

    bool RoutingChain::add_simple_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type)
    {
        if (type == simpleAddress) return add_simple_address_rule(content, rule);
        else return add_simple_process_rule(content, rule, type);
    }

    bool RoutingChain::add_simple_address_rule(const QString& content, const std::shared_ptr<RouteRule>& rule)
    {
        auto sp = content.split(":");
        if (sp.size() != 2) return false;
        const QString& address = sp[1];
        const QString& subType = sp[0];
        if (subType == "domain") {
            if (!rule->domain.contains(address)) rule->domain.append(address);
            return true;
        } else if (subType == "suffix") {
            if (!rule->domain_suffix.contains(address)) rule->domain_suffix.append(address);
            return true;
        } else if (subType == "keyword") {
            if (!rule->domain_keyword.contains(address)) rule->domain_keyword.append(address);
            return true;
        } else if (subType == "regex") {
            if (!rule->domain_regex.contains(address)) rule->domain_regex.append(address);
            return true;
        } else if (subType == "ruleset") {
            if (!rule->rule_set.contains(address)) rule->rule_set.append(address);
            return true;
        } else if (subType == "ip") {
            if (!rule->ip_cidr.contains(address)) rule->ip_cidr.append(address);
            return true;
        } else {
            return false;
        }
    }

    bool RoutingChain::add_simple_process_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type)
    {
        auto sp = content.split(":");
        if (sp.size() != 2) return false;
        const QString& address = sp[1];
        const QString& subType = sp[0];
        if (subType == "processName" && type == simpleProcessName)
        {
            if (!rule->process_name.contains(address)) rule->process_name.append(address);
            return true;
        } else if (subType == "processPath" && type == simpleProcessPath)
        {
            if (!rule->process_path.contains(address)) rule->process_path.append(address);
            return true;
        } else
        {
            return false;
        }
    }

    RoutingChain::RoutingChain(const RoutingChain& other)  : JsonStore(other) {
        id = other.id;
        name = QString(other.name);
        for (const auto& item: other.Rules) {
            Rules.push_back(std::make_shared<RouteRule>(*item));
        }
        fn = QString(other.fn);

        _add(new configItem("id", &id, itemType::integer));
        _add(new configItem("name", &name, itemType::string));
        _add(new configItem("rules", &castedRules, itemType::jsonStoreList));
    }

    bool RoutingChain::Save() {
        castedRules.clear();
        for (const auto &item: Rules) {
            castedRules.push_back(dynamic_cast<JsonStore*>(item.get()));
        }
        return JsonStore::Save();
    }

    void RoutingChain::FromJson(QJsonObject object) {
        for (const auto &key: object.keys()) {
            if (_map.count(key) == 0) {
                continue;
            }

            auto value = object[key];
            auto item = _map[key].get();

            if (item == nullptr) continue;
            if (item->type == itemType::jsonStoreList) {
                // it is of rule type
                if (!value.isArray()) continue;
                Rules.clear();
                auto arr = value.toArray();
                for (auto obj : arr) {
                    auto rule = std::make_shared<RouteRule>();
                    rule->FromJson(obj.toObject());
                    Rules << rule;
                }
            }
        }
        JsonStore::FromJson(object);
    }
}