#include "include/configs/common/Outbound.h"
#include "include/configs/common/utils.h"

namespace Configs {
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
            if (tls->reality->enabled) {
                info.label = QObject::tr("Reality");
                info.level = SecurityLevel::Secure;
                return info;
            }
            if (tls->enabled || MustTLS()) {
                if (tls->insecure) {
                    info.label = QObject::tr("Insecure TLS");
                    info.level = SecurityLevel::Weak;
                } else {
                    info.label = QObject::tr("TLS");
                    info.level = SecurityLevel::Secure;
                }
                return info;
            }
        }
        info.label = QObject::tr("Raw");
        info.level = SecurityLevel::None;
        return info;
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
        auto info = GetSecurity();
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
    BuildResult outbound::Build()
    {
        QJsonObject object;
        if (!server.isEmpty()) object["server"] = server;
        if (server_port > 0) object["server_port"] = server_port;
        mergeJsonObjects(object, dialFields->Build().object);
        // The custom fragment implementation lives at the dialer level, a sibling of "tls", not inside it.
        if (HasTLS()) {
            auto t = GetTLS();
            if (t->enabled && t->FragmentEffectivelyOn() &&
                Configs::dataManager->settingsRepo->fragment_implementation == "custom") {
                // the core rejects tls_fragment combined with tcp_fast_open
                object.remove("tcp_fast_open");
                object["tls_fragment"] = QJsonObject{
                    {"enabled", true},
                    {"size", Configs::dataManager->settingsRepo->fragment_size.isEmpty() ? QString("10-100") : Configs::dataManager->settingsRepo->fragment_size},
                    {"sleep", Configs::dataManager->settingsRepo->fragment_sleep.isEmpty() ? QString("2-5") : Configs::dataManager->settingsRepo->fragment_sleep},
                };
            }
        }
        return {object, ""};
    }
}


