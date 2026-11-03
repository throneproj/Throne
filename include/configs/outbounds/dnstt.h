#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/ResolverSource.h"

namespace Configs
{
    class dnstt : public outbound
    {
        public:
        QString mode = "dnstt";
        // VayDNS speaking the classic dnstt wire, for a stock dnstt-server.
        bool dnstt_compat = false;
        QString domain;
        QString public_key;
        QString record_type;
        // 0 or empty leaves the core's default for the mode.
        int client_id_size = 0;
        int max_qname_len = 0;
        double rps = 0;
        QString idle_timeout;
        QString keepalive_interval;
        ResolverSource resolvers;
        QString resolver_strategy;
        // Sets tcp_fallback on every udp resolver.
        bool tcp_fallback = false;

        // "" leaves the server's own stream to the next profile of a chain.
        QString backend;
        QString socks_username;
        QString socks_password;
        bool socks_udp_over_tcp = false;
        QString ss_method;
        QString ss_password;
        bool ss_udp_over_tcp = false;
        QString ssh_user;
        QString ssh_password;
        QString ssh_private_key;
        QString ssh_private_key_path;
        QString ssh_private_key_passphrase;
        QStringList ssh_host_key;
        QStringList ssh_host_key_algorithms;
        QString ssh_client_version;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;

        QString DisplayAddress() override;
        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
        void Validate(Validation::Issues& issues) override;

        bool HasServerEndpoint() override { return false; }
    };

    inline const QStringList dnsttModes = {"dnstt", "vaydns"};
    inline const QStringList dnsttRecordTypes = {"", "txt", "null", "cname", "a", "aaaa", "mx", "ns", "srv", "caa"};
    inline const QStringList dnsttResolverStrategies = {"", "round_robin", "failover", "fanout"};
    inline const QStringList dnsttBackends = {"", "socks", "shadowsocks", "ssh"};

    inline QString DnsttDisplayType(const QString &mode) {
        return mode == QLatin1String("vaydns") ? QStringLiteral("VayDNS") : QStringLiteral("DNSTT");
    }

    namespace Checks {
        QString DnsttPublicKey(const QString& text);
        QString DnsttRecordType(const QString& mode, bool compat, const QString& recordType);
        QString DnsttClientIdSize(const QString& mode, bool compat, int size);
        // Payload bytes per query as the core computes them; it refuses fewer than 25.
        int DnsttMtu(const QString& mode, bool compat, const QString& domain, int maxQnameLen, int clientIdSize);
        QString DnsttMtuError(const QString& mode, bool compat, const QString& domain, int maxQnameLen, int clientIdSize);
        QString DnsttRps(double rps);
        // An empty value counts as the mode's default.
        QString DnsttKeepalive(const QString& mode, bool compat, const QString& idleTimeout, const QString& keepalive);
        // A warning, not an error.
        QString DnsttChainOnly(const QString& backend);
    }
}
