#pragma once

#include "Const.hpp"
#include "Utils.hpp"
#include "include/database/DatabaseManager.h"
#include <map>
#include <vector>

// Switch core support

namespace Configs {
    inline QString GetRuleSetRemoteUrl(const QString& tag) {
        if (tag.startsWith("geosite-")) {
            return "https://raw.githubusercontent.com/SagerNet/sing-geosite/rule-set/" + tag + ".srs";
        }
        if (tag.startsWith("geoip-")) {
            return "https://raw.githubusercontent.com/SagerNet/sing-geoip/rule-set/" + tag + ".srs";
        }
        return {};
    }

    inline const std::vector<std::string> kDefaultRuleSetTags = {
        "geosite-geolocation-!cn",
        "geosite-geolocation-cn",
        "geosite-cn",
        "geosite-category-ads-all",
        "geosite-openai",
        "geosite-github",
        "geosite-google",
        "geosite-youtube",
        "geosite-telegram",
        "geosite-discord",
        "geosite-reddit",
        "geosite-netflix",
        "geosite-disney",
        "geosite-spotify",
        "geosite-tiktok",
        "geosite-private",
        "geoip-private",
        "geoip-cn",
        "geoip-ir",
        "geoip-ru",
        "geoip-us"
    };

    inline const std::map<std::string, std::string> ruleSetMap = [] {
        std::map<std::string, std::string> value;
        for (const auto& tag : kDefaultRuleSetTags) {
            value.emplace(tag, GetRuleSetRemoteUrl(QString::fromStdString(tag)).toStdString());
        }
        return value;
    }();

    void initDB(const std::string& dbPath);

    QString FindCoreRealPath();

    bool IsAdmin(bool forceRenew=false);

    bool isSetuidSet(const std::string& path);

    QString GetBasePath();
} // namespace Configs

#define ROUTES_PREFIX_NAME QString("route_profiles")
#define ROUTES_PREFIX QString(ROUTES_PREFIX_NAME + "/")
