#include "include/configs/outbounds/shadowtls.h"

#include <QCoreApplication>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        QString shadowTlsVersionMessage()
        {
            return QCoreApplication::translate("Validation", "The version must be 1, 2 or 3.");
        }
    }

    namespace Checks {
        QString ShadowTlsPassword(int version, const QString &password)
        {
            if (version < 2 || !password.isEmpty()) return {};
            return QCoreApplication::translate("Validation", "ShadowTLS v2 and v3 need the password set on the server.");
        }
    }

    bool shadowtls::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        if (const auto raw = query.queryItemValue("version").trimmed(); !raw.isEmpty())
        {
            const auto parsed = Validation::ToInt(raw);
            if (!parsed || *parsed < 0 || *parsed > 3) {
                ParseDiag::Error("version", shadowTlsVersionMessage(), raw);
                return false;
            }
            version = int(*parsed);
        }
        password = url.password();
        
        tls->ParseFromLink(link);
        tls->enabled = true; // ShadowTLS always uses tls
        
        if (server_port == 0) server_port = 443;

        return !server.isEmpty();
    }

    bool shadowtls::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "shadowtls") return false;
        outbound::ParseFromJson(object);
        if (object.contains("version")) version = object["version"].toVariant().toInt();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        return true;
    }

    QString shadowtls::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("shadowtls");
        if (version > 1 && !password.isEmpty()) url.setPassword(password);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        query.addQueryItem("version", Int2String(version));

        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject shadowtls::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "shadowtls";
        mergeJsonObjects(object, outbound::ExportToJson());
        object["version"] = version;
        if (version > 1 && !password.isEmpty()) object["password"] = password;
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        return object;
    }

    QJsonObject shadowtls::ExportIdentity()
    {
        auto object = outbound::ExportIdentity();
        object["version"] = version;
        return object;
    }

    BuildResult shadowtls::Build()
    {
        QJsonObject object;
        object["type"] = "shadowtls";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        object["version"] = version;
        if (version > 1 && !password.isEmpty()) object["password"] = password;
        if (tls->enabled) {
            const auto tlsResult = tls->Build();
            if (!tlsResult.error.isEmpty()) return {{}, tlsResult.error};
            object["tls"] = tlsResult.object;
        }
        return {object, ""};
    }

    QString shadowtls::DisplayType()
    {
        return "ShadowTLS";
    }

    void shadowtls::Validate(Validation::Issues &issues)
    {
        outbound::Validate(issues);
        using Validation::Severity;
        if (version < 0 || version > 3) {
            issues.append({Severity::Error, "version", shadowTlsVersionMessage(), QString::number(version)});
        } else if (auto message = Checks::ShadowTlsPassword(version, password); !message.isEmpty()) {
            issues.append({Severity::Error, "password", message});
        }
    }
}
