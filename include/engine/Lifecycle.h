#pragma once

#include <functional>

namespace Engine::Lifecycle {
    using StopHandler = std::function<void(bool crash, bool block, bool manual, bool interactive)>;

    void SetStopHandler(StopHandler handler);

    // Calls the handler on the calling thread; a logged no-op while none is set. The stop is decided on the main thread (posted
    // from others). block spins a nested loop on the caller until the stop it started or joined was torn down, or it found
    // nothing; during a start it waits for that start first. Crash stops and stops during exit never wait for another.
    // interactive false: failures and slow stops are logged, never shown.
    void StopRunningProfile(bool crash, bool block, bool manual, bool interactive = true);

    // What a restarted core resumes and what runs, held by EngineState. EngineHost installs the hooks once the connection
    // exists; without them nothing has been started, so the accessors read NoProfileId and change nothing.
    struct StateHooks {
        std::function<int()> resumeTarget;
        std::function<void(int profileId, bool persist)> setResumeTarget;
        std::function<bool(int profileId, bool persist)> clearResumeTargetIf;
        std::function<int()> runningId;
    };
    void SetStateHooks(StateHooks hooks);
    int ResumeTargetId();                     // any thread
    int RunningProfileId();                   // any thread; NoProfileId while nothing runs, a crashed profile waiting to resume included
    void ClearResumeTarget();                 // NoProfileId, not persisted (remember_id untouched)
    bool ForgetResumeTarget(int profileId);   // clears and persists (remember_id) only while it names profileId
} // namespace Engine::Lifecycle
