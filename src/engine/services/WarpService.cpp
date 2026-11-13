#include "include/engine/services/WarpService.h"

#include <memory>

#include "include/configs/generate.h"
#include "include/configs/sub/warp.h"
#include "include/database/DatabaseManager.h"
#include "include/engine/EditLocks.h"
#include "include/engine/ThreadCheck.h"
#include "include/global/Utils.hpp"

namespace Engine::WarpService {
    namespace {
        bool warpServiceRegistering = false;

        ServiceError warpServiceEditing() {
            return {409, QStringLiteral("editing"), QStringLiteral("Routing Settings is open in Throne's window; close it and try again")};
        }

        std::optional<ServiceError> warpServiceCheckMode(const QString &mode) {
            if (IsMode(mode)) return std::nullopt;
            return ServiceError{400, QStringLiteral("bad_request"), QStringLiteral("Unknown WARP mode \"%1\"; use wireguard, amnezia or masque").arg(mode)};
        }

        void warpServiceStore(const QString &mode, const Configs_network::WarpIdentity &identity, const QStringList &packets) {
            auto &settings = Configs::dataManager->settingsRepo;
            QStringList addresses;
            if (!identity.ipv4.isEmpty()) addresses << identity.ipv4.trimmed() + "/32";
            if (!identity.ipv6.isEmpty()) addresses << identity.ipv6.trimmed() + "/128";
            if (mode == QLatin1String("masque")) {
                settings->warp_masque_private_key = identity.privateKey.trimmed();
                settings->warp_masque_peer_public_key = identity.peerPublicKey.trimmed();
                settings->warp_masque_ep = identity.endpoint.trimmed();
                settings->warp_masque_ifc_addrs = addresses;
            } else {
                settings->warp_private_key = identity.privateKey.trimmed();
                settings->warp_public_key = identity.peerPublicKey.trimmed();
                settings->warp_ep = identity.endpoint.trimmed();
                settings->warp_ifc_addrs = addresses;
                settings->warp_reserved = QListInt2QListString(identity.reserved);
                if (mode == QLatin1String("amnezia")) settings->warp_amnezia_packets = packets;
            }
            settings->warp_mode = mode;
            settings->Save();
        }
    } // namespace

    Status Current() {
        const auto &settings = Configs::dataManager->settingsRepo;
        const bool masque = settings->warp_mode == QLatin1String("masque");
        return Status{
            .enabled = settings->enable_warp,
            .mode = settings->warp_mode,
            .configured = Configs::WarpConfigured(settings->warp_mode),
            .endpoint = masque ? settings->warp_masque_ep : settings->warp_ep,
            .addresses = masque ? settings->warp_masque_ifc_addrs : settings->warp_ifc_addrs,
            .termsAccepted = settings->warp_tos_accepted,
            .registering = warpServiceRegistering,
        };
    }

    bool IsMode(const QString &mode) {
        return mode == QLatin1String("wireguard") || mode == QLatin1String("amnezia") || mode == QLatin1String("masque");
    }

    std::optional<ServiceError> Apply(const Change &change, const std::function<std::optional<ServiceError>()> &beforeWrite,
                                      ChangeResult *result) {
        ENGINE_CHECK_MAIN_THREAD();
        if (change.mode) {
            if (auto refused = warpServiceCheckMode(*change.mode)) return refused;
        }
        auto &settings = Configs::dataManager->settingsRepo;
        const bool wasEnabled = settings->enable_warp;
        const bool enabled = change.enabled.value_or(wasEnabled);
        const QString mode = change.mode.value_or(settings->warp_mode);
        if (enabled == wasEnabled && mode == settings->warp_mode) return std::nullopt;
        // The dialog writes every warp_* field back on OK.
        if (IsBeingEdited(EditTarget::Routes)) return warpServiceEditing();
        if (enabled && !Configs::WarpConfigured(mode)) {
            return ServiceError{409, QStringLiteral("warp_not_configured"),
                                QStringLiteral("WARP has no device for the %1 mode yet; register one first").arg(mode)};
        }
        if (beforeWrite) {
            if (auto refused = beforeWrite()) return refused;
        }
        result->changed = true;
        const bool torWas = settings->enable_tor;
        settings->enable_warp = enabled;
        if (enabled) settings->enable_tor = false;
        settings->warp_mode = mode;
        settings->Save();
        // Tor outranks WARP, so WARP changes under a Tor that stays on change nothing.
        result->restartNeeded = torWas ? enabled : wasEnabled || enabled;
        return std::nullopt;
    }

    std::optional<ServiceError> Register(const QString &mode, bool acceptTerms,
                                         const std::function<std::optional<ServiceError>()> &beforeStore,
                                         const std::function<void(const RegisterResult &)> &done) {
        ENGINE_CHECK_MAIN_THREAD();
        if (auto refused = warpServiceCheckMode(mode)) return refused;
        if (IsBeingEdited(EditTarget::Routes)) return warpServiceEditing();
        if (warpServiceRegistering) {
            return ServiceError{409, QStringLiteral("busy"), QStringLiteral("Another WARP registration is running")};
        }
        auto &settings = Configs::dataManager->settingsRepo;
        if (!settings->warp_tos_accepted) {
            if (!acceptTerms) {
                return ServiceError{409, QStringLiteral("terms_required"),
                                    QStringLiteral("Registering a WARP device needs acceptance of Cloudflare's WARP terms: %1")
                                        .arg(QString::fromLatin1(kTermsUrl))};
            }
            settings->warp_tos_accepted = true;
            settings->Save();
        }

        warpServiceRegistering = true;
        runOnNewThread([mode, beforeStore, done] {
            QString error;
            const auto identity = Configs_network::RegisterWarp(mode == QLatin1String("masque") ? QStringLiteral("masque") : QStringLiteral("wireguard"), &error);
            QStringList packets;
            if (identity != nullptr && mode == QLatin1String("amnezia")) packets = Configs_network::GenerateWarpCamouflage(error);
            runOnUiThread([mode, beforeStore, done, identity, packets, error] {
                warpServiceRegistering = false;
                RegisterResult result;
                if (identity == nullptr || !error.isEmpty()) {
                    result.error = ServiceError{502, QStringLiteral("warp_failed"),
                                                error.isEmpty() ? QStringLiteral("The WARP registration failed") : error};
                } else if (IsBeingEdited(EditTarget::Routes)) {
                    result.error = warpServiceEditing();
                } else if (auto refused = beforeStore ? beforeStore() : std::nullopt) {
                    result.error = refused;
                } else {
                    warpServiceStore(mode, *identity, packets);
                    result.restartNeeded = Configs::dataManager->settingsRepo->enable_warp && !Configs::dataManager->settingsRepo->enable_tor;
                }
                done(result);
            });
        });
        return std::nullopt;
    }
} // namespace Engine::WarpService
