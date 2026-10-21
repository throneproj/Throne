#include "include/configs/outbounds/http.h"

#include <QCoreApplication>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace Checks {
        QString HttpAuth(const QString &username, const QString &password)
        {
            if (password.isEmpty() || !username.isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Enter a user name too; without one the password is not sent.");
        }
    }

    bool http::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        username = url.userName();
        password = url.password();

        // v2rayN base64s "user:pass" into the username; a plain name like "user" is valid base64 too.
        if (password.isEmpty() && !username.isEmpty()) {
            QString decoded = DecodeB64IfValid(username);
            if (decoded.contains(':')) {
                username = SubStrBefore(decoded, ":");
                password = SubStrAfter(decoded, ":");
            }
        }

        // Some providers set only the password (http://:token@host); username must equal it.
        if (username.isEmpty() && !password.isEmpty()) {
            username = password;
        }

        if (query.hasQueryItem("path")) path = query.queryItemValue("path");
        if (const auto raw = query.queryItemValue("headers"); !raw.isEmpty()) {
            headers = raw.split(",");
            if (headers.size() % 2 != 0) {
                ParseDiag::Warn("headers", QCoreApplication::translate("Validation", "The headers must alternate names and values separated by commas, for example User-Agent,curl, so they were ignored."));
                headers.clear();
            }
        }
        if (url.scheme() == "https" || query.queryItemValue("security") == "tls")
        {
            tls->ParseFromLink(link);
            tls->enabled = true; // force enable in case scheme is https and no security queryValue is set
        }
        if (server_port == 0) server_port = tls->enabled ? 443 : 80;
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
    bool http::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "http") return false;
        outbound::ParseFromClash(object);
        username = QString::fromStdString(object.username);
        password = QString::fromStdString(object.password);

        tls->ParseFromClash(object, object.tls);
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
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (!path.isEmpty()) object["path"] = path;
        if (auto headerObj = qStringListToJsonObject(headers); !headerObj.isEmpty()) object["headers"] = headerObj;
        const auto tlsResult = tls->Build();
        if (!tlsResult.error.isEmpty()) return {{}, tlsResult.error};
        if (!tlsResult.object.isEmpty()) object["tls"] = tlsResult.object;
        return {object, ""};
    }

    QString http::DisplayType()
    {
        return "HTTP";
    }

    void http::Validate(Validation::Issues &issues)
    {
        outbound::Validate(issues);
        if (auto message = Checks::HttpAuth(username, password); !message.isEmpty())
            issues.append({Validation::Severity::Warning, "password", message});
    }
}


