#include <QCoreApplication>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include "include/database/entities/RouteRule.h"
#include "include/configs/ProcessRules.h"


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

    namespace {
        const RouteRuleAttribute* routeRuleAttribute(const QString& name) {
            static const QHash<QString, const RouteRuleAttribute*> byName = [] {
                QHash<QString, const RouteRuleAttribute*> out;
                for (const auto& attr: RouteRule::attribute_table()) out.insert(attr.name, &attr);
                return out;
            }();
            return byName.value(name);
        }

        bool routeRuleListed(const RouteRuleAttribute& attr) {
            return !(attr.flags & RouteRuleAttribute::NotOnDarwin) || getOS() != Darwin;
        }
    }

    RouteRule::RouteRule(const RouteRule& other) {
        *this = other;
        // A copy opens on its first attribute tab.
        uiActiveAttributeTabLabel.clear();
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

        if (action == "route")
        {
            if (outboundID == -3) action = "reject";
            if (outboundID == -4) action = "hijack-dns";
        }
        obj["action"] = action;

        using Json = RouteRuleAttribute::Json;
        for (const auto& attr: attribute_table()) {
            if (attr.json == Json::ByHand || (!attr.scope.isEmpty() && !attr.scope.contains(action))) continue;
            if (!attr.needs.isEmpty() && !obj.contains(attr.needs)) continue;
            if (const bool* flag = attr.field<bool>(*this)) {
                if (*flag) obj[attr.name] = true;
            } else if (const QString* text = attr.field<QString>(*this)) {
                const QString value = text->trimmed();
                if (attr.json == Json::PositiveNumber) {
                    if (value.toInt() > 0) obj[attr.name] = value.toInt();
                } else if (!value.isEmpty()) {
                    obj[attr.name] = attr.json == Json::Number ? QJsonValue(value.toInt()) : QJsonValue(value);
                }
            } else if (const QStringList* list = attr.field<QStringList>(*this); list != nullptr && isValidStrArray(*list)) {
                if (attr.json == Json::RuleSets && !forView) obj[attr.name] = get_as_array(*list, false, get_rule_set_name);
                else obj[attr.name] = get_as_array(*list, attr.json == Json::Numbers);
            }
        }

        if (action == "reject")
        {
            // the core fails on an unknown method and on no_drop alongside drop
            const QString method = rejectMethod.trimmed().toLower();
            if (SingboxOptions::rejectMethods.contains(method)) obj["method"] = method;
            if (no_drop && method != QStringLiteral("drop")) obj["no_drop"] = true;
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
        if (disabled) obj["disabled"] = true;
        return obj;
    }

    QStringList RouteRule::tab_attributes() {
        QStringList out;
        for (const auto& attr: attribute_table()) {
            if (routeRuleListed(attr) && !(attr.flags & RouteRuleAttribute::NoTab)) out << attr.name;
        }
        return out;
    }

    bool RouteRule::is_attribute_at_default(const RouteRule& rule, const QString& attr) {
        if (attr == QStringLiteral("action")) return true;
        if (attr == QStringLiteral("outbound")) return rule.outboundID == directID;
        const RouteRuleAttribute* entry = routeRuleAttribute(attr);
        if (entry == nullptr) return true;
        if (const bool* flag = entry->field<bool>(rule)) return !*flag;
        if (const QString* text = entry->field<QString>(rule)) return entry->unset != nullptr ? entry->unset(*text) : text->isEmpty();
        if (const QStringList* list = entry->field<QStringList>(rule)) return !isValidStrArray(*list);
        return true;
    }

    void RouteRule::clear_attribute_value(const QString& attr) {
        if (attr == QStringLiteral("outbound")) {
            outboundID = directID;
            return;
        }
        const RouteRuleAttribute* entry = routeRuleAttribute(attr);
        if (entry == nullptr) return;
        if (bool* flag = entry->field<bool>(*this)) *flag = false;
        else if (QString* text = entry->field<QString>(*this)) text->clear();
        else if (QStringList* list = entry->field<QStringList>(*this)) list->clear();
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
        static const QStringList attributes = [] {
            QStringList res;
            for (const auto& attr: attribute_table()) {
                if (routeRuleListed(attr)) res << attr.name;
            }
            return res;
        }();
        return attributes;
    }

    inputType RouteRule::get_input_type(const QString& fieldName) {
        const RouteRuleAttribute* entry = routeRuleAttribute(fieldName);
        return entry != nullptr ? entry->input : text;
    }

    QStringList RouteRule::get_values_for_field(const QString& fieldName) {
        const RouteRuleAttribute* entry = routeRuleAttribute(fieldName);
        return entry != nullptr && entry->values != nullptr ? entry->values() : QStringList();
    }

    QStringList RouteRule::get_current_value_string(const QString& fieldName) const {
        if (fieldName == "action") return {action};
        if (fieldName == "outbound") return {Int2String(outboundID)};
        const RouteRuleAttribute* entry = routeRuleAttribute(fieldName);
        if (entry == nullptr) return {};
        if (const QString* text = entry->field<QString>(*this)) return {*text};
        if (const QStringList* list = entry->field<QStringList>(*this)) return *list;
        return {};
    }

    QString RouteRule::get_current_value_bool(const QString& fieldName) const {
        const RouteRuleAttribute* entry = routeRuleAttribute(fieldName);
        const bool* flag = entry != nullptr ? entry->field<bool>(*this) : nullptr;
        if (flag == nullptr) return nullptr;
        return *flag ? "true" : "false";
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
        if (fieldName == "action")
        {
            // The core reads an empty action as route, which get_rule_json would emit without its outbound.
            action = scalar.isEmpty() ? QStringLiteral("route") : scalar;
            return;
        }
        if (fieldName == "outbound")
        {
            outboundID = scalar.toInt();
            return;
        }
        // Early 2025 builds wrote it on sniff rules; kept, though get_rule_json does not emit it.
        if (fieldName == "sniffers")
        {
            sniffers = filterEmpty(value);
            return;
        }
        // Older builds wrote "reject_method", so shares and remote profiles may arrive with that key.
        const RouteRuleAttribute* entry = routeRuleAttribute(fieldName == "reject_method" ? QStringLiteral("method") : fieldName);
        if (entry == nullptr) return;
        if (bool* flag = entry->field<bool>(*this)) *flag = scalar == "true";
        else if (QString* text = entry->field<QString>(*this)) *text = scalar;
        else if (QStringList* list = entry->field<QStringList>(*this)) *list = filterEmpty(value);
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

        QString routeRuleRegexProblem(const QString& value, Validation::Severity* severity) {
            *severity = Validation::Severity::Warning;
            return Validation::GoRegex(value);
        }

        QString routeRuleCidrProblem(const QString& value, Validation::Severity*) {
            return Validation::Cidr(value, Validation::Family::Any, true);
        }

        QString routeRulePortProblem(const QString& value, bool source) {
            if (value.contains(u':') || value.contains(u'-')) {
                return source ? QCoreApplication::translate("Validation", "A range goes in source_port_range, written like 1000:2000.")
                              : QCoreApplication::translate("Validation", "A range goes in port_range, written like 1000:2000.");
            }
            // Listable[uint16] in the core, so 0 is accepted.
            return value != QStringLiteral("0") ? Validation::Port(value) : QString();
        }

        QString routeRulePortRangeProblem(const QString& value, Validation::Severity* severity) {
            const Validation::PortRangeStyle style{Validation::RangeSep::Colon, false, true, false};
            if (QString message = Validation::PortRange(value, style); !message.isEmpty()) return message;
            *severity = Validation::Severity::Warning;
            return Validation::PortRangeOrder(value, style);
        }

        QString routeRuleProcessNameProblem(const QString& value, Validation::Severity* severity) {
            *severity = Validation::Severity::Warning;
            if (value.contains(u'/') || value.contains(u'\\'))
                return QCoreApplication::translate("Validation", "Write only the program's file name, such as chrome.exe; a name with a path never matches. Match a full path with a process path condition instead.");
            if (const QString known = ProcessRules::KnownSpelling(QStringLiteral("process_name"), value); !known.isEmpty())
                return QCoreApplication::translate("Validation", "The core reports this program as %1 and compares names letter for letter, case included, so this never matches.").arg(known);
            return {};
        }

        QString routeRuleProcessPathProblem(const QString& value, Validation::Severity* severity) {
            *severity = Validation::Severity::Warning;
            if (!routeRuleAbsolutePath(value))
                return QCoreApplication::translate("Validation", "Write the full path, such as C:\\Program Files\\App\\app.exe or /usr/bin/app.");
            if (value.size() > 2 && value[1] == u':' && value.contains(u'/'))
                return QCoreApplication::translate("Validation", "Windows reports program paths with \\, so a path written with / never matches.");
            if (const QString known = ProcessRules::KnownSpelling(QStringLiteral("process_path"), value); !known.isEmpty())
                return QCoreApplication::translate("Validation", "The core reports this path as %1 and compares it letter for letter, case included, so this never matches.").arg(known);
            return {};
        }

        QString routeRuleRuleSetProblem(const QString& value, Validation::Severity*) {
            if (routeRuleKnownRuleSet(value) || routeRuleRuleSetUrl(value)) return {};
            return QCoreApplication::translate("Validation", "Unknown rule-set. Use a built-in name such as geosite-google, or an http(s) URL of a binary .srs file.");
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
                if (hop->type == "autoselector")
                    return QCoreApplication::translate("Validation", "Routing rules cannot use the chain \"%1\": it holds an auto selector, so it can only be started. Pick another outbound.").arg(name);
            }
            return {};
        }
    }

    const QList<RouteRuleAttribute>& RouteRule::attribute_table() {
        using A = RouteRuleAttribute;
        using Validation::Severity;
        static const QStringList routeLike = {"route", "route-options", "bypass"};
        static const QList<RouteRuleAttribute> table = {
            {.name = "ip_version", .member = &RouteRule::ip_version, .input = select, .flags = A::Match, .column = "ip_version",
             .json = A::Json::Number,
             .values = [] { return QStringList{"", "4", "6"}; },
             .check = [](const QString& v, Severity*) { return Validation::OneOf(v, {"4", "6"}); }},
            {.name = "network", .member = &RouteRule::network, .input = select, .flags = A::Match, .column = "network",
             .values = [] { return QStringList{"", "tcp", "udp", "icmp"}; },
             .check = [](const QString& v, Severity*) { return Validation::OneOf(v, {"tcp", "udp", "icmp"}); }},
            {.name = "protocol", .member = &RouteRule::protocol, .input = select, .flags = A::Match, .column = "protocol",
             .values = [] {
                 auto resp = SingboxOptions::SniffProtocols;
                 if (!resp.contains("ntp")) resp.append("ntp");
                 resp.prepend("");
                 return resp;
             },
             .check = [](const QString& v, Severity*) {
                 return Validation::OneOf(v, {"tls", "http", "quic", "dns", "stun", "bittorrent", "dtls", "ssh", "rdp", "ntp"});
             }},
            {.name = "inbound", .member = &RouteRule::inbound, .flags = A::Match, .column = "inbound_json"},
            {.name = "domain", .member = &RouteRule::domain, .flags = A::Match, .column = "domain_json", .check = routeRuleDomainProblem},
            {.name = "domain_suffix", .member = &RouteRule::domain_suffix, .flags = A::Match, .column = "domain_suffix_json",
             .check = routeRuleDomainProblem},
            {.name = "domain_keyword", .member = &RouteRule::domain_keyword, .flags = A::Match, .column = "domain_keyword_json",
             .check = routeRuleDomainProblem},
            {.name = "domain_regex", .member = &RouteRule::domain_regex, .flags = A::Match, .column = "domain_regex_json",
             .check = routeRuleRegexProblem},
            {.name = "source_ip_cidr", .member = &RouteRule::source_ip_cidr, .flags = A::Match, .column = "source_ip_cidr_json",
             .check = routeRuleCidrProblem},
            {.name = "source_ip_is_private", .member = &RouteRule::source_ip_is_private, .input = trufalse, .flags = A::Match,
             .column = "source_ip_is_private"},
            {.name = "ip_cidr", .member = &RouteRule::ip_cidr, .flags = A::Match, .column = "ip_cidr_json", .check = routeRuleCidrProblem},
            {.name = "ip_is_private", .member = &RouteRule::ip_is_private, .input = trufalse, .flags = A::Match, .column = "ip_is_private"},
            {.name = "source_port", .member = &RouteRule::source_port, .flags = A::Match, .column = "source_port_json", .json = A::Json::Numbers,
             .check = [](const QString& v, Severity*) { return routeRulePortProblem(v, true); }},
            {.name = "source_port_range", .member = &RouteRule::source_port_range, .flags = A::Match, .column = "source_port_range_json",
             .check = routeRulePortRangeProblem},
            {.name = "port", .member = &RouteRule::port, .flags = A::Match, .column = "port_json", .json = A::Json::Numbers,
             .check = [](const QString& v, Severity*) { return routeRulePortProblem(v, false); }},
            {.name = "port_range", .member = &RouteRule::port_range, .flags = A::Match, .column = "port_range_json",
             .check = routeRulePortRangeProblem},
            {.name = "process_name", .member = &RouteRule::process_name, .flags = A::Match, .column = "process_name_json",
             .check = routeRuleProcessNameProblem},
            {.name = "process_path", .member = &RouteRule::process_path, .flags = A::Match, .column = "process_path_json",
             .check = routeRuleProcessPathProblem},
            {.name = "process_path_regex", .member = &RouteRule::process_path_regex, .flags = A::Match, .column = "process_path_regex_json",
             .check = routeRuleRegexProblem},
            {.name = "package_name", .member = &RouteRule::package_name, .flags = A::Match, .column = "package_name_json"},
            {.name = "wifi_ssid", .member = &RouteRule::wifi_ssid, .flags = A::Match | A::NotOnDarwin, .column = "wifi_ssid_json"},
            {.name = "wifi_bssid", .member = &RouteRule::wifi_bssid, .flags = A::Match | A::NotOnDarwin, .column = "wifi_bssid_json"},
            {.name = "rule_set", .member = &RouteRule::rule_set, .flags = A::Match, .column = "rule_set_json", .json = A::Json::RuleSets,
             .check = routeRuleRuleSetProblem},
            {.name = "invert", .member = &RouteRule::invert, .input = trufalse, .flags = A::Match, .column = "invert"},
            {.name = "action", .input = select, .flags = A::NoTab, .json = A::Json::ByHand,
             .values = [] {
                 auto actions = SingboxOptions::ActionTypes;
                 if (getOS() == Linux) actions.insert(1, "bypass");
                 return actions;
             },
             .check = [](const QString& v, Severity*) {
                 return Validation::OneOf(v, {"route", "route-options", "direct", "bypass", "reject", "hijack-dns", "sniff", "resolve"});
             }},
            {.name = "outbound", .input = select, .scope = {"route", "bypass"}, .json = A::Json::ByHand},
            {.name = "override_address", .member = &RouteRule::override_address, .scope = routeLike, .column = "override_address",
             .check = [](const QString& v, Severity*) { return routeRuleOverrideAddressProblem(v); }},
            // 0 is the core's "unset"; any other text keeps its tab, or a bad value would be hidden with it.
            {.name = "override_port", .member = &RouteRule::override_port, .scope = routeLike, .column = "override_port",
             .json = A::Json::PositiveNumber,
             .unset = [](const QString& v) { return v.trimmed().isEmpty() || v.trimmed() == QStringLiteral("0"); },
             .check = [](const QString& v, Severity*) { return v != QStringLiteral("0") ? Validation::Port(v) : QString(); }},
            {.name = "tls_fragment", .member = &RouteRule::tls_fragment, .input = trufalse, .scope = routeLike, .column = "tls_fragment"},
            // the core reads the delay only alongside tls_fragment
            {.name = "tls_fragment_fallback_delay", .member = &RouteRule::tls_fragment_fallback_delay, .scope = routeLike,
             .column = "tls_fragment_fallback_delay", .needs = "tls_fragment",
             .unset = [](const QString& v) { return QStringView(v).trimmed().isEmpty(); },
             .check = [](const QString& v, Severity*) { return Validation::Duration(v); }},
            {.name = "tls_record_fragment", .member = &RouteRule::tls_record_fragment, .input = trufalse, .scope = routeLike,
             .column = "tls_record_fragment"},
            {.name = "tls_spoof", .member = &RouteRule::tls_spoof, .scope = routeLike, .column = "tls_spoof", .check = routeRuleSpoofProblem},
            // the method only qualifies a spoof, the core rejects it standalone
            {.name = "tls_spoof_method", .member = &RouteRule::tls_spoof_method, .input = select, .scope = routeLike,
             .column = "tls_spoof_method", .needs = "tls_spoof",
             .values = [] { return QStringList{"", "wrong-sequence", "wrong-checksum", "wrong-ack", "wrong-md5", "wrong-timestamp"}; },
             .check = [](const QString& v, Severity*) {
                 return Validation::OneOf(v, {"wrong-sequence", "wrong-checksum", "wrong-ack", "wrong-md5", "wrong-timestamp"});
             }},
            {.name = "method", .member = &RouteRule::rejectMethod, .input = select, .scope = {"reject"}, .column = "reject_method",
             .json = A::Json::ByHand,
             .values = [] {
                 auto resp = SingboxOptions::rejectMethods;
                 resp.prepend("");
                 return resp;
             },
             // get_rule_json lowercases it.
             .check = [](const QString& v, Severity*) { return Validation::OneOf(v.toLower(), {"default", "drop", "reply"}); }},
            {.name = "no_drop", .member = &RouteRule::no_drop, .input = trufalse, .scope = {"reject"}, .column = "no_drop",
             .json = A::Json::ByHand},
            {.name = "override_destination", .member = &RouteRule::sniffOverrideDest, .input = trufalse, .scope = {"sniff"},
             .column = "sniff_override_dest"},
            {.name = "strategy", .member = &RouteRule::strategy, .input = select, .scope = {"resolve"}, .column = "strategy",
             .values = [] { return DomainStrategy::DomainStrategy; },
             .check = [](const QString& v, Severity*) {
                 return Validation::OneOf(v, {"as_is", "prefer_ipv4", "prefer_ipv6", "ipv4_only", "ipv6_only"});
             }},
        };
        return table;
    }

    QStringList RouteRule::action_scope(const QString& attr) {
        const RouteRuleAttribute* entry = routeRuleAttribute(attr);
        return entry != nullptr ? entry->scope : QStringList();
    }

    bool RouteRule::is_list_attribute(const QString& attr) {
        if (attr == QStringLiteral("sniffers")) return true;
        const RouteRuleAttribute* entry = routeRuleAttribute(attr);
        return entry != nullptr && std::holds_alternative<QStringList RouteRule::*>(entry->member);
    }

    bool RouteRule::is_match_attribute(const QString& attr) {
        const RouteRuleAttribute* entry = routeRuleAttribute(attr);
        return entry != nullptr && (entry->flags & RouteRuleAttribute::Match) != 0;
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
        const QString value = raw.trimmed();
        if (value.isEmpty()) return {};
        const RouteRuleAttribute* entry = routeRuleAttribute(attr);
        if (entry == nullptr || entry->check == nullptr) return {};
        Validation::Severity severity = Validation::Severity::Error;
        const QString message = entry->check(value, &severity);
        if (message.isEmpty()) return {};
        return {severity, attr, message, is_list_attribute(attr) ? value : QString(), {}};
    }

    QList<RuleIssue> RouteRule::validate() const {
        using Validation::Severity;
        QList<RuleIssue> out;
        if (type == endpointPreferredBy) return out;
        const QString act = effective_action();
        const auto applies = [&act](const QString& attr) {
            const QStringList scope = action_scope(attr);
            return scope.isEmpty() || scope.contains(act);
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
            const QStringList scope = action_scope(attr);
            if (scope.isEmpty() || scope.contains(act) || is_attribute_at_default(*this, attr)) continue;
            // The block or dns_out outbound is what makes a route rule a reject or hijack-dns one.
            if (attr == "outbound" && action == "route") continue;
            add(Severity::Warning, attr,
                QCoreApplication::translate("Validation", "A %1 rule ignores it; it applies only to these actions: %2.").arg(act, scope.join(QStringLiteral(", "))),
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
            if (!action_scope(attr).isEmpty()) out << RuleIssue{Severity::Warning, attr, {}, {}, "action"};
        }
        return out;
    }
}
