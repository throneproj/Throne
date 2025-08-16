// DO NOT INCLUDE THIS

#include "Const.hpp"
#ifdef Q_OS_WIN
#include "include/sys/windows/WinVersion.h"
#endif

namespace Configs {

    class Routing : public JsonStore {
    public:
        int current_route_id = 0;

        // DNS
        QString remote_dns = "tls://8.8.8.8";
        QString remote_dns_strategy = "";
        QString direct_dns = "localhost";
        QString direct_dns_strategy = "";
        bool use_dns_object = false;
        QString dns_object = "";
        QString dns_final_out = "proxy";

        // Misc
        QString domain_strategy = "AsIs";
        QString outbound_domain_strategy = "AsIs";
        int sniffing_mode = SniffingMode::FOR_ROUTING;

        explicit Routing(int preset = 0);

        static QStringList List();
    };

    class DataStore : public JsonStore {
    public:
        // Running

        int core_port = 19810;
        int started_id = -1919;
        bool core_running = false;
        bool prepare_exit = false;
        bool spmode_vpn = false;
        bool spmode_system_proxy = false;
        bool need_keep_vpn_off = false;
        QString appdataDir = "";
        QStringList ignoreConnTag = {};
        QString proxy_scheme = "{ip}:{port}";

        std::unique_ptr<Routing> routing;
        int imported_count = 0;
        bool refreshing_group_list = false;
        bool refreshing_group = false;
        std::atomic<int> resolve_count = 0;

        // Flags
        QStringList argv = {};
        bool flag_use_appdata = false;
        bool flag_many = false;
        bool flag_tray = false;
        bool flag_debug = false;
        bool flag_restart_tun_on = false;
        bool flag_dns_set = false;

        // Saved

        // Misc
        QString log_level = "info";
        QString test_latency_url = "http://cp.cloudflare.com/";
        bool disable_tray = false;
        int test_concurrent = 10;
        bool disable_traffic_stats = false;
        int current_group = 0; // group id
        QString mux_protocol = "smux";
        bool mux_padding = false;
        int mux_concurrency = 8;
        bool mux_default_on = false;
        QString theme = "0";
        int language = 0;
        QString font = "";
        int font_size = 0;
        QString mw_size = "";
        QStringList log_ignore = {};
        bool start_minimal = false;
        int max_log_line = 200;
        QString splitter_state = "";
        bool enable_stats = true;
        int stats_tab = 0; // either connection or log
        int speed_test_mode = TestConfig::FULL;
        QString simple_dl_url = "http://cachefly.cachefly.net/1mb.test";
        bool allow_beta_update = false;

        // Subscription
        QString user_agent = ""; // set at main.cpp
        bool sub_use_proxy = false;
        bool sub_clear = false;
        bool sub_insecure = false;
        int sub_auto_update = -30;

        // Assets
        QString geoip_download_url = "";
        QString geosite_download_url = "";
        int auto_reset_assets_idx = 0;
        long long last_asset_reset_epoch_secs = 0;

        // Security
        bool skip_cert = false;
        QString utlsFingerprint = "";
        bool disable_run_admin = false; // windows only
        bool use_mozilla_certs = false;

        // Remember
        QStringList remember_spmode = {};
        int remember_id = -1919;
        bool remember_enable = false;
        bool windows_set_admin = false;

        // Socks & HTTP Inbound
        QString inbound_address = "127.0.0.1";
        int inbound_socks_port = 2080; // Mixed, actually
        bool random_inbound_port = false;
        QString custom_inbound = "{\"inbounds\": []}";

        // Routing
        QString custom_route_global = "{\"rules\": []}";
        QString active_routing = "Default";

        // VPN
        bool fake_dns = false;
        bool enable_tun_routing = false;
#ifdef Q_OS_MACOS
        QString vpn_implementation = "gvisor";
#elif defined(Q_OS_WIN)
        QString vpn_implementation = WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_10_1507) ? "system" : "gvisor";
#else
        QString vpn_implementation = "system";
#endif
        int vpn_mtu = 1500;
        bool vpn_ipv6 = false;
        bool vpn_strict_route = true;
        bool disable_privilege_req = false;

        // NTP
        bool enable_ntp = false;
        QString ntp_server_address = "";
        int ntp_server_port = 0;
        QString ntp_interval = "";

        // Hijack
        bool enable_dns_server = false;
        bool dns_server_listen_lan = false;
        int dns_server_listen_port = 53;
        QString dns_v4_resp = "127.0.0.1";
        QString dns_v6_resp = "::1";
        QStringList dns_server_rules = {};
        bool enable_redirect = false;
        QString redirect_listen_address = "127.0.0.1";
        int redirect_listen_port = 443;

        // System dns
        bool system_dns_set = false;

        // Hotkey
        QString hotkey_mainwindow = "";
        QString hotkey_group = "";
        QString hotkey_route = "";
        QString hotkey_system_proxy_menu = "";
        QString hotkey_toggle_system_proxy = "";

        // Core
        int core_box_clash_api = -9090;
        QString core_box_clash_listen_addr = "127.0.0.1";
        QString core_box_clash_api_secret = "";
        QString core_box_underlying_dns = "";

        // Methods

        DataStore();

        void UpdateStartedId(int id);

        [[nodiscard]] QString GetUserAgent(bool isDefault = false) const;
    };

    extern DataStore *dataStore;

} // namespace Configs
