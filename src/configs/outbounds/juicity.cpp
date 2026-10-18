#include "include/configs/outbounds/juicity.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool juicity::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        uuid = url.userName();
        password = url.password();
        
        tls->ParseFromLink(link);
        tls->enabled = true; // Juicity always uses tls
        
        if (server_port == 0) server_port = 443;

        return !(uuid.isEmpty() || server.isEmpty());
    }

    bool juicity::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "juicity") return false;
        outbound::ParseFromJson(object);
        if (object.contains("uuid")) uuid = object["uuid"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        return true;
    }

    void juicity::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        if (const auto message = Validation::Uuid(uuid); !message.isEmpty()) {
            issues << Validation::Issue{Validation::Severity::Error, "uuid", message, {}};
        }
    }

    QString juicity::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("juicity");
        url.setUserName(uuid);
        if (!password.isEmpty()) url.setPassword(password);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject juicity::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "juicity";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!password.isEmpty()) object["password"] = password;
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        return object;
    }

    BuildResult juicity::Build()
    {
        QJsonObject object;
        object["type"] = "juicity";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!password.isEmpty()) object["password"] = password;
        if (tls->enabled) {
            const auto tlsResult = tls->Build();
            if (!tlsResult.error.isEmpty()) return {{}, tlsResult.error};
            object["tls"] = tlsResult.object;
        }
        return {object, ""};
    }

    QString juicity::DisplayType()
    {
        return "Juicity";
    }

    SecurityInfo juicity::GetSecurity()
    {
        return SecurityFromTLS("QUIC");
    }
}
