#include "include/configs/common/TLS.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QRegularExpression>
#include <include/global/Utils.hpp>

#include <algorithm>

#include "include/configs/common/utils.h"
#include "include/configs/ParseDiagnostics.h"

namespace Configs {
    namespace {
        // Xray's pcs and sing-box's certificate_sha256 both hash the whole DER certificate: hex there, base64 here.
        QStringList certificateSha256FromPcs(const QString& pcs, const QString& reportField = {})
        {
            static const QRegularExpression sha256Hex(QStringLiteral("^[0-9a-fA-F]{64}$"));
            QStringList hashes;
            for (auto value : pcs.split(',', Qt::SkipEmptyParts)) {
                value = value.trimmed();
                if (value.isEmpty()) continue;
                if (const auto hex = QString(value).remove(':'); sha256Hex.match(hex).hasMatch()) {
                    hashes << QString::fromLatin1(QByteArray::fromHex(hex.toLatin1()).toBase64());
                } else if (!reportField.isEmpty()) {
                    ParseDiag::Error(reportField, QCoreApplication::translate("Validation", "Each pin must be a SHA-256 in hexadecimal (64 characters, colons allowed)."), value);
                }
            }
            return hashes;
        }

        struct TlsPemBlock {
            QString label;
            bool bodyOk = false;
        };

        // Go pem.Decode's view of the text: BEGIN/END lines around a base64 body; "Key: value" header lines are skipped.
        QList<TlsPemBlock> tlsPemBlocks(const QStringList& lines, bool* trailing = nullptr)
        {
            QList<TlsPemBlock> blocks;
            QString label, body;
            bool inside = false;
            for (const auto& raw : lines.join('\n').split('\n')) {
                const auto line = raw.trimmed();
                if (!inside) {
                    if (line.startsWith(QLatin1String("-----BEGIN ")) && line.endsWith(QLatin1String("-----")) && line.size() > 16) {
                        label = line.mid(11, line.size() - 16);
                        body.clear();
                        inside = true;
                    } else if (trailing != nullptr && !blocks.isEmpty() && !line.isEmpty()) {
                        *trailing = true;
                    }
                    continue;
                }
                if (line == QStringLiteral("-----END %1-----").arg(label)) {
                    blocks.append({label, Validation::Base64(body).isEmpty()});
                    inside = false;
                } else if (!line.contains(':')) {
                    body += line;
                }
            }
            return blocks;
        }

        bool tlsHasText(const QStringList& lines)
        {
            return std::any_of(lines.cbegin(), lines.cend(), [](const QString& l) { return !l.trimmed().isEmpty(); });
        }

        // Link values: trimmed, empty items dropped (an empty ALPN entry makes Go abort every handshake).
        QStringList tlsLinkList(const QString& value)
        {
            return SplitAndTrim(value, ",", false);
        }

        QString tlsCipherSuiteRule(const QString& name)
        {
            if (tlsSecureCipherSuites.contains(name)) return {};
            if (tlsInsecureCipherSuites.contains(name)) {
                return QCoreApplication::translate("Validation", "sing-box refuses this suite because Go lists it as insecure (CBC-SHA256, RC4, 3DES and RSA key exchange); only VLESS profiles running on Xray can use it.");
            }
            return QCoreApplication::translate("Validation", "Unknown cipher suite. Use Go's names, for example TLS_AES_128_GCM_SHA256.");
        }

        QStringList tlsKnownCurves(const QStringList& names, const QString& field)
        {
            QStringList kept;
            for (const auto& name : names) {
                if (const auto curve = name.trimmed(); !curve.isEmpty() && AcceptOptional(field, Checks::TlsCurve(curve), curve)) kept << curve;
            }
            return kept;
        }

        // PEM text kept one line per item, whether sing-box wrote it as one string or as a list.
        QStringList tlsPemLines(const QJsonValue& value, const QString& field)
        {
            QStringList lines;
            for (const auto& item : JsonStringList(value, field)) lines << item.split('\n', Qt::SkipEmptyParts);
            return lines;
        }

        void tlsAdd(Validation::Issues& issues, Validation::Severity severity, const QString& field, const QString& message,
                    const QString& value = {})
        {
            if (!message.isEmpty()) issues.append({severity, field, message, value});
        }

        QString pcsFromCertificateSha256(const QStringList& hashes)
        {
            QStringList pcs;
            for (const auto& hash : hashes) {
                if (const auto raw = QByteArray::fromBase64(hash.trimmed().toLatin1()); raw.size() == 32) pcs << QString::fromLatin1(raw.toHex());
            }
            return pcs.join(',');
        }
    }

    QStringList CertificatePinsFromHex(const QStringList& pins)
    {
        QStringList out;
        for (const auto& pin : pins) {
            if (pin.trimmed().isEmpty()) continue;
            const auto converted = certificateSha256FromPcs(pin);
            out << (converted.size() == 1 && !pin.contains(',') ? converted.first() : pin.trimmed());
        }
        return out;
    }

    QString ImportTlsFingerprint(const QString& raw, const QString& field)
    {
        const auto fingerprint = raw.trimmed();
        const auto message = Checks::SingBoxFingerprint(fingerprint);
        if (message.isEmpty()) return fingerprint;
        if (const auto lower = fingerprint.toLower(); Checks::SingBoxFingerprint(lower).isEmpty()) return lower;
        AcceptOptional(field, message, fingerprint);
        return {};
    }

    QStringList ImportTlsCipherSuites(const QStringList& names, const QString& field)
    {
        QStringList kept;
        for (const auto& name : names) {
            if (AcceptOptional(field, tlsCipherSuiteRule(name), name)) kept << name;
        }
        return kept;
    }

    QString ImportRealityPublicKey(const QString& raw, const QString& field)
    {
        const auto key = raw.trimmed();
        if (!key.endsWith('=')) return key;
        auto unpadded = key;
        while (unpadded.endsWith('=')) unpadded.chop(1);
        if (unpadded.isEmpty() || !Checks::RealityPublicKey(unpadded).isEmpty()) return key;
        ParseDiag::Warn(field, QCoreApplication::translate("Validation", "The trailing = padding was removed from the REALITY public key."));
        return unpadded;
    }

    bool IsClashNoFingerprint(const std::string& fingerprint)
    {
        return QString::fromStdString(fingerprint).trimmed().compare(QLatin1String("none"), Qt::CaseInsensitive) == 0;
    }

    namespace Checks {
        QString SingBoxFingerprint(const QString& fingerprint)
        {
            // sing-box common/tls/utls_client.go uTLSClientHelloID, case-sensitive; "unsafe" is Throne's.
            static const QStringList names = {"", "chrome", "chrome_psk", "chrome_psk_shuffle", "chrome_padding_psk_shuffle",
                                              "chrome_pq", "chrome_pq_psk", "firefox", "edge", "safari", "360", "qq", "ios",
                                              "android", "random", "randomized", "custom", tlsUnsafeFingerprint};
            const auto t = fingerprint.trimmed();
            if (names.contains(t)) return {};
            return QCoreApplication::translate("Validation", "Unknown fingerprint \"%1\". Use chrome, firefox, safari, ios, android, edge, 360, qq, random, randomized or unsafe.").arg(t);
        }

        QString SingBoxCipherSuites(const QString& text)
        {
            return Validation::EachItem(QString(text).replace(':', ','), Validation::Split::Comma, tlsCipherSuiteRule);
        }

        QString TlsVersion(const QString& version)
        {
            return Validation::OneOf(version, {"1.0", "1.1", "1.2", "1.3"});
        }

        QString TlsVersionOrder(const QString& minVersion, const QString& maxVersion)
        {
            const auto low = minVersion.trimmed(), high = maxVersion.trimmed();
            if (low.isEmpty() || high.isEmpty() || !TlsVersion(low).isEmpty() || !TlsVersion(high).isEmpty()) return {};
            // "1.0" to "1.3" order as text.
            if (low > high) return QCoreApplication::translate("Validation", "The minimum TLS version is above the maximum.");
            return {};
        }

        QString TlsCurve(const QString& curve)
        {
            return Validation::OneOf(curve, {"P256", "P384", "P521", "X25519", "X25519MLKEM768"}, Validation::Empty::Allowed, Qt::CaseInsensitive);
        }

        QString RealityPublicKey(const QString& key, bool required)
        {
            const auto t = key.trimmed();
            if (t.isEmpty()) {
                return required ? QCoreApplication::translate("Validation", "REALITY needs the server's public key (43 characters, as printed by xray x25519).") : QString();
            }
            const auto message = Validation::Base64Key(t, 32, true, false);
            if (message.isEmpty()) return {};
            if (auto unpadded = t; unpadded.endsWith('=')) {
                while (unpadded.endsWith('=')) unpadded.chop(1);
                if (Validation::Base64Key(unpadded, 32, true, false).isEmpty()) return QCoreApplication::translate("Validation", "Remove the trailing = padding.");
            }
            return message;
        }

        QString RealityShortId(const QString& shortId)
        {
            return Validation::Hex(shortId, 0, 16, true);
        }

        QString PemCertificates(const QStringList& lines)
        {
            if (!tlsHasText(lines)) return {};
            const auto blocks = tlsPemBlocks(lines);
            if (std::any_of(blocks.cbegin(), blocks.cend(), [](const TlsPemBlock& b) { return b.label == "CERTIFICATE" && b.bodyOk; })) return {};
            return QCoreApplication::translate("Validation", "Paste the certificate as PEM text, starting with -----BEGIN CERTIFICATE-----.");
        }

        QString PemPrivateKey(const QStringList& lines)
        {
            if (!tlsHasText(lines)) return {};
            const auto blocks = tlsPemBlocks(lines);
            // Go tls.X509KeyPair takes the first "PRIVATE KEY" or "<kind> PRIVATE KEY" block.
            const auto key = std::find_if(blocks.cbegin(), blocks.cend(), [](const TlsPemBlock& b) {
                return b.label == "PRIVATE KEY" || b.label.endsWith(QLatin1String(" PRIVATE KEY"));
            });
            if (key != blocks.cend() && key->bodyOk) return {};
            return QCoreApplication::translate("Validation", "Paste the private key as PEM text, starting with -----BEGIN PRIVATE KEY-----.");
        }

        QString CertificatePins(const QStringList& pins)
        {
            const auto rule = [](const QString& pin) -> QString {
                if (certificateSha256FromPcs(pin).size() == 1) {
                    return QCoreApplication::translate("Validation", "This is a hexadecimal SHA-256; write it in base64 (44 characters ending in =).");
                }
                return Validation::Base64Key(pin, 32);
            };
            return Validation::EachItem(pins.join('\n'), Validation::Split::Newline, rule);
        }

        QString EchConfig(const QStringList& lines)
        {
            if (!tlsHasText(lines)) return {};
            bool trailing = false;
            const auto blocks = tlsPemBlocks(lines, &trailing);
            // sing-box common/tls/ech.go: exactly one ECH CONFIGS block and nothing after it.
            if (blocks.size() == 1 && blocks.first().label == "ECH CONFIGS" && blocks.first().bodyOk && !trailing) return {};
            return QCoreApplication::translate("Validation", "Paste the ECH config list as base64, or as one -----BEGIN ECH CONFIGS----- block.");
        }

        QString SpoofMethod(const QString& method)
        {
            return Validation::OneOf(method, tlsSpoofMethods);
        }

        bool OwnSpoofOn(const QString& spoof, int spoofState)
        {
            return !spoof.trimmed().isEmpty() && spoofState != kTriStateOff;
        }

        QString RealityConflict(bool ech, bool disableSni, bool insecure, bool spoof)
        {
            if (ech) return QCoreApplication::translate("Validation", "Reality cannot be combined with ECH. Turn off ECH in Advanced, or clear the Reality public key.");
            if (disableSni && !insecure) return QCoreApplication::translate("Validation", "Reality cannot be used with Disable SNI. Turn off Disable SNI in Advanced.");
            if (spoof) return QCoreApplication::translate("Validation", "Reality cannot be combined with TLS spoof. Set TLS Spoof to Off in Advanced.");
            return {};
        }

        QString PinsWithCertificate(bool pins, bool certificate)
        {
            if (!pins || !certificate) return {};
            return QCoreApplication::translate("Validation", "A custom certificate cannot be combined with certificate pins; keep only one of them.");
        }

        QString ClientCertificatePair(bool certificate, bool key)
        {
            if (certificate == key) return {};
            return QCoreApplication::translate("Validation", "Set the client certificate and the client key together.");
        }

        QString SpoofServerName(const QString& spoof, const QString& serverName, bool disableSni)
        {
            const auto fake = spoof.trimmed();
            if (fake.isEmpty()) return {};
            if (disableSni) return QCoreApplication::translate("Validation", "TLS spoof needs an SNI. Turn off Disable SNI, or set TLS Spoof to Off.");
            auto name = serverName.trimmed();
            if (name.startsWith('[') && name.endsWith(']')) name = name.mid(1, name.size() - 2);
            // sing-box common/tls/client.go: no SNI for an empty or IP server name.
            if (name.isEmpty() || Validation::Ip(name).isEmpty()) {
                return QCoreApplication::translate("Validation", "TLS spoof needs a domain name as the SNI. Enter one in SNI, or set TLS Spoof to Off.");
            }
            if (fake.compare(name, Qt::CaseInsensitive) == 0) return QCoreApplication::translate("Validation", "The spoof SNI must differ from the real SNI.");
            return {};
        }

        QString SpoofIgnored(bool spoof, bool utls, bool fragment, bool recordFragment)
        {
            if (!spoof || !utls || (!fragment && !recordFragment)) return {};
            return QCoreApplication::translate("Validation", "TLS spoof is skipped while TLS fragment or record fragment is on with a uTLS fingerprint.");
        }
    }

    bool uTLS::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid() && !url.errorString().startsWith("Invalid port")) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("fp")) {
            const auto value = query.queryItemValue("fp");
            fingerPrint = supported ? ImportTlsFingerprint(value, "fp") : value;
        }
        if (!fingerPrint.isEmpty()) enabled = true;
        return true;
    }
    bool uTLS::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool();
        if (object.contains("fingerprint")) {
            const auto value = object["fingerprint"].toString();
            fingerPrint = supported && ParseDiag::Active() ? ImportTlsFingerprint(value, "tls.utls.fingerprint") : value;
        }
        return true;
    }
    bool uTLS::ParseFromClash(const clash::Proxies& object)
    {
        if (object.client_fingerprint.empty()) return false;
        const auto value = QString::fromStdString(object.client_fingerprint);
        // mihomo's "none" turns uTLS off, which Throne's unsafe does; empty would take the global default fingerprint.
        if (IsClashNoFingerprint(object.client_fingerprint)) fingerPrint = supported ? tlsUnsafeFingerprint : QString();
        else fingerPrint = supported ? ImportTlsFingerprint(value, "client-fingerprint") : value;
        enabled = !fingerPrint.isEmpty();
        return true;
    }
    QString uTLS::ExportToLink()
    {
        QUrlQuery query;
        if (!enabled) return "";
        if (!fingerPrint.isEmpty()) query.addQueryItem("fp", fingerPrint);
        return query.toString();
    }
    QJsonObject uTLS::ExportToJson()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        if (!fingerPrint.isEmpty()) object["fingerprint"] = fingerPrint;
        return object;
    }
    QJsonObject uTLS::ExportIdentity()
    {
        QJsonObject object;
        if (enabled && !fingerPrint.isEmpty()) object["fingerprint"] = fingerPrint;
        return object;
    }
    BuildResult uTLS::Build()
    {
        if (!supported) return {};
        auto obj = ExportToJson();
        if ((obj.isEmpty() || obj["enabled"].toBool() == false || fingerPrint.isEmpty()) && !Configs::dataManager->settingsRepo->utlsFingerprint.isEmpty()) {
            obj["enabled"] = true;
            obj["fingerprint"] = Configs::dataManager->settingsRepo->utlsFingerprint;
        }
        if (obj.value("fingerprint").toString() == tlsUnsafeFingerprint) return {};
        return {obj, ""};
    }

    QStringList ECH::NormalizeConfig(const QStringList &items) {
        for (const auto &i : items) {
            if (i.contains(QStringLiteral("-----BEGIN"))) return items;
        }
        const auto joined = items.join(QString()).trimmed();
        if (joined.isEmpty()) return {};
        return QStringList{
            QStringLiteral("-----BEGIN ECH CONFIGS-----"),
            joined,
            QStringLiteral("-----END ECH CONFIGS-----")
        };
    }

    void ECH::SetQueryTarget(const QString &target) {
        const QString trimmed = target.trimmed();
        const int schemeIdx = trimmed.indexOf("://");
        if (schemeIdx != -1) {
            const int plusIdx = trimmed.indexOf('+');
            if (plusIdx > 0 && plusIdx < schemeIdx) {
                serverName = trimmed.left(plusIdx).trimmed();
                resolver = trimmed.mid(plusIdx + 1).trimmed();
            } else {
                serverName.clear();
                resolver = trimmed;
            }
        } else {
            serverName = trimmed;
            resolver.clear();
        }
    }


    QString ECH::QueryTarget() const {
        if (resolver.isEmpty()) return serverName;
        return serverName.isEmpty() ? resolver : serverName + "+" + resolver;
    }

    QString ECH::ConfigBase64() const {
        QString b64;
        for (const auto &l : config) {
            if (!l.contains("-----")) b64 += l.trimmed();
        }
        return b64;
    }

    bool ECH::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid() && !url.errorString().startsWith("Invalid port")) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("ech")) {
            const QString echVal = query.queryItemValue("ech", QUrl::FullyDecoded).trimmed();
            enabled = !echVal.isEmpty() && echVal != "0" && echVal != "false";
            if (enabled && echVal != "1" && echVal != "true") {
                if (echVal.contains("://") || echVal.contains('.')) {
                    SetQueryTarget(echVal);
                } else {
                    config = NormalizeConfig(QStringList{echVal});
                }
            }
        }

        if (query.hasQueryItem("ech_enabled")) enabled = query.queryItemValue("ech_enabled") == "true";
        if (query.hasQueryItem("ech_config")) {
            config = NormalizeConfig(query.queryItemValue("ech_config").split(",", Qt::SkipEmptyParts));
        }
        if (query.hasQueryItem("ech_config_path")) config_path = query.queryItemValue("ech_config_path");
        if (query.hasQueryItem("ech_server_name")) serverName = query.queryItemValue("ech_server_name");
        return true;
    }
    bool ECH::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool();
        if (object.contains("config")) config = NormalizeConfig(tlsPemLines(object["config"], "tls.ech.config"));
        if (object.contains("config_path")) config_path = object["config_path"].toString();
        if (object.contains("query_server_name")) serverName = object["query_server_name"].toString();
        if (object.contains("resolver")) resolver = object["resolver"].toString();
        return true;
    }
    QString ECH::ExportToLink()
    {
        QUrlQuery query;
        if (!enabled) return "";

        const QString b64 = ConfigBase64();
        query.addQueryItem("ech_enabled", "true");
        if (!b64.isEmpty()) query.addQueryItem("ech_config", b64);
        if (!config_path.isEmpty()) query.addQueryItem("ech_config_path", config_path);
        if (!serverName.isEmpty()) query.addQueryItem("ech_server_name", serverName);

        if (!serverName.isEmpty() && !resolver.isEmpty()) query.addQueryItem("ech", serverName + "+" + resolver);
        else if (serverName.isEmpty() && !b64.isEmpty()) query.addQueryItem("ech", b64);
        else if (serverName.isEmpty() && !resolver.isEmpty()) query.addQueryItem("ech", resolver);
        return query.toString();
    }
    QJsonObject ECH::ExportToJson()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        if (!config.isEmpty()) {
            object["config"] = QListStr2QJsonArray(config);
        }
        if (!config_path.isEmpty()) object["config_path"] = config_path;
        if (!serverName.isEmpty()) object["query_server_name"] = toAceHost(serverName);
        if (!resolver.isEmpty()) object["resolver"] = resolver;
        return object;
    }
    QJsonObject ECH::ExportIdentity()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = true;
        if (!serverName.isEmpty()) object["query_server_name"] = toAceHost(serverName);
        if (!resolver.isEmpty()) object["resolver"] = resolver;
        return object;
    }
    BuildResult ECH::Build()
    {
        auto obj = ExportToJson();
        obj.remove("resolver");
        return {obj, ""};
    }

    bool Reality::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid() && !url.errorString().startsWith("Invalid port")) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("pbk"))
        {
            enabled = true;
            public_key = ImportRealityPublicKey(query.queryItemValue("pbk"), "pbk");
            short_id = query.queryItemValue("sid").trimmed();
            if (!query.queryItemValue("pqv").trimmed().isEmpty()) {
                ParseDiag::Error("pqv", QCoreApplication::translate("Validation", "sing-box cannot verify the REALITY ML-DSA-65 key, and ignoring it would weaken the server check. Only VLESS profiles running on Xray support it."));
            }
        }
        return true;
    }
    bool Reality::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool();
        if (object.contains("public_key")) {
            const auto key = object["public_key"].toString();
            public_key = ParseDiag::Active() ? ImportRealityPublicKey(key, "tls.reality.public_key") : key.trimmed();
        }
        if (object.contains("short_id")) short_id = object["short_id"].toString().trimmed();
        return true;
    }
    bool Reality::ParseFromClash(const clash::Proxies& object)
    {
        if (object.reality_opts.public_key.empty()) return false;
        enabled = true;
        public_key = ImportRealityPublicKey(QString::fromStdString(object.reality_opts.public_key), "reality-opts.public-key");
        short_id = QString::fromStdString(object.reality_opts.short_id).trimmed();
        return true;
    }
    QString Reality::ExportToLink()
    {
        QUrlQuery query;
        if (!enabled) return "";
        query.addQueryItem("pbk", public_key);
        query.addQueryItem("sid", short_id);
        return query.toString();
    }
    QJsonObject Reality::ExportToJson()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        if (!public_key.isEmpty()) object["public_key"] = public_key;
        if (!short_id.isEmpty()) object["short_id"] = short_id;
        return object;
    }
    QJsonObject Reality::ExportIdentity()
    {
        QJsonObject object;
        if (enabled) object["enabled"] = true;
        return object;
    }
    BuildResult Reality::Build()
    {
        QJsonObject object;
        QString error;
        if (!public_key.isEmpty() || enabled) {
            object["enabled"] = true;
            object["public_key"] = public_key;
            // The core hex-decodes short_id into an 8-byte array: a longer one panics it, an odd length fails.
            static const QRegularExpression shortIdHex(QStringLiteral("^(?:[0-9a-fA-F]{2}){0,8}$"));
            if (shortIdHex.match(short_id).hasMatch()) {
                if (!short_id.isEmpty()) object["short_id"] = short_id;
            } else {
                // No value in the message: the error box renders rich text.
                error = QCoreApplication::translate("Validation", "The REALITY short ID is invalid: it must be at most 16 hexadecimal characters, an even number of them.");
                qWarning() << "REALITY short_id dropped from the build: not even-length hex of at most 8 bytes";
            }
        }
        return {object, error};
    }

    bool TLS::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid() && !url.errorString().startsWith("Invalid port")) return false;
        auto query = QUrlQuery(url.query());

        // An empty security= counts as absent, so an sni still turns TLS on.
        const auto security = query.queryItemValue("security").trimmed().toLower();
        const bool securityOff = security == "none" || security == "false" || security == "0";
        if (!security.isEmpty()) enabled = security == "tls" || security == "true" || security == "1" || security == "reality";
        if (query.hasQueryItem("disable_sni")) disable_sni = query.queryItemValue("disable_sni").replace("1", "true") == "true";
        if (query.hasQueryItem("sni")) server_name = query.queryItemValue("sni");
        if (query.hasQueryItem("peer")) server_name = query.queryItemValue("peer");
        if (query.hasQueryItem("allowInsecure")) insecure = query.queryItemValue("allowInsecure").replace("1", "true") == "true";
        if (query.hasQueryItem("allow_insecure")) insecure = query.queryItemValue("allow_insecure").replace("1", "true") == "true";
        if (query.hasQueryItem("insecure")) insecure = query.queryItemValue("insecure").replace("1", "true") == "true";
        if (query.hasQueryItem("alpn")) alpn = tlsLinkList(query.queryItemValue("alpn", QUrl::FullyDecoded));
        for (const auto& [key, target] : {std::pair{QStringLiteral("tls_min_version"), &min_version}, std::pair{QStringLiteral("tls_max_version"), &max_version}}) {
            if (!query.hasQueryItem(key)) continue;
            if (const auto version = query.queryItemValue(key).trimmed(); AcceptOptional(key, Checks::TlsVersion(version), version)) *target = version;
        }
        if (query.hasQueryItem("tls_cipher_suites")) cipher_suites = ImportTlsCipherSuites(splitCipherSuites(query.queryItemValue("tls_cipher_suites", QUrl::FullyDecoded)), "tls_cipher_suites");
        if (query.hasQueryItem("cs")) cipher_suites = ImportTlsCipherSuites(splitCipherSuites(query.queryItemValue("cs", QUrl::FullyDecoded)), "cs");
        if (query.hasQueryItem("tls_curve_preferences")) curve_preferences = tlsKnownCurves(query.queryItemValue("tls_curve_preferences").split(","), "tls_curve_preferences");
        if (query.hasQueryItem("tls_certificate")) certificate = query.queryItemValue("tls_certificate").split(",");
        if (query.hasQueryItem("tls_certificate_path")) certificate_path = query.queryItemValue("tls_certificate_path");
        if (query.hasQueryItem("pcs")) certificate_sha256 = certificateSha256FromPcs(query.queryItemValue("pcs", QUrl::FullyDecoded), "pcs");
        if (query.hasQueryItem("tls_certificate_public_key_sha256")) certificate_public_key_sha256 = tlsLinkList(query.queryItemValue("tls_certificate_public_key_sha256"));
        if (query.hasQueryItem("tls_client_certificate")) client_certificate = query.queryItemValue("tls_client_certificate").split(",");
        if (query.hasQueryItem("tls_client_certificate_path")) client_certificate_path = query.queryItemValue("tls_client_certificate_path");
        if (query.hasQueryItem("tls_client_key")) client_key = query.queryItemValue("tls_client_key").split(",");
        if (query.hasQueryItem("tls_client_key_path")) client_key_path = query.queryItemValue("tls_client_key_path");
        if (query.hasQueryItem("tls_fragment")) { fragment = query.queryItemValue("tls_fragment") == "true"; fragment_unspecified = false; }
        else fragment_unspecified = true;
        if (query.hasQueryItem("tls_fragment_fallback_delay")) {
            const auto delay = query.queryItemValue("tls_fragment_fallback_delay").trimmed();
            if (AcceptOptional("tls_fragment_fallback_delay", Validation::Duration(delay), delay)) fragment_fallback_delay = delay;
        }
        if (query.hasQueryItem("tls_record_fragment")) record_fragment = query.queryItemValue("tls_record_fragment") == "true";
        if (query.hasQueryItem("tls_spoof")) spoof = query.queryItemValue("tls_spoof").trimmed();
        if (query.hasQueryItem("tls_spoof_method")) {
            const auto method = query.queryItemValue("tls_spoof_method").trimmed();
            if (AcceptOptional("tls_spoof_method", Checks::SpoofMethod(method), method)) spoof_method = method;
        }
        if (query.hasQueryItem("tls_spoof_enabled")) { spoof_enabled = query.queryItemValue("tls_spoof_enabled") == "true"; spoof_unspecified = false; }
        else spoof_unspecified = true;
        if (query.hasQueryItem("tls_tricks")) { tls_tricks = query.queryItemValue("tls_tricks") == "true"; tls_tricks_unspecified = false; }
        else tls_tricks_unspecified = true;
        if (!securityOff && !server_name.isEmpty()) enabled = true;
        ech->ParseFromLink(link);
        utls->ParseFromLink(link);
        reality->ParseFromLink(link);
        // Without pbk Reality stays off and the profile would silently become plain TLS.
        if (security == "reality" && !query.hasQueryItem("pbk")) {
            ParseDiag::Error("pbk", QCoreApplication::translate("Validation", "REALITY needs the server's public key (43 characters, as printed by xray x25519)."));
        }
        return true;
    }
    bool TLS::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool();
        if (object.contains("disable_sni")) disable_sni = object["disable_sni"].toBool();
        if (object.contains("server_name")) server_name = object["server_name"].toString();
        if (object.contains("insecure")) insecure = object["insecure"].toBool();
        // ParseFromJson is also the DB loader: only an import drops values.
        const bool importing = ParseDiag::Active();
        if (object.contains("alpn")) {
            alpn = JsonStringList(object["alpn"], "tls.alpn");
            if (importing) alpn = tlsLinkList(alpn.join(','));
        }
        for (const auto& [key, target] : {std::pair{QStringLiteral("min_version"), &min_version}, std::pair{QStringLiteral("max_version"), &max_version}}) {
            if (!object.contains(key)) continue;
            const auto version = object[key].toString().trimmed();
            if (!importing || AcceptOptional("tls." + key, Checks::TlsVersion(version), version)) *target = version;
        }
        if (object.contains("cipher_suites")) {
            cipher_suites = JsonStringList(object["cipher_suites"], "tls.cipher_suites");
            if (importing) cipher_suites = ImportTlsCipherSuites(cipher_suites, "tls.cipher_suites");
        }
        if (object.contains("curve_preferences")) {
            curve_preferences = JsonStringList(object["curve_preferences"], "tls.curve_preferences");
            if (importing) curve_preferences = tlsKnownCurves(curve_preferences, "tls.curve_preferences");
        }
        if (object.contains("certificate")) certificate = tlsPemLines(object["certificate"], "tls.certificate");
        if (object.contains("certificate_path")) certificate_path = object["certificate_path"].toString();
        if (object.contains("certificate_sha256")) {
            certificate_sha256 = JsonStringList(object["certificate_sha256"], "tls.certificate_sha256");
            if (importing) certificate_sha256 = CertificatePinsFromHex(certificate_sha256);
        }
        if (object.contains("certificate_public_key_sha256")) {
            certificate_public_key_sha256 = JsonStringList(object["certificate_public_key_sha256"], "tls.certificate_public_key_sha256");
            if (importing) certificate_public_key_sha256 = CertificatePinsFromHex(certificate_public_key_sha256);
        }
        if (object.contains("client_certificate")) client_certificate = tlsPemLines(object["client_certificate"], "tls.client_certificate");
        if (object.contains("client_certificate_path")) client_certificate_path = object["client_certificate_path"].toString();
        if (object.contains("client_key")) client_key = tlsPemLines(object["client_key"], "tls.client_key");
        if (object.contains("client_key_path")) client_key_path = object["client_key_path"].toString();
        if (object.contains("fragment")) { fragment = object["fragment"].toBool(); fragment_unspecified = false; }
        else fragment_unspecified = true;
        if (object.contains("fragment_fallback_delay")) {
            const auto delay = object["fragment_fallback_delay"].toString().trimmed();
            if (!importing || AcceptOptional("tls.fragment_fallback_delay", Validation::Duration(delay), delay)) fragment_fallback_delay = delay;
        }
        if (object.contains("record_fragment")) record_fragment = object["record_fragment"].toBool();
        if (object.contains("spoof")) spoof = object["spoof"].toString();
        if (object.contains("spoof_method")) {
            const auto method = object["spoof_method"].toString().trimmed();
            if (!importing || AcceptOptional("tls.spoof_method", Checks::SpoofMethod(method), method)) spoof_method = method;
        }
        if (object.contains("spoof_enabled")) { spoof_enabled = object["spoof_enabled"].toBool(); spoof_unspecified = false; }
        else spoof_unspecified = true;
        if (object.contains("tls_tricks")) { tls_tricks = object["tls_tricks"].toObject()["mixedcase_sni"].toBool(); tls_tricks_unspecified = false; }
        else tls_tricks_unspecified = true;
        if (object.contains("ech")) ech->ParseFromJson(object["ech"].toObject());
        if (object.contains("utls")) utls->ParseFromJson(object["utls"].toObject());
        if (object.contains("reality")) reality->ParseFromJson(object["reality"].toObject());
        return true;
    }
    bool TLS::ParseFromClash(const clash::Proxies& object)
    {
        return ParseFromClash(object, true);
    }
    bool TLS::ParseFromClash(const clash::Proxies& object, bool readPin)
    {
        enabled = object.tls;
        if (!object.servername.empty()) {
            server_name = QString::fromStdString(object.servername);
        } else if (!object.sni.empty()) {
            server_name = QString::fromStdString(object.sni);
        } else {
            server_name = QString::fromStdString(object.server);
        }
        insecure = object.skip_cert_verify;
        for (const auto& s : object.alpn) {
            if (const auto protocol = QString::fromStdString(s).trimmed(); !protocol.isEmpty()) alpn.append(protocol);
        }
        // mihomo's fingerprint is the certificate pin, hex like Xray's pcs; client-fingerprint is the uTLS hello.
        if (readPin && !object.fingerprint.empty()) certificate_sha256 = certificateSha256FromPcs(QString::fromStdString(object.fingerprint), "fingerprint");
        utls->ParseFromClash(object);
        reality->ParseFromClash(object);
        return true;
    }
    QString TLS::ExportToLink()
    {
        QUrlQuery query;
        if (!enabled) return "";
        query.addQueryItem("security", reality->enabled ? "reality" : "tls");
        if (disable_sni) query.addQueryItem("disable_sni", "true");
        if (!server_name.isEmpty()) query.addQueryItem("sni", server_name);
        if (insecure) query.addQueryItem("allowInsecure", "true");
        if (!alpn.isEmpty()) query.addQueryItem("alpn", alpn.join(","));
        if (!min_version.isEmpty()) query.addQueryItem("tls_min_version", min_version);
        if (!max_version.isEmpty()) query.addQueryItem("tls_max_version", max_version);
        if (!cipher_suites.isEmpty()) query.addQueryItem("cs", cipher_suites.join(":"));
        if (!curve_preferences.isEmpty()) query.addQueryItem("tls_curve_preferences", curve_preferences.join(","));
        if (!certificate.isEmpty()) query.addQueryItem("tls_certificate", certificate.join(","));
        if (!certificate_path.isEmpty()) query.addQueryItem("tls_certificate_path", certificate_path);
        if (const auto pcs = pcsFromCertificateSha256(certificate_sha256); !pcs.isEmpty()) query.addQueryItem("pcs", pcs);
        if (!certificate_public_key_sha256.isEmpty()) query.addQueryItem("tls_certificate_public_key_sha256", certificate_public_key_sha256.join(","));
        if (!client_certificate.isEmpty()) query.addQueryItem("tls_client_certificate", client_certificate.join(","));
        if (!client_certificate_path.isEmpty()) query.addQueryItem("tls_client_certificate_path", client_certificate_path);
        if (!client_key.isEmpty()) query.addQueryItem("tls_client_key", client_key.join(","));
        if (!client_key_path.isEmpty()) query.addQueryItem("tls_client_key_path", client_key_path);
        if (!fragment_unspecified) query.addQueryItem("tls_fragment", fragment ? "true" : "false");
        if (!fragment_fallback_delay.isEmpty()) query.addQueryItem("tls_fragment_fallback_delay", fragment_fallback_delay);
        if (record_fragment) query.addQueryItem("tls_record_fragment", "true");
        if (!spoof_unspecified) query.addQueryItem("tls_spoof_enabled", spoof_enabled ? "true" : "false");
        if (!spoof.isEmpty()) {
            query.addQueryItem("tls_spoof", spoof);
            if (!spoof_method.isEmpty()) query.addQueryItem("tls_spoof_method", spoof_method);
        }
        if (!tls_tricks_unspecified) query.addQueryItem("tls_tricks", tls_tricks ? "true" : "false");
        mergeUrlQuery(query, ech->ExportToLink());
        mergeUrlQuery(query, utls->ExportToLink());
        mergeUrlQuery(query, reality->ExportToLink());
        return query.toString();
    }
    QJsonObject TLS::ExportToJson()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        if (disable_sni) object["disable_sni"] = disable_sni;
        if (!server_name.isEmpty()) object["server_name"] = toAceHost(server_name);
        if (insecure) object["insecure"] = insecure;
        if (!alpn.isEmpty()) {
            object["alpn"] = QListStr2QJsonArray(alpn);
        }
        if (!min_version.isEmpty()) object["min_version"] = min_version;
        if (!max_version.isEmpty()) object["max_version"] = max_version;
        if (!cipher_suites.isEmpty()) {
            object["cipher_suites"] = QListStr2QJsonArray(cipher_suites);
        }
        if (!curve_preferences.isEmpty()) {
            object["curve_preferences"] = QListStr2QJsonArray(curve_preferences);
        }
        if (!certificate.isEmpty()) {
            object["certificate"] = QListStr2QJsonArray(certificate);
        }
        if (!certificate_path.isEmpty()) object["certificate_path"] = certificate_path;
        if (!certificate_sha256.isEmpty()) {
            object["certificate_sha256"] = QListStr2QJsonArray(certificate_sha256);
        }
        if (!certificate_public_key_sha256.isEmpty()) {
            object["certificate_public_key_sha256"] = QListStr2QJsonArray(certificate_public_key_sha256);
        }
        if (!client_certificate.isEmpty()) object["client_certificate"] = QListStr2QJsonArray(client_certificate);
        if (!client_certificate_path.isEmpty()) object["client_certificate_path"] = client_certificate_path;
        if (!client_key.isEmpty()) {
            object["client_key"] = QListStr2QJsonArray(client_key);
        }
        if (!client_key_path.isEmpty()) object["client_key_path"] = client_key_path;
        // Tri-state: true = On, false = Off, key absent = Keep Default.
        if (!fragment_unspecified) object["fragment"] = fragment;
        if (!fragment_fallback_delay.isEmpty()) object["fragment_fallback_delay"] = fragment_fallback_delay;
        if (record_fragment) object["record_fragment"] = record_fragment;
        // spoof_enabled is ours, not sing-box's: Build() resolves it away, but ExportToJson is the stored form.
        if (!spoof_unspecified) object["spoof_enabled"] = spoof_enabled;
        // spoof_method only means anything alongside a spoof SNI.
        if (!spoof.isEmpty()) {
            object["spoof"] = spoof;
            if (!spoof_method.isEmpty()) object["spoof_method"] = spoof_method;
        }
        if (!tls_tricks_unspecified) object["tls_tricks"] = QJsonObject{{"mixedcase_sni", tls_tricks}};
        if (ech->enabled) object["ech"] = ech->ExportToJson();
        if (utls->enabled) object["utls"] = utls->ExportToJson();
        if (reality->enabled) object["reality"] = reality->ExportToJson();
        return object;
    }
    QJsonObject TLS::ExportIdentity()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = true;
        if (disable_sni) object["disable_sni"] = true;
        // normalized, or a link's IDN sni and the stored ACE form would hash differently every refresh
        if (!server_name.isEmpty()) object["server_name"] = toAceHost(server_name);
        if (auto o = utls->ExportIdentity(); !o.isEmpty()) object["utls"] = o;
        if (auto o = ech->ExportIdentity(); !o.isEmpty()) object["ech"] = o;
        if (auto o = reality->ExportIdentity(); !o.isEmpty()) object["reality"] = o;
        return object;
    }
    BuildResult TLS::Build()
    {
        QJsonObject object;
        if (!enabled) return {};
        object["enabled"] = enabled;
        if (disable_sni) object["disable_sni"] = disable_sni;
        if (!server_name.isEmpty()) object["server_name"] = toAceHost(server_name);
        if (insecure || Configs::dataManager->settingsRepo->skip_cert) object["insecure"] = true;
        if (!alpn.isEmpty()) {
            object["alpn"] = QListStr2QJsonArray(alpn);
        }
        if (!min_version.isEmpty()) object["min_version"] = min_version;
        if (!max_version.isEmpty()) object["max_version"] = max_version;
        if (!cipher_suites.isEmpty()) {
            object["cipher_suites"] = QListStr2QJsonArray(cipher_suites);
        }
        if (!curve_preferences.isEmpty()) {
            object["curve_preferences"] = QListStr2QJsonArray(curve_preferences);
        }
        if (!certificate.isEmpty()) {
            object["certificate"] = QListStr2QJsonArray(certificate);
        }
        if (!certificate_path.isEmpty()) object["certificate_path"] = certificate_path;
        if (!certificate_sha256.isEmpty()) {
            object["certificate_sha256"] = QListStr2QJsonArray(certificate_sha256);
        }
        if (!certificate_public_key_sha256.isEmpty()) {
            object["certificate_public_key_sha256"] = QListStr2QJsonArray(certificate_public_key_sha256);
        }
        if (!client_certificate.isEmpty()) object["client_certificate"] = QListStr2QJsonArray(client_certificate);
        if (!client_certificate_path.isEmpty()) object["client_certificate_path"] = client_certificate_path;
        if (!client_key.isEmpty()) {
            object["client_key"] = QListStr2QJsonArray(client_key);
        }
        if (!client_key_path.isEmpty()) object["client_key_path"] = client_key_path;
        // The "custom" fragment implementation is emitted at the dialer level in outbound::Build() instead.
        if (FragmentEffectivelyOn() && Configs::dataManager->settingsRepo->fragment_implementation != "custom") {
            object["fragment"] = true;
            if (!fragment_fallback_delay.isEmpty()) object["fragment_fallback_delay"] = fragment_fallback_delay;
        }
        if (record_fragment) object["record_fragment"] = record_fragment;
        const auto spoofPreset = Configs::dataManager->settingsRepo->tls_spoof.trimmed();
        // The global preset stays off where the core would refuse it; an empty server_name means the address, unknown here.
        const bool presetRefused = reality->enabled || !reality->public_key.isEmpty() || disable_sni ||
                                   (!server_name.isEmpty() && !Checks::SpoofServerName(spoofPreset, toAceHost(server_name), false).isEmpty());
        if (SpoofEffectivelyOn() && (!spoof.isEmpty() || !presetRefused)) {
            if (const auto sni = spoof.isEmpty() ? spoofPreset : spoof; !sni.isEmpty()) {
                object["spoof"] = sni;
                const auto method = spoof_method.isEmpty() ? Configs::dataManager->settingsRepo->tls_spoof_method.trimmed() : spoof_method;
                if (!method.isEmpty()) object["spoof_method"] = method;
            }
        }
        if (TlsTricksEffectivelyOn()) object["tls_tricks"] = QJsonObject{{"mixedcase_sni", true}};
        if (auto obj = ech->Build().object;!obj.isEmpty()) object["ech"] = obj;
        if (auto obj = utls->Build().object;!obj.isEmpty()) {
            object["utls"] = obj;
        } else if (reality->enabled) {
            object["utls"] = QJsonObject{
                    {"enabled", true},
                    {"fingerprint", "random"},
                };
        }
        const auto realityResult = reality->Build();
        if (!realityResult.object.isEmpty()) object["reality"] = realityResult.object;
        return {object, realityResult.error};
    }

    bool TLS::FragmentEffectivelyOn()
    {
        if (fragment) return true;
        if (fragment_unspecified) return Configs::dataManager->settingsRepo->fragment_default_on;
        return false;
    }

    bool TLS::SpoofEffectivelyOn()
    {
        if (!spoof_unspecified) return spoof_enabled;
        // a profile that carries its own SNI predates the tri-state and stays on
        if (!spoof.isEmpty()) return true;
        return Configs::dataManager->settingsRepo->tls_spoof_default_on;
    }

    bool TLS::TlsTricksEffectivelyOn()
    {
        if (tls_tricks) return true;
        if (tls_tricks_unspecified) return Configs::dataManager->settingsRepo->tls_tricks_default_on;
        return false;
    }

    void TLS::Validate(Validation::Issues& issues, const QString& serverAddress, bool limited)
    {
        using Validation::Severity;
        tlsAdd(issues, Severity::Warning, "tls.server_name", Checks::ServerName(server_name), server_name);
        tlsAdd(issues, Severity::Error, "tls.certificate", Checks::PemCertificates(certificate));
        if (ech->enabled) {
            tlsAdd(issues, Severity::Error, "tls.ech.config", Checks::EchConfig(ech->config));
            tlsAdd(issues, Severity::Warning, "tls.ech.query_server_name", Validation::Host(ech->serverName, Validation::Empty::Allowed), ech->serverName);
        }
        if (limited) return;

        tlsAdd(issues, Severity::Error, "tls.min_version", Checks::TlsVersion(min_version), min_version);
        tlsAdd(issues, Severity::Error, "tls.max_version", Checks::TlsVersion(max_version), max_version);
        tlsAdd(issues, Severity::Error, "tls.max_version", Checks::TlsVersionOrder(min_version, max_version));
        tlsAdd(issues, Severity::Error, "tls.cipher_suites", Checks::SingBoxCipherSuites(cipher_suites.join(',')));
        tlsAdd(issues, Severity::Error, "tls.curve_preferences", Validation::EachItem(curve_preferences.join(','), Validation::Split::Comma, Checks::TlsCurve));
        if (utls->supported && utls->enabled) tlsAdd(issues, Severity::Error, "tls.utls.fingerprint", Checks::SingBoxFingerprint(utls->fingerPrint), utls->fingerPrint);

        tlsAdd(issues, Severity::Error, "tls.certificate_sha256", Checks::CertificatePins(certificate_sha256));
        tlsAdd(issues, Severity::Error, "tls.certificate_public_key_sha256", Checks::CertificatePins(certificate_public_key_sha256));
        tlsAdd(issues, Severity::Error, "tls.certificate_sha256",
               Checks::PinsWithCertificate(tlsHasText(certificate_sha256) || tlsHasText(certificate_public_key_sha256),
                                           tlsHasText(certificate) || !certificate_path.isEmpty()));
        tlsAdd(issues, Severity::Error, "tls.client_certificate", Checks::PemCertificates(client_certificate));
        tlsAdd(issues, Severity::Error, "tls.client_key", Checks::PemPrivateKey(client_key));
        tlsAdd(issues, Severity::Error, "tls.client_key",
               Checks::ClientCertificatePair(tlsHasText(client_certificate) || !client_certificate_path.isEmpty(),
                                             tlsHasText(client_key) || !client_key_path.isEmpty()));

        const bool tlsFragment = FragmentEffectivelyOn() && Configs::dataManager->settingsRepo->fragment_implementation != "custom";
        if (tlsFragment) {
            tlsAdd(issues, Severity::Error, "tls.fragment_fallback_delay", Validation::Duration(fragment_fallback_delay), fragment_fallback_delay);
        }

        const auto ownSpoof = Checks::OwnSpoofOn(spoof, getSpoofState()) ? spoof.trimmed() : QString();
        if (!ownSpoof.isEmpty()) tlsAdd(issues, Severity::Error, "tls.spoof_method", Checks::SpoofMethod(spoof_method), spoof_method);
        // Reality::Build's condition: a key alone turns it on.
        const bool realityOn = reality->enabled || !reality->public_key.isEmpty();
        if (realityOn) {
            tlsAdd(issues, Severity::Error, "tls.reality.public_key", Checks::RealityPublicKey(reality->public_key, true));
            tlsAdd(issues, Severity::Error, "tls.reality.short_id", Checks::RealityShortId(reality->short_id));
            tlsAdd(issues, Severity::Error, "tls.reality", Checks::RealityConflict(ech->enabled, disable_sni, insecure, !ownSpoof.isEmpty()));
        } else {
            tlsAdd(issues, Severity::Error, "tls.spoof", Checks::SpoofServerName(ownSpoof, server_name.isEmpty() ? serverAddress : server_name, disable_sni), ownSpoof);
        }
        const bool utlsOn = realityOn || !utls->Build().object.isEmpty();
        tlsAdd(issues, Severity::Warning, "tls.spoof", Checks::SpoofIgnored(!ownSpoof.isEmpty(), utlsOn, tlsFragment, record_fragment));
    }
}


