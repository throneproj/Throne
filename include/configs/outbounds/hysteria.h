#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
    inline QStringList hysteriaBBRProfiles = {"standard", "conservative", "aggressive"};
    inline QStringList hysteriaObfsTypes = {"salamander", "gecko"};

    // Shared by EditHysteria (widget text) and hysteria::Validate (bean fields).
    namespace Checks {
        // One server_ports item as Throne stores it: n, a-b or a:b, open ends allowed.
        QString HysteriaPortItem(const QString &item);
        QString HysteriaPortItemOrder(const QString &item);
        QString HysteriaHopMinimum(bool hopping, const QString &interval);
        QString HysteriaHopMaximum(bool hopping, const QString &interval, const QString &intervalMax);
        QString HysteriaHopMaximumIgnored(bool hopping, const QString &interval, const QString &intervalMax);
        QString HysteriaV1Speed(qint64 upMbps, qint64 downMbps);
        // 0 means the core default (512 / 1200).
        QString HysteriaGeckoSizes(qint64 minSize, qint64 maxSize);
        QString HysteriaAuth(const QString &type, const QString &auth);
        QString HysteriaAuthType(const QString &type, const QString &auth);
        QString HysteriaRealm(const QString &url, const QString &id, const QStringList &stunServers, bool portMapping,
                              int ipVersion, const QString &timeout, const QString &lifetime);
    }

    class hysteria : public outbound
    {
        public:
        QString protocol_version = "1";
        QStringList server_ports;
        QString hop_interval;
        int up_mbps = 0;
        int down_mbps = 0;
        QString obfs;

        // Hysteria1
        QString auth_type;
        QString auth;
        int recv_window_conn = 0;
        int recv_window = 0;
        bool disable_mtu_discovery = false;

        // Hysteria2
        QString password;
        int min_packet_size = 0;
        int max_packet_size = 0;
        QString obfs_type = "salamander";
        QString hop_interval_max;
        QString bbr_profile;
        // The core parrots Chrome's QUIC handshake unless this is set.
        bool disable_chrome_parrot = false;

        // Hysteria2 realm (NAT traversal rendezvous); replaces server/server_port/server_ports.
        bool realm_enabled = false;
        QString realm_server_url;
        QString realm_token;
        QString realm_id;
        QStringList realm_stun_servers;
        int realm_ip_version = 0;
        bool realm_port_mapping = false;
        QString realm_port_mapping_timeout;
        QString realm_port_mapping_lifetime;
        // Carried over from imported configs; the editor has no form for it.
        QJsonObject realm_http_client;

        std::shared_ptr<TLS> tls = std::make_shared<TLS>();
        std::shared_ptr<QUICFields> quic = std::make_shared<QUICFields>();

        hysteria()
        {
            tls->utls->supported = false;
        }

        bool RealmActive() const {
            return realm_enabled && protocol_version == "2";
        }

        bool HasTLS() override {
            return true;
        }

        bool MustTLS() override {
            return true;
        }

        bool HasQUIC() override {
            return true;
        }

        std::shared_ptr<TLS> GetTLS() override {
            return tls;
        }

        std::shared_ptr<QUICFields> GetQUIC() override {
            return quic;
        }

        bool ServerPortOptional() override {
            return !server_ports.isEmpty() || RealmActive();
        }

        bool ServerAddressOptional() override {
            return RealmActive();
        }

        void Validate(Validation::Issues &issues) override;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;

        QString DisplayAddress() override;
        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
    };
}
