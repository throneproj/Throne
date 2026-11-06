#pragma once

#include <optional>

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include "include/engine/services/ServiceTypes.h"
#include "include/engine/EngineTypes.h"

namespace Engine::ProfileService {
    // Main thread. Why Delete would refuse now (an editor holds one of them).
    std::optional<ServiceError> CheckDelete(const QList<int> &profileIds);

    // Main thread, with none of them running. Deletes them, resets the front/landing proxies that pointed at them, and
    // emits SubscriptionGroupChanged per affected group (auto-selector pruning) and ProfilesRemoved.
    std::optional<ServiceError> Delete(const QList<int> &profileIds, int *deleted = nullptr);

    // Main thread. Deletes the unavailable ones among profileIds (the current group's when empty), never the running
    // profile, and emits ProfilesRemoved for them; no SubscriptionGroupChanged. Returns how many it deleted.
    int RemoveUnavailable(QList<int> profileIds);

    // Main thread. Clears all results, or only kind when supplied; *groupIds gets the affected groups.
    int ClearTestResults(const QList<int> &profileIds, QList<int> *groupIds = nullptr,
                         std::optional<TestKind> kind = std::nullopt);

    struct ProfileConfig {
        QString type;
        QString name;
        // Exactly what throne.db keeps.
        QJsonObject outbound;
        // Empty for a type without a share link.
        QString link;
        // Why only the window may edit it; empty when the API may.
        QString notEditable;
    };

    // Main thread. nullopt for an unknown profile.
    std::optional<ProfileConfig> ReadConfig(int profileId);

    // Main thread. Why ApplyConfig would refuse the profile itself: unknown, editable only in the window, or open in its editor.
    std::optional<ServiceError> CheckConfigEdit(int profileId);

    // Exactly one is set.
    struct ConfigInput {
        std::optional<QJsonObject> outbound;
        std::optional<QString> link;
    };

    struct ConfigApplied {
        // "field: message" lines of the issues that do not block.
        QStringList warnings;
        // The running config was built from the profile.
        bool usedByRunning = false;
    };

    // Main thread, with no start, stop or test in flight. Parses the input as a new outbound of the profile's own type and
    // checks it like an import; only then swaps it into the live profile, saves, and emits ProfilesChanged.
    std::optional<ServiceError> ApplyConfig(int profileId, const ConfigInput &input, ConfigApplied *applied);
} // namespace Engine::ProfileService
