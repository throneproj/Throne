#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/xrayMultiplex.h"
#include "include/configs/common/xrayStreamSetting.h"

namespace Configs {
    inline QStringList xrayFlows = {"xtls-rprx-vision", "xtls-rprx-vision-udp443"};

    // Xray loads only "none" or an mlkem768x25519plus key string, which encrypts the payload without transport security.
    inline bool IsVlessEncrypted(const QString& encryption) { return !encryption.isEmpty() && encryption != "none"; }

    namespace Checks {
        // Xray infra/conf/vless.go: "none" or mlkem768x25519plus.<native|xorpub|random>.<1rtt|0rtt>.<parts>, parts of 20+ chars being keys.
        QString XrayVlessEncryption(const QString &encryption);

        // Without VLESS encryption, Vision needs TLS or REALITY directly on the raw network.
        QString XrayVlessVision(const QString &flow, const QString &encryption, const QString &network, const QString &security);

        // Xray runs XDRIVE only for VLESS with encryption, no flow, and Mux on (Keep Default counts: the build turns it on).
        QString XrayXDriveEncryption(const QString &encryption, const QString &network);
        QString XrayXDriveFlow(const QString &flow, const QString &network);
        // muxState as xrayMultiplex::getMuxState(): 0 Keep Default, 1 On, 2 Off, 3 Custom.
        QString XrayXDriveMux(int muxState, const QString &network);
    }

    class xrayVless : public outbound {
        public:
        QString uuid;
        QString encryption = "none";
        QString flow;
        std::shared_ptr<xrayStreamSetting> streamSetting = std::make_shared<xrayStreamSetting>();
        std::shared_ptr<xrayMultiplex> multiplex = std::make_shared<xrayMultiplex>();

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
        BuildResult BuildXray() override;

        std::shared_ptr<xrayStreamSetting> GetXrayStream() override { return streamSetting; }

        std::shared_ptr<xrayMultiplex> GetXrayMultiplex() override { return multiplex; }

        QString DisplayType() override {
            return "VLESS (Xray)";
        }
        SecurityInfo GetSecurity() override;
        void StripForeignState(Validation::Issues &issues) override;
        void Validate(Validation::Issues &issues) override;
        bool IsXray() override {
           return true;
        }

        bool HasXrayStream() override { return true; }
    };
}
