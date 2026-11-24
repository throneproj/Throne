#include "include/configs/generate.h"
#include "include/configs/generate_internal.h"
#include "include/api/RPC.h"
#include "include/configs/AutoSelectorPlan.h"
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
#include <srslist.h>

namespace {
    std::string_view ruleSetUrl(std::string_view key) {
        auto it = std::lower_bound(ruleSetList.begin(), ruleSetList.end(), key,
            [](const auto& e, std::string_view k) { return e.first < k; });
        return (it != ruleSetList.end() && it->first == key) ? it->second : std::string_view{};
    }

}

namespace Configs {
    using namespace gen;

    namespace {

        // The core points macOS's system resolver at this lo0 alias and moves the inbound behind a pf redirect from :53 (#2061).
        bool systemDnsInbound(const BuildContext &ctx) {
            return ctx.os == Darwin && ctx.tunEnabled && ctx.policy.listeners;
        }

        // preferred_by matches only during pre-match, leaving the copy inert at L4.
        QJsonObject l3BridgeTwin(const QJsonObject &rule) {
            auto twin = rule;
            twin["preferred_by"] = QJsonArray{tags::l3Direct};
            twin["action"] = "route";
            twin["outbound"] = tags::l3Direct;
            return twin;
        }

        // Twins must stay below the sniff rule, or UDP would reach them before it has a domain.
        QJsonArray withL3BridgeTwins(const QJsonArray &rules, QList<RouteRuleOrigin> &origins) {
            QJsonArray res;
            QList<RouteRuleOrigin> resOrigins;
            const bool tracked = origins.size() == rules.size();
            for (qsizetype i = 0; i < rules.size(); i++) {
                const auto rule = rules[i].toObject();
                const auto action = rule.value("action").toString();
                if ((action.isEmpty() || action == "route") && !rule.contains("preferred_by") &&
                    rule.value("outbound").toString() == tags::direct) {
                    res.append(l3BridgeTwin(rule));
                    if (tracked) resOrigins.append({RouteRuleOriginKind::L3Bridge, origins[i].profileRule, origins[i].name});
                }
                res.append(rule);
                if (tracked) resOrigins.append(origins[i]);
            }
            origins = tracked ? resOrigins : QList<RouteRuleOrigin>{};
            return res;
        }

    } // namespace

    namespace gen {

        // The core decodes route_exclude_address as netip.Prefix, so a bare address gets its full length; empty when unparsable.
        QString routeExcludePrefix(const QString &range) {
            const auto text = range.trimmed();
            const auto slash = text.indexOf(u'/');
            const auto addr = slash < 0 ? text : text.left(slash);
            const int maxBits = isCanonicalIPv4(addr) ? 32 : isIPv6Literal(addr, false) ? 128 : -1;
            if (maxBits < 0) return {};
            if (slash < 0) return addr + "/" + QString::number(maxBits);
            const auto bits = text.mid(slash + 1);
            if (!isCanonicalDecimal(bits, 3) || bits.toInt() > maxBits) return {};
            return text;
        }

    } // namespace gen

    namespace {

        struct RawPrefix {
            QByteArray addr;
            int bits = -1;
        };

        RawPrefix parsePrefix(const QString &cidr) {
            const auto parsed = QHostAddress::parseSubnet(cidr);
            if (parsed.second < 0) return {};
            RawPrefix prefix;
            prefix.bits = parsed.second;
            if (parsed.first.protocol() == QAbstractSocket::IPv4Protocol) {
                const auto v4 = parsed.first.toIPv4Address();
                prefix.addr.resize(4);
                for (int i = 0; i < 4; ++i) prefix.addr[i] = char((v4 >> (24 - 8 * i)) & 0xFF);
            } else if (parsed.first.protocol() == QAbstractSocket::IPv6Protocol) {
                const auto v6 = parsed.first.toIPv6Address();
                prefix.addr = QByteArray(reinterpret_cast<const char *>(v6.c), 16);
            } else {
                return {};
            }
            return prefix;
        }

        QString prefixToString(const RawPrefix &prefix) {
            QHostAddress addr;
            if (prefix.addr.size() == 4) {
                quint32 v4 = 0;
                for (int i = 0; i < 4; ++i) v4 = (v4 << 8) | quint8(prefix.addr[i]);
                addr = QHostAddress(v4);
            } else {
                Q_IPV6ADDR v6;
                for (int i = 0; i < 16; ++i) v6[i] = quint8(prefix.addr[i]);
                addr = QHostAddress(v6);
            }
            return addr.toString() + "/" + QString::number(prefix.bits);
        }

        bool prefixContains(const RawPrefix &outer, const RawPrefix &inner) {
            if (outer.bits < 0 || inner.bits < 0) return false;
            if (outer.addr.size() != inner.addr.size() || outer.bits > inner.bits) return false;
            const int wholeBytes = outer.bits / 8;
            if (outer.addr.left(wholeBytes) != inner.addr.left(wholeBytes)) return false;
            const int restBits = outer.bits % 8;
            if (restBits == 0) return true;
            const auto mask = quint8(0xFF << (8 - restBits));
            return (quint8(outer.addr[wholeBytes]) & mask) == (quint8(inner.addr[wholeBytes]) & mask);
        }

        // sing-tun offers no way to re-add a subtracted route_exclude_address, so partly-routed ranges are pre-split.
        QStringList subtractPrefix(const QStringList &ranges, const QString &hole) {
            const auto cut = parsePrefix(hole);
            if (cut.bits < 0) return ranges;
            QStringList out;
            for (const auto &entry : ranges) {
                const auto range = parsePrefix(entry);
                if (prefixContains(cut, range)) continue;
                if (!prefixContains(range, cut)) {
                    out << entry;
                    continue;
                }
                for (int bits = range.bits + 1; bits <= cut.bits; ++bits) {
                    RawPrefix sibling{cut.addr, bits};
                    const int flipped = bits - 1;
                    sibling.addr[flipped / 8] = char(quint8(sibling.addr[flipped / 8]) ^ quint8(0x80 >> (flipped % 8)));
                    for (int i = bits; i < int(sibling.addr.size()) * 8; ++i)
                        sibling.addr[i / 8] = char(quint8(sibling.addr[i / 8]) & ~quint8(0x80 >> (i % 8)));
                    out << prefixToString(sibling);
                }
            }
            return out;
        }

        QString genTunName() {
            auto tun_name = "throne-tun";
#ifdef Q_OS_MACOS
            tun_name = "";
#endif
            return tun_name;
        }

    } // namespace

    namespace gen {

        // ------------------------------------------------------------ inbounds

        void buildInboundSection(BuildContext &ctx) {
            if (!ctx.policy.listeners) return;
            const auto &settings = *dataManager->settingsRepo;
            const auto &tun = ctx.prerequisites.tun;
            QJsonArray inbounds;

            if (!settings.disable_mixed_inbound) {
                QJsonObject inboundObj;
                inboundObj["tag"] = tags::mixedIn;
                inboundObj["type"] = "mixed";
                inboundObj["listen"] = settings.inbound_address;
                inboundObj["listen_port"] = settings.inbound_socks_port;
                if (settings.inbound_auth) {
                    inboundObj["users"] = QJsonArray{
                        QJsonObject{
                                                {"username", settings.inbound_user},
                                                {"password", settings.inbound_pass}
                        }
                    };
                }
                inbounds += inboundObj;
            }

            if (settings.spmode_vpn) {
                QJsonObject inboundObj;
                inboundObj["tag"] = tags::tunIn;
                inboundObj["type"] = "tun";
                inboundObj["interface_name"] = genTunName();
                inboundObj["auto_route"] = true;
                inboundObj["mtu"] = settings.vpn_mtu;
                inboundObj["strict_route"] = settings.vpn_strict_route;
                if (ctx.os == Linux && settings.vpn_auto_redirect) inboundObj["auto_redirect"] = true;
                const auto tunIPv4CIDR = settings.vpn_tun_ipv4_cidr;
                const auto tunIPv6CIDR = settings.vpn_tun_ipv6_cidr;
                ctx.result->tunIPv4CIDR = tunIPv4CIDR;
                auto tunAddress = QJsonArray{tunIPv4CIDR};
                if (settings.vpn_ipv6) tunAddress += tunIPv6CIDR;
                inboundObj["address"] = tunAddress;

                QJsonArray routeExcludeAddrs;
                QStringList excludedRanges;
                if (!settings.disable_private_range_bypass) {
                    routeExcludeAddrs = {"127.0.0.0/8", "255.255.255.255/32"};
                    if (!tun.privateRangesAsRuleSet) excludedRanges = tun.bypassedPrivateRanges;
                }
                QJsonArray routeExcludeSets;
                if (tun.privateRangesAsRuleSet) routeExcludeSets << tags::privateRangesRuleSet;
                // The kill switch drops what leaves outside the Tun and the core, so direct IPs must reach the direct outbound (#2100).
                if (settings.enable_tun_routing && !settings.kill_switch)
                {
                    // The route rule drops a bare IPv6's zone (Prefixable), so the exclusion must too; anything unparsable fails that rule anyway.
                    for (const auto &item: tun.directIPCIDRs) {
                        auto text = item.toString().trimmed();
                        if (const auto zone = text.indexOf(u'%'); zone > 0 && text.left(zone).contains(u':') && !text.contains(u'/')) text.truncate(zone);
                        if (const auto prefix = routeExcludePrefix(text); !prefix.isEmpty()) excludedRanges << prefix;
                    }
                    for (auto item: tun.directIPSets) routeExcludeSets << item;
                }

                // On macOS a bypass covering the Tun subnet black-holes the system DNS (#1738).
                if (ctx.os == Darwin) {
                    excludedRanges = subtractPrefix(excludedRanges, tunIPv4CIDR);
                    if (settings.vpn_ipv6) excludedRanges = subtractPrefix(excludedRanges, tunIPv6CIDR);
                }
                for (const auto &range : excludedRanges) routeExcludeAddrs << range;
                // An IPv4-only Tun's strict route rejects every IPv6 flow, [::1] included; the static exclusion is checked first.
                if (ctx.os == Linux && settings.vpn_strict_route && !settings.vpn_ipv6) routeExcludeAddrs << "::1/128";
                inboundObj["route_exclude_address"] = routeExcludeAddrs;
                if (!routeExcludeSets.isEmpty()) inboundObj["route_exclude_address_set"] = routeExcludeSets;
                inbounds += inboundObj;
            }

            inbounds.prepend(QJsonObject{
                {"tag", tags::dnsIn},
                {"type", "direct"},
                {"listen", settings.core_dns_in_lan ? "::" : "127.0.0.1"},
                {"listen_port", settings.core_dns_in_port}
            });
            if (systemDnsInbound(ctx)) {
                inbounds.prepend(QJsonObject{
                    {"tag", tags::dnsSystem},
                    {"type", "direct"},
                    {"listen", "127.0.0.53"},
                    {"listen_port", 53},
                    {"reuse_addr", true},
                });
            }

            QJSONARRAY_ADD(inbounds, QString2QJsonObject(settings.custom_inbound)["inbounds"].toArray())
            ctx.result->coreConfig["inbounds"] = inbounds;
        }

    } // namespace gen

    namespace {

        // --------------------------------------------------------------- route

        QJsonArray buildRuleSetArray(const BuildContext &ctx) {
            QJsonArray ruleSetArray;
            for (const auto &item: ctx.prerequisites.routing.neededRuleSets) {
                if (auto url = QUrl(item); url.isValid() && url.fileName().contains(".srs")) {
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

            if (dataManager->settingsRepo->adblock_enable) {
                ruleSetArray += QJsonObject{
                            {"type", "remote"},
                            {"tag", tags::adblockRuleSet},
                            {"format", "binary"},
                            {"url", get_jsdelivr_link("https://raw.githubusercontent.com/217heidai/adblockfilters/main/rules/adblocksingbox.srs")},
                        };
            }

            if (const auto &tun = ctx.prerequisites.tun; tun.privateRangesAsRuleSet) {
                ruleSetArray += QJsonObject{
                            {"type", "inline"},
                            {"tag", tags::privateRangesRuleSet},
                            {"rules", QJsonArray{QJsonObject{{"ip_cidr", QJsonArray::fromStringList(tun.bypassedPrivateRanges)}}}},
                        };
            }
            return ruleSetArray;
        }

    } // namespace

    namespace gen {

        void buildRouteSection(BuildContext &ctx, const HopNeeds &needs, const QList<QString> &vpnAuxTags) {
            const auto &settings = *dataManager->settingsRepo;
            auto routeChain = dataManager->routesRepo->GetRouteProfile(ctx.routeId);
            if (routeChain == nullptr) {
                ctx.error = "Routing profile does not exist, try resetting the routing profile in Routing Settings";
                return;
            }
            routeChain = std::make_shared<RouteProfile>(*routeChain);
            const auto &routeDeps = ctx.prerequisites.routing;

            QJsonObject rawRouteObj;
            if (routeChain->isRaw) {
                rawRouteObj = QString2QJsonObject(routeChain->rawRoute);
                if (rawRouteObj.isEmpty()) {
                    ctx.error = "Raw routing profile is not a valid JSON object";
                    return;
                }
                rawRouteObj = RouteProfile::TranslateRawOutbounds(rawRouteObj, routeDeps.outboundMap);
                if (routeChain->preventModifications) {
                    auto rawRules = rawRouteObj.value("rules").toArray();
                    QList<RouteRuleOrigin> origins;
                    for (qsizetype i = 0; i < rawRules.size(); i++) origins.append({RouteRuleOriginKind::RawRule, static_cast<int>(i), {}});
                    // The raw JSON is verbatim, but endpoint tags are internal so the user cannot name them.
                    if (!vpnAuxTags.isEmpty()) {
                        for (const auto &auxTag : vpnAuxTags) {
                            rawRules.append(QJsonObject{
                                {"preferred_by", QJsonArray{auxTag}},
                                {"action", "route"},
                                {"outbound", auxTag},
                            });
                            origins.append({RouteRuleOriginKind::EndpointGate, -1, auxTag});
                        }
                        rawRouteObj["rules"] = rawRules;
                    }
                    ctx.result->routeRuleOrigins = origins;
                    ctx.result->coreConfig["route"] = rawRouteObj;
                    return;
                }
            }

            struct InjectedRules {
                QJsonObject sniff;
                QJsonObject resolve;
                QJsonObject dnsHijack;
                QJsonObject dnsInReject;
                QJsonObject dnsSystem;
            } injected;

            if (systemDnsInbound(ctx)) {
                injected.dnsSystem = QJsonObject{
                    {"inbound", tags::dnsSystem},
                    {"action", "hijack-dns"},
                };
            }

            if (!routeChain->isRaw) {
                injected.sniff = QJsonObject{{"action", "sniff"}};
                if (!settings.resolve_domain_strategy.isEmpty()) {
                    injected.resolve = QJsonObject{
                        {"inbound", QJsonArray{tags::mixedIn, tags::tunIn}},
                        {"action", "resolve"},
                        {"strategy", settings.resolve_domain_strategy},
                    };
                }
                injected.dnsHijack = QJsonObject{
                    {"protocol", "dns"},
                    {"action", "hijack-dns"},
                };
            }
            if (ctx.policy.listeners) {
                injected.dnsInReject = QJsonObject{
                    {"inbound", tags::dnsIn},
                    {"action", "reject"},
                };
            }

            QList<int> sourceIndexes;
            auto profileRules = routeChain->isRaw ? rawRouteObj.value("rules").toArray()
                                                  : routeChain->get_route_rules(false, routeDeps.outboundMap, &sourceIndexes);
            QList<RouteRuleOrigin> profileOrigins;
            if (routeChain->isRaw) {
                for (qsizetype i = 0; i < profileRules.size(); i++) profileOrigins.append({RouteRuleOriginKind::RawRule, static_cast<int>(i), {}});
            } else if (sourceIndexes.size() == profileRules.size()) {
                for (const int index : sourceIndexes) {
                    if (index < 0) profileOrigins.append({RouteRuleOriginKind::AdBlock, -1, {}});
                    else profileOrigins.append({RouteRuleOriginKind::ProfileRule, index, routeChain->Rules.at(index)->name});
                }
            }
            if (ctx.l3Bridge) profileRules = withL3BridgeTwins(profileRules, profileOrigins);

            QJsonObject extraCoreDirect;
            if (!ctx.result->extraCoreData->path.isEmpty())
            {
                extraCoreDirect = QJsonObject{
                    {"action", "route"},
                    {"process_path", extraCoreProcessPaths(ctx.result->extraCoreData->path)},
                    {"outbound", tags::direct},
                };
            }

            auto ruleSetArray = buildRuleSetArray(ctx);

            const auto bridgeRules = ctx.bridges.singRouteRules(BridgeTagScheme::ByIngress);

            // raw profiles bring their own rule_set definitions; merge them after ours.
            if (routeChain->isRaw) {
                for (const auto& rs : rawRouteObj.value("rule_set").toArray()) ruleSetArray.append(rs);
            }

            const int defOut = routeChain->defaultOutboundID;
            const QString finalTag = routeChain->isRaw
                ? (rawRouteObj.contains("final") ? rawRouteObj.value("final").toString() : QString(tags::proxy))
                : defOut == blockID       ? QString(tags::direct)
                : defOut == warpBypassID  ? QString(exitWrapperOn() ? tags::warpBypass : tags::proxy)
                                          : outboundIDToString(defOut);

            // An endpoint rule the user positioned already emitted this tag's gate.
            auto carriesGate = [](const QJsonArray &rules, const QString &tag) {
                for (const auto &r : rules) {
                    for (const auto &p : r.toObject().value("preferred_by").toArray())
                        if (p.toString() == tag) return true;
                }
                return false;
            };
            QJsonArray vpnAuxRules;
            for (const auto &auxTag : vpnAuxTags) {
                if (!routeChain->isRaw && carriesGate(profileRules, auxTag)) continue;
                vpnAuxRules.append(QJsonObject{
                    {"preferred_by", QJsonArray{auxTag}},
                    {"action", "route"},
                    {"outbound", auxTag},
                });
            }

            // defOut == blockID also yields finalTag "direct", but rejects before reaching it.
            QJsonArray l3BridgeFinalRules;
            if (ctx.l3Bridge && finalTag == tags::direct && (routeChain->isRaw || defOut == directID)) {
                l3BridgeFinalRules.append(QJsonObject{
                    {"preferred_by", QJsonArray{tags::l3Direct}},
                    {"action", "route"},
                    {"outbound", tags::l3Direct},
                });
            }

            QJsonArray vpnFallthroughRules;
            if (ctx.policy.fullRouting && needs.vpnGateTags.contains(finalTag)) {
                // A hostname destination has no address for preferred_by to match until it is resolved.
                QJsonObject gateResolve{{"action", "resolve"}};
                if (!settings.resolve_domain_strategy.isEmpty()) gateResolve["strategy"] = settings.resolve_domain_strategy;
                vpnFallthroughRules.append(gateResolve);
                vpnFallthroughRules.append(QJsonObject{
                    {"preferred_by", QJsonArray{finalTag}},
                    {"action", "route"},
                    {"outbound", finalTag},
                });
                vpnFallthroughRules.append(QJsonObject{{"action", "reject"}});
            }

            using Kind = RouteRuleOriginKind;
            QJsonArray routeRules;
            QList<RouteRuleOrigin> origins;
            auto append = [&routeRules, &origins](const QJsonValue &r, Kind kind, const QString &name = {}) {
                routeRules.append(r);
                origins.append({kind, -1, name});
            };
            for (const auto& r : bridgeRules) append(r, Kind::XrayBridge);
            if (!extraCoreDirect.isEmpty()) append(extraCoreDirect, Kind::ExtraCore);
            auto appendIfSet = [&append](const QJsonObject& r, Kind kind) { if (!r.isEmpty()) append(r, kind); };
            appendIfSet(injected.dnsSystem, Kind::SystemDns);
            appendIfSet(injected.sniff, Kind::Sniff);
            appendIfSet(injected.resolve, Kind::Resolve);
            appendIfSet(injected.dnsHijack, Kind::DnsHijack);
            appendIfSet(injected.dnsInReject, Kind::DnsInboundReject);
            const bool originsKnown = profileOrigins.size() == profileRules.size();
            for (const auto& r : profileRules) routeRules.append(r);
            origins.append(profileOrigins);
            for (const auto& r : vpnAuxRules) append(r, Kind::EndpointGate, r.toObject().value("outbound").toString());
            for (const auto& r : l3BridgeFinalRules) append(r, Kind::L3Bridge);
            // final still names the tunnel, but nothing may reach it unmatched.
            for (const auto& r : vpnFallthroughRules) append(r, Kind::TunnelGate, finalTag);
            if (!routeChain->isRaw && defOut == blockID) {
                append(QJsonObject{{"action", "reject"}}, Kind::BlockFinal);
            }
            if (originsKnown && origins.size() == routeRules.size()) ctx.result->routeRuleOrigins = origins;

            QJsonObject route = routeChain->isRaw ? rawRouteObj : QJsonObject{};
            route["rules"] = routeRules;
            route["rule_set"] = ruleSetArray;
            if (routeChain->isRaw) {
                if (!route.contains("final")) route["final"] = tags::proxy;
            } else {
                route["final"] = finalTag;
            }
            if (settings.enable_stats && !route.contains("find_process"))  route["find_process"] = true;
            if (!route.contains("default_domain_resolver"))
                route["default_domain_resolver"] = directDomainResolver();
            if (settings.spmode_vpn && !route.contains("auto_detect_interface")) route["auto_detect_interface"] = true;

            ctx.result->coreConfig["route"] = route;
        }

    } // namespace gen
}
