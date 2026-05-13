#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include "3rdparty/fkYAML/node.hpp"

namespace clash {

struct MyBool {
    bool value = false;
    operator bool() const { return value; }
};

struct MyInt {
    int value = 0;
    operator int() const { return value; }
};

struct WgReserved {
    std::vector<uint8_t> value;
};

struct smuxOpts {
    MyBool enabled;
    MyInt max_connections;
    MyInt max_streams;
    MyInt min_streams;
    MyBool padding;
    std::string protocol;
};

struct grpcOpts {
    std::string grpc_service_name;
};

struct httpOpts {
    std::map<std::string, std::vector<std::string>> headers;
    std::string method;
    std::vector<std::string> path;
};

struct h2Opts {
    std::vector<std::string> host;
    std::string path;
};

struct wsOpts {
    std::string early_data_header_name;
    std::map<std::string, std::string> headers;
    MyInt max_early_data;
    std::string path;
    MyBool v2ray_http_upgrade;
};

struct realityOpts {
    std::string public_key;
    std::string short_id;
};

struct xhttpReuseSettings {
    std::string max_concurrency;
    std::string max_connections;
    std::string c_max_reuse_times;
    std::string h_max_request_times;
    std::string h_max_reusable_secs;
    std::string h_keep_alive_period;
};

struct xhttpDownloadSettings {
    std::string host;
    std::string path;
    std::string mode;
    std::string server;
    MyInt port;
    MyBool tls;
    std::vector<std::string> alpn;
    std::string servername;
    std::string client_fingerprint;
    realityOpts reality_opts;
    MyBool x_padding_obfs_mode;
    std::string x_padding_key;
    std::string x_padding_header;
    std::string x_padding_placement;
    std::string x_padding_method;
    xhttpReuseSettings reuse_settings;
};

struct xhttpOpts {
    std::string host;
    std::string path;
    std::string mode;
    MyBool x_padding_obfs_mode;
    std::string x_padding_key;
    std::string x_padding_header;
    std::string x_padding_placement;
    std::string x_padding_method;
    std::string sc_min_posts_interval_ms;
    xhttpReuseSettings reuse_settings;
    bool has_download_settings = false;
    xhttpDownloadSettings download_settings;
};

struct obfs {
    std::string mode;
    std::string host;
};

struct v2rayPlugin {
    std::string mode;
    MyBool tls;
    std::string host;
    std::string path;
    MyBool mux;
};

struct wgPeer {
    std::string server;
    MyInt port;
    std::string ip;
    std::string ipv6;
    std::string public_key;
    std::string pre_shared_key;
    std::optional<WgReserved> reserved;
    std::vector<std::string> allowed_ips;
};

struct Proxies {
    std::string name;
    std::string type;
    std::string server;
    MyInt port;
    std::string cipher;
    std::string uuid;
    MyInt alterId;
    MyBool udp;
    MyBool tls;
    MyBool skip_cert_verify;
    std::string servername;
    std::string network;
    wsOpts ws_opts;
    std::map<std::string, std::string> ws_headers;
    xhttpOpts xhttp_opts;
    h2Opts h2_opts;
    httpOpts http_opts;
    grpcOpts grpc_opts;
    std::string username;
    std::string password;
    std::string sni;
    std::vector<std::string> alpn;
    std::string plugin;
    fkyaml::node plugin_opts;
    std::string fingerprint;
    std::string obfs;
    std::string protocol;
    std::string obfs_param;
    std::string protocol_param;
    std::string client_fingerprint;
    std::string flow;
    std::string packet_encoding;
    realityOpts reality_opts;
    std::string auth_str;
    std::string auth_str1;
    MyBool disable_mtu_discovery;
    std::string down;
    MyBool fast_open;
    MyInt recv_window;
    MyInt recv_window_conn;
    MyInt recv_window1;
    MyInt recv_window_conn1;
    std::string up;
    std::string ports;
    smuxOpts smux;
    MyBool udp_over_tcp;
    MyInt udp_over_tcp_version;
    std::string ip;
    std::string ipv6;
    std::string public_key;
    std::string pre_shared_key;
    std::string private_key;
    std::string private_key_passphrase;
    std::vector<std::string> host_key;
    std::vector<std::string> host_key_algorithms;
    std::optional<WgReserved> reserved;
    std::string dialer_proxy;
    std::vector<wgPeer> peers;
    MyInt mtu;
    MyBool disable_sni;
    std::string congestion_controller;
    std::string udp_relay_mode;
    MyBool reduce_rtt;
    MyInt heartbeat_interval;
    std::string obfs_password;
    MyBool tfo;
    MyBool mptcp;
    MyInt idle_session_check_interval;
    MyInt idle_session_timeout;
    MyInt min_idle_session;
    std::string encryption;
};

struct Clash {
    std::vector<Proxies> proxies;
};

void from_node(const fkyaml::node& node, MyBool& b);
void from_node(const fkyaml::node& node, MyInt& i);
void from_node(const fkyaml::node& node, WgReserved& w);
void from_node(const fkyaml::node& node, smuxOpts& opts);
void from_node(const fkyaml::node& node, grpcOpts& opts);
void from_node(const fkyaml::node& node, xhttpReuseSettings& opts);
void from_node(const fkyaml::node& node, xhttpDownloadSettings& opts);
void from_node(const fkyaml::node& node, xhttpOpts& opts);
void from_node(const fkyaml::node& node, httpOpts& opts);
void from_node(const fkyaml::node& node, h2Opts& opts);
void from_node(const fkyaml::node& node, wsOpts& opts);
void from_node(const fkyaml::node& node, realityOpts& opts);
void from_node(const fkyaml::node& node, obfs& opts);
void from_node(const fkyaml::node& node, v2rayPlugin& opts);
void from_node(const fkyaml::node& node, wgPeer& peer);
void from_node(const fkyaml::node& node, Proxies& p);
void from_node(const fkyaml::node& node, Clash& c);

} // namespace clash
