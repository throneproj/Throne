#include "include/engine/services/SettingsService.h"

#include "include/database/SettingsSchema.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/EngineHost.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/services/SettingsRegistry.h"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"

namespace Engine::SettingsService {
    namespace Schema = Configs::SettingsSchema;

    std::optional<ServiceError> Apply(const Configs::SettingsValues &proposed, const QStringList &keys, const ApplyOptions &options,
                                      ApplyResult *result) {
        ENGINE_CHECK_MAIN_THREAD();
        if (result != nullptr) *result = {};
        auto &settings = *Configs::dataManager->settingsRepo;
        if (settings.prepare_exit || settings.noSave) {
            return ServiceError{503, QStringLiteral("exiting"), QStringLiteral("Throne is shutting down")};
        }
        const Configs::SettingsValues &current = settings;
        auto next = current;
        Schema::Copy(proposed, next, keys);
        if (next.random_inbound_port && !current.random_inbound_port) next.inbound_socks_port = MkPort(next.inbound_address);
        if (auto conflict = SettingsRegistry::PortConflict(current, next, false)) return conflict;

        const auto changed = Schema::Diff(current, next);
        if (changed.isEmpty()) return std::nullopt;
        Schema::Copy(next, settings, changed);
        settings.Save();

        const quint32 effects = Schema::EffectsOf(changed);
        auto *engine = host();
        if (result != nullptr) {
            const bool running = engine != nullptr && engine->runningSnapshot() != nullptr;
            result->changed = changed;
            result->effects = effects;
            result->restartNeeded = running && ((effects & (Schema::RestartProfile | Schema::RestartRouting)) != 0 ||
                                                ((effects & Schema::RestartTun) != 0 && settings.spmode_vpn));
        }
        if (engine != nullptr) engine->applySettingsEffects(changed, effects, options.interactive);
        EngineEvents::SettingsChanged(changed);
        LOG_INFO(QStringLiteral("Settings changed: %1").arg(changed.join(QStringLiteral(", "))));
        return std::nullopt;
    }

    std::optional<ServiceError> ApplyEdit(const Configs::SettingsValues &snapshot, const Configs::SettingsValues &proposed,
                                          const ApplyOptions &options, ApplyResult *result) {
        return Apply(proposed, Schema::Diff(snapshot, proposed), options, result);
    }

    std::optional<ServiceError> Update(const std::function<void(Configs::SettingsValues &)> &edit, const ApplyOptions &options,
                                       ApplyResult *result) {
        ENGINE_CHECK_MAIN_THREAD();
        const Configs::SettingsValues &current = *Configs::dataManager->settingsRepo;
        auto proposed = current;
        edit(proposed);
        return Apply(proposed, Schema::Diff(current, proposed), options, result);
    }
} // namespace Engine::SettingsService
