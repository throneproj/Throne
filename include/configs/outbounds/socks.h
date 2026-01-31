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

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
    };
}
