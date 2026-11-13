#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
    class juicity : public outbound
    {
        public:
        QString uuid;
        QString password;
        QString pinned_certchain_sha256;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();

        juicity()
        {
            tls->utls->supported = false;
        }

        bool HasTLS() override {
            return true;
        }

        bool MustTLS() override {
            return true;
        }

        std::shared_ptr<TLS> GetTLS() override {
            return tls;
        }

        void Validate(Validation::Issues &issues) override;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
    };

    namespace Checks {
        // Decoded like juicity: padded URL base64, else padded standard base64, else hex; the result must be 32 bytes.
        QString JuicityPinnedCertChain(const QString& text);
    }
}
