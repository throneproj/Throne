#include "include/engine/services/ProfileService.h"

#include <QHash>
#include <QSet>

#include <utility>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/Protocols.h"
#include "include/configs/sub/GroupUpdater.hpp"
#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EditLocks.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/EngineHost.h"
#include "include/engine/Lifecycle.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/services/ImportService.h"
#include "include/global/RunningProfiles.hpp"
#include "include/global/Utils.hpp"
#include "include/global/Validation.hpp"

namespace Engine::ProfileService {
    namespace {
        // The base outbound answers with a bare query string for the types that have no link format.
        QString profileServiceLink(Configs::outbound &outbound) {
            const QString link = outbound.ExportToLink();
            return link.contains(QStringLiteral("://")) ? link : QString();
        }

        QString profileServiceIssueLine(const Validation::Issue &issue) {
            return issue.field.isEmpty() ? issue.message : QStringLiteral("%1: %2").arg(issue.field, issue.message);
        }

        ServiceError profileServiceInvalid(const QString &summary, const QStringList &lines) {
            QString message = summary;
            if (!lines.isEmpty()) message += QLatin1Char('\n') + lines.join(QLatin1Char('\n'));
            return {422, QStringLiteral("invalid_profile"), message};
        }

        // Empty when the API may edit it. StripForeignState changes what it runs on, so it runs on a round-tripped clone.
        QString profileServiceForeignReason(const std::shared_ptr<Configs::Profile> &profile) {
            if (Configs::Protocols::Is(profile->type, Configs::Protocols::RunsProgram)) {
                return QStringLiteral("Extra Core profiles run programs, so only Throne's window can edit them");
            }
            const auto clone = Configs::ProfilesRepo::NewProfile(profile->type);
            clone->outbound->ParseFromJson(profile->outbound->ExportToJson());
            Validation::Issues issues;
            clone->outbound->StripForeignState(issues);
            if (issues.isEmpty()) return {};
            const QString field = issues.first().field;
            if (field.isEmpty()) return QStringLiteral("It sets something only Throne's window can set");
            return QStringLiteral("It sets %1, which only Throne's window can set").arg(field);
        }

        // The hop rules EditChain enforces; the build checks how the hops combine.
        void profileServiceCheckHops(int profileId, const Configs::chain &chain, Validation::Issues &issues) {
            bool selectorSeen = false;
            for (qsizetype i = 0; i < chain.list.size(); ++i) {
                const int hopId = chain.list[i];
                const auto hop = Configs::dataManager->profilesRepo->GetProfile(hopId);
                QString problem;
                if (hop == nullptr || hop->outbound == nullptr) {
                    problem = QStringLiteral("does not exist");
                } else if (hopId == profileId || hop->type == QLatin1String("chain")) {
                    problem = QStringLiteral("is a chain, which cannot be a hop");
                } else if (hop->type == QLatin1String("autoselector")) {
                    if (selectorSeen) problem = QStringLiteral("is a second auto selector; a chain can hold only one");
                    selectorSeen = true;
                }
                if (problem.isEmpty()) continue;
                issues << Validation::Issue{Validation::Severity::Error, QStringLiteral("list"),
                                            QStringLiteral("Hop %1 (profile %2) %3").arg(QString::number(i + 1), QString::number(hopId), problem), {}};
            }
        }

        std::optional<ServiceError> profileServiceEditing(const QList<int> &profileIds) {
            if (!IsBeingEdited(EditTarget::Profile)) return std::nullopt;
            for (const int id : profileIds) {
                if (!IsBeingEdited(EditTarget::Profile, id)) continue;
                return ServiceError{409, QStringLiteral("editing"), QStringLiteral("Profile %1 is open in an editor in Throne's window").arg(id)};
            }
            return std::nullopt;
        }
    } // namespace

    std::optional<ServiceError> CheckDelete(const QList<int> &profileIds) {
        ENGINE_CHECK_MAIN_THREAD();
        return profileServiceEditing(profileIds);
    }

    std::optional<ServiceError> Delete(const QList<int> &profileIds, int *deleted, const DeleteOptions &options) {
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

        if (options.stopRunning) {
            if (requested.contains(Lifecycle::ResumeTargetId())) Lifecycle::StopRunningProfile(false, true, false, options.interactive);
        } else if (auto *engine = host(); engine != nullptr) {
            if (const auto running = engine->runningSnapshot(); running != nullptr && groupOf.contains(running->id)) {
                return ServiceError{409, QStringLiteral("busy"), QStringLiteral("One of the profiles is running")};
            }
        }

        bool usedByRunning = false;
        for (const int id : std::as_const(requested)) {
            if (id == Lifecycle::ResumeTargetId() || Configs::RunningUsesProfile(id)) usedByRunning = true;
        }
        // None of them runs now, so a resume target among them was left by a crash or a non-manual stop; BatchDeleteProfiles would skip it.
        if (const int target = Lifecycle::ResumeTargetId(); requested.contains(target)) Lifecycle::ForgetResumeTarget(target);

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
            QList<int> affectedGroups;
            QHash<int, QList<int>> removedByGroup;
            for (const int id : removed) {
                const int gid = groupOf.value(id);
                if (!removedByGroup.contains(gid)) affectedGroups.append(gid);
                removedByGroup[gid].append(id);
            }
            if (auto *engine = host(); engine != nullptr && usedByRunning) engine->noteRestartNeeded(RestartReason::Profile);
            for (const int gid : affectedGroups) EngineEvents::SubscriptionGroupChanged(gid, removedByGroup.value(gid));
            EngineEvents::ProfilesRemoved(removed);
        }

        for (const int id : requested) {
            if (survivors.contains(id)) return ServiceError{500, QStringLiteral("internal"), QStringLiteral("Could not delete profile %1").arg(id)};
        }
        return std::nullopt;
    }

    std::optional<ServiceError> Add(const std::shared_ptr<Configs::Profile> &profile, int groupId) {
        ENGINE_CHECK_MAIN_THREAD();
        if (Configs::dataManager->groupsRepo->GetGroup(groupId) == nullptr) {
            return ServiceError{404, QStringLiteral("not_found"), QStringLiteral("Unknown group %1").arg(groupId)};
        }
        auto added = profile;
        if (!Configs::dataManager->profilesRepo->AddProfile(added, groupId)) {
            return ServiceError{500, QStringLiteral("internal"), QStringLiteral("Could not save the profile")};
        }
        EngineEvents::ProfilesChanged({added->id});
        return std::nullopt;
    }

    std::optional<ServiceError> Save(const QList<std::shared_ptr<Configs::Profile>> &profiles) {
        ENGINE_CHECK_MAIN_THREAD();
        const int startedId = Lifecycle::ResumeTargetId();
        QList<std::shared_ptr<Configs::Profile>> saved;
        QList<int> savedIds;
        std::optional<ServiceError> missing;
        bool usedByRunning = false;
        for (const auto &profile : profiles) {
            if (profile == nullptr) continue;
            if (!Configs::dataManager->profilesRepo->Save(profile)) {
                if (!missing) missing = ServiceError{404, QStringLiteral("not_found"), QStringLiteral("Profile %1 was deleted meanwhile").arg(profile->id)};
                continue;
            }
            saved.append(profile);
            savedIds.append(profile->id);
            if (profile->id == startedId || Configs::RunningUsesProfile(profile->id)) usedByRunning = true;
        }
        if (saved.isEmpty()) return missing;

        Configs::RepositionLater(saved, Configs::SortInput::Config);
        if (auto *engine = host(); engine != nullptr && usedByRunning) engine->noteRestartNeeded(RestartReason::Profile);
        EngineEvents::ProfilesChanged(savedIds);
        return missing;
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

        // Strips the resume target from del_ids, so the running profile survives.
        if (!Configs::dataManager->profilesRepo->BatchDeleteProfiles(del_ids)) return 0;
        if (del_ids.isEmpty()) return 0;
        EngineEvents::ProfilesRemoved(del_ids);
        return static_cast<int>(del_ids.size());
    }

    std::optional<ServiceError> MoveToGroup(const QList<int> &profileIds, int targetGid, int *moved) {
        ENGINE_CHECK_MAIN_THREAD();
        if (moved != nullptr) *moved = 0;
        if (auto refusal = ImportService::CheckTarget(targetGid)) return refusal;
        if (auto refusal = profileServiceEditing(profileIds)) return refusal;

        auto *profiles = Configs::dataManager->profilesRepo.get();
        QList<std::shared_ptr<Configs::Profile>> moving;
        QList<int> movingIds;
        QSet<int> seen;
        QList<int> sources;
        QHash<int, QList<int>> leaving;
        for (const auto &profile : profiles->GetProfileBatch(profileIds)) {
            if (profile->gid == targetGid || seen.contains(profile->id)) continue;
            seen.insert(profile->id);
            moving.append(profile);
            movingIds.append(profile->id);
            if (!leaving.contains(profile->gid)) sources.append(profile->gid);
            leaving[profile->gid].append(profile->id);
        }
        if (moving.isEmpty()) return std::nullopt;
        // A refresh rewrites its group's listing from a snapshot, which would bring a moved profile back or delete it.
        for (const int gid : std::as_const(sources)) {
            if (Subscription::updater()->IsBusy(gid)) return ServiceError{409, QStringLiteral("busy"), QStringLiteral("The group's subscription is updating")};
        }

        // Target listing, then source listings, then gid: a crash in between leaves a profile listed twice, never unlisted.
        auto *groups = Configs::dataManager->groupsRepo.get();
        const auto target = groups->GetGroup(targetGid);
        target->AddProfileBatch(movingIds);
        if (!groups->Save(target)) {
            return ServiceError{404, QStringLiteral("not_found"), QStringLiteral("Group %1 was deleted meanwhile").arg(targetGid)};
        }
        for (const int gid : std::as_const(sources)) {
            const auto source = groups->GetGroup(gid);
            if (source == nullptr) continue;
            source->RemoveProfileBatch(leaving.value(gid));
            groups->Save(source);
        }

        const int startedId = Lifecycle::ResumeTargetId();
        QList<int> movedIds;
        bool usedByRunning = false;
        for (const auto &profile : std::as_const(moving)) {
            profile->gid = targetGid;
            if (!profiles->Save(profile)) continue;
            movedIds.append(profile->id);
            if (profile->id == startedId || Configs::RunningUsesProfile(profile->id)) usedByRunning = true;
        }
        if (moved != nullptr) *moved = static_cast<int>(movedIds.size());

        Configs::ResortLater(targetGid);
        // The running config was built with the old group's proxies, routing and endpoint.
        if (auto *engine = host(); engine != nullptr && usedByRunning) engine->noteRestartNeeded(RestartReason::Group);
        for (const int gid : std::as_const(sources)) EngineEvents::SubscriptionGroupChanged(gid, leaving.value(gid));
        EngineEvents::ProfilesChanged(movedIds);
        EngineEvents::GroupsChanged();
        return std::nullopt;
    }

    int ClearTestResults(const QList<int> &profileIds, QList<int> *groupIds, std::optional<TestKind> kind,
                         const QString &customTestId) {
        ENGINE_CHECK_MAIN_THREAD();
        if (groupIds != nullptr) groupIds->clear();
        const auto ents = Configs::dataManager->profilesRepo->GetProfileBatch(profileIds);
        if (ents.empty()) return 0;
        QSet<int> gids;
        for (const auto &ent: ents) {
            if (!kind) {
                ent->ClearTestResults();
            } else {
                switch (*kind) {
                    case TestKind::Latency: ent->SetLatency(0); break;
                    case TestKind::Ip:
                        ent->ip_out.clear();
                        ent->test_country.clear();
                        break;
                    case TestKind::Speed:
                        ent->dl_speed.clear();
                        ent->ul_speed.clear();
                        break;
                    case TestKind::Service: ent->service_checks.clear(); break;
                    case TestKind::Custom:
                        if (customTestId.isEmpty()) ent->custom_test_results.clear();
                        else ent->custom_test_results.remove(customTestId);
                        break;
                }
            }
            gids.insert(ent->gid);
        }
        Configs::dataManager->profilesRepo->SaveBatch(ents);
        if (groupIds != nullptr) *groupIds = gids.values();
        return static_cast<int>(ents.size());
    }

    std::optional<ProfileConfig> ReadConfig(int profileId) {
        ENGINE_CHECK_MAIN_THREAD();
        const auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId);
        if (profile == nullptr || profile->outbound == nullptr) return std::nullopt;
        ProfileConfig config;
        config.type = profile->type;
        config.name = profile->outbound->name;
        config.outbound = profile->outbound->ExportToJson();
        config.link = profileServiceLink(*profile->outbound);
        config.notEditable = profileServiceForeignReason(profile);
        return config;
    }

    std::optional<ServiceError> CheckConfigEdit(int profileId) {
        ENGINE_CHECK_MAIN_THREAD();
        const auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId);
        if (profile == nullptr || profile->outbound == nullptr) {
            return ServiceError{404, QStringLiteral("not_found"), QStringLiteral("Unknown profile %1").arg(profileId)};
        }
        if (const auto reason = profileServiceForeignReason(profile); !reason.isEmpty()) {
            return ServiceError{403, QStringLiteral("not_editable"), reason};
        }
        if (IsBeingEdited(EditTarget::Profile, profileId)) {
            return ServiceError{409, QStringLiteral("editing"),
                                QStringLiteral("Profile %1 is open in an editor in Throne's window; close it and try again").arg(profileId)};
        }
        return std::nullopt;
    }

    std::optional<ServiceError> ApplyConfig(int profileId, const ConfigInput &input, ConfigApplied *applied) {
        ENGINE_CHECK_MAIN_THREAD();
        if (auto refusal = CheckConfigEdit(profileId)) return refusal;
        const auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId);
        const QString type = profile->type;
        if (input.link && profileServiceLink(*profile->outbound).isEmpty()) {
            return profileServiceInvalid(QStringLiteral("%1 profiles have no share link; send the outbound instead").arg(type), {});
        }

        // The live profile is shared with the window and the workers, so the input goes into a fresh outbound first.
        const auto parsedProfile = Configs::ProfilesRepo::NewProfile(type);
        const auto outbound = parsedProfile->outbound;
        if (outbound == nullptr || outbound->invalid) {
            return ServiceError{500, QStringLiteral("internal"), QStringLiteral("Throne cannot parse %1 profiles").arg(type)};
        }
        Validation::Issues issues;
        bool parsed = false;
        {
            Configs::ParseDiag::Scope scope;
            parsed = input.link ? outbound->ParseFromLink(input.link->trimmed()) : outbound->ParseFromJson(input.outbound.value_or(QJsonObject()));
            issues << scope.take();
        }
        QStringList errors;
        for (const auto &issue : std::as_const(issues)) {
            if (issue.severity == Validation::Severity::Error) errors << profileServiceIssueLine(issue);
        }
        if (!parsed) {
            return profileServiceInvalid(input.link ? QStringLiteral("The link is not a %1 link").arg(type)
                                                    : QStringLiteral("The outbound is not a valid %1 outbound").arg(type), errors);
        }
        if (!errors.isEmpty()) return profileServiceInvalid(QStringLiteral("The profile could not be parsed"), errors);
        // A link seldom carries the name the profile has here.
        if (input.link && outbound->name.isEmpty()) outbound->name = profile->outbound->name;

        Validation::Issues foreign;
        outbound->StripForeignState(foreign);
        if (!foreign.isEmpty()) {
            QStringList fields;
            for (const auto &issue : std::as_const(foreign)) {
                const auto line = QStringLiteral("%1: only Throne's window can set it").arg(issue.field.isEmpty() ? type : issue.field);
                if (!fields.contains(line)) fields << line;
            }
            return profileServiceInvalid(QStringLiteral("The profile sets what only Throne's window can set"), fields);
        }

        Validation::Issues model;
        outbound->Validate(model);
        if (const auto *hops = parsedProfile->Chain()) profileServiceCheckHops(profileId, *hops, model);
        issues << model;
        QStringList warnings;
        for (const auto &issue : std::as_const(issues)) {
            (issue.severity == Validation::Severity::Error ? errors : warnings) << profileServiceIssueLine(issue);
        }
        if (!errors.isEmpty()) return profileServiceInvalid(QStringLiteral("The profile is not valid"), errors);

        outbound->profile_id = profile->id;
        profile->outbound = outbound;
        profile->name = outbound->name;
        if (auto refusal = Save({profile})) return refusal;
        if (applied != nullptr) applied->warnings = warnings;
        return std::nullopt;
    }
} // namespace Engine::ProfileService
