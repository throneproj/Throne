#pragma once

#include <optional>

#include <QList>

#include "include/engine/services/ServiceTypes.h"

namespace Engine::ProfileService {
    // Main thread. Why Delete would refuse now (an editor holds one of them).
    std::optional<ServiceError> CheckDelete(const QList<int> &profileIds);

    // Main thread, with none of them running. Deletes them, resets the front/landing proxies that pointed at them, and
    // emits SubscriptionGroupChanged per affected group (auto-selector pruning) and ProfilesRemoved.
    std::optional<ServiceError> Delete(const QList<int> &profileIds, int *deleted = nullptr);

    // Main thread. Deletes the unavailable ones among profileIds (the current group's when empty), never the running
    // profile, and emits ProfilesRemoved for them; no SubscriptionGroupChanged. Returns how many it deleted.
    int RemoveUnavailable(QList<int> profileIds);

    // Main thread. Clears the test results of the profiles; *groupIds gets their groups. Returns how many were cleared.
    int ClearTestResults(const QList<int> &profileIds, QList<int> *groupIds = nullptr);
} // namespace Engine::ProfileService
