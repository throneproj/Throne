#include "include/configs/generate.h"
#include "include/configs/generate_internal.h"
#include "include/api/RPC.h"
#include "include/configs/AutoSelectorPlan.h"
#include "include/configs/common/Protocols.h"
#include "include/configs/common/ResolverSource.h"
#include "include/configs/common/utils.h"
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

        // ----------------------------------------------------------- outbounds

        struct chainScan {
            int hopCount = 0;
            int extraCoreCount = 0;
            int extraCoreIdx = -1;
            int xrayFullConfigCount = 0;
            int xrayFullConfigIdx = -1;
            int xrayHopCount = 0;
            bool hasCustomFullConfig = false;
            int coreTransitions = 0;
        };

        QString chainScanError(const chainScan &scan) {
            if (scan.hasCustomFullConfig)
                return "Custom full config profiles cannot be used in a chain; only custom outbound profiles are chainable";
            if (scan.extraCoreCount > 1)
                return "Only one extra-core profile is allowed in a chain";
            if (scan.xrayFullConfigCount > 1)
                return "Only one custom Xray full config profile is allowed in a chain";
            if (scan.extraCoreCount > 0 && scan.xrayFullConfigCount > 0)
                return "Extra-core and custom Xray full config profiles cannot be combined in a chain";
            if (scan.xrayFullConfigCount > 0 && scan.xrayHopCount > 0)
                return "Custom Xray full config cannot be combined with other Xray hops in a chain (only one Xray instance is supported at a time)";
            // An extra core must be the last hop so its local socks server is dialed directly; sing-box does no hops after it.
            if (scan.extraCoreCount == 1 && scan.hopCount > 1 && scan.extraCoreIdx != scan.hopCount - 1)
                return "Extra-core profiles can only be the final hop in a chain (top of the chain editor)";
            // Same for a custom Xray full config: traffic exits through its socks bridge into the user's Xray.
            if (scan.xrayFullConfigCount == 1 && scan.hopCount > 1 && scan.xrayFullConfigIdx != scan.hopCount - 1)
                return "Custom Xray full config can only be the final hop in a chain (top of the chain editor)";
            if (scan.coreTransitions > 2)
                return "Too many core transitions, the valid sequence is: (optional sing-box chain)->(optional xray chain)->(optional sing-box chain)";
            return {};
        }

    } // namespace

    namespace gen {

        // Names the setting that still points at a deleted profile, so the user knows what to amend.
        QString missingHopError(const BuildContext &ctx, int id)
        {
            const auto idText = QString::number(id);
            if (ctx.ent != nullptr && ctx.ent->type == "chain")
            {
                const auto chain = ctx.ent->Chain();
                if (chain != nullptr && chain->list.contains(id))
                    return QObject::tr("Chain \"%1\" has a hop (profile id %2) that no longer exists, edit the chain to remove it").arg(ctx.ent->name, idText);
            }
            auto groupIDs = dataManager->groupsRepo->GetAllGroupIds();
            if (ctx.ent != nullptr) groupIDs.prepend(ctx.ent->gid);
            for (const int gid : groupIDs)
            {
                const auto group = dataManager->groupsRepo->GetGroup(gid);
                if (group == nullptr) continue;
                if (group->front_proxy_id == id)
                    return QObject::tr("Group \"%1\" has a front proxy that has been deleted.").arg(group->name, idText);
                if (group->landing_proxy_id == id)
                    return QObject::tr("Group \"%1\" has a landing proxy that has been deleted.").arg(group->name, idText);
            }
            return QObject::tr("A proxy in the chain can no longer be found.").arg(idText);
        }

    } // namespace gen

    namespace {

        struct resolvedHops {
            QList<std::shared_ptr<Profile>> ents;
            bool singToXray = false;
            bool xrayToSing = false;
            QString error;
        };

        resolvedHops entIDListtoEntList(const BuildContext &ctx, const QList<int> &entIDs)
        {
            resolvedHops res;
            auto &ents = res.ents;
            chainScan scan;
            bool inXray = false;
            for (auto id : entIDs)
            {
                if (id == warpProfileID) {
                    if (inXray) {
                        res.xrayToSing = true;
                        scan.coreTransitions++;
                    }
                    inXray = false;
                    auto exitEnt = exitProfile(&res.error);
                    if (exitEnt == nullptr) return res;
                    ents.append(std::move(exitEnt));
                    continue;
                }
                auto ent = getProfile(id);
                if (ent == nullptr)
                {
                    res.error = missingHopError(ctx, id);
                    return res;
                }
                if (!inXray && ent->outbound->IsXray()) {
                    res.singToXray = true;
                    scan.coreTransitions++;
                }
                if (inXray && !ent->outbound->IsXray()) {
                    res.xrayToSing = true;
                    scan.coreTransitions++;
                }
                inXray = ent->outbound->IsXray();
                if (ent->type == "chain")
                {
                    res.error = "Chain in Chain is not allowed";
                    return res;
                }
                // A selector resolves to a different member over time; a chain hop has to stay put.
                if (ent->type == "autoselector")
                {
                    res.error = "An auto selector cannot be used as a hop; it is not a fixed server";
                    return res;
                }
                if (ent->outbound != nullptr && ent->outbound->IsExtraCore()) {
                    scan.extraCoreCount++;
                    scan.extraCoreIdx = static_cast<int>(ents.size());
                }
                if (ent->outbound != nullptr && ent->outbound->IsXrayFullConfig()) {
                    scan.xrayFullConfigCount++;
                    scan.xrayFullConfigIdx = static_cast<int>(ents.size());
                }
                if (ent->outbound != nullptr && ent->outbound->IsXray()) {
                    scan.xrayHopCount++;
                }
                if (isCustomFullConfig(ent)) scan.hasCustomFullConfig = true;
                ents.append(ent);
            }
            scan.hopCount = static_cast<int>(ents.size());
            if (auto scanError = chainScanError(scan); !scanError.isEmpty()) res.error = scanError;
            return res;
        }

        struct hopChainOptions {
            QString prefix;
            bool includeProxy = false;
            bool link = true;
            int startSuffix = 0;
            bool warpWrap = false;
            bool auxiliary = false;
            QSet<QString> addressableTags;
            // The last hop's detour, an outbound built outside this chain.
            QString tailDetour;
            bool tailResolvesViaDns = false;
            // A custom Xray full config hop is emitted as a socks outbound into this bridge.
            std::optional<BridgeEndpoint> xrayFullConfigBridge;
        };

    } // namespace

    namespace gen {

        bool resolvesHostnamesViaDnsRules(const Profile &hop) {
            if (hop.outbound->IsEndpoint()) return true;
            const auto *socksOutbound = hop.Socks();
            return socksOutbound != nullptr && socksOutbound->version == 4 && !socksOutbound->socks4a;
        }

    } // namespace gen

    namespace {

        // A resolver ParseDnsAddress refuses, or a dhcp one, falls back to dns-direct like an unset one.
        bool usableEchResolver(const QString &resolver) {
            DnsAddress parsed;
            return ParseDnsAddress(resolver, &parsed) && parsed.type != "dhcp";
        }

    } // namespace

    namespace gen {

        // Without a static config the core fetches the ECH list over DNS first; via dns-remote that dials the same hop.
        void collectEchQueryName(HopNeeds &needs, const Profile &hop) {
            if (!hop.outbound->HasTLS()) return;
            const auto tls = hop.outbound->GetTLS();
            if (!tls->enabled || !tls->ech->enabled) return;
            if (!tls->ech->config.isEmpty() || !tls->ech->config_path.isEmpty()) return;
            // The core queries query_server_name when set, else the TLS server name, else the dial host.
            QString name = tls->ech->serverName;
            if (name.isEmpty()) name = tls->server_name;
            if (name.isEmpty()) name = hop.outbound->server;
            name = toAceHost(name.trimmed());
            if (name.isEmpty() || QHostAddress(name).protocol() != QAbstractSocket::UnknownNetworkLayerProtocol) return;
            if (usableEchResolver(tls->ech->resolver)) {
                if (!needs.echResolvers.contains(name)) needs.echResolvers.insert(name, tls->ech->resolver);
            } else if (!needs.echQueryNames.contains(name)) {
                needs.echQueryNames << name;
            }
        }

    } // namespace gen

    namespace {

        void collectDnsTunnelDomains(HopNeeds &needs, const Profile &hop) {
            QStringList domains;
            if (const auto *dnstt = hop.Dnstt()) domains << dnstt->domain;
            if (const auto *masterDns = hop.MasterDns()) domains << masterDns->domains;
            for (const auto &domain : domains) {
                // sing-box lower-cases the query name but matches domain_suffix entries verbatim.
                auto name = toAceHost(domain.trimmed()).toLower();
                if (name.endsWith(u'.')) name.chop(1);
                if (!name.isEmpty() && !needs.dnsTunnelDomains.contains(name)) needs.dnsTunnelDomains << name;
            }
        }

        void buildSingboxChain(ChainOutput &out, const QList<std::shared_ptr<Profile>> &ents, const hopChainOptions &opts) {
            for (int idx = 0; idx < ents.size(); idx++)
            {
                auto tag = hopTag(opts.prefix, opts.startSuffix + idx);
                QString nextTag;
                if (idx < ents.size() - 1) nextTag = hopTag(opts.prefix, opts.startSuffix + idx + 1);
                if (opts.includeProxy && idx == 0) tag = tags::proxy;
                // idx 0 is the exit under the tag "proxy", so idx 1 takes "warp-bypass" for rules to name.
                if (opts.warpWrap && idx == 1) tag = tags::warpBypass;
                const auto& ent = ents[idx];
                // Only the head hop (and the exit's wrapped outbound) gets a tag rules can name.
                const bool addressableHop = idx == 0 || (opts.warpWrap && idx == 1) || opts.addressableTags.contains(tag);
                if (addressableHop && Protocols::Is(ent->type, Protocols::VpnEndpoint)) {
                    out.vpnEndpointProfiles.insert(tag, ent->id);
                    const auto *ovpn = ent->OpenVPN();
                    const auto *ocon = ent->OpenConnect();
                    const bool gated = !opts.auxiliary &&
                                       ((ovpn != nullptr && ovpn->only_advertised_routes) ||
                                        (ocon != nullptr && ocon->only_advertised_routes));
                    const QString tunnelDns = ovpn != nullptr ? ovpn->tunnel_dns
                                            : ocon != nullptr ? ocon->tunnel_dns : QString();
                    if (tunnelDns != kTunnelDnsNone) out.needs.vpnEndpointTags.insert(tag, ent->type);
                    // Only the main profile owns the remote queries; an auxiliary tunnel keeps to the names it claims.
                    if (tunnelDns == kTunnelDnsStrict && (tag == tags::proxy || tag == tags::warpBypass))
                        out.needs.vpnStrictTag = tag;
                    if (gated) out.needs.vpnGateTags << tag;
                }
                auto [object, error] = opts.xrayFullConfigBridge && ent->outbound->IsXrayFullConfig()
                                           ? BuildResult{socksBridgeOutbound(*opts.xrayFullConfigBridge), {}}
                                           : ent->outbound->Build();
                if (!error.isEmpty())
                {
                    out.error += error;
                    return;
                }
                object["tag"] = tag;
                collectEchQueryName(out.needs, *ent);
                collectDnsTunnelDomains(out.needs, *ent);
                // Realm reads its STUN resolver off this key only; without it the hosts go through DNS rules.
                if (auto hy = ent->Hysteria(); hy != nullptr && hy->RealmActive())
                    object["domain_resolver"] = QJsonObject{{"server", tags::dnsDirect}};
                const bool tail = nextTag.isEmpty() && !opts.tailDetour.isEmpty();
                if (!nextTag.isEmpty() && opts.link) object["detour"] = nextTag;
                if (tail) object["detour"] = opts.tailDetour;
                if (opts.warpWrap && idx == 0) object["detour"] = tags::warpBypass;
                // A detour gets the server hostname unresolved; endpoints look it up via DNS rules, which end at dns-remote over the proxy.
                const bool detourResolvesViaDns = tail ? opts.tailResolvesViaDns
                                                       : !nextTag.isEmpty() && opts.link && resolvesHostnamesViaDnsRules(*ents[idx + 1]);
                if (detourResolvesViaDns && !ent->outbound->IsEndpoint() && !object.contains("domain_resolver"))
                    object["domain_resolver"] = directDomainResolver();
                if (ent->outbound->IsEndpoint())
                {
                    out.endpoints.append(object);
                } else
                {
                    out.outbounds.append(object);
                }
            }
        }

        void buildXrayChain(ChainOutput &out, const QList<std::shared_ptr<Profile>> &ents, const hopChainOptions &opts,
                            const std::optional<BridgeEndpoint> &xrayToSing) {
            for (int idx = 0; idx < ents.size(); idx++)
            {
                auto tag = hopTag(opts.prefix, opts.startSuffix + idx);
                QString nextTag;
                if (idx < ents.size() - 1 || xrayToSing.has_value()) nextTag = hopTag(opts.prefix, opts.startSuffix + idx + 1);
                if (opts.includeProxy && idx == 0) tag = tags::proxy;
                const auto& ent = ents[idx];
                auto [object, error] = ent->outbound->BuildXray();
                if (!error.isEmpty())
                {
                    out.error += error;
                    return;
                }
                object["tag"] = tag;
                // Xray removed outbound-level proxySettings; sockopt.dialerProxy is its 1:1 replacement.
                if (!nextTag.isEmpty() && (opts.link || xrayToSing.has_value())) SetXrayDialerProxy(object, nextTag);
                if (dataManager->settingsRepo->xray_use_system_ca) UseXraySystemCA(object);
                out.xrayOutbounds.append(object);
            }
            if (xrayToSing.has_value()) {
                out.xrayOutbounds.append(QJsonObject{
                    {"tag", hopTag(opts.prefix, opts.startSuffix + static_cast<int>(ents.size()))},
                    {"protocol", "socks"},
                    {"settings", QJsonObject{
                        {"address", xrayToSing->host},
                        {"port", xrayToSing->port},
                        {"user", xrayToSing->auth},
                        {"pass", xrayToSing->auth},
                    }},
                });
            }
        }

    } // namespace

    namespace gen {

        ChainOutput buildOutboundChain(const BuildContext &ctx, const ChainBuildRequest &req)
        {
            ChainOutput out;
            const auto ingressTag = req.includeProxy ? QString(tags::proxy) : hopTag(req.prefix, req.startSuffix);
            out.ingressTag = ingressTag;

            auto [ents, singToXray, xrayToSing, hopError] = entIDListtoEntList(ctx, req.hopIDs);
            out.error = hopError;
            // After an earlier failure only the hops are resolved: absorbChain lets a hop error replace that failure.
            if (!out.error.isEmpty() || !ctx.error.isEmpty()) return out;

            std::optional<BridgeEndpoint> fullConfigBridge;
            if (!ents.isEmpty() && ents.last()->outbound != nullptr && ents.last()->outbound->IsXrayFullConfig()) {
                auto custom = ents.last()->Custom();
                if (custom == nullptr) {
                    out.error = "Failed to cast to Custom for Xray full config hop";
                    return out;
                }
                auto userXrayConfig = QString2QJsonObject(custom->config);
                if (userXrayConfig.isEmpty()) {
                    out.error = "Custom Xray full config is not valid JSON";
                    return out;
                }
                // A pre-probed 0 means the caller's probe failed; re-probe rather than bake in a dead port.
                BridgeEndpoint bridge{.port = req.xrayFullConfigPort};
                if (bridge.port <= 0) bridge.port = MkManyPorts(1, bridge.host)[0];
                if (bridge.port <= 0) {
                    out.error = "Could not reserve a local port for the custom Xray full config bridge";
                    return out;
                }
                bridge.auth = GetRandomString(32);
                out.bridges.append({Bridge::Kind::XrayFullConfig, bridge, {}});
                fullConfigBridge = bridge;

                auto bridgeInbound = xraySocksInbound(tags::xrayFullConfigIn, bridge);
                bridgeInbound["sniffing"] = QJsonObject{
                    {"enabled", true},
                    {"destOverride", QJsonArray{"http", "tls", "quic"}},
                    {"routeOnly", false}
                };
                RouteXrayListenerRulesViaBridge(userXrayConfig);
                if (req.xrayRuleTarget) {
                    // A second instance must not reopen the subscription's listeners or process-wide services.
                    static const QSet<QString> isolated = {"inbounds", "api", "metrics", "reverse", "env", "geodata"};
                    for (const auto &key : userXrayConfig.keys())
                        if (isolated.contains(key.toLower())) userXrayConfig.remove(key);
                }
                auto inbounds = (!ctx.policy.listeners || req.soleXrayInbound) ? QJsonArray()
                                                                               : userXrayConfig["inbounds"].toArray();
                inbounds.prepend(bridgeInbound);
                userXrayConfig["inbounds"] = inbounds;
                if (dataManager->settingsRepo->xray_use_system_ca) {
                    auto outbounds = userXrayConfig["outbounds"].toArray();
                    for (qsizetype i = 0; i < outbounds.size(); ++i) {
                        if (!outbounds[i].isObject()) continue;
                        auto outbound = outbounds[i].toObject();
                        UseXraySystemCA(outbound);
                        outbounds[i] = outbound;
                    }
                    userXrayConfig["outbounds"] = outbounds;
                }

                out.xrayFullConfig = userXrayConfig;
            }

            QList<std::shared_ptr<Profile>> initialSingEnts;
            QList<std::shared_ptr<Profile>> xrayEnts;
            QList<std::shared_ptr<Profile>> tailingSingEnts;
            for (const auto& ent : ents) {
                if (ent->outbound->IsXray()) xrayEnts.append(ent);
                else {
                    if (xrayEnts.isEmpty()) initialSingEnts.append(ent);
                    else tailingSingEnts.append(ent);
                }
            }
            const bool xrayTail = !req.tailDetour.isEmpty() && !xrayEnts.isEmpty() && tailingSingEnts.isEmpty();
            if (xrayTail) xrayToSing = true;
            QList<int> ports;
            // A pre-probed 0 means the caller's probe failed; re-probe rather than bake in a dead port.
            auto bridgePort = [&ports](int given) {
                if (given > 0) return given;
                if (ports.isEmpty()) ports = MkManyPorts(2);
                return ports.takeFirst();
            };
            if (singToXray) {
                const int port = bridgePort(req.singToXrayPort);
                if (port <= 0) {
                    out.error = "Could not reserve a local port for the sing-box -> Xray bridge";
                    return out;
                }
                const BridgeEndpoint singToXrayBridgeConf{port, GetRandomString(32)};
                // Xray's head hop carries the chain's ingress tag in Xray's own tag space.
                out.bridges.append({Bridge::Kind::SingToXray, singToXrayBridgeConf, ingressTag});
                auto bridgeEnt = ProfilesRepo::NewProfile("socks");
                auto socksOutbound = bridgeEnt->Socks();
                socksOutbound->username = singToXrayBridgeConf.auth;
                socksOutbound->password = singToXrayBridgeConf.auth;
                socksOutbound->server = singToXrayBridgeConf.host;
                socksOutbound->server_port = singToXrayBridgeConf.port;
                initialSingEnts << bridgeEnt;
            }
            const int tailingStartSuffix = req.startSuffix + static_cast<int>(initialSingEnts.size());
            std::optional<BridgeEndpoint> xrayToSingBridgeConf;
            if (xrayToSing) {
                const int port = bridgePort(req.xrayToSingPort);
                if (port <= 0) {
                    out.error = "Could not reserve a local port for the Xray -> sing-box bridge";
                    return out;
                }
                xrayToSingBridgeConf = BridgeEndpoint{port, GetRandomString(32)};
                // Back into sing-box at the tailing chain's head, or at the detour when Xray ends the chain.
                out.bridges.append({Bridge::Kind::XrayToSing, *xrayToSingBridgeConf, xrayTail ? req.tailDetour : hopTag(req.prefix, tailingStartSuffix)});
            }

            const hopChainOptions leadingOpts{
                .prefix = req.prefix,
                .includeProxy = req.includeProxy,
                .link = req.link,
                .startSuffix = req.startSuffix,
                .warpWrap = req.warpWrap,
                .auxiliary = req.auxiliary,
                .addressableTags = req.addressableTags,
                .tailDetour = xrayEnts.isEmpty() ? req.tailDetour : QString(),
                .tailResolvesViaDns = req.tailResolvesViaDns,
                .xrayFullConfigBridge = fullConfigBridge,
            };
            if (!initialSingEnts.isEmpty()) {
                buildSingboxChain(out, initialSingEnts, leadingOpts);
            }
            if (!xrayEnts.isEmpty()) {
                buildXrayChain(out, xrayEnts, leadingOpts, xrayToSingBridgeConf);
            }
            if (!tailingSingEnts.isEmpty()) {
                buildSingboxChain(out, tailingSingEnts, {
                    .prefix = req.prefix,
                    .includeProxy = false,
                    .link = req.link,
                    .startSuffix = tailingStartSuffix,
                    .warpWrap = false,
                    .tailDetour = req.tailDetour,
                    .tailResolvesViaDns = req.tailResolvesViaDns,
                });
            }

            if (!ents.isEmpty()) {
                TrafficChainGroup group;
                for (const auto &ent : ents) group.profiles << generateEndpointLive(ent);
                if (!tailingSingEnts.isEmpty()) {
                    group.watchTag = hopTag(req.prefix, tailingStartSuffix);
                } else {
                    group.watchTag = ingressTag;
                }
                out.traffic = group;
            }
            return out;
        }

        void absorbChain(BuildContext &ctx, const ChainOutput &out)
        {
            if (!out.error.isEmpty()) ctx.error = out.error;
            QJSONARRAY_ADD(ctx.outbounds, out.outbounds)
            QJSONARRAY_ADD(ctx.endpoints, out.endpoints)
            QJSONARRAY_ADD(ctx.xrayOutbounds, out.xrayOutbounds)
            for (const auto &bridge : out.bridges) ctx.bridges.add(bridge);
            ctx.needs.merge(out.needs);
            ctx.result->vpnEndpointProfiles.insert(out.vpnEndpointProfiles);
        }

        void HopNeeds::merge(const HopNeeds &later)
        {
            for (const auto &name : later.echQueryNames) {
                if (!echQueryNames.contains(name)) echQueryNames << name;
            }
            for (auto it = later.echResolvers.cbegin(); it != later.echResolvers.cend(); ++it) {
                if (!echResolvers.contains(it.key())) echResolvers.insert(it.key(), it.value());
            }
            for (const auto &domain : later.dnsTunnelDomains) {
                if (!dnsTunnelDomains.contains(domain)) dnsTunnelDomains << domain;
            }
            vpnEndpointTags.insert(later.vpnEndpointTags);
            if (!later.vpnStrictTag.isEmpty()) vpnStrictTag = later.vpnStrictTag;
            vpnGateTags << later.vpnGateTags;
        }

    } // namespace gen
}
