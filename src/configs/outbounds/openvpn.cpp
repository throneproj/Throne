#include "include/configs/outbounds/openvpn.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QRegularExpression>
#include <include/global/Utils.hpp>

#include <algorithm>
#include <optional>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"
#include "include/configs/sub/vpnFileImport.hpp"
#include "include/global/OtpPlaceholder.hpp"
#include "include/global/VpnCredentialOverride.hpp"

namespace Configs {
    namespace {
        // sing-box listables accept a bare string as well as an array of lines.
        QStringList ovpnListable(const QJsonValue& value)
        {
            if (value.isString()) return value.toString().split("\n", Qt::SkipEmptyParts);
            return QJsonArray2QListString(value.toArray());
        }

        const QStringList& ovpnCbcCiphers()
        {
            static const QStringList names = {
                "BF-CBC", "DES-CBC", "DES-EDE-CBC", "DES-EDE3-CBC", "CAST5-CBC", "AES-128-CBC", "AES-192-CBC", "AES-256-CBC",
                "ARIA-128-CBC", "ARIA-192-CBC", "ARIA-256-CBC", "CAMELLIA-128-CBC", "CAMELLIA-192-CBC", "CAMELLIA-256-CBC",
                "SEED-CBC", "SM4-CBC",
            };
            return names;
        }

        // sing-openvpn staticCipherKeySize.
        bool ovpnStaticCipher(const QString& name)
        {
            return name == "NONE" || ovpnCbcCiphers().contains(name);
        }

        // sing-openvpn validateDataCipherName: the static ciphers, the AEADs and the -CFB/-OFB form of a CBC cipher.
        bool ovpnDataCipher(const QString& name)
        {
            static const QStringList aead = {"AES-128-GCM", "AES-192-GCM", "AES-256-GCM", "CHACHA20-POLY1305"};
            if (ovpnStaticCipher(name) || aead.contains(name)) return true;
            if (name.endsWith("-CFB") || name.endsWith("-OFB")) return ovpnCbcCiphers().contains(name.chopped(4) + "-CBC");
            return false;
        }

        QStringList ovpnUpper(const QStringList& names)
        {
            QStringList upper;
            for (const auto& name : names) upper << name.trimmed().toUpper();
            return upper;
        }

        std::optional<qint64> ovpnDurationNs(const QString& text)
        {
            const auto trimmed = text.trimmed();
            if (trimmed.isEmpty()) return 0;
            return Validation::ParseDurationNs(trimmed);
        }

        // An unparsable value counts as set: the field's own rule reports it.
        bool ovpnDurationSet(const QString& text)
        {
            return ovpnDurationNs(text).value_or(1) != 0;
        }

        // sing-openvpn validateDurationOption, plus validatePingDurationOption's whole seconds.
        QString ovpnDurationRule(const QString& text, bool wholeSeconds)
        {
            Validation::DurationRule rule;
            rule.wholeSeconds = wholeSeconds;
            if (auto message = Validation::Duration(text, rule); !message.isEmpty()) return message;
            const auto ns = ovpnDurationNs(text).value_or(0);
            if (ns > 0 && ns < 1000000000LL) return QCoreApplication::translate("Validation", "Must be 0 or at least 1s.");
            return {};
        }

        bool ovpnStaticMode(const openvpn& profile)
        {
            return profile.mode == "static_key";
        }

        bool ovpnPullFiltersSet(const openvpn& profile)
        {
            for (const auto& filter : profile.pull_filters) {
                if (!filter->action.isEmpty() || !filter->text.isEmpty()) return true;
            }
            return false;
        }

        // Import only: an optional value the core would refuse is dropped with a warning; DB loads and clones keep it.
        bool ovpnKeep(const QString& field, const QString& message, const QString& value)
        {
            return !ParseDiag::Active() || AcceptOptional(field, message, value);
        }

        void ovpnReadChecked(const QJsonObject& object, const QString& key, const QString& field, QString& target,
                             const Validation::Rule& rule)
        {
            if (!object.contains(key)) return;
            const auto value = object[key].toString().trimmed();
            if (ovpnKeep(field, rule(value), value)) target = value;
        }

        QStringList ovpnKeepItems(const QString& field, const QStringList& items, const Validation::Rule& rule)
        {
            QStringList kept;
            for (const auto& raw : items) {
                const auto item = raw.trimmed();
                if (!item.isEmpty() && ovpnKeep(field, rule(item), item)) kept << item;
            }
            return kept;
        }
    }

    namespace Checks {
        QString OpenVPNCipherName(const QString& name)
        {
            const auto upper = name.trimmed().toUpper();
            if (upper.isEmpty() || ovpnDataCipher(upper)) return {};
            if (upper.contains(':')) return QCoreApplication::translate("Validation", "Separate the ciphers with commas, not colons.");
            return QCoreApplication::translate("Validation", "Not an OpenVPN cipher name. Use a name such as AES-256-GCM, CHACHA20-POLY1305 or AES-256-CBC.");
        }

        QString OpenVPNDigest(const QString& name)
        {
            return Validation::OneOf(name.trimmed().toUpper(), {"SHA1", "SHA224", "SHA256", "SHA384", "SHA512", "RIPEMD160", "MD5", "NONE"});
        }

        QString OpenVPNNormalizeFingerprint(const QString& line)
        {
            QString hex;
            for (const QChar c : line) {
                if (c != ':' && !c.isSpace()) hex += c.toLower();
            }
            return hex;
        }

        QString OpenVPNFingerprint(const QString& line)
        {
            const auto hex = OpenVPNNormalizeFingerprint(line);
            if (hex.isEmpty()) return {};
            static const QRegularExpression sha256(QStringLiteral("^[0-9a-f]{64}$"));
            if (sha256.match(hex).hasMatch()) return {};
            return QCoreApplication::translate("Validation", "Use the certificate's SHA-256 fingerprint: 64 hexadecimal characters, colons allowed.");
        }

        // strconv.ParseUint(value, 16, 64), as --remote-cert-ku reads it.
        QString OpenVPNKeyUsage(const QString& item)
        {
            if (Validation::Hex(item, 1, 16, false).isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Use a hexadecimal key usage value such as a0 or 88.");
        }

        // sing-openvpn parseRemoteCertExtKeyUsage: an OpenSSL purpose name or a dotted OID.
        QString OpenVPNExtendedKeyUsage(const QString& text)
        {
            const auto value = text.trimmed();
            if (value.isEmpty()) return {};
            static const QStringList names = {
                "server", "client", "TLS Web Server Authentication", "TLS Web Client Authentication", "Code Signing",
                "E-mail Protection", "IPSec End System", "IPSec Tunnel", "IPSec User", "Time Stamping", "OCSP Signing",
                "Any Extended Key Usage", "Microsoft Server Gated Crypto", "Netscape Server Gated Crypto",
                "Microsoft Commercial Code Signing", "Microsoft Individual Code Signing",
            };
            if (names.contains(value)) return {};
            const auto parts = value.split('.');
            bool oid = parts.size() >= 2;
            for (const auto& part : parts) {
                if (!oid) break;
                oid = !part.isEmpty() && part.size() <= 9 && std::all_of(part.begin(), part.end(), [](QChar c) { return c >= '0' && c <= '9'; });
            }
            if (oid) return {};
            return QCoreApplication::translate("Validation", "Use a name such as TLS Web Server Authentication or an OID such as 1.3.6.1.5.5.7.3.1.");
        }

        QString OpenVPNNormalizeTlsCipher(const QString& text)
        {
            auto tokens = text.trimmed().split(':');
            for (auto& token : tokens) {
                if (token.startsWith("TLS-", Qt::CaseInsensitive) && token.contains("-WITH-", Qt::CaseInsensitive)) {
                    token = token.toUpper().replace('-', '_');
                }
            }
            return tokens.join(':');
        }

        // sing-openvpn parseTLSCipherSuites: its OpenSSL table or Go's name for the same suites.
        QString OpenVPNTlsCipher(const QString& text)
        {
            const auto value = OpenVPNNormalizeTlsCipher(text);
            if (value.isEmpty()) return {};
            static const QStringList known = {
                "RC4-SHA", "DES-CBC3-SHA", "AES128-SHA", "AES256-SHA", "AES128-SHA256", "AES128-GCM-SHA256",
                "AES256-GCM-SHA384", "ECDHE-ECDSA-AES128-SHA", "ECDHE-ECDSA-AES256-SHA", "ECDHE-ECDSA-RC4-SHA",
                "ECDHE-RSA-RC4-SHA", "ECDHE-RSA-DES-CBC3-SHA", "ECDHE-RSA-AES128-SHA", "ECDHE-RSA-AES256-SHA",
                "ECDHE-ECDSA-AES128-SHA256", "ECDHE-RSA-AES128-SHA256", "ECDHE-ECDSA-AES128-GCM-SHA256",
                "ECDHE-RSA-AES128-GCM-SHA256", "ECDHE-ECDSA-AES256-GCM-SHA384", "ECDHE-RSA-AES256-GCM-SHA384",
                "ECDHE-ECDSA-CHACHA20-POLY1305", "ECDHE-RSA-CHACHA20-POLY1305",
                "TLS_RSA_WITH_RC4_128_SHA", "TLS_RSA_WITH_3DES_EDE_CBC_SHA", "TLS_RSA_WITH_AES_128_CBC_SHA",
                "TLS_RSA_WITH_AES_256_CBC_SHA", "TLS_RSA_WITH_AES_128_CBC_SHA256", "TLS_RSA_WITH_AES_128_GCM_SHA256",
                "TLS_RSA_WITH_AES_256_GCM_SHA384", "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA", "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA",
                "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA", "TLS_ECDHE_RSA_WITH_RC4_128_SHA", "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA",
                "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA", "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",
                "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256", "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256",
                "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256", "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
                "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384", "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
                "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256", "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256",
            };
            for (const auto& token : value.split(':')) {
                if (token.isEmpty()) return QCoreApplication::translate("Validation", "Remove the empty entry between two colons.");
                if (!known.contains(token)) {
                    return QCoreApplication::translate("Validation", "\"%1\" is not a TLS 1.2 cipher suite the core supports, for example ECDHE-RSA-AES256-GCM-SHA384.").arg(token);
                }
            }
            return {};
        }

        // sing-openvpn parseTLSGroups, after the upper-casing Build() applies.
        QString OpenVPNTlsGroups(const QString& text)
        {
            const auto value = text.trimmed().toUpper();
            if (value.isEmpty()) return {};
            static const QStringList known = {"X25519", "CURVE25519", "SECP256R1", "PRIME256V1", "P-256", "NISTP256",
                                              "SECP384R1", "P-384", "NISTP384", "SECP521R1", "P-521", "NISTP521"};
            for (const auto& token : value.split(':')) {
                if (token.isEmpty()) return QCoreApplication::translate("Validation", "Remove the empty entry between two colons.");
                if (!known.contains(token)) {
                    return QCoreApplication::translate("Validation", "\"%1\" is not a TLS group the core supports. Use X25519, P-256, P-384 or P-521, separated by colons.").arg(token);
                }
            }
            return {};
        }

        QString OpenVPNInterval(const QString& text)
        {
            return ovpnDurationRule(text, false);
        }

        QString OpenVPNPingInterval(const QString& text)
        {
            return ovpnDurationRule(text, true);
        }

        QString OpenVPNReplayWindowTime(const QString& text)
        {
            Validation::DurationRule rule;
            rule.wholeSeconds = true;
            rule.maxNs = 600LL * 1000000000LL;
            return Validation::Duration(text, rule);
        }

        QString OpenVPNFragment(const QString& text)
        {
            if (auto message = Validation::Int(text, 0, 65535); !message.isEmpty()) return message;
            const auto value = Validation::ToInt(text).value_or(0);
            if (value > 0 && value < 68) return QCoreApplication::translate("Validation", "Must be 0 (off) or at least 68.");
            return {};
        }

        QString OpenVPNReplayWindow(const QString& text)
        {
            return Validation::Int(text, 0, 65536);
        }

        // sing-openvpn resolveClientRemotes refuses whitespace and brackets.
        QString OpenVPNRemoteHost(const QString& text)
        {
            const auto host = text.trimmed();
            if (host.startsWith('[') || host.endsWith(']')) return QCoreApplication::translate("Validation", "Write the IPv6 address without brackets.");
            return Validation::Host(host);
        }

        QString OpenVPNRemotePort(const QString& text)
        {
            return Validation::Port(text);
        }

        QStringList OpenVPNModeConflicts(const openvpn& profile)
        {
            QStringList keys;
            const auto add = [&keys](bool set, const char* key) {
                if (set) keys << QString::fromLatin1(key);
            };
            if (!ovpnStaticMode(profile)) {
                add(!profile.static_key.isEmpty(), "static_key");
                add(!profile.static_key_path.isEmpty(), "static_key_path");
                add(!profile.key_direction.isEmpty(), "key_direction");
                add(!profile.cipher.isEmpty(), "cipher");
                return keys;
            }
            // sing-box buildStaticKeyClientOptions and sing-openvpn validateImplementedClientOptions.
            const auto& tls = *profile.tls;
            add(!profile.username.isEmpty(), "username");
            add(!profile.password.isEmpty(), "password");
            add(!profile.auth_retry.isEmpty() && profile.auth_retry != "none", "auth_retry");
            add(!profile.static_challenge.isEmpty(), "static_challenge");
            add(profile.static_challenge_echo, "static_challenge_echo");
            add(profile.route_no_pull, "route_no_pull");
            add(ovpnPullFiltersSet(profile), "pull_filters");
            add(ovpnDurationSet(profile.renegotiate_interval), "renegotiate_interval");
            add(profile.renegotiate_disabled, "renegotiate_disabled");
            add(profile.renegotiate_bytes > 0, "renegotiate_bytes");
            add(profile.renegotiate_packets > 0, "renegotiate_packets");
            add(ovpnDurationSet(profile.tls_timeout), "tls_timeout");
            add(ovpnDurationSet(profile.handshake_window), "handshake_window");
            add(!profile.data_ciphers.isEmpty(), "data_ciphers");
            add(!profile.data_ciphers_fallback.isEmpty(), "data_ciphers_fallback");
            add(!tls.server_name.isEmpty(), "tls.server_name");
            add(!tls.server_name_type.isEmpty(), "tls.server_name_type");
            add(!tls.certificate.isEmpty(), "tls.certificate");
            add(!tls.certificate_path.isEmpty(), "tls.certificate_path");
            add(!tls.client_certificate.isEmpty(), "tls.client_certificate");
            add(!tls.client_certificate_path.isEmpty(), "tls.client_certificate_path");
            add(!tls.client_key.isEmpty(), "tls.client_key");
            add(!tls.client_key_path.isEmpty(), "tls.client_key_path");
            add(!tls.peer_fingerprint.isEmpty(), "tls.peer_fingerprint");
            add(!tls.crl_path.isEmpty(), "tls.crl_path");
            add(!tls.remote_certificate_ku.isEmpty(), "tls.remote_certificate_ku");
            add(!tls.remote_certificate_eku.isEmpty(), "tls.remote_certificate_eku");
            add(!tls.remote_certificate_tls.isEmpty(), "tls.remote_certificate_tls");
            add(!tls.certificate_profile.isEmpty(), "tls.certificate_profile");
            add(!tls.ns_certificate_type.isEmpty(), "tls.ns_certificate_type");
            add(!tls.version_min.isEmpty(), "tls.version_min");
            add(!tls.version_max.isEmpty(), "tls.version_max");
            add(!tls.cipher.isEmpty(), "tls.cipher");
            add(!tls.groups.isEmpty(), "tls.groups");
            add(!tls.control_wrap->type.isEmpty(), "tls.control_wrap");
            return keys;
        }

        QString OpenVPNModeConflict(const openvpn& profile, const QString& key)
        {
            if (!OpenVPNModeConflicts(profile).contains(key)) return {};
            if (ovpnStaticMode(profile)) {
                return QCoreApplication::translate("Validation", "Static key mode does not use this setting; clear it or switch the mode to TLS.");
            }
            if (key == "cipher") {
                return QCoreApplication::translate("Validation", "Only static key mode uses Cipher; in TLS mode list the cipher under Data Ciphers.");
            }
            if (key == "key_direction") {
                return QCoreApplication::translate("Validation", "Key direction applies only to static key mode; for tls_auth set the control channel wrap direction.");
            }
            return QCoreApplication::translate("Validation", "Only static key mode uses a static key; clear it or switch the mode to static_key.");
        }

        QString OpenVPNStaticKeyMissing(const openvpn& profile)
        {
            if (!ovpnStaticMode(profile) || !profile.static_key.isEmpty() || !profile.static_key_path.isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Static key mode needs the static key, pasted or as a file.");
        }

        QString OpenVPNStaticCipher(const openvpn& profile)
        {
            if (!ovpnStaticMode(profile)) return {};
            const auto cipher = profile.cipher.trimmed().toUpper();
            if (cipher.isEmpty()) return QCoreApplication::translate("Validation", "Static key mode needs a cipher, for example AES-256-CBC.");
            if (ovpnDataCipher(cipher) && !ovpnStaticCipher(cipher)) {
                return QCoreApplication::translate("Validation", "Static key mode supports only the CBC ciphers, such as AES-256-CBC, and NONE.");
            }
            return {};
        }

        QString OpenVPNStaticAddress(const openvpn& profile)
        {
            if (!ovpnStaticMode(profile)) return {};
            if (profile.address.isEmpty()) return QCoreApplication::translate("Validation", "Static key mode needs the tunnel address, for example 10.8.0.2/24.");
            bool v4 = false, v6 = false;
            for (const auto& item : profile.address) {
                // A malformed item is reported by its own rule.
                if (!Validation::Cidr(item).isEmpty()) return {};
                (item.contains(':') ? v6 : v4) = true;
            }
            const bool peer4 = !profile.peer_address.trimmed().isEmpty();
            const bool peer6 = !profile.peer_address_ipv6.trimmed().isEmpty();
            if (v4 && !peer4) return QCoreApplication::translate("Validation", "Add the IPv4 peer address for the IPv4 tunnel address.");
            if (v6 && !peer6) return QCoreApplication::translate("Validation", "Add the IPv6 peer address for the IPv6 tunnel address.");
            if (peer4 && !v4) return QCoreApplication::translate("Validation", "The IPv4 peer address needs an IPv4 tunnel address.");
            if (peer6 && !v6) return QCoreApplication::translate("Validation", "The IPv6 peer address needs an IPv6 tunnel address.");
            return {};
        }

        // sing-openvpn requireCAOrPeerFingerprint runs at session start, so a bare client pair still fails.
        QString OpenVPNTlsVerification(const openvpn& profile)
        {
            if (ovpnStaticMode(profile)) return {};
            const auto& tls = *profile.tls;
            if (!tls.certificate.isEmpty() || !tls.certificate_path.trimmed().isEmpty() || !tls.peer_fingerprint.isEmpty()) return {};
            return QCoreApplication::translate("Validation", "TLS mode needs the CA certificate or a peer fingerprint to verify the server.");
        }

        QString OpenVPNClientPair(const openvpn& profile)
        {
            if (ovpnStaticMode(profile)) return {};
            const auto& tls = *profile.tls;
            const bool certificate = !tls.client_certificate.isEmpty() || !tls.client_certificate_path.isEmpty();
            const bool key = !tls.client_key.isEmpty() || !tls.client_key_path.isEmpty();
            if (certificate == key) return {};
            return certificate ? QCoreApplication::translate("Validation", "Add the client key that belongs to this certificate.")
                               : QCoreApplication::translate("Validation", "Add the client certificate that belongs to this key.");
        }

        QString VpnMaterialConflict(const QStringList& inlineValue, const QString& path)
        {
            if (inlineValue.isEmpty() || path.trimmed().isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Set both pasted and as a file; keep only one of them.");
        }

        QString OpenVPNControlWrapKey(const openvpn& profile)
        {
            const auto& wrap = *profile.tls->control_wrap;
            const bool key = !wrap.key.isEmpty() || !wrap.key_path.trimmed().isEmpty();
            if (wrap.type.isEmpty()) {
                if (!key) return {};
                if (ovpnStaticMode(profile)) return QCoreApplication::translate("Validation", "Static key mode does not use the control channel wrap; clear its key.");
                return QCoreApplication::translate("Validation", "Pick the control channel wrap type for this key: tls_auth, tls_crypt or tls_crypt_v2.");
            }
            if (auto message = Validation::OneOf(wrap.type, {"tls_auth", "tls_crypt", "tls_crypt_v2"}); !message.isEmpty()) return message;
            if (!key) return QCoreApplication::translate("Validation", "The control channel wrap needs its key, pasted or as a file.");
            return {};
        }

        QString OpenVPNControlWrapDirection(const openvpn& profile)
        {
            const auto& wrap = *profile.tls->control_wrap;
            if (wrap.direction.isEmpty()) return {};
            if (auto message = Validation::OneOf(wrap.direction, {"server", "client"}); !message.isEmpty()) return message;
            if (wrap.type != "tls_auth") return QCoreApplication::translate("Validation", "The key direction applies only to tls_auth.");
            return {};
        }

        QString OpenVPNRemoteCertificate(const openvpn& profile)
        {
            const auto& tls = *profile.tls;
            if (tls.remote_certificate_eku.trimmed().isEmpty() || tls.remote_certificate_tls.isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Use either the remote certificate EKU or the remote certificate TLS role, not both.");
        }

        // sing-openvpn resolveTLSVersionBounds: an unset minimum is 1.2.
        QString OpenVPNTlsVersions(const openvpn& profile)
        {
            static const QStringList order = {"1.0", "1.1", "1.2", "1.3"};
            const auto& tls = *profile.tls;
            const auto min = tls.version_min.isEmpty() ? order.indexOf("1.2") : order.indexOf(tls.version_min);
            const auto max = order.indexOf(tls.version_max);
            if (min < 0 || max < 0 || max >= min) return {};
            return QCoreApplication::translate("Validation", "The maximum TLS version is below the minimum, which is 1.2 unless set.");
        }

        QString OpenVPNMssFix(const openvpn& profile)
        {
            if (profile.mss_fix_disabled && profile.mss_fix > 0) {
                return QCoreApplication::translate("Validation", "MSS clamping is disabled; clear the MSS fix value or enable it.");
            }
            if (profile.mss_fix_disabled && !profile.mss_fix_mode.isEmpty()) {
                return QCoreApplication::translate("Validation", "MSS clamping is disabled; clear the MSS fix mode or enable it.");
            }
            if (profile.mss_fix <= 0 && !profile.mss_fix_mode.isEmpty()) {
                return QCoreApplication::translate("Validation", "The MSS fix mode needs an MSS fix value.");
            }
            return {};
        }

        QString OpenVPNPingRestart(const openvpn& profile)
        {
            if (!profile.ping_restart_disabled || ovpnDurationNs(profile.ping_restart).value_or(0) <= 0) return {};
            return QCoreApplication::translate("Validation", "Ping restart is disabled; clear the restart time or enable it.");
        }

        QString OpenVPNRenegotiate(const openvpn& profile)
        {
            if (!profile.renegotiate_disabled || ovpnDurationNs(profile.renegotiate_interval).value_or(0) <= 0) return {};
            return QCoreApplication::translate("Validation", "Renegotiation is disabled; clear the interval or enable it.");
        }

        QString OtpPlaceholderUnbound(const QString& value, int otpProfileId)
        {
            if (otpProfileId >= 0 || !value.contains(kOtpPlaceholder)) return {};
            return QCoreApplication::translate("Validation", "%1 is replaced by a one-time code only when an authenticator entry is selected.")
                .arg(kOtpPlaceholder);
        }
    }

    bool OpenVPNRemote::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("server")) server = object["server"].toString();
        if (object.contains("server_port")) server_port = object["server_port"].toInt();
        if (object.contains("network")) network = object["network"].toString().trimmed();
        return true;
    }

    QJsonObject OpenVPNRemote::ExportToJson()
    {
        QJsonObject object;
        if (!server.isEmpty()) object["server"] = server;
        if (server_port > 0) object["server_port"] = server_port;
        if (!network.isEmpty()) object["network"] = network;
        return object;
    }

    BuildResult OpenVPNRemote::Build()
    {
        return {ExportToJson(), ""};
    }

    bool OpenVPNPullFilter::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("action")) action = object["action"].toString().trimmed();
        if (object.contains("text")) text = object["text"].toString();
        return true;
    }

    QJsonObject OpenVPNPullFilter::ExportToJson()
    {
        QJsonObject object;
        if (action.isEmpty() && text.isEmpty()) return object;
        object["action"] = action;
        object["text"] = text;
        return object;
    }

    BuildResult OpenVPNPullFilter::Build()
    {
        return {ExportToJson(), ""};
    }

    bool OpenVPNControlWrap::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("type")) type = object["type"].toString().trimmed();
        if (object.contains("key")) key = ovpnListable(object["key"]);
        if (object.contains("key_path")) key_path = object["key_path"].toString();
        if (object.contains("direction")) direction = object["direction"].toString().trimmed();
        return true;
    }

    QJsonObject OpenVPNControlWrap::ExportToJson()
    {
        QJsonObject object;
        if (type.isEmpty()) return object;
        object["type"] = type;
        if (!key.isEmpty()) object["key"] = QListStr2QJsonArray(key);
        if (!key_path.isEmpty()) object["key_path"] = key_path;
        if (!direction.isEmpty()) object["direction"] = direction;
        return object;
    }

    BuildResult OpenVPNControlWrap::Build()
    {
        return {ExportToJson(), ""};
    }

    bool OpenVPNTLS::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("server_name")) server_name = object["server_name"].toString();
        if (object.contains("server_name_type")) server_name_type = object["server_name_type"].toString().trimmed();
        if (object.contains("certificate")) certificate = ovpnListable(object["certificate"]);
        if (object.contains("certificate_path")) certificate_path = object["certificate_path"].toString();
        if (object.contains("client_certificate")) client_certificate = ovpnListable(object["client_certificate"]);
        if (object.contains("client_certificate_path")) client_certificate_path = object["client_certificate_path"].toString();
        if (object.contains("client_key")) client_key = ovpnListable(object["client_key"]);
        if (object.contains("client_key_path")) client_key_path = object["client_key_path"].toString();
        if (object.contains("peer_fingerprint")) peer_fingerprint = ovpnListable(object["peer_fingerprint"]);
        if (object.contains("crl_path")) crl_path = object["crl_path"].toString();
        if (object.contains("remote_certificate_ku")) remote_certificate_ku = ovpnListable(object["remote_certificate_ku"]);
        if (object.contains("remote_certificate_eku")) remote_certificate_eku = object["remote_certificate_eku"].toString();
        if (object.contains("remote_certificate_tls")) remote_certificate_tls = object["remote_certificate_tls"].toString().trimmed();
        if (object.contains("certificate_profile")) certificate_profile = object["certificate_profile"].toString().trimmed();
        if (object.contains("ns_certificate_type")) ns_certificate_type = object["ns_certificate_type"].toString().trimmed();
        if (object.contains("version_min")) version_min = object["version_min"].toString().trimmed();
        if (object.contains("version_max")) version_max = object["version_max"].toString().trimmed();
        // Without them the core's default suites and groups apply.
        ovpnReadChecked(object, "cipher", "tls.cipher", cipher, Checks::OpenVPNTlsCipher);
        ovpnReadChecked(object, "groups", "tls.groups", groups, Checks::OpenVPNTlsGroups);
        if (object.contains("control_wrap")) control_wrap->ParseFromJson(object["control_wrap"].toObject());
        return true;
    }

    QJsonObject OpenVPNTLS::ExportToJson()
    {
        QJsonObject object;
        if (!server_name.isEmpty()) object["server_name"] = server_name;
        if (!server_name_type.isEmpty()) object["server_name_type"] = server_name_type;
        if (!certificate.isEmpty()) object["certificate"] = QListStr2QJsonArray(certificate);
        if (!certificate_path.isEmpty()) object["certificate_path"] = certificate_path;
        if (!client_certificate.isEmpty()) object["client_certificate"] = QListStr2QJsonArray(client_certificate);
        if (!client_certificate_path.isEmpty()) object["client_certificate_path"] = client_certificate_path;
        if (!client_key.isEmpty()) object["client_key"] = QListStr2QJsonArray(client_key);
        if (!client_key_path.isEmpty()) object["client_key_path"] = client_key_path;
        if (!peer_fingerprint.isEmpty()) object["peer_fingerprint"] = QListStr2QJsonArray(peer_fingerprint);
        if (!crl_path.isEmpty()) object["crl_path"] = crl_path;
        if (!remote_certificate_ku.isEmpty()) object["remote_certificate_ku"] = QListStr2QJsonArray(remote_certificate_ku);
        if (!remote_certificate_eku.isEmpty()) object["remote_certificate_eku"] = remote_certificate_eku;
        if (!remote_certificate_tls.isEmpty()) object["remote_certificate_tls"] = remote_certificate_tls;
        if (!certificate_profile.isEmpty()) object["certificate_profile"] = certificate_profile;
        if (!ns_certificate_type.isEmpty()) object["ns_certificate_type"] = ns_certificate_type;
        if (!version_min.isEmpty()) object["version_min"] = version_min;
        if (!version_max.isEmpty()) object["version_max"] = version_max;
        if (!cipher.isEmpty()) object["cipher"] = cipher;
        if (!groups.isEmpty()) object["groups"] = groups;
        if (auto wrap = control_wrap->ExportToJson(); !wrap.isEmpty()) object["control_wrap"] = wrap;
        return object;
    }

    BuildResult OpenVPNTLS::Build()
    {
        auto object = ExportToJson();
        if (!peer_fingerprint.isEmpty()) {
            QStringList fingerprints;
            for (const auto& line : peer_fingerprint) {
                if (const auto hex = Checks::OpenVPNNormalizeFingerprint(line); !hex.isEmpty()) fingerprints << hex;
            }
            if (fingerprints.isEmpty()) object.remove("peer_fingerprint");
            else object["peer_fingerprint"] = QListStr2QJsonArray(fingerprints);
        }
        if (!cipher.isEmpty()) object["cipher"] = Checks::OpenVPNNormalizeTlsCipher(cipher);
        if (!groups.isEmpty()) object["groups"] = groups.trimmed().toUpper();
        return {object, ""};
    }

    bool openvpn::ParseFromLink(const QString& link)
    {
        return ParseOvpnConfig(link, *this);
    }

    bool openvpn::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        auto type = object["type"].toString();
        if (type != "openvpn" && type != "openvpn-client") return false;
        outbound::ParseFromJson(object);

        if (object.contains("mode")) mode = object["mode"].toString().trimmed();
        if (object.contains("network")) network = object["network"].toString().trimmed();
        if (object.contains("servers")) {
            servers.clear();
            for (const auto& item : object["servers"].toArray()) {
                auto remote = std::make_shared<OpenVPNRemote>();
                if (remote->ParseFromJson(item.toObject())) servers.append(remote);
            }
        }
        if (object.contains("remote_random")) remote_random = object["remote_random"].toBool();
        if (object.contains("address")) address = ovpnListable(object["address"]);
        if (object.contains("peer_address")) peer_address = object["peer_address"].toString();
        if (object.contains("peer_address_ipv6")) peer_address_ipv6 = object["peer_address_ipv6"].toString();
        if (object.contains("topology")) topology = object["topology"].toString().trimmed();
        if (object.contains("username")) username = object["username"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("auth_retry")) auth_retry = object["auth_retry"].toString().trimmed();
        if (object.contains("static_challenge")) static_challenge = object["static_challenge"].toString();
        if (object.contains("static_challenge_echo")) static_challenge_echo = object["static_challenge_echo"].toBool();
        if (object.contains("static_key")) static_key = ovpnListable(object["static_key"]);
        if (object.contains("static_key_path")) static_key_path = object["static_key_path"].toString();
        if (object.contains("key_direction")) key_direction = object["key_direction"].toString().trimmed();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        if (object.contains("cipher")) cipher = object["cipher"].toString();
        // Unsupported offers drop out of the negotiation; the static key cipher and auth have no such fallback.
        if (object.contains("data_ciphers")) data_ciphers = ovpnKeepItems("data_ciphers", ovpnListable(object["data_ciphers"]), Checks::OpenVPNCipherName);
        ovpnReadChecked(object, "data_ciphers_fallback", "data_ciphers_fallback", data_ciphers_fallback, Checks::OpenVPNCipherName);
        if (object.contains("auth")) auth = object["auth"].toString();
        if (object.contains("mss_fix")) mss_fix = object["mss_fix"].toInt();
        if (object.contains("mss_fix_disabled")) mss_fix_disabled = object["mss_fix_disabled"].toBool();
        if (object.contains("mss_fix_mode")) mss_fix_mode = object["mss_fix_mode"].toString().trimmed();
        if (object.contains("fragment")) {
            const auto value = object["fragment"].toInt();
            if (ovpnKeep("fragment", Checks::OpenVPNFragment(QString::number(value)), QString::number(value))) fragment = value;
        }
        if (object.contains("replay_window")) {
            const auto value = object["replay_window"].toInt();
            if (ovpnKeep("replay_window", Checks::OpenVPNReplayWindow(QString::number(value)), QString::number(value))) replay_window = value;
        }
        ovpnReadChecked(object, "replay_window_time", "replay_window_time", replay_window_time, Checks::OpenVPNReplayWindowTime);
        if (object.contains("compression")) compression = object["compression"].toString();
        if (object.contains("compression_lzo")) compression_lzo = object["compression_lzo"].toString();
        if (object.contains("allow_compression")) allow_compression = object["allow_compression"].toString();
        if (object.contains("route_no_pull")) route_no_pull = object["route_no_pull"].toBool();
        if (object.contains("pull_filters")) {
            pull_filters.clear();
            for (const auto& item : object["pull_filters"].toArray()) {
                auto filter = std::make_shared<OpenVPNPullFilter>();
                if (filter->ParseFromJson(item.toObject())) pull_filters.append(filter);
            }
        }
        if (object.contains("routes")) {
            routes = ovpnKeepItems("routes", ovpnListable(object["routes"]), [](const QString& item) { return Validation::Cidr(item); });
        }
        ovpnReadChecked(object, "route_gateway", "route_gateway", route_gateway, [](const QString& text) {
            return Validation::Ip(text, Validation::Family::Any, Validation::Empty::Allowed);
        });
        if (object.contains("route_metric")) route_metric = object["route_metric"].toInt();
        if (object.contains("redirect_gateway")) redirect_gateway = object["redirect_gateway"].toBool();
        if (object.contains("redirect_gateway_flags")) redirect_gateway_flags = ovpnListable(object["redirect_gateway_flags"]);
        if (object.contains("redirect_private")) redirect_private = object["redirect_private"].toBool();
        if (object.contains("block_ipv6")) block_ipv6 = object["block_ipv6"].toBool();
        ovpnReadChecked(object, "ping_interval", "ping_interval", ping_interval, Checks::OpenVPNPingInterval);
        ovpnReadChecked(object, "ping_restart", "ping_restart", ping_restart, Checks::OpenVPNPingInterval);
        if (object.contains("ping_restart_disabled")) ping_restart_disabled = object["ping_restart_disabled"].toBool();
        ovpnReadChecked(object, "renegotiate_interval", "renegotiate_interval", renegotiate_interval, Checks::OpenVPNInterval);
        if (object.contains("renegotiate_disabled")) renegotiate_disabled = object["renegotiate_disabled"].toBool();
        if (object.contains("renegotiate_bytes")) renegotiate_bytes = object["renegotiate_bytes"].toInteger();
        if (object.contains("renegotiate_packets")) renegotiate_packets = object["renegotiate_packets"].toInteger();
        ovpnReadChecked(object, "tls_timeout", "tls_timeout", tls_timeout, Checks::OpenVPNInterval);
        ovpnReadChecked(object, "handshake_window", "handshake_window", handshake_window, Checks::OpenVPNInterval);
        if (object.contains("explicit_exit_notify")) explicit_exit_notify = object["explicit_exit_notify"].toInt();
        if (object.contains("system")) system = object["system"].toBool();
        if (object.contains("name")) interface_name = object["name"].toString();
        if (object.contains("mtu")) mtu = object["mtu"].toInt();
        // The core also takes a bare number of seconds here; Throne stores and emits the duration string.
        if (object.contains("udp_timeout")) {
            const auto value = object["udp_timeout"];
            if (value.isDouble() && value.toInteger() > 0) udp_timeout = QString::number(value.toInteger()) + "s";
            else ovpnReadChecked(object, "udp_timeout", "udp_timeout", udp_timeout, [](const QString& text) { return Validation::Duration(text); });
        }
        if (object.contains("udp_mapping")) udp_mapping = object["udp_mapping"].toString();
        if (object.contains("udp_filtering")) udp_filtering = object["udp_filtering"].toString();
        if (object.contains("udp_nat_max")) udp_nat_max = object["udp_nat_max"].toInt();

        if (object.contains("otp_profile_id")) otp_profile_id = object["otp_profile_id"].toInt();
        if (object.contains("only_advertised_routes")) only_advertised_routes = object["only_advertised_routes"].toBool();
        // Profiles saved before the mode existed carry the two checkboxes it replaced.
        if (object.contains("tunnel_dns")) tunnel_dns = object["tunnel_dns"].toString();
        else if (object["block_outside_dns"].toBool()) tunnel_dns = kTunnelDnsStrict;
        else if (object.contains("use_tunnel_dns") && !object["use_tunnel_dns"].toBool()) tunnel_dns = kTunnelDnsNone;
        return true;
    }

    QJsonObject openvpn::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "openvpn";
        mergeJsonObjects(object, outbound::ExportToJson());

        if (!mode.isEmpty()) object["mode"] = mode;
        if (!network.isEmpty()) object["network"] = network;
        if (!servers.isEmpty()) {
            QJsonArray remotes;
            for (const auto& remote : servers) {
                if (auto item = remote->ExportToJson(); !item.isEmpty()) remotes.append(item);
            }
            if (!remotes.isEmpty()) object["servers"] = remotes;
        }
        if (remote_random) object["remote_random"] = true;
        if (!address.isEmpty()) object["address"] = QListStr2QJsonArray(address);
        if (!peer_address.isEmpty()) object["peer_address"] = peer_address;
        if (!peer_address_ipv6.isEmpty()) object["peer_address_ipv6"] = peer_address_ipv6;
        if (!topology.isEmpty()) object["topology"] = topology;
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (!auth_retry.isEmpty()) object["auth_retry"] = auth_retry;
        if (!static_challenge.isEmpty()) object["static_challenge"] = static_challenge;
        if (static_challenge_echo) object["static_challenge_echo"] = true;
        if (!static_key.isEmpty()) object["static_key"] = QListStr2QJsonArray(static_key);
        if (!static_key_path.isEmpty()) object["static_key_path"] = static_key_path;
        if (!key_direction.isEmpty()) object["key_direction"] = key_direction;
        if (auto tlsObj = tls->ExportToJson(); !tlsObj.isEmpty()) object["tls"] = tlsObj;
        if (!cipher.isEmpty()) object["cipher"] = cipher;
        if (!data_ciphers.isEmpty()) object["data_ciphers"] = QListStr2QJsonArray(data_ciphers);
        if (!data_ciphers_fallback.isEmpty()) object["data_ciphers_fallback"] = data_ciphers_fallback;
        if (!auth.isEmpty()) object["auth"] = auth;
        if (mss_fix > 0) object["mss_fix"] = mss_fix;
        if (mss_fix_disabled) object["mss_fix_disabled"] = true;
        if (!mss_fix_mode.isEmpty()) object["mss_fix_mode"] = mss_fix_mode;
        if (fragment > 0) object["fragment"] = fragment;
        if (replay_window > 0) object["replay_window"] = replay_window;
        if (!replay_window_time.isEmpty()) object["replay_window_time"] = replay_window_time;
        if (!compression.isEmpty()) object["compression"] = compression;
        if (!compression_lzo.isEmpty()) object["compression_lzo"] = compression_lzo;
        if (!allow_compression.isEmpty()) object["allow_compression"] = allow_compression;
        if (route_no_pull) object["route_no_pull"] = true;
        if (!pull_filters.isEmpty()) {
            QJsonArray filters;
            for (const auto& filter : pull_filters) {
                if (auto item = filter->ExportToJson(); !item.isEmpty()) filters.append(item);
            }
            if (!filters.isEmpty()) object["pull_filters"] = filters;
        }
        if (!routes.isEmpty()) object["routes"] = QListStr2QJsonArray(routes);
        if (!route_gateway.isEmpty()) object["route_gateway"] = route_gateway;
        if (route_metric > 0) object["route_metric"] = route_metric;
        if (redirect_gateway) object["redirect_gateway"] = true;
        if (!redirect_gateway_flags.isEmpty()) object["redirect_gateway_flags"] = QListStr2QJsonArray(redirect_gateway_flags);
        if (redirect_private) object["redirect_private"] = true;
        if (block_ipv6) object["block_ipv6"] = true;
        if (!ping_interval.isEmpty()) object["ping_interval"] = ping_interval;
        if (!ping_restart.isEmpty()) object["ping_restart"] = ping_restart;
        if (ping_restart_disabled) object["ping_restart_disabled"] = true;
        if (!renegotiate_interval.isEmpty()) object["renegotiate_interval"] = renegotiate_interval;
        if (renegotiate_disabled) object["renegotiate_disabled"] = true;
        if (renegotiate_bytes > 0) object["renegotiate_bytes"] = renegotiate_bytes;
        if (renegotiate_packets > 0) object["renegotiate_packets"] = renegotiate_packets;
        if (!tls_timeout.isEmpty()) object["tls_timeout"] = tls_timeout;
        if (!handshake_window.isEmpty()) object["handshake_window"] = handshake_window;
        if (explicit_exit_notify > 0) object["explicit_exit_notify"] = explicit_exit_notify;
        if (system) object["system"] = true;
        if (!interface_name.isEmpty()) object["name"] = interface_name;
        if (mtu > 0) object["mtu"] = mtu;
        if (!udp_timeout.isEmpty()) object["udp_timeout"] = udp_timeout;
        if (!udp_mapping.isEmpty()) object["udp_mapping"] = udp_mapping;
        if (!udp_filtering.isEmpty()) object["udp_filtering"] = udp_filtering;
        if (udp_nat_max > 0) object["udp_nat_max"] = udp_nat_max;

        if (otp_profile_id >= 0) object["otp_profile_id"] = otp_profile_id;
        object["only_advertised_routes"] = only_advertised_routes;
        object["tunnel_dns"] = tunnel_dns;
        return object;
    }

    BuildResult openvpn::Build()
    {
        QJsonObject object;
        object["type"] = "openvpn-client";
        if (!name.isEmpty()) object["tag"] = name;
        const auto dial = dialFields->Build();
        if (!dial.error.isEmpty()) return {{}, dial.error};
        mergeJsonObjects(object, dial.object);

        // `server`/`server_port` and `servers` are mutually exclusive.
        if (servers.isEmpty()) {
            if (!server.isEmpty()) object["server"] = server;
            if (server_port > 0) object["server_port"] = server_port;
        } else {
            QJsonArray remotes;
            for (const auto& remote : servers) {
                if (auto item = remote->Build().object; !item.isEmpty()) remotes.append(item);
            }
            object["servers"] = remotes;
            if (remote_random) object["remote_random"] = true;
        }

        const auto creds = ResolveVpnCredentials(profile_id, username, password);
        // A baked code is replayed on every reconnect, so leave the challenge for the poller. The
        // core packs any challenge answer as SCRV1, so an {otp} inside the credentials cannot go
        // that way and still has to be built in.
        const bool otpInCredentials = creds.username.contains(kOtpPlaceholder) ||
                                      creds.password.contains(kOtpPlaceholder);
        const bool liveChallenge = otp_profile_id >= 0 && !static_challenge.isEmpty() &&
                                   !otpInCredentials && !BuildingTestConfig();

        auto otpCode = liveChallenge ? QString() : ResolveOtpCode(otp_profile_id);
        auto user = SubstituteOtp(creds.username, otpCode);
        auto pass = SubstituteOtp(creds.password, otpCode);
        // Upstream packs a static-challenge answer as SCRV1:<b64 password>:<b64 answer>.
        bool packedChallenge = !otpCode.isEmpty() && !static_challenge.isEmpty();
        if (packedChallenge) {
            pass = "SCRV1:" + QString::fromLatin1(pass.toUtf8().toBase64()) + ":" +
                   QString::fromLatin1(otpCode.toUtf8().toBase64());
        }
        if (!user.isEmpty()) object["username"] = user;
        if (!pass.isEmpty()) object["password"] = pass;
        if (!packedChallenge) {
            if (!static_challenge.isEmpty()) object["static_challenge"] = static_challenge;
            if (static_challenge_echo) object["static_challenge_echo"] = true;
        }

        if (!mode.isEmpty()) object["mode"] = mode;
        if (!network.isEmpty()) object["network"] = network;
        if (!address.isEmpty()) object["address"] = QListStr2QJsonArray(address);
        if (!peer_address.isEmpty()) object["peer_address"] = peer_address;
        if (!peer_address_ipv6.isEmpty()) object["peer_address_ipv6"] = peer_address_ipv6;
        if (!topology.isEmpty()) object["topology"] = topology;
        // "none" makes a rejected login terminal; anything else re-raises the challenge instead.
        if (!auth_retry.isEmpty()) object["auth_retry"] = auth_retry;
        else if (liveChallenge) object["auth_retry"] = "interact";
        if (!static_key.isEmpty()) object["static_key"] = QListStr2QJsonArray(static_key);
        if (!static_key_path.isEmpty()) object["static_key_path"] = static_key_path;
        if (!key_direction.isEmpty()) object["key_direction"] = key_direction;
        const auto tlsResult = tls->Build();
        if (!tlsResult.error.isEmpty()) return {{}, tlsResult.error};
        if (!tlsResult.object.isEmpty()) object["tls"] = tlsResult.object;
        // The core matches the canonical upper-case names only; .ovpn files often spell them in lower case.
        if (!cipher.isEmpty()) object["cipher"] = cipher.trimmed().toUpper();
        if (!data_ciphers.isEmpty()) object["data_ciphers"] = QListStr2QJsonArray(ovpnUpper(data_ciphers));
        if (!data_ciphers_fallback.isEmpty()) object["data_ciphers_fallback"] = data_ciphers_fallback.trimmed().toUpper();
        if (!auth.isEmpty()) object["auth"] = auth.trimmed().toUpper();
        if (mss_fix > 0) object["mss_fix"] = mss_fix;
        if (mss_fix_disabled) object["mss_fix_disabled"] = true;
        if (!mss_fix_mode.isEmpty()) object["mss_fix_mode"] = mss_fix_mode;
        if (fragment > 0) object["fragment"] = fragment;
        if (replay_window > 0) object["replay_window"] = replay_window;
        if (!replay_window_time.isEmpty()) object["replay_window_time"] = replay_window_time;
        if (!compression.isEmpty()) object["compression"] = compression;
        if (!compression_lzo.isEmpty()) object["compression_lzo"] = compression_lzo;
        if (!allow_compression.isEmpty()) object["allow_compression"] = allow_compression;
        if (route_no_pull) object["route_no_pull"] = true;
        if (!pull_filters.isEmpty()) {
            QJsonArray filters;
            for (const auto& filter : pull_filters) {
                if (auto item = filter->Build().object; !item.isEmpty()) filters.append(item);
            }
            if (!filters.isEmpty()) object["pull_filters"] = filters;
        }
        if (!routes.isEmpty()) object["routes"] = QListStr2QJsonArray(routes);
        if (!route_gateway.isEmpty()) object["route_gateway"] = route_gateway;
        if (route_metric > 0) object["route_metric"] = route_metric;
        if (redirect_gateway) object["redirect_gateway"] = true;
        if (!redirect_gateway_flags.isEmpty()) object["redirect_gateway_flags"] = QListStr2QJsonArray(redirect_gateway_flags);
        if (redirect_private) object["redirect_private"] = true;
        if (block_ipv6) object["block_ipv6"] = true;
        if (!ping_interval.isEmpty()) object["ping_interval"] = ping_interval;
        if (!ping_restart.isEmpty()) object["ping_restart"] = ping_restart;
        if (ping_restart_disabled) object["ping_restart_disabled"] = true;
        if (!renegotiate_interval.isEmpty()) object["renegotiate_interval"] = renegotiate_interval;
        if (renegotiate_disabled) object["renegotiate_disabled"] = true;
        if (renegotiate_bytes > 0) object["renegotiate_bytes"] = renegotiate_bytes;
        if (renegotiate_packets > 0) object["renegotiate_packets"] = renegotiate_packets;
        if (!tls_timeout.isEmpty()) object["tls_timeout"] = tls_timeout;
        if (!handshake_window.isEmpty()) object["handshake_window"] = handshake_window;
        if (explicit_exit_notify > 0) object["explicit_exit_notify"] = explicit_exit_notify;
        if (system) object["system"] = true;
        if (!interface_name.isEmpty()) object["name"] = interface_name;
        if (mtu > 0) object["mtu"] = mtu;
        if (!udp_timeout.isEmpty()) object["udp_timeout"] = udp_timeout;
        if (!udp_mapping.isEmpty()) object["udp_mapping"] = udp_mapping;
        if (!udp_filtering.isEmpty()) object["udp_filtering"] = udp_filtering;
        if (udp_nat_max > 0) object["udp_nat_max"] = udp_nat_max;
        return {object, ""};
    }

    QString openvpn::DisplayType()
    {
        return "OpenVPN";
    }

    SecurityInfo openvpn::GetSecurity()
    {
        if (mode == "static_key") return {QObject::tr("Static Key"), {}, SecurityLevel::Weak};
        auto pinned = !tls->certificate.isEmpty() || !tls->certificate_path.isEmpty() || !tls->peer_fingerprint.isEmpty();
        if (!pinned) return {QObject::tr("Unverified TLS"), {}, SecurityLevel::Weak};
        return {QObject::tr("TLS"), {}, SecurityLevel::Secure};
    }

    bool openvpn::IsEndpoint()
    {
        return true;
    }

    bool openvpn::SupportsCredentialStrip() const
    {
        return true;
    }

    void openvpn::StripCredentials()
    {
        username.clear();
        password.clear();
        static_key.clear();
        static_key_path.clear();
        otp_profile_id = -1;
        tls->client_key.clear();
        tls->client_key_path.clear();
        tls->control_wrap->key.clear();
        tls->control_wrap->key_path.clear();
        // Absolute local paths: meaningless on another machine and they carry the OS user name.
        tls->certificate_path.clear();
        tls->client_certificate_path.clear();
        tls->crl_path.clear();
    }

    void openvpn::StripForeignState(Validation::Issues& issues)
    {
        if (otp_profile_id < 0) return;
        otp_profile_id = -1;
        issues << Validation::Issue{Validation::Severity::Warning, "otp_profile_id",
                                    QCoreApplication::translate("Validation", "Removed the link to a one-time password entry; choose the entry again in the profile's settings."), {}};
    }

    void openvpn::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        using Validation::Empty;
        using Validation::Family;
        using Validation::Severity;
        const auto report = [&issues](const QString& field, const QString& message, const QString& value = {},
                                      Severity severity = Severity::Error) {
            if (!message.isEmpty()) issues << Validation::Issue{severity, field, message, value};
        };
        const auto reportItems = [&report](const QString& field, const QStringList& items, const Validation::Rule& rule) {
            for (const auto& item : items) {
                if (const auto message = rule(item); !message.isEmpty()) return report(field, message, item);
            }
        };
        const auto cidr = [](const QString& item) { return Validation::Cidr(item); };

        report("mode", Validation::OneOf(mode, {"tls", "static_key"}), mode);
        report("network", Validation::OneOf(network, openvpnNetworks), network);
        if (servers.isEmpty() && (server.startsWith('[') || server.endsWith(']'))) report("server", Checks::OpenVPNRemoteHost(server), server);
        for (qsizetype i = 0; i < servers.size(); ++i) {
            const auto& remote = *servers[i];
            const auto field = QStringLiteral("servers[%1]").arg(i);
            report(field + ".server", Checks::OpenVPNRemoteHost(remote.server), remote.server);
            report(field + ".server_port", Checks::OpenVPNRemotePort(QString::number(remote.server_port)), QString::number(remote.server_port));
            report(field + ".network", Validation::OneOf(remote.network, openvpnNetworks), remote.network);
        }
        reportItems("address", address, cidr);
        report("peer_address", Validation::Ip(peer_address, Family::V4, Empty::Allowed), peer_address);
        report("peer_address_ipv6", Validation::Ip(peer_address_ipv6, Family::V6, Empty::Allowed), peer_address_ipv6);
        report("topology", Validation::OneOf(topology, {"net30", "p2p", "subnet"}), topology);
        report("auth_retry", Validation::OneOf(auth_retry, {"none", "nointeract", "interact"}), auth_retry);
        report("key_direction", Validation::OneOf(key_direction, {"server", "client"}), key_direction);
        report("cipher", Checks::OpenVPNCipherName(cipher), cipher);
        reportItems("data_ciphers", data_ciphers, Checks::OpenVPNCipherName);
        report("data_ciphers_fallback", Checks::OpenVPNCipherName(data_ciphers_fallback), data_ciphers_fallback);
        report("auth", Checks::OpenVPNDigest(auth), auth);
        report("mss_fix_mode", Validation::OneOf(mss_fix_mode, {"mtu", "fixed"}), mss_fix_mode);
        report("fragment", Checks::OpenVPNFragment(QString::number(fragment)), QString::number(fragment));
        report("replay_window", Checks::OpenVPNReplayWindow(QString::number(replay_window)), QString::number(replay_window));
        report("replay_window_time", Checks::OpenVPNReplayWindowTime(replay_window_time), replay_window_time);
        reportItems("routes", routes, cidr);
        report("route_gateway", Validation::Ip(route_gateway, Family::Any, Empty::Allowed), route_gateway);
        report("ping_interval", Checks::OpenVPNPingInterval(ping_interval), ping_interval);
        report("ping_restart", Checks::OpenVPNPingInterval(ping_restart), ping_restart);
        report("renegotiate_interval", Checks::OpenVPNInterval(renegotiate_interval), renegotiate_interval);
        report("tls_timeout", Checks::OpenVPNInterval(tls_timeout), tls_timeout);
        report("handshake_window", Checks::OpenVPNInterval(handshake_window), handshake_window);
        report("udp_timeout", Validation::Duration(udp_timeout), udp_timeout);
        for (qsizetype i = 0; i < pull_filters.size(); ++i) {
            const auto& filter = *pull_filters[i];
            if (filter.action.isEmpty() && filter.text.isEmpty()) continue;
            const auto field = QStringLiteral("pull_filters[%1]").arg(i);
            report(field + ".action", Validation::OneOf(filter.action, {"accept", "ignore", "reject"}, Empty::Required), filter.action);
            report(field + ".text", Validation::Required(filter.text));
        }

        report("tls.server_name_type", Validation::OneOf(tls->server_name_type, {"subject", "name", "name-prefix"}), tls->server_name_type);
        reportItems("tls.peer_fingerprint", tls->peer_fingerprint, Checks::OpenVPNFingerprint);
        reportItems("tls.remote_certificate_ku", tls->remote_certificate_ku, Checks::OpenVPNKeyUsage);
        report("tls.remote_certificate_eku", Checks::OpenVPNExtendedKeyUsage(tls->remote_certificate_eku), tls->remote_certificate_eku);
        report("tls.remote_certificate_tls", Validation::OneOf(tls->remote_certificate_tls, {"server", "client", "none"}), tls->remote_certificate_tls);
        report("tls.certificate_profile", Validation::OneOf(tls->certificate_profile, {"legacy", "preferred", "insecure", "suiteb"}), tls->certificate_profile);
        report("tls.ns_certificate_type", Validation::OneOf(tls->ns_certificate_type, {"server", "client"}), tls->ns_certificate_type);
        report("tls.version_min", Validation::OneOf(tls->version_min, {"1.0", "1.1", "1.2", "1.3"}), tls->version_min);
        report("tls.version_max", Validation::OneOf(tls->version_max, {"1.0", "1.1", "1.2", "1.3"}), tls->version_max);
        report("tls.cipher", Checks::OpenVPNTlsCipher(tls->cipher), tls->cipher);
        report("tls.groups", Checks::OpenVPNTlsGroups(tls->groups), tls->groups);

        for (const auto& key : Checks::OpenVPNModeConflicts(*this)) report(key, Checks::OpenVPNModeConflict(*this, key));
        report("static_key", Checks::OpenVPNStaticKeyMissing(*this));
        report("static_key", Checks::VpnMaterialConflict(static_key, static_key_path));
        report("cipher", Checks::OpenVPNStaticCipher(*this), cipher);
        report("address", Checks::OpenVPNStaticAddress(*this));
        report("tls.certificate", Checks::OpenVPNTlsVerification(*this));
        report("tls.client_certificate", Checks::OpenVPNClientPair(*this));
        report("tls.certificate", Checks::VpnMaterialConflict(tls->certificate, tls->certificate_path));
        report("tls.client_certificate", Checks::VpnMaterialConflict(tls->client_certificate, tls->client_certificate_path));
        report("tls.client_key", Checks::VpnMaterialConflict(tls->client_key, tls->client_key_path));
        report("tls.control_wrap.key", Checks::VpnMaterialConflict(tls->control_wrap->key, tls->control_wrap->key_path));
        report("tls.control_wrap.type", Checks::OpenVPNControlWrapKey(*this), tls->control_wrap->type);
        report("tls.control_wrap.direction", Checks::OpenVPNControlWrapDirection(*this), tls->control_wrap->direction);
        report("tls.remote_certificate_eku", Checks::OpenVPNRemoteCertificate(*this));
        report("tls.version_max", Checks::OpenVPNTlsVersions(*this), tls->version_max);
        report("mss_fix", Checks::OpenVPNMssFix(*this));
        report("ping_restart", Checks::OpenVPNPingRestart(*this));
        report("renegotiate_interval", Checks::OpenVPNRenegotiate(*this));
        // An import never carries the authenticator binding (StripForeignState), so this is only a reminder.
        report("username", Checks::OtpPlaceholderUnbound(username, otp_profile_id), {}, Severity::Warning);
        report("password", Checks::OtpPlaceholderUnbound(password, otp_profile_id), {}, Severity::Warning);
    }

    std::shared_ptr<openvpn> openvpn::DetachedCopy() const
    {
        auto copy = std::make_shared<openvpn>(*this);
        copy->tls = std::make_shared<OpenVPNTLS>(*tls);
        copy->tls->control_wrap = std::make_shared<OpenVPNControlWrap>(*tls->control_wrap);
        return copy;
    }
}
