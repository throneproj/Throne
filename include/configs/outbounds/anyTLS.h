#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
    class anyTLS : public outbound
    {
        public:
        QString password;
        QString idle_session_check_interval = "30s";
        QString idle_session_timeout = "30s";
        int min_idle_session = 5;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();

        anyTLS() : outbound()
        {
            _add(new configItem("password", &password, string));
            _add(new configItem("idle_session_check_interval", &idle_session_check_interval, string));
            _add(new configItem("idle_session_timeout", &idle_session_timeout, string));
            _add(new configItem("min_idle_session", &min_idle_session, integer));
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
