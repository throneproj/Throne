#pragma once

#include <functional>
#include <optional>

#include <QList>
#include <QString>

#include "include/configs/sub/GroupUpdater.hpp"
#include "include/engine/services/ServiceTypes.h"

namespace Engine::GroupService {
    struct NewGroup {
        QString name;
        // Empty: a basic group. Otherwise a subscription group; http and https only.
        QString url;
        bool autoUpdate = true;
    };

    // Main thread. Adds the group at the end of the tab order and emits GroupsChanged.
    std::optional<ServiceError> Add(const NewGroup &spec, int *groupId);

    using UpdateDone = std::function<void(const Subscription::UpdateOutcome &outcome)>;

    // Main thread. Refreshes a subscription group quietly (log only); done runs on the main thread. An update of a group
    // already queued or running shares that job's outcome.
    std::optional<ServiceError> Update(int groupId, const UpdateDone &done);

    using UpdateAllDone = std::function<void(const QList<Subscription::UpdateOutcome> &outcomes)>;

    // Main thread. Refreshes every subscription group that is not archived, in tab order; done runs on the main thread
    // once all finished, with the outcomes in that order. Returns the queued group ids (done still runs when empty).
    QList<int> UpdateAll(const UpdateAllDone &done);

    // Main thread. Why Delete would refuse now. The caller stops a running profile of the group before calling Delete.
    std::optional<ServiceError> CheckDelete(int groupId);

    // Main thread, with no profile of the group running. Deletes the group and its profiles, repairs current_group and
    // the front/landing proxies that pointed into it, and emits GroupsChanged.
    std::optional<ServiceError> Delete(int groupId);
} // namespace Engine::GroupService
