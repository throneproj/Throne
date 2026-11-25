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

    namespace {

        // Counted the same way entIDListtoEntList counts, so the ports line up with the bridges buildOutboundChain creates.
        struct memberBridges
        {
            bool singToXray = false;
            bool xrayToSing = false;
            bool xrayFullConfig = false;

            [[nodiscard]] int count() const
            {
                return (singToXray ? 1 : 0) + (xrayToSing ? 1 : 0) + (xrayFullConfig ? 1 : 0);
            }
        };

        memberBridges bridgesFor(const QList<int> &hopIDs, bool tailIntoSing)
        {
            memberBridges needed;
            bool inXray = false;
            for (int id : hopIDs)
            {
                auto hop = getProfile(id);
                if (hop == nullptr || hop->outbound == nullptr) continue;
                if (hop->outbound->IsXrayFullConfig()) needed.xrayFullConfig = true;
                const bool xray = hop->outbound->IsXray();
                if (xray && !inXray) needed.singToXray = true;
                if (!xray && inXray) needed.xrayToSing = true;
                inXray = xray;
            }
            if (tailIntoSing && inXray) needed.xrayToSing = true;
            return needed;
        }

        // entIDListtoEntList's budget with the shared upstream counted as one more sing-box hop.
        bool fitsBeforeUpstream(const QList<int> &hopIDs)
        {
            int transitions = 0;
            bool inXray = false;
            for (int id : hopIDs)
            {
                auto hop = getProfile(id);
                if (hop == nullptr || hop->outbound == nullptr) return false;
                if (hop->outbound->IsXrayFullConfig() || hop->outbound->IsExtraCore()) return false;
                const bool xray = hop->outbound->IsXray();
                if (xray != inXray) transitions++;
                inXray = xray;
            }
            if (inXray) transitions++;
            return transitions <= 2;
        }

        // A chain exiting through a custom Xray full config takes the single Xray slot; selector members run theirs side by side instead.
        void absorbWithXraySlot(BuildContext &ctx, const ChainOutput &out)
        {
            absorbChain(ctx, out);
            if (out.xrayFullConfig.isEmpty()) return;
            ctx.result->xrayConfig = out.xrayFullConfig;
            ctx.result->isXrayNeeded = true;
        }

        struct SelectorGroupRequest
        {
            QString groupTag;
            // Every member dials through this outbound; empty when they are dialed directly.
            QString upstreamTag;
            bool upstreamResolvesViaDns = false;
            // Bytes land on the exit rather than on a member, so the member groups collapse into one.
            bool warpWrap = false;
            // Off when exit hops wrap the group: connections name the exit hop, which cannot tell the members apart.
            bool memberTraffic = true;
            // Credited with every member's traffic alongside the selector.
            QList<std::shared_ptr<Profile>> credited;
            // Set when a member's outermost hop looks hostnames up via DNS rules.
            bool *headsResolveViaDns = nullptr;
        };

        QString buildAutoSelectorGroup(BuildContext &ctx, const std::shared_ptr<Profile> &selectorEnt, const SelectorGroupRequest &req)
        {
            const auto &settings = *dataManager->settingsRepo;
            auto *live = selectorEnt->AutoSelector();
            if (live == nullptr)
            {
                ctx.error = "Ent is nullptr after cast to auto selector, data is corrupted";
                return {};
            }
            // A copy: Normalize() writes even when it changes nothing, and other threads read the live bean.
            auto normalized = *live;
            normalized.Normalize();
            const auto *selector = &normalized;
            const auto plan = selectorPlan(ctx, selectorEnt);
            if (!plan.error.isEmpty())
            {
                ctx.error = plan.error;
                return {};
            }

            const QString groupTag = req.groupTag;
            const bool viaUpstream = !req.upstreamTag.isEmpty();

            AutoSelectorBuildInfo info;
            info.groupTag = groupTag;
            info.profile = selectorEnt;
            info.upstream = viaUpstream;

            // One MkManyPorts call for every member: probing per member can deal the same port twice.
            struct plannedMember
            {
                std::shared_ptr<Profile> ent;
                QList<int> hopIDs;
                memberBridges bridges;
            };
            QList<plannedMember> planned;
            int bridgeCount = 0;
            // One member the core rejects fails the whole group's start, so drop it instead.
            const auto invalid = invalidProfileIDs(plan.build);
            // A member runs as if started on its own: wrapped in its own group's landing/front.
            QHash<int, std::shared_ptr<Group>> memberGroups;
            int unfitForUpstream = 0;
            for (int id : plan.build)
            {
                if (invalid.contains(id)) continue;
                auto member = generateEndpointLive(getProfile(id));
                if (member == nullptr) continue;
                if (!memberGroups.contains(member->gid))
                    memberGroups.insert(member->gid, dataManager->groupsRepo->GetGroup(member->gid));
                const auto group = memberGroups.value(member->gid);
                QList<int> hopIDs;
                if (group != nullptr && group->landing_proxy_id >= 0) hopIDs.append(group->landing_proxy_id);
                hopIDs.append(id);
                if (group != nullptr && group->front_proxy_id >= 0) hopIDs.append(group->front_proxy_id);
                if (viaUpstream && !fitsBeforeUpstream(hopIDs))
                {
                    unfitForUpstream++;
                    continue;
                }
                if (req.headsResolveViaDns != nullptr)
                {
                    if (const auto head = getProfile(hopIDs.first()); head != nullptr && resolvesHostnamesViaDnsRules(*head))
                        *req.headsResolveViaDns = true;
                }
                const auto bridges = bridgesFor(hopIDs, viaUpstream);
                bridgeCount += bridges.count();
                planned.append({member, hopIDs, bridges});
            }
            if (unfitForUpstream > 0)
                MW_show_log(QObject::tr("[Auto selector] Left out %n profile(s) that cannot be reached through the hops in front of the selector "
                                        "(custom Xray configs, or too many switches between %1 and Xray).", nullptr, unfitForUpstream)
                                .arg(software_core_name));
            auto bridgePorts = MkManyPorts(bridgeCount);
            int portIdx = 0;
            
            const auto builtAt = QDateTime::currentSecsSinceEpoch();
            QJsonArray warm;
            QString pinnedTag;

            QJsonArray memberTags;
            QList<QJsonArray> tierTags(selector->Prioritized() ? selector->tiers.size() : 0);
            QList<TrafficChainGroup> memberChainGroups;
            int idx = 0;
            for (const auto &[member, hopIDs, bridges] : planned)
            {
                const int singToXrayPort = bridges.singToXray ? bridgePorts[portIdx++] : -1;
                const int xrayToSingPort = bridges.xrayToSing ? bridgePorts[portIdx++] : -1;
                const int xrayFullConfigPort = bridges.xrayFullConfig ? bridgePorts[portIdx++] : -1;

                auto out = buildOutboundChain(ctx, {
                    .hopIDs = hopIDs,
                    .prefix = hopTag(tags::poolChainPrefix, idx),
                    .singToXrayPort = singToXrayPort,
                    .xrayToSingPort = xrayToSingPort,
                    .xrayFullConfigPort = xrayFullConfigPort,
                    .soleXrayInbound = bridges.xrayFullConfig,
                    .tailDetour = req.upstreamTag,
                    .tailResolvesViaDns = req.upstreamResolvesViaDns,
                });
                absorbChain(ctx, out);
                if (!ctx.error.isEmpty()) return {};
                if (bridges.xrayFullConfig && out.xrayFullConfig.isEmpty())
                {
                    ctx.error = "Custom Xray full config member produced no Xray config";
                    return {};
                }
                if (bridges.xrayFullConfig) ctx.result->xrayFullConfigs << QJsonObject2QString(out.xrayFullConfig, false);
                const auto &tag = out.ingressTag;
                memberTags.append(tag);
                // Members matching no tier stay unlisted: the core ranks them after the last tier.
                if (const int tier = plan.tierOf.value(member->id, -1); tier >= 0 && tier < tierTags.size()) tierTags[tier].append(tag);
                info.members.append({tag, member});
                // Connections carry the member the selector picked, never the group's own tag.
                ctx.result->outboundNames.insert(tag, profileDisplayName(member));
                if (member->id == selector->pinnedID) pinnedTag = tag;
                // A direct result does not describe a member reached through the upstream.
                if (!viaUpstream && member->latency != 0 && member->latency_at > 0)
                {
                    if (const auto age = builtAt - member->latency_at;
                        age >= 0 && age <= AutoSelectorResultValiditySecs(*member))
                    {
                        warm.append(QJsonObject{
                            {"tag", tag},
                            // rtt 0 is how the core reads "known bad" rather than "never measured".
                            {"rtt", member->latency > 0 ? member->latency : 0},
                            {"age", static_cast<double>(age)},
                        });
                    }
                }
                if (out.traffic)
                {
                    out.traffic->profiles << selectorEnt << req.credited;
                    memberChainGroups << *out.traffic;
                }
                idx++;
            }
            if (memberTags.isEmpty())
            {
                ctx.error = "Auto selector produced no usable members";
                return {};
            }

            if (req.memberTraffic && req.warpWrap)
            {
                // Bytes land on the exit outbound now, so the per-member watch tags would all read zero.
                TrafficChainGroup warpGroup;
                warpGroup.watchTag = tags::proxy;
                for (auto it = memberChainGroups.crbegin(); it != memberChainGroups.crend(); ++it) warpGroup.profiles << it->profiles;
                ctx.result->chainGroups.append(warpGroup);
            }
            else if (req.memberTraffic)
            {
                ctx.result->chainGroups << memberChainGroups;
            }

            QJsonObject groupObject{
                {"type", "auto-selector"},
                {"tag", groupTag},
                {"outbounds", memberTags},
                {"url", selector->testURL.isEmpty() ? settings.test_latency_url : selector->testURL},
                {"interval", Int2String(selector->intervalSec) + "s"},
                {"bench_interval", Int2String(selector->BenchIntervalSec(static_cast<int>(memberTags.size()))) + "s"},
                {"watch_interval", Int2String(selector->WatchIntervalSec()) + "s"},
                {"active_size", selector->ActiveSize()},
                {"sampling", autoSelector::kSampling},
                {"tolerance", selector->toleranceMs},
                {"expected", selector->expected},
                {"dial_retries", autoSelector::kDialRetries},
                {"interrupt_exist_connections", selector->interruptOnSwitch},
            };
            if (!warm.isEmpty()) groupObject["warm"] = warm;
            if (!pinnedTag.isEmpty()) groupObject["pinned"] = pinnedTag;
            if (selector->maxRTTms > 0) groupObject["max_rtt"] = Int2String(selector->maxRTTms) + "ms";
            // Never the latency test URL: that one is fetched through the proxy, and is routinely
            // blocked directly. Omitted when unset, so the core uses the OS network state.
            const auto connectivityURL = selector->connectivityURL.isEmpty() ? settings.direct_test_url
                                                                             : selector->connectivityURL;
            if (!connectivityURL.isEmpty()) groupObject["connectivity_url"] = connectivityURL;
            if (viaUpstream) groupObject["upstream"] = req.upstreamTag;
            if (selector->balance)
            {
                groupObject["balance"] = true;
                groupObject["balance_mode"] = selector->balanceMode;
                groupObject["balance_interval"] = Int2String(autoSelector::kBalanceIntervalSec) + "s";
            }
            if (selector->Prioritized())
            {
                QJsonArray tiers;
                for (const auto &tags : std::as_const(tierTags)) tiers.append(tags);
                groupObject["strategy"] = "priority";
                groupObject["tiers"] = tiers;
                groupObject["failback_checks"] = autoSelector::kFailbackChecks;
                groupObject["failback_delay"] = Int2String(autoSelector::kFailbackDelaySec) + "s";
            }
            if (selector->passive)
                groupObject["passive"] = QJsonObject{{"enabled", true}, {"site_memory", Int2String(autoSelector::kSiteMemoryMins) + "m"}};
            ctx.outbounds.append(groupObject);
            ctx.result->autoSelectors.append(info);
            return groupTag;
        }

        // The group already holds warp-bypass, so the exit itself is emitted here as "proxy" and points at it.
        void buildExitInFrontOfSelector(BuildContext &ctx)
        {
            QString refusal;
            const auto exitEnt = exitProfile(&refusal);
            if (exitEnt == nullptr)
            {
                ctx.error += refusal;
                return;
            }
            auto [exitObject, exitBuildError] = exitEnt->outbound->Build();
            if (!exitBuildError.isEmpty())
            {
                ctx.error += exitBuildError;
                return;
            }
            exitObject["tag"] = tags::proxy;
            exitObject["detour"] = tags::warpBypass;
            if (exitEnt->outbound->IsEndpoint()) ctx.endpoints.append(exitObject);
            else ctx.outbounds.append(exitObject);
        }

        // The hops above the selector are built once and every member detours into them; the hops below detour into the group.
        void buildChainAroundSelector(BuildContext &ctx, const Group &group, const std::shared_ptr<Profile> &selectorEnt, bool warpWrap)
        {
            const auto list = ctx.ent->Chain()->list;
            const auto at = list.indexOf(selectorEnt->id);
            QList<int> exitIDs;
            if (warpWrap) exitIDs << warpProfileID;
            if (group.landing_proxy_id >= 0) exitIDs << group.landing_proxy_id;
            for (auto i = list.size() - 1; i > at; --i) exitIDs << list[i];
            QList<int> upstreamIDs;
            for (auto i = at - 1; i >= 0; --i) upstreamIDs << list[i];
            if (group.front_proxy_id >= 0) upstreamIDs << group.front_proxy_id;

            for (const int id : exitIDs + upstreamIDs) {
                if (id == warpProfileID) continue;
                const auto hop = getProfile(id);
                if (hop == nullptr) {
                    ctx.error = missingHopError(ctx, id);
                    return;
                }
                if (hop->type == "autoselector") {
                    ctx.error = QObject::tr("A chain can hold only one auto selector");
                    return;
                }
                if ((hop->outbound != nullptr && hop->outbound->IsExtraCore()) || isCustomFullConfig(hop) || isXrayFullConfig(hop)) {
                    ctx.error = QObject::tr("Extra Core profiles and custom configs cannot share a chain with an auto selector");
                    return;
                }
            }

            QList<std::shared_ptr<Profile>> credited{ctx.ent};
            QString upstreamTag;
            bool upstreamResolvesViaDns = false;
            if (!upstreamIDs.isEmpty()) {
                const auto upstreamChain = buildOutboundChain(ctx, {.hopIDs = upstreamIDs, .prefix = tags::selectorUpstreamPrefix});
                absorbWithXraySlot(ctx, upstreamChain);
                upstreamTag = upstreamChain.ingressTag;
                if (!ctx.error.isEmpty()) return;
                // No connection names the upstream as its outbound, so its bytes go to whatever carried them.
                if (upstreamChain.traffic) credited << upstreamChain.traffic->profiles;
                const auto nearest = getProfile(upstreamIDs.first());
                upstreamResolvesViaDns = nearest != nullptr && resolvesHostnamesViaDnsRules(*nearest);
            }

            const bool onlyWarp = exitIDs == QList<int>{warpProfileID};
            const QString groupTag = exitIDs.isEmpty() ? tags::proxy : onlyWarp ? tags::warpBypass : tags::chainSelector;
            bool headsResolveViaDns = false;
            buildAutoSelectorGroup(ctx, selectorEnt, {
                .groupTag = groupTag,
                .upstreamTag = upstreamTag,
                .upstreamResolvesViaDns = upstreamResolvesViaDns,
                .memberTraffic = exitIDs.isEmpty(),
                .credited = credited,
                .headsResolveViaDns = &headsResolveViaDns,
            });
            if (!ctx.error.isEmpty() || exitIDs.isEmpty()) return;

            auto exitChain = buildOutboundChain(ctx, {
                .hopIDs = exitIDs,
                .prefix = tags::mainChainPrefix,
                .includeProxy = true,
                .warpWrap = warpWrap,
                .tailDetour = groupTag,
                .tailResolvesViaDns = headsResolveViaDns,
            });
            absorbWithXraySlot(ctx, exitChain);
            if (!ctx.error.isEmpty()) return;
            if (exitChain.traffic) {
                exitChain.traffic->profiles << selectorEnt << credited;
                ctx.result->chainGroups.append(*exitChain.traffic);
            }
        }

    } // namespace

    namespace gen {

        void buildOutboundsSection(BuildContext &ctx) {
            auto group = dataManager->groupsRepo->GetGroup(ctx.ent->gid);
            if (group == nullptr)
            {
                ctx.error = "No group found for ent, data is corrupted";
                return;
            }
            const bool warpWrap = exitWrapperOn();
            if (ctx.ent->type == "autoselector")
            {
                // With an exit in front every member sits behind the single "proxy"-tagged exit outbound, so the group takes warp-bypass.
                buildAutoSelectorGroup(ctx, ctx.ent, {.groupTag = warpWrap ? tags::warpBypass : tags::proxy, .warpWrap = warpWrap});
                if (!ctx.error.isEmpty()) return;
                if (warpWrap)
                {
                    buildExitInFrontOfSelector(ctx);
                    if (!ctx.error.isEmpty()) return;
                }
            }
            else if (const auto selectorHop = chainSelectorHop(ctx.ent); selectorHop != nullptr)
            {
                buildChainAroundSelector(ctx, *group, selectorHop, warpWrap);
                if (!ctx.error.isEmpty()) return;
            }
            else
            {
                QList<int> entIDs;
                if (group->landing_proxy_id >= 0) entIDs.prepend(group->landing_proxy_id);
                if (ctx.ent->type == "chain")
                {
                    auto chain = ctx.ent->Chain();
                    if (chain == nullptr)
                    {
                        ctx.error = "Ent is nullptr after cast to chain, data is corrupted";
                        return;
                    }
                    entIDs.reserve(entIDs.size() + chain->list.size());
                    std::copy(chain->list.crbegin(), chain->list.crend(),
                             std::back_inserter(entIDs));
                } else
                {
                    entIDs.append(ctx.ent->id);
                }
                if (group->front_proxy_id >= 0) entIDs.append(group->front_proxy_id);
                if (warpWrap) {
                    entIDs.prepend(warpProfileID);
                }
                auto out = buildOutboundChain(ctx, {
                    .hopIDs = entIDs,
                    .prefix = tags::mainChainPrefix,
                    .includeProxy = true,
                    .warpWrap = warpWrap,
                });
                absorbWithXraySlot(ctx, out);

                if (out.traffic) {
                    if (ctx.ent->type == "chain") out.traffic->profiles.append(ctx.ent);
                    ctx.result->chainGroups.append(*out.traffic);
                }
            }
            ctx.result->outboundNames.insert(warpWrap ? tags::warpBypass : tags::proxy, profileDisplayName(ctx.ent));
            if (warpWrap) ctx.result->outboundNames.insert(tags::proxy, ExitName(activeExit()));

            int routeSuffix = 0;
            for (const auto& routeGroup : ctx.prerequisites.routing.routeOutboundGroups) {
                auto out = buildOutboundChain(ctx, {
                    .hopIDs = routeGroup.hopIDs,
                    .prefix = tags::routeChainPrefix,
                    .link = routeGroup.hopIDs.size() > 1,
                    .startSuffix = routeSuffix,
                });
                absorbWithXraySlot(ctx, out);
                const auto &tag = out.ingressTag;
                if (routeGroup.chainWrapper != nullptr && out.traffic) {
                    out.traffic->profiles.append(routeGroup.chainWrapper);
                }
                const auto named = routeGroup.profile;
                if (named != nullptr) ctx.result->outboundNames.insert(tag, profileDisplayName(named));
                if (named != nullptr && out.traffic) {
                    auto &chainGroup = *out.traffic;
                    chainGroup.kind = TrafficChainKind::Route;
                    chainGroup.profileId = named->id;
                    chainGroup.name = profileDisplayName(named);
                }
                if (out.traffic) ctx.result->chainGroups.append(*out.traffic);
                routeSuffix += static_cast<int>(routeGroup.hopIDs.size());
            }

            // preferred_by resolves an endpoint out of the endpoint manager, so nothing detours into these.
            if (ctx.policy.fullRouting) {
                int auxSuffix = 0;
                for (const auto &group : ctx.prerequisites.routing.auxEndpointGroups) {
                    QList<QString> innerTags;
                    for (const int idx : group.innerHopIndexes) {
                        innerTags << hopTag(tags::auxEndpointPrefix, auxSuffix + idx);
                        if (const auto hop = getProfile(group.hopIDs[idx]); hop != nullptr)
                            ctx.result->outboundNames.insert(innerTags.last(), profileDisplayName(hop));
                    }
                    auto out = buildOutboundChain(ctx, {
                        .hopIDs = group.hopIDs,
                        .prefix = tags::auxEndpointPrefix,
                        .includeProxy = false,
                        .link = group.hopIDs.size() > 1,
                        .startSuffix = auxSuffix,
                        .auxiliary = true,
                        .addressableTags = QSet<QString>(innerTags.cbegin(), innerTags.cend()),
                    });
                    absorbWithXraySlot(ctx, out);
                    if (!ctx.error.isEmpty()) return;
                    const auto &tag = out.ingressTag;
                    if (group.endpoint != nullptr) ctx.result->outboundNames.insert(tag, profileDisplayName(group.endpoint));
                    if (group.endpoint != nullptr && out.traffic) {
                        auto &chainGroup = *out.traffic;
                        chainGroup.kind = TrafficChainKind::Aux;
                        chainGroup.profileId = group.endpoint->id;
                        chainGroup.name = profileDisplayName(group.endpoint);
                    }
                    if (out.traffic) ctx.result->chainGroups.append(*out.traffic);
                    ctx.vpnAuxTags << tag;
                    ctx.vpnAuxTags << innerTags;
                    auxSuffix += static_cast<int>(group.hopIDs.size());
                }
                // The rules list decides which tunnel wins a route or name two of them claim, so DNS must follow it too.
                QList<QString> ordered;
                for (const auto &tag : ctx.prerequisites.routing.endpointRuleTags) {
                    if (ctx.vpnAuxTags.contains(tag) && !ordered.contains(tag)) ordered << tag;
                }
                for (const auto &tag : std::as_const(ctx.vpnAuxTags)) {
                    if (!ordered.contains(tag)) ordered << tag;
                }
                ctx.vpnAuxTags = ordered;
            }

            QJsonArray inboundArr;
            if (ctx.result->coreConfig.contains("inbounds")) {
                inboundArr = ctx.result->coreConfig["inbounds"].toArray();
            }
            for (const auto &inbound : ctx.bridges.singInbounds(BridgeTagScheme::ByIngress)) inboundArr.append(inbound);
            ctx.result->coreConfig["inbounds"] = inboundArr;

            ctx.outbounds.append(QJsonObject{
            {"type", "direct"},
            {"tag", tags::direct}
            });

            if (ctx.l3Bridge) {
                // The Linux kill switch recognises the bridge tun by this name prefix.
                ctx.outbounds.append(QJsonObject{
                {"type", "bridge"},
                {"tag", tags::l3Direct},
                {"bridge_name", "throne-br"}
                });
            }

            ctx.result->coreConfig["endpoints"] = ctx.endpoints;
            ctx.result->coreConfig["outbounds"] = ctx.outbounds;
        }

    } // namespace gen
}
