#pragma once

#include "Const.hpp"
#include "Utils.hpp"
#include "ConfigItem.hpp"
#include "DataStore.hpp"

// Switch core support

namespace Configs {
    QString FindCoreRealPath();

    bool IsAdmin(bool forceRenew=false);

    QString GetBasePath();

    QString GetCoreAssetDir(const QString &name);

    bool NeedGeoAssets();
} // namespace Configs

#define ROUTES_PREFIX_NAME QString("route_profiles")
#define ROUTES_PREFIX QString(ROUTES_PREFIX_NAME + "/")
#define RULE_SETS_DIR QString("rule_sets")
