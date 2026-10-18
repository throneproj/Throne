#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{

    inline QStringList ccAlgorithms = {"cubic", "new_reno", "bbr"};
    inline QStringList udpRelayModes = {"", "native", "quic"};

    class tuic : public outbound
    {
        public:
        QString uuid;
        QString password;
        QString congestion_control;
        QString udp_relay_mode;
        bool udp_over_stream = false;
        bool zero_rtt_handshake = false;
        QString heartbeat;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();
        std::shared_ptr<QUICFields> quic = std::make_shared<QUICFields>();

        tuic()
        {
            tls->utls->supported = false;
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

        void Validate(Validation::Issues &issues) override;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
    };
}
