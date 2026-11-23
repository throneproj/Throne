#pragma once

#include <optional>

#include <QJsonObject>
#include <QStringList>

#include "include/engine/services/ServiceTypes.h"

namespace Configs {
    struct SettingsValues;
}

namespace Engine::SettingsRegistry {
    // The curated settings the API reads and writes, keyed by API name; keys this platform lacks are left out.
    QJsonObject Values();

    struct Context {
        bool systemProxyOn = false;
    };

    struct PutResult {
        // API names, in request order.
        QStringList changed;
    };

    // Main thread. Checks every key before writing any, then commits the changed ones through SettingsService::Apply. Nothing is
    // written when it returns an error.
    std::optional<ServiceError> Put(const QJsonObject &patch, const Context &context, PutResult *result);

    // The port rule every commit passes: each port that next changes or brings back into use, against the other ports in use and
    // the Throne API's listener. apiNames words it with the API names and code "bad_request", as Put always answered; otherwise
    // with database keys and code "port_conflict".
    std::optional<ServiceError> PortConflict(const Configs::SettingsValues &current, const Configs::SettingsValues &next, bool apiNames);

    // Settings DB keys a backup restore keeps from this host: the API, inbound and core ports, Tun, kill switch,
    // remember_* (except remember_id), NTP, log, system proxy, OS integration and host paths.
    const QStringList &HostKeys();
} // namespace Engine::SettingsRegistry
