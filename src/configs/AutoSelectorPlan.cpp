#include "include/configs/AutoSelectorPlan.h"

#include "include/configs/common/Protocols.h"
#include "include/configs/outbounds/autoselector.h"
#include "include/configs/outbounds/chain.h"
#include "include/configs/outbounds/custom.h"
#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/entities/Group.h"
#include "include/database/entities/Profile.h"

#include <QDateTime>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <optional>

namespace Configs
{
    namespace
    {
        struct MemberTierFilter
        {
            int gid = -1;
            QRegularExpression name;
            bool hasName = false;
        };

        struct MemberFilters
        {
            QRegularExpression name;
            bool hasName = false;
            QSet<QString> countries;
            // "!RU": removed whether or not includes are given.
            QSet<QString> excludedCountries;
            // Priority strategy only, in tier order.
            QList<MemberTierFilter> tiers;
        };

        MemberFilters buildFilters(const autoSelector *selector)
        {
            MemberFilters filters;
            if (!selector->nameFilter.trimmed().isEmpty()) {
                filters.name = QRegularExpression(selector->nameFilter,
                                                  QRegularExpression::CaseInsensitiveOption);
                filters.hasName = filters.name.isValid();
            }
            for (const auto &item : selector->countryFilter.split(',', Qt::SkipEmptyParts)) {
                auto code = item.trimmed();
                const bool exclude = code.startsWith('!');
                if (exclude) code = code.mid(1).trimmed();
                if (code.isEmpty()) continue;
                (exclude ? filters.excludedCountries : filters.countries).insert(code.toUpper());
            }
            if (selector->Prioritized()) {
                for (const auto &tier : selector->tiers) {
                    MemberTierFilter tierFilter;
                    tierFilter.gid = tier.gid;
                    if (const auto pattern = tier.nameFilter.trimmed(); !pattern.isEmpty()) {
                        tierFilter.name = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
                        tierFilter.hasName = true;
                    }
                    filters.tiers << tierFilter;
                }
            }
            return filters;
        }

        // First match wins, tiers.size() when none does; an invalid pattern matches nothing.
        int memberTierOf(const std::shared_ptr<Profile> &member, const MemberFilters &filters)
        {
            const auto name = member->outbound->DisplayName();
            for (int i = 0; i < filters.tiers.size(); i++) {
                const auto &tier = filters.tiers[i];
                if (tier.gid >= 0 && member->gid != tier.gid) continue;
                if (tier.hasName && (!tier.name.isValid() || !tier.name.match(name).hasMatch())) continue;
                return i;
            }
            return static_cast<int>(filters.tiers.size());
        }

        bool hasFreshResult(const std::shared_ptr<Profile> &member, qint64 now)
        {
            if (member == nullptr || !member->HasLatencyResult()) return false;
            // Rows written before the timestamp column existed carry 0: expired, not fresh.
            if (member->latency_at <= 0) return false;
            return (now - member->latency_at) <= AutoSelectorResultValiditySecs(*member);
        }

        AutoSelectorSkip eligibilityOf(const std::shared_ptr<Profile> &member, const MemberFilters &filters)
        {
            if (member == nullptr || member->outbound == nullptr) return AutoSelectorSkip::Missing;
            if (member->type == "chain" || member->type == "autoselector") return AutoSelectorSkip::MetaType;
            if (member->type == "tailscale") return AutoSelectorSkip::Tailscale;
            if (Protocols::Is(member->type, Protocols::VpnEndpoint)) return AutoSelectorSkip::ManagementEndpoint;
            if (member->outbound->IsExtraCore()) return AutoSelectorSkip::ExtraCore;
            if (member->type == "custom") {
                const auto custom = member->Custom();
                if (custom == nullptr) return AutoSelectorSkip::Missing;
                // Only a sing-box full config is excluded; an Xray one gets its own instance.
                if (custom->type != Custom::CustomOutbound && custom->type != Custom::CustomXrayOutbound
                    && custom->type != Custom::CustomXrayFullConfig) {
                    return AutoSelectorSkip::FullConfig;
                }
                // A member whose JSON does not parse builds into an empty outbound that fails the shared build.
                if (QString2QJsonObject(custom->config).isEmpty()) return AutoSelectorSkip::Malformed;
            }
            if (filters.hasName && !filters.name.match(member->outbound->DisplayName()).hasMatch()) {
                return AutoSelectorSkip::NameFilter;
            }
            if (!filters.countries.isEmpty() || !filters.excludedCountries.isEmpty()) {
                // Unknown country is skipped either way: an exclusion cannot vouch for a profile never IP-tested.
                const auto country = member->test_country.trimmed().toUpper();
                if (country.isEmpty() || filters.excludedCountries.contains(country)
                    || (!filters.countries.isEmpty() && !filters.countries.contains(country))) {
                    return AutoSelectorSkip::CountryFilter;
                }
            }
            return AutoSelectorSkip::None;
        }

        // Mirrors entIDListtoEntList's budget: a chain may hand off sing-box <-> Xray at most once.
        int coreTransitions(const std::shared_ptr<Profile> &landing,
                            const std::shared_ptr<Profile> &member,
                            const std::shared_ptr<Profile> &front)
        {
            int transitions = 0;
            bool inXray = false;
            for (const auto &hop : {landing, member, front}) {
                if (hop == nullptr || hop->outbound == nullptr) continue;
                const bool xray = hop->outbound->IsXray();
                if (xray != inXray) transitions++;
                inXray = xray;
            }
            return transitions;
        }

        // chainScanError's rules per member: hitting them in buildOutboundChain fails the whole build.
        bool xrayFullConfigFitsChain(const std::shared_ptr<Profile> &landing,
                                     const std::shared_ptr<Profile> &front)
        {
            if (front != nullptr) return false;
            if (landing == nullptr || landing->outbound == nullptr) return true;
            return !landing->outbound->IsXray() && !landing->outbound->IsXrayFullConfig()
                   && !landing->outbound->IsExtraCore();
        }

        // Same test as the core's qualifies(): at or over the cap, a member is only a fallback.
        bool overCap(int latency, const autoSelector *selector)
        {
            return latency > 0 && selector->maxRTTms > 0 && latency >= selector->maxRTTms;
        }

        bool qualifyingLatency(int latency, const autoSelector *selector)
        {
            return latency > 0 && !overCap(latency, selector);
        }

        // Untested outranks over-cap and failed: it may still qualify, so a fresh subscription gets explored.
        int latencyRank(int latency, const autoSelector *selector)
        {
            if (latency > 0) return overCap(latency, selector) ? 2 : 0;
            if (latency == 0) return 1;
            return 3;
        }

        int effectiveLatency(const std::shared_ptr<Profile> &member, qint64 now)
        {
            return hasFreshResult(member, now) ? member->latency : 0;
        }

        bool byLatency(int leftLatency, int rightLatency, const autoSelector *selector)
        {
            const int leftRank = latencyRank(leftLatency, selector);
            const int rightRank = latencyRank(rightLatency, selector);
            if (leftRank != rightRank) return leftRank < rightRank;
            if (leftLatency > 0) return leftLatency < rightLatency;
            return false;
        }

        // Latencies are read once, as members load: a sweep still running rewrites them, and a comparator that changes its
        // answer mid-sort is UB.
        struct MemberSnapshot
        {
            QList<int> ids;
            QHash<int, int> latency;
        };

        void sortByLatency(QList<int> &ids, const QHash<int, int> &latency, const autoSelector *selector)
        {
            std::stable_sort(ids.begin(), ids.end(), [&latency, selector](int left, int right) {
                return byLatency(latency.value(left), latency.value(right), selector);
            });
        }

        // GetProfileBatch holds the repo lock for the whole call, and a main-thread GetProfile waits on it.
        constexpr qsizetype kMemberLoadChunk = 512;

        MemberSnapshot eligibleMembers(const std::shared_ptr<Profile> &ent, const autoSelector *selector, qint64 now,
                                       AutoSelectorPlan *plan)
        {
            MemberSnapshot members;
            if (selector->gids.isEmpty()) {
                if (plan != nullptr) plan->error = "Auto selector tracks no group";
                return members;
            }
            QList<std::shared_ptr<Group>> groups;
            for (int gid : selector->gids) {
                if (auto group = dataManager->groupsRepo->GetGroup(gid); group != nullptr) {
                    groups << group;
                } else if (plan != nullptr) {
                    plan->missingGroups++;
                }
            }
            if (groups.isEmpty()) {
                if (plan != nullptr) plan->error = "Auto selector points at a group that no longer exists";
                return members;
            }
            const auto filters = buildFilters(selector);
            QMap<AutoSelectorSkip, int> skips;
            QSet<int> seen;
            for (const auto &group : groups) {
                // Each member is wrapped in its own group's proxies, so the chain checks run per group.
                const auto landing = group->landing_proxy_id >= 0
                                         ? dataManager->profilesRepo->GetProfile(group->landing_proxy_id)
                                         : nullptr;
                const auto front = group->front_proxy_id >= 0
                                       ? dataManager->profilesRepo->GetProfile(group->front_proxy_id)
                                       : nullptr;
                QList<int> ids;
                for (int id : group->DisplayedProfiles()) {
                    if (id == ent->id || seen.contains(id)) continue;
                    seen.insert(id);
                    ids << id;
                }
                if (plan != nullptr) plan->membersInGroups += static_cast<int>(ids.size());
                for (qsizetype offset = 0; offset < ids.size(); offset += kMemberLoadChunk) {
                    const auto chunk = ids.mid(offset, kMemberLoadChunk);
                    QHash<int, std::shared_ptr<Profile>> loaded;
                    loaded.reserve(chunk.size());
                    for (const auto &member : dataManager->profilesRepo->GetProfileBatch(chunk)) {
                        if (member != nullptr) loaded.insert(member->id, member);
                    }
                    for (int id : chunk) {
                        const auto member = loaded.value(id);
                        const auto skip = eligibilityOf(member, filters);
                        if (skip != AutoSelectorSkip::None) {
                            skips[skip]++;
                            continue;
                        }
                        if (member->outbound->IsXrayFullConfig()) {
                            if (!xrayFullConfigFitsChain(landing, front)) {
                                skips[AutoSelectorSkip::XrayFullChained]++;
                                continue;
                            }
                        } else if (coreTransitions(landing, member, front) > 2) {
                            skips[AutoSelectorSkip::CoreTransitions]++;
                            continue;
                        }
                        if (plan != nullptr && selector->Prioritized()) plan->tierOf.insert(id, memberTierOf(member, filters));
                        const int latency = effectiveLatency(member, now);
                        members.ids << id;
                        members.latency.insert(id, latency);
                        if (plan != nullptr && latency > 0) plan->rankedByTest++;
                    }
                }
            }
            if (plan != nullptr) {
                for (auto it = skips.constBegin(); it != skips.constEnd(); ++it) {
                    plan->skipped << qMakePair(it.key(), it.value());
                }
            }
            return members;
        }

        // The persisted order is the core's ranking prior; newcomers follow by latency, over-cap then failed members last.
        QList<int> orderMembers(const MemberSnapshot &members, const QList<int> &persistedPool, const autoSelector *selector)
        {
            const QSet<int> eligible(members.ids.begin(), members.ids.end());
            QList<int> ordered;
            ordered.reserve(members.ids.size());
            QSet<int> placed;
            for (int id : persistedPool) {
                if (!eligible.contains(id) || placed.contains(id)) continue;
                ordered << id;
                placed.insert(id);
            }

            QList<int> newcomers;
            for (int id : members.ids) {
                if (!placed.contains(id)) newcomers << id;
            }
            sortByLatency(newcomers, members.latency, selector);
            ordered << newcomers;

            const auto demotion = [&members, selector](int id) {
                const int latency = members.latency.value(id);
                return latency < 0 ? 2 : overCap(latency, selector) ? 1 : 0;
            };
            std::stable_sort(ordered.begin(), ordered.end(),
                             [&demotion](int left, int right) { return demotion(left) < demotion(right); });
            return ordered;
        }

        // Cuts a tier-sorted list to `limit` without crowding out a tier: each one first gets a fair share, the rest goes in tier order.
        QList<int> capByTier(const QList<int> &ordered, int limit, const QHash<int, int> &tierOf, int noTier)
        {
            if (ordered.size() <= limit) return ordered;
            QList<QList<int>> byTier(noTier + 1);
            for (int id : ordered) byTier[tierOf.value(id, noTier)] << id;
            int nonEmpty = 0;
            for (const auto &members : std::as_const(byTier)) {
                if (!members.isEmpty()) nonEmpty++;
            }
            const int reserve = std::max(1, limit / (2 * nonEmpty));
            QList<int> quota(byTier.size(), 0);
            int given = 0;
            for (int i = 0; i < byTier.size(); i++) {
                quota[i] = std::min(static_cast<int>(byTier[i].size()), reserve);
                given += quota[i];
            }
            if (given > limit) {
                // More tiers than slots: one each, in tier order.
                given = 0;
                for (int i = 0; i < byTier.size(); i++) {
                    quota[i] = !byTier[i].isEmpty() && given < limit ? 1 : 0;
                    given += quota[i];
                }
            }
            for (int i = 0; i < byTier.size() && given < limit; i++) {
                const int extra = std::min(static_cast<int>(byTier[i].size()) - quota[i], limit - given);
                quota[i] += extra;
                given += extra;
            }
            QList<int> capped;
            capped.reserve(given);
            for (int i = 0; i < byTier.size(); i++) capped << byTier[i].mid(0, quota[i]);
            return capped;
        }

        // A copy: Normalize() writes even when it changes nothing, and other threads read the live bean.
        std::optional<autoSelector> normalizedSettings(const std::shared_ptr<Profile> &ent)
        {
            auto *selector = ent != nullptr ? ent->AutoSelector() : nullptr;
            if (selector == nullptr) return std::nullopt;
            std::optional<autoSelector> settings(std::in_place, *selector);
            settings->Normalize();
            return settings;
        }

        MemberSnapshot planSelector(AutoSelectorPlan &plan, const std::shared_ptr<Profile> &ent, const autoSelector *selector)
        {
            plan.poolCapUsed = autoSelector::kPoolCap;
            plan.buildLimitUsed = selector->buildLimit;

            auto members = eligibleMembers(ent, selector, QDateTime::currentSecsSinceEpoch(), &plan);
            if (!plan.error.isEmpty()) return members;
            plan.eligible = static_cast<int>(members.ids.size());
            if (members.ids.isEmpty()) {
                plan.error = "Auto selector has no usable members";
                return members;
            }

            auto ordered = orderMembers(members, selector->pool, selector);
            const int noTier = static_cast<int>(selector->tiers.size());
            plan.truncated = ordered.size() > autoSelector::kPoolCap;
            if (selector->Prioritized()) {
                // The core reads this order as its ranking prior, and capByTier keeps it.
                std::stable_sort(ordered.begin(), ordered.end(), [&plan, noTier](int left, int right) {
                    return plan.tierOf.value(left, noTier) < plan.tierOf.value(right, noTier);
                });
                plan.tierEligible = QList<int>(noTier + 1, 0);
                for (int id : std::as_const(ordered)) plan.tierEligible[plan.tierOf.value(id, noTier)]++;
                ordered = capByTier(ordered, autoSelector::kPoolCap, plan.tierOf, noTier);
                plan.pool = ordered;
                plan.build = capByTier(ordered, selector->buildLimit, plan.tierOf, noTier);
                plan.tierBuilt = QList<int>(noTier + 1, 0);
                for (int id : std::as_const(plan.build)) plan.tierBuilt[plan.tierOf.value(id, noTier)]++;
            } else {
                if (plan.truncated) ordered = ordered.mid(0, autoSelector::kPoolCap);
                plan.pool = ordered;
                plan.build = ordered.mid(0, std::min<qsizetype>(ordered.size(), selector->buildLimit));
            }

            if (ordered.size() > selector->buildLimit) {
                int unranked = 0;
                for (int id : std::as_const(plan.build)) {
                    if (members.latency.value(id) == 0) unranked++;
                }
                plan.needsRanking = unranked > 0;
            }
            return members;
        }
    } // namespace

    qint64 AutoSelectorResultValiditySecs(const Profile &member)
    {
        return member.IsUnavailable() ? autoSelector::kFailureValiditySecs : autoSelector::kSuccessValiditySecs;
    }

    QString AutoSelectorSkipReason(AutoSelectorSkip skip)
    {
        switch (skip) {
            case AutoSelectorSkip::Missing: return QObject::tr("missing profile");
            case AutoSelectorSkip::MetaType: return QObject::tr("chain or auto selector");
            case AutoSelectorSkip::CoreTransitions: return QObject::tr("needs too many core switches");
            case AutoSelectorSkip::ExtraCore: return QObject::tr("extra-core profile");
            case AutoSelectorSkip::FullConfig: return QObject::tr("full config profile");
            case AutoSelectorSkip::Malformed: return QObject::tr("config does not parse");
            case AutoSelectorSkip::Tailscale: return QObject::tr("Tailscale profile");
            case AutoSelectorSkip::ManagementEndpoint: return QObject::tr("OpenVPN or OpenConnect profile");
            case AutoSelectorSkip::NameFilter: return QObject::tr("filtered out by name");
            case AutoSelectorSkip::CountryFilter: return QObject::tr("filtered out by country");
            case AutoSelectorSkip::XrayFullChained:
                return QObject::tr("Xray full config cannot be combined with the group's proxies");
            default: return {};
        }
    }

    AutoSelectorPlan PlanAutoSelector(const std::shared_ptr<Profile> &ent)
    {
        AutoSelectorPlan plan;
        if (ent == nullptr) {
            plan.error = "Null profile passed to PlanAutoSelector";
            return plan;
        }
        const auto settings = normalizedSettings(ent);
        if (!settings) {
            plan.error = "Profile is not an auto selector, data is corrupted";
            return plan;
        }
        planSelector(plan, ent, &*settings);
        return plan;
    }

    QList<int> AutoSelectorRankingCandidates(const std::shared_ptr<Profile> &ent)
    {
        const auto settings = normalizedSettings(ent);
        if (!settings) return {};
        return eligibleMembers(ent, &*settings, QDateTime::currentSecsSinceEpoch(), nullptr).ids;
    }

    QList<int> AutoSelectorUnmeasuredCandidates(const std::shared_ptr<Profile> &ent, const QList<int> &stale, int *working)
    {
        if (working != nullptr) *working = 0;
        const auto settings = normalizedSettings(ent);
        if (!settings) return {};

        const auto candidates = eligibleMembers(ent, &*settings, QDateTime::currentSecsSinceEpoch(), nullptr);
        const QSet<int> staleSet(stale.begin(), stale.end());
        QList<int> needed;
        for (int id : candidates.ids) {
            if (staleSet.contains(id)) {
                needed << id;
                continue;
            }
            const int latency = candidates.latency.value(id);
            if (latency == 0) needed << id;
            else if (qualifyingLatency(latency, &*settings) && working != nullptr) ++*working;
        }
        return needed;
    }

    bool AutoSelectorMemberWorks(const std::shared_ptr<Profile> &ent, const std::shared_ptr<Profile> &member)
    {
        if (ent == nullptr) return false;
        const auto selector = ent->AutoSelector();
        return selector != nullptr && qualifyingLatency(effectiveLatency(member, QDateTime::currentSecsSinceEpoch()), selector);
    }

    int AutoSelectorStartQuorum(const std::shared_ptr<Profile> &ent)
    {
        const auto settings = normalizedSettings(ent);
        return settings ? settings->ActiveSize() : 0;
    }

    bool AutoSelectorBuildChanged(const std::shared_ptr<Profile> &ent)
    {
        const auto settings = normalizedSettings(ent);
        if (!settings) return false;
        AutoSelectorPlan plan;
        const auto members = planSelector(plan, ent, &*settings);
        if (!plan.error.isEmpty()) return false;

        const QSet<int> next(plan.build.begin(), plan.build.end());
        const QSet<int> built(settings->lastBuilt.begin(), settings->lastBuilt.end());
        for (int id : built) {
            if (!next.contains(id)) return true;
        }
        // The build drops members that fail validation, so an unbuilt one counts only when it is known to work.
        for (int id : next) {
            if (!built.contains(id) && qualifyingLatency(members.latency.value(id), &*settings)) return true;
        }
        return false;
    }

    QList<int> RerankAutoSelectorPool(const std::shared_ptr<Profile> &ent, const QList<int> &untrusted)
    {
        const auto settings = normalizedSettings(ent);
        if (!settings) return {};

        auto members = eligibleMembers(ent, &*settings, QDateTime::currentSecsSinceEpoch(), nullptr);
        for (int id : untrusted) {
            if (const auto it = members.latency.find(id); it != members.latency.end()) *it = 0;
        }
        sortByLatency(members.ids, members.latency, &*settings);
        if (members.ids.size() > autoSelector::kPoolCap) members.ids = members.ids.mid(0, autoSelector::kPoolCap);

        auto *selector = ent->AutoSelector();
        selector->pool = members.ids;
        selector->poolRankedAt = QDateTime::currentSecsSinceEpoch();
        dataManager->profilesRepo->Save(ent);
        return members.ids;
    }

    void DemoteAutoSelectorMembers(const std::shared_ptr<Profile> &ent, const AutoSelectorPlan &plan, const QList<int> &members)
    {
        if (ent == nullptr || members.isEmpty() || !plan.error.isEmpty()) return;
        auto selector = ent->AutoSelector();
        if (selector == nullptr) return;
        const QSet<int> demoted(members.begin(), members.end());
        QList<int> pool;
        QList<int> tail;
        for (int id : plan.pool) (demoted.contains(id) ? tail : pool) << id;
        selector->pool = pool << tail;
        dataManager->profilesRepo->Save(ent);
    }

    std::shared_ptr<Profile> RunningSelectorOf(const std::shared_ptr<Profile> &ent)
    {
        if (ent == nullptr) return nullptr;
        if (ent->type == "autoselector") return ent;
        const auto *chain = ent->Chain();
        if (chain == nullptr) return nullptr;
        for (int id : chain->list) {
            if (auto hop = dataManager->profilesRepo->GetProfile(id); hop != nullptr && hop->type == "autoselector") return hop;
        }
        return nullptr;
    }

    bool SelectorBehindHops(const std::shared_ptr<Profile> &ent)
    {
        const auto *chain = ent != nullptr ? ent->Chain() : nullptr;
        if (chain == nullptr) return false;
        const auto selector = RunningSelectorOf(ent);
        if (selector == nullptr) return false;
        if (chain->list.indexOf(selector->id) > 0) return true;
        const auto group = dataManager->groupsRepo->GetGroup(ent->gid);
        return group != nullptr && group->front_proxy_id >= 0;
    }
}
