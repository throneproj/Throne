#include "include/database/SettingsRepo.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>

#include "include/global/Utils.hpp"

namespace Configs {
    const QSet<QString> boolKeys = {
            "disable_tray",
            "random_inbound_port",
            "mux_padding",
            "mux_default_on",
            "net_use_proxy",
            "remember_enable",
            "skip_cert",
            "fakedns",
            "disable_traffic_stats",
            "vpn_ipv6",
            "vpn_strict_route",
            "sub_clear",
            "net_insecure",
            "sub_send_hwid",
            "start_minimal",
            "enable_ntp",
            "enable_dns_server",
            "dns_server_listen_lan",
            "enable_redirect",
            "system_dns_set",
            "windows_set_admin",
            "disable_win_admin",
            "enable_stats",
            "disable_privilege_req",
            "enable_tun_routing",
            "use_mozilla_certs",
            "allow_beta_update",
            "adblock_enable",
            "show_system_dns",
            "use_custom_icons",
            "xray_mux_default_on",
            "use_dns_object"
        };

        const QSet<QString> intKeys = {
            "current_group",
            "current_group_id",
            "inbound_socks_port",
            "mux_concurrency",
            "test_concurrent",
            "remember_id",
            "language",
            "font_size",
            "max_log_line",
            "stats_tab",
            "sub_auto_update",
            "vpn_mtu",
            "ntp_server_port",
            "dns_server_listen_port",
            "redirect_listen_port",
            "core_box_clash_api",
            "speed_test_mode",
            "speed_test_timeout_ms",
            "url_test_timeout_ms",
            "xray_mux_concurrency",
            "current_route_id",
            "sniffing_mode",
            "ruleset_mirror"
        };

        const QSet<QString> stringListKeys = {
            "spmode2",
            "dns_server_rules",
            "log_ignore",  // Not in _add() but exists as QStringList in DataStore
            "extra_core_paths"  // Extra core paths list
        };

        const QSet<QString> stringKeys = {
            "user_agent2",
            "test_url",
            "inbound_address",
            "log_level",
            "mux_protocol",
            "theme",
            "custom_inbound",
            "custom_route",
            "font",
            "hk_mw",
            "hk_group",
            "hk_route",
            "hk_spmenu",
            "hk_toggle",
            "active_routing",
            "mw_size",
            "vpn_impl",
            "sub_custom_hwid_params",
            "splitter_state",
            "utlsFingerprint",
            "core_box_clash_listen_addr",
            "core_box_clash_api_secret",
            "core_box_underlying_dns",
            "ntp_server_address",
            "ntp_interval",
            "dns_v4_resp",
            "dns_v6_resp",
            "redirect_listen_address",
            "proxy_scheme",
            "main_window_geometry",
            "xray_log_level",
            "remote_dns",
            "remote_dns_strategy",
            "direct_dns",
            "direct_dns_strategy",
            "dns_object",
            "dns_final_out",
            "domain_strategy",
            "outbound_domain_strategy",
            "simple_dl_url"
        };

    SettingsRepo::SettingsRepo(Database& database) : db(database) {
        createTables();
        loadAllSettings();
    }

    void SettingsRepo::createTables() const {
        // Create key-value table for settings
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS settings (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL
            )
        )");
    }

    QString SettingsRepo::valueToString(const QVariant& value, const QString& key) const {
        if (key == "shortcuts") {
            // Serialize shortcuts map as JSON object
            QJsonObject obj;
            auto shortcutsMap = value.value<QMap<QString,QKeySequence>>();
            for (auto it = shortcutsMap.begin(); it != shortcutsMap.end(); ++it) {
                if (!it.value().isEmpty()) {
                    obj[it.key()] = it.value().toString();
                }
            }
            QJsonDocument doc(obj);
            return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        } else if (stringListKeys.contains(key)) {
            // Serialize QStringList as JSON array
            QJsonArray arr;
            QStringList list = value.toStringList();
            for (const QString& item : list) {
                arr.append(item);
            }
            QJsonDocument doc(arr);
            return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        } else if (boolKeys.contains(key)) {
            return value.toBool() ? "true" : "false";
        } else if (intKeys.contains(key)) {
            return QString::number(value.toInt());
        } else if (stringKeys.contains(key)) {
            return value.toString();
        } else {
            // Unknown key - log and default to string
            qDebug() << "SettingsRepo::valueToString: Unknown key type for key:" << key << ", defaulting to string";
            return value.toString();
        }
    }

    QVariant SettingsRepo::stringToValue(const QString& str, const QString& key) const {
        // Special handling for shortcuts (JSON object)
        if (key == "shortcuts") {
            QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
            if (doc.isObject()) {
                QMap<QString, QKeySequence> shortcutsMap;
                QJsonObject obj = doc.object();
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    QString keyName = it.key();
                    QString valueStr = it.value().toString();
                    if (!valueStr.isEmpty()) {
                        shortcutsMap[keyName] = QKeySequence(valueStr);
                    }
                }
                return QVariant::fromValue(shortcutsMap);
            }
            return QVariant::fromValue(QMap<QString, QKeySequence>());
        }
        
        // Determine type based on key lists
        if (stringListKeys.contains(key)) {
            // Try to parse as JSON array
            QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
            if (doc.isArray()) {
                QStringList list;
                QJsonArray arr = doc.array();
                for (const QJsonValue& val : arr) {
                    list.append(val.toString());
                }
                return list;
            }
            return QStringList();
        } else if (intKeys.contains(key)) {
            bool ok;
            int val = str.toInt(&ok);
            return ok ? val : 0;
        } else if (boolKeys.contains(key)) {
            return str == "true" || str == "1";
        } else if (stringKeys.contains(key)) {
            return str;
        } else {
            // Unknown key - log and default to string
            qDebug() << "SettingsRepo::stringToValue: Unknown key type for key:" << key << ", defaulting to string";
            return str;
        }
    }

    void SettingsRepo::loadAllSettings() {
        // Load all settings from database
        auto query = db.query("SELECT key, value FROM settings");
        if (query) {
            while (query->executeStep()) {
                QString key = QString::fromStdString(query->getColumn(0).getText());
                QString value = QString::fromStdString(query->getColumn(1).getText());
                
                QVariant varValue = stringToValue(value, key);
                
                // Map keys to fields (matching DataStore key names)
                if (key == "main_window_geometry") mainWindowGeometry = varValue.toString();
                else if (key == "log_level") log_level = varValue.toString();
                else if (key == "test_url") test_latency_url = varValue.toString();
                else if (key == "url_test_timeout_ms") url_test_timeout_ms = varValue.toInt();
                else if (key == "disable_tray") disable_tray = varValue.toBool();
                else if (key == "test_concurrent") test_concurrent = varValue.toInt();
                else if (key == "disable_traffic_stats") disable_traffic_stats = varValue.toBool();
                else if (key == "current_group") current_group = varValue.toInt();
                else if (key == "mux_protocol") mux_protocol = varValue.toString();
                else if (key == "mux_padding") mux_padding = varValue.toBool();
                else if (key == "mux_concurrency") mux_concurrency = varValue.toInt();
                else if (key == "mux_default_on") mux_default_on = varValue.toBool();
                else if (key == "theme") theme = varValue.toString();
                else if (key == "language") language = varValue.toInt();
                else if (key == "font") font = varValue.toString();
                else if (key == "font_size") font_size = varValue.toInt();
                else if (key == "mw_size") mw_size = varValue.toString();
                else if (key == "log_ignore") log_ignore = varValue.toStringList();
                else if (key == "start_minimal") start_minimal = varValue.toBool();
                else if (key == "max_log_line") max_log_line = varValue.toInt();
                else if (key == "splitter_state") splitter_state = varValue.toString();
                else if (key == "enable_stats") enable_stats = varValue.toBool();
                else if (key == "stats_tab") stats_tab = varValue.toInt();
                else if (key == "speed_test_mode") speed_test_mode = varValue.toInt();
                else if (key == "speed_test_timeout_ms") speed_test_timeout_ms = varValue.toInt();
                else if (key == "simple_dl_url") simple_dl_url = varValue.toString();
                else if (key == "allow_beta_update") allow_beta_update = varValue.toBool();
                else if (key == "show_system_dns") show_system_dns = varValue.toBool();
                else if (key == "use_custom_icons") use_custom_icons = varValue.toBool();
                else if (key == "net_use_proxy") net_use_proxy = varValue.toBool();
                else if (key == "net_insecure") net_insecure = varValue.toBool();
                else if (key == "user_agent2") user_agent = varValue.toString();
                else if (key == "sub_auto_update") sub_auto_update = varValue.toInt();
                else if (key == "sub_clear") sub_clear = varValue.toBool();
                else if (key == "sub_send_hwid") sub_send_hwid = varValue.toBool();
                else if (key == "sub_custom_hwid_params") sub_custom_hwid_params = varValue.toString();
                else if (key == "skip_cert") skip_cert = varValue.toBool();
                else if (key == "utlsFingerprint") utlsFingerprint = varValue.toString();
                else if (key == "disable_win_admin") disable_run_admin = varValue.toBool();
                else if (key == "use_mozilla_certs") use_mozilla_certs = varValue.toBool();
                else if (key == "spmode2") remember_spmode = varValue.toStringList();
                else if (key == "remember_id") remember_id = varValue.toInt();
                else if (key == "remember_enable") remember_enable = varValue.toBool();
                else if (key == "windows_set_admin") windows_set_admin = varValue.toBool();
                else if (key == "shortcuts") shortcuts = varValue.value<QMap<QString, QKeySequence>>();
                else if (key == "current_route_id") current_route_id = varValue.toInt();
                else if (key == "remote_dns") remote_dns = varValue.toString();
                else if (key == "remote_dns_strategy") remote_dns_strategy = varValue.toString();
                else if (key == "direct_dns") direct_dns = varValue.toString();
                else if (key == "direct_dns_strategy") direct_dns_strategy = varValue.toString();
                else if (key == "use_dns_object") use_dns_object = varValue.toBool();
                else if (key == "dns_object") dns_object = varValue.toString();
                else if (key == "dns_final_out") dns_final_out = varValue.toString();
                else if (key == "domain_strategy") domain_strategy = varValue.toString();
                else if (key == "outbound_domain_strategy") outbound_domain_strategy = varValue.toString();
                else if (key == "sniffing_mode") sniffing_mode = varValue.toInt();
                else if (key == "ruleset_mirror") ruleset_mirror = varValue.toInt();
                else if (key == "inbound_address") inbound_address = varValue.toString();
                else if (key == "inbound_socks_port") inbound_socks_port = varValue.toInt();
                else if (key == "random_inbound_port") random_inbound_port = varValue.toBool();
                else if (key == "custom_inbound") custom_inbound = varValue.toString();
                else if (key == "custom_route") custom_route_global = varValue.toString();
                else if (key == "active_routing") active_routing = varValue.toString();
                else if (key == "adblock_enable") adblock_enable = varValue.toBool();
                else if (key == "fakedns") fake_dns = varValue.toBool();
                else if (key == "enable_tun_routing") enable_tun_routing = varValue.toBool();
                else if (key == "vpn_impl") vpn_implementation = varValue.toString();
                else if (key == "vpn_mtu") vpn_mtu = varValue.toInt();
                else if (key == "vpn_ipv6") vpn_ipv6 = varValue.toBool();
                else if (key == "vpn_strict_route") vpn_strict_route = varValue.toBool();
                else if (key == "disable_privilege_req") disable_privilege_req = varValue.toBool();
                else if (key == "enable_ntp") enable_ntp = varValue.toBool();
                else if (key == "ntp_server_address") ntp_server_address = varValue.toString();
                else if (key == "ntp_server_port") ntp_server_port = varValue.toInt();
                else if (key == "ntp_interval") ntp_interval = varValue.toString();
                else if (key == "enable_dns_server") enable_dns_server = varValue.toBool();
                else if (key == "dns_server_listen_lan") dns_server_listen_lan = varValue.toBool();
                else if (key == "dns_server_listen_port") dns_server_listen_port = varValue.toInt();
                else if (key == "dns_v4_resp") dns_v4_resp = varValue.toString();
                else if (key == "dns_v6_resp") dns_v6_resp = varValue.toString();
                else if (key == "dns_server_rules") dns_server_rules = varValue.toStringList();
                else if (key == "enable_redirect") enable_redirect = varValue.toBool();
                else if (key == "redirect_listen_address") redirect_listen_address = varValue.toString();
                else if (key == "redirect_listen_port") redirect_listen_port = varValue.toInt();
                else if (key == "system_dns_set") system_dns_set = varValue.toBool();
                else if (key == "proxy_scheme") proxy_scheme = varValue.toString();
                else if (key == "hk_mw") hotkey_mainwindow = varValue.toString();
                else if (key == "hk_group") hotkey_group = varValue.toString();
                else if (key == "hk_route") hotkey_route = varValue.toString();
                else if (key == "hk_spmenu") hotkey_system_proxy_menu = varValue.toString();
                else if (key == "hk_toggle") hotkey_toggle_system_proxy = varValue.toString();
                else if (key == "core_box_clash_api") core_box_clash_api = varValue.toInt();
                else if (key == "core_box_clash_listen_addr") core_box_clash_listen_addr = varValue.toString();
                else if (key == "core_box_clash_api_secret") core_box_clash_api_secret = varValue.toString();
                else if (key == "core_box_underlying_dns") core_box_underlying_dns = varValue.toString();
                else if (key == "xray_log_level") xray_log_level = varValue.toString();
                else if (key == "xray_mux_concurrency") xray_mux_concurrency = varValue.toInt();
                else if (key == "xray_mux_default_on") xray_mux_default_on = varValue.toBool();
                else if (key == "extra_core_paths") extraCorePaths = varValue.toStringList();
            }
        }
    }

    void SettingsRepo::saveAllSettings() const {
        if (noSave) return;
        struct SettingEntry {
            QString key;
            QVariant value;
        };
        
        QList<SettingEntry> settings = {
            {"main_window_geometry", mainWindowGeometry},
            {"log_level", log_level},
            {"test_url", test_latency_url},
            {"url_test_timeout_ms", url_test_timeout_ms},
            {"disable_tray", disable_tray},
            {"test_concurrent", test_concurrent},
            {"disable_traffic_stats", disable_traffic_stats},
            {"current_group", current_group},
            {"mux_protocol", mux_protocol},
            {"mux_padding", mux_padding},
            {"mux_concurrency", mux_concurrency},
            {"mux_default_on", mux_default_on},
            {"theme", theme},
            {"language", language},
            {"font", font},
            {"font_size", font_size},
            {"mw_size", mw_size},
            {"log_ignore", log_ignore},
            {"start_minimal", start_minimal},
            {"max_log_line", max_log_line},
            {"splitter_state", splitter_state},
            {"enable_stats", enable_stats},
            {"stats_tab", stats_tab},
            {"speed_test_mode", speed_test_mode},
            {"speed_test_timeout_ms", speed_test_timeout_ms},
            {"simple_dl_url", simple_dl_url},
            {"allow_beta_update", allow_beta_update},
            {"show_system_dns", show_system_dns},
            {"use_custom_icons", use_custom_icons},
            {"net_use_proxy", net_use_proxy},
            {"net_insecure", net_insecure},
            {"user_agent2", user_agent},
            {"sub_auto_update", sub_auto_update},
            {"sub_clear", sub_clear},
            {"sub_send_hwid", sub_send_hwid},
            {"sub_custom_hwid_params", sub_custom_hwid_params},
            {"skip_cert", skip_cert},
            {"utlsFingerprint", utlsFingerprint},
            {"disable_win_admin", disable_run_admin},
            {"use_mozilla_certs", use_mozilla_certs},
            {"spmode2", remember_spmode},
            {"remember_id", remember_id},
            {"remember_enable", remember_enable},
            {"windows_set_admin", windows_set_admin},
            {"shortcuts", QVariant::fromValue(shortcuts)},
            {"current_route_id", current_route_id},
            {"remote_dns", remote_dns},
            {"remote_dns_strategy", remote_dns_strategy},
            {"direct_dns", direct_dns},
            {"direct_dns_strategy", direct_dns_strategy},
            {"use_dns_object", use_dns_object},
            {"dns_object", dns_object},
            {"dns_final_out", dns_final_out},
            {"domain_strategy", domain_strategy},
            {"outbound_domain_strategy", outbound_domain_strategy},
            {"sniffing_mode", sniffing_mode},
            {"ruleset_mirror", ruleset_mirror},
            {"inbound_address", inbound_address},
            {"inbound_socks_port", inbound_socks_port},
            {"random_inbound_port", random_inbound_port},
            {"custom_inbound", custom_inbound},
            {"custom_route", custom_route_global},
            {"active_routing", active_routing},
            {"adblock_enable", adblock_enable},
            {"fakedns", fake_dns},
            {"enable_tun_routing", enable_tun_routing},
            {"vpn_impl", vpn_implementation},
            {"vpn_mtu", vpn_mtu},
            {"vpn_ipv6", vpn_ipv6},
            {"vpn_strict_route", vpn_strict_route},
            {"disable_privilege_req", disable_privilege_req},
            {"enable_ntp", enable_ntp},
            {"ntp_server_address", ntp_server_address},
            {"ntp_server_port", ntp_server_port},
            {"ntp_interval", ntp_interval},
            {"enable_dns_server", enable_dns_server},
            {"dns_server_listen_lan", dns_server_listen_lan},
            {"dns_server_listen_port", dns_server_listen_port},
            {"dns_v4_resp", dns_v4_resp},
            {"dns_v6_resp", dns_v6_resp},
            {"dns_server_rules", dns_server_rules},
            {"enable_redirect", enable_redirect},
            {"redirect_listen_address", redirect_listen_address},
            {"redirect_listen_port", redirect_listen_port},
            {"system_dns_set", system_dns_set},
            {"proxy_scheme", proxy_scheme},
            {"hk_mw", hotkey_mainwindow},
            {"hk_group", hotkey_group},
            {"hk_route", hotkey_route},
            {"hk_spmenu", hotkey_system_proxy_menu},
            {"hk_toggle", hotkey_toggle_system_proxy},
            {"core_box_clash_api", core_box_clash_api},
            {"core_box_clash_listen_addr", core_box_clash_listen_addr},
            {"core_box_clash_api_secret", core_box_clash_api_secret},
            {"core_box_underlying_dns", core_box_underlying_dns},
            {"xray_log_level", xray_log_level},
            {"xray_mux_concurrency", xray_mux_concurrency},
            {"xray_mux_default_on", xray_mux_default_on},
            {"extra_core_paths", extraCorePaths},
        };

        std::vector<std::pair<std::string, std::string>> keyValues;
        keyValues.reserve(settings.size());
        for (const auto& entry : settings) {
            QString valueStr = valueToString(entry.value, entry.key);
            keyValues.emplace_back(entry.key.toStdString(), valueStr.toStdString());
        }
        db.execBatchSettingsReplace(keyValues);
    }

    void SettingsRepo::UpdateStartedId(int id) {
        started_id = id;
        remember_id = id;
        Save();
    }

    QString SubStrBefore(QString str, const QString &sub) {
        if (!str.contains(sub)) return str;
        return str.left(str.indexOf(sub));
    }

    QString SettingsRepo::GetUserAgent(bool isDefault) const {
        if (user_agent.isEmpty()) {
            isDefault = true;
        }
        if (isDefault) {
            QString version = SubStrBefore(NKR_VERSION, "-");
            if (!version.contains(".")) version = "1.0.0";
            return "Throne/" + version + " (Prefer ClashMeta Format)";
        }
        return user_agent;
    }

    bool SettingsRepo::Save() {
        runOnNewThread([=, this] {
            saveAllSettings();
        });
        return true;
    }

    QStringList SettingsRepo::GetExtraCorePaths() const {
        return extraCorePaths;
    }

    bool SettingsRepo::AddExtraCorePath(const QString &path) {
        if (extraCorePaths.contains(path)) {
            return false;
        }
        extraCorePaths.append(path);
        return true;
    }
}
