#pragma once

#include <memory>
#include <optional>

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include "include/engine/services/ServiceTypes.h"

namespace Configs {
    class Profile;
}

namespace Engine::ProfileService {
    // Main thread. Why Delete would refuse now (an editor holds one of them).
    std::optional<ServiceError> CheckDelete(const QList<int> &profileIds);

    struct DeleteOptions {
        // The window's delete: a running profile among them is stopped first, as it always was there.
        bool stopRunning = false;
        bool interactive = true;
    };

    // Main thread. A running profile among them is stopped first with options.stopRunning, and refused ("busy") without it.
    // Deletes them, resets the front/landing proxies that pointed at them, notes a Profile restart when the running config
    // used one, and emits SubscriptionGroupChanged per affected group (auto-selector pruning) and ProfilesRemoved.
    std::optional<ServiceError> Delete(const QList<int> &profileIds, int *deleted = nullptr, const DeleteOptions &options = {});

    // Main thread. Adds a profile the window built (the editor, a tuned copy) at the end of groupId, which must exist; emits
    // ProfilesChanged. Subscription targets are not refused here: only the import paths refuse them.
    std::optional<ServiceError> Add(const std::shared_ptr<Configs::Profile> &profile, int groupId);

    // Main thread. Commits edits made on the main thread to live profiles (the window's editor holds their lock): saves them,
    // repositions them in kept sorts, notes a Profile restart when the running config uses one, and emits one ProfilesChanged.
    // not_found names the first that was deleted meanwhile; the others are still saved.
    std::optional<ServiceError> Save(const QList<std::shared_ptr<Configs::Profile>> &profiles);

    // Main thread. Deletes the unavailable ones among profileIds (the current group's when empty), never the running
    // profile, and emits ProfilesRemoved for them; no SubscriptionGroupChanged. Returns how many it deleted.
    int RemoveUnavailable(QList<int> profileIds);

    // Main thread. Refuses a target ImportService::CheckTarget refuses, profiles open in an editor, or a source group whose
    // subscription is updating ("busy"). Notes a Group restart if the running config uses a moved profile; emits
    // SubscriptionGroupChanged per source group, ProfilesChanged and GroupsChanged.
    std::optional<ServiceError> MoveToGroup(const QList<int> &profileIds, int targetGid, int *moved = nullptr);

    // Main thread. Clears the test results of the profiles; *groupIds gets their groups. Returns how many were cleared.
    int ClearTestResults(const QList<int> &profileIds, QList<int> *groupIds = nullptr);

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
    };

    // Main thread, with no start, stop or test in flight. Parses the input as a new outbound of the profile's own type and
    // checks it like an import; only then swaps it into the live profile and commits it as Save does.
    std::optional<ServiceError> ApplyConfig(int profileId, const ConfigInput &input, ConfigApplied *applied);
} // namespace Engine::ProfileService
