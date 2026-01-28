#include <QJsonObject>
#include <QJsonArray>
#include "include/database/entities/RouteRule.h"


#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"

namespace Configs {
    QJsonArray get_as_array(const QList<QString>& str, bool castToNum = false, const std::function<QString(QString)>& converter = nullptr) {
        QJsonArray res;
        for (const auto &item: str) {
            auto conv = converter ? converter(item) : item;
            if (castToNum) res.append(conv.toInt());
            else res.append(conv);
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
        if (isValidStrArray(rule_set))
            if (forView)
                obj["rule_set"] = get_as_array(rule_set);
            else
                obj["rule_set"] = get_as_array(rule_set, false, get_rule_set_name);
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
                        auto prof = Configs::dataManager->profilesRepo->GetProfile(outboundID);
                        if (prof == nullptr) {
                            MW_show_log("The outbound described in the rule chain is missing, maybe your data is corrupted");
                            return {};
                        }
                        obj["outbound"] = prof->outbound->DisplayName();
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
            auto resp = SingboxOptions::SniffProtocols;
            resp.prepend("");
            return resp;
        }
        if (fieldName == "action")
        {
            return SingboxOptions::ActionTypes;
        }
        if (fieldName == "method")
        {
            auto resp = SingboxOptions::rejectMethods;
            resp.prepend("");
            return resp;
        }
        if (fieldName == "strategy")
        {
            auto resp = DomainStrategy::DomainStrategy;
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
}
