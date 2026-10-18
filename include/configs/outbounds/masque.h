#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
    namespace Checks {
        // Base64 of DER; the core decodes it with Go's strict StdEncoding, padding included.
        QString MasqueKey(const QString &text, Validation::Empty empty);
    }

    class masque : public outbound
    {
        public:
        QString private_key;
        QString peer_public_key;
        QStringList address;
        int mtu = 1280;
        // 0 = HTTP/3 falling back to HTTP/2, 2 = HTTP/2, 3 = HTTP/3.
        int http_version = 0;
        bool disable_version_fallback = false;

        std::shared_ptr<TLS> tls = std::make_shared<TLS>();
        std::shared_ptr<QUICFields> quic = std::make_shared<QUICFields>();

        masque()
        {
            server_port = 443;
            tls->enabled = true;
            tls->server_name = "consumer-masque.cloudflareclient.com";
        }

        bool HasTLS() override {
            return true;
        }

        bool MustTLS() override {
            return true;
        }

        bool HasQUIC() override {
            return true;
        }

        std::shared_ptr<TLS> GetTLS() override {
            return tls;
        }

        std::shared_ptr<QUICFields> GetQUIC() override {
            return quic;
        }

        // The core dials 443 when the port is 0.
        bool ServerPortOptional() override {
            return true;
        }

        void Validate(Validation::Issues &issues) override;

        // Bare addresses get /32 or /128; the core needs a prefix.
        static QStringList HostPrefixed(const QStringList &addresses);

        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;

        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
        bool IsEndpoint() override;
    };
}
