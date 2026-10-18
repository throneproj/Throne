#pragma once

#include <QProcessEnvironment>

#include "include/api/remote/Overlay.hpp"
#include "include/engine/app/Bootstrap.h"
#include "include/engine/headless/HeadlessCli.h"

// Throne --headless --check.
namespace Engine::Preflight {
    // A client mode, after the flag parse and ResolveDataDir(create = false): never takes the data-dir lock, never calls
    // Logging::Init, never opens the database for writing, never creates or changes a file. One "[ OK ]", "[WARN]", "[FAIL]" or
    // "[INFO]" line per item on stdout, with an indented "fix:" line where there is one; plain English. 0 when nothing FAILed,
    // 1 otherwise. FAIL only for what stops every operation: the data dir, TLS, the core binary, a required port.
    int Run(const HeadlessCli::LaunchOptions &options, const QProcessEnvironment &env, const RemoteApi::Overlay &overlay,
            const Bootstrap::DataDir &dataDir);
} // namespace Engine::Preflight
