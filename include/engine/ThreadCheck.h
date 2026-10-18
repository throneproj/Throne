#pragma once

#include <atomic>

#include <QCoreApplication>
#include <QString>
#include <QThread>

#include "include/global/Logger.hpp"

// Main-thread-only entry points: warns once per call site when reached from another thread, never aborts.
#define ENGINE_CHECK_MAIN_THREAD()                                                                       \
    do {                                                                                                 \
        const auto *engineCheckApp = QCoreApplication::instance();                                       \
        if (engineCheckApp != nullptr && QThread::currentThread() != engineCheckApp->thread()) {         \
            static std::atomic<bool> engineCheckWarned{false};                                           \
            if (!engineCheckWarned.exchange(true)) {                                                     \
                LOG_WARN("THREAD-AFFINITY " + QString(Q_FUNC_INFO) + " called off the main thread");     \
            }                                                                                            \
        }                                                                                                \
    } while (false)
