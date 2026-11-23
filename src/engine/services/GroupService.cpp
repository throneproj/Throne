#include "include/engine/services/GroupService.h"

#include <algorithm>
#include <memory>

#include <QMetaObject>
#include <QSet>

#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/engine/EditLocks.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/EngineHost.h"
#include "include/engine/Lifecycle.h"
#include "include/engine/ThreadCheck.h"
#include "include/global/RunningProfiles.hpp"
#include "include/global/Utils.hpp"
#include "include/global/Validation.hpp"

namespace Engine::GroupService {
    namespace {
        // Queued even from the main thread: the updater answers a request it refuses before returning.
        void groupServiceDeliver(const std::function<void()> &fn) {
            QMetaObject::invokeMethod(EngineEvents::instance(), fn, Qt::QueuedConnection);
        }

        ServiceError groupServiceUnknown(int groupId) {
            return {404, QStringLiteral("not_found"), QStringLiteral("Unknown group %1").arg(groupId)};
        }

        // Tab-ordered groups that still exist; ghost groups (no tab) are left out.
        QList<int> groupServiceTabGroups() {
            QList<int> ids;
            QSet<int> seen;
            for (const int gid : Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
                if (seen.contains(gid)) continue;
                seen.insert(gid);
                if (Configs::dataManager->groupsRepo->GetGroup(gid) != nullptr) ids.append(gid);
            }
            return ids;
        }
    } // namespace

    std::optional<ServiceError> Add(const NewGroup &spec, int *groupId) {
        ENGINE_CHECK_MAIN_THREAD();
        const QString name = spec.name.trimmed();
        if (name.isEmpty()) {
            return ServiceError{400, QStringLiteral("bad_request"), QStringLiteral("The group needs a name")};
        }
        const QString url = spec.url.trimmed();
        if (!url.isEmpty() &&
            !Validation::Url(url, {QStringLiteral("http"), QStringLiteral("https")}, Validation::Empty::Required).isEmpty()) {
            return ServiceError{400, QStringLiteral("bad_request"), QStringLiteral("The subscription URL must be an http or https URL")};
        }
        if (spec.routeId >= 0 && !Configs::dataManager->routesRepo->HasRouteProfile(spec.routeId)) {
            return ServiceError{404, QStringLiteral("not_found"), QStringLiteral("Unknown routing profile %1").arg(spec.routeId)};
        }
        if (spec.importRoute && url.isEmpty()) {
            return ServiceError{400, QStringLiteral("bad_request"), QStringLiteral("Only a subscription group can import its routing profile")};
        }

        auto group = Configs::GroupsRepo::NewGroup();
        group->name = name;
        group->url = url;
        group->skip_auto_update = !spec.autoUpdate;
        group->route_id = spec.routeId < 0 ? -1 : spec.routeId;
        group->import_sub_route = spec.importRoute;
        if (auto refusal = Add(group)) return refusal;
        if (groupId != nullptr) *groupId = group->id;
        return std::nullopt;
    }

    std::optional<ServiceError> Add(const std::shared_ptr<Configs::Group> &group) {
        ENGINE_CHECK_MAIN_THREAD();
        auto added = group;
        if (!Configs::dataManager->groupsRepo->AddGroup(added)) {
            return ServiceError{500, QStringLiteral("internal"), QStringLiteral("Could not save the group")};
        }
        EngineEvents::GroupsChanged();
        return std::nullopt;
    }

    std::optional<ServiceError> Save(const std::shared_ptr<Configs::Group> &group, bool restartNeeded) {
        ENGINE_CHECK_MAIN_THREAD();
        if (!Configs::dataManager->groupsRepo->Save(group)) {
            return ServiceError{404, QStringLiteral("not_found"), QStringLiteral("Group %1 was deleted meanwhile").arg(group->id)};
        }
        if (auto *engine = host(); engine != nullptr && restartNeeded) engine->noteRestartNeeded(RestartReason::Group);
        EngineEvents::GroupsChanged();
        return std::nullopt;
    }

    std::optional<ServiceError> Update(int groupId, const UpdateDone &done) {
        ENGINE_CHECK_MAIN_THREAD();
        const auto group = Configs::dataManager->groupsRepo->GetGroup(groupId);
        if (group == nullptr) return groupServiceUnknown(groupId);
        if (group->url.isEmpty()) {
            return ServiceError{409, QStringLiteral("not_subscription"), QStringLiteral("The group has no subscription")};
        }
        if (group->archive) {
            return ServiceError{409, QStringLiteral("group_archived"), QStringLiteral("The group is archived")};
        }

        Subscription::updater()->RefreshWithOutcome(groupId, Subscription::GroupUpdater::Surface::Log,
                                                    [done](const Subscription::UpdateOutcome &outcome) {
                                                        if (!done) return;
                                                        groupServiceDeliver([done, outcome] { done(outcome); });
                                                    });
        return std::nullopt;
    }

    QList<int> UpdateAll(const UpdateAllDone &done) {
        ENGINE_CHECK_MAIN_THREAD();
        QList<int> queued;
        for (const int gid : groupServiceTabGroups()) {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
            if (group == nullptr || group->url.isEmpty() || group->archive) continue;
            queued.append(gid);
        }
        if (queued.isEmpty()) {
            if (done) groupServiceDeliver([done] { done({}); });
            return queued;
        }

        struct Collect {
            QList<Subscription::UpdateOutcome> outcomes;
            qsizetype remaining = 0;
        };
        auto collect = std::make_shared<Collect>();
        collect->outcomes.resize(queued.size());
        collect->remaining = queued.size();
        for (qsizetype i = 0; i < queued.size(); ++i) {
            Subscription::updater()->RefreshWithOutcome(queued[i], Subscription::GroupUpdater::Surface::Log,
                                                        [collect, i, done](const Subscription::UpdateOutcome &outcome) {
                                                            // Counted on the main thread, so the last arrival alone calls done.
                                                            groupServiceDeliver([collect, i, done, outcome] {
                                                                collect->outcomes[i] = outcome;
                                                                if (--collect->remaining > 0) return;
                                                                if (done) done(collect->outcomes);
                                                            });
                                                        });
        }
        return queued;
    }

    std::optional<ServiceError> CheckDelete(int groupId) {
        ENGINE_CHECK_MAIN_THREAD();
        const auto group = Configs::dataManager->groupsRepo->GetGroup(groupId);
        if (group == nullptr) return groupServiceUnknown(groupId);
        if (const auto tabGroups = groupServiceTabGroups(); tabGroups.size() == 1 && tabGroups.first() == groupId) {
            return ServiceError{409, QStringLiteral("last_group"), QStringLiteral("Throne needs at least one group")};
        }
        if (Subscription::updater()->IsBusy(groupId)) {
            return ServiceError{409, QStringLiteral("busy"), QStringLiteral("The group's subscription is updating")};
        }
        bool editing = IsBeingEdited(EditTarget::Group, groupId);
        if (!editing && IsBeingEdited(EditTarget::Profile)) {
            for (const int id : group->Profiles()) {
                if (!IsBeingEdited(EditTarget::Profile, id)) continue;
                editing = true;
                break;
            }
        }
        if (editing) {
            return ServiceError{409, QStringLiteral("editing"),
                                QStringLiteral("The group or one of its profiles is open in an editor in Throne's window")};
        }
        // Delete takes the routing profiles its subscription provided along.
        for (const int routeId : Configs::dataManager->routesRepo->OwnedBy(groupId)) {
            if (!IsBeingEdited(EditTarget::Routes, routeId)) continue;
            return ServiceError{409, QStringLiteral("routes_editing"),
                                QStringLiteral("The group's routing profile is open for editing in Throne's window; close it and try again")};
        }
        return std::nullopt;
    }

    std::optional<ServiceError> Delete(int groupId) {
        ENGINE_CHECK_MAIN_THREAD();
        if (auto refusal = CheckDelete(groupId)) return refusal;
        const auto group = Configs::dataManager->groupsRepo->GetGroup(groupId);
        const auto profileIds = group->Profiles();
        const QSet<int> deletedProfiles(profileIds.cbegin(), profileIds.cend());
        const bool usedByRunning = std::any_of(profileIds.cbegin(), profileIds.cend(), [](int id) { return Configs::RunningUsesProfile(id); });
        if (!Configs::dataManager->groupsRepo->DeleteGroup(groupId)) {
            return ServiceError{500, QStringLiteral("internal"), QStringLiteral("Could not delete the group")};
        }
        // The routing profiles its subscription imported go with it; the repo moves the global choice off them.
        if (const auto ownedRoutes = Configs::dataManager->routesRepo->OwnedBy(groupId); !ownedRoutes.isEmpty()) {
            Configs::dataManager->routesRepo->DeleteRouteProfiles(ownedRoutes);
            MW_show_log(QStringLiteral("[Groups] %1: deleted the routing profile(s) its subscription provided").arg(group->name));
        }

        auto *settings = Configs::dataManager->settingsRepo.get();
        // The caller stopped a running profile of the group, but a non-manual stop leaves it as the resume target.
        if (const int target = Lifecycle::ResumeTargetId(); deletedProfiles.contains(target)) Lifecycle::ForgetResumeTarget(target);
        if (settings->current_group == groupId) {
            const auto tabGroups = groupServiceTabGroups();
            settings->current_group = tabGroups.isEmpty() ? -1 : tabGroups.first();
            settings->Save();
        }

        if (auto *engine = host(); engine != nullptr && usedByRunning) engine->noteRestartNeeded(RestartReason::Group);
        EngineEvents::GroupsChanged();
        return std::nullopt;
    }
} // namespace Engine::GroupService
