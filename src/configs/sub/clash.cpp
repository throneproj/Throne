#include "include/configs/sub/clash.hpp"

#include "include/configs/ParseDiagnostics.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <cmath>
#include <limits>

namespace clash {

namespace {
// A whole decimal number in int range.
bool clashReadInt(const fkyaml::node& node, int& out) {
    constexpr long long low = std::numeric_limits<int>::min();
    constexpr long long high = std::numeric_limits<int>::max();
    if (node.is_integer()) {
        const auto v = node.get_value<long long>();
        if (v < low || v > high) return false;
        out = static_cast<int>(v);
        return true;
    }
    if (node.is_float_number()) {
        const auto v = node.get_value<double>();
        if (!std::isfinite(v) || v != std::trunc(v) || v < static_cast<double>(low) || v > static_cast<double>(high)) return false;
        out = static_cast<int>(v);
        return true;
    }
    if (node.is_string()) {
        bool ok = false;
        const auto v = QString::fromStdString(node.get_value<std::string>()).trimmed().toInt(&ok, 10);
        if (!ok) return false;
        out = v;
        return true;
    }
    return false;
}

// Only true/false/1/0: mihomo refuses the YAML 1.1 yes/no/on/off spellings, which arrive as strings.
bool clashReadBool(const fkyaml::node& node, bool& out) {
    if (node.is_boolean()) {
        out = node.get_value<bool>();
        return true;
    }
    if (node.is_integer()) {
        const auto v = node.get_value<long long>();
        if (v != 0 && v != 1) return false;
        out = v == 1;
        return true;
    }
    if (node.is_string()) {
        const auto s = QString::fromStdString(node.get_value<std::string>()).trimmed().toLower();
        if (s == "true" || s == "1") {
            out = true;
            return true;
        }
        if (s == "false" || s == "0") {
            out = false;
            return true;
        }
    }
    return false;
}

QString clashNodeText(const fkyaml::node& node) {
    if (node.is_string()) return QString::fromStdString(node.get_value<std::string>());
    if (node.is_integer()) return QString::number(node.get_value<long long>());
    if (node.is_float_number()) return QString::number(node.get_value<double>(), 'g', 15);
    if (node.is_boolean()) return node.get_value<bool>() ? QStringLiteral("true") : QStringLiteral("false");
    if (node.is_sequence()) return QStringLiteral("[…]");
    if (node.is_mapping()) return QStringLiteral("{…}");
    return {};
}
}

void from_node(const fkyaml::node& node, MyBool& b) {
    bool value = false;
    b.value = clashReadBool(node, value) && value;
}

void from_node(const fkyaml::node& node, MyInt& i) {
    int value = 0;
    i.value = clashReadInt(node, value) ? value : 0;
}

void from_node(const fkyaml::node& node, WgReserved& w) {
    w = {};
    if (node.is_string()) {
        const auto s = node.get_value<std::string>();
        // Go's StdEncoding: padded, so the length is a multiple of four.
        const auto decoded = QByteArray::fromBase64Encoding(QByteArray::fromStdString(s), QByteArray::AbortOnBase64DecodingErrors);
        if (decoded && s.size() % 4 == 0) {
            for (const char byte : *decoded) w.value.push_back(static_cast<uint8_t>(byte));
        } else {
            w.text = s;
        }
    } else if (node.is_sequence()) {
        QStringList items;
        bool numeric = true;
        for (const auto& item : node.as_seq()) {
            int byte = 0;
            numeric = clashReadInt(item, byte) && numeric;
            w.value.push_back(byte);
            items << clashNodeText(item);
        }
        if (!numeric) {
            w.value.clear();
            w.text = items.join(',').toStdString();
        }
    } else if (!node.is_null()) {
        w.text = clashNodeText(node).toStdString();
    }
}

template <typename Target>
inline void load_opt(const fkyaml::node& node, const char* key, Target& target) {
    if (node.is_mapping() && node.contains(key)) {
        target = node[key].get_value<Target>();
    }
}

template <>
inline void load_opt<std::string>(const fkyaml::node& node, const char* key, std::string& target) {
    if (node.is_mapping() && node.contains(key)) {
        if (node[key].is_string()) target = node[key].get_value<std::string>();
        else if (node[key].is_integer()) target = std::to_string(node[key].get_value<long long>());
        else if (node[key].is_float_number()) target = QByteArray::number(node[key].get_value<double>(), 'g', 15).toStdString();
        else if (node[key].is_boolean()) target = node[key].get_value<bool>() ? "true" : "false";
    }
}

// The key is known here, unlike in from_node, so a value of the wrong type is reported instead of silently zeroed.
template <>
inline void load_opt<MyInt>(const fkyaml::node& node, const char* key, MyInt& target) {
    if (!node.is_mapping() || !node.contains(key)) return;
    const auto& value = node[key];
    if (value.is_null()) return;
    int parsed = 0;
    const bool ok = clashReadInt(value, parsed);
    target.value = ok ? parsed : 0;
    if (ok || !Configs::ParseDiag::Active()) return;
    Configs::ParseDiag::Warn(QString::fromUtf8(key), QCoreApplication::translate("Validation", "Expected a whole number; ignored."),
                             clashNodeText(value));
}

template <>
inline void load_opt<MyBool>(const fkyaml::node& node, const char* key, MyBool& target) {
    if (!node.is_mapping() || !node.contains(key)) return;
    const auto& value = node[key];
    if (value.is_null()) return;
    bool parsed = false;
    const bool ok = clashReadBool(value, parsed);
    target.value = ok && parsed;
    if (ok || !Configs::ParseDiag::Active()) return;
    Configs::ParseDiag::Warn(QString::fromUtf8(key), QCoreApplication::translate("Validation", "Expected true or false; ignored."),
                             clashNodeText(value));
}

template <>
inline void load_opt<bool>(const fkyaml::node& node, const char* key, bool& target) {
    if (node.is_mapping() && node.contains(key)) {
        if (node[key].is_boolean()) target = node[key].get_value<bool>();
        else if (node[key].is_integer()) target = node[key].get_value<long long>() == 1;
        else if (node[key].is_string()) {
            std::string s = node[key].get_value<std::string>();
            target = (s == "true" || s == "1");
        }
    }
}

void from_node(const fkyaml::node& node, smuxOpts& opts) {
    load_opt(node, "enabled", opts.enabled);
    load_opt(node, "max-connections", opts.max_connections);
    load_opt(node, "max-streams", opts.max_streams);
    load_opt(node, "min-streams", opts.min_streams);
    load_opt(node, "padding", opts.padding);
    load_opt(node, "protocol", opts.protocol);
}

void from_node(const fkyaml::node& node, grpcOpts& opts) {
    load_opt(node, "grpc-service-name", opts.grpc_service_name);
}

void from_node(const fkyaml::node& node, xhttpReuseSettings& opts) {
    load_opt(node, "max-concurrency", opts.max_concurrency);
    load_opt(node, "max-connections", opts.max_connections);
    load_opt(node, "c-max-reuse-times", opts.c_max_reuse_times);
    load_opt(node, "h-max-request-times", opts.h_max_request_times);
    load_opt(node, "h-max-reusable-secs", opts.h_max_reusable_secs);
    load_opt(node, "h-keep-alive-period", opts.h_keep_alive_period);
}

// A missing or null key leaves the target unset.
template <typename Target>
void load_present(const fkyaml::node& node, const char* key, std::optional<Target>& target) {
    if (node.is_mapping() && node.contains(key) && !node[key].is_null()) load_opt(node, key, target.emplace());
}

void from_node(const fkyaml::node& node, xhttpDownloadSettings& opts) {
    load_present(node, "host", opts.host);
    load_present(node, "path", opts.path);
    load_present(node, "headers", opts.headers);
    load_opt(node, "mode", opts.mode);
    load_present(node, "server", opts.server);
    load_present(node, "port", opts.port);
    load_present(node, "tls", opts.tls);
    if (node.is_mapping() && node.contains("alpn")) {
        if (node["alpn"].is_string()) {
            opts.alpn = std::vector{node["alpn"].get_value<std::string>()};
        } else if (node["alpn"].is_sequence()) {
            opts.alpn = node["alpn"].get_value<std::vector<std::string>>();
        }
    }
    load_present(node, "servername", opts.servername);
    load_present(node, "client-fingerprint", opts.client_fingerprint);
    load_present(node, "fingerprint", opts.fingerprint);
    load_present(node, "reality-opts", opts.reality_opts);
    load_opt(node, "x-padding-obfs-mode", opts.x_padding_obfs_mode);
    load_opt(node, "x-padding-key", opts.x_padding_key);
    load_opt(node, "x-padding-header", opts.x_padding_header);
    load_opt(node, "x-padding-placement", opts.x_padding_placement);
    load_opt(node, "x-padding-method", opts.x_padding_method);
    load_opt(node, "reuse-settings", opts.reuse_settings);
}

void from_node(const fkyaml::node& node, xhttpOpts& opts) {
    load_opt(node, "host", opts.host);
    load_opt(node, "path", opts.path);
    load_opt(node, "mode", opts.mode);
    load_opt(node, "headers", opts.headers);
    load_opt(node, "no-grpc-header", opts.no_grpc_header);
    load_opt(node, "x-padding-bytes", opts.x_padding_bytes);
    load_opt(node, "x-padding-obfs-mode", opts.x_padding_obfs_mode);
    load_opt(node, "x-padding-key", opts.x_padding_key);
    load_opt(node, "x-padding-header", opts.x_padding_header);
    load_opt(node, "x-padding-placement", opts.x_padding_placement);
    load_opt(node, "x-padding-method", opts.x_padding_method);
    load_opt(node, "uplink-http-method", opts.uplink_http_method);
    // 3x-ui writes session-id-*, which mihomo ignores; mihomo's own names win.
    load_opt(node, "session-id-placement", opts.session_placement);
    load_opt(node, "session-id-key", opts.session_key);
    load_opt(node, "session-id-table", opts.session_table);
    load_opt(node, "session-id-length", opts.session_length);
    load_opt(node, "session-placement", opts.session_placement);
    load_opt(node, "session-key", opts.session_key);
    load_opt(node, "session-table", opts.session_table);
    load_opt(node, "session-length", opts.session_length);
    load_opt(node, "seq-placement", opts.seq_placement);
    load_opt(node, "seq-key", opts.seq_key);
    load_opt(node, "uplink-data-placement", opts.uplink_data_placement);
    load_opt(node, "uplink-data-key", opts.uplink_data_key);
    load_opt(node, "uplink-chunk-size", opts.uplink_chunk_size);
    load_opt(node, "sc-max-each-post-bytes", opts.sc_max_each_post_bytes);
    load_opt(node, "sc-min-posts-interval-ms", opts.sc_min_posts_interval_ms);
    load_opt(node, "reuse-settings", opts.reuse_settings);
    // mihomo reads a null download-settings as none, not as a copy of the proxy.
    if (node.is_mapping() && node.contains("download-settings") && !node["download-settings"].is_null()) {
        opts.has_download_settings = true;
        opts.download_settings = node["download-settings"].get_value<xhttpDownloadSettings>();
    }
}

void from_node(const fkyaml::node& node, httpOpts& opts) {
    if (node.is_mapping() && node.contains("headers")) {
        opts.headers = node["headers"].get_value<std::map<std::string, std::vector<std::string>>>();
    }
    load_opt(node, "method", opts.method);
    load_opt(node, "path", opts.path);
}

void from_node(const fkyaml::node& node, h2Opts& opts) {
    if (node.is_mapping() && node.contains("host")) {
        auto& host_node = node["host"];
        if (host_node.is_string()) {
            opts.host.push_back(host_node.get_value<std::string>());
        } else if (host_node.is_sequence()) {
            opts.host = host_node.get_value<std::vector<std::string>>();
        }
    }
    load_opt(node, "path", opts.path);
}

void from_node(const fkyaml::node& node, wsOpts& opts) {
    load_opt(node, "early-data-header-name", opts.early_data_header_name);
    load_opt(node, "headers", opts.headers);
    load_opt(node, "max-early-data", opts.max_early_data);
    load_opt(node, "path", opts.path);
    load_opt(node, "v2ray-http-upgrade", opts.v2ray_http_upgrade);
}

void from_node(const fkyaml::node& node, realityOpts& opts) {
    load_opt(node, "public-key", opts.public_key);
    load_opt(node, "short-id", opts.short_id);
}

void from_node(const fkyaml::node& node, obfs& opts) {
    load_opt(node, "mode", opts.mode);
    load_opt(node, "host", opts.host);
}

void from_node(const fkyaml::node& node, v2rayPlugin& opts) {
    load_opt(node, "mode", opts.mode);
    load_opt(node, "tls", opts.tls);
    load_opt(node, "host", opts.host);
    load_opt(node, "path", opts.path);
    load_opt(node, "mux", opts.mux);
}

void from_node(const fkyaml::node& node, wgPeer& peer) {
    load_opt(node, "server", peer.server);
    load_opt(node, "port", peer.port);
    load_opt(node, "ip", peer.ip);
    load_opt(node, "ipv6", peer.ipv6);
    load_opt(node, "public-key", peer.public_key);
    load_opt(node, "pre-shared-key", peer.pre_shared_key);
    if (node.is_mapping() && node.contains("reserved")) {
        peer.reserved = node["reserved"].get_value<WgReserved>();
    }
    // mihomo folds '_' into '-' when matching keys.
    for (const char* key : {"allowed-ips", "allowed_ips"}) {
        if (!node.is_mapping() || !node.contains(key)) continue;
        const auto& ips = node[key];
        if (ips.is_string()) {
            peer.allowed_ips.push_back(ips.get_value<std::string>());
        } else if (ips.is_sequence()) {
            for (const auto& ip : ips.as_seq()) {
                if (ip.is_string()) peer.allowed_ips.push_back(ip.get_value<std::string>());
            }
        }
        break;
    }
}

void from_node(const fkyaml::node& node, wgAmneziaOpts& opts) {
    load_opt(node, "jc", opts.jc);
    load_opt(node, "jmin", opts.jmin);
    load_opt(node, "jmax", opts.jmax);
    load_opt(node, "s1", opts.s1);
    load_opt(node, "s2", opts.s2);
    load_opt(node, "s3", opts.s3);
    load_opt(node, "s4", opts.s4);
    load_opt(node, "h1", opts.h1);
    load_opt(node, "h2", opts.h2);
    load_opt(node, "h3", opts.h3);
    load_opt(node, "h4", opts.h4);
    load_opt(node, "i1", opts.i1);
    load_opt(node, "i2", opts.i2);
    load_opt(node, "i3", opts.i3);
    load_opt(node, "i4", opts.i4);
    load_opt(node, "i5", opts.i5);
    load_opt(node, "header-protection-key", opts.header_protection_key);
    load_opt(node, "content-padding-addition", opts.content_padding_addition);
    load_opt(node, "rekey-after-time", opts.rekey_after_time);
    load_opt(node, "rekey-timeout", opts.rekey_timeout);
    load_opt(node, "reject-after-time", opts.reject_after_time);
    load_opt(node, "keepalive-timeout", opts.keepalive_timeout);
    load_opt(node, "max-handshake-attempts", opts.max_handshake_attempts);
    load_opt(node, "random-trailers", opts.random_trailers);
    load_opt(node, "disable-cookies", opts.disable_cookies);
}

void from_node(const fkyaml::node& node, Proxies& p) {
    load_opt(node, "name", p.name);
    load_opt(node, "type", p.type);
    load_opt(node, "server", p.server);
    load_opt(node, "port", p.port);
    load_opt(node, "cipher", p.cipher);
    load_opt(node, "uuid", p.uuid);
    load_opt(node, "alterId", p.alterId);
    load_opt(node, "udp", p.udp);
    load_opt(node, "tls", p.tls);
    load_opt(node, "skip-cert-verify", p.skip_cert_verify);
    load_opt(node, "servername", p.servername);
    load_opt(node, "network", p.network);
    load_opt(node, "ws-opts", p.ws_opts);
    load_opt(node, "ws-headers", p.ws_headers);
    load_opt(node, "xhttp-opts", p.xhttp_opts);
    load_opt(node, "h2-opts", p.h2_opts);
    load_opt(node, "http-opts", p.http_opts);
    load_opt(node, "grpc-opts", p.grpc_opts);
    load_opt(node, "username", p.username);
    load_opt(node, "password", p.password);
    load_opt(node, "sni", p.sni);
    if (node.is_mapping() && node.contains("alpn")) {
        if (node["alpn"].is_string()) {
            p.alpn.push_back(node["alpn"].get_value<std::string>());
        } else if (node["alpn"].is_sequence()) {
            p.alpn = node["alpn"].get_value<std::vector<std::string>>();
        }
    }
    load_opt(node, "plugin", p.plugin);
    // Read in place: destroying a copy of an anchored fkYAML node nulls the anchor and every alias of it.
    if (p.type == "ss" && node.is_mapping() && node.contains("plugin-opts")) {
        const auto& opts = node["plugin-opts"];
        if (p.plugin == "v2ray-plugin") {
            p.plugin_opts = opts.get_value<v2rayPlugin>();
        } else if (p.plugin == "obfs") {
            const auto simple = opts.get_value<obfs>();
            p.plugin_opts.mode = simple.mode;
            p.plugin_opts.host = simple.host;
        }
    }
    load_opt(node, "fingerprint", p.fingerprint);
    load_opt(node, "obfs", p.obfs);
    load_opt(node, "protocol", p.protocol);
    load_opt(node, "obfs-param", p.obfs_param);
    load_opt(node, "protocol-param", p.protocol_param);
    load_opt(node, "client-fingerprint", p.client_fingerprint);
    load_opt(node, "flow", p.flow);
    load_opt(node, "packet-encoding", p.packet_encoding);
    load_opt(node, "reality-opts", p.reality_opts);
    load_opt(node, "auth-str", p.auth_str);
    load_opt(node, "auth_str", p.auth_str1);
    load_opt(node, "disable_mtu_discovery", p.disable_mtu_discovery);
    load_opt(node, "down", p.down);
    load_opt(node, "fast-open", p.fast_open);
    load_opt(node, "recv-window", p.recv_window);
    load_opt(node, "recv-window-conn", p.recv_window_conn);
    load_opt(node, "recv_window", p.recv_window1);
    load_opt(node, "recv_window_conn", p.recv_window_conn1);
    load_opt(node, "up", p.up);
    load_opt(node, "ports", p.ports);
    load_opt(node, "smux", p.smux);
    load_opt(node, "udp-over-tcp", p.udp_over_tcp);
    load_opt(node, "udp-over-tcp-version", p.udp_over_tcp_version);
    load_opt(node, "ip", p.ip);
    load_opt(node, "ipv6", p.ipv6);
    load_opt(node, "public-key", p.public_key);
    load_opt(node, "pre-shared-key", p.pre_shared_key);
    load_opt(node, "private-key", p.private_key);
    load_opt(node, "private-key-passphrase", p.private_key_passphrase);
    if (node.is_mapping() && node.contains("host-key")) {
        if (node["host-key"].is_string()) {
            p.host_key.push_back(node["host-key"].get_value<std::string>());
        } else if (node["host-key"].is_sequence()) {
            p.host_key = node["host-key"].get_value<std::vector<std::string>>();
        }
    }
    if (node.is_mapping() && node.contains("host-key-algorithms")) {
        if (node["host-key-algorithms"].is_string()) {
            p.host_key_algorithms.push_back(node["host-key-algorithms"].get_value<std::string>());
        } else if (node["host-key-algorithms"].is_sequence()) {
            p.host_key_algorithms = node["host-key-algorithms"].get_value<std::vector<std::string>>();
        }
    }
    if (node.is_mapping() && node.contains("reserved")) {
        p.reserved = node["reserved"].get_value<WgReserved>();
    }
    load_opt(node, "dialer-proxy", p.dialer_proxy);
    load_opt(node, "peers", p.peers);
    load_opt(node, "workers", p.workers);
    load_opt(node, "persistent-keepalive", p.persistent_keepalive);
    load_opt(node, "keepalive", p.keepalive);
    // A null option leaves mihomo's pointer nil, so it is not AmneziaWG.
    if (node.is_mapping() && node.contains("amnezia-wg-option") && node["amnezia-wg-option"].is_mapping()) {
        p.amnezia_wg_option = node["amnezia-wg-option"].get_value<wgAmneziaOpts>();
    }
    load_opt(node, "wnoise", p.wnoise);
    load_opt(node, "wnoisecount", p.wnoisecount);
    load_opt(node, "wpayloadsize", p.wpayloadsize);
    load_opt(node, "mtu", p.mtu);
    load_opt(node, "disable-sni", p.disable_sni);
    load_opt(node, "congestion-controller", p.congestion_controller);
    load_opt(node, "udp-relay-mode", p.udp_relay_mode);
    load_opt(node, "reduce-rtt", p.reduce_rtt);
    load_opt(node, "heartbeat-interval", p.heartbeat_interval);
    load_opt(node, "obfs-password", p.obfs_password);
    load_opt(node, "tfo", p.tfo);
    load_opt(node, "mptcp", p.mptcp);
    load_opt(node, "idle-session-check-interval", p.idle_session_check_interval);
    load_opt(node, "idle-session-timeout", p.idle_session_timeout);
    load_opt(node, "min-idle-session", p.min_idle_session);
    load_opt(node, "encryption", p.encryption);
    load_opt(node, "psk", p.psk);
    load_opt(node, "version", p.version);
    load_opt(node, "obfs-opts", p.obfs_opts);
}

void from_node(const fkyaml::node& node, Clash& c) {
    load_opt(node, "proxies", c.proxies);
}

} // namespace clash
