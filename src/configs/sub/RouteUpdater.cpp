#include "include/configs/sub/RouteUpdater.hpp"

#include <algorithm>
#include <iterator>

#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUrl>

#include "include/global/Configs.hpp"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/global/RunningProfiles.hpp"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/configs/generate.h"
#include "include/engine/EditLocks.h"
#include "include/engine/EngineHost.h"

namespace RouteUpdate {
    namespace {
        // Endpoint rules belong to the local profile, so they survive the swap by index.
        void replaceRules(Configs::RouteProfile& profile, const Configs::RouteProfile& fetched) {
            QList<QPair<int, std::shared_ptr<Configs::RouteRule>>> endpointRules;
            QSet<QString> disabledNames;
            for (int i = 0; i < profile.Rules.size(); i++) {
                if (profile.Rules[i]->type == Configs::endpointPreferredBy) endpointRules << qMakePair(i, profile.Rules[i]);
                else if (profile.Rules[i]->disabled) disabledNames << profile.Rules[i]->name;
            }

            profile.isRaw = false;
            profile.rawRoute.clear();
            profile.Rules.clear();
            for (const auto& rule : fetched.Rules) {
                if (rule->type == Configs::endpointPreferredBy) continue;
                // A rule switched off locally stays off across refreshes, matched by name.
                if (disabledNames.contains(rule->name)) rule->disabled = true;
                profile.Rules << rule;
            }
            for (const auto& [index, rule] : endpointRules) {
                profile.Rules.insert(std::min<qsizetype>(index, profile.Rules.size()), rule);
            }
            profile.SyncEndpointRules();
            profile.defaultOutboundID = fetched.defaultOutboundID;
        }

        QByteArray routeFingerprint(const Configs::RouteProfile& profile) {
            QJsonArray rules;
            for (const auto& rule : profile.Rules) rules << rule->to_share_json();
            const QJsonObject object{{"default", profile.defaultOutboundID}, {"raw", profile.isRaw}, {"rawRoute", profile.rawRoute}, {"rules", rules}};
            return QJsonDocument(object).toJson(QJsonDocument::Compact);
        }

        void armRoutingRestart() {
            if (auto* host = Engine::host()) host->noteRestartNeeded(Engine::RestartReason::Routing);
        }

        bool builtinOutbound(int id) {
            return id == Configs::proxyID || id == Configs::directID || id == Configs::blockID;
        }

        // Nothing that names a local server, rewrites the destination or needs a local WARP or endpoint setup.
        bool subscriptionSafeRule(const Configs::RouteRule& rule) {
            if (rule.type == Configs::endpointPreferredBy) return false;
            if (!rule.override_address.isEmpty() || !rule.override_port.isEmpty()) return false;
            if (rule.action == QLatin1String("route") || rule.action == QLatin1String("bypass")) return builtinOutbound(rule.outboundID) || rule.outboundID == -4;
            static const QStringList safe = {"reject", "hijack-dns", "sniff", "resolve", "route-options"};
            return safe.contains(rule.action);
        }

        // Standard or URL-safe, padded or not, possibly wrapped.
        QByteArray decodeLenientBase64(const QString& text) {
            QByteArray bytes;
            bytes.reserve(text.size() + 3);
            for (const QChar c : text) {
                if (c.isSpace()) continue;
                if (c == u'-') bytes += '+';
                else if (c == u'_') bytes += '/';
                else bytes += c.toLatin1();
            }
            while (bytes.endsWith('=')) bytes.chop(1);
            while (bytes.size() % 4 != 0) bytes += '=';
            const auto result = QByteArray::fromBase64Encoding(bytes, QByteArray::AbortOnBase64DecodingErrors);
            return result ? result.decoded : QByteArray();
        }

        QJsonValue happValue(const QJsonObject& object, QLatin1String key) {
            for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
                if (it.key().compare(key, Qt::CaseInsensitive) == 0) return it.value();
            }
            return QJsonValue(QJsonValue::Undefined);
        }

        QStringList happList(const QJsonValue& value) {
            QStringList items;
            if (value.isArray()) {
                for (const auto& item : value.toArray()) {
                    if (item.isString()) items << item.toString();
                }
            } else if (value.isString()) {
                items = value.toString().split(u',', Qt::SkipEmptyParts);
            }
            return items;
        }

        enum class HappEntry { Skip, Line, PrivateIp, Dropped };

        // An Xray-style entry as a simple-rule line ("suffix:example.com", "ruleset:geosite-google", "ip:10.0.0.0/8").
        HappEntry happEntry(const QString& raw, bool ipList, QString* line) {
            const QString entry = raw.trimmed();
            if (entry.isEmpty()) return HappEntry::Skip;
            if (entry.startsWith(u'!') || entry.contains(u'@')) return HappEntry::Dropped;
            if ((entry.contains(u'.') || entry.contains(u':')) &&
                (!QHostAddress(entry).isNull() || (entry.contains(u'/') && !QHostAddress::parseSubnet(entry).first.isNull()))) {
                *line = QStringLiteral("ip:") + entry;
                return HappEntry::Line;
            }
            const auto colon = entry.indexOf(u':');
            if (colon < 0) {
                if (ipList) return HappEntry::Dropped;
                *line = QStringLiteral("keyword:") + entry.toLower();
                return HappEntry::Line;
            }
            const QString prefix = entry.left(colon).trimmed().toLower();
            const QString value = entry.mid(colon + 1).trimmed();
            if (value.isEmpty()) return HappEntry::Dropped;
            if (prefix == QLatin1String("geoip") && value.compare(QLatin1String("private"), Qt::CaseInsensitive) == 0) return HappEntry::PrivateIp;
            if (prefix == QLatin1String("geosite") || prefix == QLatin1String("geoip")) *line = QStringLiteral("ruleset:%1-%2").arg(prefix, value.toLower());
            else if (prefix == QLatin1String("domain")) *line = QStringLiteral("suffix:") + value.toLower();
            else if (prefix == QLatin1String("full")) *line = QStringLiteral("domain:") + value.toLower();
            else if (prefix == QLatin1String("regexp")) *line = QStringLiteral("regex:") + value;
            else if (prefix == QLatin1String("keyword")) *line = QStringLiteral("keyword:") + value.toLower();
            else return HappEntry::Dropped;
            return HappEntry::Line;
        }

        std::shared_ptr<Configs::RouteProfile> fromHapp(const QJsonObject& object, int* imported, int* dropped, QString* error) {
            static const char* const kKeys[] = {"Name", "GlobalProxy", "DirectSites", "DirectIp", "ProxySites", "ProxyIp", "BlockSites", "BlockIp"};
            if (std::none_of(std::begin(kKeys), std::end(kKeys), [&object](const char* key) { return !happValue(object, QLatin1String(key)).isUndefined(); })) {
                *error = QObject::tr("the routing profile is not in the Happ format");
                return nullptr;
            }

            auto profile = std::make_shared<Configs::RouteProfile>();
            profile->name = happValue(object, QLatin1String("Name")).toString().trimmed();
            const auto global = happValue(object, QLatin1String("GlobalProxy"));
            const bool globalProxy = global.isBool() ? global.toBool() : global.toString().trimmed().compare(QLatin1String("false"), Qt::CaseInsensitive) != 0;
            profile->defaultOutboundID = globalProxy ? Configs::proxyID : Configs::directID;

            QList<Configs::simpleAction> order;
            const auto append = [&order](Configs::simpleAction action) {
                if (!order.contains(action)) order << action;
            };
            const auto orderValue = happValue(object, QLatin1String("RouteOrder"));
            const QStringList words = orderValue.isArray()
                ? happList(orderValue)
                : orderValue.toString().split(QRegularExpression(QStringLiteral("[^A-Za-z]+")), Qt::SkipEmptyParts);
            for (const auto& word : words) {
                const QString lower = word.trimmed().toLower();
                if (lower == QLatin1String("block")) append(Configs::block);
                else if (lower == QLatin1String("proxy")) append(Configs::proxy);
                else if (lower == QLatin1String("direct")) append(Configs::bypass);
            }
            for (const auto action : {Configs::block, Configs::proxy, Configs::bypass}) append(action);

            for (const auto action : order) {
                const char* sitesKey = action == Configs::block ? "BlockSites" : action == Configs::proxy ? "ProxySites" : "DirectSites";
                const char* ipKey = action == Configs::block ? "BlockIp" : action == Configs::proxy ? "ProxyIp" : "DirectIp";
                bool privateIp = false;
                QStringList lines;
                for (const bool ipList : {false, true}) {
                    for (const auto& item : happList(happValue(object, QLatin1String(ipList ? ipKey : sitesKey)))) {
                        QString line;
                        switch (happEntry(item, ipList, &line)) {
                            case HappEntry::Skip: break;
                            case HappEntry::Dropped: ++*dropped; break;
                            case HappEntry::PrivateIp: privateIp = true; break;
                            case HappEntry::Line: lines << line; break;
                        }
                    }
                }
                // Simple rules ignore ip_is_private, so private ranges get a rule of their own.
                if (privateIp) {
                    auto rule = std::make_shared<Configs::RouteRule>();
                    rule->name = action == Configs::block ? QStringLiteral("Private IP Block") : action == Configs::proxy ? QStringLiteral("Private IP Proxy") : QStringLiteral("Private IP Bypass");
                    rule->ip_is_private = true;
                    if (action == Configs::block) rule->action = QStringLiteral("reject");
                    else rule->outboundID = action == Configs::proxy ? Configs::proxyID : Configs::directID;
                    profile->Rules << rule;
                    ++*imported;
                }
                for (const auto& line : lines) {
                    if (profile->AppendSimpleRule(line, Configs::SimpleTarget{action})) ++*imported;
                    else ++*dropped;
                }
            }
            return profile;
        }

        std::shared_ptr<Configs::RouteProfile> fromThroneShare(const QString& text, int* imported, int* dropped, QString* error) {
            QString fatal, warn;
            bool wasOldArray = false;
            auto profile = Configs::RouteProfile::FromShareInput(text, &fatal, &warn, &wasOldArray, false);
            if (!profile) {
                *error = fatal.isEmpty() ? QObject::tr("could not parse a routing profile from the response") : fatal;
                return nullptr;
            }
            if (profile->isRaw) {
                *error = QObject::tr("a raw routing profile is not accepted from a subscription");
                return nullptr;
            }
            profile->customDns = false;
            profile->dns.reset();
            profile->endpointProfileIDs.clear();
            profile->innerHopEndpointIDs.clear();
            QList<std::shared_ptr<Configs::RouteRule>> kept;
            for (const auto& rule : profile->Rules) {
                if (subscriptionSafeRule(*rule)) {
                    kept << rule;
                    ++*imported;
                } else {
                    ++*dropped;
                }
            }
            profile->Rules = kept;
            if (!builtinOutbound(profile->defaultOutboundID)) profile->defaultOutboundID = Configs::proxyID;
            return profile;
        }

        std::shared_ptr<Configs::RouteProfile> parseSubscriptionRoute(QString text, int* imported, int* dropped, QString* error) {
            if (text.isEmpty() || text.compare(QLatin1String("off"), Qt::CaseInsensitive) == 0) return nullptr;
            if (text.startsWith(QLatin1String("throne://route/"), Qt::CaseInsensitive)) return fromThroneShare(text, imported, dropped, error);

            static const QLatin1String kRouting("://routing/");
            if (const auto marker = text.indexOf(kRouting, 0, Qt::CaseInsensitive); marker >= 0) {
                const QString rest = text.mid(marker + kRouting.size());
                const auto slash = rest.indexOf(u'/');
                const QString verb = (slash < 0 ? rest : rest.left(slash)).trimmed().toLower();
                if (verb == QLatin1String("off")) return nullptr;
                if (verb != QLatin1String("add") && verb != QLatin1String("onadd")) {
                    *error = QObject::tr("unknown routing link action \"%1\"").arg(verb);
                    return nullptr;
                }
                text = slash < 0 ? QString() : QUrl::fromPercentEncoding(rest.mid(slash + 1).toUtf8()).trimmed();
                if (text.isEmpty()) return nullptr;
            }

            const QByteArray json = text.startsWith(u'{') ? text.toUtf8() : decodeLenientBase64(text);
            if (json.isEmpty()) {
                *error = QObject::tr("the routing profile is not valid base64");
                return nullptr;
            }
            const auto document = QJsonDocument::fromJson(json);
            if (!document.isObject()) {
                *error = QObject::tr("the routing profile is not a JSON object");
                return nullptr;
            }
            return fromHapp(document.object(), imported, dropped, error);
        }

        bool startedInGroup(int gid) {
            const int started = Configs::dataManager->settingsRepo->started_id;
            if (started < 0) return false;
            const auto profile = Configs::dataManager->profilesRepo->GetProfile(started);
            return profile != nullptr && profile->gid == gid;
        }
    } // namespace

    std::shared_ptr<Configs::RouteProfile> ParseSubscriptionRoute(const QString& value, int* imported, int* dropped, QString* error) {
        int importedCount = 0;
        int droppedCount = 0;
        QString why;
        auto profile = parseSubscriptionRoute(value.trimmed(), &importedCount, &droppedCount, &why);
        if (imported) *imported = importedCount;
        if (dropped) *dropped = droppedCount;
        if (error) *error = why;
        return profile;
    }

    void ImportSubscriptionRoute(int gid, const QString& value) {
        int imported = 0;
        int dropped = 0;
        QString error;
        const auto fetched = ParseSubscriptionRoute(value, &imported, &dropped, &error);
        if (!fetched && error.isEmpty()) return;

        // The repos' shared objects and the editor locks belong to the UI thread.
        runOnUiThread([gid, fetched, imported, dropped, error] {
            auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
            if (group == nullptr || !group->import_sub_route) return;
            if (!fetched) {
                MW_show_log(QObject::tr("[Routing] %1: the routing profile the subscription provides was ignored: %2").arg(group->name, error));
                return;
            }
            if (Engine::IsBeingEdited(Engine::EditTarget::Routes)) {
                MW_show_log(QObject::tr("[Routing] %1: the routing profile the subscription provides was not applied, the routing profiles are open for editing.")
                                .arg(group->name));
                return;
            }

            auto* routes = Configs::dataManager->routesRepo.get();
            const QString counts = dropped > 0 ? QObject::tr("%1 entries, %2 skipped").arg(imported).arg(dropped) : QObject::tr("%1 entries").arg(imported);
            const auto owned = routes->OwnedBy(gid);
            const auto current = owned.isEmpty() ? nullptr : routes->GetRouteProfile(owned.first());
            if (current == nullptr) {
                auto created = fetched;
                created->id = -1;
                created->source_gid = gid;
                if (created->name.isEmpty()) created->name = QObject::tr("%1 routing").arg(group->name);
                if (!routes->AddRouteProfile(created)) return;
                if (group->route_id < 0 || !routes->HasRouteProfile(group->route_id)) {
                    group->route_id = created->id;
                    Configs::dataManager->groupsRepo->Save(group);
                }
                MW_show_log(QObject::tr("[Routing] %1: created the routing profile \"%2\" from the subscription (%3)").arg(group->name, created->name, counts));
                if (Configs::RunningRouteId() >= 0 && startedInGroup(gid) && Configs::RouteIdFor(gid) != Configs::RunningRouteId()) armRoutingRestart();
                return;
            }

            auto updated = std::make_shared<Configs::RouteProfile>(*current);
            replaceRules(*updated, *fetched);
            if (routeFingerprint(*updated) == routeFingerprint(*current)) return;
            routes->Save(updated);
            MW_show_log(QObject::tr("[Routing] %1: updated the routing profile \"%2\" from the subscription (%3)").arg(group->name, updated->name, counts));
            if (updated->id == Configs::RunningRouteId()) armRoutingRestart();
        });
    }

    QString UpdateProfile(const std::shared_ptr<Configs::RouteProfile>& profile, QString* warnings) {
        if (!profile) return QObject::tr("internal error: null profile");
        if (!profile->isRemote) return QObject::tr("not a remote routing profile");
        const QString url = profile->remoteURL.trimmed();
        if (url.isEmpty()) return QObject::tr("remote URL is empty");

        auto resp = NetworkRequestHelper::HttpGet(Configs::get_jsdelivr_link(url));
        if (!resp.error.isEmpty()) return resp.error;

        QString fatal, warn;
        bool wasOldArray = false;
        // unmaterialized: a remote URL must never create server profiles here
        auto fetched = Configs::RouteProfile::FromShareInput(QString::fromUtf8(resp.data), &fatal, &warn, &wasOldArray, false);
        if (!fetched) return fatal.isEmpty() ? QObject::tr("could not parse a routing profile from the response") : fatal;
        if (fetched->isRaw) return QObject::tr("the remote content is a raw routing profile, which is not supported for remote profiles yet");

        replaceRules(*profile, *fetched);
        if (profile->name.trimmed().isEmpty() && !fetched->name.trimmed().isEmpty()) {
            profile->name = fetched->name;
        }
        profile->remoteLastUpdate = QDateTime::currentSecsSinceEpoch();
        if (warnings) *warnings = warn;
        return {};
    }
}

void UI_update_all_remote_routes(bool onlyAllowed) {
    runOnNewThread([=] {
        auto profiles = Configs::dataManager->routesRepo->GetAllRouteProfiles();
        int updated = 0;
        QStringList failures;
        for (const auto& p : profiles) {
            if (!p->isRemote || p->remoteURL.trimmed().isEmpty()) continue;
            if (onlyAllowed && !p->autoUpdate) continue;

            MW_show_log(">>>>>>>> " + QObject::tr("Updating remote routing profile: %1").arg(p->name));
            const bool running = p->id == Configs::RunningRouteId();
            const QByteArray before = running ? RouteUpdate::routeFingerprint(*p) : QByteArray();
            QString warnings;
            auto err = RouteUpdate::UpdateProfile(p, &warnings);
            if (!err.isEmpty()) {
                failures << (p->name + ": " + err);
                MW_show_log("<<<<<<<< " + QObject::tr("Remote routing profile %1 failed: %2").arg(p->name, err));
                continue;
            }
            Configs::dataManager->routesRepo->Save(p);
            updated++;
            if (running && RouteUpdate::routeFingerprint(*p) != before) runOnUiThread(RouteUpdate::armRoutingRestart);
            MW_show_log("<<<<<<<< " + QObject::tr("Remote routing profile updated: %1").arg(p->name)
                        + (warnings.isEmpty() ? QString() : "\n" + warnings));
        }
        MW_show_log(QObject::tr("Remote routing profiles: %1 updated, %2 failed").arg(updated).arg(failures.size()));
    });
}
