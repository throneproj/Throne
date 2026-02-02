#include "include/configs/generate.h"
#include "include/api/RPC.h"
#include "include/global/Configs.hpp"
#include "include/global/Utils.hpp"

#include <QApplication>
#include <QFileInfo>


#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"


#include "include/database/entities/Profile.h"

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

    inline OSType getOS()
    {
#ifdef Q_OS_MACOS
        return Darwin;
#endif
#ifdef Q_OS_LINUX
        return Linux;
#endif
#ifdef Q_OS_WIN
        return Windows;
#endif
        return Unknown;
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
                if (ent->type == "extracore") continue;
                if (ent->type == "chain") domains << getChainDomains(ent, error);
                else
                {
                    if (auto addr = ent->outbound->GetAddress(); !addr.isEmpty() && !IsIpAddress(addr)) domains.append(addr);
                }
            }
        }

        return domains;
    }

    void CalculatePrerequisities(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        ctx->tunEnabled = Configs::dataManager->settingsRepo->spmode_vpn;
        ctx->os = getOS();
        if (ctx->os == Linux)
        {
            ctx->isResolvedUsed = ReadFileText("/etc/resolv.conf").contains("systemd-resolved");
        }
        auto preReqs = ctx->buildPrerequisities;
        
        // Get route chain
        auto routeChain = Configs::dataManager->routesRepo->GetRouteProfile(Configs::dataManager->settingsRepo->current_route_id);
        if (routeChain == nullptr) {
            ctx->error = "Routing profile does not exist, try resetting the route profile in Routing Settings";
            return;
        }

        // Routing dependencies
        auto neededOutbounds = routeChain->get_used_outbounds();
        auto neededRuleSets = routeChain->get_used_rule_sets();
        preReqs->routingDeps->defaultOutboundID = routeChain->defaultOutboundID;
        preReqs->routingDeps->outboundMap[-1] = "proxy";
        preReqs->routingDeps->outboundMap[-2] = "direct";
        int suffix = 0;
        for (const auto &item: *neededOutbounds) {
            if (item < 0) continue;
            auto neededEnt = Configs::dataManager->profilesRepo->GetProfile(item);
            if (neededEnt == nullptr) {
                ctx->error = "The routing profile is referencing outbounds that no longer exists, consider revising your settings";
                return;
            }
            if (neededEnt->type == "extracore" || neededEnt->type == "custom" || neededEnt->type == "chain")
            {
                ctx->error = "Outbounds used in routing profile cannot be of types extracore, custom or chain";
                return;
            }
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
            preReqs->routingDeps->neededOutbounds << item;
        }

        for (const auto &item: *neededRuleSets) {
            preReqs->routingDeps->neededRuleSets << item;
        }

        // Direct domains
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

        // Extra core
        if (ctx->ent->type == "extracore")
        {
            auto outbound = ctx->ent->ExtraCore();
            if (outbound == nullptr)
            {
                MW_show_log("INVALID ENT TYPE, NEEDED EXTRACORE GOT NULLPTR");
                ctx->error = "failed to cast to extracore, type is: " + ctx->ent->type;
                return;
            }
            ctx->buildConfigResult->extraCoreData->path = QFileInfo(outbound->extraCorePath).canonicalFilePath();
            ctx->buildConfigResult->extraCoreData->args = outbound->extraCoreArgs;
            ctx->buildConfigResult->extraCoreData->config = outbound->extraCoreConf;
            ctx->buildConfigResult->extraCoreData->configDir = GetBasePath();
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
            if (addr.contains("/")) {
                path = addr.split("/").last();
                addr = addr.left(addr.indexOf("/"));
            }
        }
        if (address.startsWith("h3://")) {
            type = "h3";
            addr = addr.replace("h3://", "");
            if (addr.contains("/")) {
                path = addr.split("/").last();
                addr = addr.left(addr.indexOf("/"));
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
            if (isTailscale)
            {
                auto tailscale = ctx->ent->Tailscale();
                if (tailscale == nullptr)
                {
                    ctx->error = "Corrupted state, needed tailscale been but could not cast";
                    return;
                }
                auto tailDns = QJsonObject{
                        {"type", "tailscale"},
                        {"tag", "dns-remote"},
                        {"endpoint", "proxy"},
                        {"accept_default_resolvers", tailscale->globalDNS},
                    };
                servers += tailDns;
            } else
            {
                auto remoteDnsObj = buildDnsObj(Configs::dataManager->settingsRepo->remote_dns, ctx);
                remoteDnsObj["tag"] = "dns-remote";
                remoteDnsObj["domain_resolver"] = "dns-local";
                remoteDnsObj["detour"] = "proxy";
                servers += remoteDnsObj;
            }
        }

        // direct
        auto directDnsObj = buildDnsObj(Configs::dataManager->settingsRepo->direct_dns, ctx);
        directDnsObj["tag"] = "dns-direct";
        directDnsObj["domain_resolver"] = "dns-local";
        if (Configs::dataManager->settingsRepo->dns_final_out == "direct") {
            servers.prepend(directDnsObj);
        } else {
            servers.append(directDnsObj);
        }

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
                    {"rcode", "NOERROR"},
                    {"answer", "localhost. IN AAAA ::1"},
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
                    {"server", "dns-direct"},
                };
        }

        // Local
        auto dnsLocalAddress = Configs::dataManager->settingsRepo->core_box_underlying_dns.isEmpty() ? "local" : Configs::dataManager->settingsRepo->core_box_underlying_dns;
        auto dnsLocalObj = buildDnsObj(dnsLocalAddress, ctx);
        dnsLocalObj["tag"] = "dns-local";
        servers += dnsLocalObj;

        auto dnsObj = QJsonObject{
            {"servers", servers},
            {"rules", rules}
        };
        if (independentCache) dnsObj["independent_cache"] = true;
        ctx->buildConfigResult->coreConfig["dns"] = dnsObj;
    }

    void buildInboundSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        auto tunDeps = ctx->buildPrerequisities->tunDeps;
        QJsonArray inbounds;
        // mixed
        if (!ctx->forTest) {
            QJsonObject inboundObj;
            inboundObj["tag"] = "mixed-in";
            inboundObj["type"] = "mixed";
            inboundObj["listen"] = Configs::dataManager->settingsRepo->inbound_address;
            inboundObj["listen_port"] = Configs::dataManager->settingsRepo->inbound_socks_port;
            inbounds += inboundObj;
        }

        // Tun
        if (Configs::dataManager->settingsRepo->spmode_vpn && !ctx->forTest) {
            QJsonObject inboundObj;
            inboundObj["tag"] = "tun-in";
            inboundObj["type"] = "tun";
            inboundObj["interface_name"] = genTunName();
            inboundObj["auto_route"] = true;
            inboundObj["mtu"] = Configs::dataManager->settingsRepo->vpn_mtu;
            inboundObj["stack"] = Configs::dataManager->settingsRepo->vpn_implementation;
            inboundObj["strict_route"] = Configs::dataManager->settingsRepo->vpn_strict_route;
            if (ctx->os == Linux) inboundObj["auto_redirect"] = true;
            auto tunAddress = QJsonArray{"172.19.0.1/24"};
            if (Configs::dataManager->settingsRepo->vpn_ipv6) tunAddress += "fdfe:dcba:9876::1/96";
            inboundObj["address"] = tunAddress;

            if (ctx->buildPrerequisities->routingDeps->defaultOutboundID == proxyID && Configs::dataManager->settingsRepo->enable_tun_routing)
            {
                QJsonArray routeExcludeAddrs = {"127.0.0.0/8"};
                QJsonArray routeExcludeSets;
                for (auto item: tunDeps->directIPCIDRs) routeExcludeAddrs << item;
                for (auto item: tunDeps->directIPSets) routeExcludeSets << item;
                inboundObj["route_exclude_address"] = routeExcludeAddrs;
                if (!routeExcludeSets.isEmpty()) inboundObj["route_exclude_address_set"] = routeExcludeSets;
            }
            inbounds += inboundObj;
        }

        // Hijack
        if (Configs::dataManager->settingsRepo->enable_redirect && !ctx->forTest) {
            inbounds.prepend(QJsonObject{
                {"tag", "hijack"},
                {"type", "direct"},
                {"listen", Configs::dataManager->settingsRepo->redirect_listen_address},
                {"listen_port", Configs::dataManager->settingsRepo->redirect_listen_port},
            });
        }
        if (Configs::dataManager->settingsRepo->enable_dns_server && !ctx->forTest) {
            inbounds.prepend(QJsonObject{
                {"tag", "dns-in"},
                {"type", "direct"},
                {"listen", Configs::dataManager->settingsRepo->dns_server_listen_lan ? "0.0.0.0" : "127.1.1.1"},
                {"listen_port", Configs::dataManager->settingsRepo->dns_server_listen_port},
            });
        }

        // custom
        if (!ctx->forTest) QJSONARRAY_ADD(inbounds, QString2QJsonObject(Configs::dataManager->settingsRepo->custom_inbound)["inbounds"].toArray())
        ctx->buildConfigResult->coreConfig["inbounds"] = inbounds;
    }

    void entIDListtoEntList(const QList<int>& entIDs, QList<std::shared_ptr<Profile>> &ents, QString& error)
    {
        bool hasExtracore = false;
        bool hasCustom = false;
        bool hasXray = false;
        for (auto id : entIDs)
        {
            auto ent = Configs::dataManager->profilesRepo->GetProfile(id);
            if (ent == nullptr)
            {
                error = "Null proxy in chain, you may want to check your configs";
                return;
            }
            if (ent->type == "chain")
            {
                error = "Chain in Chain is not allowed";
                return;
            }
            if (ent->type == "extracore") hasExtracore = true;
            if (ent->type == "custom") hasCustom = true;
            if (ent->outbound->IsXray()) hasXray = true;
            ents.append(ent);
        }
        if (ents.size() > 1 && (hasExtracore || hasCustom || hasXray))
        {
            error = "Cannot use Extracore or Custom or Xray configs in a chain";
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
            return chain->list;
        }
        return {entID};
    }

    void buildOutboundChain(std::shared_ptr<BuildSingBoxConfigContext> &ctx, const QList<int>& entIDs, const QString& prefix, bool includeProxy, bool link, int xrayPort = -1)
    {
        QList<std::shared_ptr<Profile>> ents;
        entIDListtoEntList(entIDs, ents, ctx->error);
        for (int idx = 0; idx < ents.size(); idx++)
        {
            auto tag = prefix + "-" + Int2String(idx);
            QString nextTag;
            if (idx < ents.size() - 1) nextTag = prefix + "-" + Int2String(idx+1);
            if (includeProxy && idx == 0) tag = "proxy";
            const auto& ent = ents[idx];
            auto [object, error] = ent->outbound->Build();
            if (!error.isEmpty())
            {
                ctx->error += error;
                return;
            }
            object["tag"] = tag;
            if (!nextTag.isEmpty() && link) object["detour"] = nextTag;
            if (ent->outbound->IsXray()) {
                auto [xrayObj, xrayErr] = ent->outbound->BuildXray();
                if (!xrayErr.isEmpty()) {
                    ctx->error += xrayErr;
                    return;
                }
                if (xrayPort == -1) xrayPort = MkPort();
                object["server_port"] = xrayPort;
                xrayObj["tag"] = tag;
                ctx->xrayOutbounds.append({xrayPort, xrayObj});
            }
            if (ent->outbound->IsEndpoint())
            {
                ctx->endpoints.append(object);
            } else
            {
                ctx->outbounds.append(object);
            }
            ent->traffic_data->id = ent->id;
            ent->traffic_data->tag = tag.toStdString();
            ctx->buildConfigResult->outboundStats += ent->traffic_data;
        }
    }

    void buildOutboundsSection(std::shared_ptr<BuildSingBoxConfigContext> &ctx) {
        // First, our own ent
        bool noChain = ctx->ent->outbound->IsXray();
        QList<int> entIDs;
        auto group = Configs::dataManager->groupsRepo->GetGroup(ctx->ent->gid);
        if (group == nullptr)
        {
            ctx->error = "No group found for ent, data is corrupted";
            return;
        }
        if (group->landing_proxy_id >= 0 && !noChain) entIDs.prepend(group->landing_proxy_id);
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
        if (group->front_proxy_id >= 0 && !noChain) entIDs.append(group->front_proxy_id);
        buildOutboundChain(ctx, entIDs, "config", true, true);

        // Now, build the outbounds needed by the route profile
        buildOutboundChain(ctx, ctx->buildPrerequisities->routingDeps->neededOutbounds, "route", false, false);

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

        // hijack
        if (Configs::dataManager->settingsRepo->enable_dns_server && !ctx->forTest)
        {
            auto sniffRule = std::make_shared<RouteRule>();
            sniffRule->action = "sniff";
            sniffRule->inbound = {"dns-in"};

            auto redirRule = std::make_shared<RouteRule>();
            redirRule->action = "hijack-dns";
            redirRule->inbound = {"dns-in"};

            routeChain->Rules.prepend(redirRule);
            routeChain->Rules.prepend(sniffRule);
        }
        if (Configs::dataManager->settingsRepo->enable_redirect && !ctx->forTest) {
            auto sniffRule = std::make_shared<RouteRule>();
            sniffRule->action = "sniff";
            sniffRule->sniffOverrideDest = true;
            sniffRule->inbound = {"hijack"};
            routeChain->Rules.prepend(sniffRule);
        }

        // sniff and resolve
        if (!Configs::dataManager->settingsRepo->domain_strategy.isEmpty())
        {
            auto resolveRule = std::make_shared<RouteRule>();
            resolveRule->action = "resolve";
            resolveRule->strategy = Configs::dataManager->settingsRepo->domain_strategy;
            resolveRule->inbound = {"mixed-in", "tun-in"};
            routeChain->Rules.prepend(resolveRule);
        }
        if (Configs::dataManager->settingsRepo->sniffing_mode != SniffingMode::DISABLE)
        {
            auto sniffRule = std::make_shared<RouteRule>();
            sniffRule->action = "sniff";
            sniffRule->inbound = {"mixed-in", "tun-in"};
            routeChain->Rules.prepend(sniffRule);
        }

        // rules
        auto routeRules = routeChain->get_route_rules(false, routeDeps->outboundMap);
        routeRules.prepend(QJsonObject{
            {"action", "route"},
            {"process_path", FindCoreRealPath()},
            {"outbound", "direct"},
        });

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
                if(ruleSetMap.contains(item.toStdString())) {
                    ruleSetArray += QJsonObject{
                                {"type", "remote"},
                                {"tag", item},
                                {"format", "binary"},
                                {"url", get_jsdelivr_link(QString::fromStdString(ruleSetMap.at(item.toStdString())))},
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

        // apply
        QJsonObject route;
        route["rules"] = routeRules;
        route["rule_set"] = ruleSetArray;
        route["final"] = outboundIDToString(routeChain->defaultOutboundID);
        if (Configs::dataManager->settingsRepo->enable_stats)  route["find_process"] = true;
        route["default_domain_resolver"] = QJsonObject{
                                {"server", "dns-direct"},
                                {"strategy", Configs::dataManager->settingsRepo->outbound_domain_strategy}};
        if (Configs::dataManager->settingsRepo->spmode_vpn) route["auto_detect_interface"] = true;

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
        QJsonArray inbounds;
        QJsonArray outbounds;
        QJsonArray routeRules;

        for (auto [port, outboundObj] : ctx->xrayOutbounds) {
            auto inboundTag = outboundObj["tag"].toString() + "-inbound";
            inbounds << QJsonObject{
                {"tag", inboundTag},
                {"listen", "127.0.0.1"},
                {"port", port},
                {"protocol", "socks"},
                {"settings", QJsonObject{{"auth", "noauth"}, {"udp", true}}}
            };
            outbounds << outboundObj;
            routeRules << QJsonObject{
                {"type", "field"},
                {"inboundTag", QJsonArray{inboundTag}},
                {"outboundTag", outboundObj["tag"].toString()}
            };
        }

        ctx->buildConfigResult->xrayConfig["log"] = QJsonObject{
        {"loglevel", Configs::dataManager->settingsRepo->xray_log_level},
        {"access", Configs::dataManager->settingsRepo->xray_log_level == "info" ? "" : "none"}
        };
        ctx->buildConfigResult->xrayConfig["inbounds"] = inbounds;
        ctx->buildConfigResult->xrayConfig["outbounds"] = outbounds;
        ctx->buildConfigResult->xrayConfig["routing"] = QJsonObject{
            {"domainStrategy", "AsIs"},
            {"rules", routeRules},
        };
    }

    std::shared_ptr<BuildConfigResult> BuildSingBoxConfig(const std::shared_ptr<Profile>& ent) {
        if (ent->type == "custom")
        {
            auto res = std::make_shared<BuildConfigResult>();
            auto custom = ent->Custom();
            if (custom == nullptr)
            {
                res->error = "Corrupted data, needed custom ent, got nullptr";
                return res;
            }
            if (custom->type == "fullconfig")
            {
                res->coreConfig = custom->Build().object;
                return res;
            }
        }

        auto ctx = std::make_shared<BuildSingBoxConfigContext>();
        ctx->ent = ent;
        ctx->buildConfigResult->outboundStats << std::make_shared<Stats::TrafficData>("direct");

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
            if (custom->type == "fullconfig")
            {
                conf = QString2QJsonObject(custom->config);
                fullConf = true;
            }
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
        QList<int> entIDs;
        for (const auto& proxy : profiles) entIDs << proxy->id;
        ctx->buildPrerequisities->dnsDeps->directDomains = QListStr2QJsonArray(getEntDomains(entIDs, ctx->error));
        if (!ctx->buildPrerequisities->dnsDeps->directDomains.isEmpty()) ctx->buildPrerequisities->dnsDeps->needDirectDnsRules = true;
        buildDNSSection(ctx);
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
        for (const auto& proxy : profiles) {
            if (proxy->outbound->IsXray()) xrayCount++;
        }
        auto xrayPorts = MkManyPorts(xrayCount);

        for (const auto& item : profiles)
        {
            if (item->type == "extracore")
            {
                MW_show_log("Skipping ExtraCore conf");
                continue;
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
                if (custom->type == "fullconfig")
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
            if (group->landing_proxy_id >= 0 && !item->outbound->IsXray()) IDs.prepend(group->landing_proxy_id);
            if (group->front_proxy_id >= 0 && !item->outbound->IsXray()) IDs.append(group->front_proxy_id);
            buildOutboundChain(ctx, IDs, "proxy-" + Int2String(suffix), false, true, item->outbound->IsXray() ? xrayPorts[xrayPortIdx++] : -1);
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
                        {"strategy", Configs::dataManager->settingsRepo->outbound_domain_strategy},
                   }}
        };
        res->coreConfig = ctx->buildConfigResult->coreConfig;
        res->xrayConfig = ctx->buildConfigResult->xrayConfig;
        res->isXrayNeeded = ctx->buildConfigResult->isXrayNeeded;

        return res;
    }
}

