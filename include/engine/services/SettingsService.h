#pragma once

#include <functional>
#include <optional>

#include <QStringList>

#include "include/database/SettingsValues.h"
#include "include/engine/services/ServiceTypes.h"

// The only path that commits settings with effects: windows, menus and the HTTP API all end here. Bookkeeping values without
// effects (geometry, sort orders, histories, remember_*) may still be written in place and saved with SettingsRepo::Save().
namespace Engine::SettingsService {
    struct ApplyOptions {
        // From a window or a menu: a needed program or Tun restart is asked for; otherwise it is only logged.
        bool interactive = false;
    };

    struct ApplyResult {
        // Database keys whose value changed, in schema order.
        QStringList changed;
        // SettingsSchema::Effect bits of changed.
        quint32 effects = 0;
        // A changed key takes effect only when the running profile, or the running Tun, restarts.
        bool restartNeeded = false;
    };

    // Main thread. Copies keys from proposed over the current values, checks the result (port clashes), saves what changed, runs
    // its engine effects and emits settingsChanged. Turning random_inbound_port on picks the port before the save, as before.
    // Nothing is written when it returns an error.
    std::optional<ServiceError> Apply(const Configs::SettingsValues &proposed, const QStringList &keys, const ApplyOptions &options = {},
                                      ApplyResult *result = nullptr);

    // A settings window opened on snapshot and the user edited it into proposed: only the keys that differ are applied, so values
    // changed elsewhere while it was open survive.
    std::optional<ServiceError> ApplyEdit(const Configs::SettingsValues &snapshot, const Configs::SettingsValues &proposed,
                                          const ApplyOptions &options = {}, ApplyResult *result = nullptr);

    // One-off writes such as menu toggles: edit changes a copy of the current values.
    std::optional<ServiceError> Update(const std::function<void(Configs::SettingsValues &)> &edit, const ApplyOptions &options = {},
                                       ApplyResult *result = nullptr);
} // namespace Engine::SettingsService
