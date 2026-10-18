#pragma once

#include <functional>

namespace Engine::Lifecycle {
    using StopHandler = std::function<void(bool crash, bool block, bool manual, bool interactive)>;

    void SetStopHandler(StopHandler handler);

    // Runs the handler on the calling thread, never marshalled with a wait; a logged no-op while none is set.
    // interactive false: failures and slow stops are logged, never shown.
    void StopRunningProfile(bool crash, bool block, bool manual, bool interactive = true);
} // namespace Engine::Lifecycle
