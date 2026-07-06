#include "include/configs/outbounds/mieru.h"

#include <QJsonArray>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // Split the user-entered comma-separated port ranges into a JSON string array.
        QJsonArray splitServerPorts(const QString& raw) {
            QJsonArray arr;
            for (auto part : raw.split(',', Qt::SkipEmptyParts)) {
                part = part.trimmed();
                if (!part.isEmpty()) arr.append(part);
            }
            return arr;
        }
    }

    bool mieru::ParseFromLink(const QString& link)
    {
        // We support mieru's "simple" sharing link (mierus://user:pass@host?...).
        // The "standard" mieru:// link is a base64-encoded protobuf blob with no
        // authority/query that cannot be mapped onto our fields; reject it here so
        // it is discarded cleanly instead of imported as a garbage profile.
        auto url = QUrl(link);
        if (!url.isValid() || url.host().isEmpty() || url.query().isEmpty()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        username = url.userName();
        password = url.password();

        // mieru pairs every port with its own protocol; we only model a single
        // transport, so adopt the first one advertised.
        const auto protocols = query.allQueryItemValues("protocol");
        if (!protocols.isEmpty()) transport = protocols.first().toUpper();

        // mieru lists each port (single or range) as a repeated "port" item. Map
        // the first single port to server_port (the shared address:port field) and
        // keep the rest in server_ports, normalising any extra single port to an
        // "N-N" range since the core only accepts ranges in server_ports.
        QStringList ranges;
        bool haveSinglePort = false;
        server_port = 0;
        for (auto port : query.allQueryItemValues("port")) {
            port = port.trimmed();
            if (port.isEmpty()) continue;
            if (port.contains('-')) {
                ranges << port;
            } else if (!haveSinglePort) {
                server_port = port.toInt();
                haveSinglePort = true;
            } else {
                ranges << QStringLiteral("%1-%1").arg(port);
            }
        }
        server_ports = ranges.join(",");

        if (query.hasQueryItem("multiplexing")) multiplexing = query.queryItemValue("multiplexing");
        if (query.hasQueryItem("traffic-pattern")) traffic_pattern = query.queryItemValue("traffic-pattern");

        return true;
    }

    bool mieru::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "mieru") return false;
        outbound::ParseFromJson(object);
        if (object.contains("transport")) transport = object["transport"].toString();
        if (object.contains("username")) username = object["username"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("multiplexing")) multiplexing = object["multiplexing"].toString();
        if (object.contains("traffic_pattern")) traffic_pattern = object["traffic_pattern"].toString();
        if (object.contains("server_ports")) {
            QStringList ports;
            for (const auto& v : object["server_ports"].toArray()) ports << v.toString();
            server_ports = ports.join(",");
        }
        return true;
    }

    bool mieru::ParseFromClash(const clash::Proxies& object)
    {
        // Clash does not support the mieru protocol.
        return false;
    }

    QString mieru::ExportToLink()
    {
        // Emit mieru's "simple" sharing link:
        //   mierus://user:pass@host?profile=...&port=...&protocol=...
        // mieru carries no authority port, so every port (the single server_port
        // plus each server_ports range) is emitted as a repeated "port" item, each
        // paired with a matching "protocol" so the two lists line up.
        QUrl url;
        QUrlQuery query;
        url.setScheme("mierus");
        url.setUserName(username);
        url.setPassword(password);
        url.setHost(server);
        if (!name.isEmpty()) url.setFragment(name);

        // mieru requires a profile name; we don't model one, so use "default".
        query.addQueryItem("profile", "default");

        const auto protocol = transport.isEmpty() ? QStringLiteral("TCP") : transport.toUpper();
        auto addPort = [&](const QString& port) {
            query.addQueryItem("port", port);
            query.addQueryItem("protocol", protocol);
        };
        if (server_port > 0) addPort(QString::number(server_port));
        for (auto part : server_ports.split(',', Qt::SkipEmptyParts)) {
            part = part.trimmed();
            if (!part.isEmpty()) addPort(part);
        }

        if (!multiplexing.isEmpty()) query.addQueryItem("multiplexing", multiplexing);
        if (!traffic_pattern.isEmpty()) query.addQueryItem("traffic-pattern", traffic_pattern);

        mergeUrlQuery(query, outbound::ExportToLink());

        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject mieru::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "mieru";
        mergeJsonObjects(object, outbound::ExportToJson());
        object["transport"] = transport.isEmpty() ? QString("TCP") : transport;
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (!multiplexing.isEmpty()) object["multiplexing"] = multiplexing;
        if (!traffic_pattern.isEmpty()) object["traffic_pattern"] = traffic_pattern;
        auto ports = splitServerPorts(server_ports);
        if (!ports.isEmpty()) object["server_ports"] = ports;
        return object;
    }

    BuildResult mieru::Build()
    {
        QJsonObject object;
        object["type"] = "mieru";
        mergeJsonObjects(object, outbound::Build().object);
        object["transport"] = transport.isEmpty() ? QString("TCP") : transport;
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (!multiplexing.isEmpty()) object["multiplexing"] = multiplexing;
        if (!traffic_pattern.isEmpty()) object["traffic_pattern"] = traffic_pattern;
        auto ports = splitServerPorts(server_ports);
        if (!ports.isEmpty()) object["server_ports"] = ports;
        return {object, ""};
    }

    QString mieru::DisplayType()
    {
        return "Mieru";
    }
}
