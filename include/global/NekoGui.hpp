#pragma once

#include "Const.hpp"
#include "NekoGui_Utils.hpp"
#include "NekoGui_ConfigItem.hpp"
#include "NekoGui_DataStore.hpp"

// Switch core support

namespace NekoGui {
    QString FindNekoBoxCoreRealPath();

    bool IsAdmin(bool forceRenew=false);

    QString GetBasePath();

    QString GetCoreAssetDir(const QString &name);

    bool NeedGeoAssets();
} // namespace NekoGui

#define ROUTES_PREFIX_NAME QString("route_profiles")
#define ROUTES_PREFIX QString(ROUTES_PREFIX_NAME + "/")
#define RULE_SETS_DIR QString("rule_sets")
