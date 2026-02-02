#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
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

        std::shared_ptr<TLS> tls = std::make_shared<TLS>();

        hysteria()
        {
            tls->utls->supported = false;;
        }

        bool HasTLS() override {
            return true;
        }

        bool MustTLS() override {
            return true;
        }

        std::shared_ptr<TLS> GetTLS() override {
            return tls;
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
    };
}
