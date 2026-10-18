#pragma once
#include "include/configs/baseConfig.h"
#include "include/global/Validation.hpp"

namespace Configs
{

    // Xray's name for Go's own TLS stack; for sing-box it means uTLS off, overriding the global default.
    inline const QString tlsUnsafeFingerprint = "unsafe";

    inline QStringList tlsFingerprints = {"", "chrome", "firefox", "edge", "safari", "360", "qq", "ios", "android", "random", "randomized", tlsUnsafeFingerprint};

    // Xray's `cs` link value is colon separated; commas are accepted too.
    inline QStringList splitCipherSuites(const QString& value)
    {
        QStringList result;
        for (const auto& part : QString(value).replace(',', ':').split(':', Qt::SkipEmptyParts)) {
            if (const auto trimmed = part.trimmed(); !trimmed.isEmpty()) result << trimmed;
        }
        return result;
    }

    inline QStringList tlsSpoofMethods = {"", "wrong-sequence", "wrong-checksum", "wrong-ack", "wrong-md5", "wrong-timestamp"};

    // Go crypto/tls CipherSuites(): the only names sing-box accepts.
    inline const QStringList tlsSecureCipherSuites = {
        "TLS_AES_128_GCM_SHA256", "TLS_AES_256_GCM_SHA384", "TLS_CHACHA20_POLY1305_SHA256",
        "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA", "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA",
        "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA", "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",
        "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256", "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384",
        "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256", "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
        "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256", "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256",
    };

    // Go crypto/tls InsecureCipherSuites(): Xray accepts them too, sing-box refuses them.
    inline const QStringList tlsInsecureCipherSuites = {
        "TLS_RSA_WITH_RC4_128_SHA", "TLS_RSA_WITH_3DES_EDE_CBC_SHA", "TLS_RSA_WITH_AES_128_CBC_SHA",
        "TLS_RSA_WITH_AES_256_CBC_SHA", "TLS_RSA_WITH_AES_128_CBC_SHA256", "TLS_RSA_WITH_AES_128_GCM_SHA256",
        "TLS_RSA_WITH_AES_256_GCM_SHA384", "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA", "TLS_ECDHE_RSA_WITH_RC4_128_SHA",
        "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA", "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256",
        "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256",
    };

    // Index of "Off" in the Keep Default / On / Off combos.
    inline constexpr int kTriStateOff = 2;

    // Pins pasted as hex SHA-256 (as openssl prints them, colons allowed) in the base64 form sing-box takes; others unchanged.
    QStringList CertificatePinsFromHex(const QStringList& pins);

    // Import sanitizers: a value sing-box would refuse is dropped with a Warning under `field`.
    // The fingerprint as given, lower-cased when only its case was wrong, else empty.
    QString ImportTlsFingerprint(const QString& raw, const QString& field);
    QStringList ImportTlsCipherSuites(const QStringList& names, const QString& field);
    // The key without the trailing = padding that sing-box and Xray (RawURLEncoding) refuse, removed with a note.
    QString ImportRealityPublicKey(const QString& raw, const QString& field);

    // mihomo's client-fingerprint "none", its way of turning uTLS off.
    bool IsClashNoFingerprint(const std::string& fingerprint);

    // Each returns an empty string when acceptable, else one sentence; shared by the editors and the import check.
    namespace Checks {
        QString SingBoxFingerprint(const QString& fingerprint);
        // Colon or comma separated, as the editor and the `cs` link value carry them.
        QString SingBoxCipherSuites(const QString& text);
        QString TlsVersion(const QString& version);
        QString TlsVersionOrder(const QString& minVersion, const QString& maxVersion);
        QString TlsCurve(const QString& curve);
        QString RealityPublicKey(const QString& key, bool required = false);
        QString RealityShortId(const QString& shortId);
        QString PemCertificates(const QStringList& lines);
        QString PemPrivateKey(const QStringList& lines);
        QString CertificatePins(const QStringList& pins);
        QString EchConfig(const QStringList& lines);
        QString SpoofMethod(const QString& method);

        // Only the profile's own spoof SNI: the global preset is not the profile's to check.
        bool OwnSpoofOn(const QString& spoof, int spoofState);
        QString RealityConflict(bool ech, bool disableSni, bool insecure, bool spoof);
        QString PinsWithCertificate(bool pins, bool certificate);
        QString ClientCertificatePair(bool certificate, bool key);
        // serverName is the effective SNI: the TLS server name, else the server address.
        QString SpoofServerName(const QString& spoof, const QString& serverName, bool disableSni);
        // A uTLS client applies TLS fragment / record fragment and returns before it applies the spoof.
        QString SpoofIgnored(bool spoof, bool utls, bool fragment, bool recordFragment);
    }

    class uTLS : public baseConfig
    {
        public:
        bool supported = true;
        bool enabled = false;
        QString fingerPrint;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;
    };

    class ECH : public baseConfig
    {
        public:
        bool enabled = false;
        QStringList config;
        QString config_path;
        QString serverName;
        QString resolver;

        static QStringList NormalizeConfig(const QStringList& items);
        void SetQueryTarget(const QString& target);
        QString QueryTarget() const;
        QString ConfigBase64() const;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;
    };

    class Reality : public baseConfig
    {
        public:
        bool enabled = false;
        QString public_key;
        QString short_id;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
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
        QStringList certificate_sha256;
        QStringList certificate_public_key_sha256;
        QStringList client_certificate;
        QString client_certificate_path;
        QStringList client_key;
        QString client_key_path;
        // Tri-state: unspecified == "Keep Default", resolved against the global setting at Build.
        bool fragment = false;
        bool fragment_unspecified = true;
        QString fragment_fallback_delay;
        bool record_fragment = false;
        // Tri-state too; spoof / spoof_method fall back to the global preset when empty.
        QString spoof;
        QString spoof_method;
        bool spoof_enabled = false;
        bool spoof_unspecified = true;
        bool tls_tricks = false;
        bool tls_tricks_unspecified = true;
        std::shared_ptr<ECH> ech = std::make_shared<ECH>();
        std::shared_ptr<uTLS> utls = std::make_shared<uTLS>();
        std::shared_ptr<Reality> reality = std::make_shared<Reality>();

        // Tri-state combo helpers (index: 0 = Keep Default, 1 = On, 2 = Off).
        int getFragmentState() {
            if (fragment) return 1;
            if (fragment_unspecified) return 0;
            return 2;
        }
        void saveFragmentState(int state) {
            fragment = state == 1;
            fragment_unspecified = state == 0;
        }
        int getSpoofState() {
            if (spoof_enabled) return 1;
            if (spoof_unspecified) return 0;
            return 2;
        }
        void saveSpoofState(const int state) {
            spoof_enabled = state == 1;
            spoof_unspecified = state == 0;
        }
        int getTlsTricksState() {
            if (tls_tricks) return 1;
            if (tls_tricks_unspecified) return 0;
            return 2;
        }
        void saveTlsTricksState(int state) {
            tls_tricks = state == 1;
            tls_tricks_unspecified = state == 0;
        }
        bool FragmentEffectivelyOn();
        bool TlsTricksEffectivelyOn();
        bool SpoofEffectivelyOn();

        // The editor's TLS rules over the stored fields; limited mirrors outbound::LimitedTLS().
        void Validate(Validation::Issues& issues, const QString& serverAddress, bool limited);

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        // readPin false skips the certificate pin, which mihomo ignores where the protocol's tls switch is off.
        bool ParseFromClash(const clash::Proxies& object, bool readPin);
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;
    };
}
