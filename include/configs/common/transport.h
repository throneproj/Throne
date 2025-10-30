#pragma once
#include "include/configs/baseConfig.h"

namespace Configs
{
    class Transport : public baseConfig
    {
        public:
        QString type;

        // HTTP
        QString host;
        QString path;
        QString method;
        QStringList headers;
        QString idle_timeout;
        QString ping_timeout;

        // Websocket
        int max_early_data = 0;
        QString early_data_header_name;

        // gRPC
        QString service_name;

        Transport()
        {
            _add(new configItem("type", &type, string));
            _add(new configItem("host", &host, string));
            _add(new configItem("path", &path, string));
            _add(new configItem("method", &method, string));
            _add(new configItem("headers", &headers, stringList));
            _add(new configItem("idle_timeout", &idle_timeout, string));
            _add(new configItem("ping_timeout", &ping_timeout, string));
            _add(new configItem("max_early_data", &max_early_data, integer));
            _add(new configItem("early_data_header_name", &early_data_header_name, string));
            _add(new configItem("service_name", &service_name, string));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
