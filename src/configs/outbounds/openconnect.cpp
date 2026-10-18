#include "include/configs/outbounds/openconnect.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QUrl>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"
#include "include/configs/outbounds/openvpn.h"
#include "include/configs/sub/vpnFileImport.hpp"
#include "include/global/OtpPlaceholder.hpp"
#include "include/global/VpnCredentialOverride.hpp"

namespace Configs {
    namespace {
        // sing-box listables accept a bare string as well as an array of lines.
        QStringList ocListable(const QJsonValue& value)
        {
            if (value.isString()) return value.toString().split("\n", Qt::SkipEmptyParts);
            return QJsonArray2QListString(value.toArray());
        }

        constexpr int kOpenConnectDefaultPort = 443;

        // sing-openconnect anyconnect_auth.go, gp_auth.go and pulse_auth.go share this list; the other flavors take anything.
        const QStringList kOpenConnectReportedOs = {"linux", "linux-64", "win", "mac-intel", "android", "apple-ios"};

        // Import only: an optional value the core would refuse is dropped with a warning; DB loads and clones keep it.
        void ocReadChecked(const QJsonObject& object, const QString& key, QString& target, const Validation::Rule& rule)
        {
            if (!object.contains(key)) return;
            const auto value = object[key].toString().trimmed();
            if (!ParseDiag::Active() || AcceptOptional(key, rule(value), value)) target = value;
        }

        void ocReadCheckedInt(const QJsonObject& object, const QString& key, int& target, const Validation::Rule& rule)
        {
            if (!object.contains(key)) return;
            const auto value = object[key].toInt();
            const auto text = QString::number(value);
            if (!ParseDiag::Active() || AcceptOptional(key, rule(text), text)) target = value;
        }

        QString ocDuration(const QString& text)
        {
            return Validation::Duration(text);
        }
    }

    namespace Checks {
        // The core parses ComposeServer() with net/url and refuses a query or fragment.
        QString OpenConnectServerPath(const QString& text)
        {
            const auto path = text.trimmed();
            if (path.contains('?') || path.contains('#')) {
                return QCoreApplication::translate("Validation", "Remove the part from ? or #; the server takes only a path such as /vpn.");
            }
            const auto hexDigit = [](QChar c) {
                const auto u = c.unicode();
                return (u >= '0' && u <= '9') || (u >= 'a' && u <= 'f') || (u >= 'A' && u <= 'F');
            };
            for (qsizetype i = path.indexOf('%'); i >= 0; i = path.indexOf('%', i + 1)) {
                if (i + 2 >= path.size() || !hexDigit(path[i + 1]) || !hexDigit(path[i + 2])) {
                    return QCoreApplication::translate("Validation", "A % must start an escape such as %20.");
                }
            }
            return {};
        }

        QString OpenConnectMtuFloor(const QString& text)
        {
            const auto value = Validation::ToInt(text).value_or(0);
            if (value <= 0 || value >= 576) return {};
            return QCoreApplication::translate("Validation", "The core raises values below 576 to 576.");
        }

        // sing-openconnect parsePeerFingerprint.
        QString OpenConnectFingerprint(const QString& line)
        {
            const auto value = line.trimmed();
            if (value.isEmpty()) return {};
            QString encoded = value;
            qsizetype maximum = 40;
            bool base64 = false;
            if (value.startsWith("sha1:")) {
                encoded = value.mid(5);
            } else if (value.startsWith("sha256:")) {
                encoded = value.mid(7);
                maximum = 64;
            } else if (value.startsWith("pin-sha256:")) {
                encoded = value.mid(11);
                maximum = 44;
                base64 = true;
            } else if (value.contains(':')) {
                return QCoreApplication::translate("Validation", "Write the fingerprint without colons between the bytes, or as sha1:, sha256: or pin-sha256:.");
            }
            bool ok = encoded.size() >= 4 && encoded.size() <= maximum;
            for (qsizetype i = 0; ok && i < encoded.size(); ++i) {
                const auto c = encoded[i].unicode();
                const bool hexDigit = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
                const bool base64Digit = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '+' || c == '/' ||
                                         (c == '=' && i >= encoded.size() - 2);
                ok = base64 ? base64Digit : hexDigit;
            }
            if (ok && base64 && encoded.size() == maximum) ok = Validation::Base64Key(encoded, 32).isEmpty();
            if (ok) return {};
            if (base64) return QCoreApplication::translate("Validation", "After pin-sha256: write the base64 SHA-256 of the public key (44 characters).");
            return QCoreApplication::translate("Validation", "Use at least 4 hexadecimal characters of the fingerprint (up to %1).").arg(maximum);
        }

        QString OpenConnectReportedOs(const QString& flavor, const QString& os)
        {
            if (!flavor.isEmpty() && flavor != "anyconnect" && flavor != "gp" && flavor != "pulse") return {};
            return Validation::OneOf(os, kOpenConnectReportedOs);
        }

        // sing-openconnect NewClient raises a shorter interval to 2s.
        QString OpenConnectDpdFloor(const QString& text)
        {
            const auto ns = Validation::ParseDurationNs(text.trimmed()).value_or(0);
            if (ns <= 0 || ns >= 2000000000LL) return {};
            return QCoreApplication::translate("Validation", "The core raises values below 2s to 2s.");
        }

        QString OpenConnectCertificatePair(const QStringList& certificate, const QString& certificatePath,
                                           const QStringList& key, const QString& keyPath)
        {
            const bool hasCertificate = !certificate.isEmpty() || !certificatePath.trimmed().isEmpty();
            const bool hasKey = !key.isEmpty() || !keyPath.trimmed().isEmpty();
            if (hasCertificate == hasKey) return {};
            return hasCertificate ? QCoreApplication::translate("Validation", "Add the key that belongs to this certificate.")
                                  : QCoreApplication::translate("Validation", "Add the certificate that belongs to this key.");
        }

        QString OpenConnectFormEntry(const QString& formId, const QString& submissionKey, const QString& name,
                                     const QString& value, bool promote)
        {
            if (submissionKey.isEmpty() && (formId.isEmpty() || name.isEmpty())) {
                return QCoreApplication::translate("Validation", "Needs a submission key, or a form ID and a field name.");
            }
            if (promote && !value.isEmpty()) {
                return QCoreApplication::translate("Validation", "A promoted field is asked for when connecting, so it cannot also have a value.");
            }
            return {};
        }

        QString OpenConnectToken(const openconnect& profile)
        {
            const auto& token = *profile.token;
            if (token.mode.isEmpty()) return {};
            if (auto message = Validation::OneOf(token.mode, {"totp", "hotp", "stoken", "oidc"}); !message.isEmpty()) return message;
            if (!token.secret.isEmpty() && !token.secret_path.isEmpty()) {
                return QCoreApplication::translate("Validation", "Set the token secret either directly or as a file, not both.");
            }
            if (token.secret.isEmpty() && token.secret_path.isEmpty()) {
                return QCoreApplication::translate("Validation", "The software token needs its secret, directly or as a file.");
            }
            return {};
        }

        // ExportToJson drops the whole token when the mode is empty.
        QString OpenConnectTokenUnused(const openconnect& profile)
        {
            const auto& token = *profile.token;
            if (!token.mode.isEmpty()) return {};
            const bool used = !token.secret.isEmpty() || !token.secret_path.isEmpty() || !token.pin.isEmpty() ||
                              !token.password.isEmpty() || !token.device_id.isEmpty() || token.counter > 0;
            if (!used) return {};
            return QCoreApplication::translate("Validation", "Without a token mode these settings are not used or saved; pick a mode or clear them.");
        }

        QString OpenConnectTokenCounter(const openconnect& profile)
        {
            if (profile.token->counter <= 0 || profile.token->mode.isEmpty() || profile.token->mode == "hotp") return {};
            return QCoreApplication::translate("Validation", "Only hotp tokens use the counter.");
        }

        // Build() emits all three as soon as one is set, and the core refuses an empty one.
        QString OpenConnectMobile(const openconnect& profile)
        {
            const auto& mobile = *profile.mobile;
            const int set = int(!mobile.platform_version.isEmpty()) + int(!mobile.device_type.isEmpty()) + int(!mobile.device_unique_id.isEmpty());
            if (set == 0 || set == 3) return {};
            return QCoreApplication::translate("Validation", "Fill in all three mobile fields or leave them all empty.");
        }

        QString OpenConnectCompression(const openconnect& profile)
        {
            if (!profile.compression_disabled || profile.compression_mode != "all") return {};
            return QCoreApplication::translate("Validation", "Compression is disabled, so the mode must be empty or stateless.");
        }
    }

    bool OpenConnectToken::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("mode")) mode = object["mode"].toString().trimmed();
        if (object.contains("secret")) secret = object["secret"].toString();
        if (object.contains("secret_path")) secret_path = object["secret_path"].toString();
        if (object.contains("pin")) pin = object["pin"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("device_id")) device_id = object["device_id"].toString();
        if (object.contains("counter")) counter = object["counter"].toInteger();
        return true;
    }

    QJsonObject OpenConnectToken::ExportToJson()
    {
        QJsonObject object;
        if (mode.isEmpty()) return object;
        object["mode"] = mode;
        if (!secret.isEmpty()) object["secret"] = secret;
        if (!secret_path.isEmpty()) object["secret_path"] = secret_path;
        if (!pin.isEmpty()) object["pin"] = pin;
        if (!password.isEmpty()) object["password"] = password;
        if (!device_id.isEmpty()) object["device_id"] = device_id;
        if (counter > 0) object["counter"] = counter;
        return object;
    }

    BuildResult OpenConnectToken::Build()
    {
        return {ExportToJson(), ""};
    }

    bool OpenConnectMobile::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("platform_version")) platform_version = object["platform_version"].toString();
        if (object.contains("device_type")) device_type = object["device_type"].toString();
        if (object.contains("device_unique_id")) device_unique_id = object["device_unique_id"].toString();
        return true;
    }

    QJsonObject OpenConnectMobile::ExportToJson()
    {
        QJsonObject object;
        if (platform_version.isEmpty() && device_type.isEmpty() && device_unique_id.isEmpty()) return object;
        object["platform_version"] = platform_version;
        object["device_type"] = device_type;
        object["device_unique_id"] = device_unique_id;
        return object;
    }

    BuildResult OpenConnectMobile::Build()
    {
        return {ExportToJson(), ""};
    }

    bool OpenConnectWrapper::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("wrapper_path")) wrapper_path = object["wrapper_path"].toString();
        return true;
    }

    QJsonObject OpenConnectWrapper::ExportToJson()
    {
        QJsonObject object;
        if (!wrapper_path.isEmpty()) object["wrapper_path"] = wrapper_path;
        return object;
    }

    BuildResult OpenConnectWrapper::Build()
    {
        return {ExportToJson(), ""};
    }

    bool OpenConnectTNCCCertificate::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("certificate")) certificate = ocListable(object["certificate"]);
        if (object.contains("certificate_path")) certificate_path = object["certificate_path"].toString();
        return true;
    }

    QJsonObject OpenConnectTNCCCertificate::ExportToJson()
    {
        QJsonObject object;
        if (!certificate.isEmpty()) object["certificate"] = QListStr2QJsonArray(certificate);
        if (!certificate_path.isEmpty()) object["certificate_path"] = certificate_path;
        return object;
    }

    BuildResult OpenConnectTNCCCertificate::Build()
    {
        return {ExportToJson(), ""};
    }

    bool OpenConnectTNCC::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("wrapper_path")) wrapper_path = object["wrapper_path"].toString();
        if (object.contains("device_id")) device_id = object["device_id"].toString();
        if (object.contains("user_agent")) user_agent = object["user_agent"].toString();
        if (object.contains("machine_identification_enabled")) machine_identification_enabled = object["machine_identification_enabled"].toBool();
        if (object.contains("certificates")) {
            certificates.clear();
            for (const auto& item : object["certificates"].toArray()) {
                auto cert = std::make_shared<OpenConnectTNCCCertificate>();
                if (cert->ParseFromJson(item.toObject())) certificates.append(cert);
            }
        }
        return true;
    }

    QJsonObject OpenConnectTNCC::ExportToJson()
    {
        QJsonObject object;
        if (!wrapper_path.isEmpty()) object["wrapper_path"] = wrapper_path;
        if (!device_id.isEmpty()) object["device_id"] = device_id;
        if (!user_agent.isEmpty()) object["user_agent"] = user_agent;
        if (machine_identification_enabled) object["machine_identification_enabled"] = true;
        if (!certificates.isEmpty()) {
            QJsonArray certs;
            for (const auto& cert : certificates) {
                if (auto item = cert->ExportToJson(); !item.isEmpty()) certs.append(item);
            }
            if (!certs.isEmpty()) object["certificates"] = certs;
        }
        return object;
    }

    BuildResult OpenConnectTNCC::Build()
    {
        return {ExportToJson(), ""};
    }

    bool OpenConnectFortinetHostCheck::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("hostcheck")) hostcheck = object["hostcheck"].toString();
        if (object.contains("check_virtual_desktop")) check_virtual_desktop = object["check_virtual_desktop"].toString();
        return true;
    }

    QJsonObject OpenConnectFortinetHostCheck::ExportToJson()
    {
        QJsonObject object;
        // An empty hostcheck disables the feature, so the rest is meaningless alone.
        if (hostcheck.isEmpty()) return object;
        object["hostcheck"] = hostcheck;
        if (!check_virtual_desktop.isEmpty()) object["check_virtual_desktop"] = check_virtual_desktop;
        return object;
    }

    BuildResult OpenConnectFortinetHostCheck::Build()
    {
        return {ExportToJson(), ""};
    }

    bool OpenConnectFormEntry::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("form_id")) form_id = object["form_id"].toString();
        if (object.contains("submission_key")) submission_key = object["submission_key"].toString();
        if (object.contains("name")) name = object["name"].toString();
        if (object.contains("value")) value = object["value"].toString();
        if (object.contains("promote")) promote = object["promote"].toBool();
        return true;
    }

    QJsonObject OpenConnectFormEntry::ExportToJson()
    {
        QJsonObject object;
        if (submission_key.isEmpty() && (form_id.isEmpty() || name.isEmpty())) return object;
        if (!form_id.isEmpty()) object["form_id"] = form_id;
        if (!submission_key.isEmpty()) object["submission_key"] = submission_key;
        if (!name.isEmpty()) object["name"] = name;
        if (!value.isEmpty()) object["value"] = value;
        if (promote) object["promote"] = true;
        return object;
    }

    BuildResult OpenConnectFormEntry::Build()
    {
        return {ExportToJson(), ""};
    }

    bool OpenConnectTLS::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("insecure")) insecure = object["insecure"].toBool();
        if (object.contains("server_name")) server_name = object["server_name"].toString();
        if (object.contains("peer_fingerprint")) peer_fingerprint = ocListable(object["peer_fingerprint"]);
        if (object.contains("system_trust_disabled")) system_trust_disabled = object["system_trust_disabled"].toBool();
        if (object.contains("certificate_authority")) certificate_authority = ocListable(object["certificate_authority"]);
        if (object.contains("certificate_authority_path")) certificate_authority_path = object["certificate_authority_path"].toString();
        if (object.contains("client_certificate")) client_certificate = ocListable(object["client_certificate"]);
        if (object.contains("client_certificate_path")) client_certificate_path = object["client_certificate_path"].toString();
        if (object.contains("client_key")) client_key = ocListable(object["client_key"]);
        if (object.contains("client_key_path")) client_key_path = object["client_key_path"].toString();
        if (object.contains("client_key_password")) client_key_password = object["client_key_password"].toString();
        if (object.contains("mca_certificate")) mca_certificate = ocListable(object["mca_certificate"]);
        if (object.contains("mca_certificate_path")) mca_certificate_path = object["mca_certificate_path"].toString();
        if (object.contains("mca_key")) mca_key = ocListable(object["mca_key"]);
        if (object.contains("mca_key_path")) mca_key_path = object["mca_key_path"].toString();
        if (object.contains("mca_key_password")) mca_key_password = object["mca_key_password"].toString();
        return true;
    }

    QJsonObject OpenConnectTLS::ExportToJson()
    {
        QJsonObject object;
        if (insecure) object["insecure"] = true;
        if (!server_name.isEmpty()) object["server_name"] = toAceHost(server_name);
        if (!peer_fingerprint.isEmpty()) object["peer_fingerprint"] = QListStr2QJsonArray(peer_fingerprint);
        if (system_trust_disabled) object["system_trust_disabled"] = true;
        if (!certificate_authority.isEmpty()) object["certificate_authority"] = QListStr2QJsonArray(certificate_authority);
        if (!certificate_authority_path.isEmpty()) object["certificate_authority_path"] = certificate_authority_path;
        if (!client_certificate.isEmpty()) object["client_certificate"] = QListStr2QJsonArray(client_certificate);
        if (!client_certificate_path.isEmpty()) object["client_certificate_path"] = client_certificate_path;
        if (!client_key.isEmpty()) object["client_key"] = QListStr2QJsonArray(client_key);
        if (!client_key_path.isEmpty()) object["client_key_path"] = client_key_path;
        if (!client_key_password.isEmpty()) object["client_key_password"] = client_key_password;
        if (!mca_certificate.isEmpty()) object["mca_certificate"] = QListStr2QJsonArray(mca_certificate);
        if (!mca_certificate_path.isEmpty()) object["mca_certificate_path"] = mca_certificate_path;
        if (!mca_key.isEmpty()) object["mca_key"] = QListStr2QJsonArray(mca_key);
        if (!mca_key_path.isEmpty()) object["mca_key_path"] = mca_key_path;
        if (!mca_key_password.isEmpty()) object["mca_key_password"] = mca_key_password;
        return object;
    }

    BuildResult OpenConnectTLS::Build()
    {
        auto object = ExportToJson();
        // The core reads each line verbatim.
        if (!peer_fingerprint.isEmpty()) {
            QStringList fingerprints;
            for (const auto& line : peer_fingerprint) {
                if (const auto trimmed = line.trimmed(); !trimmed.isEmpty()) fingerprints << trimmed;
            }
            if (fingerprints.isEmpty()) object.remove("peer_fingerprint");
            else object["peer_fingerprint"] = QListStr2QJsonArray(fingerprints);
        }
        return {object, ""};
    }

    bool openconnect::ParseFromLink(const QString& link)
    {
        return ParseOpenConnectProfile(link, *this);
    }

    bool openconnect::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "openconnect") return false;
        outbound::ParseFromJson(object);
        if (object.contains("server")) SetServerUrl(object["server"].toString());
        if (object.contains("server_path")) server_path = object["server_path"].toString();

        if (object.contains("flavor")) flavor = object["flavor"].toString().trimmed();
        if (object.contains("username")) username = object["username"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("auth_group")) auth_group = object["auth_group"].toString();
        if (object.contains("cookie")) cookie = object["cookie"].toString();
        if (object.contains("token")) token->ParseFromJson(object["token"].toObject());
        ocReadChecked(object, "reported_os", reported_os, [this](const QString& os) { return Checks::OpenConnectReportedOs(flavor, os); });
        if (object.contains("user_agent")) user_agent = object["user_agent"].toString();
        if (object.contains("version")) version = object["version"].toString();
        if (object.contains("local_hostname")) local_hostname = object["local_hostname"].toString();
        if (object.contains("mobile")) mobile->ParseFromJson(object["mobile"].toObject());
        if (object.contains("csd")) csd->ParseFromJson(object["csd"].toObject());
        if (object.contains("hip")) hip->ParseFromJson(object["hip"].toObject());
        if (object.contains("tncc")) tncc->ParseFromJson(object["tncc"].toObject());
        if (object.contains("fortinet_host_check")) fortinet_host_check->ParseFromJson(object["fortinet_host_check"].toObject());
        if (object.contains("no_udp")) no_udp = object["no_udp"].toBool();
        ocReadCheckedInt(object, "dtls_local_port", dtls_local_port, [](const QString& port) {
            return port == "0" ? QString() : Validation::Port(port);
        });
        if (object.contains("compression_disabled")) compression_disabled = object["compression_disabled"].toBool();
        ocReadChecked(object, "compression_mode", compression_mode, [](const QString& mode) {
            return Validation::OneOf(mode, {"stateless", "all"});
        });
        if (object.contains("ipv6_disabled")) ipv6_disabled = object["ipv6_disabled"].toBool();
        if (object.contains("http_keepalive_disabled")) http_keepalive_disabled = object["http_keepalive_disabled"].toBool();
        if (object.contains("xml_post_disabled")) xml_post_disabled = object["xml_post_disabled"].toBool();
        if (object.contains("external_auth_disabled")) external_auth_disabled = object["external_auth_disabled"].toBool();
        if (object.contains("password_authentication_disabled")) password_authentication_disabled = object["password_authentication_disabled"].toBool();
        if (object.contains("tcp_keep_alive_enabled")) tcp_keep_alive_enabled = object["tcp_keep_alive_enabled"].toBool();
        if (object.contains("pfs")) pfs = object["pfs"].toBool();
        const auto mtuRule = [](const QString& value) { return Validation::Int(value, 0, 65535); };
        ocReadCheckedInt(object, "mtu", mtu, mtuRule);
        ocReadCheckedInt(object, "base_mtu", base_mtu, mtuRule);
        ocReadChecked(object, "dpd_interval", dpd_interval, ocDuration);
        ocReadChecked(object, "reconnect_timeout", reconnect_timeout, ocDuration);
        ocReadChecked(object, "trojan_interval", trojan_interval, ocDuration);
        if (object.contains("queue_length")) queue_length = object["queue_length"].toInt();
        if (object.contains("allow_insecure_crypto")) allow_insecure_crypto = object["allow_insecure_crypto"].toBool();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        if (object.contains("form_entries")) {
            form_entries.clear();
            for (const auto& item : object["form_entries"].toArray()) {
                auto entry = std::make_shared<OpenConnectFormEntry>();
                if (entry->ParseFromJson(item.toObject())) form_entries.append(entry);
            }
        }
        if (object.contains("system")) system = object["system"].toBool();
        if (object.contains("name")) interface_name = object["name"].toString();
        ocReadChecked(object, "udp_timeout", udp_timeout, ocDuration);
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

    void openconnect::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        using Validation::Severity;
        const auto report = [&issues](const QString& field, const QString& message, const QString& value = {},
                                      Severity severity = Severity::Error) {
            if (!message.isEmpty()) issues << Validation::Issue{severity, field, message, value};
        };

        report("flavor", Validation::OneOf(flavor, {"anyconnect", "gp", "fortinet", "f5", "pulse", "nc"}), flavor);
        report("server_path", Checks::OpenConnectServerPath(server_path), server_path);
        report("mtu", Validation::Int(QString::number(mtu), 0, 65535), QString::number(mtu));
        report("mtu", Checks::OpenConnectMtuFloor(QString::number(mtu)), QString::number(mtu), Severity::Warning);
        report("base_mtu", Validation::Int(QString::number(base_mtu), 0, 65535), QString::number(base_mtu));
        report("base_mtu", Checks::OpenConnectMtuFloor(QString::number(base_mtu)), QString::number(base_mtu), Severity::Warning);
        if (dtls_local_port != 0) report("dtls_local_port", Validation::Port(QString::number(dtls_local_port)), QString::number(dtls_local_port));
        report("reported_os", Checks::OpenConnectReportedOs(flavor, reported_os), reported_os);
        report("compression_mode", Validation::OneOf(compression_mode, {"stateless", "all"}), compression_mode);
        report("dpd_interval", Validation::Duration(dpd_interval), dpd_interval);
        report("dpd_interval", Checks::OpenConnectDpdFloor(dpd_interval), dpd_interval, Severity::Warning);
        report("reconnect_timeout", Validation::Duration(reconnect_timeout), reconnect_timeout);
        report("trojan_interval", Validation::Duration(trojan_interval), trojan_interval);
        report("udp_timeout", Validation::Duration(udp_timeout), udp_timeout);
        report("tls.server_name", Checks::ServerName(tls->server_name), tls->server_name);
        for (const auto& line : tls->peer_fingerprint) {
            if (const auto message = Checks::OpenConnectFingerprint(line); !message.isEmpty()) {
                report("tls.peer_fingerprint", message, line);
                break;
            }
        }

        report("tls.certificate_authority", Checks::VpnMaterialConflict(tls->certificate_authority, tls->certificate_authority_path));
        report("tls.client_certificate", Checks::VpnMaterialConflict(tls->client_certificate, tls->client_certificate_path));
        report("tls.client_key", Checks::VpnMaterialConflict(tls->client_key, tls->client_key_path));
        report("tls.mca_certificate", Checks::VpnMaterialConflict(tls->mca_certificate, tls->mca_certificate_path));
        report("tls.mca_key", Checks::VpnMaterialConflict(tls->mca_key, tls->mca_key_path));
        report("tls.client_certificate", Checks::OpenConnectCertificatePair(tls->client_certificate, tls->client_certificate_path,
                                                                            tls->client_key, tls->client_key_path));
        report("tls.mca_certificate", Checks::OpenConnectCertificatePair(tls->mca_certificate, tls->mca_certificate_path,
                                                                         tls->mca_key, tls->mca_key_path));
        for (qsizetype i = 0; i < tncc->certificates.size(); ++i) {
            const auto& certificate = *tncc->certificates[i];
            report(QStringLiteral("tncc.certificates[%1]").arg(i), Checks::VpnMaterialConflict(certificate.certificate, certificate.certificate_path));
        }
        report("token", Checks::OpenConnectToken(*this), token->mode);
        report("token", Checks::OpenConnectTokenUnused(*this), {}, Severity::Warning);
        report("token.counter", Checks::OpenConnectTokenCounter(*this), {}, Severity::Warning);
        report("mobile", Checks::OpenConnectMobile(*this));
        report("compression_mode", Checks::OpenConnectCompression(*this), compression_mode);
        for (qsizetype i = 0; i < form_entries.size(); ++i) {
            const auto& entry = *form_entries[i];
            report(QStringLiteral("form_entries[%1]").arg(i),
                   Checks::OpenConnectFormEntry(entry.form_id, entry.submission_key, entry.name, entry.value, entry.promote));
        }

        // An import never carries the authenticator binding (StripForeignState), so this is only a reminder.
        QStringList otpFields = {username, password, token->pin, token->password};
        for (const auto& entry : form_entries) otpFields << entry->value;
        for (const auto& value : otpFields) {
            if (const auto message = Checks::OtpPlaceholderUnbound(value, otp_profile_id); !message.isEmpty()) {
                report("otp_profile_id", message, {}, Severity::Warning);
                break;
            }
        }
    }

    std::shared_ptr<openconnect> openconnect::DetachedCopy() const
    {
        auto copy = std::make_shared<openconnect>(*this);
        copy->tls = std::make_shared<OpenConnectTLS>(*tls);
        copy->token = std::make_shared<OpenConnectToken>(*token);
        copy->mobile = std::make_shared<OpenConnectMobile>(*mobile);
        copy->csd = std::make_shared<OpenConnectWrapper>(*csd);
        copy->hip = std::make_shared<OpenConnectWrapper>(*hip);
        copy->tncc = std::make_shared<OpenConnectTNCC>(*tncc);
        copy->fortinet_host_check = std::make_shared<OpenConnectFortinetHostCheck>(*fortinet_host_check);
        return copy;
    }

    QJsonObject openconnect::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "openconnect";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!server_path.isEmpty()) object["server_path"] = server_path;

        if (!flavor.isEmpty()) object["flavor"] = flavor;
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (!auth_group.isEmpty()) object["auth_group"] = auth_group;
        if (!cookie.isEmpty()) object["cookie"] = cookie;
        if (auto tokenObj = token->ExportToJson(); !tokenObj.isEmpty()) object["token"] = tokenObj;
        if (!reported_os.isEmpty()) object["reported_os"] = reported_os;
        if (!user_agent.isEmpty()) object["user_agent"] = user_agent;
        if (!version.isEmpty()) object["version"] = version;
        if (!local_hostname.isEmpty()) object["local_hostname"] = local_hostname;
        if (auto mobileObj = mobile->ExportToJson(); !mobileObj.isEmpty()) object["mobile"] = mobileObj;
        if (auto csdObj = csd->ExportToJson(); !csdObj.isEmpty()) object["csd"] = csdObj;
        if (auto hipObj = hip->ExportToJson(); !hipObj.isEmpty()) object["hip"] = hipObj;
        if (auto tnccObj = tncc->ExportToJson(); !tnccObj.isEmpty()) object["tncc"] = tnccObj;
        if (auto fortinetObj = fortinet_host_check->ExportToJson(); !fortinetObj.isEmpty()) object["fortinet_host_check"] = fortinetObj;
        if (no_udp) object["no_udp"] = true;
        if (dtls_local_port > 0) object["dtls_local_port"] = dtls_local_port;
        if (compression_disabled) object["compression_disabled"] = true;
        if (!compression_mode.isEmpty()) object["compression_mode"] = compression_mode;
        if (ipv6_disabled) object["ipv6_disabled"] = true;
        if (http_keepalive_disabled) object["http_keepalive_disabled"] = true;
        if (xml_post_disabled) object["xml_post_disabled"] = true;
        if (external_auth_disabled) object["external_auth_disabled"] = true;
        if (password_authentication_disabled) object["password_authentication_disabled"] = true;
        if (tcp_keep_alive_enabled) object["tcp_keep_alive_enabled"] = true;
        if (pfs) object["pfs"] = true;
        if (mtu > 0) object["mtu"] = mtu;
        if (base_mtu > 0) object["base_mtu"] = base_mtu;
        if (!dpd_interval.isEmpty()) object["dpd_interval"] = dpd_interval;
        if (!reconnect_timeout.isEmpty()) object["reconnect_timeout"] = reconnect_timeout;
        if (!trojan_interval.isEmpty()) object["trojan_interval"] = trojan_interval;
        if (queue_length > 0) object["queue_length"] = queue_length;
        if (allow_insecure_crypto) object["allow_insecure_crypto"] = true;
        if (auto tlsObj = tls->ExportToJson(); !tlsObj.isEmpty()) object["tls"] = tlsObj;
        if (!form_entries.isEmpty()) {
            QJsonArray entries;
            for (const auto& entry : form_entries) {
                if (auto item = entry->ExportToJson(); !item.isEmpty()) entries.append(item);
            }
            if (!entries.isEmpty()) object["form_entries"] = entries;
        }
        if (system) object["system"] = true;
        if (!interface_name.isEmpty()) object["name"] = interface_name;
        if (!udp_timeout.isEmpty()) object["udp_timeout"] = udp_timeout;
        if (!udp_mapping.isEmpty()) object["udp_mapping"] = udp_mapping;
        if (!udp_filtering.isEmpty()) object["udp_filtering"] = udp_filtering;
        if (udp_nat_max > 0) object["udp_nat_max"] = udp_nat_max;

        if (otp_profile_id >= 0) object["otp_profile_id"] = otp_profile_id;
        object["only_advertised_routes"] = only_advertised_routes;
        object["tunnel_dns"] = tunnel_dns;
        return object;
    }

    BuildResult openconnect::Build()
    {
        QJsonObject object;
        object["type"] = "openconnect";
        if (!name.isEmpty()) object["tag"] = name;
        const auto dial = dialFields->Build();
        if (!dial.error.isEmpty()) return {{}, dial.error};
        mergeJsonObjects(object, dial.object);
        if (auto url = ComposeServer(); !url.isEmpty()) object["server"] = url;

        const auto creds = ResolveVpnCredentials(profile_id, username, password);
        auto otpCode = ResolveOtpCode(otp_profile_id);
        if (auto user = SubstituteOtp(creds.username, otpCode); !user.isEmpty()) object["username"] = user;
        if (auto pass = SubstituteOtp(creds.password, otpCode); !pass.isEmpty()) object["password"] = pass;

        if (!flavor.isEmpty()) object["flavor"] = flavor;
        if (!auth_group.isEmpty()) object["auth_group"] = auth_group;
        if (!cookie.isEmpty()) object["cookie"] = cookie;
        if (auto tokenObj = token->Build().object; !tokenObj.isEmpty()) {
            if (tokenObj.contains("pin")) tokenObj["pin"] = SubstituteOtp(tokenObj["pin"].toString(), otpCode);
            if (tokenObj.contains("password")) tokenObj["password"] = SubstituteOtp(tokenObj["password"].toString(), otpCode);
            object["token"] = tokenObj;
        }
        if (!reported_os.isEmpty()) object["reported_os"] = reported_os;
        if (!user_agent.isEmpty()) object["user_agent"] = user_agent;
        if (!version.isEmpty()) object["version"] = version;
        if (!local_hostname.isEmpty()) object["local_hostname"] = local_hostname;
        if (auto mobileObj = mobile->Build().object; !mobileObj.isEmpty()) object["mobile"] = mobileObj;
        if (auto csdObj = csd->Build().object; !csdObj.isEmpty()) object["csd"] = csdObj;
        if (auto hipObj = hip->Build().object; !hipObj.isEmpty()) object["hip"] = hipObj;
        if (auto tnccObj = tncc->Build().object; !tnccObj.isEmpty()) object["tncc"] = tnccObj;
        if (auto fortinetObj = fortinet_host_check->Build().object; !fortinetObj.isEmpty()) object["fortinet_host_check"] = fortinetObj;
        if (no_udp) object["no_udp"] = true;
        if (dtls_local_port > 0) object["dtls_local_port"] = dtls_local_port;
        if (compression_disabled) object["compression_disabled"] = true;
        if (!compression_mode.isEmpty()) object["compression_mode"] = compression_mode;
        if (ipv6_disabled) object["ipv6_disabled"] = true;
        if (http_keepalive_disabled) object["http_keepalive_disabled"] = true;
        if (xml_post_disabled) object["xml_post_disabled"] = true;
        if (external_auth_disabled) object["external_auth_disabled"] = true;
        if (password_authentication_disabled) object["password_authentication_disabled"] = true;
        if (tcp_keep_alive_enabled) object["tcp_keep_alive_enabled"] = true;
        if (pfs) object["pfs"] = true;
        if (mtu > 0) object["mtu"] = mtu;
        if (base_mtu > 0) object["base_mtu"] = base_mtu;
        if (!dpd_interval.isEmpty()) object["dpd_interval"] = dpd_interval;
        if (!reconnect_timeout.isEmpty()) object["reconnect_timeout"] = reconnect_timeout;
        if (!trojan_interval.isEmpty()) object["trojan_interval"] = trojan_interval;
        if (queue_length > 0) object["queue_length"] = queue_length;
        if (allow_insecure_crypto) object["allow_insecure_crypto"] = true;
        const auto tlsResult = tls->Build();
        if (!tlsResult.error.isEmpty()) return {{}, tlsResult.error};
        if (!tlsResult.object.isEmpty()) object["tls"] = tlsResult.object;
        if (!form_entries.isEmpty()) {
            QJsonArray entries;
            for (const auto& entry : form_entries) {
                auto item = entry->Build().object;
                if (item.isEmpty()) continue;
                const auto rawValue = item["value"].toString();
                // A form entry is never re-asked, so a baked code would be replayed forever;
                // withholding it turns the field into a challenge the poller answers live.
                if (otp_profile_id >= 0 && !BuildingTestConfig() && rawValue.contains(kOtpPlaceholder)) continue;
                if (item.contains("value")) item["value"] = SubstituteOtp(rawValue, otpCode);
                entries.append(item);
            }
            if (!entries.isEmpty()) object["form_entries"] = entries;
        }
        if (system) object["system"] = true;
        if (!interface_name.isEmpty()) object["name"] = interface_name;
        if (!udp_timeout.isEmpty()) object["udp_timeout"] = udp_timeout;
        if (!udp_mapping.isEmpty()) object["udp_mapping"] = udp_mapping;
        if (!udp_filtering.isEmpty()) object["udp_filtering"] = udp_filtering;
        if (udp_nat_max > 0) object["udp_nat_max"] = udp_nat_max;
        return {object, ""};
    }

    QString openconnect::ComposeServer()
    {
        if (server.isEmpty()) return {};
        auto host = server;
        auto url = WrapIPV6Host(host);
        if (server_port > 0 && server_port != kOpenConnectDefaultPort) url += ":" + Int2String(server_port);
        if (!server_path.isEmpty()) {
            url += server_path.startsWith("/") ? server_path : "/" + server_path;
        }
        return url;
    }

    void openconnect::SetServerUrl(const QString& url)
    {
        auto text = url.trimmed();
        if (text.isEmpty()) return;
        if (!text.contains("://")) {
            // A bare IPv6 literal has to be bracketed before QUrl will parse it.
            auto host = text;
            text = WrapIPV6Host(host);
            text.prepend("https://");
        }
        auto parsed = QUrl(text);
        if (!parsed.isValid() || parsed.host().isEmpty()) return;
        server = parsed.host(QUrl::FullyEncoded);
        if (parsed.port() > 0) server_port = parsed.port();
        server_path = parsed.path();
        if (server_path == "/") server_path.clear();
    }

    QString openconnect::DisplayAddress()
    {
        return ComposeServer();
    }

    QString openconnect::DisplayType()
    {
        return "OpenConnect";
    }

    SecurityInfo openconnect::GetSecurity()
    {
        // The core still checks a pinned fingerprint when insecure is set.
        if (tls->insecure && tls->peer_fingerprint.isEmpty()) return {QObject::tr("Insecure TLS"), {}, SecurityLevel::Weak};
        return {QObject::tr("TLS"), {}, SecurityLevel::Secure};
    }

    bool openconnect::IsEndpoint()
    {
        return true;
    }

    bool openconnect::SupportsCredentialStrip() const
    {
        return true;
    }

    void openconnect::StripCredentials()
    {
        username.clear();
        password.clear();
        cookie.clear();
        otp_profile_id = -1;
        // Every field of the soft-token object is secret-bearing, so drop the whole object.
        token = std::make_shared<OpenConnectToken>();
        tls->client_key.clear();
        tls->client_key_path.clear();
        tls->client_key_password.clear();
        tls->mca_key.clear();
        tls->mca_key_path.clear();
        tls->mca_key_password.clear();
        for (const auto& entry : form_entries) entry->value.clear();
        // Absolute local paths: meaningless on another machine and they carry the OS user name.
        tls->certificate_authority_path.clear();
        tls->client_certificate_path.clear();
        tls->mca_certificate_path.clear();
        token->secret_path.clear();
        csd->wrapper_path.clear();
        hip->wrapper_path.clear();
        tncc->wrapper_path.clear();
        for (const auto& cert : tncc->certificates) cert->certificate_path.clear();
    }

    void openconnect::StripForeignState(Validation::Issues& issues)
    {
        if (otp_profile_id >= 0) {
            otp_profile_id = -1;
            issues << Validation::Issue{Validation::Severity::Warning, "otp_profile_id",
                                        QCoreApplication::translate("Validation", "Removed the link to a one-time password entry; choose the entry again in the profile's settings."), {}};
        }
        // Each wrapper is an executable the core starts during authentication.
        if (!csd->wrapper_path.isEmpty()) {
            issues << Validation::Issue{Validation::Severity::Warning, "csd.wrapper_path",
                                        QCoreApplication::translate("Validation", "Removed the CSD wrapper path from the imported profile; set it again in Advanced settings if you trust it."),
                                        csd->wrapper_path};
            csd->wrapper_path.clear();
        }
        if (!hip->wrapper_path.isEmpty()) {
            issues << Validation::Issue{Validation::Severity::Warning, "hip.wrapper_path",
                                        QCoreApplication::translate("Validation", "Removed the HIP wrapper path from the imported profile; set it again in Advanced settings if you trust it."),
                                        hip->wrapper_path};
            hip->wrapper_path.clear();
        }
        if (!tncc->wrapper_path.isEmpty()) {
            issues << Validation::Issue{Validation::Severity::Warning, "tncc.wrapper_path",
                                        QCoreApplication::translate("Validation", "Removed the TNCC wrapper path from the imported profile; set it again in Advanced settings if you trust it."),
                                        tncc->wrapper_path};
            tncc->wrapper_path.clear();
        }
    }
}
