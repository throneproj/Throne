#include "include/configs/ConfigBuilder.hpp"
#include "include/dataStore/Database.hpp"
#include "include/configs/proxy/includes.h"
#include "include/configs/proxy/Preset.hpp"
#include "include/api/RPC.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
namespace Configs {
    QString genTunName() {
        auto tun_name = "throne-tun";
#ifdef Q_OS_MACOS
        tun_name = "";
#endif
        return tun_name;
    }

    void MergeJson(const QJsonObject &custom, QJsonObject &outbound) {
        // 合并
        if (custom.isEmpty()) return;
        for (const auto &key: custom.keys()) {
            if (outbound.contains(key)) {
                auto v = custom[key];
                auto v_orig = outbound[key];
                if (v.isObject() && v_orig.isObject()) { // isObject 则合并？
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



    // Common

    std::shared_ptr<BuildConfigResult> BuildConfig(const std::shared_ptr<ProxyEntity> &ent, const std::map<std::string, std::string>& ruleSetMap, bool forTest, bool forExport, int chainID) {
        auto result = std::make_shared<BuildConfigResult>();
        result->extraCoreData = std::make_shared<ExtraCoreData>();
        auto status = std::make_shared<BuildConfigStatus>();
        status->ent = ent;
        status->result = result;
        status->forTest = forTest;
        status->forExport = forExport;
        status->chainID = chainID;

        auto customBean = dynamic_cast<Configs::CustomBean *>(ent->bean.get());
        if (customBean != nullptr && customBean->core == "internal-full") {
            if (dataStore->spmode_vpn)
            {
                status->result->error = QObject::tr("Tun mode cannot be used with Custom configs");
                return result;
            }
            result->coreConfig = QString2QJsonObject(customBean->config_simple);
        } else {
            BuildConfigSingBox(status, ruleSetMap);
        }

        // apply custom config
        MergeJson(QString2QJsonObject(ent->bean->custom_config), result->coreConfig);

        return result;
    }

    bool IsValid(const std::shared_ptr<ProxyEntity>& ent)
    {
        if (ent->type == "chain")
        {
            for (int eId : ent->ChainBean()->list)
            {
                auto e = profileManager->GetProfile(eId);
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
        if (ent->type == "custom" && ent->CustomBean()->core == "internal-full")
        {
            conf = QString2QJsonObject(ent->CustomBean()->config_simple);
        } else
        {
            auto out = ent->bean->BuildCoreObjSingBox();
            auto outArr = QJsonArray{out.outbound};
            auto key = ent->bean->IsEndpoint() ? "endpoints" : "outbounds";
            conf = {
            {key, outArr},
            };
        }
        bool ok;
        auto resp = API::defaultClient->CheckConfig(&ok, QJsonObject2QString(conf, true));
        if (!ok)
        {
            MW_show_log("Failed to contact the Core: " + resp);
            return false;
        }
        if (resp.isEmpty()) return true;
        // else
        MW_show_log("Invalid ent " + ent->bean->name + ": " + resp);
        return false;
    }


    std::shared_ptr<BuildTestConfigResult> BuildTestConfig(const QList<std::shared_ptr<ProxyEntity>>& profiles, const std::map<std::string, std::string>& ruleSetMap) {
        auto results = std::make_shared<BuildTestConfigResult>();

        QJsonArray outboundArray = {
            QJsonObject{
                {"type", "direct"},
                {"tag", "direct"}
            }
        };
        QJsonArray endpointArray = {};
        int index = 0;

        QJsonArray directDomainArray;
        for (const auto &item: profiles) {
            if (item->type == "extracore")
            {
                MW_show_log("Skipping ExtraCore conf");
                continue;
            }
            if (!IsValid(item)) {
                MW_show_log("Skipping invalid config: " + item->bean->name);
                item->latency = -1;
                continue;
            }
            auto res = BuildConfig(item, ruleSetMap, true, false, ++index);
            if (!res->error.isEmpty()) {
                results->error = res->error;
                return results;
            }
            if (item->type == "custom" && item->CustomBean()->core == "internal-full") {
                res->coreConfig["inbounds"] = QJsonArray();
                results->fullConfigs[item->id] = QJsonObject2QString(res->coreConfig, true);
                continue;
            }

            // not full config, process it
            if (results->coreConfig.isEmpty()) {
                results->coreConfig = res->coreConfig;
            }
            // add the direct dns domains
            for (const auto &rule: res->coreConfig["dns"].toObject()["rules"].toArray()) {
                if (rule.toObject().contains("domain")) {
                    for (const auto &domain: rule.toObject()["domain"].toArray()) {
                        directDomainArray.append(domain);
                    }
                }
            }
            // now we add the outbounds of the current config to the final one
            auto outbounds = res->coreConfig["outbounds"].toArray();
            if (outbounds.isEmpty()) {
                results->error = QString("outbounds is empty for %1").arg(item->bean->name);
                return results;
            }
            auto endpoints = res->coreConfig["endpoints"].toArray();
            for (auto endpoint : endpoints) outbounds.append(endpoint);
            for (const auto &outboundRef: outbounds) {
                auto outbound = outboundRef.toObject();
                if (outbound["tag"] == "direct" || outbound["tag"] == "block" || outbound["tag"] == "dns-out" || outbound["tag"].toString().startsWith("rout")) continue;
                if (outbound["tag"] == "proxy") {
                    QString tag = "proxy";
                    if (index > 1) tag += Int2String(index);
                    outbound.insert("tag", tag);
                    if (outbound["type"] == "wireguard" || outbound["type"] == "tailscale")
                    {
                        endpointArray.append(outbound);
                    } else
                    {
                        outboundArray.append(outbound);
                    }
                    results->outboundTags << tag;
                    results->tag2entID.insert(tag, item->id);
                    continue;
                }
                outboundArray.append(outbound);
            }
        }

        results->coreConfig["outbounds"] = outboundArray;
        results->coreConfig["endpoints"] = endpointArray;
        auto dnsObj = results->coreConfig["dns"].toObject();
        auto dnsRulesObj = QJsonArray();
        if (!directDomainArray.empty()) {
            dnsRulesObj += QJsonObject{
                {"domain", directDomainArray},
                {"action", "route"},
                {"server", "dns-direct"}
            };
        }
        dnsObj["rules"] = dnsRulesObj;
        results->coreConfig["dns"] = dnsObj;
        results->coreConfig["route"] = QJsonObject{
            {"auto_detect_interface", true},
            {"default_domain_resolver", QJsonObject{
                    {"server", "dns-direct"},
                    {"strategy", dataStore->routing->outbound_domain_strategy},
               }}
        };

        return results;
    }

    QString BuildChain(int chainId, const std::shared_ptr<BuildConfigStatus> &status) {
        auto group = profileManager->GetGroup(status->ent->gid);
        if (group == nullptr) {
            status->result->error = QString("This profile is not in any group, your data may be corrupted.");
            return {};
        }

        auto resolveChain = [=](const std::shared_ptr<ProxyEntity> &ent) {
            QList<std::shared_ptr<ProxyEntity>> resolved;
            if (ent->type == "chain") {
                auto list = ent->ChainBean()->list;
                std::reverse(std::begin(list), std::end(list));
                for (auto id: list) {
                    resolved += profileManager->GetProfile(id);
                    if (resolved.last() == nullptr) {
                        status->result->error = QString("chain missing ent: %1").arg(id);
                        break;
                    }
                    if (resolved.last()->type == "chain") {
                        status->result->error = QString("chain in chain is not allowed: %1").arg(id);
                        break;
                    }
                }
            } else {
                resolved += ent;
            };
            return resolved;
        };

        // Make list
        auto ents = resolveChain(status->ent);
        if (!status->result->error.isEmpty()) return {};

        if (group->front_proxy_id >= 0) {
            auto fEnt = profileManager->GetProfile(group->front_proxy_id);
            if (fEnt == nullptr) {
                status->result->error = QString("front proxy ent not found.");
                return {};
            }
            ents += resolveChain(fEnt);
            if (!status->result->error.isEmpty()) return {};
        }

        if (group->landing_proxy_id >= 0) {
            auto lEnt = profileManager->GetProfile(group->landing_proxy_id);
            if (lEnt == nullptr) {
                status->result->error = QString("landing proxy ent not found.");
                return {};
            }
            ents = resolveChain(lEnt) + ents;
            if (!status->result->error.isEmpty()) return {};
        }

        // BuildChain
        QString chainTagOut = BuildChainInternal(chainId, ents, status);

        // Chain ent traffic stat
        if (ents.length() > 1) {
            status->ent->traffic_data->id = status->ent->id;
            status->ent->traffic_data->tag = chainTagOut.toStdString();
            status->ent->traffic_data->ignoreForRate = true;
            status->result->outboundStats += status->ent->traffic_data;
        }

        return chainTagOut;
    }

    QString BuildChainInternal(int chainId, const QList<std::shared_ptr<ProxyEntity>> &ents,
                               const std::shared_ptr<BuildConfigStatus> &status) {
        QString chainTag = "c-" + Int2String(chainId);
        QString chainTagOut;
        bool lastWasEndpoint = false;

        for (int index = 0; index < ents.length(); index++) {
            const auto& ent = ents.at(index);
            auto tagOut = chainTag + "-" + Int2String(ent->id) + "-" + Int2String(index);

            // last profile set as "proxy"
            if (index == 0) {
                tagOut = "proxy";
            }

            if (index > 0) {
                // chain rules: past
                auto replaced = (lastWasEndpoint ? status->endpoints : status->outbounds).last().toObject();
                replaced["detour"] = tagOut;
                ent->traffic_data->isChainTail = true;
                (lastWasEndpoint ? status->endpoints : status->outbounds).removeLast();
                (lastWasEndpoint ? status->endpoints : status->outbounds) += replaced;
            } else {
                // index == 0 means last profile in chain / not chain
                chainTagOut = tagOut;
            }

            // Outbound

            QJsonObject outbound;

            BuildOutbound(ent, status, outbound, tagOut);

            // apply custom outbound settings
            MergeJson(QString2QJsonObject(ent->bean->custom_outbound), outbound);

            // Bypass Lookup for the first profile
            auto serverAddress = ent->bean->serverAddress;

            if (auto customBean = dynamic_cast<Configs::CustomBean *>(ent->bean.get()); customBean != nullptr && customBean->core == "internal") {
                auto server = QString2QJsonObject(customBean->config_simple)["server"].toString();
                if (!server.isEmpty()) serverAddress = server;
            }

            if (!IsIpAddress(serverAddress)) {
                status->domainListDNSDirect += serverAddress;
            }

            if (ent->bean->IsEndpoint())
            {
                status->endpoints += outbound;
                lastWasEndpoint = true;
            } else
            {
                status->outbounds += outbound;
                lastWasEndpoint = false;
            }
        }

        return chainTagOut;
    }

    void BuildOutbound(const std::shared_ptr<ProxyEntity> &ent, const std::shared_ptr<BuildConfigStatus> &status, QJsonObject& outbound, const QString& tag) {
        if (ent->type == "wireguard") {
            if (ent->WireguardBean()->useSystemInterface && !IsAdmin()) {
                MW_dialog_message("configBuilder" ,"NeedAdmin");
                status->result->error = "using wireguard system interface requires elevated permissions";
                return;
            }
        }

        const auto coreR = ent->bean->BuildCoreObjSingBox();
        if (coreR.outbound.isEmpty()) {
            status->result->error = "unsupported outbound";
            return;
        }
        if (!coreR.error.isEmpty()) { // rejected
            status->result->error = coreR.error;
            return;
        }
        outbound = coreR.outbound;

        // outbound misc
        outbound["tag"] = tag;
        ent->traffic_data->id = ent->id;
        ent->traffic_data->tag = tag.toStdString();
        status->result->outboundStats += ent->traffic_data;

        // mux common
        auto needMux = ent->type == "vmess" || ent->type == "trojan" || ent->type == "vless" || ent->type == "shadowsocks";
        needMux &= dataStore->mux_concurrency > 0;

        auto stream = GetStreamSettings(ent->bean.get());
        if (stream != nullptr) {
            if (stream->network == "grpc" || stream->network == "quic" || stream->network == "anytls" || (stream->network == "http" && stream->security == "tls")) {
                needMux = false;
            }
        }

        auto mux_state = ent->bean->mux_state;
        if (mux_state == 0) {
            if (!dataStore->mux_default_on && !ent->bean->enable_brutal) needMux = false;
        } else if (mux_state == 1) {
            needMux = true;
        } else if (mux_state == 2) {
            needMux = false;
        }

        if (ent->type == "vless" && outbound["flow"] != "") {
            needMux = false;
        }

        // common
        // apply mux
        if (needMux) {
            auto muxObj = QJsonObject{
                {"enabled", true},
                {"protocol", dataStore->mux_protocol},
                {"padding", dataStore->mux_padding},
                {"max_streams", dataStore->mux_concurrency},
            };
            if (ent->bean->enable_brutal) {
                auto brutalObj = QJsonObject{
                    {"enabled", true},
                    {"up_mbps", ent->bean->brutal_speed},
                    {"down_mbps", ent->bean->brutal_speed},
                };
                muxObj["max_connections"] = 1;
                muxObj["brutal"] = brutalObj;
            }
            outbound["multiplex"] = muxObj;
        }
    }

    // SingBox

    QJsonObject BuildDnsObject(QString address, bool tunEnabled)
    {
        bool usingSystemdResolved = false;
#ifdef Q_OS_LINUX
        usingSystemdResolved = ReadFileText("/etc/resolv.conf").contains("systemd-resolved");
#endif
        if (address.startsWith("local"))
        {
            if (tunEnabled && usingSystemdResolved)
            {
                return {
                    {"type", "underlying"}
                };
            }
            return {
            {"type", "local"}
            };
        }
        if (address.startsWith("dhcp://"))
        {
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
        if (address.startsWith("tcp://"))
        {
            type = "tcp";
            addr = addr.replace("tcp://", "");
        }
        if (address.startsWith("tls://"))
        {
            type = "tls";
            addr = addr.replace("tls://", "");
        }
        if (address.startsWith("quic://"))
        {
            type = "quic";
            addr = addr.replace("quic://", "");
        }
        if (address.startsWith("https://"))
        {
            type = "https";
            addr = addr.replace("https://", "");
            if (addr.contains("/"))
            {
                path = addr.split("/").last();
                addr = addr.left(addr.indexOf("/"));
            }
        }
        if (address.startsWith("h3://"))
        {
            type = "h3";
            addr = addr.replace("h3://", "");
            if (addr.contains("/"))
            {
                path = addr.split("/").last();
                addr = addr.left(addr.indexOf("/"));
            }
        }
        if (addr.contains(":"))
        {
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


    void BuildConfigSingBox(const std::shared_ptr<BuildConfigStatus> &status, const std::map<std::string, std::string>& ruleSetMap) {
        // Prefetch
        auto routeChain = profileManager->GetRouteChain(dataStore->routing->current_route_id);
        if (routeChain == nullptr) {
            status->result->error = "Routing profile does not exist, try resetting the route profile in Routing Settings";
            return;
        }

        // will be removed on November 1st, 2025
        for (auto ruleItem = routeChain->Rules.begin(); ruleItem != routeChain->Rules.end(); ++ruleItem) {
            for (auto ruleSetItem = (*ruleItem)->rule_set.begin(); ruleSetItem != (*ruleItem)->rule_set.end(); ++ruleSetItem) {
                if ((*ruleSetItem).endsWith("_IP")) {
                    *ruleSetItem = "geoip-" + (*ruleSetItem).left((*ruleSetItem).length() - 3);
                }
                if ((*ruleSetItem).endsWith("_SITE")) {
                    *ruleSetItem = "geosite-" + (*ruleSetItem).left((*ruleSetItem).length() - 5);
                }
            }
        }
        routeChain->Save();
        
        // copy for modification
        routeChain = std::make_shared<RoutingChain>(*routeChain);

        // Outbounds
        auto tagProxy = BuildChain(status->chainID, status);
        if (!status->result->error.isEmpty()) return;
        if (status->ent->type == "extracore")
        {
            auto bean = status->ent->ExtraCoreBean();
            status->result->extraCoreData->path = QFileInfo(bean->extraCorePath).canonicalFilePath();
            status->result->extraCoreData->args = bean->extraCoreArgs;
            status->result->extraCoreData->config = bean->extraCoreConf;
            status->result->extraCoreData->configDir = GetBasePath();
            status->result->extraCoreData->noLog = bean->noLogs;
            routeChain->Rules << RouteRule::get_processPath_direct_rule(status->result->extraCoreData->path);
        }

        // Direct domains
        bool needDirectDnsRules = false;
        QJsonArray directDomains;
        QJsonArray directRuleSets;
        QJsonArray directSuffixes;
        QJsonArray directKeywords;
        QJsonArray directRegexes;

        // server addresses
        for (const auto &item: status->domainListDNSDirect) {
            directDomains.append(item);
            needDirectDnsRules = true;
        }

        auto sets = routeChain->get_direct_sites();
        for (const auto &item: sets) {
            if (item.startsWith("ruleset:")) {
                directRuleSets << item.mid(8);
            }
            if (item.startsWith("domain:")) {
                directDomains << item.mid(7);
            }
            if (item.startsWith("suffix:")) {
                directSuffixes << item.mid(7);
            }
            if (item.startsWith("keyword:")) {
                directKeywords << item.mid(8);
            }
            if (item.startsWith("regex:")) {
                directRegexes << item.mid(6);
            }
            needDirectDnsRules = true;
        }

        // Direct IPs
        QJsonArray directIPSets;
        QJsonArray directIPCIDRs;

        auto directIPraw = routeChain->get_direct_ips();
        for (const auto &item: directIPraw)
        {
            if (item.startsWith("ruleset:"))
            {
                directIPSets << item.mid(8);
            }
            if (item.startsWith("ip:"))
            {
                directIPCIDRs << item.mid(3);
            }
        }

        // Inbounds
        // mixed-in
        if (IsValidPort(dataStore->inbound_socks_port) && !status->forTest) {
            QJsonObject inboundObj;
            inboundObj["tag"] = "mixed-in";
            inboundObj["type"] = "mixed";
            inboundObj["listen"] = dataStore->inbound_address;
            inboundObj["listen_port"] = dataStore->inbound_socks_port;
            status->inbounds += inboundObj;
        }

        // tun-in
        if (dataStore->spmode_vpn && !status->forTest) {
            QJsonObject inboundObj;
            inboundObj["tag"] = "tun-in";
            inboundObj["type"] = "tun";
            inboundObj["interface_name"] = genTunName();
            inboundObj["auto_route"] = true;
            inboundObj["mtu"] = dataStore->vpn_mtu;
            inboundObj["stack"] = dataStore->vpn_implementation;
            inboundObj["strict_route"] = dataStore->vpn_strict_route;
#ifdef Q_OS_LINUX
            inboundObj["auto_redirect"] = true;
#endif
            auto tunAddress = QJsonArray{"172.19.0.1/24"};
            if (dataStore->vpn_ipv6) tunAddress += "fdfe:dcba:9876::1/96";
            inboundObj["address"] = tunAddress;
            if (dataStore->enable_tun_routing && routeChain->defaultOutboundID == proxyID)
            {
                if (!directIPCIDRs.isEmpty()) inboundObj["route_exclude_address"] = directIPCIDRs;
                if (!directIPSets.isEmpty()) inboundObj["route_exclude_address_set"] = directIPSets;
            }
            status->inbounds += inboundObj;
        }

        // ntp
        if (dataStore->enable_ntp) {
            QJsonObject ntpObj;
            ntpObj["enabled"] = true;
            ntpObj["server"] = dataStore->ntp_server_address;
            ntpObj["server_port"] = dataStore->ntp_server_port;
            ntpObj["interval"] = dataStore->ntp_interval;
            status->result->coreConfig["ntp"] = ntpObj;
        }

        // direct
        status->outbounds += QJsonObject{
            {"type", "direct"},
            {"tag", "direct"},
        };
        status->result->outboundStats += std::make_shared<Stats::TrafficData>("direct");

        // Hijack
        if (dataStore->enable_dns_server && !status->forTest)
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
        if (dataStore->enable_redirect && !status->forTest) {
            status->inbounds.prepend(QJsonObject{
                {"tag", "hijack"},
                {"type", "direct"},
                {"listen", dataStore->redirect_listen_address},
                {"listen_port", dataStore->redirect_listen_port},
            });
            auto sniffRule = std::make_shared<RouteRule>();
            sniffRule->action = "sniff";
            sniffRule->sniffOverrideDest = true;
            sniffRule->inbound = {"hijack"};
            routeChain->Rules.prepend(sniffRule);
        }

        // custom inbound
        if (!status->forTest) QJSONARRAY_ADD(status->inbounds, QString2QJsonObject(dataStore->custom_inbound)["inbounds"].toArray())

        // manage routing section
        auto routeObj = QJsonObject();
        if (dataStore->spmode_vpn) {
            routeObj["auto_detect_interface"] = true;
        }
        if (dataStore->enable_stats && !status->forTest)
        {
            routeObj["find_process"] = true;
        }
        if (!status->forTest) routeObj["final"] = outboundIDToString(routeChain->defaultOutboundID);

        if (!dataStore->routing->domain_strategy.isEmpty())
        {
            auto resolveRule = std::make_shared<RouteRule>();
            resolveRule->action = "resolve";
            resolveRule->strategy = dataStore->routing->domain_strategy;
            resolveRule->inbound = {"mixed-in", "tun-in"};
            routeChain->Rules.prepend(resolveRule);
        }
        if (dataStore->routing->sniffing_mode != SniffingMode::DISABLE)
        {
            auto sniffRule = std::make_shared<RouteRule>();
            sniffRule->action = "sniff";
            sniffRule->inbound = {"mixed-in", "tun-in"};
            routeChain->Rules.prepend(sniffRule);
        }
        auto neededOutbounds = routeChain->get_used_outbounds();
        auto neededRuleSets = routeChain->get_used_rule_sets();
        std::map<int, QString> outboundMap;
        outboundMap[-1] = "proxy";
        outboundMap[-2] = "direct";
        int suffix = 0;
        for (const auto &item: *neededOutbounds) {
            if (item < 0) continue;
            auto neededEnt = profileManager->GetProfile(item);
            if (neededEnt == nullptr) {
                status->result->error = "The routing profile is referencing outbounds that no longer exists, consider revising your settings";
                return;
            }
            QJsonObject currOutbound;
            QString tag = "rout-" + Int2String(suffix++);
            BuildOutbound(neededEnt, status, currOutbound, tag);
            if (neededEnt->type == "wireguard")
            {
                status->endpoints += currOutbound;
            } else
            {
                status->outbounds += currOutbound;
            }
            outboundMap[item] = tag;

            // add to dns direct resolve
            if (!IsIpAddress(neededEnt->bean->serverAddress)) {
                directDomains << neededEnt->bean->serverAddress;
                needDirectDnsRules = true;
            }
        }
        auto routeRules = routeChain->get_route_rules(false, outboundMap);
        routeObj["rules"] = routeRules;

        // DNS hijack deps
        QJsonArray hijackDomains;
        QJsonArray hijackDomainSuffix;
        QJsonArray hijackDomainRegex;
        QJsonArray hijackGeoAssets;

        if (dataStore->enable_dns_server && !status->forTest) {
            for (const auto& rule : dataStore->dns_server_rules) {
                if (rule.startsWith("ruleset:")) {
                    hijackGeoAssets << rule.mid(8);
                }
                if (rule.startsWith("domain:")) {
                    hijackDomains << rule.mid(7);
                }
                if (rule.startsWith("suffix:")) {
                    hijackDomainSuffix << rule.mid(7);
                }
                if (rule.startsWith("regex:")) {
                    hijackDomainRegex << rule.mid(6);
                }
            }
        }
        for (auto ruleSet : hijackGeoAssets) {
            if (!neededRuleSets->contains(ruleSet.toString())) neededRuleSets->append(ruleSet.toString());
        }

        auto ruleSetArray = QJsonArray();
        for (const auto &item: *neededRuleSets) {
            if(auto url = QUrl(item); url.isValid() && url.fileName().contains(".srs")) {
                ruleSetArray += QJsonObject{
                    {"type", "remote"},
                    {"tag", get_rule_set_name(item)},
                    {"format", "binary"},
                    {"url", item},
                };
            }
            else
                if(ruleSetMap.count(item.toStdString()) > 0) {
                    ruleSetArray += QJsonObject{
                        {"type", "remote"},
                        {"tag", item},
                        {"format", "binary"},
                        {"url", get_jsdelivr_link(QString::fromStdString(ruleSetMap.at(item.toStdString())))},
                    };
                }
        }
        routeObj["rule_set"] = ruleSetArray;

        // DNS settings
        QJsonObject dns;
        QJsonArray dnsServers;
        QJsonArray dnsRules;

        routeObj["default_domain_resolver"] = QJsonObject{
            {"server", "dns-direct"},
            {"strategy", dataStore->routing->outbound_domain_strategy},
        };

        // Remote
        if (status->ent->type == "tailscale")
        {
            auto tailDns = QJsonObject{
                {"type", "tailscale"},
                {"tag", "dns-remote"},
                {"endpoint", "proxy"},
                {"accept_default_resolvers", status->ent->TailscaleBean()->globalDNS},
            };
            dnsServers += tailDns;
        } else
        {
            auto remoteDnsObj = BuildDnsObject(dataStore->routing->remote_dns, dataStore->spmode_vpn);
            remoteDnsObj["tag"] = "dns-remote";
            remoteDnsObj["domain_resolver"] = "dns-local";
            remoteDnsObj["detour"] = tagProxy;
            dnsServers += remoteDnsObj;
        }

        // Direct
        auto directDNSAddress = dataStore->routing->direct_dns;
        auto directDnsObj = BuildDnsObject(directDNSAddress, dataStore->spmode_vpn);
        directDnsObj["tag"] = "dns-direct";
        directDnsObj["domain_resolver"] = "dns-local";

        // default dns server
        if (dataStore->routing->dns_final_out == "direct") {
            dnsServers.prepend(directDnsObj);
        } else {
            dnsServers.append(directDnsObj);
        }

        // Handle localhost
        dnsRules += QJsonObject{
            {"domain", "localhost"},
            {"action", "predefined"},
            {"query_type", "A"},
            {"rcode", "NOERROR"},
            {"answer", "localhost. IN A 127.0.0.1"},
        };

        dnsRules += QJsonObject{
            {"domain", "localhost"},
            {"action", "predefined"},
            {"query_type", "AAAA"},
            {"rcode", "NOERROR"},
            {"answer", "localhost. IN AAAA ::1"},
        };

        // Hijack
        if (dataStore->enable_dns_server && !status->forTest) {
            dnsRules += QJsonObject{
                {"rule_set", hijackGeoAssets},
                {"domain", hijackDomains},
                {"domain_suffix", hijackDomainSuffix},
                {"domain_regex", hijackDomainRegex},
                {"query_type", "A"},
                {"action", "predefined"},
                {"rcode", "NOERROR"},
                {"answer", QString("* IN A %1").arg(dataStore->dns_v4_resp)},
            };

            if (!dataStore->dns_v6_resp.isEmpty())
            {
                dnsRules += QJsonObject{
                    {"rule_set", hijackGeoAssets},
                    {"domain", hijackDomains},
                    {"domain_suffix", hijackDomainSuffix},
                    {"domain_regex", hijackDomainRegex},
                    {"query_type", "AAAA"},
                    {"action", "predefined"},
                    {"rcode", "NOERROR"},
                    {"answer", QString("* IN AAAA %1").arg(dataStore->dns_v6_resp)},
                };
            }

            status->inbounds.prepend(QJsonObject{
                {"tag", "dns-in"},
                {"type", "direct"},
                {"listen", dataStore->dns_server_listen_lan ? "0.0.0.0" : "127.0.0.1"},
                {"listen_port", dataStore->dns_server_listen_port},
            });
        }

        // Fakedns
        if (dataStore->fake_dns) {
            dnsServers += QJsonObject{
                {"tag", "dns-fake"},
                {"type", "fakeip"},
                {"inet4_range", "198.18.0.0/15"},
                {"inet6_range", "fc00::/18"},
            };
            dnsRules += QJsonObject{
                {"query_type", QJsonArray{
                                   "A",
                                   "AAAA"
                               }},
                {"action", "route"},
                {"server", "dns-fake"}
            };
            dns["independent_cache"] = true;
        }

        if (needDirectDnsRules) {
            dnsRules += QJsonObject{
                {"rule_set", directRuleSets},
                {"domain", directDomains},
                {"domain_suffix", directSuffixes},
                {"domain_keyword", directKeywords},
                {"domain_regex", directRegexes},
                {"action", "route"},
                {"server", "dns-direct"},
            };
        }

        // Underlying 100% Working DNS
        auto dnsLocalAddress = dataStore->core_box_underlying_dns.isEmpty() ? "local" : dataStore->core_box_underlying_dns;
        auto dnsLocalObj = BuildDnsObject(dnsLocalAddress, dataStore->spmode_vpn);
        dnsLocalObj["tag"] = "dns-local";
        dnsServers += dnsLocalObj;

        dns["servers"] = dnsServers;
        dns["rules"] = dnsRules;

        if (dataStore->routing->use_dns_object) {
            dns = QString2QJsonObject(dataStore->routing->dns_object);
        }

        // experimental
        QJsonObject experimentalObj;
        QJsonObject clash_api = {
            {"default_mode", ""} // dummy to make sure it is created
        };

        if (!status->forTest)
        {
            if (dataStore->core_box_clash_api > 0){
                clash_api = {
                {"external_controller", dataStore->core_box_clash_listen_addr + ":" + Int2String(dataStore->core_box_clash_api)},
                {"secret", dataStore->core_box_clash_api_secret},
                {"external_ui", "dashboard"},
                };
            }
            if (dataStore->core_box_clash_api > 0 || dataStore->enable_stats)
            {
                experimentalObj["clash_api"] = clash_api;
            }

            QJsonObject cache_file = {
                {"enabled", true},
                {"store_fakeip", true},
                {"store_rdrc", true}
            };
            experimentalObj["cache_file"] = cache_file;
        }

        status->result->coreConfig.insert("log", QJsonObject{{"level", dataStore->log_level}});
        status->result->coreConfig.insert("certificate", QJsonObject{{"store", dataStore->use_mozilla_certs ? "mozilla" : "system"}});
        status->result->coreConfig.insert("dns", dns);
        status->result->coreConfig.insert("inbounds", status->inbounds);
        status->result->coreConfig.insert("outbounds", status->outbounds);
        status->result->coreConfig.insert("endpoints", status->endpoints);
        status->result->coreConfig.insert("route", routeObj);
        if (!experimentalObj.isEmpty()) status->result->coreConfig.insert("experimental", experimentalObj);
    }
} // namespace Configs
