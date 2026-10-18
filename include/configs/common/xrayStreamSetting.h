#pragma once
#include "include/configs/baseConfig.h"
#include "include/global/Validation.hpp"

namespace Configs {
    inline QStringList XrayNetworks = {"raw", "xhttp", "ws", "httpupgrade", "grpc"};
    inline QStringList XrayXHTTPModes = {"auto", "packet-up", "stream-up", "stream-one"};
    inline QStringList XrayXHTTPMetaPlacements = {"", "path", "cookie", "header", "query"};
    inline QStringList XrayXHTTPUplinkDataPlacements = {"", "auto", "body", "cookie", "header"};
    inline QStringList XrayXHTTPUplinkMethods = {"", "POST", "PUT", "PATCH", "GET"};

    // Folds the aliases Xray itself accepts (tcp, splithttp, websocket), case-insensitively, onto the XrayNetworks names.
    QString NormalizeXrayNetwork(const QString& network);

    // Go's strings.ToLower, for comparing against ASCII names: Qt lowercases U+0130 to two characters, Go to 'i'.
    QString XrayLower(const QString& text);

    // The spellings Go merges into the one XHTTP download leg Xray dials for an outbound (the object holding streamSettings), in decode order.
    QList<QJsonObject> XrayDownloadLeg(const QJsonObject& outbound);

    enum class XrayLegProtection { Verified, Unverified, Plain };
    // How Xray protects a download leg made of these spellings, and the server it dials.
    XrayLegProtection XrayDownloadLegProtection(const QList<QJsonObject>& leg, QString& server);

    // Routes every leg of an Xray outbound through the outbound tagged tag, the XHTTP download leg included.
    void SetXrayDialerProxy(QJsonObject& outbound, const QString& tag);

    // Binds the XHTTP download leg of an Xray outbound to iface unless it names an interface of its own.
    void BindXrayDownloadLeg(QJsonObject& outbound, const QString& iface);

    // Removes dialerProxy and domainStrategy from every sockopt spelling of a stream object; true when any went.
    bool StripXrayDialerState(QJsonObject& stream);

    // Rules the Xray stream editor and the import check share; each returns an empty string when acceptable.
    namespace Checks {
        QString XrayNetwork(const QString& network);
        QString XraySecurity(const QString& security);
        QString XrayRealityNetwork(const QString& security, const QString& network);
        QString XrayFingerprint(const QString& fingerprint, bool reality);
        // Warnings: Xray skips unknown names, and a uTLS hello ignores the list.
        QString XrayCipherSuites(const QString& text);
        QString XrayCipherSuitesNeedUnsafe(const QString& cipherSuites, const QString& fingerprint);
        QString XrayPinnedSha256(const QString& text);
        // A warning: a bad list makes every connection fail. serverName is the effective SNI.
        QString XrayEchConfigList(const QString& text, const QString& serverName);
        QString XraySpiderX(const QString& spiderX);
        QString XrayMldsa65Verify(const QString& key);
        QString XrayHeadersHost(const QStringList& pairs);

        // Xray Int32Range: "n" or "a-b" (negatives allowed, reversed bounds swapped).
        QString XrayInt32Range(const QString& text);
        QString XrayXPaddingBytes(const QString& text);
        QString XrayXHTTPMode(const QString& mode);
        QString XrayPaddingMethod(const QString& method);
        QString XrayPaddingPlacement(const QString& placement);
        QString XrayMetaPlacement(const QString& placement);
        QString XrayUplinkDataPlacement(const QString& placement, const QString& mode);
        QString XrayUplinkMethod(const QString& method, const QString& mode);
        QString XraySessionId(const QString& table, const QString& length);
        QString XrayXmuxExclusive(const QString& maxConnections, const QString& maxConcurrency);
        // Errors for an outbound's XHTTP download leg that would crash the core: no address, no port 1-65535, or another network.
        void XrayXHTTPDownloadLeg(Validation::Issues& issues, const QString& at, const QJsonObject& outbound);
    }

    // dns-direct answers every outbound server domain, so its "disable IPv6" caps this.
    QString getDirectDomainStrategy();

    // Passed to the core as LoadConfigReq.xray_outbound_dns_strategy (ThroneWiring), not baked into the config.
    QString getXrayOutboundDomainStrategy();

    class xrayTLS : public baseConfig {
        public:
        QString serverName;
        QString pinnedPeerCertSha256;
        QString verifyPeerCertByName;
        QStringList alpn;
        QString fingerprint;
        // Colon separated, as Xray takes it.
        QString cipherSuites;
        // Base64 ECHConfigList, or "<query domain>+<DoH url>" for Xray to fetch it itself.
        QString echConfigList;

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
        // The server's ML-DSA-65 public key ("pqv" in links), which signs the REALITY certificate.
        QString mldsa65Verify;

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
        long long scMaxBufferedPosts = 0;
        QString scStreamUpServerSecs;
        int serverMaxHeaderBytes = 0;
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

        bool ParseExtraJson(const QString &str);
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
        QJsonObject rawSettings;
        QJsonObject finalmask;
        QJsonObject sockopt;
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
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;

        void Validate(Validation::Issues& issues, const QString& serverAddress);
    };
}
