#pragma once

#include <functional>
#include <optional>

#include <QString>
#include <QStringList>

#include "include/engine/services/ServiceTypes.h"

// The built-in WARP of Routing Settings (settings enable_warp and warp_*), for the Throne API. Main thread only.
namespace Engine::WarpService {
    inline constexpr auto kTermsUrl = "https://www.cloudflare.com/application/terms/";

    struct Status {
        bool enabled = false;
        // "wireguard", "amnezia" or "masque".
        QString mode;
        // The mode's identity is complete, as a config build requires while WARP is on.
        bool configured = false;
        // The mode's endpoint and interface addresses.
        QString endpoint;
        QStringList addresses;
        bool termsAccepted = false;
        // A Register() is in flight.
        bool registering = false;
    };

    Status Current();

    bool IsMode(const QString &mode);

    struct Change {
        std::optional<bool> enabled;
        std::optional<QString> mode;
    };

    struct ChangeResult {
        bool changed = false;
        // WARP was on or is on now, so the running config is out of date.
        bool restartNeeded = false;
    };

    // Writes and saves enable_warp and warp_mode. Refuses an unknown mode; a change that changes nothing succeeds. A real
    // change is refused while Routing Settings is open, when enabled with an unconfigured mode, and when beforeWrite refuses.
    std::optional<ServiceError> Apply(const Change &change, const std::function<std::optional<ServiceError>()> &beforeWrite,
                                      ChangeResult *result);

    struct RegisterResult {
        // Nothing was stored when set.
        std::optional<ServiceError> error;
        // WARP is on, so the running config's identity is out of date.
        bool restartNeeded = false;
    };

    // Refuses an unknown mode, while Routing Settings is open, while another registration runs, and while the terms are
    // not accepted; acceptTerms accepts and saves them first. Otherwise registers a new Cloudflare device on a new thread,
    // then, on the main thread, stores its identity for mode and sets warp_mode unless Routing Settings opened meanwhile or
    // beforeStore refuses, and runs done.
    std::optional<ServiceError> Register(const QString &mode, bool acceptTerms,
                                         const std::function<std::optional<ServiceError>()> &beforeStore,
                                         const std::function<void(const RegisterResult &)> &done);
} // namespace Engine::WarpService
