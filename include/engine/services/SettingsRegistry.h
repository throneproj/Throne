#pragma once

#include <optional>

#include <QJsonObject>
#include <QStringList>

#include "include/engine/services/ServiceTypes.h"

namespace Engine::SettingsRegistry {
    // The curated settings the API reads and writes, keyed by API name; keys this platform lacks are left out.
    QJsonObject Values();

    struct Context {
        bool profileRunning = false;
        bool tunOn = false;
        bool systemProxyOn = false;
    };

    struct PutResult {
        // API names, in request order.
        QStringList changed;
        // A changed key takes effect only when the running profile restarts.
        bool restartNeeded = false;
    };

    // Main thread. Checks every key before writing any; then writes the changed ones, saves once, applies them and emits
    // SettingsChanged. Nothing is written when it returns an error.
    std::optional<ServiceError> Put(const QJsonObject &patch, const Context &context, PutResult *result);

    // Settings DB keys a backup restore keeps from this host: the API, inbound and core ports, Tun, kill switch,
    // remember_* (except remember_id), NTP, log, system proxy, OS integration and host paths.
    const QStringList &HostKeys();
} // namespace Engine::SettingsRegistry
