#include "include/engine/services/ImportService.h"

#include <QMetaObject>

#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/ThreadCheck.h"
#include "include/global/Utils.hpp"
#include "include/global/Validation.hpp"

namespace Engine::ImportService {
    namespace {
        // Queued even from the main thread: the updater answers a request it refuses before returning.
        void importServiceDeliver(const Done &done, const Subscription::UpdateOutcome &outcome) {
            if (!done) return;
            QMetaObject::invokeMethod(EngineEvents::instance(), [done, outcome] { done(outcome); }, Qt::QueuedConnection);
        }
    } // namespace

    std::optional<ServiceError> Import(const Request &request, const Done &done, int *groupId) {
        ENGINE_CHECK_MAIN_THREAD();
        const QString payload = request.payload.trimmed();
        if (payload.isEmpty()) {
            return ServiceError{400, QStringLiteral("bad_request"), QStringLiteral("Nothing to import")};
        }
        if (request.source == Source::Url &&
            !Validation::Url(payload, {QStringLiteral("http"), QStringLiteral("https")}, Validation::Empty::Required).isEmpty()) {
            return ServiceError{400, QStringLiteral("bad_request"), QStringLiteral("Only http and https URLs can be imported")};
        }

        const int gid = request.groupId >= 0 ? request.groupId : Configs::dataManager->settingsRepo->current_group;
        if (groupId != nullptr) *groupId = gid;
        const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (group == nullptr) {
            return ServiceError{404, QStringLiteral("not_found"), QStringLiteral("Unknown group %1").arg(gid)};
        }
        if (group->archive) {
            return ServiceError{409, QStringLiteral("group_archived"), QStringLiteral("The group is archived")};
        }
        if (!group->url.isEmpty()) {
            return ServiceError{409, QStringLiteral("subscription_group"),
                                QStringLiteral("Profiles imported into a subscription group are removed by its next update")};
        }

        const auto finish = [done, gid](const Subscription::UpdateOutcome &outcome) {
            // The updater logs an import with problems itself.
            if (outcome.ok() && !outcome.report.hasProblems()) {
                MW_show_log(QStringLiteral("[Import] %1 profile(s) into group %2").arg(outcome.addedIds.size()).arg(gid));
            }
            importServiceDeliver(done, outcome);
        };
        if (request.source == Source::Url) {
            Subscription::updater()->ImportUrlTo(gid, payload, finish);
        } else {
            Subscription::updater()->ImportTextTo(gid, payload, finish);
        }
        return std::nullopt;
    }
} // namespace Engine::ImportService
