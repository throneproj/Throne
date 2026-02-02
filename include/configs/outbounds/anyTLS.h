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

        bool HasTLS() override {
            return true;
        }

        bool MustTLS() override {
            return true;
        }

        std::shared_ptr<TLS> GetTLS() override {
            return tls;
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
