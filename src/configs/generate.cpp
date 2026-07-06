#include "include/configs/generate.h"
#include "include/api/RPC.h"
#include "include/global/Configs.hpp"

#include <QApplication>
#include <QFileInfo>


#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"


#include "include/database/entities/Profile.h"
#include "include/sys/linux/systemChecks.h"

#include <algorithm>
#include <string_view>

namespace {
    // Single binary search over the sorted ruleSetList — replaces the
    // ruleSetMap.contains + ruleSetMap.at lookups from the former std::map.
    std::string_view ruleSetUrl(std::string_view key) {
        auto it = std::lower_bound(ruleSetList.begin(), ruleSetList.end(), key,
            [](const auto& e, std::string_view k) { return e.first < k; });
        return (it != ruleSetList.end() && it->first == key) ? it->second : std::string_view{};
    }
}

namespace Configs {

    QString genTunName() {
        auto tun_name = "throne-tun";
#ifdef Q_OS_MACOS
        tun_name = "";
#endif
        return tun_name;
    }

    void MergeJson(const QJsonObject &custom, QJsonObject &outbound) {
        if (custom.isEmpty()) return;
        for (const auto &key: custom.keys()) {
            if (outbound.contains(key)) {
                auto v = custom[key];
                auto v_orig = outbound[key];
                if (v.isObject() && v_orig.isObject()) {
                    auto vo = v.toObject();
                    QJsonObject vo_orig = v_orig.toObject();
                    MergeJson(vo, vo_orig);
                    outbound[key] = vo_orig;
                } else {
                    outbound[key] = v;
                }
            } else {
                outbound[key] = custom[key];
            }
        }
    }

    QStringList getChainDomains (const std::shared_ptr<Profile>& ent, QString &error)
    {
        QStringList domains;
        auto chain = ent->Chain();
        if (!chain)
        {
            error = "Ent is Nullptr after cast to chain in getChainDomains, data is corrupted";
            return domains;
        }
        auto entIDs = ent->Chain()->list;
        for (int id : entIDs)
        {
            if (auto subEnt = Configs::dataManager->profilesRepo->GetProfile(id); subEnt != nullptr)
            {
                if (subEnt->outbound != nullptr && subEnt->outbound->IsExtraCore()) continue;
                if (auto addr = subEnt->outbound->GetAddress(); !addr.isEmpty() && !IsIpAddress(addr)) domains.append(addr);
            }
        }
        return domains;
    }

    QStringList getEntDomains(const QList<int>& entIDs, QString &error)
    {
        QStringList domains;
        for (const auto &id: entIDs)
        {
            if (auto ent = Configs::dataManager->profilesRepo->GetProfile(id); ent != nullptr)
            {
                if (ent->outbound != nullptr && ent->outbound->IsExtraCore()) continue;
                if (ent->type == "chain") domains << getChainDomains(ent, error);
                else
                {
                    if (auto addr = ent->outbound->GetAddress(); !addr.isEmpty() && !IsIpAddress(addr)) domains.append(addr);
                }
            }
        }

        return domains;
    }

    std::shared_ptr<Profile> getWarpProfile() {
        auto warpProfile = std::make_shared<Profile>();
        warpProfile->name = "warp";
        warpProfile->id = warpProfileID;
        warpProfile->type = "wireguard";
        auto outbound = std::make_shared<wireguard>();
        outbound->name = "warp";
        outbound->server = dataManager->settingsRepo->warp_ep.contains(":") ? SubStrBefore(dataManager->settingsRepo->warp_ep, ":") : dataManager->settingsRepo->warp_ep;
        outbound->server_port = dataManager->settingsRepo->warp_ep.contains(":") ? SubStrAfter(dataManager->settingsRepo->warp_ep, ":").toInt() : 2408;
        outbound->private_key = dataManager->settingsRepo->warp_private_key;
        outbound->address = dataManager->settingsRepo->warp_ifc_addrs;
        auto peer = std::make_shared<Peer>();
        peer->public_key = dataManager->settingsRepo->warp_public_key;
        peer->address = outbound->server;
        peer->port = outbound->server_port;
        outbound->peer = peer;
        outbound->mtu = 1280;

        warpProfile->outbound = outbound;
        return warpProfile;
    }

    void CalculatePrerequisities(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        ctx->tunEnabled = Configs::dataManager->settingsRepo->spmode_vpn;
        ctx->os = getOS();
        if (ctx->os == Linux)
        {
            ctx->isResolvedUsed = isSystemdResolvedDefaultResolver();
        }
        // Resolve the physical default-route interface once per real build. When
        // Xray is the final egress under TUN we bind its egress socket directly
        // to this interface (streamSettings.sockopt.interface) instead of
        // detouring through a sing-box socks loopback bridge. Skipped for
        // test/export builds (an export must stay machine-portable) and when TUN
        // is off (no loop to prevent). On any RPC failure defaultInterface stays
        // empty and the build falls back to the loopback bridge.
        if (ctx->tunEnabled)
        {
            bool ifcOK = false;
            auto ifc = API::defaultClient->GetDefaultInterface(&ifcOK);
            if (ifcOK && !ifc.isEmpty()) ctx->defaultInterface = ifc;
        }
        auto preReqs = ctx->buildPrerequisities;
        
        // Get route chain
        auto routeChain = Configs::dataManager->routesRepo->GetRouteProfile(Configs::dataManager->settingsRepo->current_route_id);
        if (routeChain == nullptr) {
            ctx->error = "Routing profile does not exist, try resetting the route profile in Routing Settings";
            return;
        }

        // Warp is enabled but its config was never generated: getWarpProfile()
        // would build a wireguard outbound from empty keys/endpoint/addresses,
        // which fails silently downstream. Break the flow early and tell the
        // user to generate the warp config first.
        if (dataManager->settingsRepo->enable_warp &&
            (dataManager->settingsRepo->warp_private_key.isEmpty() ||
             dataManager->settingsRepo->warp_public_key.isEmpty() ||
             dataManager->settingsRepo->warp_ep.isEmpty() ||
             dataManager->settingsRepo->warp_ifc_addrs.isEmpty())) {
            ctx->error = "Warp is enabled but its config has not been generated. Please generate the Warp config first in Routing Settings.";
            return;
        }

        // Routing dependencies
        auto neededOutbounds = routeChain->get_used_outbounds();
        auto neededRuleSets = routeChain->get_used_rule_sets();
        preReqs->routingDeps->defaultOutboundID = routeChain->defaultOutboundID;
        preReqs->routingDeps->outboundMap[-1] = "proxy";
        preReqs->routingDeps->outboundMap[-2] = "direct";
        // warp-bypass resolves to the real proxy outbound: when warp is on it's the
        // outbound warp detours into (tagged "warp-bypass"); when off it's just "proxy".
        preReqs->routingDeps->outboundMap[warpBypassID] = dataManager->settingsRepo->enable_warp ? "warp-bypass" : "proxy";
        int suffix = 0;
        auto isCustomFullConfig = [](const std::shared_ptr<Profile>& p) {
            return p->type == "custom" && p->Custom() != nullptr && p->Custom()->type == Custom::CustomFullConfig;
        };
        auto isXrayFullConfig = [](const std::shared_ptr<Profile>& p) {
            return p->outbound != nullptr && p->outbound->IsXrayFullConfig();
        };
        for (const auto &item: *neededOutbounds) {
            if (item < 0) continue;
            auto neededEnt = Configs::dataManager->profilesRepo->GetProfile(item);
            if (neededEnt == nullptr) {
                ctx->error = "The routing profile is referencing outbounds that no longer exist, consider revising your settings";
                return;
            }
            if ((neededEnt->outbound != nullptr && neededEnt->outbound->IsExtraCore()) || isCustomFullConfig(neededEnt) || isXrayFullConfig(neededEnt)) {
                ctx->error = "Outbounds used in routing profile cannot use an extra core or be a custom full config";
                return;
            }
            if (neededEnt->type == "chain") {
                auto chain = neededEnt->Chain();
                if (chain == nullptr || chain->list.isEmpty()) {
                    ctx->error = "Chain outbound in routing profile is empty or corrupted";
                    return;
                }
                // Validate each hop
                for (int hopID : chain->list) {
                    auto hopEnt = Configs::dataManager->profilesRepo->GetProfile(hopID);
                    if (hopEnt == nullptr) {
                        ctx->error = "Chain outbound in routing profile contains a missing profile";
                        return;
                    }
                    if ((hopEnt->outbound != nullptr && hopEnt->outbound->IsExtraCore()) || isCustomFullConfig(hopEnt) || isXrayFullConfig(hopEnt) || hopEnt->type == "chain") {
                        ctx->error = "Chain hops in routing profile cannot use an extra core, a custom full config, or be of type chain";
                        return;
                    }
                    // Collect domains for DNS direct rules
                    if (auto addrs = getEntDomains({hopID}, ctx->error); !addrs.empty()) {
                        if (!ctx->error.isEmpty()) return;
                        for (const auto &addr : addrs) preReqs->dnsDeps->directDomains << addr;
                        preReqs->dnsDeps->needDirectDnsRules = true;
                    }
                }
                // Map chain ID -> tag of the outermost (first-built) hop
                preReqs->routingDeps->outboundMap[item] = "route-" + Int2String(suffix);
                // Build reversed hop list (matching main-chain build order: outer first)
                QList<int> reversedHops;
                for (int idx = chain->list.size() - 1; idx >= 0; idx--) reversedHops << chain->list[idx];
                preReqs->routingDeps->routeOutboundGroups << RoutingDeps::RouteOutboundGroup{reversedHops, neededEnt};
                suffix += chain->list.size();
            } else {
                // Single-hop outbound (existing logic)
                if (auto entAddrs = getEntDomains({neededEnt->id}, ctx->error); !entAddrs.empty())
                {
                    if (!ctx->error.isEmpty()) return;
                    for (const auto &addr: entAddrs)
                    {
                        preReqs->dnsDeps->directDomains << addr;
                    }
                    preReqs->dnsDeps->needDirectDnsRules = true;
                }
                preReqs->routingDeps->outboundMap[item] = "route-" + Int2String(suffix++);
                preReqs->routingDeps->routeOutboundGroups << RoutingDeps::RouteOutboundGroup{QList<int>{item}, nullptr};
            }
        }

        for (const auto &item: *neededRuleSets) {
            preReqs->routingDeps->neededRuleSets << item;
        }

        // Direct domains
        if (dataManager->settingsRepo->enable_dns_routing) {
            auto sets = routeChain->get_direct_sites();
            for (const auto &item: sets) {
                if (item.startsWith("ruleset:")) {
                    preReqs->dnsDeps->directRuleSets << item.mid(8);
                }
                if (item.startsWith("domain:")) {
                    preReqs->dnsDeps->directDomains << item.mid(7);
                }
                if (item.startsWith("suffix:")) {
                    preReqs->dnsDeps->directSuffixes << item.mid(7);
                }
                if (item.startsWith("keyword:")) {
                    preReqs->dnsDeps->directKeywords << item.mid(8);
                }
                if (item.startsWith("regex:")) {
                    preReqs->dnsDeps->directRegexes << item.mid(6);
                }
                preReqs->dnsDeps->needDirectDnsRules = true;
            }

            // Proxy sites (symmetric to direct sites): when the final DNS is
            // direct these need an explicit remote-DNS carve-out, otherwise
            // they'd resolve via direct DNS.
            auto proxySets = routeChain->get_proxy_sites();
            for (const auto &item: proxySets) {
                if (item.startsWith("ruleset:")) {
                    preReqs->dnsDeps->proxyRuleSets << item.mid(8);
                }
                if (item.startsWith("domain:")) {
                    preReqs->dnsDeps->proxyDomains << item.mid(7);
                }
                if (item.startsWith("suffix:")) {
                    preReqs->dnsDeps->proxySuffixes << item.mid(7);
                }
                if (item.startsWith("keyword:")) {
                    preReqs->dnsDeps->proxyKeywords << item.mid(8);
                }
                if (item.startsWith("regex:")) {
                    preReqs->dnsDeps->proxyRegexes << item.mid(6);
                }
                preReqs->dnsDeps->needProxyDnsRules = true;
            }
        }
        if (auto entAddrs = getEntDomains({ctx->ent->id}, ctx->error); !entAddrs.isEmpty())
        {
            if (!ctx->error.isEmpty()) return;
            for (const auto &addr: entAddrs) preReqs->dnsDeps->directDomains << addr;
            preReqs->dnsDeps->needDirectDnsRules = true;
        }
        if (auto group = Configs::dataManager->groupsRepo->GetGroup(ctx->ent->gid); group != nullptr)
        {
            QList<int> groupEnts;
            if (auto frontEntID = group->front_proxy_id; frontEntID >= 0) groupEnts << frontEntID;
            if (auto landingEntID = group->landing_proxy_id; landingEntID >= 0) groupEnts << landingEntID;
            auto addrs = getEntDomains(groupEnts, ctx->error);
            if (!ctx->error.isEmpty()) return;
            for (const auto &addr: addrs)
            {
                preReqs->dnsDeps->directDomains << addr;
            }
        }

        // Hijack
        if (Configs::dataManager->settingsRepo->enable_dns_server) {
            for (const auto& rule : Configs::dataManager->settingsRepo->dns_server_rules) {
                if (rule.startsWith("ruleset:")) {
                    preReqs->hijackDeps->hijackGeoAssets << rule.mid(8);
                }
                if (rule.startsWith("domain:")) {
                    preReqs->hijackDeps->hijackDomains << rule.mid(7);
                }
                if (rule.startsWith("suffix:")) {
                    preReqs->hijackDeps->hijackDomainSuffix << rule.mid(7);
                }
                if (rule.startsWith("regex:")) {
                    preReqs->hijackDeps->hijackDomainRegex << rule.mid(6);
                }
            }
        }
        for (auto ruleSet : preReqs->hijackDeps->hijackGeoAssets) {
            if (!preReqs->routingDeps->neededRuleSets.contains(ruleSet.toString())) preReqs->routingDeps->neededRuleSets.append(ruleSet.toString());
        }

        // Direct IPs
        auto directIPraw = routeChain->get_direct_ips();
        for (const auto &item: directIPraw) {
            if (item.startsWith("ruleset:")) {
                preReqs->tunDeps->directIPSets << item.mid(8);
            }
            if (item.startsWith("ip:")) {
                preReqs->tunDeps->directIPCIDRs << item.mid(3);
            }
        }

        // Extra core (single ent OR final hop in a chain)
        std::shared_ptr<Profile> extraCoreEnt;
        if (ctx->ent->outbound != nullptr && ctx->ent->outbound->IsExtraCore())
        {
            extraCoreEnt = ctx->ent;
        }
        else if (ctx->ent->type == "chain")
        {
            auto chain = ctx->ent->Chain();
            if (chain != nullptr && !chain->list.isEmpty())
            {
                // Profiles using an extra core (when present in a chain) must
                // be at list[0], which becomes the outermost detour after the
                // chain reversal in buildOutboundsSection. entIDListtoEntList
                // enforces this; we just look it up here.
                auto firstEnt = Configs::dataManager->profilesRepo->GetProfile(chain->list[0]);
                if (firstEnt != nullptr && firstEnt->outbound != nullptr && firstEnt->outbound->IsExtraCore())
                {
                    extraCoreEnt = firstEnt;
                }
            }
        }
        if (extraCoreEnt != nullptr)
        {
            auto outbound = extraCoreEnt->ExtraCore();
            if (outbound == nullptr)
            {
                MW_show_log("INVALID ENT TYPE, NEEDED EXTRACORE GOT NULLPTR");
                ctx->error = "failed to cast to extracore, type is: " + extraCoreEnt->type;
                return;
            }
            ctx->buildConfigResult->extraCoreData->path = QFileInfo(outbound->extraCorePath).canonicalFilePath();
            ctx->buildConfigResult->extraCoreData->args = outbound->extraCoreArgs;
            ctx->buildConfigResult->extraCoreData->config = outbound->extraCoreConf;
            ctx->buildConfigResult->extraCoreData->noLog = outbound->noLogs;
        }
    }

    void buildLogSections(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        ctx->buildConfigResult->coreConfig.insert("log", QJsonObject{{"level", Configs::dataManager->settingsRepo->log_level}});
    }

    void buildNTPSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        if (Configs::dataManager->settingsRepo->enable_ntp) {
            QJsonObject ntpObj;
            ntpObj["enabled"] = true;
            ntpObj["server"] = Configs::dataManager->settingsRepo->ntp_server_address;
            ntpObj["server_port"] = Configs::dataManager->settingsRepo->ntp_server_port;
            ntpObj["interval"] = Configs::dataManager->settingsRepo->ntp_interval;
            ctx->buildConfigResult->coreConfig["ntp"] = ntpObj;
        }
    }

    void buildCertificateSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        ctx->buildConfigResult->coreConfig.insert("certificate", QJsonObject{{"store", Configs::dataManager->settingsRepo->use_mozilla_certs ? "mozilla" : "system"}});
    }

    QJsonObject buildDnsObj(QString address, std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        if (address.startsWith("local")) {
            if (ctx->tunEnabled && ctx->isResolvedUsed) {
                return {{"type", "underlying"}};
            }
            if (ctx->tunEnabled && ctx->os == Darwin) {
                return {
                    {"type", "udp"},
                    {"server", Configs::dataManager->settingsRepo->core_box_underlying_dns}
                };
            }
            return {{"type", "local"}};
        }
        if (address.startsWith("dhcp://")) {
            auto ifcName = address.replace("dhcp://", "");
            if (ifcName == "auto") ifcName = "";
            return {
                {"type", "dhcp"},
                {"interface", ifcName},
            };
        }
        QString addr = address;
        int port = -1;
        QString type = "udp";
        QString path = "";
        if (address.startsWith("tcp://")) {
            type = "tcp";
            addr = addr.replace("tcp://", "");
        }
        if (address.startsWith("tls://")) {
            type = "tls";
            addr = addr.replace("tls://", "");
        }
        if (address.startsWith("quic://")) {
            type = "quic";
            addr = addr.replace("quic://", "");
        }
        if (address.startsWith("https://")) {
            type = "https";
            addr = addr.replace("https://", "");
            auto slashIndex = addr.indexOf("/");
            if (slashIndex != -1) {
                path = addr.mid(slashIndex);
                addr = addr.left(slashIndex);
            }
        }
        if (address.startsWith("h3://")) {
            type = "h3";
            addr = addr.replace("h3://", "");
            auto slashIndex = addr.indexOf("/");
            if (slashIndex != -1) {
                path = addr.mid(slashIndex);
                addr = addr.left(slashIndex);
            }
        }
        if (addr.contains(":")) {
            auto spl = addr.split(":");
            addr = spl[0];
            port = spl[1].toInt();
        }
        QJsonObject res = {
            {"type", type},
            {"server", addr},
        };
        if (port != -1) res["server_port"] = port;
        if (!path.isEmpty()) res["path"] = path;
        return res;
    }

    void buildDNSSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx, bool useDnsObj) {
        if (getOS() == Darwin && Configs::dataManager->settingsRepo->core_box_underlying_dns.isEmpty() && Configs::dataManager->settingsRepo->spmode_vpn)
        {
            ctx->error = QObject::tr("Local DNS and Tun mode do not work together, please set an IP to be used as the Local DNS server in the Routing Settings -> Local override");
            return;
        }

        if (Configs::dataManager->settingsRepo->use_dns_object && useDnsObj) {
            ctx->buildConfigResult->coreConfig["dns"] = QString2QJsonObject(Configs::dataManager->settingsRepo->dns_object);
            return;
        }

        auto dnsDeps = ctx->buildPrerequisities->dnsDeps;
        auto hijackDeps = ctx->buildPrerequisities->hijackDeps;
        bool isTailscale = ctx->ent->type == "tailscale";
        bool independentCache = false;
        QJsonArray servers;
        QJsonArray rules;
        // remote
        if (!ctx->forTest) {
            auto remoteDnsObj = buildDnsObj(Configs::dataManager->settingsRepo->remote_dns, ctx);
            remoteDnsObj["tag"] = "dns-remote";
            remoteDnsObj["domain_resolver"] = "dns-local";
            remoteDnsObj["detour"] = "proxy";
            servers += remoteDnsObj;

            if (isTailscale)
            {
                auto tailscale = ctx->ent->Tailscale();
                if (tailscale != nullptr)
                {
                    // Add an additional DNS server for Tailscale MagicDNS
                    servers += QJsonObject{
                        {"type", "tailscale"},
                        {"tag", "dns-tailscale"},
                        {"endpoint", "proxy"},
                        {"accept_default_resolvers", tailscale->globalDNS},
                    };
                    
                    // Route Tailscale internal domains to MagicDNS
                    rules.prepend(QJsonObject{
                        {"domain_suffix", QJsonArray{"ts.net", "tailscale.net"}},
                        {"action", "route"},
                        {"server", "dns-tailscale"},
                    });
                }

                // Add direct bootstrap rules for tailscale control plane and services
                rules.prepend(QJsonObject{
                    {"domain", QJsonArray{
                        "controlplane.tailscale.com", 
                        "login.tailscale.com",
                        "log.tailscale.io"
                    }},
                    {"domain_suffix", QJsonArray{
                        "tailscale.com", 
                        "tailscale.net", 
                        "tailscale.io"
                    }},
                    {"action", "route"},
                    {"server", "dns-direct"},
                });
            }
        }

        // direct
        auto directDnsObj = buildDnsObj(Configs::dataManager->settingsRepo->direct_dns, ctx);
        directDnsObj["tag"] = "dns-direct";
        directDnsObj["domain_resolver"] = "dns-local";
        servers.append(directDnsObj);

        // Handle localhost
        if (!ctx->forTest) {
            rules += QJsonObject{
                    {"domain", "localhost"},
                    {"action", "predefined"},
                    {"query_type", "A"},
                    {"rcode", "NOERROR"},
                    {"answer", "localhost. IN A 127.0.0.1"},
                };

            rules += QJsonObject{
                    {"domain", "localhost"},
                    {"action", "predefined"},
                    {"query_type", "AAAA"},
                    {"rcode", "NXDOMAIN"},
                };
        }

        // process_path matching forces sing-box's process finder, which is very
        // heavy on Windows (large latency spikes). It's only needed to keep an
        // extra core's egress out of the proxy/TUN loop — sing-box's own egress
        // is handled by auto_detect_interface and xray by its loopback bridges —
        // so only emit the direct-DNS carve-out when an extra core is present.
        if (!ctx->forTest && !ctx->buildConfigResult->extraCoreData->path.isEmpty())
        {
            QJsonArray coreProcessPaths;
            auto extraCorePath = ctx->buildConfigResult->extraCoreData->path;
#ifdef Q_OS_WIN
            extraCorePath.replace("/", "\\");
#endif
            coreProcessPaths.append(extraCorePath);
            rules += QJsonObject{
                {"process_path", coreProcessPaths},
                {"action", "route"},
                {"strategy", dataManager->settingsRepo->direct_dns_strategy},
                {"server", "dns-direct"},
            };
        }

        // HijackRules
        if (Configs::dataManager->settingsRepo->enable_dns_server && !ctx->forTest)
        {
            rules += QJsonObject{
                        {"rule_set", hijackDeps->hijackGeoAssets},
                        {"domain", hijackDeps->hijackDomains},
                        {"domain_suffix", hijackDeps->hijackDomainSuffix},
                        {"domain_regex", hijackDeps->hijackDomainRegex},
                        {"query_type", "A"},
                        {"action", "predefined"},
                        {"rcode", "NOERROR"},
                        {"answer", QString("* IN A %1").arg(Configs::dataManager->settingsRepo->dns_v4_resp)},
                    };

            if (!Configs::dataManager->settingsRepo->dns_v6_resp.isEmpty())
            {
                rules += QJsonObject{
                            {"rule_set", hijackDeps->hijackGeoAssets},
                            {"domain", hijackDeps->hijackDomains},
                            {"domain_suffix", hijackDeps->hijackDomainSuffix},
                            {"domain_regex", hijackDeps->hijackDomainRegex},
                            {"query_type", "AAAA"},
                            {"action", "predefined"},
                            {"rcode", "NOERROR"},
                            {"answer", QString("* IN AAAA %1").arg(Configs::dataManager->settingsRepo->dns_v6_resp)},
                        };
            }
        }

        // FakeIP
        if (Configs::dataManager->settingsRepo->fake_dns) {
            servers += QJsonObject{
                    {"tag", "dns-fake"},
                    {"type", "fakeip"},
                    {"inet4_range", "198.18.0.0/15"},
                    {"inet6_range", "fc00::/18"},
                };
            rules += QJsonObject{
                    {"query_type", QJsonArray{
                        "A",
                        "AAAA"
                    }},
                 {"action", "route"},
                 {"server", "dns-fake"}
            };
            independentCache = true;
        }

        if (dnsDeps->needDirectDnsRules) {
            rules += QJsonObject{
                    {"rule_set", dnsDeps->directRuleSets},
                    {"domain", dnsDeps->directDomains},
                    {"domain_suffix", dnsDeps->directSuffixes},
                    {"domain_keyword", dnsDeps->directKeywords},
                    {"domain_regex", dnsDeps->directRegexes},
                    {"action", "route"},
                    {"strategy", dataManager->settingsRepo->direct_dns_strategy},
                    {"server", "dns-direct"},
                };
        }

        const bool useDirectFinalDNS = dataManager->settingsRepo->dns_final_out == "direct";

        // Symmetric to the direct carve-out above: when the final DNS is direct,
        // proxy-routed sites would otherwise resolve via direct DNS, so route
        // them to remote DNS. "proxy" and "remote" both use remote DNS; only
        // explicit "direct" should fall back to local DNS.
        if (dnsDeps->needProxyDnsRules && useDirectFinalDNS) {
            rules += QJsonObject{
                    {"rule_set", dnsDeps->proxyRuleSets},
                    {"domain", dnsDeps->proxyDomains},
                    {"domain_suffix", dnsDeps->proxySuffixes},
                    {"domain_keyword", dnsDeps->proxyKeywords},
                    {"domain_regex", dnsDeps->proxyRegexes},
                    {"action", "route"},
                    {"strategy", dataManager->settingsRepo->remote_dns_strategy},
                    {"server", "dns-remote"},
                };
        }

        // final rule: proxy
        auto finalStrategy = useDirectFinalDNS ? dataManager->settingsRepo->direct_dns_strategy : dataManager->settingsRepo->remote_dns_strategy;
        auto finalDNS = useDirectFinalDNS ? "dns-direct" : "dns-remote";
        rules += QJsonObject{
            {"strategy", finalStrategy},
            {"action", "route"},
            {"server", finalDNS},
        };

        // Local
        auto dnsLocalAddress = Configs::dataManager->settingsRepo->core_box_underlying_dns.isEmpty() ? "local" : Configs::dataManager->settingsRepo->core_box_underlying_dns;
        auto dnsLocalObj = buildDnsObj(dnsLocalAddress, ctx);
        dnsLocalObj["tag"] = "dns-local";
        servers += dnsLocalObj;

        auto dnsObj = QJsonObject{
            {"servers", servers},
            {"rules", rules},
            {"cache_capacity", dataManager->settingsRepo->dns_cache_capacity},
        };
        if (dataManager->settingsRepo->dns_disable_cache) dnsObj["disable_cache"] = true;
        if (dataManager->settingsRepo->dns_disable_expire) dnsObj["disable_expire"] = true;
        if (dataManager->settingsRepo->dns_reverse_mapping) dnsObj["reverse_mapping"] = true;
        if (independentCache) dnsObj["independent_cache"] = true;
        ctx->buildConfigResult->coreConfig["dns"] = dnsObj;
    }

    void buildInboundSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        if (ctx->forTest) return;
        auto tunDeps = ctx->buildPrerequisities->tunDeps;
        QJsonArray inbounds;

        // mixed
        QJsonObject inboundObj;

        if (!Configs::dataManager->settingsRepo->disable_mixed_inbound) {
            inboundObj["tag"] = "mixed-in";
            inboundObj["type"] = "mixed";
            inboundObj["listen"] = Configs::dataManager->settingsRepo->inbound_address;
            inboundObj["listen_port"] = Configs::dataManager->settingsRepo->inbound_socks_port;
            if (Configs::dataManager->settingsRepo->inbound_auth) {
                inboundObj["users"] = QJsonArray{
                    QJsonObject{
                                            {"username", Configs::dataManager->settingsRepo->inbound_user},
                                            {"password", Configs::dataManager->settingsRepo->inbound_pass}
                    }
                };
            }
            inbounds += inboundObj;
        }

        // Tun
        if (Configs::dataManager->settingsRepo->spmode_vpn) {
            QJsonObject inboundObj;
            inboundObj["tag"] = "tun-in";
            inboundObj["type"] = "tun";
            inboundObj["interface_name"] = genTunName();
            inboundObj["auto_route"] = true;
            inboundObj["mtu"] = Configs::dataManager->settingsRepo->vpn_mtu;
            inboundObj["stack"] = Configs::dataManager->settingsRepo->vpn_implementation;
            inboundObj["strict_route"] = Configs::dataManager->settingsRepo->vpn_strict_route;
            const auto tunIPv4CIDR = Configs::dataManager->settingsRepo->vpn_tun_ipv4_cidr;
            const auto tunIPv6CIDR = Configs::dataManager->settingsRepo->vpn_tun_ipv6_cidr;
            ctx->buildConfigResult->tunIPv4CIDR = tunIPv4CIDR;
            auto tunAddress = QJsonArray{tunIPv4CIDR};
            if (Configs::dataManager->settingsRepo->vpn_ipv6) tunAddress += tunIPv6CIDR;
            inboundObj["address"] = tunAddress;

            QJsonArray routeExcludeAddrs;
            if (!Configs::dataManager->settingsRepo->disable_private_range_bypass) routeExcludeAddrs = {
                "127.0.0.0/8", "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "169.254.0.0/16", "224.0.0.0/4",
                "255.255.255.255/32"
            };
            QJsonArray routeExcludeSets;
            if (Configs::dataManager->settingsRepo->enable_tun_routing)
            {
                for (auto item: tunDeps->directIPCIDRs) routeExcludeAddrs << item;
                for (auto item: tunDeps->directIPSets) routeExcludeSets << item;
            }
            inboundObj["route_exclude_address"] = routeExcludeAddrs;
            if (!routeExcludeSets.isEmpty()) inboundObj["route_exclude_address_set"] = routeExcludeSets;
            inbounds += inboundObj;
        }

        // dns-in
        inbounds.prepend(QJsonObject{
            {"tag", "dns-in"},
            {"type", "direct"},
            {"listen", "127.0.0.1"},
            {"listen_port", dataManager->settingsRepo->core_dns_in_port}
        });

        // Hijack
        if (Configs::dataManager->settingsRepo->enable_redirect) {
            inbounds.prepend(QJsonObject{
                {"tag", "hijack"},
                {"type", "direct"},
                {"listen", Configs::dataManager->settingsRepo->redirect_listen_address},
                {"listen_port", Configs::dataManager->settingsRepo->redirect_listen_port},
            });
        }
        if (Configs::dataManager->settingsRepo->enable_dns_server) {
            inbounds.prepend(QJsonObject{
                {"tag", "hijack-dns"},
                {"type", "direct"},
                {"listen", Configs::dataManager->settingsRepo->dns_server_listen_lan ? "0.0.0.0" : "127.1.1.1"},
                {"listen_port", Configs::dataManager->settingsRepo->dns_server_listen_port},
            });
        }

        // custom
        QJSONARRAY_ADD(inbounds, QString2QJsonObject(Configs::dataManager->settingsRepo->custom_inbound)["inbounds"].toArray())
        ctx->buildConfigResult->coreConfig["inbounds"] = inbounds;
    }

    void entIDListtoEntList(std::shared_ptr<BuildSingBoxConfigContext> &ctx, const QList<int>& entIDs, QList<std::shared_ptr<Profile>> &ents, QString& error)
    {
        int extracoreCount = 0;
        int extracoreIdx = -1;
        int xrayFullConfigCount = 0;
        int xrayFullConfigIdx = -1;
        int xrayHopCount = 0;
        bool hasCustomFullConfig = false;
        int coreTransitions = 0;
        bool inXray = false;
        for (auto id : entIDs)
        {
            if (id == warpProfileID) {
                if (inXray) {
                    ctx->xrayToSingTransitioned = true;
                    coreTransitions++;
                }
                inXray = false;
                ents.append(getWarpProfile());
                continue;
            }
            auto ent = Configs::dataManager->profilesRepo->GetProfile(id);
            if (ent == nullptr)
            {
                error = "Null proxy in chain, you may want to check your configs";
                return;
            }
            if (!inXray && ent->outbound->IsXray()) {
                ctx->singToXrayTransitioned = true;
                coreTransitions++;
            }
            if (inXray && !ent->outbound->IsXray()) {
                ctx->xrayToSingTransitioned = true;
                coreTransitions++;
            }
            inXray = ent->outbound->IsXray();
            if (ent->type == "chain")
            {
                error = "Chain in Chain is not allowed";
                return;
            }
            if (ent->outbound != nullptr && ent->outbound->IsExtraCore()) {
                extracoreCount++;
                extracoreIdx = ents.size();
            }
            if (ent->outbound != nullptr && ent->outbound->IsXrayFullConfig()) {
                xrayFullConfigCount++;
                xrayFullConfigIdx = ents.size();
            }
            if (ent->outbound != nullptr && ent->outbound->IsXray()) {
                xrayHopCount++;
            }
            if (ent->type == "custom" && ent->Custom() != nullptr && ent->Custom()->type == Custom::CustomFullConfig) hasCustomFullConfig = true;
            ents.append(ent);
        }
        if (hasCustomFullConfig)
        {
            error = "Custom full config profiles cannot be used in a chain; only custom outbound profiles are chainable";
            return;
        }
        if (extracoreCount > 1)
        {
            error = "Only one extra-core profile is allowed in a chain";
            return;
        }
        if (xrayFullConfigCount > 1)
        {
            error = "Only one custom Xray full config profile is allowed in a chain";
            return;
        }
        if (extracoreCount > 0 && xrayFullConfigCount > 0)
        {
            error = "Extra-core and custom Xray full config profiles cannot be combined in a chain";
            return;
        }
        if (xrayFullConfigCount > 0 && xrayHopCount > 0)
        {
            error = "Custom Xray full config cannot be combined with other Xray hops in a chain (only one Xray instance is supported at a time)";
            return;
        }
        // A profile that uses an extra core must occupy the deepest detour
        // slot (ents.last()) so its local socks server (127.0.0.1) is dialed
        // directly. After this hop sing-box hands off to the extra core
        // process and does no more hops.
        if (extracoreCount == 1 && ents.size() > 1 && extracoreIdx != ents.size() - 1)
        {
            error = "Extra-core profiles can only be the final hop in a chain (top of the chain editor)";
            return;
        }
        // Same constraint for custom Xray full config: traffic exits through
        // its sing-box socks bridge, then user's Xray (running their full
        // config) takes over.
        if (xrayFullConfigCount == 1 && ents.size() > 1 && xrayFullConfigIdx != ents.size() - 1)
        {
            error = "Custom Xray full config can only be the final hop in a chain (top of the chain editor)";
            return;
        }
        if (coreTransitions > 2) {
            error = "Too many core transitions, the valid sequence is: (optional sing-box chain)->(optional xray chain)->(optional sing-box chain)";
        }
    }

    QList<int> unwrapChain(int entID) {
        auto ent = Configs::dataManager->profilesRepo->GetProfile(entID);
        if (ent == nullptr)
        {
            return {};
        }
        if (ent->type == "chain") {
            auto chain = ent->Chain();
            if (chain == nullptr) return {};
            // Reverse to match the main chain build order: chain.list[0] is
            // the outermost detour (egress, per the convention in
            // edit_chain.cpp) and must end up LAST in the build sequence so
            // it becomes the deepest hop with no further detour. Without
            // this, test configs run the chain inverted vs. the live config
            // and the measured latency reflects a topology the user never
            // deployed.
            QList<int> reversed;
            for (int idx = chain->list.size() - 1; idx >= 0; idx--) reversed.append(chain->list[idx]);
            return reversed;
        }
        return {entID};
    }

    void buildSingboxChain(std::shared_ptr<BuildSingBoxConfigContext> &ctx, QList<std::shared_ptr<Profile>> &ents, const QString& prefix, bool includeProxy, bool link, int startSuffix = 0, bool markIngress = false, bool warpWrap = false) {
        for (int idx = 0; idx < ents.size(); idx++)
        {
            auto tag = prefix + "-" + Int2String(startSuffix + idx);
            QString nextTag;
            if (idx < ents.size() - 1) nextTag = prefix + "-" + Int2String(startSuffix + idx + 1);
            if (includeProxy && idx == 0) tag = "proxy";
            // warp wrapping: idx 0 is warp (tag "proxy") and idx 1 is the outbound it
            // detours into. Expose that outbound under the stable tag "warp-bypass" so
            // rules / final can reach the real proxy without the warp layer.
            if (warpWrap && idx == 1) tag = "warp-bypass";
            if (markIngress && idx == 0) ctx->singIngressTags << tag;
            const auto& ent = ents[idx];
            auto [object, error] = ent->outbound->Build();
            if (!error.isEmpty())
            {
                ctx->error += error;
                return;
            }
            object["tag"] = tag;
            if (!nextTag.isEmpty() && link) object["detour"] = nextTag;
            if (warpWrap && idx == 0) object["detour"] = "warp-bypass";
            if (ent->outbound->IsEndpoint())
            {
                ctx->endpoints.append(object);
            } else
            {
                ctx->outbounds.append(object);
            }
        }
    }

    void buildXrayChain(std::shared_ptr<BuildSingBoxConfigContext> &ctx, QList<std::shared_ptr<Profile>> &ents, const QString& prefix, bool includeProxy, bool link, int startSuffix = 0, coreBridgeConfig bridgeConfig = {}) {
        for (int idx = 0; idx < ents.size(); idx++)
        {
            auto tag = prefix + "-" + Int2String(startSuffix + idx);
            QString nextTag;
            if (idx < ents.size() - 1 || bridgeConfig.needed) nextTag = prefix + "-" + Int2String(startSuffix + idx + 1);
            if (includeProxy && idx == 0) tag = "proxy";
            if (idx == 0) ctx->xrayIngressTags << tag;
            const auto& ent = ents[idx];
            auto [object, error] = ent->outbound->BuildXray();
            if (!error.isEmpty())
            {
                ctx->error += error;
                return;
            }
            object["tag"] = tag;
            if (bridgeConfig.loopbackProtect) {
                auto streamSettings = object["streamSettings"].toObject();
                const auto network = streamSettings["network"].toString();
                if (network == "xhttp" || network == "splithttp") {
                    auto xhttpSettings = streamSettings["xhttpSettings"].toObject();
                    if (xhttpSettings.isEmpty()) xhttpSettings = streamSettings["splithttpSettings"].toObject();
                    auto extraSettings = xhttpSettings["extra"].toObject();
                    if (extraSettings.contains("downloadSettings")) {
                        // XHTTP downloadSettings gets its own stream config;
                        // penetrate makes Xray copy this sockopt there too.
                        auto sockopt = streamSettings["sockopt"].toObject();
                        sockopt["penetrate"] = true;
                        streamSettings["sockopt"] = sockopt;
                        object["streamSettings"] = streamSettings;
                    }
                }
            }
            if (!bridgeConfig.bindInterface.isEmpty() && idx == ents.size() - 1) {
                // Final egress hop under TUN: bind its physical socket to the
                // default interface so it doesn't re-enter TUN. Inner hops dial
                // through this one via proxySettings and open no socket of their
                // own, so only the last hop needs the sockopt.
                auto streamSettings = object["streamSettings"].toObject();
                auto sockopt = streamSettings["sockopt"].toObject();
                sockopt["interface"] = bridgeConfig.bindInterface;
                const auto network = streamSettings["network"].toString();
                if (network == "xhttp" || network == "splithttp") {
                    auto xhttpSettings = streamSettings["xhttpSettings"].toObject();
                    if (xhttpSettings.isEmpty()) xhttpSettings = streamSettings["splithttpSettings"].toObject();
                    if (xhttpSettings["extra"].toObject().contains("downloadSettings")) {
                        // XHTTP's download leg gets its own stream config;
                        // penetrate makes Xray copy this sockopt (including the
                        // interface bind) onto it too.
                        sockopt["penetrate"] = true;
                    }
                }
                streamSettings["sockopt"] = sockopt;
                object["streamSettings"] = streamSettings;
            }
            // A bridge always requires chaining the preceding hop into it,
            // even when `link` is false (single-hop route groups). nextTag
            // already encodes whether anything follows, so honoring it
            // unconditionally when a bridge is needed wires the loopback
            // protection up for single-hop xray route outbounds too.
            if (!nextTag.isEmpty() && (link || bridgeConfig.needed)) object["proxySettings"] = QJsonObject{
                {"tag", nextTag},
                {"transportLayer", true}
            };
            ctx->xrayOutbounds.append(object);
        }
        if (bridgeConfig.needed) {
            QJsonObject socksSettings = {
                {"address", bridgeConfig.host},
                {"port", bridgeConfig.port},
                {"user", bridgeConfig.auth},
                {"pass", bridgeConfig.auth},
            };
            QJsonObject socksOutbound = {
                {"tag", prefix + "-" + Int2String(startSuffix + ents.size())},
                {"protocol", "socks"},
                {"settings", socksSettings}
            };
            ctx->xrayOutbounds.append(socksOutbound);
        }
    }

    void buildOutboundChain(std::shared_ptr<BuildSingBoxConfigContext> &ctx, const QList<int>& entIDs, const QString& prefix, bool includeProxy, bool link, int singToXrayPort = -1, int xrayToSingPort = -1, int startSuffix = 0, bool warpWrap = false)
    {
        // Core-transition flags are per-chain: entIDListtoEntList only ever
        // sets them true, so clear any value left by a previous chain in this
        // context before evaluating this one.
        ctx->singToXrayTransitioned = false;
        ctx->xrayToSingTransitioned = false;
        QList<std::shared_ptr<Profile>> ents;
        entIDListtoEntList(ctx, entIDs, ents, ctx->error);
        if (!ctx->error.isEmpty()) return;

        // If the deepest hop is a custom Xray full config, allocate a bridge
        // port the sing-box chain dials into and inject a matching socks
        // inbound into the user's Xray config. The user's Xray runs as our
        // Xray instance, so its full config becomes ctx->xrayConfig directly.
        if (!ents.isEmpty() && ents.last()->outbound != nullptr && ents.last()->outbound->IsXrayFullConfig()) {
            auto custom = ents.last()->Custom();
            if (custom == nullptr) {
                ctx->error = "Failed to cast to Custom for Xray full config hop";
                return;
            }
            auto userXrayConfig = QString2QJsonObject(custom->config);
            if (userXrayConfig.isEmpty()) {
                ctx->error = "Custom Xray full config is not valid JSON";
                return;
            }
            auto bridgePorts = MkManyPorts(1);
            custom->bridgePort = bridgePorts[0];
            custom->bridgeAuth = GetRandomString(32);
            custom->bridgeHost = GenRandomLoopback();

            auto inbounds = userXrayConfig["inbounds"].toArray();
            inbounds.prepend(QJsonObject{
                {"tag", "throne-bridge"},
                {"listen", custom->bridgeHost},
                {"port", custom->bridgePort},
                {"protocol", "socks"},
                {"settings", QJsonObject{
                    {"auth", "password"},
                    {"udp", true},
                    {"accounts", QJsonArray{
                        QJsonObject{
                            {"user", custom->bridgeAuth},
                            {"pass", custom->bridgeAuth}
                        }
                    }}
                }}
            });
            userXrayConfig["inbounds"] = inbounds;
            ctx->buildConfigResult->xrayConfig = userXrayConfig;
            ctx->buildConfigResult->isXrayNeeded = true;
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
        auto ports = MkManyPorts(2);
        if (ctx->singToXrayTransitioned) {
            coreBridgeConfig singToXrayBridgeConf = {
                true, singToXrayPort == -1 ? ports[0] : singToXrayPort, GetRandomString(32), false, GenRandomLoopback()
            };
            ctx->singToXrayBridges << singToXrayBridgeConf;
            auto bridgeEnt = ProfilesRepo::NewProfile("socks");
            auto socksOutbound = bridgeEnt->Socks();
            socksOutbound->username = singToXrayBridgeConf.auth;
            socksOutbound->password = singToXrayBridgeConf.auth;
            socksOutbound->server = singToXrayBridgeConf.host;
            socksOutbound->server_port = singToXrayBridgeConf.port;
            initialSingEnts << bridgeEnt;
        }
        // Xray-final-egress under TUN: detour xray's egress back through a
        // sing-box socks inbound that routes to `direct`. Without this, when
        // xray (running in the same process as sing-box) dials the internet
        // its packets re-enter TUN — sing-box's process_path rule should
        // short-circuit them to direct but in practice doesn't always match,
        // producing a TUN -> xray -> TUN loop. Sing-box's direct outbound
        // honors auto_detect_interface, so handing xray's egress to it bypasses
        // TUN cleanly. Mutually exclusive with xrayToSingTransitioned (which
        // implies a tailing sing-box hop already exists).
        // Chain ends in xray with no following sing-box hop. Its egress socket
        // would otherwise follow the OS default route — which is a TUN whenever
        // one is up: this profile's own TUN, or another profile already running
        // while we build a test config. Either way we want it on the physical NIC.
        bool xrayFinalEgress = !xrayEnts.isEmpty() && tailingSingEnts.isEmpty() && !ctx->xrayToSingTransitioned;
        coreBridgeConfig xrayToSingBridgeConf;
        if (ctx->xrayToSingTransitioned) {
            xrayToSingBridgeConf = {true, xrayToSingPort == -1 ? ports[1] : xrayToSingPort, GetRandomString(32), false, GenRandomLoopback()};
            ctx->xrayToSingBridges << xrayToSingBridgeConf;
        } else if (xrayFinalEgress && !ctx->defaultInterface.isEmpty()) {
            // Preferred: bind the egress straight to the physical default
            // interface (streamSettings.sockopt.interface) — no socks loopback
            // hop. Covers the main config ending in xray under our own TUN, and a
            // test build while a TUN profile is running (so the test isn't
            // captured by the live profile, mirroring auto_detect_interface on
            // the sing-box side). No bridge inbound / route rule / xray-direct
            // outbound is emitted for this chain.
            xrayToSingBridgeConf = {};
            xrayToSingBridgeConf.bindInterface = ctx->defaultInterface;
            MW_show_log("[interface-bind] " + prefix + " xray egress bound to default interface " + ctx->defaultInterface);
            // Only a real running profile is watched for interface changes; tests
            // are short-lived and use a separate result object.
            if (!ctx->forTest) ctx->buildConfigResult->boundInterface = ctx->defaultInterface;
        } else if (xrayFinalEgress && ctx->tunEnabled) {
            // Fallback for the main config under our own TUN when no interface
            // was resolved: detour xray's egress through a sing-box socks inbound
            // that routes to `direct` (auto_detect_interface bypasses TUN).
            xrayToSingBridgeConf = {true, xrayToSingPort == -1 ? ports[1] : xrayToSingPort, GetRandomString(32), true, GenRandomLoopback()};
            ctx->xrayToSingBridges << xrayToSingBridgeConf;
        }
        if (!initialSingEnts.isEmpty()) {
            buildSingboxChain(ctx, initialSingEnts, prefix, includeProxy, link, startSuffix, false, warpWrap);
        }
        if (!xrayEnts.isEmpty()) {
            buildXrayChain(ctx, xrayEnts, prefix, includeProxy, link, startSuffix, xrayToSingBridgeConf);
        }
        if (!tailingSingEnts.isEmpty()) {
            buildSingboxChain(ctx, tailingSingEnts, prefix, false, link, startSuffix + initialSingEnts.size(), true);
        }

        // Traffic group: watchTag is the matched outbound of the last routing
        // rule that points into this chain on the sing-box side. With no xray
        // re-entry that's the first hop of initialSingEnts; with an xray->sing
        // re-entry (interlocking [sing,xray,sing] pattern) the bridge inbound's
        // route rule sends traffic to the first hop of tailingSingEnts and that
        // becomes the egress-side watch point. profiles is the original user
        // chain — synthetic socks bridges are appended to initialSingEnts above
        // but never enter `ents`, so they're naturally excluded.
        if (!ents.isEmpty()) {
            TrafficChainGroup group;
            group.profiles = ents;
            if (!tailingSingEnts.isEmpty()) {
                group.watchTag = prefix + "-" + Int2String(startSuffix + initialSingEnts.size());
            } else if (includeProxy) {
                group.watchTag = "proxy";
            } else {
                group.watchTag = prefix + "-" + Int2String(startSuffix);
            }
            ctx->buildConfigResult->chainGroups.append(group);
        }
    }

    void buildOutboundsSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        // First, our own ent
        QList<int> entIDs;
        auto group = Configs::dataManager->groupsRepo->GetGroup(ctx->ent->gid);
        if (group == nullptr)
        {
            ctx->error = "No group found for ent, data is corrupted";
            return;
        }
        if (group->landing_proxy_id >= 0) entIDs.prepend(group->landing_proxy_id);
        if (ctx->ent->type == "chain")
        {
            auto chain = ctx->ent->Chain();
            if (chain == nullptr)
            {
                ctx->error = "Ent is nullptr after cast to chain, data is corrupted";
                return;
            }
            for (int idx = chain->list.size()-1; idx >=0; idx--) entIDs.append(chain->list[idx]);
        } else
        {
            entIDs.append(ctx->ent->id);
        }
        if (group->front_proxy_id >= 0) entIDs.append(group->front_proxy_id);
        const bool warpWrap = dataManager->settingsRepo->enable_warp;
        if (warpWrap) {
            entIDs.prepend(warpProfileID);
        }
        buildOutboundChain(ctx, entIDs, "config", true, true, -1, -1, 0, warpWrap);

        // A chain-typed profile wrapper isn't in entIDs (only its hops are),
        // so the chainGroup just built doesn't include it. Add it so the
        // chain's row in the proxy list also accumulates traffic.
        if (ctx->ent->type == "chain" && !ctx->buildConfigResult->chainGroups.isEmpty()) {
            ctx->buildConfigResult->chainGroups.last().profiles.append(ctx->ent);
        }

        // Now, build the outbounds needed by the route profile
        int routeSuffix = 0;
        for (const auto& routeGroup : ctx->buildPrerequisities->routingDeps->routeOutboundGroups) {
            bool linked = routeGroup.hopIDs.size() > 1;
            buildOutboundChain(ctx, routeGroup.hopIDs, "route", false, linked, -1, -1, routeSuffix);
            // Same as main chain: credit the chain wrapper if the route rule's
            // referenced outbound was a chain.
            if (routeGroup.chainWrapper != nullptr && !ctx->buildConfigResult->chainGroups.isEmpty()) {
                ctx->buildConfigResult->chainGroups.last().profiles.append(routeGroup.chainWrapper);
            }
            routeSuffix += routeGroup.hopIDs.size();
        }

        // Also add the needed socks inbound bridges. Loopback-protect bridges
        // have no sing-box ingress to pair with (their inbound routes straight
        // to the hidden `xray-direct`), so the singIngressTags index only
        // advances for normal xray->sing bridges.
        int loopbackBridgeCount = 0;
        for (const auto& b : ctx->xrayToSingBridges) if (b.loopbackProtect) loopbackBridgeCount++;
        if (ctx->xrayToSingBridges.size() - loopbackBridgeCount != ctx->singIngressTags.size()) {
            ctx->error = "xray to sing-box bridges count does not match ingress tags count";
            return;
        }
        QJsonArray inboundArr;
        if (ctx->buildConfigResult->coreConfig.contains("inbounds")) {
            inboundArr = ctx->buildConfigResult->coreConfig["inbounds"].toArray();
        }
        int singIngressIdx = 0;
        for (auto idx=0;idx<ctx->xrayToSingBridges.size();idx++) {
            auto bridgeConf = ctx->xrayToSingBridges[idx];
            QString bridgeTag = bridgeConf.loopbackProtect
                ? QString("bridge-loopback-") + Int2String(bridgeConf.port)
                : QString("bridge-") + ctx->singIngressTags[singIngressIdx++];
            QJsonObject userObj = {
                {"username", bridgeConf.auth},
                {"password", bridgeConf.auth}
            };
            QJsonObject socksBridge = {
                {"type", "socks"},
                {"tag", bridgeTag},
                {"listen", bridgeConf.host},
                {"listen_port", bridgeConf.port},
                {"users", QJsonArray{userObj}}
            };
            inboundArr.append(socksBridge);
        }
        ctx->buildConfigResult->coreConfig["inbounds"] = inboundArr;

        if (loopbackBridgeCount > 0) {
            ctx->outbounds.append(QJsonObject{
                {"type", "direct"},
                {"tag", "xray-direct"}
            });
        }

        // Add the direct outbound
        ctx->outbounds.append(QJsonObject{
        {"type", "direct"},
        {"tag", "direct"}
        });

        ctx->buildConfigResult->coreConfig["endpoints"] = ctx->endpoints;
        ctx->buildConfigResult->coreConfig["outbounds"] = ctx->outbounds;
    }

    void buildRouteSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        auto routeChain = Configs::dataManager->routesRepo->GetRouteProfile(Configs::dataManager->settingsRepo->current_route_id);
        if (routeChain == nullptr) {
            ctx->error = "Routing profile does not exist, try resetting the route profile in Routing Settings";
            return;
        }
        routeChain = std::make_shared<RouteProfile>(*routeChain);
        auto routeDeps = ctx->buildPrerequisities->routingDeps;

        // Raw routing profile: translate the user's outbound ids -> sing-box tags up front
        // (always, both modes). In "prevent modifications" mode we emit it verbatim and stop;
        // otherwise we fall through so the shared plumbing below wraps the user's rules.
        QJsonObject rawRouteObj;
        if (routeChain->isRaw) {
            rawRouteObj = QString2QJsonObject(routeChain->rawRoute);
            if (rawRouteObj.isEmpty()) {
                ctx->error = "Raw routing profile is not a valid JSON object";
                return;
            }
            rawRouteObj = RouteProfile::TranslateRawOutbounds(rawRouteObj, routeDeps->outboundMap);
            if (routeChain->preventModifications) {
                ctx->buildConfigResult->coreConfig["route"] = rawRouteObj;
                return;
            }
        }

        struct InjectedRules {
            QJsonObject sniff;
            QJsonObject resolve;
            QJsonObject dnsHijack;
            QJsonObject dnsInReject;
            QJsonObject redirectSniff;
        } injected;

        if (!routeChain->isRaw) {
            injected.sniff = QJsonObject{{"action", "sniff"}};
            if (!Configs::dataManager->settingsRepo->resolve_domain_strategy.isEmpty()) {
                injected.resolve = QJsonObject{
                    {"inbound", QJsonArray{"mixed-in", "tun-in"}},
                    {"action", "resolve"},
                    {"strategy", Configs::dataManager->settingsRepo->resolve_domain_strategy},
                };
            }
            injected.dnsHijack = QJsonObject{
                {"protocol", "dns"},
                {"action", "hijack-dns"},
            };
            if (Configs::dataManager->settingsRepo->enable_redirect && !ctx->forTest) {
                injected.redirectSniff = QJsonObject{
                    {"inbound", QJsonArray{"hijack"}},
                    {"action", "sniff"},
                    {"override_destination", true},
                };
            }
        }
        if (!ctx->forTest) {
            injected.dnsInReject = QJsonObject{
                {"inbound", "dns-in"},
                {"action", "reject"},
            };
        }

        auto profileRules = routeChain->isRaw ? rawRouteObj.value("rules").toArray()
                                              : routeChain->get_route_rules(false, routeDeps->outboundMap);

        QJsonObject extraCoreDirect;
        if (!ctx->buildConfigResult->extraCoreData->path.isEmpty())
        {
            QJsonArray coreProcessPaths;
            auto extraCorePath = ctx->buildConfigResult->extraCoreData->path;
#ifdef Q_OS_WIN
            extraCorePath.replace("/", "\\");
#endif
            coreProcessPaths.append(extraCorePath);
            extraCoreDirect = QJsonObject{
                {"action", "route"},
                {"process_path", coreProcessPaths},
                {"outbound", "direct"},
            };
        }

        // rulesets
        auto ruleSetArray = QJsonArray();
        for (const auto &item: routeDeps->neededRuleSets) {
            if(auto url = QUrl(item); url.isValid() && url.fileName().contains(".srs")) {
                ruleSetArray += QJsonObject{
                            {"type", "remote"},
                            {"tag", get_rule_set_name(item)},
                            {"format", "binary"},
                            {"url", item},
                        };
            }
            else
                if (auto url = ruleSetUrl(item.toStdString()); !url.empty()) {
                    ruleSetArray += QJsonObject{
                                {"type", "remote"},
                                {"tag", item},
                                {"format", "binary"},
                                {"url", get_jsdelivr_link(QString::fromUtf8(url.data(), url.size()))},
                            };
                }
        }

        // add block
        if (Configs::dataManager->settingsRepo->adblock_enable) {
            ruleSetArray += QJsonObject{
                        {"type", "remote"},
                        {"tag", "throne-adblocksingbox"},
                        {"format", "binary"},
                        {"url", get_jsdelivr_link("https://raw.githubusercontent.com/217heidai/adblockfilters/main/rules/adblocksingbox.srs")},
                    };
        }

        // map ingress socks inbounds to their corresponding outbounds.
        // Loopback-protect bridges route to hidden `xray-direct`
        // (auto_detect_interface bypasses TUN); normal bridges route to the
        // paired sing-box ingress.
        int routeLoopbackBridgeCount = 0;
        for (const auto& b : ctx->xrayToSingBridges) if (b.loopbackProtect) routeLoopbackBridgeCount++;
        if (ctx->xrayToSingBridges.size() - routeLoopbackBridgeCount != ctx->singIngressTags.size()) {
            ctx->error = "xray to sing-box bridges count does not match ingress tags count";
            return;
        }
        QJsonArray bridgeRules;
        int routeSingIngressIdx = 0;
        for (auto idx = 0; idx < ctx->xrayToSingBridges.size(); idx++) {
            auto bridgeConf = ctx->xrayToSingBridges[idx];
            QString inboundTag, outboundTag;
            if (bridgeConf.loopbackProtect) {
                inboundTag = "bridge-loopback-" + Int2String(bridgeConf.port);
                outboundTag = "xray-direct";
            } else {
                inboundTag = "bridge-" + ctx->singIngressTags[routeSingIngressIdx];
                outboundTag = ctx->singIngressTags[routeSingIngressIdx];
                routeSingIngressIdx++;
            }
            bridgeRules.append(QJsonObject{
                {"inbound", inboundTag},
                {"action", "route"},
                {"outbound", outboundTag},
            });
        }

        // raw profiles bring their own rule_set definitions; merge them after ours.
        if (routeChain->isRaw) {
            for (const auto& rs : rawRouteObj.value("rule_set").toArray()) ruleSetArray.append(rs);
        }

        // apply
        const int defOut = routeChain->defaultOutboundID;

        // Assemble the final rule list in priority order:
        //   bridges -> extra-core carve-out -> our injected plumbing
        //   (sniff, resolve, dns-in, dns hijack/redirect) -> profile rules
        //   -> block-by-default catch-all.
        // Appending in order (instead of the old prepend juggling) makes the
        // precedence explicit.
        QJsonArray routeRules;
        for (const auto& r : bridgeRules) routeRules.append(r);
        if (!extraCoreDirect.isEmpty()) routeRules.append(extraCoreDirect);
        auto appendIfSet = [&routeRules](const QJsonObject& r) { if (!r.isEmpty()) routeRules.append(r); };
        appendIfSet(injected.sniff);
        appendIfSet(injected.resolve);
        appendIfSet(injected.dnsHijack);
        appendIfSet(injected.dnsInReject);
        appendIfSet(injected.redirectSniff);
        for (const auto& r : profileRules) routeRules.append(r);
        // block-by-default: drop any unmatched connection. (DNS still resolves; it's
        // the connection that's blocked.)
        if (!routeChain->isRaw && defOut == blockID) {
            routeRules.append(QJsonObject{{"action", "reject"}});
        }

        // For a raw profile start from the user's (translated) route object so any extra
        // keys they set survive; rules/rule_set we assembled and the mandatory route keys
        // below override or fill in.
        QJsonObject route = routeChain->isRaw ? rawRouteObj : QJsonObject{};
        route["rules"] = routeRules;
        route["rule_set"] = ruleSetArray;
        if (routeChain->isRaw) {
            if (!route.contains("final")) route["final"] = "proxy"; // user's final, else a safe default
        } else if (defOut == blockID) {
            // Unreachable thanks to the catch-all reject above, but `final` must still
            // name a real outbound; `direct` is always present.
            route["final"] = "direct";
        } else if (defOut == warpBypassID) {
            route["final"] = dataManager->settingsRepo->enable_warp ? "warp-bypass" : "proxy";
        } else {
            route["final"] = outboundIDToString(defOut);
        }
        if (Configs::dataManager->settingsRepo->enable_stats && !route.contains("find_process"))  route["find_process"] = true;
        if (!route.contains("default_domain_resolver"))
            route["default_domain_resolver"] = QJsonObject{
                                    {"server", "dns-direct"},
                                    {"strategy", Configs::dataManager->settingsRepo->default_domain_strategy}};
        if (Configs::dataManager->settingsRepo->spmode_vpn && !route.contains("auto_detect_interface")) route["auto_detect_interface"] = true;

        ctx->buildConfigResult->coreConfig["route"] = route;
    }

    void buildExperimentalSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        if (ctx->forTest) return;

        QJsonObject experimentalObj;
        QJsonObject clash_api = {
            {"default_mode", ""} // dummy to make sure it is created
        };
        if (Configs::dataManager->settingsRepo->core_box_clash_api > 0){
            clash_api = {
                {"external_controller", Configs::dataManager->settingsRepo->core_box_clash_listen_addr + ":" + Int2String(Configs::dataManager->settingsRepo->core_box_clash_api)},
                {"secret", Configs::dataManager->settingsRepo->core_box_clash_api_secret},
                {"external_ui", "dashboard"},
                };
        }
        if (Configs::dataManager->settingsRepo->core_box_clash_api > 0 || Configs::dataManager->settingsRepo->enable_stats)
        {
            experimentalObj["clash_api"] = clash_api;
        }

        QJsonObject cache_file = {
            {"enabled", true},
            {"store_fakeip", true},
            {"store_rdrc", true}
        };
        experimentalObj["cache_file"] = cache_file;

        if (experimentalObj.isEmpty()) return;

        // apply
        ctx->buildConfigResult->coreConfig["experimental"] = experimentalObj;
    }

    void buildXrayConfig(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        if (ctx->xrayOutbounds.isEmpty()) return;
        ctx->buildConfigResult->isXrayNeeded = true;
        QJsonObject dnsObj;
        QJsonArray inbounds;
        QJsonArray routeRules;
        int dnsPort = dataManager->settingsRepo->core_dns_in_port;

        if (!ctx->forTest) {
            dnsObj = {
                {"servers", QJsonArray{
                    QJsonObject{
                        {"address", "127.0.0.1"},
                        {"port", dnsPort},
                        {"queryStrategy", "UseIPv4"},
                        {"skipFallBack", true}
                    },
                    QJsonObject{
                            {"address", "127.0.0.1"},
                            {"port", dnsPort},
                            {"queryStrategy", "UseIPv6"},
                            {"skipFallBack", true}
                    }
                }}
            };
        }

        if (ctx->xrayIngressTags.size() != ctx->singToXrayBridges.size()) {
            ctx->error = "xray ingress tags size does not match bridge count!";
            return;
        }

        for (int i = 0; i<ctx->xrayIngressTags.size(); i++) {
            auto outboundTag = ctx->xrayIngressTags[i];
            auto bridgeConf = ctx->singToXrayBridges[i];
            auto inboundTag = outboundTag + "-" + "inbound";
            inbounds << QJsonObject{
                {"tag", inboundTag},
                {"listen", bridgeConf.host},
                {"port", bridgeConf.port},
                {"protocol", "socks"},
                {"settings", QJsonObject{
                    {"auth", "password"},
                    {"udp", true},
                    {"accounts", QJsonArray{
                        QJsonObject{
                            {"user", bridgeConf.auth},
                            {"pass", bridgeConf.auth}
                        }
                    }}
                }}
            };
            routeRules << QJsonObject{
                {"type", "field"},
                {"inboundTag", QJsonArray{inboundTag}},
                {"outboundTag", outboundTag}
            };
        }

        // dnsRouting
        if (!ctx->forTest) {
            ctx->xrayOutbounds << QJsonObject{
                {"tag", "direct"},
                {"protocol", "freedom"},
            };
            routeRules << QJsonObject{
                {"type", "field"},
                {"ip", QJsonArray{"127.0.0.1"}},
                {"port", dnsPort},
                {"outboundTag", "direct"},
            };
        }

        ctx->buildConfigResult->xrayConfig["log"] = QJsonObject{
        {"loglevel", Configs::dataManager->settingsRepo->xray_log_level},
        {"access", Configs::dataManager->settingsRepo->xray_log_level == "info" ? "" : "none"}
        };
        ctx->buildConfigResult->xrayConfig["dns"] = dnsObj;
        ctx->buildConfigResult->xrayConfig["inbounds"] = inbounds;
        ctx->buildConfigResult->xrayConfig["outbounds"] = ctx->xrayOutbounds;
        ctx->buildConfigResult->xrayConfig["routing"] = QJsonObject{
            {"domainStrategy", "AsIs"},
            {"rules", routeRules},
        };
    }

    std::shared_ptr<BuildConfigResult> BuildSingBoxConfig(const std::shared_ptr<Profile>& ent, bool forExport) {
        if (ent->type == "custom")
        {
            auto res = std::make_shared<BuildConfigResult>();
            auto custom = ent->Custom();
            if (custom == nullptr)
            {
                res->error = "Corrupted data, needed custom ent, got nullptr";
                return res;
            }
            if (custom->type == Custom::CustomFullConfig)
            {
                res->coreConfig = custom->Build().object;
                return res;
            }
        }

        auto ctx = std::make_shared<BuildSingBoxConfigContext>();
        ctx->ent = ent;
        ctx->forExport = forExport;

        CalculatePrerequisities(ctx);

        // log
        buildLogSections(ctx);
        // ntp
        buildNTPSection(ctx);
        // DNS
        buildDNSSection(ctx);
        if (!ctx->error.isEmpty())
        {
            MW_show_log("Config build error:" + ctx->error);
            ctx->buildConfigResult->error = ctx->error;
            return ctx->buildConfigResult;
        }
        // certificate
        buildCertificateSection(ctx);
        // Inbound
        buildInboundSection(ctx);
        if (!ctx->error.isEmpty())
        {
            MW_show_log("Config build error:" + ctx->error);
            ctx->buildConfigResult->error = ctx->error;
            return ctx->buildConfigResult;
        }
        // outbound
        buildOutboundsSection(ctx);
        if (!ctx->error.isEmpty())
        {
            MW_show_log("Config build error:" + ctx->error);
            ctx->buildConfigResult->error = ctx->error;
            return ctx->buildConfigResult;
        }
        // Route
        buildRouteSection(ctx);
        if (!ctx->error.isEmpty())
        {
            MW_show_log("Config build error:" + ctx->error);
            ctx->buildConfigResult->error = ctx->error;
            return ctx->buildConfigResult;
        }
        // experimental
        buildExperimentalSection(ctx);
        if (!ctx->error.isEmpty())
        {
            MW_show_log("Config build error:" + ctx->error);
            ctx->buildConfigResult->error = ctx->error;
            return ctx->buildConfigResult;
        }
        // xray
        buildXrayConfig(ctx);
        if (!ctx->error.isEmpty()) {
            MW_show_log("Config build error:" + ctx->error);
            ctx->buildConfigResult->error = ctx->error;
            return ctx->buildConfigResult;
        }
        return ctx->buildConfigResult;
    }

    bool IsValid(const std::shared_ptr<Profile>& ent)
    {
        if (ent->type == "chain")
        {
            auto chain = ent->Chain();
            if (chain == nullptr)
            {
                MW_show_log("Corrupted data, needed chain ent, got nullptr");
                return false;
            }
            for (int eId : chain->list)
            {
                auto e = Configs::dataManager->profilesRepo->GetProfile(eId);
                if (e == nullptr)
                {
                    MW_show_log("Null ent in validator");
                    return false;
                }
                if (!IsValid(e))
                {
                    MW_show_log("Invalid ent in chain: ID=" + QString::number(eId));
                    return false;
                }
            }
            return true;
        }
        QJsonObject conf;
        bool fullConf = false;
        if (ent->type == "custom")
        {
            auto custom = ent->Custom();
            if (custom == nullptr)
            {
                MW_show_log("Corrupted data in isValid, needed custom ent, got nullptr");
                return false;
            }
            if (custom->type == Custom::CustomFullConfig)
            {
                conf = QString2QJsonObject(custom->config);
                fullConf = true;
            }
            if (custom->type == Custom::CustomXrayFullConfig)
            {
                // sing-box can't validate Xray-format configs; just check the
                // user provided parseable JSON.
                if (QString2QJsonObject(custom->config).isEmpty()) {
                    MW_show_log("Custom Xray full config is not valid JSON");
                    return false;
                }
                return true;
            }
        }
        // Xray profiles (native Xray outbounds and custom Xray outbounds) carry
        // an Xray-format outbound that sing-box can't parse — its sing-box
        // Build() is only a dummy placeholder. Validate the real outbound via
        // the Xray core instead. Custom full configs never reach here (handled
        // above), so IsXray() cleanly selects the Xray-validation path.
        if (!fullConf && ent->outbound->IsXray())
        {
            auto [out, err] = ent->outbound->BuildXray();
            if (!err.isEmpty())
            {
                MW_show_log("Invalid Xray ent " + ent->outbound->name + ": " + err);
                return false;
            }
            QJsonObject xrayConf{
                {"outbounds", QJsonArray{out}},
            };
            bool ok;
            auto resp = API::defaultClient->CheckConfig(&ok, QJsonObject2QString(xrayConf, true), true);
            if (!ok)
            {
                MW_show_log("Failed to Call the Core: " + resp);
                return false;
            }
            if (resp.isEmpty()) return true;
            // else
            MW_show_log("Invalid Xray ent " + ent->outbound->name + ": " + resp);
            return false;
        }
        if (!fullConf)
        {
            auto out = ent->outbound->Build();
            auto outArr = QJsonArray{out.object};
            auto key = ent->outbound->IsEndpoint() ? "endpoints" : "outbounds";
            conf = {
                {key, outArr},
                };
        }
        bool ok;
        conf.insert("log", QJsonObject{{"level", dataManager->settingsRepo->log_level}});
        auto resp = API::defaultClient->CheckConfig(&ok, QJsonObject2QString(conf, true));
        if (!ok)
        {
            MW_show_log("Failed to Call the Core: " + resp);
            return false;
        }
        if (resp.isEmpty()) return true;
        // else
        MW_show_log("Invalid ent " + ent->outbound->name + ": " + resp);
        return false;
    }

    std::shared_ptr<BuildTestConfigResult> BuildTestConfig(const QList<std::shared_ptr<Profile> > &profiles)
    {
        auto res = std::make_shared<BuildTestConfigResult>();
        auto ctx = std::make_shared<BuildSingBoxConfigContext>();
        ctx->forTest = true;
        // Always bind a test's xray egress to the physical default interface so a
        // result never depends on the live TUN state: if a profile is running with
        // TUN, its TUN owns the default route and would otherwise capture and
        // re-proxy the test; with no TUN up this is a harmless no-op (the physical
        // NIC is already the default route). The sing-box side gets the same
        // bypass via the test route's auto_detect_interface. On RPC failure we
        // simply don't bind.
        {
            bool ifcOK = false;
            auto ifc = API::defaultClient->GetDefaultInterface(&ifcOK);
            if (ifcOK && !ifc.isEmpty()) ctx->defaultInterface = ifc;
        }
        QList<int> entIDs;
        for (const auto& proxy : profiles) entIDs << proxy->id;
        ctx->buildPrerequisities->dnsDeps->directDomains = QListStr2QJsonArray(getEntDomains(entIDs, ctx->error));
        if (!ctx->buildPrerequisities->dnsDeps->directDomains.isEmpty()) ctx->buildPrerequisities->dnsDeps->needDirectDnsRules = true;
        buildDNSSection(ctx, false);
        if (!ctx->error.isEmpty())
        {
            res->error = ctx->error;
            return res;
        }
        buildLogSections(ctx);
        buildCertificateSection(ctx);
        buildNTPSection(ctx);
        int suffix = 1;

        int xrayPortIdx=0;
        int xrayCount=0;
        int chainCount=0;
        for (const auto& proxy : profiles) {
            if (proxy->outbound->IsXray()) xrayCount++;
            if (proxy->type == "chain") chainCount++;
        }
        auto xrayPorts = MkManyPorts(xrayCount + 2*chainCount); // assume all chains transition twice and allocate port for them

        for (const auto& item : profiles)
        {
            if (item->outbound != nullptr && item->outbound->IsExtraCore())
            {
                MW_show_log("Skipping extra-core conf");
                continue;
            }
            if (item->outbound != nullptr && item->outbound->IsXrayFullConfig())
            {
                MW_show_log("Skipping custom Xray full config (cannot batch-test)");
                continue;
            }
            if (item->type == "chain")
            {
                bool chainHasTerminal = false;
                if (auto c = item->Chain(); c != nullptr) {
                    for (int hopID : c->list) {
                        auto hopEnt = Configs::dataManager->profilesRepo->GetProfile(hopID);
                        if (hopEnt != nullptr && hopEnt->outbound != nullptr && (hopEnt->outbound->IsExtraCore() || hopEnt->outbound->IsXrayFullConfig())) { chainHasTerminal = true; break; }
                    }
                }
                if (chainHasTerminal) {
                    MW_show_log("Skipping chain with terminal (extra-core or Xray full config) hop (cannot test)");
                    continue;
                }
            }
            if (item->type == "tailscale")
            {
                MW_show_log("Skipping Tailscale conf");
                continue;
            }
            if (!IsValid(item)) {
                MW_show_log("Skipping invalid config: " + item->outbound->name);
                item->latency = -1;
                continue;
            }
            if (item->type == "custom")
            {
                auto custom = item->Custom();
                if (custom == nullptr)
                {
                    MW_show_log("Corrupted data in build test config");
                    res->error = "Corrupted data in build test config";
                    return res;
                }
                if (custom->type == Custom::CustomFullConfig)
                {
                    auto obj = QString2QJsonObject(custom->config);
                    obj["inbounds"] = QJsonArray();
                    res->fullConfigs[item->id] = QJsonObject2QString(obj, true);
                    continue;
                }
            }
            auto IDs = unwrapChain(item->id);
            auto group = Configs::dataManager->groupsRepo->GetGroup(item->gid);
            if (group == nullptr) {
                res->error = "Null group on profile, data is corrupted";
                return res;
            }
            if (group->landing_proxy_id >= 0) IDs.prepend(group->landing_proxy_id);
            if (group->front_proxy_id >= 0) IDs.append(group->front_proxy_id);
            int singToXrayPort = -1;
            int xrayToSingPort = -1;
            if (item->outbound->IsXray()) singToXrayPort = xrayPorts[xrayPortIdx++];
            if (item->type == "chain") {
                singToXrayPort = xrayPorts[xrayPortIdx++];
                xrayToSingPort = xrayPorts[xrayPortIdx++];
            }
            buildOutboundChain(ctx, IDs, "proxy-" + Int2String(suffix), false, true, singToXrayPort, xrayToSingPort);
            if (!ctx->error.isEmpty()) {
                res->error = ctx->error;
                return res;
            }
            auto tag = "proxy-" + Int2String(suffix) + "-0";
            res->outboundTags << tag;
            res->tag2entID.insert(tag, item->id);
            suffix++;
        }
        buildXrayConfig(ctx);
        if (!ctx->error.isEmpty()) {
            res->error = ctx->error;
            return res;
        }
        ctx->outbounds << QJsonObject{{"type", "direct"}, {"tag", "direct"}};
        ctx->buildConfigResult->coreConfig["outbounds"] = ctx->outbounds;
        ctx->buildConfigResult->coreConfig["endpoints"] = ctx->endpoints;
        ctx->buildConfigResult->coreConfig["route"] = QJsonObject{
                {"auto_detect_interface", true},
                {"default_domain_resolver", QJsonObject{
                        {"server", "dns-direct"},
                        {"strategy", Configs::dataManager->settingsRepo->default_domain_strategy},
                   }}
        };
        // Also add the needed socks inbound bridges
        QJsonArray inboundArr;
        for (auto bridgeConf : ctx->xrayToSingBridges) {
            QJsonObject userObj = {
                {"username", bridgeConf.auth},
                {"password", bridgeConf.auth}
            };
            QJsonObject socksBridge = {
                {"type", "socks"},
                {"tag", "bridge-"+Int2String(bridgeConf.port)},
                {"listen", bridgeConf.host},
                {"listen_port", bridgeConf.port},
                {"users", QJsonArray{userObj}}
            };
            inboundArr.append(socksBridge);
        }
        ctx->buildConfigResult->coreConfig["inbounds"] = inboundArr;
        res->coreConfig = ctx->buildConfigResult->coreConfig;
        res->xrayConfig = ctx->buildConfigResult->xrayConfig;
        res->isXrayNeeded = ctx->buildConfigResult->isXrayNeeded;

        return res;
    }
}
