#include "include/engine/services/RouteService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

#include "include/configs/generate.h"
#include "include/database/DatabaseManager.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/engine/EditLocks.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/ThreadCheck.h"
#include "include/global/Utils.hpp"

namespace Engine::RouteService {
    namespace {
        using Configs::RouteProfile;
        using Configs::RouteRule;

        enum class RouteServiceKind { Bool, Select, Outbound, List, Text };

        struct RouteServiceSimpleKey {
            const char *key;
            Configs::simpleAction action;
        };

        constexpr RouteServiceSimpleKey kRouteServiceSimpleKeys[] = {
            {"proxy", Configs::proxy},
            {"direct", Configs::bypass},
            {"block", Configs::block},
            {"warp_bypass", Configs::warpBypass},
        };

        ServiceError routeServiceBad(const QString &message) {
            return {400, QStringLiteral("bad_request"), message};
        }

        ServiceError routeServiceUnknown(int routeId) {
            return {404, QStringLiteral("not_found"), QStringLiteral("Unknown routing profile %1").arg(routeId)};
        }

        ServiceError routeServiceInvalid(const QStringList &problems) {
            return {422, QStringLiteral("invalid_route"),
                    QStringLiteral("The routing profile is not valid\n") + problems.join(QLatin1Char('\n'))};
        }

        bool routeServicePresent(const QJsonObject &body, const QString &key) {
            const auto value = body.value(key);
            return !value.isUndefined() && !value.isNull();
        }

        std::optional<int> routeServiceInteger(const QJsonValue &value) {
            if (!value.isDouble()) return std::nullopt;
            const double number = value.toDouble();
            if (std::trunc(number) != number || number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
                return std::nullopt;
            }
            return static_cast<int>(number);
        }

        std::optional<ServiceError> routeServiceReadString(const QJsonObject &body, const QString &key, std::optional<QString> *out) {
            if (!routeServicePresent(body, key)) return std::nullopt;
            if (!body.value(key).isString()) return routeServiceBad(QStringLiteral("\"%1\" must be a string").arg(key));
            *out = body.value(key).toString();
            return std::nullopt;
        }

        std::optional<ServiceError> routeServiceReadBool(const QJsonObject &body, const QString &key, std::optional<bool> *out) {
            if (!routeServicePresent(body, key)) return std::nullopt;
            if (!body.value(key).isBool()) return routeServiceBad(QStringLiteral("\"%1\" must be true or false").arg(key));
            *out = body.value(key).toBool();
            return std::nullopt;
        }

        std::optional<ServiceError> routeServiceReadInt(const QJsonObject &body, const QString &key, std::optional<int> *out) {
            if (!routeServicePresent(body, key)) return std::nullopt;
            const auto number = routeServiceInteger(body.value(key));
            if (!number) return routeServiceBad(QStringLiteral("\"%1\" must be an integer").arg(key));
            *out = *number;
            return std::nullopt;
        }

        std::optional<ServiceError> routeServiceReadIds(const QJsonObject &body, const QString &key, std::optional<QList<int>> *out) {
            if (!routeServicePresent(body, key)) return std::nullopt;
            const auto bad = routeServiceBad(QStringLiteral("\"%1\" must be an array of profile ids").arg(key));
            const auto value = body.value(key);
            if (!value.isArray()) return bad;
            QList<int> ids;
            for (const auto &item : value.toArray()) {
                const auto id = routeServiceInteger(item);
                if (!id || *id < 0) return bad;
                if (!ids.contains(*id)) ids << *id;
            }
            *out = ids;
            return std::nullopt;
        }

        // Only http and https: a file:// URL would make Throne read local files for a remote client.
        bool routeServiceHttpUrl(const QString &text) {
            const QUrl url(text, QUrl::StrictMode);
            if (!url.isValid() || url.host().isEmpty()) return false;
            return url.scheme().compare(QLatin1String("http"), Qt::CaseInsensitive) == 0 ||
                   url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) == 0;
        }

        // get_attributes() plus what another OS or an older build may have stored, so a round trip loses nothing.
        const QStringList &routeServiceAttributes() {
            static const QStringList attributes = [] {
                auto list = RouteRule::get_attributes();
                for (const auto &extra : {QStringLiteral("wifi_ssid"), QStringLiteral("wifi_bssid"), QStringLiteral("sniffers")}) {
                    if (!list.contains(extra)) list << extra;
                }
                return list;
            }();
            return attributes;
        }

        RouteServiceKind routeServiceKind(const QString &attr) {
            switch (RouteRule::get_input_type(attr)) {
                case Configs::trufalse: return RouteServiceKind::Bool;
                case Configs::select: return attr == QLatin1String("outbound") ? RouteServiceKind::Outbound : RouteServiceKind::Select;
                case Configs::text: break;
            }
            return RouteRule::is_list_attribute(attr) ? RouteServiceKind::List : RouteServiceKind::Text;
        }

        QString routeServiceKindName(RouteServiceKind kind) {
            switch (kind) {
                case RouteServiceKind::Bool: return QStringLiteral("bool");
                case RouteServiceKind::Select: return QStringLiteral("select");
                case RouteServiceKind::Outbound: return QStringLiteral("outbound");
                case RouteServiceKind::List: return QStringLiteral("list");
                case RouteServiceKind::Text: break;
            }
            return QStringLiteral("text");
        }

        bool routeServiceAtDefault(const RouteRule &rule, const QString &attr) {
            if (attr == QLatin1String("sniffers")) return rule.sniffers.isEmpty();
            return RouteRule::is_attribute_at_default(rule, attr);
        }

        QJsonValue routeServiceValue(const RouteRule &rule, const QString &attr) {
            switch (routeServiceKind(attr)) {
                case RouteServiceKind::Bool: return rule.get_current_value_bool(attr) == QLatin1String("true");
                case RouteServiceKind::Outbound: return rule.outboundID;
                case RouteServiceKind::List:
                    return QJsonArray::fromStringList(attr == QLatin1String("sniffers") ? rule.sniffers : rule.get_current_value_string(attr));
                case RouteServiceKind::Select:
                case RouteServiceKind::Text: break;
            }
            return rule.get_current_value_string(attr).value(0);
        }

        QJsonObject routeServiceRuleJson(const RouteRule &rule) {
            const bool outboundUsed = RouteRule::action_scope(QStringLiteral("outbound")).contains(rule.action);
            QJsonObject attributes;
            for (const auto &attr : routeServiceAttributes()) {
                const bool always = attr == QLatin1String("action") || (attr == QLatin1String("outbound") && outboundUsed);
                if (!always && routeServiceAtDefault(rule, attr)) continue;
                attributes.insert(attr, routeServiceValue(rule, attr));
            }
            return {
                {"type", Configs::ruleTypeToToken(rule.type)},
                {"name", rule.name},
                {"disabled", rule.disabled},
                {"attributes", attributes},
            };
        }

        std::optional<QString> routeServiceScalarText(const QJsonValue &value) {
            if (value.isString()) return value.toString();
            if (const auto number = routeServiceInteger(value)) return QString::number(*number);
            return std::nullopt;
        }

        // Decodes value per attr's kind into what set_field_value takes; on a mismatch, returns what the value must be.
        std::optional<QString> routeServiceDecodeValue(const QString &attr, const QJsonValue &value, QStringList *out) {
            switch (routeServiceKind(attr)) {
                case RouteServiceKind::Bool:
                    if (!value.isBool()) return QStringLiteral("true or false");
                    *out = {value.toBool() ? QStringLiteral("true") : QStringLiteral("false")};
                    return std::nullopt;
                case RouteServiceKind::Outbound: {
                    const auto id = routeServiceInteger(value);
                    if (!id) return QStringLiteral("an outbound number");
                    *out = {QString::number(*id)};
                    return std::nullopt;
                }
                case RouteServiceKind::List: {
                    if (value.isString()) {
                        *out = {value.toString()};
                        return std::nullopt;
                    }
                    if (!value.isArray()) return QStringLiteral("an array of strings");
                    QStringList items;
                    for (const auto &item : value.toArray()) {
                        const auto text = routeServiceScalarText(item);
                        if (!text) return QStringLiteral("an array of strings");
                        items << *text;
                    }
                    *out = items;
                    return std::nullopt;
                }
                case RouteServiceKind::Select:
                case RouteServiceKind::Text: break;
            }
            const auto text = routeServiceScalarText(value);
            if (!text) return QStringLiteral("a string");
            *out = {*text};
            return std::nullopt;
        }

        std::optional<ServiceError> routeServiceReadRule(int position, const QJsonValue &item, std::shared_ptr<RouteRule> *out) {
            const auto bad = [position](const QString &message) {
                return routeServiceBad(QStringLiteral("Rule %1: %2").arg(QString::number(position), message));
            };
            if (!item.isObject()) return bad(QStringLiteral("must be an object"));
            const auto object = item.toObject();
            std::optional<QString> token;
            std::optional<QString> name;
            std::optional<bool> disabled;
            if (auto error = routeServiceReadString(object, QStringLiteral("type"), &token)) return bad(error->message);
            if (auto error = routeServiceReadString(object, QStringLiteral("name"), &name)) return bad(error->message);
            if (auto error = routeServiceReadBool(object, QStringLiteral("disabled"), &disabled)) return bad(error->message);
            const int type = Configs::tokenToRuleType(token.value_or(QStringLiteral("custom")));
            if (Configs::ruleTypeToToken(type) != token.value_or(QStringLiteral("custom"))) {
                return bad(QStringLiteral("unknown type \"%1\"").arg(*token));
            }
            QJsonObject attributes;
            if (routeServicePresent(object, QStringLiteral("attributes"))) {
                if (!object.value(QStringLiteral("attributes")).isObject()) return bad(QStringLiteral("\"attributes\" must be an object"));
                attributes = object.value(QStringLiteral("attributes")).toObject();
            }
            for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it) {
                if (!routeServiceAttributes().contains(it.key())) return bad(QStringLiteral("unknown attribute \"%1\"").arg(it.key()));
            }

            std::shared_ptr<RouteRule> rule;
            if (type == Configs::endpointPreferredBy) {
                const auto id = routeServiceInteger(attributes.value(QStringLiteral("outbound")));
                if (!id || *id < 0) return bad(QStringLiteral("an endpoint rule needs its endpoint's profile id in \"outbound\""));
                rule = RouteProfile::MakeEndpointRule(*id);
            } else {
                rule = type == Configs::custom ? std::make_shared<RouteRule>() : RouteProfile::NewSimpleRule(static_cast<Configs::ruleType>(type));
                if (rule == nullptr) return bad(QStringLiteral("unknown type \"%1\"").arg(*token));
                if (name) rule->name = *name;
                for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it) {
                    if (it.value().isNull()) continue;
                    QStringList values;
                    if (const auto expected = routeServiceDecodeValue(it.key(), it.value(), &values)) {
                        return bad(QStringLiteral("\"%1\" must be %2").arg(it.key(), *expected));
                    }
                    // A simple rule keeps its fixed action and outbound.
                    if (rule->canEditAttr(it.key())) rule->set_field_value(it.key(), values);
                }
            }
            if (disabled && type != Configs::endpointPreferredBy) rule->disabled = *disabled;
            *out = rule;
            return std::nullopt;
        }

        std::shared_ptr<RouteRule> routeServiceDnsHijackRule() {
            auto rule = std::make_shared<RouteRule>();
            rule->name = QStringLiteral("dns-hijack");
            rule->protocol = QStringLiteral("dns");
            rule->action = QStringLiteral("hijack-dns");
            return rule;
        }

        // The example the window's raw editor starts a new profile with.
        QString routeServiceRawTemplate() {
            const QJsonObject route{
                {"rules", QJsonArray{QJsonObject{{"domain_suffix", QJsonArray{QStringLiteral(".example.com")}}, {"outbound", -2}}}},
                {"final", -1},
            };
            return QJsonObject2QString(route, false);
        }

        // FilterEmptyRules, as the window's editor applies it on OK, naming the custom rules it drops.
        void routeServiceDropEmptyRules(RouteProfile &route, QStringList *warnings) {
            QList<std::shared_ptr<RouteRule>> kept;
            for (const auto &rule : std::as_const(route.Rules)) {
                if (!rule->isEmpty()) {
                    kept << rule;
                } else if (rule->type == Configs::custom) {
                    warnings->append(QStringLiteral("rules: The rule \"%1\" was left out: it is empty").arg(rule->name));
                }
            }
            route.Rules = kept;
        }

        // The window's editor gives a rule without a name, or with another rule's name, a name of its own.
        void routeServiceNameRules(RouteProfile &route, QStringList *warnings) {
            QSet<QString> taken;
            for (const auto &rule : std::as_const(route.Rules)) {
                if (rule->type == Configs::endpointPreferredBy) continue;
                QString name = rule->name;
                if (name.trimmed().isEmpty()) {
                    int n = 1;
                    while (taken.contains(QStringLiteral("rule_%1").arg(n))) ++n;
                    name = QStringLiteral("rule_%1").arg(n);
                } else {
                    for (int n = 2; taken.contains(name); ++n) name = QStringLiteral("%1 (%2)").arg(rule->name, QString::number(n));
                }
                if (name != rule->name) {
                    warnings->append(QStringLiteral("rules: The rule \"%1\" was renamed to \"%2\": rule names must be unique and not empty")
                                         .arg(rule->name, name));
                    rule->name = name;
                }
                taken.insert(name);
            }
        }

        std::optional<ServiceError> routeServiceReadSimple(RouteProfile &route, const QJsonValue &value, QStringList *problems) {
            if (!value.isObject()) return routeServiceBad(QStringLiteral("\"simple\" must be an object of rule texts"));
            const auto simple = value.toObject();
            for (auto it = simple.constBegin(); it != simple.constEnd(); ++it) {
                const bool known = std::any_of(std::begin(kRouteServiceSimpleKeys), std::end(kRouteServiceSimpleKeys),
                                               [&it](const RouteServiceSimpleKey &entry) { return it.key() == QLatin1String(entry.key); });
                if (!known) {
                    return routeServiceBad(QStringLiteral("Unknown \"simple\" list \"%1\"; use proxy, direct, block or warp_bypass").arg(it.key()));
                }
                if (!it.value().isNull() && !it.value().isString()) {
                    return routeServiceBad(QStringLiteral("\"simple.%1\" must be a string").arg(it.key()));
                }
                if (route.isRaw && !it.value().toString().trimmed().isEmpty()) {
                    return routeServiceBad(QStringLiteral("\"simple\" applies only to structured routing profiles"));
                }
            }
            if (route.isRaw) return std::nullopt;
            for (const auto &[key, action] : kRouteServiceSimpleKeys) {
                const QString label = QLatin1String(key);
                if (!routeServicePresent(simple, label)) continue;
                const auto rejected = route.UpdateSimpleRules(simple.value(label).toString(), action, label);
                for (const auto &line : rejected.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                    problems->append(QStringLiteral("simple: %1").arg(line.trimmed()));
                }
            }
            return std::nullopt;
        }

        // Laid over the profile's stored DNS (else the global settings), and typed by the keys and values RouteDns::ToJson writes.
        std::optional<ServiceError> routeServiceReadDns(const QJsonValue &value, const RouteProfile &route, Configs::RouteDns *out) {
            if (!value.isObject()) return routeServiceBad(QStringLiteral("\"dns\" must be an object of DNS settings"));
            auto merged = route.dns.value_or(Configs::RouteDns::Global()).ToJson();
            const auto patch = value.toObject();
            for (auto it = patch.constBegin(); it != patch.constEnd(); ++it) {
                const auto current = merged.value(it.key());
                if (current.isUndefined()) return routeServiceBad(QStringLiteral("Unknown DNS setting \"%1\"").arg(it.key()));
                if (it.value().isNull()) continue;
                if (current.isBool() && !it.value().isBool()) {
                    return routeServiceBad(QStringLiteral("\"dns.%1\" must be true or false").arg(it.key()));
                }
                if (current.isString() && !it.value().isString()) {
                    return routeServiceBad(QStringLiteral("\"dns.%1\" must be a string").arg(it.key()));
                }
                merged.insert(it.key(), it.value());
            }
            *out = Configs::RouteDns::FromJson(merged);
            return std::nullopt;
        }

        // What the window's Endpoints tab allows; an endpoint whose profile is gone drops out, as it does on the window's OK.
        void routeServiceApplyEndpoints(RouteProfile &route, const std::optional<QList<int>> &endpoints, const std::optional<QList<int>> &innerHops,
                                        QStringList *problems, QStringList *warnings) {
            QList<int> kept;
            for (const int id : endpoints.value_or(route.endpointProfileIDs)) {
                if (kept.contains(id)) continue;
                const auto ent = Configs::dataManager->profilesRepo->GetProfile(id);
                if (ent == nullptr || ent->outbound == nullptr) {
                    warnings->append(QStringLiteral("endpoints: Profile %1 no longer exists, so it was removed").arg(id));
                    continue;
                }
                if (endpoints && !Configs::CanBeAuxEndpoint(ent)) {
                    problems->append(QStringLiteral("endpoints: Profile %1 cannot be an endpoint; only OpenVPN and OpenConnect profiles, "
                                                    "and chains that exit through one, can").arg(id));
                    continue;
                }
                kept << id;
            }
            route.endpointProfileIDs = kept;

            QList<int> hops;
            for (const int id : innerHops.value_or(route.innerHopEndpointIDs)) {
                if (hops.contains(id)) continue;
                if (!kept.contains(id)) {
                    if (innerHops) problems->append(QStringLiteral("inner_hop_endpoints: Profile %1 is not one of the endpoints").arg(id));
                    continue;
                }
                // Only a chain with OpenVPN or OpenConnect hops behind its exit has inner hops to open up.
                if (Configs::AuxEndpointInnerHops(id).isEmpty()) continue;
                hops << id;
            }
            route.innerHopEndpointIDs = hops;
            route.SyncEndpointRules();
        }

        // Reads every field PUT accepts into the draft; a 400 for a malformed request, otherwise problems and warnings.
        std::optional<ServiceError> routeServiceApply(RouteProfile &route, const QJsonObject &body, QStringList *problems, QStringList *warnings) {
            std::optional<QString> name;
            std::optional<QString> rawRoute;
            std::optional<QString> remoteUrl;
            std::optional<bool> preventModifications;
            std::optional<bool> autoUpdate;
            std::optional<bool> customDns;
            std::optional<int> defaultOutbound;
            std::optional<QList<int>> endpoints;
            std::optional<QList<int>> innerHops;
            if (auto error = routeServiceReadString(body, QStringLiteral("name"), &name)) return error;
            if (auto error = routeServiceReadInt(body, QStringLiteral("default_outbound"), &defaultOutbound)) return error;
            if (auto error = routeServiceReadString(body, QStringLiteral("raw_route"), &rawRoute)) return error;
            if (auto error = routeServiceReadBool(body, QStringLiteral("prevent_modifications"), &preventModifications)) return error;
            if (auto error = routeServiceReadString(body, QStringLiteral("remote_url"), &remoteUrl)) return error;
            if (auto error = routeServiceReadBool(body, QStringLiteral("auto_update"), &autoUpdate)) return error;
            if (auto error = routeServiceReadBool(body, QStringLiteral("custom_dns"), &customDns)) return error;
            if (auto error = routeServiceReadIds(body, QStringLiteral("endpoints"), &endpoints)) return error;
            if (auto error = routeServiceReadIds(body, QStringLiteral("inner_hop_endpoints"), &innerHops)) return error;

            // A field of another kind may only carry its empty value, so a detail sent back unchanged passes.
            const bool raw = route.isRaw;
            const bool remote = route.isRemote;
            if (rawRoute && !raw && !rawRoute->isEmpty()) return routeServiceBad(QStringLiteral("\"raw_route\" applies only to raw routing profiles"));
            if (preventModifications.value_or(false) && !raw) {
                return routeServiceBad(QStringLiteral("\"prevent_modifications\" applies only to raw routing profiles"));
            }
            if (remoteUrl && !remote && !remoteUrl->isEmpty()) {
                return routeServiceBad(QStringLiteral("\"remote_url\" applies only to remote routing profiles"));
            }
            if (autoUpdate.value_or(false) && !remote) return routeServiceBad(QStringLiteral("\"auto_update\" applies only to remote routing profiles"));

            const bool rulesGiven = routeServicePresent(body, QStringLiteral("rules"));
            const bool simpleGiven = routeServicePresent(body, QStringLiteral("simple"));
            if (rulesGiven) {
                const auto value = body.value(QStringLiteral("rules"));
                if (!value.isArray()) return routeServiceBad(QStringLiteral("\"rules\" must be an array of rules"));
                const auto items = value.toArray();
                if (raw && !items.isEmpty()) return routeServiceBad(QStringLiteral("\"rules\" applies only to structured routing profiles"));
                if (!raw) {
                    QList<std::shared_ptr<RouteRule>> rules;
                    for (qsizetype i = 0; i < items.size(); ++i) {
                        std::shared_ptr<RouteRule> rule;
                        if (auto error = routeServiceReadRule(static_cast<int>(i + 1), items[i], &rule)) return error;
                        rules << rule;
                    }
                    route.Rules = rules;
                    routeServiceDropEmptyRules(route, warnings);
                }
            }
            if (simpleGiven) {
                if (auto error = routeServiceReadSimple(route, body.value(QStringLiteral("simple")), problems)) return error;
            }
            const bool dnsGiven = routeServicePresent(body, QStringLiteral("dns"));
            if (dnsGiven) {
                Configs::RouteDns dns;
                if (auto error = routeServiceReadDns(body.value(QStringLiteral("dns")), route, &dns)) return error;
                route.dns = dns;
            }

            routeServiceApplyEndpoints(route, endpoints, innerHops, problems, warnings);
            if (!raw && (rulesGiven || simpleGiven)) routeServiceNameRules(route, warnings);

            if (name) route.name = name->trimmed();
            if (defaultOutbound) route.defaultOutboundID = *defaultOutbound;
            if (raw && rawRoute) {
                // Stored the way the window's raw editor stores it; validation reports a text that is not an object.
                const auto document = QJsonDocument::fromJson(rawRoute->toUtf8());
                route.rawRoute = document.isObject() ? QJsonObject2QString(document.object(), false) : *rawRoute;
            }
            if (raw && preventModifications) route.preventModifications = *preventModifications;
            if (remote && remoteUrl) {
                route.remoteURL = remoteUrl->trimmed();
                if (!routeServiceHttpUrl(route.remoteURL)) problems->append(QStringLiteral("remote_url: The remote URL must be an http or https URL"));
            }
            if (remote && autoUpdate) route.autoUpdate = *autoUpdate;
            if (customDns) route.customDns = *customDns;
            // Turning its own DNS on starts from the global settings, as the window's DNS tab does.
            if (route.customDns && !route.dns) route.dns = Configs::RouteDns::Global();
            if ((dnsGiven || customDns.value_or(false)) && route.dns) {
                if (const auto problem = route.dns->Problem(); !problem.isEmpty()) problems->append(QStringLiteral("dns: %1").arg(problem));
            }
            return std::nullopt;
        }

        QString routeServiceIssueLine(qsizetype position, const RouteRule &rule, const Configs::RuleIssue &issue) {
            QString what = issue.attr;
            if (!issue.value.isEmpty()) {
                const QString value = issue.value.size() > 60 ? issue.value.left(59) + QChar(0x2026) : issue.value;
                what = QStringLiteral("%1 (\"%2\")").arg(issue.attr, value);
            }
            return QStringLiteral("Rule %1 (%2): %3: %4").arg(QString::number(position), rule.name, what, issue.message);
        }

        // The checks RouteItem and RawRouteItem make on OK.
        void routeServiceValidate(RouteProfile &route, QStringList *problems, QStringList *warnings) {
            if (route.name.trimmed().isEmpty()) problems->append(QStringLiteral("name: The routing profile needs a name"));
            // The choices of the window's default outbound box; the build has no route final for a profile id.
            static const QList<int> defaults = {Configs::proxyID, Configs::directID, Configs::blockID, Configs::warpBypassID};
            if (!defaults.contains(route.defaultOutboundID)) {
                problems->append(QStringLiteral("default_outbound: Use -1 (proxy), -2 (direct), -3 (block) or -5 (warp-bypass)"));
            }
            if (route.isRaw) {
                if (!QJsonDocument::fromJson(route.rawRoute.toUtf8()).isObject()) {
                    problems->append(QStringLiteral("raw_route: The route must be a valid JSON object"));
                }
                return;
            }
            // A remote profile may be saved before its first fetch.
            if (!route.isRemote && route.IsEmpty()) problems->append(QStringLiteral("rules: No valid rules are in the profile"));
            for (qsizetype i = 0; i < route.Rules.size(); ++i) {
                const auto &rule = *route.Rules[i];
                for (const auto &issue : rule.validate()) {
                    (issue.severity == Validation::Severity::Error ? *problems : *warnings) << routeServiceIssueLine(i + 1, rule, issue);
                }
            }
        }
    } // namespace

    QJsonObject SchemaJson() {
        QJsonArray attributes;
        for (const auto &attr : RouteRule::get_attributes()) {
            attributes.append(QJsonObject{
                {"name", attr},
                {"kind", routeServiceKindName(routeServiceKind(attr))},
                {"match", RouteRule::is_match_attribute(attr)},
                {"values", QJsonArray::fromStringList(RouteRule::get_values_for_field(attr))},
                {"actions", QJsonArray::fromStringList(RouteRule::action_scope(attr))},
            });
        }
        QJsonArray ruleTypes;
        for (int type = Configs::custom; type <= Configs::endpointPreferredBy; ++type) ruleTypes.append(Configs::ruleTypeToToken(type));
        QJsonArray outbounds;
        for (const int id : {Configs::proxyID, Configs::directID, Configs::blockID, Configs::warpBypassID}) {
            outbounds.append(QJsonObject{{"id", id}, {"name", Configs::outboundIDToString(id)}});
        }
        return {
            {"attributes", attributes},
            {"rule_types", ruleTypes},
            {"outbounds", outbounds},
        };
    }

    QJsonObject DetailJson(Configs::RouteProfile &route) {
        QJsonArray rules;
        for (const auto &rule : std::as_const(route.Rules)) rules.append(routeServiceRuleJson(*rule));
        QJsonArray endpoints;
        for (const int id : std::as_const(route.endpointProfileIDs)) endpoints.append(id);
        QJsonArray innerHops;
        for (const int id : std::as_const(route.innerHopEndpointIDs)) innerHops.append(id);
        QString kind = QStringLiteral("rules");
        if (route.isRaw) kind = QStringLiteral("raw");
        else if (route.isRemote) kind = QStringLiteral("remote");
        return {
            {"kind", kind},
            {"default_outbound", route.defaultOutboundID},
            {"rules", rules},
            {"simple", QJsonObject{
                {"proxy", route.GetSimpleRules(Configs::proxy)},
                {"direct", route.GetSimpleRules(Configs::bypass)},
                {"block", route.GetSimpleRules(Configs::block)},
                {"warp_bypass", route.GetSimpleRules(Configs::warpBypass)},
            }},
            {"raw_route", route.rawRoute},
            {"prevent_modifications", route.preventModifications},
            {"remote_url", route.remoteURL},
            {"auto_update", route.autoUpdate},
            {"remote_updated_at", route.remoteLastUpdate},
            {"endpoints", endpoints},
            {"inner_hop_endpoints", innerHops},
            {"custom_dns", route.customDns},
            // Without its own, the global settings a custom_dns switch starts from.
            {"dns", route.dns.value_or(Configs::RouteDns::Global()).ToJson()},
        };
    }

    std::optional<ServiceError> CheckEdit(int routeId) {
        ENGINE_CHECK_MAIN_THREAD();
        if (!Configs::dataManager->routesRepo->HasRouteProfile(routeId)) return routeServiceUnknown(routeId);
        // The routing dialog writes its whole list back on OK; a single profile's editor locks just its id.
        if (IsBeingEdited(EditTarget::Routes, routeId)) {
            return ServiceError{409, QStringLiteral("editing"),
                                QStringLiteral("The routing profile is open in an editor in Throne's window; close it and try again")};
        }
        return std::nullopt;
    }

    std::optional<ServiceError> Create(const QJsonObject &body, int *routeId, QStringList *warnings) {
        ENGINE_CHECK_MAIN_THREAD();
        std::optional<QString> kind;
        std::optional<QString> name;
        if (auto error = routeServiceReadString(body, QStringLiteral("kind"), &kind)) return error;
        if (auto error = routeServiceReadString(body, QStringLiteral("name"), &name)) return error;
        const QString kindName = kind.value_or(QStringLiteral("rules"));
        if (kindName == QLatin1String("remote")) {
            return routeServiceBad(QStringLiteral("A remote routing profile cannot be created through the API; add it in Throne's window"));
        }
        if (kindName != QLatin1String("rules") && kindName != QLatin1String("raw")) {
            return routeServiceBad(QStringLiteral("\"kind\" must be rules or raw"));
        }
        if (name.value_or(QString()).trimmed().isEmpty()) return routeServiceBad(QStringLiteral("\"name\" is required and must not be empty"));

        auto route = Configs::RoutesRepo::NewRouteProfile();
        route->isRaw = kindName == QLatin1String("raw");
        if (route->isRaw) {
            route->rawRoute = routeServiceRawTemplate();
        } else if (!routeServicePresent(body, QStringLiteral("rules"))) {
            // What the window's editor starts a new structured profile with.
            route->Rules << routeServiceDnsHijackRule();
        }
        QStringList problems;
        QStringList notes;
        if (auto error = routeServiceApply(*route, body, &problems, &notes)) return error;
        routeServiceValidate(*route, &problems, &notes);
        if (!problems.isEmpty()) return routeServiceInvalid(problems);
        if (!Configs::dataManager->routesRepo->AddRouteProfile(route)) {
            return ServiceError{500, QStringLiteral("internal"), QStringLiteral("Could not save the routing profile")};
        }
        if (routeId != nullptr) *routeId = route->id;
        if (warnings != nullptr) *warnings = notes;
        EngineEvents::RoutesChanged();
        return std::nullopt;
    }

    std::optional<ServiceError> Update(int routeId, const QJsonObject &body, QStringList *warnings) {
        ENGINE_CHECK_MAIN_THREAD();
        if (auto refusal = CheckEdit(routeId)) return refusal;
        const auto stored = Configs::dataManager->routesRepo->GetRouteProfile(routeId);
        if (stored == nullptr) return routeServiceUnknown(routeId);
        // The repo hands out the instance the window and the builder share, so the edit stays on a copy until it validates.
        auto route = std::make_shared<RouteProfile>(*stored);
        QStringList problems;
        QStringList notes;
        if (auto error = routeServiceApply(*route, body, &problems, &notes)) return error;
        routeServiceValidate(*route, &problems, &notes);
        if (!problems.isEmpty()) return routeServiceInvalid(problems);
        if (!Configs::dataManager->routesRepo->Save(route)) {
            return ServiceError{500, QStringLiteral("internal"), QStringLiteral("Could not save the routing profile")};
        }
        if (warnings != nullptr) *warnings = notes;
        EngineEvents::RoutesChanged();
        return std::nullopt;
    }

    std::optional<ServiceError> Delete(int routeId) {
        ENGINE_CHECK_MAIN_THREAD();
        if (auto refusal = CheckEdit(routeId)) return refusal;
        if (Configs::dataManager->routesRepo->GetAllRouteProfileIds().size() <= 1) {
            return ServiceError{409, QStringLiteral("last_route"),
                                QStringLiteral("Throne needs at least one routing profile; add another one before deleting this one")};
        }
        Configs::dataManager->routesRepo->DeleteRouteProfile(routeId);
        EngineEvents::RoutesChanged();
        return std::nullopt;
    }
} // namespace Engine::RouteService
