#pragma once
#include "include/configs/common/multiplex.h"
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"
#include "include/configs/common/transport.h"

namespace Configs
{

    inline QStringList vmessSecurity = {"auto", "none", "zero", "aes-128-gcm", "chacha20-poly1305"};
    inline QStringList vPacketEncoding = {"", "packetaddr", "xudp"};

    class vmess : public outbound
    {
        public:
        QString uuid;
        QString security = "auto";
        int alter_id = 0;
        bool global_padding = false;
        bool authenticated_length = false;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();
        QString packet_encoding = "xudp";
        std::shared_ptr<Transport> transport = std::make_shared<Transport>();
        std::shared_ptr<Multiplex> multiplex = std::make_shared<Multiplex>();

        bool HasTLS() override {
            return true;
        }

        bool HasMux() override {
            return true;
        }

        bool HasTransport() override {
            return true;
        }

        std::shared_ptr<TLS> GetTLS() override {
            return tls;
        }

        std::shared_ptr<Multiplex> GetMux() override {
            return multiplex;
        }

        std::shared_ptr<Transport> GetTransport() override {
            return transport;
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
