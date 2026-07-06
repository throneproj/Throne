#pragma once
#include "include/configs/baseConfig.h"

namespace Configs {
    inline QStringList XrayNetworks = {"raw", "xhttp", "ws", "httpupgrade", "grpc"};
    inline QStringList XrayXHTTPModes = {"auto", "packet-up", "stream-up", "stream-one"};
    inline QStringList XrayXHTTPMetaPlacements = {"", "path", "cookie", "header", "query"};
    inline QStringList XrayXHTTPUplinkDataPlacements = {"", "auto", "body", "cookie", "header"};
    inline QStringList XrayXHTTPUplinkMethods = {"", "POST", "PUT", "PATCH", "GET"};

    class xrayTLS : public baseConfig {
        public:
        QString serverName;
        QString pinnedPeerCertSha256;
        QString verifyPeerCertByName;
        QStringList alpn;
        QString fingerprint;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
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
        bool ParseFromClash(const clash::Proxies& object) override;
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
        QJsonObject rawExtra;
        QStringList headers;
        QString xPaddingBytes;
        bool xPaddingObfsMode = false;
        QString xPaddingKey;
        QString xPaddingHeader;
        QString xPaddingPlacement;
        QString xPaddingMethod;
        QString uplinkHTTPMethod;
        QString sessionIDPlacement;
        QString sessionIDKey;
        QString sessionIDTable;
        QString sessionIDLength;
        QString seqPlacement;
        QString seqKey;
        QString uplinkDataPlacement;
        QString uplinkDataKey;
        QString uplinkChunkSize;
        bool noGRPCHeader = false;
        bool noSSEHeader = false;
        QString scMaxEachPostBytes;
        QString scMinPostsIntervalMs;
        long long scMaxBufferedPosts;
        QString scStreamUpServerSecs;
        int serverMaxHeaderBytes;
        // extra/xmux
        QJsonObject rawXmux;
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
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class xrayWS : public baseConfig {
    public:
        QString path;
        QString host;
        int ed = 0;
        QStringList headers;
        int heartbeatPeriod = 0;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class xrayHttpUpgrade : public baseConfig {
    public:
        QString path;
        int ed = 0;
        QString host;
        QStringList headers;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class xrayGRPC : public baseConfig {
    public:
        QString authority;
        QString serviceName;
        bool multiMode = false;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
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
        std::shared_ptr<xrayWS> ws = std::make_shared<xrayWS>();
        std::shared_ptr<xrayHttpUpgrade> httpupgrade = std::make_shared<xrayHttpUpgrade>();
        std::shared_ptr<xrayGRPC> grpc = std::make_shared<xrayGRPC>();

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
