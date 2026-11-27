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

        struct SelectorSink {
            QJsonArray *ruleSets = nullptr;
            QJsonArray *domains = nullptr;
            QJsonArray *suffixes = nullptr;
            QJsonArray *keywords = nullptr;
            QJsonArray *regexes = nullptr;
            QJsonArray *ipCIDRs = nullptr;
        };

        SelectorSink sinkFor(DomainSelectors &selectors) {
            return {
                .ruleSets = &selectors.ruleSets,
                .domains = &selectors.domains,
                .suffixes = &selectors.suffixes,
                .keywords = &selectors.keywords,
                .regexes = &selectors.regexes,
            };
        }

        void parseSelectorList(const QStringList &items, const SelectorSink &sink) {
            const std::pair<QLatin1String, QJsonArray *> kinds[] = {
                {QLatin1String("ruleset:"), sink.ruleSets},
                {QLatin1String("domain:"), sink.domains},
                {QLatin1String("suffix:"), sink.suffixes},
                {QLatin1String("keyword:"), sink.keywords},
                {QLatin1String("regex:"), sink.regexes},
                {QLatin1String("ip:"), sink.ipCIDRs},
            };
            for (const auto &rawItem : items) {
                const QString item = rawItem.trimmed();
                for (const auto &[prefix, target] : kinds) {
                    if (!item.startsWith(prefix)) continue;
                    const QString value = item.mid(prefix.size()).trimmed();
                    if (target != nullptr && !value.isEmpty()) *target << value;
                    break;
                }
            }
        }

        // The core has no bridge backend for Windows on ARM, and a failed one aborts the config.
        bool l3BridgeEnabled(const BuildContext &ctx) {
#if defined(Q_OS_WIN) && defined(Q_PROCESSOR_ARM)
            return false;
#else
            return ctx.tunEnabled && ctx.policy.listeners && dataManager->settingsRepo->vpn_l3_bridge;
#endif
        }

        bool prefixesOverlap(const QString &lhs, const QString &rhs) {
            const auto a = QHostAddress::parseSubnet(lhs);
            const auto b = QHostAddress::parseSubnet(rhs);
            if (a.second < 0 || b.second < 0) return false;
            if (a.first.protocol() != b.first.protocol()) return false;
            return a.second <= b.second ? b.first.isInSubnet(a.first, a.second)
                                        : a.first.isInSubnet(b.first, b.second);
        }

        QList<int> groupProxyIDs(int gid)
        {
            const auto group = dataManager->groupsRepo->GetGroup(gid);
            if (group == nullptr) return {};
            QList<int> ids;
            if (group->front_proxy_id >= 0) ids << group->front_proxy_id;
            if (group->landing_proxy_id >= 0) ids << group->landing_proxy_id;
            return ids;
        }

        // A selector member is wrapped in its own group's front/landing, never the selector group's.
        QList<int> memberGroupProxyIDs(const QList<int> &memberIDs)
        {
            QSet<int> groups;
            QList<int> ids;
            for (int pid : memberIDs) {
                const auto member = getProfile(pid);
                if (member == nullptr || groups.contains(member->gid)) continue;
                groups.insert(member->gid);
                for (int id : groupProxyIDs(member->gid)) {
                    if (!ids.contains(id)) ids << id;
                }
            }
            return ids;
        }

    } // namespace

    namespace gen {

        std::shared_ptr<Profile> chainSelectorHop(const std::shared_ptr<Profile> &ent) {
            const auto *chain = ent != nullptr ? ent->Chain() : nullptr;
            if (chain == nullptr) return nullptr;
            for (int id : chain->list) {
                if (auto hop = getProfile(id); hop != nullptr && hop->type == "autoselector") return hop;
            }
            return nullptr;
        }

        AutoSelectorPlan selectorPlan(BuildContext &ctx, const std::shared_ptr<Profile> &selectorEnt) {
            if (const auto it = ctx.selectorPlans.constFind(selectorEnt->id); it != ctx.selectorPlans.cend()) return *it;
            const auto plan = PlanAutoSelector(selectorEnt);
            ctx.selectorPlans.insert(selectorEnt->id, plan);
            return plan;
        }

    } // namespace gen

    namespace {

        // Decided up front: the sidecar's DNS carve-outs must be in place before the first connection.
        bool proxyPathUsesXray(BuildContext &ctx, const std::shared_ptr<Profile> &ent)
        {
            if (ent->type == "chain") {
                if (auto chain = ent->Chain(); chain != nullptr) {
                    for (int pid : chain->list) {
                        auto hop = getProfile(pid);
                        if (hop == nullptr) continue;
                        if (hop->type == "autoselector" ? proxyPathUsesXray(ctx, hop) : usesXrayCore(hop)) return true;
                    }
                }
                return false;
            }
            if (ent->type == "autoselector") {
                const auto plan = selectorPlan(ctx, ent);
                for (int pid : plan.build) {
                    auto member = getProfile(pid);
                    if (member != nullptr && usesXrayCore(member)) return true;
                }
                for (int id : memberGroupProxyIDs(plan.build)) {
                    if (auto proxy = getProfile(id); proxy != nullptr && usesXrayCore(proxy)) return true;
                }
                return false;
            }
            return usesXrayCore(ent);
        }

    } // namespace

    namespace gen {

        QList<int> unwrapChain(int entID);

        void calculatePrerequisites(BuildContext &ctx) {
            const auto &settings = *dataManager->settingsRepo;
            ctx.tunEnabled = settings.spmode_vpn;
            ctx.os = getOS();
#ifdef Q_OS_LINUX
            ctx.isResolvedUsed = isSystemdResolvedDefaultResolver();
#endif
            ctx.dns = RouteDns::OfRoute(ctx.routeId, &ctx.routeOwnsDns);
            auto &preReqs = ctx.prerequisites;

            auto routeChain = dataManager->routesRepo->GetRouteProfile(ctx.routeId);
            if (routeChain == nullptr) {
                ctx.error = "Routing profile does not exist, try resetting the routing profile in Routing Settings";
                return;
            }
            // A verbatim raw profile takes no twins, and an unreachable bridge still starts a tun.
            ctx.l3Bridge = l3BridgeEnabled(ctx) && !(routeChain->isRaw && routeChain->preventModifications);

            if (auto exitRefusal = exitError(); !exitRefusal.isEmpty()) {
                ctx.error = std::move(exitRefusal);
                return;
            }

            auto neededOutbounds = routeChain->get_used_outbounds();
            auto neededRuleSets = routeChain->get_used_rule_sets();
            preReqs.routing.outboundMap[-1] = tags::proxy;
            preReqs.routing.outboundMap[-2] = tags::direct;
            preReqs.routing.outboundMap[warpBypassID] = exitWrapperOn() ? tags::warpBypass : tags::proxy;
            int suffix = 0;
            if (proxyPathUsesXray(ctx, ctx.ent)) ctx.proxyUsesXray = true;
            for (const auto &item: *neededOutbounds) {
                if (item < 0 || preReqs.routing.outboundMap.contains(item)) continue;
                auto neededEnt = getProfile(item);
                if (neededEnt == nullptr) {
                    ctx.error = "The routing profile is referencing outbounds that no longer exist, consider revising your settings";
                    return;
                }
                if ((neededEnt->outbound != nullptr && neededEnt->outbound->IsExtraCore()) || isCustomFullConfig(neededEnt)) {
                    ctx.error = "Outbounds used in routing profile cannot use an extra core or be a custom full config";
                    return;
                }
                QList<int> hopIDs;
                if (neededEnt->type == "chain") {
                    auto chain = neededEnt->Chain();
                    if (chain == nullptr || chain->list.isEmpty()) {
                        ctx.error = "Chain outbound in routing profile is empty or corrupted";
                        return;
                    }
                    for (int hopID : chain->list) {
                        auto hopEnt = getProfile(hopID);
                        if (hopEnt == nullptr) {
                            ctx.error = "Chain outbound in routing profile contains a missing profile";
                            return;
                        }
                        if ((hopEnt->outbound != nullptr && hopEnt->outbound->IsExtraCore()) || isCustomFullConfig(hopEnt) || isXrayFullConfig(hopEnt) || hopEnt->type == "chain") {
                            ctx.error = "Chain hops in routing profile cannot use an extra core, a custom full config, or be of type chain";
                            return;
                        }
                        if (hopEnt->type == "autoselector") {
                            ctx.error = QObject::tr("The routing profile sends traffic to %1, which holds an auto selector; such a chain can only be started, not used in routing rules")
                                            .arg(neededEnt->outbound->DisplayTypeAndName());
                            return;
                        }
                        if (usesXrayCore(hopEnt)) ctx.proxyUsesXray = true;
                    }
                    // Reversed to match the main-chain build order: outer hop first.
                    hopIDs = {chain->list.rbegin(), chain->list.rend()};
                } else {
                    if (usesXrayCore(neededEnt)) ctx.proxyUsesXray = true;
                    hopIDs = {item};
                }
                // Wrapped in its own group's landing/front, as when the profile is started on its own.
                if (const auto group = dataManager->groupsRepo->GetGroup(neededEnt->gid); group != nullptr) {
                    if (isXrayFullConfig(neededEnt) && (group->front_proxy_id >= 0 || group->landing_proxy_id >= 0)) {
                        ctx.error = QCoreApplication::translate("Validation", "An Xray full config used in a routing rule cannot have a group front or landing proxy.");
                        return;
                    }
                    for (const int proxyID : {group->landing_proxy_id, group->front_proxy_id}) {
                        const auto proxyEnt = proxyID >= 0 ? getProfile(proxyID) : nullptr;
                        if (proxyEnt == nullptr) continue;
                        // No extra core starts for a route chain, and a full config would take over the shared Xray config.
                        if ((proxyEnt->outbound != nullptr && proxyEnt->outbound->IsExtraCore()) || isCustomFullConfig(proxyEnt) || isXrayFullConfig(proxyEnt)) {
                            ctx.error = QObject::tr("Group \"%1\" has a front or landing proxy that uses an extra core or is a full config, so its profiles cannot be routing rule outbounds").arg(group->name);
                            return;
                        }
                        if (usesXrayCore(proxyEnt)) ctx.proxyUsesXray = true;
                    }
                    if (group->landing_proxy_id >= 0) hopIDs.prepend(group->landing_proxy_id);
                    if (group->front_proxy_id >= 0) hopIDs.append(group->front_proxy_id);
                }
                preReqs.routing.outboundMap[item] = hopTag(tags::routeChainPrefix, suffix);
                preReqs.routing.routeOutboundGroups << RoutingDeps::RouteOutboundGroup{hopIDs, neededEnt->type == "chain" ? neededEnt : nullptr, neededEnt};
                suffix += static_cast<int>(hopIDs.size());
            }

            if (!routeChain->endpointProfileIDs.isEmpty()) {
                QSet<int> usedProfileIDs;
                for (const auto &routeGroup : preReqs.routing.routeOutboundGroups) {
                    for (int hopID : routeGroup.hopIDs) usedProfileIDs << hopID;
                }
                const auto entHopIDs = unwrapChain(ctx.ent->id);
                for (int hopID : entHopIDs) usedProfileIDs << hopID;
                auto entGroupProxyIDs = ctx.ent->type == "autoselector"
                                            ? memberGroupProxyIDs(selectorPlan(ctx, ctx.ent).build)
                                            : groupProxyIDs(ctx.ent->gid);
                if (const auto selectorHop = chainSelectorHop(ctx.ent); selectorHop != nullptr)
                    entGroupProxyIDs << memberGroupProxyIDs(selectorPlan(ctx, selectorHop).build);
                for (int proxyID : entGroupProxyIDs) usedProfileIDs << proxyID;
                int auxSuffix = 0;
                for (int endpointID : routeChain->endpointProfileIDs) {
                    auto endpointEnt = getProfile(endpointID);
                    if (endpointEnt == nullptr || endpointEnt->outbound == nullptr) {
                        ctx.error = QObject::tr("The routing profile lists an endpoint profile (id %1) that no longer exists").arg(endpointID);
                        return;
                    }
                    const auto endpointName = endpointEnt->outbound->DisplayTypeAndName();
                    // unwrapChain reverses the stored list, so hop 0 is the exit that carries the tag.
                    const auto hopIDs = unwrapChain(endpointID);
                    if (hopIDs.isEmpty()) {
                        ctx.error = QObject::tr("%1 is listed as a routing profile endpoint but is empty or corrupted").arg(endpointName);
                        return;
                    }
                    auto exitEnt = getProfile(hopIDs.first());
                    if (exitEnt == nullptr || exitEnt->outbound == nullptr) {
                        ctx.error = QObject::tr("%1 is listed as a routing profile endpoint but a hop of it no longer exists").arg(endpointName);
                        return;
                    }
                    if (!Protocols::Is(exitEnt->type, Protocols::VpnEndpoint)) {
                        ctx.error = QObject::tr("%1 is listed as a routing profile endpoint, so its last hop must be an OpenVPN or OpenConnect profile").arg(endpointName);
                        return;
                    }
                    for (int hopID : hopIDs) {
                        auto hopEnt = getProfile(hopID);
                        if (hopEnt == nullptr || hopEnt->outbound == nullptr) {
                            ctx.error = QObject::tr("%1 is listed as a routing profile endpoint but a hop of it no longer exists").arg(endpointName);
                            return;
                        }
                        if (hopEnt->outbound->IsExtraCore() || isCustomFullConfig(hopEnt) || isXrayFullConfig(hopEnt)
                            || hopEnt->type == "chain") {
                            ctx.error = QObject::tr("Hops of the routing profile endpoint %1 cannot use an extra core, a full config, or be a chain").arg(endpointName);
                            return;
                        }
                        // An auxiliary endpoint is reached only through preferred_by, so there is no ingress to bridge.
                        if (usesXrayCore(hopEnt)) {
                            ctx.error = QObject::tr("Hops of the routing profile endpoint %1 cannot run on the Xray core").arg(endpointName);
                            return;
                        }
                        if (usedProfileIDs.contains(hopID)) {
                            ctx.error = QObject::tr("%1 is used as an endpoint of the routing profile and by the started profile at the same time, remove it from one of them").arg(hopEnt->outbound->DisplayTypeAndName());
                            return;
                        }
                    }
                    if (usedProfileIDs.contains(endpointID)) {
                        ctx.error = QObject::tr("%1 is listed twice in the endpoints of the routing profile").arg(endpointName);
                        return;
                    }
                    usedProfileIDs << endpointID;
                    for (int hopID : hopIDs) usedProfileIDs << hopID;
                    RoutingDeps::AuxEndpointGroup auxGroup{hopIDs, {}, endpointEnt};
                    if (routeChain->innerHopEndpointIDs.contains(endpointID)) {
                        // preferred_by needs an outbound that advertises routes; naming any other hop fails core start.
                        for (qsizetype i = 1; i < hopIDs.size(); i++) {
                            const auto hopEnt = getProfile(hopIDs[i]);
                            if (hopEnt == nullptr || !Protocols::Is(hopEnt->type, Protocols::VpnEndpoint)) continue;
                            auxGroup.innerHopIndexes << static_cast<int>(i);
                            preReqs.routing.outboundMap[hopIDs[i]] = hopTag(tags::auxEndpointPrefix, auxSuffix + static_cast<int>(i));
                        }
                    }
                    preReqs.routing.auxEndpointGroups << auxGroup;
                    preReqs.routing.outboundMap[endpointID] = hopTag(tags::auxEndpointPrefix, auxSuffix);
                    auxSuffix += static_cast<int>(hopIDs.size());
                }
                if (!routeChain->isRaw) {
                    for (const auto &rule : routeChain->Rules) {
                        if (rule->disabled || rule->type != endpointPreferredBy) continue;
                        if (const auto it = preReqs.routing.outboundMap.find(rule->outboundID); it != preReqs.routing.outboundMap.end())
                            preReqs.routing.endpointRuleTags << it->second;
                    }
                }
            }

            for (const auto &item: *neededRuleSets) {
                if (!preReqs.routing.neededRuleSets.contains(item)) preReqs.routing.neededRuleSets << item;
            }

            if (ctx.dns.enable_dns_routing) {
                auto sets = routeChain->get_direct_sites();
                parseSelectorList(sets, sinkFor(preReqs.dns.direct));
                if (!sets.isEmpty()) preReqs.dns.needDirectDnsRules = true;

                // With a direct final DNS these need an explicit remote-DNS carve-out.
                auto proxySets = routeChain->get_proxy_sites();
                parseSelectorList(proxySets, sinkFor(preReqs.dns.proxy));
                if (!proxySets.isEmpty()) preReqs.dns.needProxyDnsRules = true;
            }
            // proxyPathUsesXray already weighed a selector's member groups; its own group wraps nothing.
            if (ctx.ent->type != "autoselector")
            {
                for (const auto &id : groupProxyIDs(ctx.ent->gid))
                {
                    if (auto pe = getProfile(id); pe != nullptr && usesXrayCore(pe)) ctx.proxyUsesXray = true;
                }
            }

            parseSelectorList(routeChain->get_direct_ips(), {
                .ruleSets = &preReqs.tun.directIPSets,
                .ipCIDRs = &preReqs.tun.directIPCIDRs,
            });

            if (!settings.disable_private_range_bypass) {
                const auto hijackedIPs = routeChain->get_hijacked_ips();
                // sing-tun keeps an excluded range out of the Tun entirely, so a rule aimed at one never fires (#1741).
                for (const auto &saved : settings.vpn_private_ranges) {
                    if (saved.trimmed().isEmpty()) continue;
                    const auto range = routeExcludePrefix(saved);
                    if (range.isEmpty()) {
                        if (ctx.tunEnabled) MW_show_log(QObject::tr("Skipped the Tun private range \"%1\": it is not a valid IP address or CIDR.").arg(saved.trimmed()));
                        continue;
                    }
                    const bool hijacked = std::any_of(hijackedIPs.cbegin(), hijackedIPs.cend(),
                        [&range](const QString &cidr) { return prefixesOverlap(range, cidr); });
                    if (!hijacked) preReqs.tun.bypassedPrivateRanges << range;
                }
                // auto_redirect hijacks DNS after static excludes but before address-set ones (#1895); a verbatim raw route cannot host the set.
                preReqs.tun.privateRangesAsRuleSet = ctx.tunEnabled && ctx.os == Linux && settings.vpn_auto_redirect &&
                    !preReqs.tun.bypassedPrivateRanges.isEmpty() && !(routeChain->isRaw && routeChain->preventModifications);
            }

            auto extraCoreEnt = resolveExtraCoreProfile(ctx.ent);
            if (extraCoreEnt == nullptr) return;
            auto outbound = extraCoreEnt->ExtraCore();
            if (outbound == nullptr)
            {
                MW_show_log("INVALID ENT TYPE, NEEDED EXTRACORE GOT NULLPTR");
                ctx.error = "failed to cast to extracore, type is: " + extraCoreEnt->type;
                return;
            }
            auto &extraCoreData = *ctx.result->extraCoreData;
            extraCoreData.path = QFileInfo(outbound->extraCorePath).canonicalFilePath();
            extraCoreData.args = outbound->extraCoreArgs;
            extraCoreData.config = outbound->extraCoreConf;
            extraCoreData.noLog = outbound->noLogs;
        }

        QList<int> unwrapChain(int entID) {
            auto ent = getProfile(entID);
            if (ent == nullptr)
            {
                return {};
            }
            if (ent->type == "chain") {
                auto chain = ent->Chain();
                if (chain == nullptr) return {};
                return {chain->list.rbegin(), chain->list.rend()};
            }
            return {entID};
        }

    } // namespace gen

    bool CanBeAuxEndpoint(const std::shared_ptr<Profile>& ent)
    {
        if (ent == nullptr || ent->outbound == nullptr) return false;
        if (Protocols::Is(ent->type, Protocols::VpnEndpoint)) return true;
        if (ent->type != "chain") return false;
        // unwrapChain reverses the stored list, so hop 0 is the exit.
        const auto hopIDs = unwrapChain(ent->id);
        if (hopIDs.isEmpty()) return false;
        const auto exitEnt = getProfile(hopIDs.first());
        if (exitEnt == nullptr) return false;
        return Protocols::Is(exitEnt->type, Protocols::VpnEndpoint);
    }

    QList<int> AuxEndpointInnerHops(int endpointProfileID)
    {
        const auto ent = getProfile(endpointProfileID);
        if (ent == nullptr || ent->type != "chain" || !CanBeAuxEndpoint(ent)) return {};
        QList<int> inner;
        const auto hopIDs = unwrapChain(endpointProfileID);
        for (qsizetype i = 1; i < hopIDs.size(); i++)
        {
            const auto hopEnt = getProfile(hopIDs[i]);
            if (hopEnt == nullptr || hopEnt->outbound == nullptr) continue;
            if (Protocols::Is(hopEnt->type, Protocols::VpnEndpoint)) inner << hopIDs[i];
        }
        return inner;
    }
}
