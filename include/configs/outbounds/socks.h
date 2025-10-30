#pragma once

#include "include/configs/common/Outbound.h"

namespace Configs
{
    class socks : public outbound
    {
        public:
        QString username;
        QString password;
        int version = 5;
        bool uot = false;

        socks() : outbound()
        {
            _add(new configItem("username", &username, string));
            _add(new configItem("password", &password, string));
            _add(new configItem("version", &version, integer));
            _add(new configItem("uot", &uot, boolean));
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
