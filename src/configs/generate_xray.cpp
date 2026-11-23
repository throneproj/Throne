#include "include/configs/generate.h"
#include "include/configs/generate_internal.h"
#include "include/api/RPC.h"
#include "include/configs/AutoSelectorPlan.h"
#include "include/configs/common/ResolverSource.h"
#include "include/configs/common/utils.h"
#include "include/configs/outbounds/tor.h"
#include "include/global/Configs.hpp"

#include <QApplication>
#include <QFileInfo>
#include <QHostAddress>
#include <QMutex>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QThreadPool>


#include "include/database/GroupsRepo.h"
#include "include/database/IpListsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"


#include "include/database/entities/Profile.h"
#include "include/global/OtpPlaceholder.hpp"
#include "include/global/RunningProfiles.hpp"
#include "include/global/VpnCredentialOverride.hpp"
#ifdef Q_OS_LINUX
#include "include/sys/linux/systemChecks.h"
#endif

#include <algorithm>
#include <optional>
#include <string_view>

namespace Configs {
    using namespace gen;

    namespace {

        QJsonObject socksBridgeInbound(const QString &tag, const BridgeEndpoint &bridge) {
            return QJsonObject{
                {"type", "socks"},
                {"tag", tag},
                {"listen", bridge.host},
                {"listen_port", bridge.port},
                {"users", QJsonArray{QJsonObject{
                    {"username", bridge.auth},
                    {"password", bridge.auth},
                }}},
            };
        }

        // Pool members bridging into one shared upstream repeat its ingress tag.
        QString singInboundTag(const QList<Bridge> &bridges, qsizetype idx, BridgeTagScheme scheme) {
            const auto &bridge = bridges[idx];
            if (scheme == BridgeTagScheme::ByPort) return QString(tags::bridgePrefix) + "-" + Int2String(bridge.endpoint.port);
            const auto repeats = std::count_if(bridges.cbegin(), bridges.cbegin() + idx, [&bridge](const Bridge &earlier) {
                return earlier.kind == Bridge::Kind::XrayToSing && earlier.target == bridge.target;
            });
            const auto tag = QString(tags::bridgePrefix) + "-" + bridge.target;
            return repeats == 0 ? tag : tag + "-" + Int2String(static_cast<int>(repeats));
        }

        QString xrayInboundTag(const Bridge &bridge) { return bridge.target + "-inbound"; }

    } // namespace

    namespace gen {

        QJsonObject xraySocksInbound(const QString &tag, const BridgeEndpoint &bridge) {
            return QJsonObject{
                {"tag", tag},
                {"listen", bridge.host},
                {"port", bridge.port},
                {"protocol", "socks"},
                {"settings", QJsonObject{
                    {"auth", "password"},
                    {"udp", true},
                    {"accounts", QJsonArray{QJsonObject{
                        {"user", bridge.auth},
                        {"pass", bridge.auth},
                    }}},
                }},
            };
        }

        QJsonObject socksBridgeOutbound(const BridgeEndpoint &bridge) {
            return QJsonObject{
                {"type", "socks"},
                {"server", bridge.host},
                {"server_port", bridge.port},
                {"username", bridge.auth},
                {"password", bridge.auth},
            };
        }

        QJsonArray BridgeSet::singInbounds(BridgeTagScheme scheme) const {
            QJsonArray inbounds;
            for (qsizetype idx = 0; idx < items_.size(); idx++) {
                if (items_[idx].kind != Bridge::Kind::XrayToSing) continue;
                inbounds.append(socksBridgeInbound(singInboundTag(items_, idx, scheme), items_[idx].endpoint));
            }
            return inbounds;
        }

        QJsonArray BridgeSet::singRouteRules(BridgeTagScheme scheme) const {
            QJsonArray rules;
            for (qsizetype idx = 0; idx < items_.size(); idx++) {
                if (items_[idx].kind != Bridge::Kind::XrayToSing) continue;
                rules.append(QJsonObject{
                    {"inbound", singInboundTag(items_, idx, scheme)},
                    {"action", "route"},
                    {"outbound", items_[idx].target},
                });
            }
            return rules;
        }

        QJsonArray BridgeSet::xrayInbounds() const {
            QJsonArray inbounds;
            for (const auto &bridge : items_) {
                if (bridge.kind != Bridge::Kind::SingToXray) continue;
                inbounds.append(xraySocksInbound(xrayInboundTag(bridge), bridge.endpoint));
            }
            return inbounds;
        }

        QJsonArray BridgeSet::xrayRouteRules() const {
            QJsonArray rules;
            for (const auto &bridge : items_) {
                if (bridge.kind != Bridge::Kind::SingToXray) continue;
                rules.append(QJsonObject{
                    {"type", "field"},
                    {"inboundTag", QJsonArray{xrayInboundTag(bridge)}},
                    {"outboundTag", bridge.target},
                });
            }
            return rules;
        }

    } // namespace gen

    namespace {

        // ----------------------------------------------------------------- xray

        // Xray picks these outbounds by tag prefix; a balancer's selector may also be one comma-separated string.
        QStringList xrayTagSelectors(const QJsonObject &config) {
            QStringList selectors;
            const auto collect = [&selectors](const QJsonValue &value) {
                if (value.isString()) selectors << value.toString().split(u',');
                for (const auto &item : value.toArray()) {
                    if (item.isString()) selectors << item.toString();
                }
            };
            for (const auto &balancer : config.value("routing").toObject().value("balancers").toArray())
                collect(balancer.toObject().value("selector"));
            collect(config.value("observatory").toObject().value("subjectSelector"));
            collect(config.value("burstObservatory").toObject().value("subjectSelector"));
            return selectors;
        }

        // The user's first outbound stays the default, and the bridge rules go first so no user rule takes their traffic.
        void mergeIntoXrayFullConfig(BuildContext &ctx, const QJsonArray &inbounds, const QJsonArray &routeRules) {
            auto &config = ctx.result->xrayConfig;
            auto userInbounds = config.value("inbounds").toArray();
            auto userOutbounds = config.value("outbounds").toArray();
            const auto clash = [](const QJsonArray &generated, const QJsonArray &user) -> QString {
                QSet<QString> userTags;
                for (const auto &item : user) userTags.insert(item.toObject().value("tag").toString());
                for (const auto &item : generated) {
                    if (const auto tag = item.toObject().value("tag").toString(); userTags.contains(tag)) return tag;
                }
                return {};
            };
            for (const auto &tag : {clash(inbounds, userInbounds), clash(ctx.xrayOutbounds, userOutbounds)}) {
                if (tag.isEmpty()) continue;
                ctx.error = QObject::tr("The custom Xray config uses the tag \"%1\", which Throne also generates for another Xray profile; rename it in the custom config").arg(tag);
                return;
            }
            const auto selectors = xrayTagSelectors(config);
            for (const auto &item : std::as_const(ctx.xrayOutbounds)) {
                const auto tag = item.toObject().value("tag").toString();
                for (const auto &selector : selectors) {
                    if (!tag.startsWith(selector)) continue;
                    ctx.error = QObject::tr("The custom Xray config's selector \"%1\" also matches \"%2\", which Throne generates for another Xray profile; make the selector more specific in the custom config").arg(selector, tag);
                    return;
                }
            }
            for (const auto &item : inbounds) userInbounds.append(item);
            for (const auto &item : std::as_const(ctx.xrayOutbounds)) userOutbounds.append(item);
            auto routing = config.contains("routing") ? config.value("routing").toObject() : QJsonObject{{"domainStrategy", "AsIs"}};
            auto rules = routeRules;
            for (const auto &rule : routing.value("rules").toArray()) rules.append(rule);
            routing["rules"] = rules;
            config["inbounds"] = userInbounds;
            config["outbounds"] = userOutbounds;
            config["routing"] = routing;
        }

    } // namespace

    namespace gen {

        void buildXrayConfig(BuildContext &ctx) {
            if (ctx.xrayOutbounds.isEmpty()) return;
            ctx.result->isXrayNeeded = true;
            const auto inbounds = ctx.bridges.xrayInbounds();
            const auto routeRules = ctx.bridges.xrayRouteRules();

            // Only a custom Xray full config that ended a chain can already be in the slot.
            if (!ctx.result->xrayConfig.isEmpty()) {
                mergeIntoXrayFullConfig(ctx, inbounds, routeRules);
                return;
            }

            ctx.result->xrayConfig["log"] = QJsonObject{
            {"loglevel", dataManager->settingsRepo->xray_log_level},
            {"access", dataManager->settingsRepo->xray_log_level == "info" ? "" : "none"}
            };
            ctx.result->xrayConfig["inbounds"] = inbounds;
            ctx.result->xrayConfig["outbounds"] = ctx.xrayOutbounds;
            ctx.result->xrayConfig["routing"] = QJsonObject{
                {"domainStrategy", "AsIs"},
                {"rules", routeRules},
            };
        }

    } // namespace gen
}
