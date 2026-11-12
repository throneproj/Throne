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

// A string is base64, as mihomo decodes it into []uint8; `text` keeps a value that decodes to no byte list.
struct WgReserved {
    std::vector<int> value;
    std::string text;
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

// The optional values, when unset, take the proxy's own, as mihomo does.
struct xhttpDownloadSettings {
    std::optional<std::string> host;
    std::optional<std::string> path;
    std::optional<std::map<std::string, std::string>> headers;
    std::string mode;
    std::optional<std::string> server;
    std::optional<MyInt> port;
    std::optional<MyBool> tls;
    std::optional<std::vector<std::string>> alpn;
    std::optional<std::string> servername;
    std::optional<std::string> client_fingerprint;
    // The certificate pin.
    std::optional<std::string> fingerprint;
    std::optional<realityOpts> reality_opts;
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
    std::map<std::string, std::string> headers;
    MyBool no_grpc_header;
    std::string x_padding_bytes;
    MyBool x_padding_obfs_mode;
    std::string x_padding_key;
    std::string x_padding_header;
    std::string x_padding_placement;
    std::string x_padding_method;
    std::string uplink_http_method;
    std::string session_placement;
    std::string session_key;
    std::string session_table;
    std::string session_length;
    std::string seq_placement;
    std::string seq_key;
    std::string uplink_data_placement;
    std::string uplink_data_key;
    std::string uplink_chunk_size;
    std::string sc_max_each_post_bytes;
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

// Numbers are kept as text so the WireGuard parser can validate them like its link counterparts.
struct wgAmneziaOpts {
    std::string jc;
    std::string jmin;
    std::string jmax;
    std::string s1;
    std::string s2;
    std::string s3;
    std::string s4;
    std::string h1;
    std::string h2;
    std::string h3;
    std::string h4;
    std::string i1;
    std::string i2;
    std::string i3;
    std::string i4;
    std::string i5;
    std::string header_protection_key;
    std::string content_padding_addition;
    std::string rekey_after_time;
    std::string rekey_timeout;
    std::string reject_after_time;
    std::string keepalive_timeout;
    std::string max_handshake_attempts;
    MyBool random_trailers;
    MyBool disable_cookies;
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
    // Decoded by `plugin` as mihomo's ss adapter does: all fields for v2ray-plugin, mode and host for obfs.
    v2rayPlugin plugin_opts;
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
    MyInt workers;
    std::string persistent_keepalive;
    std::string keepalive;
    std::optional<wgAmneziaOpts> amnezia_wg_option;
    // GFW-knocker Xray's WireGuard noise, which converters copy into Clash entries.
    std::string wnoise;
    std::string wnoisecount;
    std::string wpayloadsize;
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
    std::string psk;
    MyInt version;
    // Qualified: the plain `obfs` member above shadows the struct name here.
    clash::obfs obfs_opts;
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
void from_node(const fkyaml::node& node, wgAmneziaOpts& opts);
void from_node(const fkyaml::node& node, Proxies& p);
void from_node(const fkyaml::node& node, Clash& c);

} // namespace clash
