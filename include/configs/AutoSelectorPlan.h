#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <memory>

namespace Configs
{
    class Profile;
    class autoSelector;

    enum class AutoSelectorSkip
    {
        None = 0,
        Missing,
        MetaType,
        CoreTransitions,
        ExtraCore,
        FullConfig,
        Malformed,
        Tailscale,
        ManagementEndpoint,
        NameFilter,
        CountryFilter,
        Unavailable,
        XrayFullChained,
    };

    QString AutoSelectorSkipReason(AutoSelectorSkip skip);

    struct AutoSelectorPlan
    {
        // Eligible members, best first, capped at poolCap; in priority mode tier by tier, each tier keeping a share.
        QList<int> pool;
        // The members of pool that enter the config, capped at buildLimit the same way.
        QList<int> build;

        // Profiles across every tracked group that still exists, the selector itself excluded.
        int membersInGroups = 0;
        // Tracked groups that no longer exist; they are skipped.
        int missingGroups = 0;
        int eligible = 0;
        int rankedByTest = 0;
        // Failed-last-test members kept anyway, because excluding them would have emptied the pool.
        int keptUnavailable = 0;
        QList<QPair<AutoSelectorSkip, int>> skipped;
        bool truncated = false;
        int poolCapUsed = 0;
        int buildLimitUsed = 0;
        // The caller should run a client-side URL test before building.
        bool needsRanking = false;

        // Priority strategy only. Member id -> index of the first tier it matches, tiers.size() when none.
        QHash<int, int> tierOf;
        // Members of build per tier; the extra last slot counts those matching no tier.
        QList<int> tierBuilt;
        // Eligible members per tier before the caps, laid out like tierBuilt.
        QList<int> tierEligible;

        QString error;

        [[nodiscard]] int skippedCount() const
        {
            int total = 0;
            for (const auto &entry : skipped) total += entry.second;
            return total;
        }
    };

    AutoSelectorPlan PlanAutoSelector(const std::shared_ptr<Profile> &ent);

    QList<int> AutoSelectorRankingCandidates(const std::shared_ptr<Profile> &ent);

    // `stale` = members whose stored result no longer reflects reality; they are re-measured.
    // `working`, when set, receives how many of the other candidates already hold a fresh successful result.
    QList<int> AutoSelectorUnmeasuredCandidates(const std::shared_ptr<Profile> &ent,
                                                const QList<int> &stale = {}, int *working = nullptr);

    // Whether the ranking counts `member` as working: a successful result inside the selector's validity window.
    bool AutoSelectorMemberWorks(const std::shared_ptr<Profile> &ent, const std::shared_ptr<Profile> &member);

    // Working members a selector needs before it may start while its ranking sweep still runs.
    int AutoSelectorStartQuorum(const std::shared_ptr<Profile> &ent);

    // Whether the build the plan takes now would drop a member of lastBuilt or add one that works.
    bool AutoSelectorBuildChanged(const std::shared_ptr<Profile> &ent);

    // `untrusted` members rank as untested whatever their stored result says.
    QList<int> RerankAutoSelectorPool(const std::shared_ptr<Profile> &ent, const QList<int> &untrusted = {});

    // Moves `members` behind the rest of the pool, so the next build tries the others first.
    void DemoteAutoSelectorMembers(const std::shared_ptr<Profile> &ent, const QList<int> &members);

    // The auto selector a started profile runs: the profile itself, or the selector hop of a chain.
    std::shared_ptr<Profile> RunningSelectorOf(const std::shared_ptr<Profile> &ent);

    // A chain whose selector members dial through fixed hops, so a direct test says nothing about them.
    bool SelectorBehindHops(const std::shared_ptr<Profile> &ent);
}
