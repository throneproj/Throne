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

        xrayTLS() {
            _add(new configItem("serverName", &serverName, string));
            _add(new configItem("allowInsecure", &allowInsecure, boolean));
            _add(new configItem("alpn", &alpn, stringList));
            _add(new configItem("fingerprint", &fingerprint, string));
        }

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

        xrayReality() {
            _add(new configItem("serverName", &serverName, string));
            _add(new configItem("fingerprint", &fingerprint, string));
            _add(new configItem("serverName", &serverName, string));
            _add(new configItem("password", &password, string));
            _add(new configItem("shortId", &shortId, string));
            _add(new configItem("spiderX", &spiderX, string));
        }

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

        xrayXHTTP() {
            _add(new configItem("host", &host, string));
            _add(new configItem("path", &path, string));
            _add(new configItem("mode", &mode, string));
            _add(new configItem("headers", &headers, stringList));
            _add(new configItem("xPaddingBytes", &xPaddingBytes, string));
            _add(new configItem("noGRPCHeader", &noGRPCHeader, boolean));
            _add(new configItem("scMaxEachPostBytes", &scMaxEachPostBytes, string));
            _add(new configItem("scMinPostsIntervalMs", &scMinPostsIntervalMs, string));
            _add(new configItem("maxConcurrency", &maxConcurrency, string));
            _add(new configItem("maxConnections", &maxConnections, string));
            _add(new configItem("cMaxReuseTimes", &cMaxReuseTimes, string));
            _add(new configItem("hMaxRequestTimes", &hMaxRequestTimes, string));
            _add(new configItem("hMaxReusableSecs", &hMaxReusableSecs, string));
            _add(new configItem("hKeepAlivePeriod", &hKeepAlivePeriod, integer64));
            _add(new configItem("downloadSettings", &downloadSettings, string));
        }

        QString getHeadersString();

        QStringList getHeaderPairs(QString rawHeader);

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

        xrayStreamSetting() {
            _add(new configItem("network", &network, string));
            _add(new configItem("security", &security, string));
            _add(new configItem("tls", dynamic_cast<JsonStore *>(TLS.get()), jsonStore));
            _add(new configItem("reality", dynamic_cast<JsonStore *>(reality.get()), jsonStore));
            _add(new configItem("xhttp", dynamic_cast<JsonStore *>(xhttp.get()), jsonStore));
        }

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
