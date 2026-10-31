#pragma once

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
        // Eligible members, best first, capped at poolCap.
        QList<int> pool;
        // The prefix of pool that actually enters the config.
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
}
