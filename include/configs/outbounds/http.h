#pragma once

#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
    namespace Checks {
        // The core sends Proxy-Authorization only when a user name is set.
        QString HttpAuth(const QString &username, const QString &password);
    }

    class http : public outbound
    {
        public:
        QString username;
        QString password;
        QString path;
        QStringList headers;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();

        bool HasTLS() override {
            return true;
        }

        std::shared_ptr<TLS> GetTLS() override {
            return tls;
        }

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;

        void Validate(Validation::Issues &issues) override;
    };
}
