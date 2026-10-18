#pragma once

// Process exit codes of the displayless modes (headless, its client modes, -dump-configs); the supervisor units key on them.
namespace Engine::ExitCode {
    inline constexpr int Ok = 0;
    // Also "--health: not ok".
    inline constexpr int Fatal = 1;
    // Client modes only: no instance, or no answer.
    inline constexpr int NoInstance = 2;
    inline constexpr int DataDirOwned = 3;
    inline constexpr int Usage = 64;
    // -dump-configs.
    inline constexpr int NoInput = 66;
    inline constexpr int CoreGaveUp = 70;
    inline constexpr int Stuck = 71;
    // -dump-configs.
    inline constexpr int CantCreate = 73;
    // -dump-configs.
    inline constexpr int IoError = 74;
    inline constexpr int Restart = 75;
    inline constexpr int Config = 78;
} // namespace Engine::ExitCode
