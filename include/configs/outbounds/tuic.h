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

        tuic() : outbound()
        {
            _add(new configItem("uuid", &uuid, string));
            _add(new configItem("password", &password, string));
            _add(new configItem("congestion_control", &congestion_control, string));
            _add(new configItem("udp_relay_mode", &udp_relay_mode, string));
            _add(new configItem("udp_over_stream", &udp_over_stream, boolean));
            _add(new configItem("zero_rtt_handshake", &zero_rtt_handshake, boolean));
            _add(new configItem("heartbeat", &heartbeat, string));
            _add(new configItem("tls", dynamic_cast<JsonStore *>(tls.get()), jsonStore));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        // outboundMeta overrides
        QString DisplayAddress() override;
        QString DisplayName() override;
        QString DisplayType() override;
        QString DisplayTypeAndName() override;
        bool IsEndpoint() override;
    };
}
