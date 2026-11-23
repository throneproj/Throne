#include "include/configs/generate.h"
#include "include/configs/generate_internal.h"
#include "include/api/RPC.h"
#include "include/configs/AutoSelectorPlan.h"
#include "include/configs/common/Protocols.h"
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

        // ------------------------------------------------------- test candidates

        enum class testCandidate {
            Build,           // ordinary profile or chain, built into the shared test box
            XrayFullConfig,  // opaque user Xray config, gets its own Xray instance
            Skip,
        };

        struct testCandidateKind {
            testCandidate kind = testCandidate::Build;
            const char *skipReason = nullptr;
        };

        testCandidateKind classifyTestCandidate(const std::shared_ptr<Profile> &profile)
        {
            if (profile->outbound != nullptr && profile->outbound->IsExtraCore())
                return {testCandidate::Skip, "Skipping extra-core conf"};
            if (profile->outbound != nullptr && profile->outbound->IsXrayFullConfig())
                return {testCandidate::XrayFullConfig, nullptr};
            if (profile->type == "chain")
            {
                if (auto chain = profile->Chain(); chain != nullptr) {
                    for (int hopID : chain->list) {
                        auto hopEnt = getProfile(hopID);
                        if (hopEnt != nullptr && hopEnt->outbound != nullptr &&
                            (hopEnt->outbound->IsExtraCore() || hopEnt->outbound->IsXrayFullConfig()))
                            return {testCandidate::Skip, "Skipping chain with terminal (extra-core or Xray full config) hop (cannot test)"};
                        if (hopEnt != nullptr && hopEnt->type == "autoselector")
                            return {testCandidate::Skip, "Skipping chain with an auto selector hop (test its members instead)"};
                    }
                }
                return {testCandidate::Build, nullptr};
            }
            if (profile->type == "tailscale")
                return {testCandidate::Skip, "Skipping Tailscale conf"};
            if (profile->type == "autoselector")
                return {testCandidate::Skip, "Skipping auto selector conf (test its members instead)"};
            return {testCandidate::Build, nullptr};
        }

        // ---------------------------------------------------------- scan clones

        // Far below every sentinel id (-1, warpProfileID), so a clone never resolves to one.
        constexpr int kScanSyntheticIdBase = -1000000;
        constexpr int kChainStepSyntheticIdBase = -2000000;

        bool chainNeedsMissingTor(const ChainOutput &out) {
            for (const auto item : out.outbounds) {
                if (item.toObject().value("type").toString() == QLatin1String("tor")) return !tor::Installed();
            }
            return false;
        }

        bool isVpnEndpointType(const QString &type) {
            return Protocols::Is(type, Protocols::VpnEndpoint);
        }

        std::shared_ptr<Profile> scanChainHop(int id) {
            return id == warpProfileID ? exitProfile() : getProfile(id);
        }

        // A chain is scanned through the hop it dials directly, the first of its in-to-out list.
        QString scanBaseBlocker(const std::shared_ptr<Profile> &base) {
            if (base == nullptr || base->outbound == nullptr) return QObject::tr("The profile no longer exists");
            if (base->type != "chain") return scanCloneBlocker(base);
            const auto *chain = base->Chain();
            if (chain == nullptr || chain->list.isEmpty()) return QObject::tr("The chain has no hops");
            for (const int hopId : chain->list) {
                const auto hop = scanChainHop(hopId);
                if (hop == nullptr || hop->outbound == nullptr) return QObject::tr("A hop of the chain no longer exists");
                if (hop->outbound->IsExtraCore() || hop->outbound->IsXrayFullConfig())
                    return QObject::tr("Chains with an extra-core or Xray full config hop cannot be tested");
                if (hop->type == "autoselector")
                    return QObject::tr("Chains with an auto selector cannot be scanned");
            }
            return scanCloneBlocker(scanChainHop(chain->list.first()));
        }

        struct TestBuildOptions {
            QString chainPrefix = tags::testChainPrefix;
            bool validate = true;
            bool groupHops = true;
        };

        class ScopedProfileOverrides {
        public:
            explicit ScopedProfileOverrides(const QHash<int, std::shared_ptr<Profile>> *overrides)
                : previous_(buildProfileOverrides) {
                buildProfileOverrides = overrides;
            }

            ~ScopedProfileOverrides() { buildProfileOverrides = previous_; }

            ScopedProfileOverrides(const ScopedProfileOverrides &) = delete;
            ScopedProfileOverrides &operator=(const ScopedProfileOverrides &) = delete;

        private:
            const QHash<int, std::shared_ptr<Profile>> *previous_;
        };

    } // namespace

    namespace {
    std::shared_ptr<BuildTestConfigResult> buildTestConfigImpl(const QList<std::shared_ptr<Profile> > &profiles,
                                                               const TestBuildOptions &options)
    {
        auto res = std::make_shared<BuildTestConfigResult>();
        const GenerateEndpointScope endpointScope;
        // outbound::Build() cannot see the BuildContext.
        SetBuildingTestConfig(true);
        const auto clearTestBuildFlag = qScopeGuard([] { SetBuildingTestConfig(false); });
        BuildContext ctx;
        ctx.policy = PolicyFor(BuildPurpose::Test);
#ifdef Q_OS_LINUX
        ctx.isResolvedUsed = isSystemdResolvedDefaultResolver();
#endif
        // Tests mix groups, so they keep the global routing profile's DNS.
        const ScopedBuildRoute routeScope(dataManager->settingsRepo->current_route_id);
        ctx.dns = RouteDns::Current(&ctx.routeOwnsDns);

        HopNeeds echNeeds;
        for (const auto &item : profiles) {
            if (item == nullptr) continue;
            for (int hopId : unwrapChain(item->id)) {
                if (const auto hop = getProfile(hopId); hop != nullptr) collectEchQueryName(echNeeds, *hop);
            }
        }

        buildDNSSection(ctx, echNeeds, {}, false);
        if (!ctx.error.isEmpty())
        {
            res->error = ctx.error;
            return res;
        }
        buildLogSection(ctx);
        buildCertificateSection(ctx);
        buildNTPSection(ctx);
        int suffix = 1;

        int xrayPortIdx=0;
        int xrayCount=0;
        int chainCount=0;
        int fullConfigCount=0;
        const auto frontIsXrayFullConfig = [&options](const std::shared_ptr<Profile> &item) {
            if (!options.groupHops) return false;
            const auto group = dataManager->groupsRepo->GetGroup(item->gid);
            const auto front = group != nullptr && group->front_proxy_id >= 0 ? getProfile(group->front_proxy_id) : nullptr;
            return front != nullptr && isXrayFullConfig(front);
        };
        for (const auto& proxy : profiles) {
            if (proxy->outbound->IsXray()) xrayCount++;
            if (proxy->type == "chain") chainCount++;
            if (isXrayFullConfig(proxy) || frontIsXrayFullConfig(proxy)) fullConfigCount++;
        }
        // assume all chains transition twice and allocate port for them.
        auto xrayPorts = MkManyPorts(xrayCount + 2*chainCount + fullConfigCount);
        // One probe for every drained full config too: they run side by side, and separate probes can deal a port twice.
        auto fullConfigPorts = xrayPorts.mid(xrayCount + 2*chainCount);
        const auto takeFullConfigPort = [&fullConfigPorts] { return fullConfigPorts.isEmpty() ? -1 : fullConfigPorts.takeFirst(); };

        // A validated batch marks a profile that fails to build as failed and tests the rest; a scan stops at the first.
        const auto skipFailedBuild = [&](const std::shared_ptr<Profile> &item, const QString &error) {
            if (!options.validate) return false;
            MW_show_log(QObject::tr("Skipped testing %1: %2").arg(item->outbound->DisplayTypeAndName(), error));
            item->SetLatency(-1);
            return true;
        };

        for (const auto& item : profiles)
        {
            const auto candidate = classifyTestCandidate(item);
            if (candidate.kind == testCandidate::Skip)
            {
                MW_show_log(candidate.skipReason);
                continue;
            }
            if (candidate.kind == testCandidate::XrayFullConfig)
            {
                if (options.validate && !IsValid(item)) {
                    MW_show_log("Skipping invalid custom Xray full config: " + item->outbound->name);
                    item->SetLatency(-1);
                    continue;
                }
                const auto out = buildOutboundChain(ctx, {
                    .hopIDs = {item->id},
                    .prefix = hopTag(tags::testXrayFullPrefix, item->id),
                    .xrayFullConfigPort = takeFullConfigPort(),
                });
                if (!out.error.isEmpty()) {
                    if (skipFailedBuild(item, out.error)) continue;
                    res->error = out.error;
                    return res;
                }
                absorbChain(ctx, out);
                if (out.xrayFullConfig.isEmpty()) {
                    MW_show_log("Custom Xray full config produced no Xray config: " + item->outbound->name);
                    item->SetLatency(-1);
                    continue;
                }
                res->xrayFullConfigs << QJsonObject2QString(out.xrayFullConfig, false);
                res->outboundTags << out.ingressTag;
                res->tag2entID.insert(out.ingressTag, item->id);
                continue;
            }
            if (options.validate && !IsValid(item)) {
                MW_show_log("Skipping invalid config: " + item->outbound->name);
                item->SetLatency(-1);
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
            if (options.groupHops) {
                auto group = dataManager->groupsRepo->GetGroup(item->gid);
                if (group == nullptr) {
                    res->error = "Null group on profile, data is corrupted";
                    return res;
                }
                if (group->landing_proxy_id >= 0) IDs.prepend(group->landing_proxy_id);
                if (group->front_proxy_id >= 0) IDs.append(group->front_proxy_id);
            }
            int singToXrayPort = -1;
            int xrayToSingPort = -1;
            if (item->outbound->IsXray()) singToXrayPort = xrayPorts[xrayPortIdx++];
            if (item->type == "chain") {
                singToXrayPort = xrayPorts[xrayPortIdx++];
                xrayToSingPort = xrayPorts[xrayPortIdx++];
            }
            const auto out = buildOutboundChain(ctx, {
                .hopIDs = IDs,
                .prefix = hopTag(options.chainPrefix, suffix),
                .singToXrayPort = singToXrayPort,
                .xrayToSingPort = xrayToSingPort,
                .xrayFullConfigPort = frontIsXrayFullConfig(item) ? takeFullConfigPort() : -1,
            });
            if (!out.error.isEmpty()) {
                if (!skipFailedBuild(item, out.error)) {
                    res->error = out.error;
                    return res;
                }
                suffix++;
                continue;
            }
            if (chainNeedsMissingTor(out)) {
                if (!skipFailedBuild(item, TorNotDownloadedError())) {
                    res->error = TorNotDownloadedError();
                    return res;
                }
                suffix++;
                continue;
            }
            absorbChain(ctx, out);
            // A custom Xray config as the group's front proxy ends this chain.
            if (!out.xrayFullConfig.isEmpty()) res->xrayFullConfigs << QJsonObject2QString(out.xrayFullConfig, false);
            res->outboundTags << out.ingressTag;
            res->tag2entID.insert(out.ingressTag, item->id);
            suffix++;
        }
        buildXrayConfig(ctx);
        if (!ctx.error.isEmpty()) {
            res->error = ctx.error;
            return res;
        }
        ctx.outbounds << QJsonObject{{"type", "direct"}, {"tag", tags::direct}};
        ctx.result->coreConfig["outbounds"] = ctx.outbounds;
        ctx.result->coreConfig["endpoints"] = ctx.endpoints;
        const auto inboundArr = ctx.bridges.singInbounds(BridgeTagScheme::ByPort);
        // There is no route.final here, so an unrouted bridge would exit through the batch's first outbound.
        const auto routeRules = ctx.bridges.singRouteRules(BridgeTagScheme::ByPort);
        // A running Tun owns the OS resolver, so the sidecar resolves against the probe box's dns-direct instead.
        if (ctx.result->isXrayNeeded || !res->xrayFullConfigs.isEmpty()) {
            res->xrayDnsStrategy = getXrayOutboundDomainStrategy();
        }
        QJsonObject routeObj{
                {"auto_detect_interface", true},
                {"default_domain_resolver", directDomainResolver()},
        };
        if (!routeRules.isEmpty()) routeObj["rules"] = routeRules;
        ctx.result->coreConfig["route"] = routeObj;
        ctx.result->coreConfig["inbounds"] = inboundArr;
        res->coreConfig = ctx.result->coreConfig;
        res->xrayConfig = ctx.result->xrayConfig;
        res->isXrayNeeded = ctx.result->isXrayNeeded;

        return res;
    }
    } // namespace

    std::shared_ptr<BuildTestConfigResult> BuildTestConfig(const QList<std::shared_ptr<Profile> > &profiles)
    {
        return buildTestConfigImpl(profiles, {});
    }

    QString ValidateScanBase(const std::shared_ptr<Profile> &base)
    {
        if (auto blocker = scanBaseBlocker(base); !blocker.isEmpty()) return blocker;
        auto clone = scanDeepClone(base);
        if (clone == nullptr) return QObject::tr("The profile could not be copied for the scan");
        clone->id = kScanSyntheticIdBase;
        const QHash<int, std::shared_ptr<Profile>> overrides{{clone->id, clone}};
        const ScopedProfileOverrides scoped(&overrides);
        SetBuildingTestConfig(true);
        const auto clearTestBuildFlag = qScopeGuard([] { SetBuildingTestConfig(false); });
        bool coreUnreachable = false;
        if (IsValid(clone, &coreUnreachable)) return {};
        if (coreUnreachable) return QObject::tr("The profile could not be checked: core unreachable");
        return QObject::tr("The profile is not valid; the log has the details");
    }

    ScanTestBuild BuildScanTestConfig(const std::shared_ptr<Profile> &base, const QList<ScanTestTarget> &targets)
    {
        const GenerateEndpointScope endpointScope;
        ScanTestBuild out;
        if (auto blocker = scanBaseBlocker(base); !blocker.isEmpty()) {
            out.error = blocker;
            return out;
        }
        const auto *chain = base->Chain();
        std::shared_ptr<Profile> dialedHop;
        QString exitType = base->type;
        if (chain != nullptr) {
            dialedHop = scanChainHop(chain->list.first());
            const auto exitHop = chain->list.size() == 1 ? dialedHop : scanChainHop(chain->list.last());
            exitType = exitHop != nullptr ? exitHop->type : QString();
        }

        QHash<int, std::shared_ptr<Profile>> overrides;
        QHash<int, int> synth2target;
        QList<std::shared_ptr<Profile>> items;
        int nextId = kScanSyntheticIdBase;
        for (int i = 0; i < targets.size(); ++i) {
            const auto &target = targets[i];
            std::shared_ptr<Profile> item;
            if (chain != nullptr) {
                if (auto hop = CloneProfileWithServer(dialedHop, target.address, target.port); hop != nullptr) {
                    item = scanDeepClone(base);
                    if (item != nullptr && item->Chain() != nullptr) {
                        hop->id = nextId--;
                        overrides.insert(hop->id, hop);
                        item->Chain()->list[0] = hop->id;
                    } else {
                        item = nullptr;
                    }
                }
            } else {
                item = CloneProfileWithServer(base, target.address, target.port);
            }
            if (item == nullptr) {
                out.unsupported << i;
                continue;
            }
            item->id = nextId--;
            overrides.insert(item->id, item);
            synth2target.insert(item->id, i);
            items << item;
        }
        if (items.isEmpty()) {
            if (!targets.isEmpty()) out.error = QObject::tr("The profile cannot be pointed at the scanned addresses");
            return out;
        }

        {
            const ScopedProfileOverrides scoped(&overrides);
            out.build = buildTestConfigImpl(items, {.chainPrefix = QStringLiteral("scan"), .validate = false, .groupHops = false});
        }
        if (!out.build->error.isEmpty()) {
            out.error = out.build->error;
            return out;
        }
        for (auto it = out.build->tag2entID.cbegin(); it != out.build->tag2entID.cend(); ++it)
            out.tag2target.insert(it.key(), synth2target.value(it.value(), -1));
        if (isVpnEndpointType(exitType)) out.vpnEndpointTags = out.build->outboundTags;
        return out;
    }

    ChainStepTestBuild BuildChainStepTestConfig(const QList<int> &hops, int gid)
    {
        const GenerateEndpointScope endpointScope;
        ChainStepTestBuild out;
        if (hops.isEmpty()) {
            out.error = QObject::tr("The chain has no hops");
            return out;
        }
        int front = -1;
        int landing = -1;
        if (const auto group = dataManager->groupsRepo->GetGroup(gid); group != nullptr) {
            front = group->front_proxy_id;
            landing = group->landing_proxy_id;
        }
        QList<int> involved = hops;
        if (front >= 0) involved << front;
        if (landing >= 0) involved << landing;
        for (const int id : involved) {
            const auto hop = getProfile(id);
            if (hop == nullptr || hop->outbound == nullptr) {
                out.error = id == front || id == landing ? QObject::tr("The group's front or landing proxy no longer exists")
                                                         : QObject::tr("A hop of the chain no longer exists");
                return out;
            }
            if (hop->outbound->IsExtraCore() || hop->outbound->IsXrayFullConfig()) {
                out.error = QObject::tr("Chains with an extra-core or Xray full config hop cannot be tested");
                return out;
            }
            if (hop->type == "autoselector") {
                out.error = QObject::tr("Chains with an auto selector cannot be tested hop by hop; test its members instead");
                return out;
            }
        }

        const auto invalid = invalidProfileIDs(QSet<int>(involved.cbegin(), involved.cend()).values());
        if (invalid.contains(front) || invalid.contains(landing)) {
            out.error = QObject::tr("The group's front or landing proxy is not valid; the log has the details");
            return out;
        }
        for (int i = 0; i < hops.size(); ++i) {
            if (invalid.contains(hops[i])) out.invalidHops << i;
        }

        QHash<int, std::shared_ptr<Profile>> overrides;
        QHash<int, ChainStepProbe> synth2probe;
        QHash<int, QString> synthExitType;
        QList<std::shared_ptr<Profile>> items;
        int nextId = kChainStepSyntheticIdBase;
        const auto addProbe = [&](QList<int> list, const ChainStepProbe &probe) {
            const auto exit = getProfile(list.last());
            if (front >= 0) list.prepend(front);
            auto item = ProfilesRepo::NewProfile("chain");
            if (item == nullptr || item->Chain() == nullptr) return;
            item->Chain()->list = list;
            item->gid = gid;
            item->id = nextId--;
            overrides.insert(item->id, item);
            synth2probe.insert(item->id, probe);
            synthExitType.insert(item->id, exit != nullptr ? exit->type : QString());
            items << item;
        };
        bool pathValid = true;
        for (int i = 0; i < hops.size(); ++i) {
            if (out.invalidHops.contains(i)) {
                pathValid = false;
                continue;
            }
            if (i > 0) addProbe({hops[i]}, {i, true});
            if (pathValid) addProbe(hops.mid(0, i + 1), {i, false});
        }
        if (landing >= 0) {
            out.hasLanding = true;
            addProbe({landing}, {static_cast<int>(hops.size()), true});
            if (pathValid) addProbe(QList<int>(hops) << landing, {static_cast<int>(hops.size()), false});
        }
        if (items.isEmpty()) return out;

        {
            const ScopedProfileOverrides scoped(&overrides);
            out.build = buildTestConfigImpl(items, {.chainPrefix = QStringLiteral("step"), .validate = false, .groupHops = false});
        }
        if (!out.build->error.isEmpty()) {
            out.error = out.build->error;
            return out;
        }
        for (auto it = out.build->tag2entID.cbegin(); it != out.build->tag2entID.cend(); ++it) {
            out.tag2probe.insert(it.key(), synth2probe.value(it.value()));
            if (isVpnEndpointType(synthExitType.value(it.value()))) out.vpnEndpointTags << it.key();
        }
        return out;
    }
}
