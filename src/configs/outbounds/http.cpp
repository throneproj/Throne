#include "include/configs/outbounds/http.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool http::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        username = url.userName();
        password = url.password();

        // Handle v2rayN format: credentials are base64-encoded in the username field
        // e.g. http://base64(token)@server:port
        // If decoded string has no ':', SubStrBefore/After both return the full string,
        // so username == password == decoded_token (matches expected behavior).
        if (password.isEmpty() && !username.isEmpty()) {
            QString decoded = DecodeB64IfValid(username);
            if (!decoded.isEmpty()) {
                username = SubStrBefore(decoded, ":");
                password = SubStrAfter(decoded, ":");
            }
        }

        // Handle single-credential format: http://:token@server:port
        // Some providers set password only; username should equal password.
        if (username.isEmpty() && !password.isEmpty()) {
            username = password;
        }

        if (query.hasQueryItem("path")) path = query.queryItemValue("path");
        if (query.hasQueryItem("headers")) headers = query.queryItemValue("headers").split(",");
        if (url.scheme() == "https" || query.queryItemValue("security") == "tls")
        {
            tls->ParseFromLink(link);
            tls->enabled = true; // force enable in case scheme is https and no security queryValue is set
        }
        if (server_port == 0) server_port = 443;
        return true;
    }
    bool http::ParseFromJson(const QJsonObject& object)
    {
        if (object.empty() || object["type"] != "http") return false;
        outbound::ParseFromJson(object);
        if (object.contains("username")) username = object["username"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("path")) path = object["path"].toString();
        if (object.contains("headers") && object["headers"].isObject()) headers = jsonObjectToQStringList(object["headers"].toObject());
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        return true;
    }
    QString http::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme(tls->enabled ? "https" : "http");
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);
        if (!username.isEmpty()) url.setUserName(username);
        if (!password.isEmpty()) url.setPassword(password);
        if (!path.isEmpty()) query.addQueryItem("path", path);
        if (!headers.empty()) query.addQueryItem("headers", headers.join(","));
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }
    QJsonObject http::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "http";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (!path.isEmpty()) object["path"] = path;
        if (auto headerObj = qStringListToJsonObject(headers); !headerObj.isEmpty()) object["headers"] = headerObj;
        if (auto tlsObj = tls->ExportToJson(); !tlsObj.isEmpty()) object["tls"] = tlsObj;
        return object;
    }
    BuildResult http::Build()
    {
        QJsonObject object;
        object["type"] = "http";
        mergeJsonObjects(object, outbound::Build().object);
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (!path.isEmpty()) object["path"] = path;
        if (auto headerObj = qStringListToJsonObject(headers); !headerObj.isEmpty()) object["headers"] = headerObj;
        if (auto tlsObj = tls->Build().object; !tlsObj.isEmpty()) object["tls"] = tlsObj;
        return {object, ""};
    }

    QString http::DisplayType()
    {
        return "HTTP";
    }
}


