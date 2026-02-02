#pragma once
#include "include/configs/baseConfig.h"

namespace Configs
{

    inline QStringList tlsFingerprints = {"", "chrome", "firefox", "edge", "safari", "360", "qq", "ios", "android", "random", "randomized"};

    class uTLS : public baseConfig
    {
        public:
        bool supported = true;
        bool enabled = false;
        QString fingerPrint;

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class ECH : public baseConfig
    {
        public:
        bool enabled = false;
        QStringList config;
        QString config_path;

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class Reality : public baseConfig
    {
        public:
        bool enabled = false;
        QString public_key;
        QString short_id;

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class TLS : public baseConfig
    {
        public:
        bool enabled = false;
        bool disable_sni = false;
        QString server_name;
        bool insecure = false;
        QStringList alpn;
        QString min_version;
        QString max_version;
        QStringList cipher_suites;
        QStringList curve_preferences;
        QStringList certificate;
        QString certificate_path;
        QStringList certificate_public_key_sha256;
        QStringList client_certificate;
        QString client_certificate_path;
        QStringList client_key;
        QString client_key_path;
        bool fragment = false;
        QString fragment_fallback_delay;
        bool record_fragment = false;
        std::shared_ptr<ECH> ech = std::make_shared<ECH>();
        std::shared_ptr<uTLS> utls = std::make_shared<uTLS>();
        std::shared_ptr<Reality> reality = std::make_shared<Reality>();

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
