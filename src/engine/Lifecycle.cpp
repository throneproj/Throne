#include "include/engine/Lifecycle.h"

#include <utility>
#include <QMutex>
#include <QMutexLocker>

#include "include/global/Logger.hpp"

namespace Engine::Lifecycle {
    namespace {
        QMutex lifecycleStopMutex;
        StopHandler lifecycleStopHandler;
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
} // namespace Engine::Lifecycle
