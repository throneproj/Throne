#pragma once
#include "include/configs/ConfigBuild.h"
#include "include/configs/DnsAddress.h"
#include "include/configs/EndpointOverride.h"
#include "include/configs/ExitThrough.h"
#include "include/configs/RuleSetMirror.h"
#include "include/configs/Warp.h"

namespace Configs
{
    // Not "dashboard": that one is the Clash external_ui dir, holding a different UI.
    inline constexpr auto apiDashboardDir = "sb-dashboard";
}
