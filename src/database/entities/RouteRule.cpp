#include <QCoreApplication>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include "include/database/entities/RouteRule.h"


#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"

#include <srslist.h>

#include <algorithm>
#include <string_view>

namespace Configs {
    template<typename Converter = std::nullptr_t>
    QJsonArray get_as_array(const QList<QString>& str, bool castToNum = false, Converter converter = nullptr)
    {
        QJsonArray result;

        // Last gate before the core: heals values already stored with stray whitespace.
        for (const QString& raw : str) {
            const QString item = raw.trimmed();
            if (item.isEmpty()) continue;
            const QString converted = [&] {
                if constexpr (std::is_same_v<Converter, std::nullptr_t>)
                    return item;
                else
                    return converter(item);
            }();

            if (castToNum)
                result.append(converted.toInt());
            else
                result.append(converted);
        }

        return result;
    }

    static bool isValidStrArray(const QStringList& arr) {
        return std::ranges::any_of(arr,
                                   [](const QString& item) { return !QStringView(item).trimmed().isEmpty(); });
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
        package_name << other.package_name;
        wifi_ssid << other.wifi_ssid;
        wifi_bssid << other.wifi_bssid;
        rule_set << other.rule_set;
        invert = other.invert;
        outboundID = other.outboundID;
        action = other.action;
        rejectMethod = other.rejectMethod;
        no_drop = other.no_drop;
        override_address = other.override_address;
        override_port = other.override_port;
        tls_fragment = other.tls_fragment;
        tls_fragment_fallback_delay = other.tls_fragment_fallback_delay;
        tls_record_fragment = other.tls_record_fragment;
        tls_spoof = other.tls_spoof;
        tls_spoof_method = other.tls_spoof_method;
        sniffers << other.sniffers;
        sniffOverrideDest = other.sniffOverrideDest;
        strategy = other.strategy;
        type = other.type;
        uiVisibleAttributes = other.uiVisibleAttributes;
        uiAttributeTabsSeeded = other.uiAttributeTabsSeeded;
    }

    QJsonObject RouteRule::get_rule_json(bool forView, const QString& outboundTag) {
        QJsonObject obj;

        // Placeholder rule: it persists only the endpoint id, the gate comes from its resolved tag.
        if (type == endpointPreferredBy) {
            QString tag = outboundTag;
            if (forView) {
                const auto prof = Configs::dataManager->profilesRepo->GetProfile(outboundID);
                if (prof == nullptr || prof->outbound == nullptr) return {};
                tag = prof->outbound->DisplayName();
            }
            if (tag.isEmpty()) return {};
            return QJsonObject{
                {"preferred_by", QJsonArray{tag}},
                {"action", "route"},
                {"outbound", tag},
            };
        }

        if (!ip_version.trimmed().isEmpty()) obj["ip_version"] = ip_version.trimmed().toInt();
        if (!network.trimmed().isEmpty()) obj["network"] = network.trimmed();
        if (!protocol.trimmed().isEmpty()) obj["protocol"] = protocol.trimmed();
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
        if (isValidStrArray(package_name)) obj["package_name"] = get_as_array(package_name);
        if (isValidStrArray(wifi_ssid)) obj["wifi_ssid"] = get_as_array(wifi_ssid);
        if (isValidStrArray(wifi_bssid)) obj["wifi_bssid"] = get_as_array(wifi_bssid);
        if (isValidStrArray(rule_set))
            if (forView)
                obj["rule_set"] = get_as_array(rule_set);
            else
                obj["rule_set"] = get_as_array(rule_set, false, get_rule_set_name);
        if (invert) obj["invert"] = invert;
        if (action == "route")
        {
            if (outboundID == -3) action = "reject";
            if (outboundID == -4) action = "hijack-dns";
        }
        obj["action"] = action;

        if (action == "reject")
        {
            // the core fails on an unknown method and on no_drop alongside drop
            const QString method = rejectMethod.trimmed().toLower();
            if (SingboxOptions::rejectMethods.contains(method)) obj["method"] = method;
            if (no_drop && method != QStringLiteral("drop")) obj["no_drop"] = true;
        }
        if (action == "route" || action == "route-options" || action == "bypass")
        {
            if (!override_address.trimmed().isEmpty()) obj["override_address"] = override_address.trimmed();
            if (override_port.trimmed().toInt() > 0) obj["override_port"] = override_port.trimmed().toInt();
            if (tls_fragment)
            {
                obj["tls_fragment"] = true;
                // the core reads the delay only alongside tls_fragment
                if (!tls_fragment_fallback_delay.trimmed().isEmpty()) obj["tls_fragment_fallback_delay"] = tls_fragment_fallback_delay.trimmed();
            }
            if (tls_record_fragment) obj["tls_record_fragment"] = true;
            if (!tls_spoof.trimmed().isEmpty())
            {
                obj["tls_spoof"] = tls_spoof.trimmed();
                // the method only qualifies a spoof, the core rejects it standalone
                if (!tls_spoof_method.trimmed().isEmpty()) obj["tls_spoof_method"] = tls_spoof_method.trimmed();
            }

            if (action == "route" || action == "bypass")
            {
                if (forView) {
                    switch (outboundID) { // TODO use constants
                    case -1:
                        obj["outbound"] = "proxy";
                        break;
                    case -2:
                        obj["outbound"] = "direct";
                        break;
                    case warpBypassID:
                        obj["outbound"] = "warp-bypass";
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
            if (!strategy.trimmed().isEmpty()) obj["strategy"] = strategy.trimmed();
        }

        return obj;
    }

    QJsonObject RouteRule::to_share_json() {
        // Same key space as the endpoints list, so a rule and its endpoint drop or survive together.
        if (type == endpointPreferredBy) {
            return QJsonObject{
                {"name", name},
                {"type", ruleTypeToToken(type)},
                {"outbound", outboundID},
            };
        }
        // forView=true renders outbound as a portable string and skips the adblock injection.
        QJsonObject obj = get_rule_json(true);
        if (obj.isEmpty()) return obj; // outbound profile missing; caller skips it
        obj["name"] = name;
        obj["type"] = ruleTypeToToken(type);
        return obj;
    }

    // TODO use constant for field names
    QStringList RouteRule::tab_attributes() {
        QStringList out;
        for (const QString& a : get_attributes()) {
            if (a != QStringLiteral("action")) out << a;
        }
        return out;
    }

    bool RouteRule::is_attribute_at_default(const RouteRule& rule, const QString& attr) {
        if (attr == QStringLiteral("action")) return true;
        switch (RouteRule::get_input_type(attr)) {
            case trufalse:
                if (attr == QStringLiteral("source_ip_is_private")) return !rule.source_ip_is_private;
                if (attr == QStringLiteral("ip_is_private")) return !rule.ip_is_private;
                if (attr == QStringLiteral("invert")) return !rule.invert;
                if (attr == QStringLiteral("no_drop")) return !rule.no_drop;
                if (attr == QStringLiteral("override_destination")) return !rule.sniffOverrideDest;
                if (attr == QStringLiteral("tls_fragment")) return !rule.tls_fragment;
                if (attr == QStringLiteral("tls_record_fragment")) return !rule.tls_record_fragment;
                return true;
            case select:
                if (attr == QStringLiteral("outbound")) return rule.outboundID == directID;
                {
                    const QStringList s = rule.get_current_value_string(attr);
                    return s.isEmpty() || s.first().isEmpty();
                }
            case text:
                if (attr == QStringLiteral("override_address")) return rule.override_address.isEmpty();
                // 0 is the core's "unset"; any other text keeps its tab, or a bad value would be hidden with it.
                if (attr == QStringLiteral("override_port"))
                    return rule.override_port.trimmed().isEmpty() || rule.override_port.trimmed() == QStringLiteral("0");
                if (attr == QStringLiteral("tls_spoof")) return rule.tls_spoof.isEmpty();
                return !isValidStrArray(rule.get_current_value_string(attr));
        }
        return true;
    }

    void RouteRule::clear_attribute_value(const QString& attr) {
        if (attr == QStringLiteral("ip_version")) ip_version.clear();
        else if (attr == QStringLiteral("network")) network.clear();
        else if (attr == QStringLiteral("protocol")) protocol.clear();
        else if (attr == QStringLiteral("inbound")) inbound.clear();
        else if (attr == QStringLiteral("domain")) domain.clear();
        else if (attr == QStringLiteral("domain_suffix")) domain_suffix.clear();
        else if (attr == QStringLiteral("domain_keyword")) domain_keyword.clear();
        else if (attr == QStringLiteral("domain_regex")) domain_regex.clear();
        else if (attr == QStringLiteral("source_ip_cidr")) source_ip_cidr.clear();
        else if (attr == QStringLiteral("source_ip_is_private")) source_ip_is_private = false;
        else if (attr == QStringLiteral("ip_cidr")) ip_cidr.clear();
        else if (attr == QStringLiteral("ip_is_private")) ip_is_private = false;
        else if (attr == QStringLiteral("source_port")) source_port.clear();
        else if (attr == QStringLiteral("source_port_range")) source_port_range.clear();
        else if (attr == QStringLiteral("port")) port.clear();
        else if (attr == QStringLiteral("port_range")) port_range.clear();
        else if (attr == QStringLiteral("process_name")) process_name.clear();
        else if (attr == QStringLiteral("process_path")) process_path.clear();
        else if (attr == QStringLiteral("process_path_regex")) process_path_regex.clear();
        else if (attr == QStringLiteral("package_name")) package_name.clear();
        else if (attr == QStringLiteral("wifi_ssid")) wifi_ssid.clear();
        else if (attr == QStringLiteral("wifi_bssid")) wifi_bssid.clear();
        else if (attr == QStringLiteral("rule_set")) rule_set.clear();
        else if (attr == QStringLiteral("invert")) invert = false;
        else if (attr == QStringLiteral("outbound")) outboundID = directID;
        else if (attr == QStringLiteral("method")) rejectMethod.clear();
        else if (attr == QStringLiteral("no_drop")) no_drop = false;
        else if (attr == QStringLiteral("override_address")) override_address.clear();
        else if (attr == QStringLiteral("override_port")) override_port.clear();
        else if (attr == QStringLiteral("tls_fragment")) tls_fragment = false;
        else if (attr == QStringLiteral("tls_fragment_fallback_delay")) tls_fragment_fallback_delay.clear();
        else if (attr == QStringLiteral("tls_record_fragment")) tls_record_fragment = false;
        else if (attr == QStringLiteral("tls_spoof")) tls_spoof.clear();
        else if (attr == QStringLiteral("tls_spoof_method")) tls_spoof_method.clear();
        else if (attr == QStringLiteral("override_destination")) sniffOverrideDest = false;
        else if (attr == QStringLiteral("strategy")) strategy.clear();
    }

    void RouteRule::ensure_ui_visible_attribute_tabs_seeded() {
        if (uiAttributeTabsSeeded) return;
        uiAttributeTabsSeeded = true;
        for (const QString& a : tab_attributes()) {
            if (!is_attribute_at_default(*this, a)) uiVisibleAttributes.insert(a);
        }
    }

    QStringList RouteRule::get_attributes()
    {
        auto res = QStringList{
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
            "package_name",
            "wifi_ssid",
            "wifi_bssid",
            "rule_set",
            "invert",
            "action",
            "outbound",
            "override_address",
            "override_port",
            "tls_fragment",
            "tls_fragment_fallback_delay",
            "tls_record_fragment",
            "tls_spoof",
            "tls_spoof_method",
            "method",
            "no_drop",
            "override_destination",
            "strategy",
        };
        if (getOS() == Darwin) res.removeAll("wifi_ssid"), res.removeAll("wifi_bssid");
        return res;
    }

    inputType RouteRule::get_input_type(const QString& fieldName) {
        if (fieldName == "invert" ||
            fieldName == "source_ip_is_private" ||
            fieldName == "ip_is_private" ||
            fieldName == "no_drop" ||
            fieldName == "tls_fragment" ||
            fieldName == "tls_record_fragment" ||
            fieldName == "override_destination") return trufalse;

        if (fieldName == "ip_version" ||
            fieldName == "network" ||
            fieldName == "protocol" ||
            fieldName == "action" ||
            fieldName == "method" ||
            fieldName == "strategy" ||
            fieldName == "tls_spoof_method" ||
            fieldName == "outbound") return select;

        return text;
    }

    QStringList RouteRule::get_values_for_field(const QString& fieldName) {
        if (fieldName == "ip_version") {
            return {"", "4", "6"};
        }
        if (fieldName == "network") {
            return {"", "tcp", "udp", "icmp"};
        }
        if (fieldName == "protocol") {
            auto resp = SingboxOptions::SniffProtocols;
            if (!resp.contains("ntp")) resp.append("ntp");
            resp.prepend("");
            return resp;
        }
        if (fieldName == "action")
        {
            auto actions = SingboxOptions::ActionTypes;
            if (getOS() == Linux) actions.insert(1, "bypass");
            return actions;
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
        if (fieldName == "tls_spoof_method")
        {
            return {"", "wrong-sequence", "wrong-checksum", "wrong-ack", "wrong-md5", "wrong-timestamp"};
        }
        return {};
    }

    QStringList RouteRule::get_current_value_string(const QString& fieldName) const {
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
        if (fieldName == "tls_fragment_fallback_delay")
        {
            return {tls_fragment_fallback_delay};
        }
        if (fieldName == "tls_spoof")
        {
            return {tls_spoof};
        }
        if (fieldName == "tls_spoof_method")
        {
            return {tls_spoof_method};
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
        if (fieldName == "package_name") return package_name;
        if (fieldName == "wifi_ssid") return wifi_ssid;
        if (fieldName == "wifi_bssid") return wifi_bssid;
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
        if (fieldName == "tls_fragment")
        {
            return tls_fragment? "true":"false";
        }
        if (fieldName == "tls_record_fragment")
        {
            return tls_record_fragment? "true":"false";
        }
        if (fieldName == "override_destination")
        {
            return sniffOverrideDest? "true":"false";
        }
        return nullptr;
    }

    static QStringList filterEmpty(const QStringList& base) {
        QStringList res;
        for (const auto& item: base) {
            if (const auto trimmed = item.trimmed(); !trimmed.isEmpty()) {
                res << trimmed;
            }
        }
        return res;
    }

    void RouteRule::set_field_value(const QString& fieldName, const QStringList& value) {
        // Imported rules can carry an empty array for a scalar field, so never index blindly.
        const QString scalar = value.isEmpty() ? QString() : value[0].trimmed();
        if (fieldName == "ip_version") {
            ip_version = scalar;
        }
        if (fieldName == "network") {
            network = scalar;
        }
        if (fieldName == "protocol") {
            protocol = scalar;
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
            source_ip_is_private = scalar=="true";
        }
        if (fieldName == "ip_cidr") {
            ip_cidr = filterEmpty(value);
        }
        if (fieldName == "ip_is_private") {
            ip_is_private = scalar=="true";
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
        if (fieldName == "package_name") {
            package_name = filterEmpty(value);
        }
        if (fieldName == "wifi_ssid") {
            wifi_ssid = filterEmpty(value);
        }
        if (fieldName == "wifi_bssid") {
            wifi_bssid = filterEmpty(value);
        }
        if (fieldName == "rule_set") {
            rule_set = filterEmpty(value);
        }
        // Early 2025 builds wrote it on sniff rules; kept, though get_rule_json does not emit it.
        if (fieldName == "sniffers") {
            sniffers = filterEmpty(value);
        }
        if (fieldName == "invert") {
            invert = scalar=="true";
        }
        if (fieldName == "action")
        {
            // The core reads an empty action as route, which get_rule_json would emit without its outbound.
            action = scalar.isEmpty() ? QStringLiteral("route") : scalar;
        }
        // Older builds wrote "reject_method", so shares and remote profiles may arrive with that key.
        if (fieldName == "method" || fieldName == "reject_method")
        {
            rejectMethod = scalar;
        }
        if (fieldName == "no_drop")
        {
            no_drop = scalar=="true";
        }
        if (fieldName == "override_address")
        {
            override_address = scalar;
        }
        if (fieldName == "override_port")
        {
            override_port = scalar;
        }
        if (fieldName == "tls_fragment")
        {
            tls_fragment = scalar=="true";
        }
        if (fieldName == "tls_fragment_fallback_delay")
        {
            tls_fragment_fallback_delay = scalar;
        }
        if (fieldName == "tls_record_fragment")
        {
            tls_record_fragment = scalar=="true";
        }
        if (fieldName == "tls_spoof")
        {
            tls_spoof = scalar;
        }
        if (fieldName == "tls_spoof_method")
        {
            tls_spoof_method = scalar;
        }
        if (fieldName == "override_destination")
        {
            sniffOverrideDest = scalar=="true";
        }
        if (fieldName == "strategy")
        {
            strategy = scalar;
        }
        if (fieldName == "outbound")
        {
            outboundID = scalar.toInt();
        }
    }

    bool RouteRule::isEmpty() {
        if (type != custom) {
            if (type == endpointPreferredBy) return false;
            if (type == simpleAddressProxy || type == simpleAddressBypass || type == simpleAddressBlock || type == simpleAddressWarpBypass) {
                return domain.empty() &&
                    domain_suffix.empty() &&
                    domain_keyword.empty() &&
                    domain_regex.empty() &&
                    rule_set.empty() &&
                    ip_cidr.empty();
            } else {
                return process_name.empty() && process_path.empty();
            }
        }
        auto ruleJson = get_rule_json();
        if (action == "route" || action == "route-options" || action == "hijack-dns") return ruleJson.keys().length() <= 1;
        if (action == "sniff" || action == "resolve" || action == "reject" || action == "bypass") return ruleJson.keys().length() < 1;
        return false;
    }

    bool RouteRule::canEditAttr(const QString &attr) {
        if (type == custom) return true;
        if (type == endpointPreferredBy) return false;
        if (type == simpleAddressProxy || type == simpleAddressBypass || type == simpleAddressBlock || type == simpleAddressWarpBypass) {
            return attr == "domain" || attr == "domain_suffix" || attr == "domain_keyword" || attr == "domain_regex" || attr == "rule_set" || attr == "ip_cidr";
        } else {
            return attr == "process_path" || attr == "process_name";
        }
    }

    namespace {
        bool routeRuleDigits(QStringView text) {
            return !text.isEmpty() && std::all_of(text.begin(), text.end(), [](QChar c) { return c >= u'0' && c <= u'9'; });
        }

        // "host:1234", not an IPv6 address.
        bool routeRuleHasPortSuffix(const QString& text) {
            const qsizetype colon = text.lastIndexOf(u':');
            return colon > 0 && text.indexOf(u':') == colon && routeRuleDigits(QStringView(text).mid(colon + 1));
        }

        bool routeRuleKnownRuleSet(const QString& name) {
            const QByteArray utf8 = name.toUtf8();
            const std::string_view key(utf8.constData(), static_cast<size_t>(utf8.size()));
            const auto it = std::lower_bound(ruleSetList.begin(), ruleSetList.end(), key,
                                             [](const auto& entry, std::string_view k) { return entry.first < k; });
            return it != ruleSetList.end() && it->first == key;
        }

        // The build downloads any URL naming a .srs file; only http(s) ones can succeed.
        bool routeRuleRuleSetUrl(const QString& text) {
            const QUrl url(text);
            const QString scheme = url.scheme().toLower();
            return url.isValid() && (scheme == "http" || scheme == "https") && !url.host().isEmpty() && url.fileName().contains(".srs");
        }

        // Shared profiles travel between systems, so both path styles count.
        bool routeRuleAbsolutePath(const QString& text) {
            if (text.startsWith(u'/') || text.startsWith(u'\\')) return true;
            return text.size() >= 3 && text[0].isLetter() && text[1] == u':' && (text[2] == u'\\' || text[2] == u'/');
        }

        // The core accepts any non-empty item; these just never match.
        QString routeRuleDomainProblem(const QString& value, Validation::Severity* severity) {
            *severity = Validation::Severity::Warning;
            if (value.contains(QStringLiteral("://")))
                return QCoreApplication::translate("Validation", "Write only the domain, without a scheme such as https://; as written, this never matches.");
            if (std::any_of(value.begin(), value.end(), [](QChar c) { return c.isSpace(); }))
                return QCoreApplication::translate("Validation", "A domain has no spaces in it, so this never matches.");
            if (value.contains(u'/')) return QCoreApplication::translate("Validation", "Write only the domain, without a path; as written, this never matches.");
            if (value.contains(u':'))
                return QCoreApplication::translate("Validation", "A domain has no port or ':' in it, so this never matches; match ports and IP addresses with conditions of their own.");
            if (value != value.toLower())
                return QCoreApplication::translate("Validation", "The core compares lowercase host names, so this never matches; write it in lowercase.");
            return {};
        }

        // ParseSocksaddrHostPort unwraps one pair of brackets around the whole value.
        QString routeRuleOverrideAddressProblem(const QString& value) {
            if (value.startsWith(u'[')) {
                if (value.size() > 2 && value.endsWith(u']')) return Validation::Ip(value.mid(1, value.size() - 2));
                return QCoreApplication::translate("Validation", "Write an IPv6 address as 2001:db8::1 or [2001:db8::1]; a port goes in override_port.");
            }
            if (Validation::Ip(value).isEmpty()) return {};
            if (routeRuleHasPortSuffix(value))
                return QCoreApplication::translate("Validation", "Put the port in override_port and only the address here, such as 1.2.3.4 or example.com.");
            return Validation::Host(value);
        }

        QString routeRuleSpoofProblem(const QString& value, Validation::Severity* severity) {
            const QString bare = value.startsWith(u'[') && value.endsWith(u']') ? value.mid(1, value.size() - 2) : value;
            if (Validation::Ip(bare).isEmpty()) {
                *severity = Validation::Severity::Warning;
                return QCoreApplication::translate("Validation", "An IP address sends no server name; use a host name such as www.example.com.");
            }
            if (!value.contains(QStringLiteral("://")) && routeRuleHasPortSuffix(value))
                return QCoreApplication::translate("Validation", "Write only the host name, without a port.");
            return Validation::Host(value);
        }

        bool routeRuleUnroutable(const std::shared_ptr<Profile>& ent) {
            if (ent->outbound == nullptr || ent->outbound->IsExtraCore() || ent->outbound->IsXrayFullConfig()) return true;
            return ent->type == "custom" && ent->Custom() != nullptr && ent->Custom()->type == Custom::CustomFullConfig;
        }

        // Mirrors calculatePrerequisites' checks of route outbounds, which otherwise fail the start without naming the rule.
        QString routeRuleOutboundProblem(int id) {
            if (id == proxyID || id == directID || id == warpBypassID) return {};
            if (id < 0) return QCoreApplication::translate("Validation", "Not a valid outbound; pick another one.");
            const auto ent = dataManager->profilesRepo->GetProfile(id);
            if (ent == nullptr || ent->outbound == nullptr)
                return QCoreApplication::translate("Validation", "The outbound profile no longer exists; pick another one.");
            const QString name = ent->outbound->DisplayName();
            if (routeRuleUnroutable(ent))
                return QCoreApplication::translate("Validation", "Routing rules cannot use \"%1\": extra core profiles and full configs are not supported. Pick another outbound.").arg(name);
            if (ent->type != "chain") return {};
            const auto chain = ent->Chain();
            if (chain == nullptr || chain->list.isEmpty())
                return QCoreApplication::translate("Validation", "The chain \"%1\" has no hops; pick another outbound.").arg(name);
            for (const int hopID: chain->list) {
                const auto hop = dataManager->profilesRepo->GetProfile(hopID);
                if (hop == nullptr || hop->type == "chain" || routeRuleUnroutable(hop))
                    return QCoreApplication::translate("Validation", "Routing rules cannot use the chain \"%1\": a hop of it is missing, another chain, an extra core profile or a full config. Pick another outbound.").arg(name);
            }
            return {};
        }

        // The actions get_rule_json emits each action-specific attribute for.
        const QHash<QString, QStringList>& routeRuleActionScopes() {
            static const QStringList routeLike = {"route", "route-options", "bypass"};
            static const QHash<QString, QStringList> scopes = {
                {"outbound", {"route", "bypass"}},
                {"override_address", routeLike},
                {"override_port", routeLike},
                {"tls_fragment", routeLike},
                {"tls_fragment_fallback_delay", routeLike},
                {"tls_record_fragment", routeLike},
                {"tls_spoof", routeLike},
                {"tls_spoof_method", routeLike},
                {"method", {"reject"}},
                {"no_drop", {"reject"}},
                {"override_destination", {"sniff"}},
                {"strategy", {"resolve"}},
            };
            return scopes;
        }
    }

    bool RouteRule::is_list_attribute(const QString& attr) {
        static const QSet<QString> lists = {
            "inbound", "domain", "domain_suffix", "domain_keyword", "domain_regex", "source_ip_cidr", "ip_cidr",
            "source_port", "source_port_range", "port", "port_range", "process_name", "process_path", "process_path_regex",
            "package_name", "wifi_ssid", "wifi_bssid", "rule_set", "sniffers",
        };
        return lists.contains(attr);
    }

    bool RouteRule::is_match_attribute(const QString& attr) {
        static const QSet<QString> match = {
            "ip_version", "network", "protocol", "inbound", "domain", "domain_suffix", "domain_keyword", "domain_regex",
            "source_ip_cidr", "source_ip_is_private", "ip_cidr", "ip_is_private", "source_port", "source_port_range",
            "port", "port_range", "process_name", "process_path", "process_path_regex", "package_name",
            "wifi_ssid", "wifi_bssid", "rule_set", "invert",
        };
        return match.contains(attr);
    }

    QString RouteRule::effective_action() const {
        if (action == "route") {
            if (outboundID == blockID) return QStringLiteral("reject");
            if (outboundID == -4) return QStringLiteral("hijack-dns");
        }
        return action;
    }

    bool RouteRule::has_match_conditions() const {
        if (source_ip_is_private || ip_is_private) return true;
        for (const QString& attr: get_attributes()) {
            if (!is_match_attribute(attr) || get_input_type(attr) == trufalse) continue;
            if (isValidStrArray(get_current_value_string(attr))) return true;
        }
        return false;
    }

    RuleIssue RouteRule::check_value(const QString& attr, const QString& raw) {
        using Validation::Severity;
        const QString value = raw.trimmed();
        if (value.isEmpty()) return {};
        QString message;
        Severity severity = Severity::Error;
        if (attr == "domain" || attr == "domain_suffix" || attr == "domain_keyword") {
            message = routeRuleDomainProblem(value, &severity);
        } else if (attr == "domain_regex" || attr == "process_path_regex") {
            message = Validation::GoRegex(value);
            severity = Severity::Warning;
        } else if (attr == "ip_cidr" || attr == "source_ip_cidr") {
            message = Validation::Cidr(value, Validation::Family::Any, true);
        } else if (attr == "port" || attr == "source_port") {
            if (value.contains(u':') || value.contains(u'-')) {
                message = attr == "port"
                    ? QCoreApplication::translate("Validation", "A range goes in port_range, written like 1000:2000.")
                    : QCoreApplication::translate("Validation", "A range goes in source_port_range, written like 1000:2000.");
            } else if (value != QStringLiteral("0")) {
                // Listable[uint16] in the core, so 0 is accepted.
                message = Validation::Port(value);
            }
        } else if (attr == "port_range" || attr == "source_port_range") {
            const Validation::PortRangeStyle style{Validation::RangeSep::Colon, false, true, false};
            message = Validation::PortRange(value, style);
            if (message.isEmpty()) {
                message = Validation::PortRangeOrder(value, style);
                severity = Severity::Warning;
            }
        } else if (attr == "process_name") {
            if (value.contains(u'/') || value.contains(u'\\')) {
                message = QCoreApplication::translate("Validation", "Write only the program's file name, such as chrome.exe; a name with a path never matches. Match a full path with a process path condition instead.");
                severity = Severity::Warning;
            }
        } else if (attr == "process_path") {
            if (!routeRuleAbsolutePath(value)) {
                message = QCoreApplication::translate("Validation", "Write the full path, such as C:\\Program Files\\App\\app.exe or /usr/bin/app.");
                severity = Severity::Warning;
            }
        } else if (attr == "rule_set") {
            if (!routeRuleKnownRuleSet(value) && !routeRuleRuleSetUrl(value))
                message = QCoreApplication::translate("Validation", "Unknown rule-set. Use a built-in name such as geosite-google, or an http(s) URL of a binary .srs file.");
        } else if (attr == "override_address") {
            message = routeRuleOverrideAddressProblem(value);
        } else if (attr == "override_port") {
            if (value != QStringLiteral("0")) message = Validation::Port(value);
        } else if (attr == "tls_fragment_fallback_delay") {
            message = Validation::Duration(value);
        } else if (attr == "tls_spoof") {
            message = routeRuleSpoofProblem(value, &severity);
        } else if (attr == "ip_version") {
            message = Validation::OneOf(value, {"4", "6"});
        } else if (attr == "network") {
            message = Validation::OneOf(value, {"tcp", "udp", "icmp"});
        } else if (attr == "protocol") {
            message = Validation::OneOf(value, {"tls", "http", "quic", "dns", "stun", "bittorrent", "dtls", "ssh", "rdp", "ntp"});
        } else if (attr == "action") {
            message = Validation::OneOf(value, {"route", "route-options", "direct", "bypass", "reject", "hijack-dns", "sniff", "resolve"});
        } else if (attr == "method") {
            // get_rule_json lowercases it.
            message = Validation::OneOf(value.toLower(), {"default", "drop", "reply"});
        } else if (attr == "strategy") {
            message = Validation::OneOf(value, {"as_is", "prefer_ipv4", "prefer_ipv6", "ipv4_only", "ipv6_only"});
        } else if (attr == "tls_spoof_method") {
            message = Validation::OneOf(value, {"wrong-sequence", "wrong-checksum", "wrong-ack", "wrong-md5", "wrong-timestamp"});
        }
        if (message.isEmpty()) return {};
        return {severity, attr, message, is_list_attribute(attr) ? value : QString(), {}};
    }

    QList<RuleIssue> RouteRule::validate() const {
        using Validation::Severity;
        QList<RuleIssue> out;
        if (type == endpointPreferredBy) return out;
        const QString act = effective_action();
        const auto& scopes = routeRuleActionScopes();
        const auto applies = [&](const QString& attr) {
            const auto it = scopes.constFind(attr);
            return it == scopes.constEnd() || it->contains(act);
        };
        const auto add = [&out](Severity severity, const QString& attr, const QString& message, const QString& related = {}) {
            out << RuleIssue{severity, attr, message, {}, related};
        };

        const QStringList attributes = get_attributes();
        for (const QString& attr: attributes) {
            if (attr == "outbound" || get_input_type(attr) == trufalse || !applies(attr)) continue;
            for (const QString& item: get_current_value_string(attr)) {
                if (auto issue = check_value(attr, item); !issue.message.isEmpty()) out << issue;
            }
        }

        if (act == "route" || act == "bypass") {
            if (const auto message = routeRuleOutboundProblem(outboundID); !message.isEmpty()) add(Severity::Error, "outbound", message);
        }
        // The core's RouteOptionsActionOptions.UnmarshalJSON checks; route and bypass rules accept both fragment kinds.
        if (act == "route-options") {
            if (tls_fragment && tls_record_fragment) {
                add(Severity::Error, "tls_record_fragment",
                    QCoreApplication::translate("Validation", "A route-options rule cannot turn on both tls_fragment and tls_record_fragment; keep one, or use a route rule, which accepts both."),
                    "tls_fragment");
            }
            const bool anyOption = !override_address.trimmed().isEmpty() || override_port.trimmed().toInt() > 0 || tls_fragment ||
                                   tls_record_fragment || !tls_spoof.trimmed().isEmpty();
            // Without conditions either, FilterEmptyRules drops the rule at save.
            if (!anyOption && has_match_conditions()) {
                add(Severity::Error, "action",
                    QCoreApplication::translate("Validation", "A route-options rule must set at least one option: override_address, override_port, tls_fragment, tls_record_fragment or tls_spoof."));
            }
        }
        if (applies("tls_fragment")) {
            if (!tls_fragment_fallback_delay.trimmed().isEmpty() && !tls_fragment)
                add(Severity::Warning, "tls_fragment_fallback_delay", QCoreApplication::translate("Validation", "Has no effect unless tls_fragment is on."), "tls_fragment");
            if (!tls_spoof_method.trimmed().isEmpty() && tls_spoof.trimmed().isEmpty())
                add(Severity::Warning, "tls_spoof_method", QCoreApplication::translate("Validation", "Has no effect without a tls_spoof host name."), "tls_spoof");
        }
        if (act == "reject" && no_drop && rejectMethod.trimmed().toLower() == QStringLiteral("drop"))
            add(Severity::Warning, "no_drop", QCoreApplication::translate("Validation", "Has no effect with the drop method."), "method");

        for (const QString& attr: attributes) {
            const auto it = scopes.constFind(attr);
            if (it == scopes.constEnd() || it->contains(act) || is_attribute_at_default(*this, attr)) continue;
            // The block or dns_out outbound is what makes a route rule a reject or hijack-dns one.
            if (attr == "outbound" && action == "route") continue;
            add(Severity::Warning, attr,
                QCoreApplication::translate("Validation", "A %1 rule ignores it; it applies only to these actions: %2.").arg(act, it->join(QStringLiteral(", "))),
                "action");
        }

        if (type == custom && (act == "route" || act == "bypass" || act == "reject") && !has_match_conditions())
            add(Severity::Warning, "action", QCoreApplication::translate("Validation", "This rule has no match conditions, so it matches all traffic."));
        return out;
    }

    QList<RuleIssue> RouteRule::cross_checks() {
        using Validation::Severity;
        QList<RuleIssue> out = {
            {Severity::Error, "tls_record_fragment", {}, {}, "tls_fragment"},
            {Severity::Warning, "tls_fragment_fallback_delay", {}, {}, "tls_fragment"},
            {Severity::Warning, "tls_spoof_method", {}, {}, "tls_spoof"},
            {Severity::Warning, "no_drop", {}, {}, "method"},
        };
        for (const QString& attr: get_attributes()) {
            if (routeRuleActionScopes().contains(attr)) out << RuleIssue{Severity::Warning, attr, {}, {}, "action"};
        }
        return out;
    }
}
