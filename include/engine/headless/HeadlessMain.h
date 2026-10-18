#pragma once

#include "include/engine/app/Bootstrap.h"

namespace Engine {
    // "Throne --headless" and the client modes (--health, --print-connection); QCoreApplication only. Returns the exit code.
    int RunHeadless(int argc, char **argv, const Bootstrap::Launch &launch);
} // namespace Engine
