#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    inline const QStringList openvpnNetworks = {"", "udp", "udp4", "udp6", "tcp", "tcp4", "tcp6"};

    class OpenVPNRemote : public baseConfig
    {
        public:
        QString server;
        int server_port = 0;
        QString network;

        bool ParseFromJson(const QJsonObject& object) override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class OpenVPNPullFilter : public baseConfig
    {
        public:
        QString action;
        QString text;

        bool ParseFromJson(const QJsonObject& object) override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    // tls-auth / tls-crypt / tls-crypt-v2 control channel wrapping.
    class OpenVPNControlWrap : public baseConfig
    {
        public:
        QString type;
        QStringList key;
        QString direction;

        // Conflicts with `key`.
        QString key_path;

        bool ParseFromJson(const QJsonObject& object) override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class OpenVPNTLS : public baseConfig
    {
        public:
        QStringList certificate;
        QStringList client_certificate;
        QStringList client_key;
        std::shared_ptr<OpenVPNControlWrap> control_wrap = std::make_shared<OpenVPNControlWrap>();

        // Each *_path conflicts with its inline value.
        QString server_name;
        QString server_name_type;
        QString certificate_path;
        QString client_certificate_path;
        QString client_key_path;
        QStringList peer_fingerprint;
        QString crl_path;
        QStringList remote_certificate_ku;
        QString remote_certificate_eku;
        QString remote_certificate_tls;
        QString certificate_profile;
        QString ns_certificate_type;
        QString version_min;
        QString version_max;
        QString cipher;
        QString groups;

        bool ParseFromJson(const QJsonObject& object) override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class openvpn : public outbound
    {
        public:
        QString network;
        QString username;
        QString password;
        QString static_challenge;
        bool static_challenge_echo = false;
        int mtu = 0;
        std::shared_ptr<OpenVPNTLS> tls = std::make_shared<OpenVPNTLS>();
        // Throne-only, never sent to the core.
        int otp_profile_id = -1;
        bool only_advertised_routes = true;
        QString tunnel_dns = kTunnelDnsPrefer;

        QString mode;
        QList<std::shared_ptr<OpenVPNRemote>> servers;
        bool remote_random = false;
        QStringList address;
        QString peer_address;
        QString peer_address_ipv6;
        QString topology;
        QString auth_retry;
        QStringList static_key;
        QString static_key_path;
        QString key_direction;
        QString cipher;
        QStringList data_ciphers;
        QString data_ciphers_fallback;
        QString auth;
        int mss_fix = 0;
        bool mss_fix_disabled = false;
        QString mss_fix_mode;
        int fragment = 0;
        int replay_window = 0;
        QString replay_window_time;
        QString compression;
        QString compression_lzo;
        QString allow_compression;
        bool route_no_pull = false;
        QList<std::shared_ptr<OpenVPNPullFilter>> pull_filters;
        QStringList routes;
        QString route_gateway;
        int route_metric = 0;
        bool redirect_gateway = false;
        QStringList redirect_gateway_flags;
        bool redirect_private = false;
        bool block_ipv6 = false;
        QString ping_interval;
        QString ping_restart;
        bool ping_restart_disabled = false;
        QString renegotiate_interval;
        bool renegotiate_disabled = false;
        qint64 renegotiate_bytes = 0;
        qint64 renegotiate_packets = 0;
        QString tls_timeout;
        QString handshake_window;
        int explicit_exit_notify = 0;
        bool system = false;
        // sing-box "name" (system interface name); outbound::name is the tag.
        QString interface_name;
        QString udp_timeout;
        QString udp_mapping;
        QString udp_filtering;
        int udp_nat_max = 0;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
        bool IsEndpoint() override;
        bool SupportsCredentialStrip() const override;
        void StripCredentials() override;
        void StripForeignState(Validation::Issues& issues) override;
        void Validate(Validation::Issues& issues) override;

        // A non-empty Advanced server list replaces the profile's own server and port.
        bool ServerPortOptional() override { return !servers.isEmpty(); }
        bool ServerAddressOptional() override { return !servers.isEmpty(); }

        // A copy the editors can fill with unsaved input: the option objects they write are not shared with this one.
        std::shared_ptr<openvpn> DetachedCopy() const;
    };

    namespace Checks {
        // Names are checked upper-cased, as the editors save them and Build() emits them.
        QString OpenVPNCipherName(const QString& name);
        QString OpenVPNDigest(const QString& name);
        // Colons and spaces removed, lower-cased.
        QString OpenVPNNormalizeFingerprint(const QString& line);
        QString OpenVPNFingerprint(const QString& line);
        QString OpenVPNKeyUsage(const QString& item);
        QString OpenVPNExtendedKeyUsage(const QString& text);
        // OpenVPN's TLS-ECDHE-RSA-WITH-... spelling becomes the TLS_ECDHE_RSA_WITH_... name the core knows.
        QString OpenVPNNormalizeTlsCipher(const QString& text);
        QString OpenVPNTlsCipher(const QString& text);
        QString OpenVPNTlsGroups(const QString& text);
        // Renegotiation, TLS timeout and handshake window: 0 or at least 1s.
        QString OpenVPNInterval(const QString& text);
        // Ping interval and restart: the same, in whole seconds.
        QString OpenVPNPingInterval(const QString& text);
        QString OpenVPNReplayWindowTime(const QString& text);
        QString OpenVPNFragment(const QString& text);
        QString OpenVPNReplayWindow(const QString& text);
        QString OpenVPNRemoteHost(const QString& text);
        QString OpenVPNRemotePort(const QString& text);

        // JSON keys of the options the profile's mode refuses (static_key: TLS and authentication; tls: the static key).
        QStringList OpenVPNModeConflicts(const openvpn& profile);
        // The message for one key of OpenVPNModeConflicts(), or "" when that key is fine.
        QString OpenVPNModeConflict(const openvpn& profile, const QString& key);
        QString OpenVPNStaticKeyMissing(const openvpn& profile);
        QString OpenVPNStaticCipher(const openvpn& profile);
        QString OpenVPNStaticAddress(const openvpn& profile);
        QString OpenVPNTlsVerification(const openvpn& profile);
        QString OpenVPNClientPair(const openvpn& profile);
        QString VpnMaterialConflict(const QStringList& inlineValue, const QString& path);
        QString OpenVPNControlWrapKey(const openvpn& profile);
        QString OpenVPNControlWrapDirection(const openvpn& profile);
        QString OpenVPNRemoteCertificate(const openvpn& profile);
        QString OpenVPNTlsVersions(const openvpn& profile);
        QString OpenVPNMssFix(const openvpn& profile);
        QString OpenVPNPingRestart(const openvpn& profile);
        QString OpenVPNRenegotiate(const openvpn& profile);
        // The {otp} placeholder is substituted only while an authenticator entry is bound.
        QString OtpPlaceholderUnbound(const QString& value, int otpProfileId);
    }
}
