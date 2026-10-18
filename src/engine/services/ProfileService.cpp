#include "include/engine/services/ProfileService.h"

#include <QHash>
#include <QSet>

#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EditLocks.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/ThreadCheck.h"
#include "include/global/Utils.hpp"

namespace Engine::ProfileService {
    std::optional<ServiceError> CheckDelete(const QList<int> &profileIds) {
        ENGINE_CHECK_MAIN_THREAD();
        if (!IsBeingEdited(EditTarget::Profile)) return std::nullopt;
        for (const int id : profileIds) {
            if (!IsBeingEdited(EditTarget::Profile, id)) continue;
            return ServiceError{409, QStringLiteral("editing"), QStringLiteral("Profile %1 is open in an editor in Throne's window").arg(id)};
        }
        return std::nullopt;
    }

    std::optional<ServiceError> Delete(const QList<int> &profileIds, int *deleted) {
        ENGINE_CHECK_MAIN_THREAD();
        if (deleted != nullptr) *deleted = 0;
        if (auto refusal = CheckDelete(profileIds)) return refusal;

        auto *profiles = Configs::dataManager->profilesRepo.get();
        QHash<int, int> groupOf;
        QList<int> requested;
        for (const auto &profile : profiles->GetProfileBatch(profileIds)) {
            if (groupOf.contains(profile->id)) continue;
            groupOf.insert(profile->id, profile->gid);
            requested.append(profile->id);
        }
        if (requested.isEmpty()) return std::nullopt;

        // None of them runs, so a started_id among them was left by a core crash; BatchDeleteProfiles would skip it.
        auto *settings = Configs::dataManager->settingsRepo.get();
        if (requested.contains(settings->started_id)) settings->UpdateStartedId(Configs::NoProfileId);

        QList<int> removed = requested;
        const bool ok = profiles->BatchDeleteProfiles(removed, false);
        QSet<int> survivors;
        if (ok) {
            const QSet<int> gone(removed.cbegin(), removed.cend());
            for (const int id : requested) {
                if (!gone.contains(id)) survivors.insert(id);
            }
        } else {
            for (const auto &profile : profiles->GetProfileBatch(requested)) survivors.insert(profile->id);
        }
        removed = requested;
        removed.removeIf([&survivors](int id) { return survivors.contains(id); });
        if (deleted != nullptr) *deleted = static_cast<int>(removed.size());

        if (!removed.isEmpty()) {
            const QSet<int> removedSet(removed.cbegin(), removed.cend());
            for (const int gid : Configs::dataManager->groupsRepo->GetAllGroupIds()) {
                const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
                if (group == nullptr) continue;
                bool changed = false;
                if (removedSet.contains(group->front_proxy_id)) {
                    group->front_proxy_id = -1;
                    changed = true;
                    MW_show_log(QStringLiteral("[Groups] %1: front proxy removed (its profile was deleted)").arg(group->name));
                }
                if (removedSet.contains(group->landing_proxy_id)) {
                    group->landing_proxy_id = -1;
                    changed = true;
                    MW_show_log(QStringLiteral("[Groups] %1: landing proxy removed (its profile was deleted)").arg(group->name));
                }
                if (changed) Configs::dataManager->groupsRepo->Save(group);
            }

            QList<int> affectedGroups;
            QHash<int, QList<int>> removedByGroup;
            for (const int id : removed) {
                const int gid = groupOf.value(id);
                if (!removedByGroup.contains(gid)) affectedGroups.append(gid);
                removedByGroup[gid].append(id);
            }
            for (const int gid : affectedGroups) EngineEvents::SubscriptionGroupChanged(gid, removedByGroup.value(gid));
            EngineEvents::ProfilesRemoved(removed);
        }

        for (const int id : requested) {
            if (survivors.contains(id)) return ServiceError{500, QStringLiteral("internal"), QStringLiteral("Could not delete profile %1").arg(id)};
        }
        return std::nullopt;
    }

    int RemoveUnavailable(QList<int> profileIds) {
        ENGINE_CHECK_MAIN_THREAD();
        const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
        if (!group) return 0;

        if (profileIds.isEmpty()) profileIds = group->Profiles();

        QList<int> del_ids;
        for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileBatch(profileIds)) {
            if (profile->IsUnavailable()) del_ids += profile->id;
        }
        if (del_ids.isEmpty()) return 0;

        // Strips started_id from del_ids, so the running profile survives.
        if (!Configs::dataManager->profilesRepo->BatchDeleteProfiles(del_ids)) return 0;
        if (del_ids.isEmpty()) return 0;
        EngineEvents::ProfilesRemoved(del_ids);
        return static_cast<int>(del_ids.size());
    }

    int ClearTestResults(const QList<int> &profileIds, QList<int> *groupIds) {
        ENGINE_CHECK_MAIN_THREAD();
        if (groupIds != nullptr) groupIds->clear();
        const auto ents = Configs::dataManager->profilesRepo->GetProfileBatch(profileIds);
        if (ents.empty()) return 0;
        QSet<int> gids;
        for (const auto &ent: ents) {
            ent->ClearTestResults();
            gids.insert(ent->gid);
        }
        Configs::dataManager->profilesRepo->SaveBatch(ents);
        if (groupIds != nullptr) *groupIds = gids.values();
        return static_cast<int>(ents.size());
    }
} // namespace Engine::ProfileService
