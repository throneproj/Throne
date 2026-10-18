#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
    namespace Checks {
        // The core reads version 0 as 1; v2 and v3 authenticate with the server's password.
        QString ShadowTlsPassword(int version, const QString &password);
    }

    class shadowtls : public outbound
    {
        public:
        int version = 1;
        QString password;
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

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;

        QString DisplayType() override;

        void Validate(Validation::Issues &issues) override;
    };
}
