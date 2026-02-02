#pragma once
#include "include/configs/baseConfig.h"

namespace Configs {
    inline QStringList XrayNetworks = {"raw", "xhttp"};
    inline QStringList XrayXHTTPModes = {"auto", "packet-up", "stream-up", "stream-one"};

    class xrayTLS : public baseConfig {
        public:
        QString serverName;
        bool allowInsecure = false;
        QStringList alpn;
        QString fingerprint;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class xrayReality : public baseConfig {
        public:
        QString serverName;
        QString fingerprint;
        QString password;
        QString shortId;
        QString spiderX;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class xrayXHTTP : public baseConfig {
        public:
        QString host;
        QString path;
        QString mode = "auto";
        // extra
        QStringList headers;
        QString xPaddingBytes;
        bool noGRPCHeader = false;
        QString scMaxEachPostBytes; // packet-up only
        QString scMinPostsIntervalMs; // packet-up only
        // extra/xmux
        QString maxConcurrency;
        QString maxConnections;
        QString cMaxReuseTimes;
        QString hMaxRequestTimes;
        QString hMaxReusableSecs;
        long long hKeepAlivePeriod = 0;
        // extra/downloadSettings
        QString downloadSettings;

        bool ParseExtraJson(QString str);
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class xrayStreamSetting : public baseConfig {
        public:
        QString network = "raw";
        QString security = "none";
        std::shared_ptr<xrayTLS> TLS = std::make_shared<xrayTLS>();
        std::shared_ptr<xrayReality> reality = std::make_shared<xrayReality>();
        std::shared_ptr<xrayXHTTP> xhttp = std::make_shared<xrayXHTTP>();

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
