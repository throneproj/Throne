#include "include/configs/outbounds/trojan.h"

#include <QCoreApplication>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    bool Trojan::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;

        outbound::ParseFromLink(link);
        password = url.userName();
        if (server_port == 0) server_port = 443;
        tls->ParseFromLink(link);
        if (!tls->enabled && QUrlQuery(url.query()).queryItemValue("security").trimmed().isEmpty()) {
            ParseDiag::Warn("security", QCoreApplication::translate("Validation", "The link sets neither security nor sni, so the profile connects without TLS, which most Trojan servers refuse."));
        }
        transport->ParseFromLink(link);
        multiplex->ParseFromLink(link);
        return true;
    }
    bool Trojan::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "trojan") return false;
        outbound::ParseFromJson(object);
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        if (object.contains("transport")) transport->ParseFromJson(object["transport"].toObject());
        if (object.contains("multiplex")) multiplex->ParseFromJson(object["multiplex"].toObject());
        return true;
    }
    bool Trojan::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "trojan") return false;
        outbound::ParseFromClash(object);
        password = QString::fromStdString(object.password);

        tls->ParseFromClash(object);
        tls->enabled = true;
        transport->ParseFromClash(object);
        multiplex->ParseFromClash(object);
        return true;
    }
    QString Trojan::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setHost(server);
        url.setPort(server_port);
        url.setScheme("trojan");
        if (!name.isEmpty()) url.setFragment(name);
        url.setUserName(password);
        if (tls->enabled) mergeUrlQuery(query, tls->ExportToLink());
        if (!transport->type.isEmpty()) mergeUrlQuery(query, transport->ExportToLink(tls->enabled));
        if (multiplex->enabled) mergeUrlQuery(query, multiplex->ExportToLink());
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }
    QJsonObject Trojan::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "trojan";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!password.isEmpty()) object["password"] = password;
        if (auto tlsObj = tls->ExportToJson(); !tlsObj.isEmpty()) object["tls"] = tlsObj;
        if (auto transportObj = transport->ExportToJson(); !transportObj.isEmpty()) object["transport"] = transportObj;
        if (auto muxObj = multiplex->ExportToJson(); !muxObj.isEmpty()) object["multiplex"] = muxObj;
        return object;
    }
    BuildResult Trojan::Build()
    {
        QJsonObject object;
        object["type"] = "trojan";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        if (!password.isEmpty()) object["password"] = password;
        const auto tlsResult = tls->Build();
        if (!tlsResult.error.isEmpty()) return {{}, tlsResult.error};
        if (!tlsResult.object.isEmpty()) object["tls"] = TlsForTransport(tlsResult.object, transport->type);
        const auto transportResult = transport->Build();
        if (!transportResult.error.isEmpty()) return {{}, transportResult.error};
        if (!transportResult.object.isEmpty()) object["transport"] = transportResult.object;
        const auto muxResult = multiplex->Build();
        if (!muxResult.error.isEmpty()) return {{}, muxResult.error};
        if (!muxResult.object.isEmpty()) object["multiplex"] = muxResult.object;
        return {object, ""};
    }

    QString Trojan::DisplayType()
    {
        return "Trojan";
    }

    void Trojan::Validate(Validation::Issues &issues)
    {
        outbound::Validate(issues);
        if (auto message = Validation::Required(password); !message.isEmpty()) issues.append({Validation::Severity::Error, "password", message});
    }
}


