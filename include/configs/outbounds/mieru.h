#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    inline QStringList mieruTransports = {"TCP", "UDP"};
    inline QStringList mieruMultiplexing = {"", "MULTIPLEXING_OFF", "MULTIPLEXING_LOW", "MULTIPLEXING_MIDDLE", "MULTIPLEXING_HIGH"};

    namespace Checks {
        // The core reads each range with Sscanf("%d-%d"), so a single port must be written n-n.
        QString MieruServerPorts(const QString &ports);

        // The core decodes it as padded standard base64.
        QString MieruTrafficPattern(const QString &pattern);
    }

    class mieru : public outbound
    {
        public:
        QString transport = "TCP";
        QString username;
        QString password;
        QString multiplexing;
        QString traffic_pattern;
        // Comma-separated port ranges ("9000-9010,9020-9030"); exported as sing-box's server_ports array.
        QString server_ports;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
        SecurityInfo GetSecurity() override;

        bool ServerPortOptional() override;
        void Validate(Validation::Issues &issues) override;
    };
}
