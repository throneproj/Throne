#include "include/database/SettingsSchema.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <type_traits>
#include <utility>

namespace Configs::SettingsSchema {
    namespace {
        std::string compactJson(const QJsonDocument &document) {
            return document.toJson(QJsonDocument::Compact).toStdString();
        }

        std::string serializeShortcuts(const SettingsValues &values) {
            QJsonObject object;
            for (auto it = values.shortcuts.cbegin(); it != values.shortcuts.cend(); ++it) object[it.key()] = it.value().toString();
            return compactJson(QJsonDocument(object));
        }

        void parseShortcuts(SettingsValues &values, const QString &raw) {
            const auto document = QJsonDocument::fromJson(raw.toUtf8());
            if (!document.isObject()) return;
            const auto object = document.object();
            QMap<QString, QKeySequence> shortcuts;
            for (auto it = object.constBegin(); it != object.constEnd(); ++it) shortcuts[it.key()] = QKeySequence(it.value().toString());
            values.shortcuts = std::move(shortcuts);
        }

        void copyShortcuts(const SettingsValues &from, SettingsValues &to) {
            to.shortcuts = from.shortcuts;
        }

        std::string serializeVlessPreference(const SettingsValues &values) {
            return std::to_string(static_cast<int>(values.xray_vless_preference));
        }

        void parseVlessPreference(SettingsValues &values, const QString &raw) {
            values.xray_vless_preference = static_cast<Xray::XrayVlessPreference>(raw.toInt());
        }

        void copyVlessPreference(const SettingsValues &from, SettingsValues &to) {
            to.xray_vless_preference = from.xray_vless_preference;
        }

        std::vector<Key> buildKeys() {
            using enum Scope;
            const Custom shortcuts{serializeShortcuts, parseShortcuts, copyShortcuts};
            const Custom vlessPreference{serializeVlessPreference, parseVlessPreference, copyVlessPreference};
            return {
                {"main_window_geometry", &SettingsValues::mainWindowGeometry, Host},
                {"log_level", &SettingsValues::log_level, Host, RestartProfile},
                {"test_url", &SettingsValues::test_latency_url},
                {"direct_test_url", &SettingsValues::direct_test_url, Portable, RestartProfile},
                {"exit_country_lookup", &SettingsValues::exit_country_lookup},
                {"exit_lookup_provider", &SettingsValues::exit_lookup_provider},
                {"service_check_disabled", &SettingsValues::service_check_disabled, Portable, UiProfileList},
                {"url_test_timeout_ms", &SettingsValues::url_test_timeout_ms, Portable, NoEffect, IntRange{0, 60000}},
                {"disable_tray", &SettingsValues::disable_tray, Host, UiTray},
                // 0 runs the core's own maximum.
                {"test_concurrent", &SettingsValues::test_concurrent, Portable, NoEffect, IntRange{0, 499}},
                {"disable_traffic_stats", &SettingsValues::disable_traffic_stats, Portable, RestartProfile | UiTrafficStats},
                {"current_group", &SettingsValues::current_group},
                {"mux_protocol", &SettingsValues::mux_protocol, Portable, RestartProfile},
                {"mux_padding", &SettingsValues::mux_padding, Portable, RestartProfile},
                {"mux_mode", &SettingsValues::mux_mode, Portable, RestartProfile},
                {"mux_concurrency", &SettingsValues::mux_concurrency, Portable, RestartProfile},
                {"mux_max_connections", &SettingsValues::mux_max_connections, Portable, RestartProfile},
                {"mux_min_streams", &SettingsValues::mux_min_streams, Portable, RestartProfile},
                {"mux_brutal", &SettingsValues::mux_brutal, Portable, RestartProfile},
                {"mux_brutal_up_mbps", &SettingsValues::mux_brutal_up_mbps, Portable, RestartProfile},
                {"mux_brutal_down_mbps", &SettingsValues::mux_brutal_down_mbps, Portable, RestartProfile},
                {"mux_default_on", &SettingsValues::mux_default_on, Portable, RestartProfile},
                {"fragment_implementation", &SettingsValues::fragment_implementation, Portable, RestartProfile},
                {"fragment_default_on", &SettingsValues::fragment_default_on, Portable, RestartProfile},
                {"fragment_size", &SettingsValues::fragment_size, Portable, RestartProfile},
                {"fragment_sleep", &SettingsValues::fragment_sleep, Portable, RestartProfile},
                {"tls_tricks_default_on", &SettingsValues::tls_tricks_default_on, Portable, RestartProfile},
                {"tls_spoof", &SettingsValues::tls_spoof, Portable, RestartProfile},
                {"tls_spoof_method", &SettingsValues::tls_spoof_method, Portable, RestartProfile},
                {"tls_spoof_default_on", &SettingsValues::tls_spoof_default_on, Portable, RestartProfile},
                {"h2_idle_timeout", &SettingsValues::h2_idle_timeout, Portable, RestartProfile},
                {"h2_keep_alive_period", &SettingsValues::h2_keep_alive_period, Portable, RestartProfile},
                {"h2_stream_receive_window", &SettingsValues::h2_stream_receive_window, Portable, RestartProfile},
                {"h2_connection_receive_window", &SettingsValues::h2_connection_receive_window, Portable, RestartProfile},
                {"h2_max_concurrent_streams", &SettingsValues::h2_max_concurrent_streams, Portable, RestartProfile},
                {"quic_initial_packet_size", &SettingsValues::quic_initial_packet_size, Portable, RestartProfile},
                {"quic_disable_path_mtu_discovery", &SettingsValues::quic_disable_path_mtu_discovery, Portable, RestartProfile},
                {"theme", &SettingsValues::theme, Host},
                {"language", &SettingsValues::language, Host, RestartProgram},
                {"font", &SettingsValues::font, Host},
                {"font_size", &SettingsValues::font_size, Host},
                {"mw_size", &SettingsValues::mw_size, Host},
                {"routing_settings_size", &SettingsValues::routing_settings_size, Host},
                {"log_enable_include", &SettingsValues::log_enable_include, Portable, UiLogFilter},
                {"log_enable_exclude", &SettingsValues::log_enable_exclude, Portable, UiLogFilter},
                {"log_include_keyword", &SettingsValues::log_include_keyword, Portable, UiLogFilter},
                {"log_include_regex", &SettingsValues::log_include_regex, Portable, UiLogFilter},
                {"log_exclude_keyword", &SettingsValues::log_exclude_keyword, Portable, UiLogFilter},
                {"log_exclude_regex", &SettingsValues::log_exclude_regex, Portable, UiLogFilter},
                {"start_minimal", &SettingsValues::start_minimal, Host},
                {"max_log_line", &SettingsValues::max_log_line, Host, UiLogLines},
                {"log_font_family", &SettingsValues::log_font_family, Host, UiLogFont},
                {"log_font_size", &SettingsValues::log_font_size, Host, UiLogFont},
                {"log_file_level", &SettingsValues::log_file_level, Host},
                {"splitter_state", &SettingsValues::splitter_state, Host},
                {"enable_stats", &SettingsValues::enable_stats, Portable, RestartProfile},
                {"stats_tab", &SettingsValues::stats_tab},
                {"traffic_graph_hidden", &SettingsValues::traffic_graph_hidden},
                {"traffic_graph_hidden_dirs", &SettingsValues::traffic_graph_hidden_dirs},
                {"traffic_graph_range", &SettingsValues::traffic_graph_range},
                {"traffic_graph_smooth", &SettingsValues::traffic_graph_smooth},
                {"traffic_graph_markers", &SettingsValues::traffic_graph_markers},
                {"connection_sort", &SettingsValues::connection_sort},
                {"connection_sort_asc", &SettingsValues::connection_sort_asc},
                {"traffic_stats_retention_days", &SettingsValues::traffic_stats_retention_days},
                {"disable_traffic_aggregation", &SettingsValues::disable_traffic_aggregation, Portable, UiTrafficStats},
                {"speed_test_mode", &SettingsValues::speed_test_mode},
                {"speed_test_timeout_ms", &SettingsValues::speed_test_timeout_ms},
                {"simple_dl_url", &SettingsValues::simple_dl_url},
                {"allow_beta_update", &SettingsValues::allow_beta_update},
                {"use_custom_icons", &SettingsValues::use_custom_icons, Portable, UiTrayIcon},
                {"follow_status_in_taskbar", &SettingsValues::follow_status_in_taskbar, Portable, UiTrayIcon},
                {"skip_delete_confirmation", &SettingsValues::skip_delete_confirmation},
                {"show_config_security", &SettingsValues::show_config_security, Portable, UiProfileList},
                {"simple_mode", &SettingsValues::simple_mode, Portable, UiMode},
                {"last_filter_column", &SettingsValues::last_filter_column},
                {"url_scheme_mirror", &SettingsValues::url_scheme_mirror, Host},
                {"url_scheme_auto_register", &SettingsValues::url_scheme_auto_register, Host},
                {"file_assoc_mirror", &SettingsValues::file_assoc_mirror, Host},
                {"file_assoc_auto_register", &SettingsValues::file_assoc_auto_register, Host},
                {"net_use_proxy", &SettingsValues::net_use_proxy},
                {"net_insecure", &SettingsValues::net_insecure},
                {"reset_proxy_on_disable_sp", &SettingsValues::reset_proxy_on_disable_sp, Host},
                {"user_agent2", &SettingsValues::user_agent},
                {"sub_tls_version", &SettingsValues::sub_tls_version},
                {"sub_http_version", &SettingsValues::sub_http_version},
                {"sub_auto_update", &SettingsValues::sub_auto_update, Portable, Periodic},
                {"sub_respect_server_interval", &SettingsValues::sub_respect_server_interval, Portable, Periodic},
                {"sub_clear", &SettingsValues::sub_clear},
                {"sub_show_change_popup", &SettingsValues::sub_show_change_popup},
                {"sub_send_hwid", &SettingsValues::sub_send_hwid},
                {"sub_custom_hwid_params", &SettingsValues::sub_custom_hwid_params},
                {"allow_stopping_active_profile", &SettingsValues::allow_stopping_active_profile},
                {"skip_cert", &SettingsValues::skip_cert, Portable, RestartProfile | UiProfileList},
                {"utlsFingerprint", &SettingsValues::utlsFingerprint, Portable, RestartProfile},
                {"disable_win_admin", &SettingsValues::disable_run_admin, Host, AutoRun},
                {"use_mozilla_certs", &SettingsValues::use_mozilla_certs, Portable, RestartProfile},
                {"kill_switch", &SettingsValues::kill_switch, Host, RestartProfile | RestartTun | KillSwitch},
                {"remote_api_enable", &SettingsValues::remote_api_enable, Host, RemoteApi},
                {"remote_api_lan", &SettingsValues::remote_api_lan, Host, RemoteApi},
                {"remote_api_admin_lan", &SettingsValues::remote_api_admin_lan, Host, RemoteApi},
                {"remote_api_port", &SettingsValues::remote_api_port, Host, RemoteApi},
                {"remote_api_key", &SettingsValues::remote_api_key, Host, RemoteApi},
                {"remote_api_allow", &SettingsValues::remote_api_allow, Host, RemoteApi},
                {"webdav_server", &SettingsValues::webdav_server, Device},
                {"webdav_user", &SettingsValues::webdav_user, Device},
                {"webdav_pass", &SettingsValues::webdav_pass, Device},
                {"webdav_path", &SettingsValues::webdav_path, Device},
                {"webdav_use_proxy", &SettingsValues::webdav_use_proxy, Device},
                {"device_id", &SettingsValues::device_id, Device},
                {"device_name", &SettingsValues::device_name, Device},
                {"system_proxy_enabled", &SettingsValues::remember_system_proxy, Host},
                {"tun_mode_enabled", &SettingsValues::remember_tun, Host},
                {"remember_id", &SettingsValues::remember_id},
                {"remember_enable", &SettingsValues::remember_enable, Host, UiMirrors},
                {"windows_set_admin", &SettingsValues::windows_set_admin, Host},
                {"shortcuts", shortcuts, Host, UiShortcuts},
                {"current_route_id", &SettingsValues::current_route_id, Portable, RestartRouting},
                {"route_auto_update", &SettingsValues::route_auto_update, Portable, Periodic},
                {"route_auto_update_last", &SettingsValues::route_auto_update_last},
                {"remote_dns", &SettingsValues::remote_dns, Portable, RestartRouting},
                {"remote_dns_disable_ipv6", &SettingsValues::remote_dns_disable_ipv6, Portable, RestartRouting},
                {"direct_dns", &SettingsValues::direct_dns, Portable, RestartRouting},
                {"direct_dns_disable_ipv6", &SettingsValues::direct_dns_disable_ipv6, Portable, RestartRouting},
                {"remote_dns_history", &SettingsValues::remote_dns_history},
                {"direct_dns_history", &SettingsValues::direct_dns_history},
                {"dns_cache_capacity", &SettingsValues::dns_cache_capacity, Portable, RestartRouting},
                {"dns_disable_cache", &SettingsValues::dns_disable_cache, Portable, RestartRouting},
                {"dns_disable_expire", &SettingsValues::dns_disable_expire, Portable, RestartRouting},
                {"dns_persist_cache", &SettingsValues::dns_persist_cache, Portable, RestartRouting},
                {"dns_reverse_mapping", &SettingsValues::dns_reverse_mapping, Portable, RestartRouting},
                {"enable_dns_routing", &SettingsValues::enable_dns_routing, Portable, RestartRouting},
                {"use_dns_object", &SettingsValues::use_dns_object, Portable, RestartRouting},
                {"dns_object", &SettingsValues::dns_object, Portable, RestartRouting},
                {"dns_final_out", &SettingsValues::dns_final_out, Portable, RestartRouting},
                {"dns_optimistic", &SettingsValues::dns_optimistic, Portable, RestartRouting},
                {"dns_optimistic_timeout", &SettingsValues::dns_optimistic_timeout, Portable, RestartRouting},
                {"dns_query_timeout", &SettingsValues::dns_query_timeout, Portable, RestartRouting},
                {"dns_use_hosts", &SettingsValues::dns_use_hosts, Portable, RestartRouting},
                {"dns_predefined_enable", &SettingsValues::dns_predefined_enable, Portable, RestartRouting},
                {"dns_predefined_rules", &SettingsValues::dns_predefined_rules, Portable, RestartRouting},
                {"domain_strategy", &SettingsValues::resolve_domain_strategy, Portable, RestartRouting},
                {"outbound_domain_strategy", &SettingsValues::default_domain_strategy, Portable, RestartRouting},
                {"ruleset_mirror", &SettingsValues::ruleset_mirror, Portable, RestartRouting},
                {"disable_mixed_inbound", &SettingsValues::disable_mixed_inbound, Host, RestartProfile},
                {"inbound_address", &SettingsValues::inbound_address, Host, RestartProfile | UiMirrors},
                {"inbound_socks_port", &SettingsValues::inbound_socks_port, Host, RestartProfile, IntRange{1, 65535}},
                {"random_inbound_port", &SettingsValues::random_inbound_port, Host, RestartProfile},
                {"custom_inbound", &SettingsValues::custom_inbound, Host, RestartProfile},
                {"proxy_scheme", &SettingsValues::proxy_scheme, Portable, RestartProfile},
                {"inbound_auth", &SettingsValues::inbound_auth, Host, RestartProfile},
                {"inbound_user", &SettingsValues::inbound_user, Host, RestartProfile},
                {"inbound_pass", &SettingsValues::inbound_pass, Host, RestartProfile},
                {"custom_route", &SettingsValues::custom_route_global},
                {"active_routing", &SettingsValues::active_routing},
                {"adblock_enable", &SettingsValues::adblock_enable, Portable, RestartRouting},
                {"fakedns", &SettingsValues::fake_dns, Portable, RestartRouting},
                {"fakeip_disable_ipv6", &SettingsValues::fakeip_disable_ipv6, Portable, RestartRouting},
                {"enable_tun_routing", &SettingsValues::enable_tun_routing, Host, RestartTun | KillSwitch},
                {"vpn_strict_route", &SettingsValues::vpn_strict_route, Host, RestartTun | KillSwitch},
                {"vpn_auto_redirect", &SettingsValues::vpn_auto_redirect, Host, RestartTun | KillSwitch},
                {"vpn_l3_bridge", &SettingsValues::vpn_l3_bridge, Host, RestartTun | KillSwitch},
                {"vpn_mtu", &SettingsValues::vpn_mtu, Host, RestartTun | KillSwitch, IntRange{1000, 10000}},
                {"disable_private_range_bypass", &SettingsValues::disable_private_range_bypass, Host, RestartTun | KillSwitch},
                {"vpn_private_ranges", &SettingsValues::vpn_private_ranges, Host, RestartTun | KillSwitch},
                {"vpn_ipv6", &SettingsValues::vpn_ipv6, Host, RestartTun | KillSwitch},
                {"vpn_tun_ipv4_cidr", &SettingsValues::vpn_tun_ipv4_cidr, Host, RestartTun | KillSwitch},
                {"vpn_tun_ipv6_cidr", &SettingsValues::vpn_tun_ipv6_cidr, Host, RestartTun | KillSwitch},
                {"disable_privilege_req", &SettingsValues::disable_privilege_req, Host},
                {"enable_ntp", &SettingsValues::enable_ntp, Host, RestartProfile},
                {"ntp_server_address", &SettingsValues::ntp_server_address, Host, RestartProfile},
                {"ntp_server_port", &SettingsValues::ntp_server_port, Host, RestartProfile},
                {"ntp_interval", &SettingsValues::ntp_interval, Host, RestartProfile},
                {"ntp_outbound", &SettingsValues::ntp_outbound, Host, RestartProfile},
                {"enable_warp", &SettingsValues::enable_warp, Portable, RestartRouting},
                {"enable_tor", &SettingsValues::enable_tor, Portable, RestartRouting},
                {"warp_private_key", &SettingsValues::warp_private_key, Portable, RestartRouting},
                {"warp_public_key", &SettingsValues::warp_public_key, Portable, RestartRouting},
                {"warp_ifc_addrs", &SettingsValues::warp_ifc_addrs, Portable, RestartRouting},
                {"warp_ep", &SettingsValues::warp_ep, Portable, RestartRouting},
                {"warp_reserved", &SettingsValues::warp_reserved, Portable, RestartRouting},
                {"warp_tos_accepted", &SettingsValues::warp_tos_accepted},
                {"warp_mode", &SettingsValues::warp_mode, Portable, RestartRouting},
                {"warp_amnezia_packets", &SettingsValues::warp_amnezia_packets, Portable, RestartRouting},
                {"warp_masque_private_key", &SettingsValues::warp_masque_private_key, Portable, RestartRouting},
                {"warp_masque_peer_public_key", &SettingsValues::warp_masque_peer_public_key, Portable, RestartRouting},
                {"warp_masque_ep", &SettingsValues::warp_masque_ep, Portable, RestartRouting},
                {"warp_masque_ifc_addrs", &SettingsValues::warp_masque_ifc_addrs, Portable, RestartRouting},
                {"warp_masque_sni", &SettingsValues::warp_masque_sni, Portable, RestartRouting},
                {"warp_masque_http_mode", &SettingsValues::warp_masque_http_mode, Portable, RestartRouting},
                {"warp_api_hosts", &SettingsValues::warp_api_hosts},
                {"warp_api_tls_fragment", &SettingsValues::warp_api_tls_fragment},
                {"warp_api_tls_record_fragment", &SettingsValues::warp_api_tls_record_fragment},
                {"warp_api_tls_fragment_fallback_delay", &SettingsValues::warp_api_tls_fragment_fallback_delay},
                {"hk_mw", &SettingsValues::hotkey_mainwindow, Host},
                {"hk_group", &SettingsValues::hotkey_group, Host},
                {"hk_route", &SettingsValues::hotkey_route, Host},
                {"hk_spmenu", &SettingsValues::hotkey_system_proxy_menu, Host},
                {"hk_toggle", &SettingsValues::hotkey_toggle_system_proxy, Host},
                {"hk_startstop", &SettingsValues::hotkey_toggle_connection, Host},
                {"hk_tun", &SettingsValues::hotkey_toggle_tun, Host},
                {"core_box_clash_api", &SettingsValues::core_box_clash_api, Host, RestartProfile},
                {"core_box_clash_listen_addr", &SettingsValues::core_box_clash_listen_addr, Host, RestartProfile},
                {"core_box_clash_api_secret", &SettingsValues::core_box_clash_api_secret, Host, RestartProfile},
                {"core_box_api_port", &SettingsValues::core_box_api_port, Host, RestartProfile},
                {"core_box_api_secret", &SettingsValues::core_box_api_secret, Host, RestartProfile},
                {"core_box_underlying_dns", &SettingsValues::core_box_underlying_dns, Host, RestartRouting},
                {"core_dns_in_port", &SettingsValues::core_dns_in_port, Host, RestartProfile, IntRange{1, 65535}},
                {"core_dns_in_lan", &SettingsValues::core_dns_in_lan, Host, RestartProfile},
                {"xray_log_level", &SettingsValues::xray_log_level, Host, RestartProfile},
                {"xray_mux_concurrency", &SettingsValues::xray_mux_concurrency, Portable, RestartProfile},
                {"xray_mux_max_connections", &SettingsValues::xray_mux_max_connections, Portable, RestartProfile},
                {"xray_mux_xudp_concurrency", &SettingsValues::xray_mux_xudp_concurrency, Portable, RestartProfile},
                {"xray_mux_xudp_proxy_udp443", &SettingsValues::xray_mux_xudp_proxy_udp443, Portable, RestartProfile},
                {"xray_mux_default_on", &SettingsValues::xray_mux_default_on, Portable, RestartProfile},
                {"xray_use_system_ca", &SettingsValues::xray_use_system_ca, Portable, RestartProfile},
                {"xray_vless_preference", vlessPreference, Portable, RestartProfile},
                {"xray_geoip_url", &SettingsValues::xray_geoip_url},
                {"xray_geosite_url", &SettingsValues::xray_geosite_url},
                {"xray_geoip_url_history", &SettingsValues::xray_geoip_url_history},
                {"xray_geosite_url_history", &SettingsValues::xray_geosite_url_history},
                {"extra_core_paths", &SettingsValues::extraCorePaths, Host},
                {"dial_bind_ifc_history", &SettingsValues::dial_bind_interface_history, Host},
                {"dial_inet4_bind_history", &SettingsValues::dial_inet4_bind_address_history, Host},
                {"dial_inet6_bind_history", &SettingsValues::dial_inet6_bind_address_history, Host},
            };
        }
    } // namespace

    const std::vector<Key> &Keys() {
        static const std::vector<Key> keys = buildKeys();
        return keys;
    }

    const Key *Find(const QString &name) {
        static const QHash<QString, const Key *> index = [] {
            QHash<QString, const Key *> byName;
            for (const auto &key : Keys()) byName.insert(key.name, &key);
            return byName;
        }();
        return index.value(name, nullptr);
    }

    std::string Serialize(const SettingsValues &values, const Key &key) {
        return std::visit(
            [&values](const auto &member) -> std::string {
                if constexpr (std::is_same_v<std::decay_t<decltype(member)>, Custom>) {
                    return member.serialize(values);
                } else {
                    const auto &value = values.*member;
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, bool>) return value ? "true" : "false";
                    else if constexpr (std::is_same_v<T, QString>) return value.toStdString();
                    else if constexpr (std::is_same_v<T, QStringList>) return compactJson(QJsonDocument(QJsonArray::fromStringList(value)));
                    else return std::to_string(value);
                }
            },
            key.member);
    }

    void Parse(SettingsValues &values, const Key &key, const QString &raw) {
        std::visit(
            [&values, &raw](const auto &member) {
                if constexpr (std::is_same_v<std::decay_t<decltype(member)>, Custom>) {
                    member.parse(values, raw);
                } else {
                    auto &value = values.*member;
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, bool>) {
                        value = raw == QLatin1String("true") || raw == QLatin1String("1");
                    } else if constexpr (std::is_same_v<T, int>) {
                        value = raw.toInt();
                    } else if constexpr (std::is_same_v<T, qint64>) {
                        value = raw.toLongLong();
                    } else if constexpr (std::is_same_v<T, QString>) {
                        value = raw;
                    } else {
                        const auto document = QJsonDocument::fromJson(raw.toUtf8());
                        if (!document.isArray()) return;
                        const auto array = document.array();
                        QStringList list;
                        list.reserve(array.size());
                        for (const auto &item : array) list << item.toString();
                        value = std::move(list);
                    }
                }
            },
            key.member);
    }

    QStringList Diff(const SettingsValues &a, const SettingsValues &b) {
        QStringList names;
        for (const auto &key : Keys()) {
            if (Serialize(a, key) != Serialize(b, key)) names << key.name;
        }
        return names;
    }

    void Copy(const SettingsValues &from, SettingsValues &to, const QStringList &keys) {
        for (const auto &name : keys) {
            const auto *key = Find(name);
            if (key == nullptr) continue;
            std::visit(
                [&from, &to](const auto &member) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(member)>, Custom>) member.copy(from, to);
                    else to.*member = from.*member;
                },
                key->member);
        }
    }

    quint32 EffectsOf(const QStringList &keys) {
        quint32 effects = NoEffect;
        for (const auto &name : keys) {
            if (const auto *key = Find(name)) effects |= key->effects;
        }
        return effects;
    }

    QStringList KeysIn(Scope scope) {
        QStringList names;
        for (const auto &key : Keys()) {
            if (key.scope == scope) names << key.name;
        }
        return names;
    }
} // namespace Configs::SettingsSchema
