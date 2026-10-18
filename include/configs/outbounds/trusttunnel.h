#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/TLS.h"

namespace Configs
{
    class trusttunnel : public outbound
    {
        public:
        QString username;
        QString password;
        QString congestion_control;
        QString custom_sni;
        QString client_random;
        bool health_check = false;
        bool quic = false;
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

        bool ParseFromDeepLink(const QString& payload);
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
        void Validate(Validation::Issues& issues) override;
    };

    // sing-trusttunnel quic.go: cubic and reno are distinct, every other name (and an unknown one) is BBR.
    inline const QStringList trustTunnelCongestionControls = {"", "bbr", "cubic", "reno", "bbr_standard", "bbr2", "bbr_variant"};

    namespace Checks {
        // hex prefix[/mask], 1 to 32 bytes, the mask as long as the prefix.
        QString TrustTunnelClientRandom(const QString& text);
        QString TrustTunnelCredentials(const QString& username, const QString& password);
        // client_random and custom_sni against the TLS settings; a QUIC build drops uTLS and REALITY.
        QString TrustTunnelTlsConflict(bool clientRandom, bool customSni, bool quic, bool ech, bool reality, bool disableSni,
                                       const QString& utlsFingerprint);
    }
}
