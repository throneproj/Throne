#pragma once

namespace Engine {
    // Hidden "Throne -dump-configs <outdir> -appdata <dir>": writes every config generated from a copied data dir, displayless.
    int RunConfigDump(int argc, char **argv);
} // namespace Engine
