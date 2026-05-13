#include "include/configs/sub/clash.hpp"

namespace clash {

void from_node(const fkyaml::node& node, MyBool& b) {
    if (node.is_boolean()) {
        b.value = node.get_value<bool>();
    } else if (node.is_integer()) {
        b.value = node.get_value<long long>() == 1;
    } else if (node.is_string()) {
        std::string s = node.get_value<std::string>();
        b.value = (s == "true" || s == "1");
    } else {
        b.value = false;
    }
}

void from_node(const fkyaml::node& node, MyInt& i) {
    if (node.is_integer()) {
        i.value = static_cast<int>(node.get_value<long long>());
    } else if (node.is_string()) {
        try {
            i.value = std::stoi(node.get_value<std::string>());
        } catch (...) {
            i.value = 0;
        }
    } else {
        i.value = 0;
    }
}

void from_node(const fkyaml::node& node, WgReserved& w) {
    if (node.is_string()) {
        std::string s = node.get_value<std::string>();
        w.value.assign(s.begin(), s.end());
    } else if (node.is_sequence()) {
        auto seq = node.get_value<std::vector<uint8_t>>();
        w.value = seq;
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
        else if (node[key].is_boolean()) target = node[key].get_value<bool>() ? "true" : "false";
    }
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

void from_node(const fkyaml::node& node, xhttpDownloadSettings& opts) {
    load_opt(node, "host", opts.host);
    load_opt(node, "path", opts.path);
    load_opt(node, "mode", opts.mode);
    load_opt(node, "server", opts.server);
    load_opt(node, "port", opts.port);
    load_opt(node, "tls", opts.tls);
    if (node.is_mapping() && node.contains("alpn")) {
        if (node["alpn"].is_string()) {
            opts.alpn.push_back(node["alpn"].get_value<std::string>());
        } else if (node["alpn"].is_sequence()) {
            opts.alpn = node["alpn"].get_value<std::vector<std::string>>();
        }
    }
    load_opt(node, "servername", opts.servername);
    load_opt(node, "client-fingerprint", opts.client_fingerprint);
    load_opt(node, "reality-opts", opts.reality_opts);
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
    load_opt(node, "x-padding-obfs-mode", opts.x_padding_obfs_mode);
    load_opt(node, "x-padding-key", opts.x_padding_key);
    load_opt(node, "x-padding-header", opts.x_padding_header);
    load_opt(node, "x-padding-placement", opts.x_padding_placement);
    load_opt(node, "x-padding-method", opts.x_padding_method);
    load_opt(node, "sc-min-posts-interval-ms", opts.sc_min_posts_interval_ms);
    load_opt(node, "reuse-settings", opts.reuse_settings);
    if (node.is_mapping() && node.contains("download-settings")) {
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
    load_opt(node, "allowed_ips", peer.allowed_ips);
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
    if (node.is_mapping() && node.contains("plugin-opts")) {
        p.plugin_opts = node["plugin-opts"];
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
}

void from_node(const fkyaml::node& node, Clash& c) {
    load_opt(node, "proxies", c.proxies);
}

} // namespace clash
