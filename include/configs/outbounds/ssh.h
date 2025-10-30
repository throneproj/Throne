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

        ssh() : outbound()
        {
            _add(new configItem("user", &user, string));
            _add(new configItem("password", &password, string));
            _add(new configItem("private_key", &private_key, string));
            _add(new configItem("private_key_path", &private_key_path, string));
            _add(new configItem("private_key_passphrase", &private_key_passphrase, string));
            _add(new configItem("host_key", &host_key, stringList));
            _add(new configItem("host_key_algorithms", &host_key_algorithms, stringList));
            _add(new configItem("client_version", &client_version, string));
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
