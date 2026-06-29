#include "include/database/SettingsRepo.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutexLocker>
#include <QDebug>

#include "include/global/Utils.hpp"

namespace Configs {
    SettingsRepo::SettingsRepo(Database& database) : db(database) {
        initMaps();
        createTables();
        loadAllSettings();
    }

    void SettingsRepo::initMaps() {
        boolMap = {
            {"disable_tray",                  &disable_tray},
            {"random_inbound_port",           &random_inbound_port},
            {"mux_padding",                   &mux_padding},
            {"mux_default_on",                &mux_default_on},
            {"fragment_default_on",           &fragment_default_on},
            {"tls_tricks_default_on",         &tls_tricks_default_on},
            {"net_use_proxy",                 &net_use_proxy},
            {"remember_enable",               &remember_enable},
            {"skip_cert",                     &skip_cert},
            {"fakedns",                       &fake_dns},
            {"disable_traffic_stats",         &disable_traffic_stats},
            {"disable_traffic_aggregation",   &disable_traffic_aggregation},
            {"vpn_ipv6",                      &vpn_ipv6},
            {"vpn_strict_route",              &vpn_strict_route},
            {"sub_clear",                     &sub_clear},
            {"net_insecure",                  &net_insecure},
            {"sub_send_hwid",                 &sub_send_hwid},
            {"start_minimal",                 &start_minimal},
            {"enable_ntp",                    &enable_ntp},
            {"enable_dns_server",             &enable_dns_server},
            {"dns_server_listen_lan",         &dns_server_listen_lan},
            {"enable_redirect",               &enable_redirect},
            {"system_dns_set",                &system_dns_set},
            {"windows_set_admin",             &windows_set_admin},
            {"disable_win_admin",             &disable_run_admin},
            {"enable_stats",                  &enable_stats},
            {"disable_privilege_req",         &disable_privilege_req},
            {"enable_tun_routing",            &enable_tun_routing},
            {"use_mozilla_certs",             &use_mozilla_certs},
            {"allow_beta_update",             &allow_beta_update},
            {"adblock_enable",                &adblock_enable},
            {"show_system_dns",               &show_system_dns},
            {"use_custom_icons",              &use_custom_icons},
            {"xray_mux_default_on",           &xray_mux_default_on},
            {"use_dns_object",                &use_dns_object},
            {"skip_delete_confirmation",      &skip_delete_confirmation},
            {"log_enable_include",            &log_enable_include},
            {"log_enable_exclude",            &log_enable_exclude},
            {"log_auto_scroll",               &log_auto_scroll},
            {"enable_warp",                   &enable_warp},
            {"enable_dns_routing",            &enable_dns_routing},
            {"inbound_auth",                  &inbound_auth},
            {"allow_stopping_active_profile", &allow_stopping_active_profile},
            {"disable_mixed_inbound",         &disable_mixed_inbound},
            {"system_proxy_enabled",          &remember_system_proxy},
            {"tun_mode_enabled",              &remember_tun},
            {"reset_proxy_on_disable_sp", &reset_proxy_on_disable_sp},
            {"dns_disable_cache", &dns_disable_cache},
            {"dns_disable_expire", &dns_disable_expire},
            {"dns_reverse_mapping", &dns_reverse_mapping},
        };

        intMap = {
            {"current_group",          &current_group},
            {"inbound_socks_port",     &inbound_socks_port},
            {"mux_concurrency",        &mux_concurrency},
            {"test_concurrent",        &test_concurrent},
            {"remember_id",            &remember_id},
            {"language",               &language},
            {"font_size",              &font_size},
            {"max_log_line",           &max_log_line},
            {"stats_tab",              &stats_tab},
            {"traffic_stats_retention_days", &traffic_stats_retention_days},
            {"sub_auto_update",        &sub_auto_update},
            {"vpn_mtu",                &vpn_mtu},
            {"ntp_server_port",        &ntp_server_port},
            {"dns_server_listen_port", &dns_server_listen_port},
            {"redirect_listen_port",   &redirect_listen_port},
            {"core_box_clash_api",     &core_box_clash_api},
            {"speed_test_mode",        &speed_test_mode},
            {"speed_test_timeout_ms",  &speed_test_timeout_ms},
            {"url_test_timeout_ms",    &url_test_timeout_ms},
            {"xray_mux_concurrency",   &xray_mux_concurrency},
            {"current_route_id",       &current_route_id},
            {"ruleset_mirror",         &ruleset_mirror},
            {"core_dns_in_port",       &core_dns_in_port},
            {"dns_cache_capacity", &dns_cache_capacity},
        };

        stringMap = {
            {"user_agent2",                &user_agent},
            {"test_url",                   &test_latency_url},
            {"inbound_address",            &inbound_address},
            {"log_level",                  &log_level},
            {"mux_protocol",               &mux_protocol},
            {"fragment_implementation",    &fragment_implementation},
            {"fragment_size",              &fragment_size},
            {"fragment_sleep",             &fragment_sleep},
            {"theme",                      &theme},
            {"custom_inbound",             &custom_inbound},
            {"custom_route",               &custom_route_global},
            {"font",                       &font},
            {"hk_mw",                      &hotkey_mainwindow},
            {"hk_group",                   &hotkey_group},
            {"hk_route",                   &hotkey_route},
            {"hk_spmenu",                  &hotkey_system_proxy_menu},
            {"hk_toggle",                  &hotkey_toggle_system_proxy},
            {"active_routing",             &active_routing},
            {"mw_size",                    &mw_size},
            {"vpn_impl",                   &vpn_implementation},
            {"vpn_tun_ipv4_cidr",          &vpn_tun_ipv4_cidr},
            {"vpn_tun_ipv6_cidr",          &vpn_tun_ipv6_cidr},
            {"sub_custom_hwid_params",     &sub_custom_hwid_params},
            {"splitter_state",             &splitter_state},
            {"utlsFingerprint",            &utlsFingerprint},
            {"core_box_clash_listen_addr", &core_box_clash_listen_addr},
            {"core_box_clash_api_secret",  &core_box_clash_api_secret},
            {"core_box_underlying_dns",    &core_box_underlying_dns},
            {"ntp_server_address",         &ntp_server_address},
            {"ntp_interval",               &ntp_interval},
            {"dns_v4_resp",                &dns_v4_resp},
            {"dns_v6_resp",                &dns_v6_resp},
            {"redirect_listen_address",    &redirect_listen_address},
            {"proxy_scheme",               &proxy_scheme},
            {"main_window_geometry",       &mainWindowGeometry},
            {"xray_log_level",             &xray_log_level},
            {"remote_dns",                 &remote_dns},
            {"remote_dns_strategy",        &remote_dns_strategy},
            {"direct_dns",                 &direct_dns},
            {"direct_dns_strategy",        &direct_dns_strategy},
            {"dns_object",                 &dns_object},
            {"dns_final_out",              &dns_final_out},
            {"domain_strategy",            &resolve_domain_strategy},
            {"outbound_domain_strategy",   &default_domain_strategy},
            {"simple_dl_url",              &simple_dl_url},
            {"warp_private_key",           &warp_private_key},
            {"warp_public_key",            &warp_public_key},
            {"warp_ep",                    &warp_ep},
            {"inbound_user",               &inbound_user},
            {"inbound_pass",               &inbound_pass},
            {"url_scheme_mirror",          &url_scheme_mirror},
        };

        stringListMap = {
            {"dns_server_rules",         &dns_server_rules},
            {"extra_core_paths",         &extraCorePaths},
            {"log_include_keyword",      &log_include_keyword},
            {"log_include_regex",        &log_include_regex},
            {"log_exclude_keyword",      &log_exclude_keyword},
            {"log_exclude_regex",        &log_exclude_regex},
            {"warp_ifc_addrs",           &warp_ifc_addrs},
            {"dial_bind_ifc_history",    &dial_bind_interface_history},
            {"dial_inet4_bind_history",  &dial_inet4_bind_address_history},
            {"dial_inet6_bind_history",  &dial_inet6_bind_address_history},
        };
    }

    void SettingsRepo::createTables() const {
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS settings (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL
            )
        )");
    }

    void SettingsRepo::loadAllSettings() {
        auto query = db.query("SELECT key, value FROM settings");
        if (!query) return;

        while (query->executeStep()) {
            const QString key = QString::fromStdString(query->getColumn(0).getText());
            const QString str = QString::fromStdString(query->getColumn(1).getText());

            if (auto boolVal = boolMap.find(key); boolVal != boolMap.end()) {
                *boolVal.value() = str == "true" || str == "1";
            } else if (auto intVal = intMap.find(key); intVal != intMap.end()) {
                bool ok;
                *intVal.value() = str.toInt(&ok);
                if (!ok) *intVal.value() = 0;
            } else if (auto strListVal = stringListMap.find(key); strListVal != stringListMap.end()) {
                QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
                if (doc.isArray()) {
                    QStringList list;
                    for (const auto& val : doc.array()) list << val.toString();
                    *strListVal.value() = list;
                }
            } else if (auto strVal = stringMap.find(key); strVal != stringMap.end()) {
                *strVal.value() = str;
            } else if (key == "shortcuts") {
                QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
                if (doc.isObject()) {
                    auto obj = doc.object();
                    for (const auto& key : obj.keys()) {
                        qDebug() << key << obj[key];
                        shortcuts[key] = QKeySequence(obj[key].toString());
                    }
                }
            } else if (key == "xray_vless_preference") {
                bool ok;
                int v = str.toInt(&ok);
                xray_vless_preference = static_cast<Xray::XrayVlessPreference>(ok ? v : 0);
            }
        }
    }

    void SettingsRepo::saveAllSettings() const {
        if (noSave) return;

        std::vector<std::pair<std::string, std::string>> keyValues;
        keyValues.reserve(boolMap.size() + intMap.size() + stringMap.size() + stringListMap.size() + 2);

        for (auto it = boolMap.begin(); it != boolMap.end(); ++it)
            keyValues.emplace_back(it.key().toStdString(), *it.value() ? "true" : "false");

        for (auto it = intMap.begin(); it != intMap.end(); ++it)
            keyValues.emplace_back(it.key().toStdString(), QString::number(*it.value()).toStdString());

        for (auto it = stringMap.begin(); it != stringMap.end(); ++it)
            keyValues.emplace_back(it.key().toStdString(), it.value()->toStdString());

        for (auto it = stringListMap.begin(); it != stringListMap.end(); ++it) {
            QJsonArray arr;
            for (const QString& s : *it.value()) arr.append(s);
            keyValues.emplace_back(it.key().toStdString(),
                QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)).toStdString());
        }

        {
            QJsonObject obj;
            for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it)
                obj[it.key()] = it.value().toString();
            keyValues.emplace_back("shortcuts",
                QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)).toStdString());
        }

        keyValues.emplace_back("xray_vless_preference",
            QString::number(static_cast<int>(xray_vless_preference)).toStdString());

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
            return "Throne/" + version;
        }
        return user_agent;
    }

    bool SettingsRepo::Save() {
        saveAllSettings();
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
