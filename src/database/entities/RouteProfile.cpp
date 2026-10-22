#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>
#include "include/database/entities/RouteProfile.h"
#include <array>
#include <iostream>
#include <optional>

#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/OutboundFactory.h"
#include "include/configs/generate.h"
#include "include/global/Configs.hpp"

#include <algorithm>

namespace Configs {
    bool isOutboundIDValid(int id) {
        switch (id) {
            case -1:
            case -2:
                return true;
            default:
                return Configs::dataManager->profilesRepo->GetProfile(id) != nullptr;
        }
    }

    int getOutboundID(const QString& name) {
        if (name == "proxy") return -1;
        if (name == "direct") return -2;
        if (name == "block") return blockID;
        if (name == "warp-bypass") return warpBypassID;
        if (const auto &profile = Configs::dataManager->profilesRepo->GetProfileByName(name)) return profile->id;

        return INVALID_ID;
    }

    static void collectRawOutboundIdsRec(const QJsonValue& node, QList<int>& out) {
        if (node.isObject()) {
            const QJsonObject o = node.toObject();
            for (auto it = o.begin(); it != o.end(); ++it) {
                if ((it.key() == "outbound" || it.key() == "final") && it.value().isDouble()) {
                    const int id = it.value().toInt();
                    if (!out.contains(id)) out.append(id);
                } else {
                    collectRawOutboundIdsRec(it.value(), out);
                }
            }
        } else if (node.isArray()) {
            for (const auto& e : node.toArray()) collectRawOutboundIdsRec(e, out);
        }
    }

    QList<int> RouteProfile::CollectRawOutboundIds(const QJsonObject& route) {
        QList<int> out;
        collectRawOutboundIdsRec(route, out);
        return out;
    }

    static QJsonValue translateRawOutboundsRec(const QJsonValue& node, const std::map<int, QString>& outboundMap) {
        if (node.isObject()) {
            const QJsonObject o = node.toObject();
            QJsonObject res;
            for (auto it = o.begin(); it != o.end(); ++it) {
                if ((it.key() == "outbound" || it.key() == "final") && it.value().isDouble()) {
                    const int id = it.value().toInt();
                    auto found = outboundMap.find(id);
                    res[it.key()] = found != outboundMap.end() ? QJsonValue(found->second) : QJsonValue("proxy");
                } else {
                    res[it.key()] = translateRawOutboundsRec(it.value(), outboundMap);
                }
            }
            return res;
        }
        if (node.isArray()) {
            QJsonArray res;
            for (const auto& e : node.toArray()) res.append(translateRawOutboundsRec(e, outboundMap));
            return res;
        }
        return node;
    }

    QJsonObject RouteProfile::TranslateRawOutbounds(const QJsonObject& route, const std::map<int, QString>& outboundMap) {
        return translateRawOutboundsRec(route, outboundMap).toObject();
    }

    static QJsonValue remapRawOutboundsByNameRec(const QJsonValue& node, const QJsonObject& names, QString* warnings) {
        if (node.isObject()) {
            const QJsonObject o = node.toObject();
            QJsonObject res;
            for (auto it = o.begin(); it != o.end(); ++it) {
                if ((it.key() == "outbound" || it.key() == "final") && it.value().isDouble()) {
                    const int id = it.value().toInt();
                    if (id < 0) { res[it.key()] = id; continue; }
                    const QString nm = names.value(QString::number(id)).toString();
                    std::shared_ptr<Profile> local;
                    if (!nm.isEmpty()) local = Configs::dataManager->profilesRepo->GetProfileByName(nm);
                    if (local) {
                        res[it.key()] = local->id;
                    } else {
                        res[it.key()] = static_cast<int>(proxyID);
                        if (warnings) warnings->append(QString("outbound \"%1\" not found, using proxy\n").arg(nm.isEmpty() ? QString::number(id) : nm));
                    }
                } else {
                    res[it.key()] = remapRawOutboundsByNameRec(it.value(), names, warnings);
                }
            }
            return res;
        }
        if (node.isArray()) {
            QJsonArray res;
            for (const auto& e : node.toArray()) res.append(remapRawOutboundsByNameRec(e, names, warnings));
            return res;
        }
        return node;
    }

    static QJsonObject remapRawOutboundsByName(const QJsonObject& route, const QJsonObject& names, QString* warnings) {
        return remapRawOutboundsByNameRec(route, names, warnings).toObject();
    }

    QList<std::shared_ptr<RouteRule>> RouteProfile::get_simple_rules() {
        struct RuleConfig {
            ruleType type;
            QStringView action;
            std::optional<int> outboundID;
        };

        static constexpr std::array kConfigs = {
            RuleConfig{simpleAddressProxy, u"route", proxyID},
            RuleConfig{simpleAddressBypass, u"route", directID},
            RuleConfig{simpleAddressBlock, u"reject", std::nullopt},

            RuleConfig{simpleProcessNameProxy, u"route", proxyID},
            RuleConfig{simpleProcessNameBypass, u"route", directID},
            RuleConfig{simpleProcessNameBlock, u"reject", std::nullopt},

            RuleConfig{simpleProcessPathProxy, u"route", proxyID},
            RuleConfig{simpleProcessPathBypass, u"route", directID},
            RuleConfig{simpleProcessPathBlock, u"reject", std::nullopt},

            RuleConfig{simpleAddressWarpBypass, u"route", warpBypassID},
            RuleConfig{simpleProcessNameWarpBypass, u"route", warpBypassID},
            RuleConfig{simpleProcessPathWarpBypass, u"route", warpBypassID},
        };

        QList<std::shared_ptr<RouteRule>> rules;
        rules.reserve(kConfigs.size());
        for (const auto &config : kConfigs) {
            auto rule = std::make_shared<RouteRule>();
            rule->type = config.type;
            rule->action = config.action.toString();
            rule->name = ruleTypeToString(config.type);
            if (config.outboundID) rule->outboundID = *config.outboundID;
            rules.append(std::move(rule));
        }
        return rules;
    }

    void RouteProfile::reset_simple_rule(std::shared_ptr<RouteRule>& rule) {
        auto cleanRules = get_simple_rules();
        for (auto &r : cleanRules) {
            if (r->type == rule->type) {
                r->disabled = rule->disabled;
                rule = std::move(r);
                return;
            }
        }
    }

    RouteProfile::RouteProfile(const RouteProfile& other) {
        id = other.id;
        name = QString(other.name);
        for (const auto& item: other.Rules) {
            Rules.push_back(std::make_shared<RouteRule>(*item));
        }
        defaultOutboundID = other.defaultOutboundID;
        isRaw = other.isRaw;
        rawRoute = other.rawRoute;
        preventModifications = other.preventModifications;
        isRemote = other.isRemote;
        remoteURL = other.remoteURL;
        autoUpdate = other.autoUpdate;
        remoteLastUpdate = other.remoteLastUpdate;
        endpointProfileIDs = other.endpointProfileIDs;
        innerHopEndpointIDs = other.innerHopEndpointIDs;
        customDns = other.customDns;
        dns = other.dns;
    }

    RouteDns RouteDns::Global() {
        const auto& settings = *Configs::dataManager->settingsRepo;
        RouteDns res;
        res.remote_dns = settings.remote_dns;
        res.remote_dns_disable_ipv6 = settings.remote_dns_disable_ipv6;
        res.direct_dns = settings.direct_dns;
        res.direct_dns_disable_ipv6 = settings.direct_dns_disable_ipv6;
        res.dns_final_out = settings.dns_final_out;
        res.enable_dns_routing = settings.enable_dns_routing;
        res.fake_dns = settings.fake_dns;
        res.fakeip_disable_ipv6 = settings.fakeip_disable_ipv6;
        return res;
    }

    RouteDns RouteDns::Current(bool* own) {
        const auto custom = Configs::dataManager->routesRepo->GetCustomDns(Configs::dataManager->settingsRepo->current_route_id);
        if (own) *own = custom.has_value();
        return custom ? *custom : Global();
    }

    QJsonObject RouteDns::ToJson() const {
        return {
            {"remote_dns", remote_dns},
            {"remote_dns_disable_ipv6", remote_dns_disable_ipv6},
            {"direct_dns", direct_dns},
            {"direct_dns_disable_ipv6", direct_dns_disable_ipv6},
            {"dns_final_out", dns_final_out},
            {"enable_dns_routing", enable_dns_routing},
            {"fake_dns", fake_dns},
            {"fakeip_disable_ipv6", fakeip_disable_ipv6},
        };
    }

    RouteDns RouteDns::FromJson(const QJsonObject& obj) {
        RouteDns res;
        res.remote_dns = obj.value("remote_dns").toString(res.remote_dns).trimmed();
        res.remote_dns_disable_ipv6 = obj.value("remote_dns_disable_ipv6").toBool(res.remote_dns_disable_ipv6);
        res.direct_dns = obj.value("direct_dns").toString(res.direct_dns).trimmed();
        res.direct_dns_disable_ipv6 = obj.value("direct_dns_disable_ipv6").toBool(res.direct_dns_disable_ipv6);
        res.dns_final_out = obj.value("dns_final_out").toString(res.dns_final_out);
        res.enable_dns_routing = obj.value("enable_dns_routing").toBool(res.enable_dns_routing);
        res.fake_dns = obj.value("fake_dns").toBool(res.fake_dns);
        res.fakeip_disable_ipv6 = obj.value("fakeip_disable_ipv6").toBool(res.fakeip_disable_ipv6);
        return res;
    }

    QString RouteDns::Problem() const {
        DnsAddress parsed;
        QString why;
        if (!ParseDnsAddress(remote_dns, &parsed, &why))
            return QCoreApplication::translate("Validation", "Remote DNS \"%1\": %2").arg(remote_dns, why);
        if (!ParseDnsAddress(direct_dns, &parsed, &why))
            return QCoreApplication::translate("Validation", "Direct DNS \"%1\": %2").arg(direct_dns, why);
        if (dns_final_out != "remote" && dns_final_out != "direct")
            return QCoreApplication::translate("Validation", "The default DNS server must be remote or direct, not \"%1\".").arg(dns_final_out);
        return {};
    }

    static void appendWarning(QString* warnings, const QString& msg) {
        if (warnings) warnings->append(msg + "\n");
    }

    static void routeProfileAdoptDns(RouteProfile& profile, const QJsonObject& root, QString* warnings) {
        if (!root.value("dns").isObject()) return;
        const auto dns = RouteDns::FromJson(root.value("dns").toObject());
        if (const auto problem = dns.Problem(); !problem.isEmpty()) {
            appendWarning(warnings, QCoreApplication::translate("Validation", "The profile's own DNS settings were dropped, so it uses the global ones: %1").arg(problem));
            return;
        }
        profile.customDns = true;
        profile.dns = dns;
    }

    // toString() is "" for a number, and the writer emits ports, ip_version and override_port as numbers.
    static QString jsonScalarText(const QJsonValue& val) {
        if (!val.isDouble()) return val.toString();
        const qint64 whole = val.toInteger();
        return static_cast<double>(whole) == val.toDouble() ? QString::number(whole) : QString::number(val.toDouble());
    }

    // name, type, outbound and disabled, plus exactly the keys RouteRule::set_field_value stores.
    static bool isStorableRuleKey(const QString& key) {
        static const QSet<QString> keys = {
            "name", "type", "outbound", "disabled",
            "ip_version", "network", "protocol", "inbound", "domain", "domain_suffix", "domain_keyword", "domain_regex",
            "source_ip_cidr", "source_ip_is_private", "ip_cidr", "ip_is_private", "source_port", "source_port_range",
            "port", "port_range", "process_name", "process_path", "process_path_regex", "package_name",
            "wifi_ssid", "wifi_bssid", "rule_set", "invert", "action", "method", "reject_method", "no_drop",
            "override_address", "override_port", "tls_fragment", "tls_fragment_fallback_delay", "tls_record_fragment",
            "tls_spoof", "tls_spoof_method", "override_destination", "strategy",
            "sniffers",
        };
        return keys.contains(key);
    }

    // Objects and null carry no value a rule field can hold.
    static std::optional<QString> routeProfileScalarText(const QJsonValue& val) {
        if (val.isBool()) return val.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        if (val.isString() || val.isDouble()) return jsonScalarText(val);
        return std::nullopt;
    }

    static QString routeProfileIssueText(const RuleIssue& issue) {
        if (issue.value.isEmpty()) return QCoreApplication::translate("Validation", "%1: %2").arg(issue.attr, issue.message);
        const QString value = issue.value.size() > 60 ? issue.value.left(59) + QChar(0x2026) : issue.value;
        return QCoreApplication::translate("Validation", "%1 (\"%2\"): %3").arg(issue.attr, value, issue.message);
    }

    // A bad condition drops the rule, which would otherwise match more; a bad option is cleared to the core's default.
    static bool routeProfileCheckImportedRule(RouteRule& rule, const QString& label, QString* warnings) {
        // Each pass repairs one error, and every repair removes it.
        for (int pass = 0; pass < 64; ++pass) {
            const auto issues = rule.validate();
            const auto error = std::find_if(issues.cbegin(), issues.cend(),
                                            [](const RuleIssue& issue) { return issue.severity == Validation::Severity::Error; });
            if (error == issues.cend()) {
                for (const auto& issue: issues)
                    appendWarning(warnings, QCoreApplication::translate("Validation", "Rule \"%1\": %2").arg(label, routeProfileIssueText(issue)));
                return true;
            }
            // Without invert one item less only narrows the rule; a custom rule whose condition loses every item would match more.
            if (!rule.invert && RouteRule::is_list_attribute(error->attr) && !error->value.isEmpty()) {
                QStringList kept;
                QList<RuleIssue> removed;
                for (const auto& item: rule.get_current_value_string(error->attr)) {
                    const auto issue = RouteRule::check_value(error->attr, item);
                    if (issue.message.isEmpty() || issue.severity != Validation::Severity::Error) kept << item;
                    else removed << issue;
                }
                if (kept.isEmpty() && rule.type == custom && RouteRule::is_match_attribute(error->attr)) {
                    appendWarning(warnings, QCoreApplication::translate("Validation", "Rule \"%1\" was dropped: %2").arg(label, routeProfileIssueText(*error)));
                    return false;
                }
                for (const auto& issue: removed)
                    appendWarning(warnings, QCoreApplication::translate("Validation", "Rule \"%1\": removed the item %2").arg(label, routeProfileIssueText(issue)));
                rule.set_field_value(error->attr, kept);
                continue;
            }
            if (error->attr == "action" || RouteRule::is_match_attribute(error->attr)) {
                appendWarning(warnings, QCoreApplication::translate("Validation", "Rule \"%1\" was dropped: %2").arg(label, routeProfileIssueText(*error)));
                return false;
            }
            if (error->attr == "outbound") {
                rule.outboundID = proxyID;
                appendWarning(warnings, QCoreApplication::translate("Validation", "Rule \"%1\" now uses the proxy outbound: %2").arg(label, error->message));
            } else {
                rule.clear_attribute_value(error->attr);
                appendWarning(warnings, QCoreApplication::translate("Validation", "Rule \"%1\": %2 was cleared: %3").arg(label, error->attr, error->message));
            }
        }
        return false;
    }

    // name/type are schema-only keys: skipped here, applied by the caller.
    // A key that cannot be stored drops the whole rule (nullptr): without that condition it would match more.
    // position is the rule's 1-based place in its array, naming a rule that has no name.
    static std::shared_ptr<RouteRule> parse_rule_object(const QJsonObject& obj, int position, QString* warnings) {
        const QString nm = obj.value("name").toString();
        const QString label = nm.isEmpty() ? QString("#%1").arg(position) : nm;
        for (const auto& key: obj.keys()) {
            if (isStorableRuleKey(key)) continue;
            appendWarning(warnings, QString("rule \"%1\" dropped: unsupported field \"%2\"").arg(label, key));
            return nullptr;
        }
        auto rule = std::make_shared<RouteRule>();
        // The caller sets it too; validation needs it now, since simple rules have fixed actions.
        rule->type = tokenToRuleType(obj.value("type").toString());
        for (const auto& key: obj.keys()) {
            if (key == "name" || key == "type") continue;
            auto val = obj.value(key);
            if (key == "disabled") {
                rule->disabled = val.toBool();
                continue;
            }
            if (key == "outbound") {
                if (val.isDouble()) {
                    const int id = val.toInt();
                    if (isOutboundIDValid(id)) {
                        rule->outboundID = id;
                    } else {
                        appendWarning(warnings, QString("outbound id %1 not found, using proxy").arg(id));
                        rule->outboundID = proxyID;
                    }
                } else if (val.isString()) {
                    const int id = getOutboundID(val.toString());
                    if (id != INVALID_ID) {
                        rule->outboundID = id;
                    } else {
                        appendWarning(warnings, QString("outbound \"%1\" not found, using proxy").arg(val.toString()));
                        rule->outboundID = proxyID;
                    }
                } else {
                    appendWarning(warnings, QCoreApplication::translate("Validation", "Rule \"%1\": the outbound is neither a profile name nor an id, so it uses proxy.").arg(label));
                    rule->outboundID = proxyID;
                }
                continue;
            }
            QStringList items;
            bool unusable = false;
            if (val.isArray()) {
                for (const auto& item: val.toArray()) {
                    const auto text = routeProfileScalarText(item);
                    if (!text) {
                        unusable = true;
                        break;
                    }
                    items << *text;
                }
            } else if (const auto text = routeProfileScalarText(val)) {
                items << *text;
            } else {
                unusable = true;
            }
            if (unusable) {
                if (RouteRule::is_match_attribute(key)) {
                    appendWarning(warnings, QCoreApplication::translate("Validation", "Rule \"%1\" was dropped: %2 holds something other than text, numbers or booleans.").arg(label, key));
                    return nullptr;
                }
                appendWarning(warnings, QCoreApplication::translate("Validation", "Rule \"%1\": %2 holds something other than text, numbers or booleans, so it was ignored.").arg(label, key));
                continue;
            }
            if (items.size() > 1 && !RouteRule::is_list_attribute(key)) {
                appendWarning(warnings, QCoreApplication::translate("Validation", "Rule \"%1\": %2 takes one value, so only \"%3\" was kept.").arg(label, key, items.first()));
            }
            rule->set_field_value(key, items);
        }
        if (rule->type == endpointPreferredBy) return rule;
        if (!routeProfileCheckImportedRule(*rule, label, warnings)) return nullptr;
        return rule;
    }

    QList<std::shared_ptr<RouteRule>> RouteProfile::parseJsonArray(const QJsonArray& arr, QString* parseError, QString* warnings) {
        if (arr.empty()) {
            parseError->append("Input is not a valid json array");
            return {};
        }

        auto rules = QList<std::shared_ptr<RouteRule>>();
        QString ruleWarnings;
        auto ruleID = 1;
        int position = 0;
        for (const auto& item: arr) {
            if (!item.isObject()) {
                parseError->append(QString("expected array of json objects but have member of type '%1'").arg(item.type()));
                return {};
            }
            const QJsonObject ro = item.toObject();
            auto rule = parse_rule_object(ro, ++position, &ruleWarnings);
            if (!rule) continue;
            const QString nm = ro.value("name").toString();
            rule->name = nm.isEmpty() ? ("imported rule #" + Int2String(ruleID++)) : nm;
            rules << rule;
        }
        // An array is nothing but its rules, so losing them all fails like an empty one (ruleWarnings then holds only drops).
        if (rules.isEmpty()) {
            parseError->append("No rule in the array can be imported:\n" + ruleWarnings.trimmed());
            return {};
        }
        if (warnings) warnings->append(ruleWarnings);

        return rules;
    }

    // The repo hands back the live profile, so strip a round-tripped clone of it instead.
    static QJsonObject routeProfileStrippedConfig(const std::shared_ptr<Profile>& ent) {
        const std::shared_ptr<outbound> clone(NewOutboundByType(ent->type));
        if (clone->invalid) return {};
        clone->ParseFromJson(ent->outbound->ExportToJson());
        clone->StripCredentials();
        return clone->ExportToJson();
    }

    // A chain also carries its hops in `list` order, so hops[i] describes config["list"][i].
    static QJsonObject routeProfileEndpointToJson(int id, bool innerHops, QString* warnings) {
        const auto ent = Configs::dataManager->profilesRepo->GetProfile(id);
        if (ent == nullptr || ent->outbound == nullptr) {
            appendWarning(warnings, QString("endpoint profile id %1 no longer exists, not shared").arg(id));
            return {};
        }
        const QString label = ent->outbound->DisplayTypeAndName();
        QList<std::shared_ptr<Profile>> hops;
        if (const auto ch = ent->Chain(); ch != nullptr) {
            for (const int hopID: ch->list) {
                if (auto hop = Configs::dataManager->profilesRepo->GetProfile(hopID);
                    hop != nullptr && hop->outbound != nullptr) hops << hop;
            }
            if (hops.isEmpty() || hops.size() != ch->list.size()) {
                appendWarning(warnings, QString("endpoint %1 has missing hops, not shared").arg(label));
                return {};
            }
        }
        auto strippable = [](const std::shared_ptr<Profile>& p) {
            return p->outbound->SupportsCredentialStrip();
        };
        if (!strippable(ent) || !std::all_of(hops.begin(), hops.end(), strippable)) {
            appendWarning(warnings, QString("endpoint %1 uses a protocol whose credentials cannot be cleared, not shared").arg(label));
            return {};
        }

        QJsonObject entry;
        entry["id"] = id;
        entry["config"] = routeProfileStrippedConfig(ent);
        if (!hops.isEmpty()) {
            QJsonArray hopArr;
            for (const auto& hop: hops) {
                hopArr.append(QJsonObject{{"id", hop->id}, {"config", routeProfileStrippedConfig(hop)}});
            }
            entry["hops"] = hopArr;
            if (innerHops) entry["inner_hops"] = true;
        }
        return entry;
    }

    static QJsonArray routeProfileEndpointsToJson(const QList<int>& ids, const QList<int>& innerHopIDs, QString* warnings) {
        QJsonArray arr;
        for (const int id: ids) {
            if (auto entry = routeProfileEndpointToJson(id, innerHopIDs.contains(id), warnings); !entry.isEmpty()) arr.append(entry);
        }
        return arr;
    }

    static QString routeProfileIdentityKey(const std::shared_ptr<Profile>& ent) {
        return ent->type + "|" + QString::fromUtf8(QJsonDocument(ent->outbound->ExportIdentity()).toJson(QJsonDocument::Compact));
    }

    static int routeProfileMatchLocal(const std::shared_ptr<Profile>& candidate) {
        const QString key = routeProfileIdentityKey(candidate);
        for (const int id: Configs::dataManager->profilesRepo->GetProfileIdsByType(candidate->type)) {
            const auto ent = Configs::dataManager->profilesRepo->GetProfile(id);
            if (ent == nullptr || ent->outbound == nullptr) continue;
            if (routeProfileIdentityKey(ent) == key) return id;
        }
        return -1;
    }

    // Errors that StripCredentials causes on the sharer's side: the material it cleared is missing, so these ask to re-enter it.
    static bool routeProfileStrippedSecret(const QString& type, const Validation::Issue& issue) {
        if (type == "openvpn") {
            // The wrap type's own enum check reports on this field too, and that one is a real error.
            if (issue.field == "tls.control_wrap.type") return Validation::OneOf(issue.value, {"tls_auth", "tls_crypt", "tls_crypt_v2"}).isEmpty();
            return issue.field == "static_key" || issue.field == "tls.certificate" || issue.field == "tls.client_certificate";
        }
        if (type == "openconnect") return issue.field == "tls.client_certificate" || issue.field == "tls.mca_certificate";
        return false;
    }

    // hopMap rewrites a chain's machine-local hop ids onto the ones created here.
    static int routeProfileAdoptConfig(const QJsonObject& config, QString* warnings, const QMap<int, int>* hopMap) {
        const auto type = config.value("type").toString();
        // Only a top-level chain gets its hop ids remapped onto profiles created here.
        if (const auto refusal = RefusedForeignType(type); !refusal.isEmpty() && (type != "chain" || hopMap == nullptr)) {
            appendWarning(warnings, refusal);
            return -1;
        }
        auto ent = ProfilesRepo::NewProfile(type);
        if (ent == nullptr || ent->outbound == nullptr || ent->outbound->invalid) {
            appendWarning(warnings, QString("shared endpoint uses unknown protocol \"%1\", dropped").arg(type));
            return -1;
        }
        Validation::Issues issues;
        bool parsed = false;
        {
            Configs::ParseDiag::Scope scope;
            parsed = ent->outbound->ParseFromJson(config);
            issues << scope.take();
        }
        if (!parsed) {
            if (!Validation::HasError(issues))
                issues << Validation::Issue{Validation::Severity::Error, {}, QCoreApplication::translate("Validation", "Could not be parsed."), {}};
        } else {
            ent->outbound->StripForeignState(issues);
            Validation::Issues model;
            ent->outbound->Validate(model);
            for (auto& issue: model) {
                if (issue.severity == Validation::Severity::Error && routeProfileStrippedSecret(type, issue)) {
                    issue.severity = Validation::Severity::Warning;
                    issue.message = QCoreApplication::translate("Validation", "%1 Shared endpoints arrive without their keys and passwords.").arg(issue.message);
                }
                issues << issue;
            }
        }
        for (const auto& issue: issues) {
            const auto message = issue.severity == Validation::Severity::Error
                ? QCoreApplication::translate("Validation", "The shared endpoint %1 was not imported: %2")
                : QCoreApplication::translate("Validation", "The shared endpoint %1 was imported with a warning: %2");
            const auto text = issue.field.isEmpty() ? issue.message : QCoreApplication::translate("Validation", "%1: %2").arg(issue.field, issue.message);
            appendWarning(warnings, message.arg(ent->outbound->DisplayTypeAndName(), text));
        }
        if (Validation::HasError(issues)) return -1;
        if (const auto ch = ent->Chain(); ch != nullptr) {
            QList<int> mapped;
            for (const int hopID: ch->list) {
                if (hopMap == nullptr || !hopMap->contains(hopID)) return -1;
                mapped << hopMap->value(hopID);
            }
            ch->list = mapped;
        }
        if (const int existing = routeProfileMatchLocal(ent); existing >= 0) return existing;
        if (!Configs::dataManager->profilesRepo->AddProfile(ent)) {
            appendWarning(warnings, QString("could not create endpoint profile \"%1\"").arg(ent->outbound->DisplayName()));
            return -1;
        }
        appendWarning(warnings, QString("created endpoint profile %1").arg(ent->outbound->DisplayTypeAndName()));
        return ent->id;
    }

    static int routeProfileAdoptEndpoint(const QJsonObject& entry, QString* warnings, QMap<int, int>* outHopMap = nullptr) {
        QMap<int, int> hopMap;
        for (const auto& item: entry.value("hops").toArray()) {
            const QJsonObject hop = item.toObject();
            const int original = hop.value("id").toInt(INVALID_ID);
            const int local = original == INVALID_ID ? -1 : routeProfileAdoptConfig(hop.value("config").toObject(), warnings, nullptr);
            if (local < 0) {
                appendWarning(warnings, "a shared endpoint chain has an unusable hop, dropped");
                return -1;
            }
            hopMap[original] = local;
        }
        if (outHopMap != nullptr) *outHopMap = hopMap;
        return routeProfileAdoptConfig(entry.value("config").toObject(), warnings, &hopMap);
    }

    // *idMap is original id -> local id, hops included, so the paired rules can be remapped.
    static QList<int> routeProfileEndpointsFromJson(const QJsonArray& arr, QString* warnings, bool materialize,
                                                   QMap<int, int>* idMap, QList<int>* innerHopIDs) {
        QList<int> ids;
        for (const auto& item: arr) {
            int originalID = INVALID_ID;
            int localID = -1;
            bool innerHops = false;
            QMap<int, int> hopMap;
            if (item.isDouble()) {
                originalID = item.toInt(INVALID_ID);
                localID = originalID;
            } else if (item.isObject()) {
                const QJsonObject entry = item.toObject();
                originalID = entry.value("id").toInt(INVALID_ID);
                innerHops = entry.value("inner_hops").toBool();
                if (!materialize) continue;
                if (originalID != INVALID_ID) localID = routeProfileAdoptEndpoint(entry, warnings, &hopMap);
            }
            if (originalID == INVALID_ID || localID < 0 || ids.contains(localID)) continue;
            const auto profile = Configs::dataManager->profilesRepo->GetProfile(localID);
            if (!CanBeAuxEndpoint(profile)) {
                appendWarning(warnings, QString("endpoint profile id %1 not usable here, dropped").arg(originalID));
                continue;
            }
            ids << localID;
            if (innerHops && innerHopIDs != nullptr) *innerHopIDs << localID;
            if (idMap) {
                (*idMap)[originalID] = localID;
                for (auto it = hopMap.cbegin(); it != hopMap.cend(); ++it) (*idMap)[it.key()] = it.value();
            }
        }
        return ids;
    }

    QJsonObject RouteProfile::ToShareObject(QString* warnings) {
        QJsonObject root;
        root["kind"] = "throne-route-profile";
        root["v"] = 1;
        root["name"] = name;
        if (const auto active = ActiveDns()) root["dns"] = active->ToJson();
        QJsonArray endpointsArr;
        if (!endpointProfileIDs.isEmpty()) {
            endpointsArr = routeProfileEndpointsToJson(endpointProfileIDs, innerHopEndpointIDs, warnings);
            if (!endpointsArr.isEmpty()) root["endpoints"] = endpointsArr;
        }
        if (isRaw) {
            root["raw"] = true;
            root["prevent_modifications"] = preventModifications;
            const auto routeObj = QString2QJsonObject(rawRoute);
            root["route"] = routeObj;
            // id -> name of the referenced server profiles, so the importer can re-resolve them on another machine.
            QJsonObject names;
            for (const int oid : CollectRawOutboundIds(routeObj)) {
                if (oid < 0) continue; // predefined outbounds (proxy/direct/warp-bypass) are stable
                if (auto p = Configs::dataManager->profilesRepo->GetProfile(oid))
                    names[QString::number(oid)] = p->name;
            }
            root["outbound_names"] = names;
            return root;
        }
        root["default_outbound"] = outboundIDToString(defaultOutboundID);
        QSet<int> sharedEndpoints;
        for (const auto& entry: endpointsArr) {
            const QJsonObject obj = entry.toObject();
            sharedEndpoints << obj.value("id").toInt(INVALID_ID);
            // The inner hops travel with the entry, so their rules may travel too.
            if (!obj.value("inner_hops").toBool()) continue;
            for (const auto& hop: obj.value("hops").toArray()) sharedEndpoints << hop.toObject().value("id").toInt(INVALID_ID);
        }
        QJsonArray rulesArr;
        for (const auto& rule: Rules) {
            if (rule->type != custom && rule->isEmpty()) continue;
            // a rule and its endpoint drop or survive together
            if (rule->type == endpointPreferredBy && !sharedEndpoints.contains(rule->outboundID)) continue;
            auto obj = rule->to_share_json();
            if (obj.isEmpty()) continue;
            rulesArr.append(obj);
        }
        root["rules"] = rulesArr;
        return root;
    }

    QString RouteProfile::ToShareLink(QString* warnings) {
        const auto json = QJsonDocument(ToShareObject(warnings)).toJson(QJsonDocument::Compact);
        const auto b64 = json.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        return QStringLiteral("throne://route/") + QString::fromLatin1(b64);
    }

    std::shared_ptr<RouteProfile> RouteProfile::FromShareInput(const QString& input, QString* fatalError, QString* warnings, bool* wasOldArray, bool materializeEndpoints) {
        if (wasOldArray) *wasOldArray = false;
        QString text = input.trimmed();
        if (text.isEmpty()) {
            fatalError->append("Empty input");
            return nullptr;
        }

        if (text.startsWith("throne://route/", Qt::CaseInsensitive)) {
            const QUrl u(text);
            if (!u.isValid()) {
                fatalError->append("Deep link is invalid");
                return nullptr;
            }
            text = u.path().mid(1);
            if (text.isEmpty()) {
                fatalError->append("Deep link has no data");
                return nullptr;
            }
        }

        QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
        if (doc.isNull()) {
            doc = QJsonDocument::fromJson(QByteArray::fromBase64(text.toUtf8(), QByteArray::Base64UrlEncoding));
            if (doc.isNull())
                doc = QJsonDocument::fromJson(QByteArray::fromBase64(text.toUtf8()));
        }
        if (doc.isNull()) {
            fatalError->append("Input is not valid JSON, base64, or a Throne route link");
            return nullptr;
        }

        if (doc.isObject()) {
            const QJsonObject root = doc.object();
            if (root.value("kind").toString() != QStringLiteral("throne-route-profile")) {
                fatalError->append("Unrecognized route object");
                return nullptr;
            }
            if (root.value("raw").toBool()) {
                auto profile = std::make_shared<RouteProfile>();
                profile->id = -1;
                profile->isRaw = true;
                profile->name = root.value("name").toString();
                profile->preventModifications = root.value("prevent_modifications").toBool();
                profile->endpointProfileIDs = routeProfileEndpointsFromJson(root.value("endpoints").toArray(), warnings, materializeEndpoints, nullptr, &profile->innerHopEndpointIDs);
                QJsonObject routeObj = root.value("route").toObject();
                routeObj = remapRawOutboundsByName(routeObj, root.value("outbound_names").toObject(), warnings);
                profile->rawRoute = QJsonObject2QString(routeObj, false);
                routeProfileAdoptDns(*profile, root, warnings);
                return profile;
            }
            auto profile = std::make_shared<RouteProfile>();
            profile->id = -1;
            profile->name = root.value("name").toString();
            const QString defaultOutbound = root.value("default_outbound").toString();
            profile->defaultOutboundID = stringToOutboundID(defaultOutbound);
            if (!defaultOutbound.isEmpty() && outboundIDToString(profile->defaultOutboundID) != defaultOutbound) {
                appendWarning(warnings, QCoreApplication::translate("Validation", "The default outbound \"%1\" is unknown, so proxy is used.").arg(defaultOutbound));
            }
            QMap<int, int> endpointIDMap;
            profile->endpointProfileIDs = routeProfileEndpointsFromJson(root.value("endpoints").toArray(), warnings, materializeEndpoints, &endpointIDMap, &profile->innerHopEndpointIDs);
            int fallbackNum = 1;
            int position = 0;
            for (const auto& v: root.value("rules").toArray()) {
                ++position;
                if (!v.isObject()) {
                    appendWarning(warnings, QCoreApplication::translate("Validation", "Rule #%1 is not a JSON object and was skipped.").arg(position));
                    continue;
                }
                const QJsonObject ro = v.toObject();
                const ruleType type = tokenToRuleType(ro.value("type").toString());
                // an endpoint rule's outbound is the sharer's profile id; it must survive the proxy fallback.
                // Shared as {name, type, outbound} it is never dropped, and SyncEndpointRules re-pairs one that is.
                auto rule = parse_rule_object(ro, position, type == endpointPreferredBy ? nullptr : warnings);
                if (!rule) continue;
                rule->type = type;
                if (type == endpointPreferredBy) {
                    rule->outboundID = endpointIDMap.value(ro.value("outbound").toInt(INVALID_ID), INVALID_ID);
                }
                rule->name = ro.value("name").toString();
                if (rule->name.isEmpty()) rule->name = "rule_" + Int2String(fallbackNum++);
                profile->Rules << rule;
            }
            profile->SyncEndpointRules();
            routeProfileAdoptDns(*profile, root, warnings);
            return profile;
        }

        if (doc.isArray()) {
            QString fe;
            auto rules = parseJsonArray(doc.array(), &fe, warnings);
            if (!fe.isEmpty()) {
                fatalError->append(fe);
                return nullptr;
            }
            auto profile = std::make_shared<RouteProfile>();
            profile->id = -1;
            profile->Rules = rules;
            if (wasOldArray) *wasOldArray = true;
            return profile;
        }

        fatalError->append("Unsupported input");
        return nullptr;
    }

    QList<std::shared_ptr<RouteProfile>> RouteProfile::FromRemoteRoutesLink(const QString& input, bool* wasRemoteRouteLink, QString* error) {
        if (wasRemoteRouteLink) *wasRemoteRouteLink = false;
        const QString text = input.trimmed();
        if (!text.startsWith("throne://remoteroute/", Qt::CaseInsensitive)) return {};
        if (wasRemoteRouteLink) *wasRemoteRouteLink = true;

        const QUrl u(text);
        if (!u.isValid()) {
            if (error) *error = "Deep link is invalid";
            return {};
        }
        QString base64 = u.path().mid(1);
        if (base64.isEmpty()) {
            if (error) *error = "Deep link has no data";
            return {};
        }
        const QString data = DecodeB64IfValid(base64);
        if (data.isEmpty()) {
            if (error) *error = "Base64 is invalid.";
            return {};
        }

        QList<std::shared_ptr<RouteProfile>> res;
        for (const auto& v : data.split('\n', Qt::SkipEmptyParts)) {
            const QString eurl = v.trimmed();
            if (!eurl.startsWith("http://", Qt::CaseInsensitive) && !eurl.startsWith("https://", Qt::CaseInsensitive)) continue;
            const QUrl link(eurl);
            if (!link.isValid()) continue;

            auto profile = std::make_shared<RouteProfile>();
            profile->id = -1;
            profile->isRemote = true;
            profile->remoteURL = link.toString(QUrl::RemoveFragment);
            profile->name = link.fragment();
            if (profile->name.isEmpty()) profile->name = QUrl(eurl).host();
            res << profile;
        }
        if (res.isEmpty() && error) *error = "The link did not contain any valid http(s) routing profile URLs.";
        return res;
    }

    QJsonArray RouteProfile::get_route_rules(bool forView, std::map<int, QString> outboundMap) {
        QJsonArray res;
        bool added_adblock = false;
        auto createAdblockRule = []() -> QJsonObject {
            QJsonObject obj;
            obj["action"] = "reject";
            QJsonArray jarray;
            jarray.append("throne-adblocksingbox");
            obj["rule_set"] = jarray;
            return obj;
        };
        for (const auto &item: Rules) {
            if (item->disabled || (item->type != custom && item->isEmpty())) continue;
            auto outboundTag = QString();
            if (outboundMap.contains(item->outboundID)) outboundTag = outboundMap[item->outboundID];
            // an endpoint that left the list has no tag to gate on; skip instead of aborting the build
            if (!forView && item->type == endpointPreferredBy && outboundTag.isEmpty()) {
                MW_show_log("Skipping an endpoint rule whose endpoint is no longer in the routing profile");
                continue;
            }
            auto rule_json = item->get_rule_json(forView, outboundTag);
            if (rule_json.empty()) {
                MW_show_log("Aborted generating routing section, an error has occurred");
                return {};
            }
            if (!added_adblock && Configs::dataManager->settingsRepo->adblock_enable && rule_json["action"] == "route") {
                res += createAdblockRule();
                added_adblock = true;
            }                
            res += rule_json;
        }
        if (!added_adblock && Configs::dataManager->settingsRepo->adblock_enable)
            res += createAdblockRule();

        return res;
    }

    std::shared_ptr<RouteRule> RouteProfile::MakeEndpointRule(int endpointProfileID) {
        auto rule = std::make_shared<RouteRule>();
        rule->type = endpointPreferredBy;
        rule->outboundID = endpointProfileID;
        rule->name = ruleTypeToString(endpointPreferredBy);
        if (const auto prof = Configs::dataManager->profilesRepo->GetProfile(endpointProfileID);
            prof != nullptr && prof->outbound != nullptr)
            rule->name = QObject::tr("%1 route prefer").arg(prof->outbound->DisplayName());
        return rule;
    }

    QList<int> RouteProfile::endpointRuleTargets() const {
        QList<int> targets;
        for (const int id: endpointProfileIDs) {
            if (targets.contains(id)) continue;
            targets << id;
            if (!innerHopEndpointIDs.contains(id)) continue;
            for (const int hopID: AuxEndpointInnerHops(id)) {
                if (!targets.contains(hopID)) targets << hopID;
            }
        }
        return targets;
    }

    void RouteProfile::SyncEndpointRules() {
        if (isRaw) return;
        const auto targets = endpointRuleTargets();
        QList<std::shared_ptr<RouteRule>> kept;
        QSet<int> paired;
        for (const auto& rule: Rules) {
            if (rule->type == endpointPreferredBy) {
                if (!targets.contains(rule->outboundID) || paired.contains(rule->outboundID)) continue;
                paired << rule->outboundID;
            }
            kept << rule;
        }
        for (const int id: targets) {
            if (!paired.contains(id)) kept << MakeEndpointRule(id);
        }
        Rules = kept;
    }

    std::shared_ptr<RouteProfile> RouteProfile::GetDefaultChain() {
        auto defaultChain = std::make_shared<RouteProfile>();
        defaultChain->name = "Default";
        auto defaultRule = std::make_shared<RouteRule>();
        defaultRule->name = "Route DNS";
        defaultRule->action = "hijack-dns";
        defaultRule->protocol = "dns";
        defaultChain->Rules << defaultRule;
        return defaultChain;
    }

    std::shared_ptr<QList<int>> RouteProfile::get_used_outbounds() {
        auto res = std::make_shared<QList<int>>();
        if (isRaw) {
            // Raw ids must be collected too, so their servers get built and their domains added to direct DNS.
            *res = CollectRawOutboundIds(QString2QJsonObject(rawRoute));
            return res;
        }
        for (const auto& item: Rules) {
            // its id names an endpoint built from endpointProfileIDs, not a routing outbound
            if (item->disabled || item->type == endpointPreferredBy) continue;
            // Only these emit their outbound; a stale id left on another action must not fail the build.
            if (const auto action = item->effective_action(); action != "route" && action != "bypass") continue;
            res->push_back(item->outboundID);
        }
        return res;
    }

    std::shared_ptr<QStringList> RouteProfile::get_used_rule_sets() {
        auto res = std::make_shared<QStringList>();
        for (const auto& item: Rules) {
            if (item->disabled) continue;
            for (const auto& ruleItem: item->rule_set) {
                res->push_back(ruleItem);
            }
        }
        return res;
    }

    QStringList RouteProfile::get_direct_sites() {
        auto res = QStringList();
        for (const auto& item: Rules) {
            if (!item->disabled && item->outboundID == directID && item->action == "route") {
                for (const auto& rset: item->rule_set) {
                    if (rset.startsWith("geosite-")) res << QString("ruleset:" + rset);
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

    QStringList RouteProfile::get_proxy_sites() {
        auto res = QStringList();
        for (const auto& item: Rules) {
            if (!item->disabled && item->outboundID == proxyID && item->action == "route") {
                for (const auto& rset: item->rule_set) {
                    if (rset.startsWith("geosite-")) res << QString("ruleset:" + rset);
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

    QStringList RouteProfile::get_direct_ips()
    {
        auto res = QStringList();
        for (const auto& item: Rules) {
            if (!item->disabled && item->outboundID == directID && item->action == "route") {
                for (const auto& rset: item->rule_set) {
                    if (rset.startsWith("geoip-")) res << QString("ruleset:" + rset);
                }
                for (const auto& domain: item->ip_cidr) {
                    res << QString("ip:" + domain);
                }
            }
        }
        return res;
    }

    QStringList RouteProfile::get_hijacked_ips()
    {
        auto res = QStringList();
        for (const auto& item: Rules) {
            if (item->disabled || (item->action == "route" && item->outboundID == directID)) continue;
            if (item->action != "route" && item->action != "reject") continue;
            // ip_is_private covers every range the Tun bypass carves out, so it hijacks all of them at once.
            if (item->ip_is_private) res << dataManager->settingsRepo->vpn_private_ranges;
            for (const auto& cidr: item->ip_cidr) res << cidr;
        }
        return res;
    }

    bool RouteProfile::IsEmpty() {
        if (isRaw) return rawRoute.trimmed().isEmpty();
        for (const auto& item: Rules) {
            if (!item->isEmpty()) return false;
        }
        return true;
    }

    void RouteProfile::ResetRules() {
        Rules.clear();
    }

    void RouteProfile::ResetSimpleRule(ruleType type) {
        for (std::shared_ptr<RouteRule> &item: Rules) {
            if (item->type == type) {
                reset_simple_rule(item);
                return;
            }
        }
        auto cleanRules = get_simple_rules();
        for (std::shared_ptr<RouteRule> &item: cleanRules) {
            if (item->type == type) {
                Rules << item;
                return;
            }
        }
    }

    QString RouteProfile::GetSimpleRules(simpleAction action)
    {
        QList<int> types;
        if (action == proxy) {
            types << simpleAddressProxy;
            types << simpleProcessNameProxy;
            types << simpleProcessPathProxy;
        } else if (action == bypass) {
            types << simpleAddressBypass;
            types << simpleProcessNameBypass;
            types << simpleProcessPathBypass;
        } else if (action == warpBypass) {
            types << simpleAddressWarpBypass;
            types << simpleProcessNameWarpBypass;
            types << simpleProcessPathWarpBypass;
        } else {
            types << simpleAddressBlock;
            types << simpleProcessNameBlock;
            types << simpleProcessPathBlock;
        }
        QString res;
        for (const auto& item: Rules)
        {
            if (types.contains(item->type))
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


    QString RouteProfile::UpdateSimpleRules(const QString& content, simpleAction action, const QString& label)
    {
        QString res;
        auto items = content.split("\n");
        QList<ruleType> types;
        if (action == proxy) {
            types << simpleAddressProxy;
            types << simpleProcessNameProxy;
            types << simpleProcessPathProxy;
        } else if (action == bypass) {
            types << simpleAddressBypass;
            types << simpleProcessNameBypass;
            types << simpleProcessPathBypass;
        } else if (action == warpBypass) {
            types << simpleAddressWarpBypass;
            types << simpleProcessNameWarpBypass;
            types << simpleProcessPathWarpBypass;
        } else {
            types << simpleAddressBlock;
            types << simpleProcessNameBlock;
            types << simpleProcessPathBlock;
        }
        for (auto t : types) {
            ResetSimpleRule(t);
        }
        for (qsizetype i = 0; i < items.size(); ++i) {
            const QString raw = items[i].trimmed();
            if (raw.isEmpty()) continue;
            const auto reject = [&](const QString& reason) {
                res += QCoreApplication::translate("Validation", "Line %1 of the %2 rules (\"%3\"): %4")
                           .arg(QString::number(i + 1), label.isEmpty() ? simpleActionToString(action) : label, raw, reason) + "\n";
            };
            auto type = get_rule_type(raw, action);
            if (type == custom) {
                reject(CheckSimpleRule(raw).message);
                continue;
            }
            auto rule = get_simple_rule_by_type(type);
            if (!rule) {
                res += "internal error, failed to get rule for: " + ruleTypeToString(type) + "\n";
                continue;
            }
            if (const auto reason = add_simple_rule(raw, rule, type); !reason.isEmpty()) reject(reason);
        }
        FilterEmptyRules();
        return res;
    }

    RuleIssue RouteProfile::CheckSimpleRule(const QString& rawLine) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) return {};
        static const QHash<QString, QString> attrs = {
            {"domain", "domain"}, {"suffix", "domain_suffix"}, {"keyword", "domain_keyword"}, {"regex", "domain_regex"},
            {"ruleset", "rule_set"}, {"ip", "ip_cidr"}, {"processName", "process_name"}, {"processPath", "process_path"},
        };
        const auto colon = line.indexOf(':');
        const QString prefix = colon < 0 ? QString() : line.left(colon).trimmed();
        const auto attr = attrs.constFind(prefix);
        if (attr == attrs.constEnd()) {
            return {Validation::Severity::Error, {},
                    QCoreApplication::translate("Validation", "Start the line with domain:, suffix:, keyword:, regex:, ruleset:, ip:, processName: or processPath:."),
                    {}, {}};
        }
        QString value = line.mid(colon + 1).trimmed();
        if (value.isEmpty())
            return {Validation::Severity::Error, *attr, QCoreApplication::translate("Validation", "Add a value after \"%1:\".").arg(prefix), {}, {}};
        // Stored lowercased by add_simple_address_rule.
        if (prefix == "domain" || prefix == "suffix" || prefix == "keyword") value = value.toLower();
        return RouteRule::check_value(*attr, value);
    }

    void RouteProfile::FilterEmptyRules() {
        QList<std::shared_ptr<RouteRule>> newRules;
        for (const auto& rule : Rules) {
            if (!rule->isEmpty()) newRules.append(rule);
        }
        Rules = newRules;
    }

    bool RouteProfile::AppendSimpleRule(const QString& rawRule, const SimpleTarget& target, QString* why) {
        const QString raw = rawRule.trimmed();
        if (raw.isEmpty()) return false;

        auto type = get_rule_type(raw, target.action);
        if (type == custom) {
            if (why != nullptr) *why = CheckSimpleRule(raw).message;
            return false;
        }

        const auto own = rules_for(raw, target);
        auto rule = own.isEmpty() ? nullptr : own.first();
        const bool isNewRule = rule == nullptr;
        if (isNewRule && target.isServer()) {
            const auto server = Configs::dataManager->profilesRepo->GetProfile(target.serverID);
            if (!server) return false;
            rule = std::make_shared<RouteRule>();
            rule->outboundID = target.serverID;
            // Named as the server list names it, so an unnamed profile still gives its type and address.
            const QString serverName = server->name.trimmed().isEmpty() && server->outbound ? server->outbound->DisplayTypeAndName() : server->name;
            // Names stay unique, or the editor renames the rule to rule_<random>.
            const auto kind = get_rule_type(raw, proxy);
            const QString base = QString(kind == simpleAddressProxy ? "%1 (address)" : kind == simpleProcessNameProxy ? "%1 (process)" : "%1 (process path)")
                                     .arg(serverName);
            rule->name = base;
            for (int n = 2; std::any_of(Rules.cbegin(), Rules.cend(), [&rule](const auto& r) { return r->name == rule->name; }); ++n)
                rule->name = QString("%1 %2").arg(base, QString::number(n));
        } else if (isNewRule) {
            for (auto &item : get_simple_rules()) {
                if (item->type == type) {
                    rule = item;
                    break;
                }
            }
        }
        if (!rule) return false;

        if (const auto reason = add_simple_rule(raw, rule, type); !reason.isEmpty()) {
            if (why != nullptr) *why = reason;
            return false;
        }
        // Published only once the value stuck, so a rejected line needs no FilterEmptyRules() sweep to undo it.
        if (isNewRule) Rules.append(rule);
        return true;
    }

    bool RouteProfile::HasSimpleRule(const QString& rawRule, const SimpleTarget& target) {
        const QString raw = rawRule.trimmed();
        for (const auto& rule : rules_for(raw, target)) {
            QString value;
            const auto* values = simple_rule_values(raw, *rule, &value);
            if (values && values->contains(value)) return true;
        }
        return false;
    }

    bool RouteProfile::RemoveSimpleRule(const QString& rawRule, const SimpleTarget& target) {
        const QString raw = rawRule.trimmed();

        // Every rule of the target, not just the first: an imported profile may carry duplicates, and a toggle must really switch off.
        bool removed = false;
        for (const auto& rule : rules_for(raw, target)) {
            QString value;
            auto* values = simple_rule_values(raw, *rule, &value);
            if (!values || values->removeAll(value) == 0) continue;
            removed = true;
            // Only the rule just emptied: a FilterEmptyRules() sweep would take unrelated empty rules with it.
            // A server rule's outbound makes isEmpty() see it as set, but with no condition left it would catch all traffic.
            if (target.isServer() ? !rule->has_match_conditions() : rule->isEmpty()) Rules.removeOne(rule);
        }
        return removed;
    }

    QList<int> RouteProfile::ServersHoldingSimpleRule(const QString& rawRule) {
        const QString raw = rawRule.trimmed();
        QList<int> servers;
        for (const auto& rule : Rules) {
            if (servers.contains(rule->outboundID) || !is_server_rule_for(*rule, raw)) continue;
            QString value;
            const auto* values = simple_rule_values(raw, *rule, &value);
            if (values && values->contains(value)) servers << rule->outboundID;
        }
        return servers;
    }

    QString RouteProfile::CoveringSimpleRule(const QString& rawRule, const SimpleTarget& target, SimpleTarget* coveringTarget) {
        const QString raw = rawRule.trimmed();
        const auto colonIdx = raw.indexOf(':');
        if (colonIdx == -1) return {};
        const QString prefix = raw.left(colonIdx).trimmed();
        const QString value = raw.mid(colonIdx + 1).trimmed().toLower();
        if (value.isEmpty() || (prefix != "suffix" && prefix != "keyword")) return {};

        const auto addressTarget = [&raw](const RouteRule& rule) -> std::optional<SimpleTarget> {
            switch (rule.type) {
                case simpleAddressProxy: return SimpleTarget{proxy};
                case simpleAddressBypass: return SimpleTarget{bypass};
                case simpleAddressBlock: return SimpleTarget{block};
                case simpleAddressWarpBypass: return SimpleTarget{warpBypass};
                default: break;
            }
            if (is_server_rule_for(rule, raw)) return SimpleTarget{proxy, rule.outboundID};
            return std::nullopt;
        };

        // Rules match in list order, and a rule the target does not have yet gets appended at the end.
        const auto own = rules_for(raw, target);
        for (const auto& rule : Rules) {
            if (!own.isEmpty() && rule == own.first()) break;
            const auto other = addressTarget(*rule);
            if (!other || *other == target || rule->disabled) continue;

            // Every host a suffix or keyword matches contains the value, so an earlier keyword inside it catches them all.
            // The same line elsewhere does not count: adding rawRule moves it out of that list.
            for (const auto& keyword : rule->domain_keyword) {
                const QString k = keyword.toLower();
                if (!k.isEmpty() && value.contains(k) && !(prefix == "keyword" && k == value)) {
                    *coveringTarget = *other;
                    return "keyword:" + keyword;
                }
            }
            // A suffix covers a longer suffix, and only whole labels count: github.com covers api.github.com, not mygithub.com.
            if (prefix != "suffix") continue;
            for (const auto& suffix : rule->domain_suffix) {
                const QString s = suffix.toLower();
                if (!s.isEmpty() && value.endsWith("." + s)) {
                    *coveringTarget = *other;
                    return "suffix:" + suffix;
                }
            }
        }
        return {};
    }

    bool RouteProfile::is_server_rule_for(const RouteRule& rule, const QString& content) {
        // A disabled rule routes nothing, so a target in it is not routed through the server.
        if (rule.type != custom || rule.disabled || rule.action != "route" || rule.outboundID < 0 || !rule.has_match_conditions()) return false;
        const auto type = get_rule_type(content, proxy);
        if (type == custom) return false;
        // Address, process name and process path conditions in one rule must all match, so each kind keeps a rule of its own.
        static const QStringList addressAttrs = {"domain", "domain_suffix", "domain_keyword", "domain_regex", "rule_set", "ip_cidr"};
        static const QStringList processNameAttrs = {"process_name"};
        static const QStringList processPathAttrs = {"process_path"};
        const auto& own = type == simpleAddressProxy ? addressAttrs : type == simpleProcessNameProxy ? processNameAttrs : processPathAttrs;
        // Checked for every rule on each dialog refresh, so built once.
        static const auto attrs = RouteRule::get_attributes();
        return std::all_of(attrs.cbegin(), attrs.cend(), [&](const QString& attr) {
            return attr == "outbound" || own.contains(attr) || RouteRule::is_attribute_at_default(rule, attr);
        });
    }

    QList<std::shared_ptr<RouteRule>> RouteProfile::rules_for(const QString& content, const SimpleTarget& target) {
        QList<std::shared_ptr<RouteRule>> rules;
        const auto type = get_rule_type(content, target.action);
        if (type == custom) return rules;
        for (const auto& rule : Rules) {
            if (target.isServer() ? rule->outboundID == target.serverID && is_server_rule_for(*rule, content) : rule->type == type)
                rules << rule;
        }
        return rules;
    }

    QList<QString>* RouteProfile::simple_rule_values(const QString& content, RouteRule& rule, QString* value)
    {
        const auto colonIdx = content.indexOf(':');
        if (colonIdx == -1) return nullptr;
        *value = content.mid(colonIdx + 1).trimmed();
        if (value->isEmpty()) return nullptr;

        const QString prefix = content.left(colonIdx).trimmed();
        // Stored lowercased by add_simple_address_rule, so looked up the same way.
        if (prefix == "domain" || prefix == "suffix" || prefix == "keyword") *value = value->toLower();
        if (prefix == "domain") return &rule.domain;
        if (prefix == "suffix") return &rule.domain_suffix;
        if (prefix == "keyword") return &rule.domain_keyword;
        if (prefix == "regex") return &rule.domain_regex;
        if (prefix == "ruleset") return &rule.rule_set;
        if (prefix == "ip") return &rule.ip_cidr;
        if (prefix == "processName") return &rule.process_name;
        if (prefix == "processPath") return &rule.process_path;
        return nullptr;
    }

    QString RouteProfile::add_simple_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type)
    {
        if (const auto issue = CheckSimpleRule(content); issue.severity == Validation::Severity::Error && !issue.message.isEmpty())
            return issue.message;
        if (type == simpleAddressProxy || type == simpleAddressBypass || type == simpleAddressBlock || type == simpleAddressWarpBypass) return add_simple_address_rule(content, rule);
        else return add_simple_process_rule(content, rule);
    }

    QString RouteProfile::add_simple_address_rule(const QString& content, const std::shared_ptr<RouteRule>& rule)
    {
        const auto notAddress = [] {
            return QCoreApplication::translate("Validation", "Start the line with domain:, suffix:, keyword:, regex:, ruleset: or ip:.");
        };
        auto colonIdx = content.indexOf(':');
        if (colonIdx == -1) return notAddress();
        const QString address = content.mid(colonIdx+1).trimmed();
        if (address.isEmpty()) return notAddress();
        const QString subType = content.left(colonIdx).trimmed();
        // sing-box lowercases the host before matching but takes these values as written, so a capital letter here never matches.
        const QString lowered = address.toLower();
        if (subType == "domain") {
            if (!rule->domain.contains(lowered)) rule->domain.append(lowered);
        } else if (subType == "suffix") {
            if (!rule->domain_suffix.contains(lowered)) rule->domain_suffix.append(lowered);
        } else if (subType == "keyword") {
            if (!rule->domain_keyword.contains(lowered)) rule->domain_keyword.append(lowered);
        } else if (subType == "regex") {
            // Left as written: it is matched against the lowercased host too, but lowercasing a pattern can change it (\D is not \d).
            if (!rule->domain_regex.contains(address)) rule->domain_regex.append(address);
        } else if (subType == "ruleset") {
            if (!rule->rule_set.contains(address)) rule->rule_set.append(address);
        } else if (subType == "ip") {
            if (!rule->ip_cidr.contains(address)) rule->ip_cidr.append(address);
        } else {
            return notAddress();
        }
        return {};
    }

    QString RouteProfile::add_simple_process_rule(const QString& content, const std::shared_ptr<RouteRule>& rule)
    {
        const auto notProcess = [] {
            return QCoreApplication::translate("Validation", "Start the line with processName: or processPath:.");
        };
        if (!content.contains(":")) return notProcess();
        const QString prefix = content.first(content.indexOf(':')).trimmed();
        const QString address = content.section(':', 1).trimmed();
        if (address.isEmpty()) return notProcess();
        if (prefix == "processPath")
        {
            if (!rule->process_path.contains(address)) rule->process_path.append(address);
        } else if (prefix == "processName")
        {
            if (!rule->process_name.contains(address)) rule->process_name.append(address);
        } else
        {
            return notProcess();
        }
        return {};
    }

    std::shared_ptr<RouteRule> RouteProfile::get_simple_rule_by_type(ruleType type) {
        for (const auto &r : Rules) {
            if (r->type == type) return r;
        }
        return nullptr;
    }

    ruleType RouteProfile::get_rule_type(const QString& content, simpleAction action) {
        if (content.startsWith("domain") ||
            content.startsWith("suffix") ||
            content.startsWith("keyword") ||
            content.startsWith("regex") ||
            content.startsWith("ruleset") ||
            content.startsWith("ip")) {
            if (action == proxy) return simpleAddressProxy;
            if (action == bypass) return simpleAddressBypass;
            if (action == warpBypass) return simpleAddressWarpBypass;
            return simpleAddressBlock;
        }
        if (content.startsWith("processName")) {
            if (action == proxy) return simpleProcessNameProxy;
            if (action == bypass) return simpleProcessNameBypass;
            if (action == warpBypass) return simpleProcessNameWarpBypass;
            return simpleProcessNameBlock;
        }
        if (content.startsWith("processPath")) {
            if (action == proxy) return simpleProcessPathProxy;
            if (action == bypass) return simpleProcessPathBypass;
            if (action == warpBypass) return simpleProcessPathWarpBypass;
            return simpleProcessPathBlock;
        }
        return custom;
    }
}
