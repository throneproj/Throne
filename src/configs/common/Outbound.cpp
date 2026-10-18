#include "include/configs/common/Outbound.h"
#include "include/configs/common/utils.h"
#include "include/configs/outbounds/juicity.h"
#include "include/configs/outbounds/masque.h"
#include "include/configs/outbounds/naive.h"
#include "include/configs/outbounds/trusttunnel.h"

#include <QCoreApplication>

#include <limits>

namespace Configs {
    namespace {
        // sing-box common/dialer/fragment.go: chunks go through a 1024-byte buffer, five of them into a 2048-byte queue.
        constexpr qint64 kFragmentMaxChunk = 1019;
        constexpr qint64 kFragmentSafeChunk = 400;

        // The larger bound of a spec that already passed IntOrRange.
        qint64 outboundFragmentMax(const QString& spec)
        {
            const auto t = spec.trimmed();
            return Validation::ToInt(t.mid(t.indexOf('-') + 1)).value_or(0);
        }

        // The dialer-level fragment wraps only TCP dials (sing-box common/dialer/extended_tcp.go).
        bool outboundDialsTcp(outbound& out)
        {
            // warpmasque falls back to HTTP/2 over TCP unless that is turned off.
            if (const auto m = dynamic_cast<masque*>(&out)) return m->http_version == 2 || !m->disable_version_fallback;
            if (out.HasQUIC() || dynamic_cast<juicity*>(&out) != nullptr) return false;
            if (const auto t = dynamic_cast<trusttunnel*>(&out)) return !t->quic;
            if (const auto n = dynamic_cast<naive*>(&out)) return !n->quic;
            return !out.HasTransport() || out.GetTransport()->type != "quic";
        }

        QString outboundFragmentSize()
        {
            const auto value = Configs::dataManager->settingsRepo->fragment_size.trimmed();
            return value.isEmpty() ? QStringLiteral("10-100") : value;
        }

        QString outboundFragmentSleep()
        {
            const auto value = Configs::dataManager->settingsRepo->fragment_sleep.trimmed();
            return value.isEmpty() ? QStringLiteral("2-5") : value;
        }
    }

    namespace Checks {
        QString TlsFragmentSize(const QString& spec)
        {
            if (auto message = Validation::IntOrRange(spec, 0, kFragmentMaxChunk); !message.isEmpty()) return message;
            if (!spec.trimmed().isEmpty() && outboundFragmentMax(spec) == 0) {
                return QCoreApplication::translate("Validation", "The largest fragment must be at least 1 byte, for example 10-100.");
            }
            return {};
        }

        QString TlsFragmentSizeLarge(const QString& spec)
        {
            if (!TlsFragmentSize(spec).isEmpty() || outboundFragmentMax(spec) <= kFragmentSafeChunk) return {};
            return QCoreApplication::translate("Validation", "Fragments above 400 bytes can fail the connection when the ClientHello is very large; a maximum of 400 or less is always safe.");
        }

        QString TlsFragmentSleep(const QString& spec)
        {
            return Validation::IntOrRange(spec, 0, std::numeric_limits<qint64>::max());
        }
    }

    QString FragmentSettingsError()
    {
        if (Configs::dataManager->settingsRepo->fragment_implementation != "custom") return {};
        const auto size = outboundFragmentSize();
        const auto sleep = outboundFragmentSleep();
        if (const auto message = Checks::TlsFragmentSize(size); !message.isEmpty()) {
            return QCoreApplication::translate("Validation", "The TLS fragment size \"%1\" in Preset Settings > TLS is invalid. %2").arg(size, message);
        }
        if (const auto message = Checks::TlsFragmentSleep(sleep); !message.isEmpty()) {
            return QCoreApplication::translate("Validation", "The TLS fragment sleep \"%1\" in Preset Settings > TLS is invalid. %2").arg(sleep, message);
        }
        return {};
    }

    bool CustomFragmentApplies(outbound& out)
    {
        if (!out.HasTLS() || Configs::dataManager->settingsRepo->fragment_implementation != "custom") return false;
        const auto tls = out.GetTLS();
        return tls->enabled && tls->FragmentEffectivelyOn() && outboundDialsTcp(out);
    }

    QString DisplayTransportName(const QString& type)
    {
        // tcp (sing-box) and raw (Xray) mean no transport.
        if (type.isEmpty() || type == "tcp" || type == "raw") return {};
        if (type == "ws") return "WebSocket";
        if (type == "grpc") return "gRPC";
        if (type == "http") return "HTTP";
        if (type == "httpupgrade") return "HTTPUpgrade";
        if (type == "xhttp") return "XHTTP";
        if (type == "quic") return "QUIC";
        return type.toUpper();
    }

    SecurityInfo outbound::SecurityFromTLS(const QString& transport)
    {
        SecurityInfo info;
        info.transport = transport;
        if (HasTLS()) {
            auto tls = GetTLS();
            // TLS::Build emits nothing while TLS is off, Reality included.
            if (tls->enabled || MustTLS()) {
                if (tls->reality->enabled) {
                    info.label = QObject::tr("Reality");
                    info.level = SecurityLevel::Secure;
                } else if (!tls->certificate_sha256.isEmpty() || !tls->certificate_public_key_sha256.isEmpty()) {
                    // The core checks a pinned certificate or key in place of the CA chain, insecure or not.
                    info.label = QObject::tr("TLS");
                    info.level = SecurityLevel::Secure;
                } else if (tls->insecure) {
                    info.label = QObject::tr("Insecure TLS");
                    info.level = SecurityLevel::Weak;
                } else {
                    info.label = QObject::tr("TLS");
                    info.level = SecurityLevel::Secure;
                    info.caVerified = true;
                }
                return info;
            }
        }
        info.label = QObject::tr("Raw");
        info.level = SecurityLevel::None;
        return info;
    }

    SecurityInfo WithPrivateServer(SecurityInfo info, const QString& host)
    {
        if (info.isDangerous() && !info.publicLeg && IsPrivateHost(host)) {
            info.label = QObject::tr("Private");
            info.level = SecurityLevel::Secure;
            info.compromised = false;
        }
        return info;
    }

    SecurityInfo WithXrayDownloadLeg(SecurityInfo info, const QString& host, const QList<QJsonObject>& leg)
    {
        if (leg.isEmpty() || info.level == SecurityLevel::Unknown) return info;
        QString server;
        const auto protection = XrayDownloadLegProtection(leg, server);
        if (protection == XrayLegProtection::Verified || IsPrivateHost(server)) return info;
        const auto level = protection == XrayLegProtection::Plain ? SecurityLevel::None : SecurityLevel::Weak;
        // A label that already counts gives way only to a weaker leg.
        if (info.isDangerous() && !IsPrivateHost(host) && info.level <= level) return info;
        info.label = protection == XrayLegProtection::Plain ? QObject::tr("Raw download") : QObject::tr("Insecure TLS download");
        info.level = level;
        info.caVerified = false;
        info.publicLeg = true;
        return info;
    }

    SecurityInfo outbound::EffectiveSecurity()
    {
        auto info = GetSecurity();
        // Only TLS::Build injects skip_cert: naive builds its own TLS object, and custom JSON passes through untouched.
        if (info.caVerified && HasTLS() && !LimitedTLS() && Configs::dataManager->settingsRepo->skip_cert) {
            info.label = QObject::tr("Compromised");
            info.level = SecurityLevel::Weak;
            info.compromised = true;
        }
        if (!info.isDangerous()) return info;
        return WithPrivateServer(info, GetAddress());
    }

    SecurityInfo outbound::GetSecurity()
    {
        if (IsXray()) {
            auto stream = GetXrayStream();
            SecurityInfo info;
            info.transport = DisplayTransportName(stream->network);
            if (stream->security == "reality") {
                info.label = QObject::tr("Reality");
                info.level = SecurityLevel::Secure;
            } else if (stream->security == "tls") {
                info.label = QObject::tr("TLS");
                info.level = SecurityLevel::Secure;
            } else {
                info.label = QObject::tr("Raw");
                info.level = SecurityLevel::None;
            }
            return info;
        }

        return SecurityFromTLS(HasTransport() ? DisplayTransportName(GetTransport()->type) : QString());
    }

    QString outbound::DisplaySecurity()
    {
        auto info = EffectiveSecurity();
        if (info.label.isEmpty()) return {};
        auto text = info.transport.isEmpty() ? info.label
                                             : QStringLiteral("%1+%2").arg(info.transport, info.label);
        if (info.isDangerous()) return QStringLiteral("⚠️ ") + text;
        return text;
    }

    bool outbound::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid())
        {
            if(!url.errorString().startsWith("Invalid port"))
                return false;
            server_port = 0;
        } else {
            server_port = url.port(0);
        }

        if (url.hasFragment()) name = url.fragment(QUrl::FullyDecoded);
        // FullyEncoded keeps an IDN host in its xn-- form, which is what DNS carries
        server = url.host(QUrl::FullyEncoded);
        dialFields->ParseFromLink(link);
        return true;
    }
    bool outbound::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("tag")) name = object["tag"].toString();
        if (object.contains("server")) server = toAceHost(object["server"].toString());
        if (object.contains("server_port")) server_port = object["server_port"].toInt();
        dialFields->ParseFromJson(object);
        return true;
    }
    bool outbound::ParseFromClash(const clash::Proxies& object)
    {
        name = QString::fromStdString(object.name);
        server = toAceHost(QString::fromStdString(object.server));
        server_port = object.port;
        return true;
    }
    QString outbound::ExportToLink()
    {
        QUrlQuery query;
        mergeUrlQuery(query, dialFields->ExportToLink());
        return query.toString();
    }
    QJsonObject outbound::ExportToJson()
    {
        QJsonObject object;
        if (!name.isEmpty()) object["tag"] = name;
        if (!server.isEmpty()) object["server"] = server;
        if (server_port > 0) object["server_port"] = server_port;
        auto dialFieldsObj = dialFields->ExportToJson();
        mergeJsonObjects(object, dialFieldsObj);
        return object;
    }
    QJsonObject outbound::ExportIdentity()
    {
        if (server.isEmpty()) return ExportToJson();

        QJsonObject object;
        object["server"] = server;
        object["server_port"] = server_port;
        if (IsXray()) {
            if (auto s = GetXrayStream()->ExportIdentity(); !s.isEmpty()) object["stream"] = s;
        } else {
            if (HasTLS()) {
                if (auto t = GetTLS()->ExportIdentity(); !t.isEmpty()) object["tls"] = t;
            }
            if (HasTransport()) {
                if (auto t = GetTransport()->ExportIdentity(); !t.isEmpty()) object["transport"] = t;
            }
        }
        return object;
    }

    void outbound::Validate(Validation::Issues &issues)
    {
        using Validation::Severity;
        const auto add = [&issues](Severity severity, const QString& field, const QString& message, const QString& value = {}) {
            if (!message.isEmpty()) issues.append({severity, field, message, value});
        };
        const auto address = GetAddress().trimmed();
        if (HasServerEndpoint()) {
            if (address.isEmpty()) {
                if (!ServerAddressOptional()) add(Severity::Error, "server", QCoreApplication::translate("Validation", "The server address is missing."));
            } else {
                add(Severity::Error, "server", Validation::Host(address), address);
            }
            if (const auto port = GetPort().trimmed(); Validation::ToInt(port).value_or(0) == 0) {
                if (!ServerPortOptional()) add(Severity::Error, "server_port", QCoreApplication::translate("Validation", "The server port is missing."));
            } else {
                add(Severity::Error, "server_port", Validation::Port(port), port);
            }
        }

        bool tlsOn = false, ownUtls = false, realityOn = false;
        if (HasTLS()) {
            const auto tls = GetTLS();
            tlsOn = tls->enabled || MustTLS();
            if (tlsOn) {
                tls->Validate(issues, address, LimitedTLS());
                const auto fingerprint = tls->utls->fingerPrint.trimmed();
                ownUtls = tls->utls->supported && tls->utls->enabled && !fingerprint.isEmpty() && fingerprint != tlsUnsafeFingerprint;
                realityOn = tls->reality->enabled || !tls->reality->public_key.isEmpty();
            }
        }
        if (HasTransport()) GetTransport()->Validate(issues, tlsOn, ownUtls, realityOn);
        if (HasQUIC()) GetQUIC()->Validate(issues);
        if (HasMux()) GetMux()->Validate(issues);
        dialFields->Validate(issues);
        if (HasXrayStream()) GetXrayStream()->Validate(issues, address);
    }

    BuildResult outbound::Build()
    {
        QJsonObject object;
        if (!server.isEmpty()) object["server"] = server;
        if (server_port > 0) object["server_port"] = server_port;
        mergeJsonObjects(object, dialFields->Build().object);
        // The custom fragment implementation lives at the dialer level, a sibling of "tls", not inside it.
        if (CustomFragmentApplies(*this)) {
            if (const auto error = FragmentSettingsError(); !error.isEmpty()) {
                qWarning() << "Invalid TLS fragment settings:" << error;
                return {object, error};
            }
            // the core rejects tls_fragment combined with tcp_fast_open
            object.remove("tcp_fast_open");
            object["tls_fragment"] = QJsonObject{
                {"enabled", true},
                {"size", outboundFragmentSize()},
                {"sleep", outboundFragmentSleep()},
            };
        }
        return {object, ""};
    }
}


