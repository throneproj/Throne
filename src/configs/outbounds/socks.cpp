#include "include/configs/outbounds/socks.h"

#include <QCoreApplication>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // Build() emits SOCKS5 for anything but 4, which is also the core's default.
        void parseSocksVersion(const QString& text, int& version, bool& socks4a)
        {
            const auto value = text.trimmed().toLower();
            socks4a = value == "4a";
            version = socks4a || value == "4" ? 4 : 5;
            if (!value.isEmpty() && version == 5 && value != "5") {
                ParseDiag::Warn("version", QCoreApplication::translate("Validation", "Unknown SOCKS version, so SOCKS5 was used. Use 4, 4a or 5."), text.trimmed());
            }
        }
    }

    namespace Checks {
        QString SocksAuth(int version, const QString &username, const QString &password)
        {
            if (password.isEmpty()) return {};
            if (version == 4) return QCoreApplication::translate("Validation", "SOCKS4 sends no password, so it is ignored.");
            if (username.isEmpty()) return QCoreApplication::translate("Validation", "Enter a user name too; without one the password is not sent.");
            return {};
        }
    }

    bool socks::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        if (query.hasQueryItem("version"))
        {
            parseSocksVersion(query.queryItemValue("version"), version, socks4a);
        } else
        {
            if (url.scheme() == "socks4" || url.scheme() == "socks4a") version = 4;
            if (url.scheme() == "socks5") version = 5;
            socks4a = url.scheme() == "socks4a";
        }
        
        if (!url.password().isEmpty() || !url.userName().isEmpty()) {
            username = url.userName();
            password = url.password();
            
            // v2rayN base64-encodes "user:pass" into the username; a plain name like "user" is valid base64 too.
            if (password.isEmpty() && !username.isEmpty()) {
                QString decoded = DecodeB64IfValid(username);
                if (decoded.contains(':')) {
                    username = SubStrBefore(decoded, ":");
                    password = SubStrAfter(decoded, ":");
                }
            }
        }

        if (query.hasQueryItem("uot")) uot = query.queryItemValue("uot") == "true" || query.queryItemValue("uot").toInt() > 0;
        
        if (server_port == 0) server_port = 1080;

        return !server.isEmpty();
    }

    bool socks::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "socks") return false;
        outbound::ParseFromJson(object);
        if (object.contains("username")) username = object["username"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("version")) parseSocksVersion(object["version"].toVariant().toString(), version, socks4a);
        if (object.contains("uot")) uot = object["uot"].toBool();
        if (object.contains("udp_over_tcp"))
        {
            if (object["udp_over_tcp"].isBool()) uot = object["udp_over_tcp"].toBool();
            if (object["udp_over_tcp"].isObject()) uot = object["udp_over_tcp"].toObject()["enabled"].toBool();
        }
        return true;
    }

    bool socks::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "socks5") return false;
        outbound::ParseFromClash(object);
        username = QString::fromStdString(object.username);
        password = QString::fromStdString(object.password);

        return true;
    }

    QString socks::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        
        url.setScheme(version != 4 ? "socks5" : socks4a ? "socks4a" : "socks4");
        
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);
        
        if (!username.isEmpty()) url.setUserName(username);
        if (!password.isEmpty()) url.setPassword(password);
        if (uot) query.addQueryItem("uot", "1");
        
        mergeUrlQuery(query, outbound::ExportToLink());
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject socks::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "socks";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (version == 4) object["version"] = socks4a ? "4a" : "4";
        if (uot) object["uot"] = uot;
        return object;
    }

    BuildResult socks::Build()
    {
        QJsonObject object;
        object["type"] = "socks";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (version == 4) object["version"] = socks4a ? "4a" : "4";
        if (uot) object["udp_over_tcp"] = true;
        return {object, ""};
    }

    QString socks::DisplayType()
    {
        return "Socks";
    }

    void socks::Validate(Validation::Issues &issues)
    {
        outbound::Validate(issues);
        if (auto message = Checks::SocksAuth(version, username, password); !message.isEmpty())
            issues.append({Validation::Severity::Warning, "password", message});
    }
}
