#pragma once
#include "include/configs/baseConfig.h"
#include "include/global/Validation.hpp"

namespace Configs {
    inline QStringList XrayNetworks = {"raw", "xhttp", "ws", "httpupgrade", "grpc", "xdrive"};
    // Xray still takes "Google Drive" for google-drive; ParseFromJson folds it.
    inline QStringList XrayXDriveServices = {"google-drive", "s3", "template", "local"};
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

    // Turns on useSystemCA in every TLS block of an Xray outbound, the XHTTP download leg included, unless the block sets it itself.
    void UseXraySystemCA(QJsonObject& outbound);

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

        QString XrayXDriveService(const QString& service);
        QString XrayXDriveFolder(const QString& service, const QString& folder);
        // google-drive takes exactly ClientID, ClientSecret and RefreshToken; s3 AccessKeyID, SecretAccessKey and an optional SessionToken.
        QString XrayXDriveSecrets(const QString& service, const QStringList& secrets);
        QString XrayXDriveTemplate(const QString& service, const QJsonObject& templateSpec);
        QString XrayXDriveS3Endpoint(const QString& service, const QString& endpoint);
        QString XrayXDriveS3Region(const QString& service, const QString& region);
        // Virtual-hosted addressing puts the bucket in the endpoint's host name, which a bare IP or a non-DNS name cannot carry.
        QString XrayXDriveS3Bucket(const QString& service, const QString& bucket, const QString& endpoint, bool pathStyle);
        // A tuning value: Xray reads a uint32, and 0 keeps its default.
        QString XrayXDriveTuning(const QString& text);
        QString XrayXDriveFrontPort(const QString& port);
        // A warning: the front port is dialed only together with a front address.
        QString XrayXDriveFrontPortAlone(const QString& address, const QString& port);
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
        // Only stored; the global xray_use_system_ca also turns it on at build time.
        bool useSystemCA = false;

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

    // XDRIVE has no share-link format: a link would carry the storage secrets, so profiles travel as JSON.
    class xrayXDrive : public baseConfig {
    public:
        QString service = "google-drive";
        QString remoteFolder;
        // google-drive: ClientID, ClientSecret, RefreshToken; s3: AccessKeyID, SecretAccessKey[, SessionToken];
        // template: read as {secret0}, {secret1}, ...
        QStringList secrets;
        QJsonObject templateSpec;
        // s3 only; Xray addresses the bucket path-style unless pathStyle is false.
        QString s3Endpoint;
        QString s3Region;
        QString s3Bucket;
        bool s3PathStyle = true;
        // The stream's own address and port: a front the storage API is dialed through.
        QString frontAddress;
        int frontPort = 0;
        // 0 keeps Xray's default.
        qint64 segmentBytes = 0;
        qint64 flushIntervalMs = 0;
        qint64 pollIntervalMs = 0;
        qint64 maxPollIntervalMs = 0;
        qint64 sessionTtlSeconds = 0;
        qint64 concurrency = 0;
        qint64 eagerWindowMs = 0;
        qint64 holeTimeoutMs = 0;
        // Kept as written; the editor has no fields for it.
        QJsonObject xmux;

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
        std::shared_ptr<xrayXDrive> xdrive = std::make_shared<xrayXDrive>();

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
