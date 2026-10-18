#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
    inline QStringList naiveCongestionControls = {"bbr", "bbr2", "cubic", "reno"};

    namespace Checks {
        // Name="value" pairs as the editor writes them; parseHeaderPairs() drops every header when one is unpaired.
        QString NaiveExtraHeaders(const QString &text);
        QString NaiveExtraHeaderNames(const QStringList &pairs);
    }

    class naive : public outbound
    {
        public:
        QString username;
        QString password;
        QString congestion_control;
        QStringList extra_headers;
        int insecure_concurrency = 0;
        bool quic = false;
        bool uot = false;
        std::shared_ptr<TLS> tls = std::make_shared<TLS>();

        naive() {
            tls->enabled = true;
            tls->utls->supported = false;
        }

        bool HasTLS() override {
            return true;
        }

        bool MustTLS() override {
            return true;
        }

        bool LimitedTLS() override {
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
}
