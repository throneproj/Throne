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

namespace Configs {
    using namespace gen;

    namespace gen {

        bool isCanonicalDecimal(const QString &text, int maxDigits) {
            if (text.isEmpty() || text.size() > maxDigits) return false;
            if (text.size() > 1 && text.startsWith(u'0')) return false;
            return std::all_of(text.cbegin(), text.cend(), [](QChar c) { return c.unicode() >= '0' && c.unicode() <= '9'; });
        }

        // Go netip: QHostAddress also takes "10.1" or "010.0.0.1", which the core refuses.
        bool isCanonicalIPv4(const QString &text) {
            const auto octets = text.split(u'.');
            return octets.size() == 4 && std::all_of(octets.cbegin(), octets.cend(),
                [](const QString &octet) { return isCanonicalDecimal(octet, 3) && octet.toInt() <= 255; });
        }

        bool isIPv6Literal(const QString &text, bool allowZone) {
            if (!text.contains(u':') || (!allowZone && text.contains(u'%'))) return false;
            return QHostAddress(text).protocol() == QAbstractSocket::IPv6Protocol;
        }

        // ------------------------------------------------------- json fragments

        // sing-box matches process_path against the OS-native form.
        QJsonArray extraCoreProcessPaths(const QString &corePath) {
            auto path = corePath;
#ifdef Q_OS_WIN
            path.replace("/", "\\");
#endif
            return QJsonArray{path};
        }

    } // namespace gen

    namespace {

        // The guard answers empty instead of falling through, which would hand the query to another server.
        // A role tag (dns-remote, dns-direct) -> its servers, lead first; the lead keeps the plain tag that resolvers pin.
        struct DnsRace {
            QHash<QString, QStringList> members;
            int groups = 0;
        };

        QStringList dnsRoleTags(const QString &lead, qsizetype count) {
            QStringList res{lead};
            for (int i = 1; i < count; ++i) res << hopTag(lead, i);
            return res;
        }

        void appendDnsRoute(QJsonArray &rules, const QJsonObject &conditions, const QString &server,
                            bool disableIPv6, DnsRace &race) {
            if (disableIPv6) {
                auto guard = conditions;
                guard["query_type"] = QJsonArray{"AAAA"};
                guard["action"] = "predefined";
                rules += guard;
            }
            const auto members = race.members.value(server);
            if (members.size() < 2) {
                auto route = conditions;
                route["action"] = "route";
                route["server"] = server;
                rules += route;
                return;
            }
            // Evaluates precede the race rules: an armed race rule would hold back each later non-speculative evaluate.
            const auto prefix = QString("race%1-").arg(race.groups++);
            for (qsizetype i = 0; i < members.size(); ++i) {
                auto evaluate = conditions;
                evaluate["action"] = "evaluate";
                evaluate["server"] = members[i];
                evaluate["tag"] = prefix + QString::number(i);
                rules += evaluate;
            }
            for (const auto *rcode : {"NOERROR", "NXDOMAIN"}) {
                for (qsizetype i = 0; i < members.size(); ++i) {
                    rules += QJsonObject{
                        {"match_response", prefix + QString::number(i)},
                        {"response_rcode", rcode},
                        {"action", "respond"},
                        {"race", true},
                    };
                }
            }
            // Held until every race rule missed; without it the query would fall through to a later rule's server.
            auto fail = conditions;
            fail["action"] = "predefined";
            fail["rcode"] = "SERVFAIL";
            rules += fail;
        }

        void appendDnsRoutingRules(QJsonArray &rules, const DomainSelectors &selectors,
                                   const QString &server, bool disableIPv6, DnsRace &race) {
            if (!selectors.ruleSets.isEmpty()) {
                appendDnsRoute(rules, QJsonObject{{"rule_set", selectors.ruleSets}}, server, disableIPv6, race);
            }
            if (selectors.hasInlineConditions()) {
                appendDnsRoute(rules, QJsonObject{
                    {"domain", selectors.domains},
                    {"domain_suffix", selectors.suffixes},
                    {"domain_keyword", selectors.keywords},
                    {"domain_regex", selectors.regexes},
                }, server, disableIPv6, race);
            }
        }

    } // namespace

    namespace gen {

        // ---------------------------------------------------------------- dns

        QJsonObject directDomainResolver() {
            return {{"server", tags::dnsDirect}, {"strategy", getDirectDomainStrategy()}};
        }

    } // namespace gen

    namespace {

        // The pre-ParseDnsAddress split, kept for refused addresses so a saved value builds as it always did.
        QJsonObject legacyDnsObj(QString address) {
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
            if (address.startsWith("udp://")) addr = addr.mid(6);
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

        QJsonObject buildDnsObj(BuildContext &ctx, const QString &address) {
            DnsAddress parsed;
            QString why;
            if (!ParseDnsAddress(address, &parsed, &why)) {
                // Reported by real builds only: test builds repeat for every probe batch.
                if (ctx.policy.reportBadDns) MW_show_log(QObject::tr("DNS address \"%1\" is invalid: %2").arg(address, why));
                return legacyDnsObj(address);
            }
            if (parsed.type == "local") {
                // The kill switch drops resolved's own upstream queries; a probe box outlives Tun toggles, and its `local` would follow resolved's `~.` into a running tun (#1975).
                if (ctx.isResolvedUsed && (ctx.tunEnabled || ctx.policy.dnsAsIfTun || dataManager->settingsRepo->kill_switch)) {
                    return {{"type", "underlying"}};
                }
                // A macOS Tun points the system resolver at its dns-system inbound, which `local` would query and loop through (#2045).
                if (ctx.os == Darwin && (ctx.tunEnabled || ctx.policy.dnsAsIfTun)) {
                    return {{"type", "underlying"}};
                }
                return {{"type", "local"}};
            }
            if (parsed.type == "dhcp") {
                return {
                    {"type", "dhcp"},
                    {"interface", parsed.iface},
                };
            }
            QJsonObject res = {
                {"type", parsed.type},
                {"server", parsed.server},
            };
            if (parsed.port > 0) res["server_port"] = parsed.port;
            if (!parsed.path.isEmpty()) res["path"] = parsed.path;
            return res;
        }

        QString upgradeUdpDnsToDoH(const QString &server) {
            static const QMap<QString, QString> known = {
                // Google
                {"8.8.8.8", "https://8.8.8.8/dns-query"},
                {"8.8.4.4", "https://8.8.4.4/dns-query"},
                // Cloudflare
                {"1.1.1.1", "https://1.1.1.1/dns-query"},
                {"1.0.0.1", "https://1.0.0.1/dns-query"},
                {"1.1.1.2", "https://1.1.1.2/dns-query"},
                {"1.0.0.2", "https://1.0.0.2/dns-query"},
                {"1.1.1.3", "https://1.1.1.3/dns-query"},
                {"1.0.0.3", "https://1.0.0.3/dns-query"},
                // Quad9
                {"9.9.9.9", "https://9.9.9.9/dns-query"},
                {"149.112.112.112", "https://149.112.112.112/dns-query"},
                // AdGuard
                {"94.140.14.14", "https://94.140.14.14/dns-query"},
                {"94.140.15.15", "https://94.140.15.15/dns-query"},
            };
            return known.value(server, "https://8.8.8.8/dns-query");
        }

    } // namespace

    namespace gen {

        void buildDNSSection(BuildContext &ctx, const HopNeeds &needs, const QList<QString> &vpnAuxTags, bool useDnsObj) {
            const auto &settings = *dataManager->settingsRepo;
            if (!ctx.routeOwnsDns && settings.use_dns_object && useDnsObj) {
                ctx.result->coreConfig["dns"] = QString2QJsonObject(settings.dns_object);
                return;
            }

            const auto &dns = ctx.prerequisites.dns;
            bool isTailscale = ctx.ent->type == "tailscale";
            bool independentCache = false;
            QJsonArray servers;
            QJsonArray rules;
            // Merged in front of `rules` at the end.
            QJsonArray headRules;
            DnsRace race;
            // An empty setting still reaches buildDnsObj, which reports it.
            const auto roleAddresses = [](const QString &text) {
                auto res = SplitDnsAddresses(text);
                return res.isEmpty() ? QStringList{text} : res;
            };
            const auto directAddresses = roleAddresses(ctx.dns.direct_dns);
            race.members[tags::dnsDirect] = dnsRoleTags(tags::dnsDirect, directAddresses.size());
            if (ctx.policy.fullDns) {
                const auto remoteAddresses = roleAddresses(ctx.dns.remote_dns);
                const auto remoteTags = dnsRoleTags(tags::dnsRemote, remoteAddresses.size());
                race.members[tags::dnsRemote] = remoteTags;
                for (qsizetype i = 0; i < remoteAddresses.size(); ++i) {
                    auto remoteDnsObj = buildDnsObj(ctx, remoteAddresses[i]);
                    // Xray handles UDP DNS poorly.
                    if (ctx.proxyUsesXray && ( remoteDnsObj.value("type").toString() == "udp" || remoteDnsObj.value("type").toString() == "quic" )) {
                        remoteDnsObj = buildDnsObj(ctx, upgradeUdpDnsToDoH(remoteDnsObj.value("server").toString()));
                    }
                    remoteDnsObj["tag"] = remoteTags[i];
                    remoteDnsObj["domain_resolver"] = tags::dnsLocal;
                    remoteDnsObj["detour"] = tags::proxy;
                    servers += remoteDnsObj;
                }

                if (isTailscale)
                {
                    appendDnsRoute(rules, QJsonObject{
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
                    }, tags::dnsDirect, false, race);

                    auto tailscale = ctx.ent->Tailscale();
                    if (tailscale != nullptr)
                    {
                        servers += QJsonObject{
                            {"type", "tailscale"},
                            {"tag", tags::dnsTailscale},
                            {"endpoint", tags::proxy},
                            {"accept_default_resolvers", tailscale->globalDNS},
                        };

                        rules += QJsonObject{
                            {"domain_suffix", QJsonArray{"ts.net", "tailscale.net"}},
                            {"action", "route"},
                            {"server", tags::dnsTailscale},
                        };
                    }
                }
            }

            const auto directTags = race.members.value(tags::dnsDirect);
            for (qsizetype i = 0; i < directAddresses.size(); ++i) {
                auto directDnsObj = buildDnsObj(ctx, directAddresses[i]);
                directDnsObj["tag"] = directTags[i];
                directDnsObj["domain_resolver"] = tags::dnsLocal;
                servers.append(directDnsObj);
            }

            if (ctx.policy.fullDns && settings.dns_predefined_enable) {
                QList<PredefinedDNSEntry> predefined;
                if (!ParsePredefinedDNS(settings.dns_predefined_rules, predefined)) predefined.clear();

                // "*." is rewritten to the queried name by the core; a literal owner would have to parse as a zone name.
                auto emitFamily = [&](const QString &domain, const QStringList &addrs, const QString &type) {
                    // Refused rather than passed through, else the other family defeats the override.
                    if (addrs.isEmpty()) {
                        headRules += QJsonObject{
                            {"domain", domain},
                            {"action", "predefined"},
                            {"query_type", type},
                            {"rcode", "NXDOMAIN"},
                        };
                        return;
                    }
                    QJsonArray answers;
                    for (const auto &addr : addrs) answers += QString("*. IN %1 %2").arg(type, addr);
                    headRules += QJsonObject{
                        {"domain", domain},
                        {"action", "predefined"},
                        {"query_type", type},
                        {"rcode", "NOERROR"},
                        {"answer", answers},
                    };
                };

                for (const auto &entry : predefined) {
                    emitFamily(entry.domain, entry.v4, "A");
                    emitFamily(entry.domain, entry.v6, "AAAA");
                }
            }

            if (ctx.policy.fullDns && settings.dns_use_hosts) {
                servers += QJsonObject{{"tag", tags::dnsHosts}, {"type", "hosts"}};
                // The transport NXDOMAINs whatever it cannot answer, hence the preferred_by gate and query_type limit.
                headRules += QJsonObject{
                    {"preferred_by", QJsonArray{tags::dnsHosts}},
                    {"query_type", QJsonArray{"A", "AAAA"}},
                    {"action", "route"},
                    {"server", tags::dnsHosts},
                    {"disable_cache", true},
                };
            }

            if (ctx.policy.fullDns && !needs.echQueryNames.isEmpty()) {
                appendDnsRoute(headRules, QJsonObject{
                    {"domain", QJsonArray::fromStringList(needs.echQueryNames)},
                    {"query_type", QJsonArray{"HTTPS"}},
                }, tags::dnsDirect, false, race);
            }

            if (ctx.policy.fullDns && !needs.dnsTunnelDomains.isEmpty()) {
                appendDnsRoute(headRules, QJsonObject{
                    {"domain_suffix", QJsonArray::fromStringList(needs.dnsTunnelDomains)},
                }, tags::dnsDirect, false, race);
            }

            // Strict tunnel DNS takes every query that would otherwise go to dns-remote.
            QString remoteDnsTag = tags::dnsRemote;
            if (ctx.policy.fullDns) {
                // Auxiliary tunnels in routing-rule order; a tag-keyed walk would put aux-10 before aux-2.
                QList<QString> vpnDnsOrder;
                for (const auto &tag : vpnAuxTags) {
                    if (needs.vpnEndpointTags.contains(tag)) vpnDnsOrder << tag;
                }
                for (auto it = needs.vpnEndpointTags.cbegin(); it != needs.vpnEndpointTags.cend(); ++it) {
                    if (!vpnDnsOrder.contains(it.key())) vpnDnsOrder << it.key();
                }
                for (const auto &endpointTag : vpnDnsOrder) {
                    const auto dnsTag = QString(tags::dnsVpnPrefix) + "-" + endpointTag;
                    QJsonObject server{
                        {"tag", dnsTag},
                        {"type", needs.vpnEndpointTags.value(endpointTag)},
                        {"endpoint", endpointTag},
                    };
                    if (endpointTag == needs.vpnStrictTag) {
                        server["accept_default_resolvers"] = true;
                        server["accept_search_domain"] = true;
                        remoteDnsTag = dnsTag;
                    }
                    servers += server;
                    headRules += QJsonObject{
                        {"preferred_by", QJsonArray{dnsTag}},
                        {"action", "route"},
                        {"server", dnsTag},
                    };
                }
            }

            // No dns-in carve-out: Xray resolves against dns-direct in-process now, so a query on that port is an ordinary local one.

            if (ctx.policy.fullDns && !ctx.result->extraCoreData->path.isEmpty())
            {
                appendDnsRoute(rules, QJsonObject{{"process_path", extraCoreProcessPaths(ctx.result->extraCoreData->path)}},
                               tags::dnsDirect, ctx.dns.direct_dns_disable_ipv6, race);
            }

            if (ctx.dns.fake_dns) {
                QJsonObject fakeServer{
                        {"tag", tags::dnsFake},
                        {"type", "fakeip"},
                        {"inet4_range", "198.18.0.0/15"},
                    };
                // No inet6_range makes the transport answer AAAA empty itself; the rule stays on both types.
                // Not fc00::/18: the Tun's fc00::/7 private-range bypass would route fake addresses outside it.
                if (!ctx.dns.fakeip_disable_ipv6) fakeServer["inet6_range"] = "2001:db8::/32";
                servers += fakeServer;
                rules += QJsonObject{
                        {"query_type", QJsonArray{
                            "A",
                            "AAAA"
                        }},
                     {"action", "route"},
                     {"server", tags::dnsFake}
                };
                independentCache = true;
            }

            if (dns.needDirectDnsRules) {
                appendDnsRoutingRules(rules, dns.direct, tags::dnsDirect, ctx.dns.direct_dns_disable_ipv6, race);
            }

            // A test box builds no dns-remote server at all, so its fall-through goes out direct.
            const bool useDirectFinalDNS = !ctx.policy.fullDns || ctx.dns.dns_final_out == tags::direct;

            if (ctx.policy.fullDns && dns.needProxyDnsRules && useDirectFinalDNS) {
                appendDnsRoutingRules(rules, dns.proxy, remoteDnsTag, ctx.dns.remote_dns_disable_ipv6, race);
            }

            appendDnsRoute(rules, QJsonObject{}, useDirectFinalDNS ? QString(tags::dnsDirect) : remoteDnsTag,
                           useDirectFinalDNS ? ctx.dns.direct_dns_disable_ipv6 : ctx.dns.remote_dns_disable_ipv6, race);

            auto dnsLocalAddress = settings.core_box_underlying_dns.isEmpty() ? "local" : settings.core_box_underlying_dns;
            auto dnsLocalObj = buildDnsObj(ctx, dnsLocalAddress);
            dnsLocalObj["tag"] = tags::dnsLocal;
            servers += dnsLocalObj;

            int echDnsIdx = 0;
            for (auto it = needs.echResolvers.cbegin(); it != needs.echResolvers.cend(); ++it) {
                const QString tag = hopTag(tags::dnsEchPrefix, echDnsIdx++);

                auto echDnsObj = buildDnsObj(ctx, it.value());
                echDnsObj["tag"] = tag;
                echDnsObj["domain_resolver"] = tags::dnsLocal;
                servers.append(echDnsObj);

                headRules.prepend(QJsonObject{
                    {"domain", QJsonArray{it.key()}},
                    {"query_type", QJsonArray{"HTTPS"}},
                    {"action", "route"},
                    {"server", tag},
                });
            }

            if (!headRules.isEmpty()) {
                for (const auto &rule : rules) headRules.append(rule);
                rules = headRules;
            }

            auto dnsObj = QJsonObject{
                {"servers", servers},
                {"rules", rules},
                {"cache_capacity", settings.dns_cache_capacity},
            };
            if (settings.dns_disable_cache) dnsObj["disable_cache"] = true;
            if (settings.dns_disable_expire) dnsObj["disable_expire"] = true;
            if (settings.dns_reverse_mapping) dnsObj["reverse_mapping"] = true;
            if (independentCache) dnsObj["independent_cache"] = true;
            if (!settings.dns_query_timeout.isEmpty()) dnsObj["timeout"] = settings.dns_query_timeout;
            // The core refuses the config outright when optimistic meets either cache switch.
            if (settings.dns_optimistic && !settings.dns_disable_cache && !settings.dns_disable_expire) {
                if (settings.dns_optimistic_timeout.isEmpty()) dnsObj["optimistic"] = true;
                else dnsObj["optimistic"] = QJsonObject{
                    {"enabled", true},
                    {"timeout", settings.dns_optimistic_timeout},
                };
            }
            ctx.result->coreConfig["dns"] = dnsObj;
        }

    } // namespace gen

    bool ParsePredefinedDNS(const QStringList& lines, QList<PredefinedDNSEntry>& out, QString* error) {
        QMap<QString, int> indexOf;
        for (const auto& rawLine : lines) {
            auto line = rawLine;
            if (const auto hash = line.indexOf('#'); hash != -1) line = line.left(hash);
            const auto fields = line.simplified().split(' ', Qt::SkipEmptyParts);
            if (fields.isEmpty()) continue;

            QHostAddress addr;
            if (fields.size() < 2 || !addr.setAddress(fields[0])) {
                if (error != nullptr) *error = rawLine.trimmed();
                return false;
            }
            addr.setScopeId({});
            const bool isV6 = addr.protocol() == QAbstractSocket::IPv6Protocol;

            for (qsizetype i = 1; i < fields.size(); i++) {
                auto domain = fields[i].toLower();
                while (domain.endsWith('.')) domain.chop(1);
                if (domain.isEmpty()) {
                    if (error != nullptr) *error = rawLine.trimmed();
                    return false;
                }
                if (!indexOf.contains(domain)) {
                    indexOf[domain] = static_cast<int>(out.size());
                    out.append(PredefinedDNSEntry{domain, {}, {}});
                }
                auto& bucket = isV6 ? out[indexOf[domain]].v6 : out[indexOf[domain]].v4;
                if (const auto text = addr.toString(); !bucket.contains(text)) bucket.append(text);
            }
        }
        return true;
    }

    bool IsValidDuration(const QString& text) {
        return Validation::Duration(text, {Validation::Empty::Required}).isEmpty();
    }

    bool ParseDnsAddress(const QString &text, DnsAddress *out, QString *why) {
        const auto fail = [why](const QString &reason) {
            if (why != nullptr) *why = reason;
            return false;
        };
        const QString address = text.trimmed();
        if (address.isEmpty()) return fail(QCoreApplication::translate("Validation", "The DNS address is empty."));

        DnsAddress res;
        if (address.startsWith("local", Qt::CaseInsensitive)) {
            res.type = "local";
            if (out != nullptr) *out = res;
            return true;
        }

        const auto schemeEnd = address.indexOf("://");
        const QString scheme = schemeEnd < 0 ? QString("udp") : address.left(schemeEnd).toLower();
        QString rest = schemeEnd < 0 ? address : address.mid(schemeEnd + 3);

        if (scheme == "dhcp") {
            static const QRegularExpression ifaceRe(R"(^[^\s/]+$)");
            if (rest.isEmpty())
                return fail(QCoreApplication::translate("Validation", "Name an interface after dhcp://, or use dhcp://auto."));
            if (!ifaceRe.match(rest).hasMatch())
                return fail(QCoreApplication::translate("Validation", "\"%1\" is not a valid interface name.").arg(rest));
            res.type = "dhcp";
            if (rest.compare("auto", Qt::CaseInsensitive) != 0) res.iface = rest;
            if (out != nullptr) *out = res;
            return true;
        }

        static const QStringList plainSchemes = {"udp", "tcp", "tls", "quic"};
        static const QStringList pathSchemes = {"https", "h3"};
        if (!plainSchemes.contains(scheme) && !pathSchemes.contains(scheme))
            return fail(QCoreApplication::translate("Validation", "\"%1\" is not a supported DNS scheme; use udp, tcp, tls, quic, https, h3, dhcp or local.").arg(scheme));
        res.type = scheme;

        if (const auto slash = rest.indexOf(u'/'); slash >= 0) {
            if (!pathSchemes.contains(scheme))
                return fail(QCoreApplication::translate("Validation", "Only https:// and h3:// DNS addresses take a path."));
            res.path = rest.mid(slash);
            rest.truncate(slash);
        }

        QString host = rest;
        QString portText;
        bool hasPort = false;
        if (rest.startsWith(u'[')) {
            const auto close = rest.indexOf(u']');
            if (close < 0) return fail(QCoreApplication::translate("Validation", "The IPv6 address is missing its closing bracket."));
            host = rest.mid(1, close - 1);
            const auto tail = rest.mid(close + 1);
            if (!tail.isEmpty()) {
                if (!tail.startsWith(u':'))
                    return fail(QCoreApplication::translate("Validation", "Only a port may follow a bracketed IPv6 address."));
                portText = tail.mid(1);
                hasPort = true;
            }
            if (!isIPv6Literal(host, true))
                return fail(QCoreApplication::translate("Validation", "\"%1\" is not a valid IPv6 address.").arg(host));
        } else if (rest.count(u':') > 1) {
            // A bare IPv6 cannot carry a port: its last group would read as one.
            if (!isIPv6Literal(rest, true))
                return fail(QCoreApplication::translate("Validation", "\"%1\" is not a valid IPv6 address; put it in brackets to add a port.").arg(rest));
        } else {
            if (const auto colon = rest.indexOf(u':'); colon >= 0) {
                host = rest.left(colon);
                portText = rest.mid(colon + 1);
                hasPort = true;
            }
            if (host.isEmpty()) return fail(QCoreApplication::translate("Validation", "The DNS server address is missing."));
            static const QRegularExpression ipv4LikeRe(R"(^[0-9.]+$)");
            static const QRegularExpression hostnameRe(
                R"(^(?=.{1,253}\.?$)[A-Za-z0-9_](?:[A-Za-z0-9_-]{0,61}[A-Za-z0-9_])?(?:\.[A-Za-z0-9_](?:[A-Za-z0-9_-]{0,61}[A-Za-z0-9_])?)*\.?$)");
            if (ipv4LikeRe.match(host).hasMatch()) {
                if (!isCanonicalIPv4(host))
                    return fail(QCoreApplication::translate("Validation", "\"%1\" is not a valid IPv4 address.").arg(host));
            } else if (!hostnameRe.match(toAceHost(host)).hasMatch()) {
                return fail(QCoreApplication::translate("Validation", "\"%1\" is not a valid IP address or host name.").arg(host));
            }
        }

        if (hasPort) {
            if (portText.isEmpty()) return fail(QCoreApplication::translate("Validation", "The port is missing after the colon."));
            static const QRegularExpression portRe(R"(^[0-9]{1,5}$)");
            const int port = portRe.match(portText).hasMatch() ? portText.toInt() : 0;
            if (port < 1 || port > 65535)
                return fail(QCoreApplication::translate("Validation", "\"%1\" is not a valid port; use a number from 1 to 65535.").arg(portText));
            res.port = port;
        }
        res.server = toAceHost(host);
        if (out != nullptr) *out = res;
        return true;
    }

    QStringList SplitDnsAddresses(const QString &text) {
        auto res = Validation::Items(text, Validation::Split::Comma);
        res.removeDuplicates();
        return res;
    }

    QString DnsAddressListProblem(const QString &text) {
        const auto addresses = SplitDnsAddresses(text);
        if (addresses.isEmpty()) return QCoreApplication::translate("Validation", "The DNS address is empty.");
        for (qsizetype i = 0; i < addresses.size(); ++i) {
            QString why;
            if (ParseDnsAddress(addresses[i], nullptr, &why)) continue;
            if (addresses.size() == 1) return why;
            return QCoreApplication::translate("Validation", "Server %1 (\"%2\"): %3").arg(QString::number(i + 1), addresses[i], why);
        }
        return {};
    }

    QString NormalizeDnsAddressList(const QString &text) {
        return SplitDnsAddresses(text).join(", ");
    }
}
