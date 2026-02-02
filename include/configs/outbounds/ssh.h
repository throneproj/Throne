#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class ssh : public outbound
    {
        public:
        QString user;
        QString password;
        QString private_key;
        QString private_key_path;
        QString private_key_passphrase;
        QStringList host_key;
        QStringList host_key_algorithms;
        QString client_version;

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
    };
}
