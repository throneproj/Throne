#include "include/engine/Lifecycle.h"

#include <utility>
#include <QMutex>
#include <QMutexLocker>

#include "include/global/Const.hpp"
#include "include/global/Logger.hpp"

namespace Engine::Lifecycle {
    namespace {
        QMutex lifecycleStopMutex;
        StopHandler lifecycleStopHandler;
        QMutex lifecycleStateMutex;
        StateHooks lifecycleStateHooks;

        template <typename Hook>
        Hook lifecycleStateHook(Hook StateHooks::*hook) {
            QMutexLocker locker(&lifecycleStateMutex);
            return lifecycleStateHooks.*hook;
        }
    } // namespace

    void SetStopHandler(StopHandler handler) {
        QMutexLocker locker(&lifecycleStopMutex);
        lifecycleStopHandler = std::move(handler);
    }

    void StopRunningProfile(bool crash, bool block, bool manual, bool interactive) {
        StopHandler handler;
        {
            QMutexLocker locker(&lifecycleStopMutex);
            handler = lifecycleStopHandler;
        }
        if (!handler) {
            LOG_WARN("StopRunningProfile ignored: no lifecycle handler");
            return;
        }
        handler(crash, block, manual, interactive);
    }

    void SetStateHooks(StateHooks hooks) {
        QMutexLocker locker(&lifecycleStateMutex);
        lifecycleStateHooks = std::move(hooks);
    }

    int ResumeTargetId() {
        if (const auto get = lifecycleStateHook(&StateHooks::resumeTarget)) return get();
        return Configs::NoProfileId;
    }

    int RunningProfileId() {
        if (const auto get = lifecycleStateHook(&StateHooks::runningId)) return get();
        return Configs::NoProfileId;
    }

    void ClearResumeTarget() {
        if (const auto set = lifecycleStateHook(&StateHooks::setResumeTarget)) set(Configs::NoProfileId, false);
    }

    bool ForgetResumeTarget(int profileId) {
        const auto clearIf = lifecycleStateHook(&StateHooks::clearResumeTargetIf);
        return clearIf && clearIf(profileId, true);
    }
} // namespace Engine::Lifecycle
