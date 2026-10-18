#include "include/configs/outbounds/mieru.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        QJsonArray splitServerPorts(const QString& raw) {
            QJsonArray arr;
            for (auto part : raw.split(',', Qt::SkipEmptyParts)) {
                part = part.trimmed();
                if (!part.isEmpty()) arr.append(part);
            }
            return arr;
        }

        // The core also accepts the protobuf's MULTIPLEXING_DEFAULT.
        const QStringList &mieruMultiplexingLevels() {
            static const QStringList levels = mieruMultiplexing + QStringList{"MULTIPLEXING_DEFAULT"};
            return levels;
        }

        void mieruImportTuning(QString& multiplexing, QString& trafficPattern) {
            multiplexing = multiplexing.trimmed();
            trafficPattern = trafficPattern.trimmed();
            if (!AcceptOptional("multiplexing", Validation::OneOf(multiplexing, mieruMultiplexingLevels()), multiplexing)) multiplexing.clear();
            if (Validation::Base64(trafficPattern).isEmpty()) {
                while (trafficPattern.size() % 4) trafficPattern += u'=';
            }
            if (!AcceptOptional("traffic_pattern", Checks::MieruTrafficPattern(trafficPattern))) trafficPattern.clear();
        }

        // validateMieruOptions: both ends within 1-65535, begin <= end.
        QString mieruPortRange(const QString &item) {
            Validation::PortRangeStyle style{Validation::RangeSep::Dash};
            style.checkOrder = false;
            if (auto message = Validation::PortRange(item, style); !message.isEmpty()) return message;
            const QStringList sides = item.trimmed().split(u'-');
            const int begin = sides[0].toInt(), end = sides[1].toInt();
            if (begin == 0 || end == 0) return QCoreApplication::translate("Validation", "Ports must be from 1 to 65535.");
            if (begin > end)
                return QCoreApplication::translate("Validation", "The range starts after it ends; write it as %1-%2.").arg(QString::number(end), QString::number(begin));
            return {};
        }
    }

    namespace Checks {
        QString MieruServerPorts(const QString &ports) {
            return Validation::EachItem(ports, Validation::Split::Comma, mieruPortRange);
        }

        QString MieruTrafficPattern(const QString &pattern) {
            if (auto message = Validation::Base64(pattern); !message.isEmpty()) return message;
            if (pattern.trimmed().size() % 4 == 0) return {};
            return QCoreApplication::translate("Validation", "The = padding at the end is missing; copy the whole pattern.");
        }
    }

    bool mieru::ParseFromLink(const QString& link)
    {
        // Only mieru's "simple" link is mappable; the "standard" base64-protobuf link is rejected here.
        auto url = QUrl(link);
        if (!url.isValid() || url.host().isEmpty() || url.query().isEmpty()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        username = url.userName();
        password = url.password();

        // mieru pairs every port with its own protocol; we model one transport, so take the first.
        const auto protocols = query.allQueryItemValues("protocol");
        if (!protocols.isEmpty()) transport = protocols.first().toUpper();

        // The core only accepts ranges in server_ports, so an extra single port becomes "N-N".
        QStringList ranges;
        bool haveSinglePort = false;
        server_port = 0;
        for (auto port : query.allQueryItemValues("port")) {
            port = port.trimmed();
            if (port.isEmpty()) continue;
            if (port.contains('-')) {
                ranges << port;
            } else if (!ParseDiag::Accept("port", Validation::Port(port), port, Validation::Severity::Error)) {
                continue;
            } else if (!haveSinglePort) {
                server_port = port.toInt();
                haveSinglePort = true;
            } else {
                ranges << QStringLiteral("%1-%1").arg(port);
            }
        }
        server_ports = ranges.join(",");

        if (query.hasQueryItem("multiplexing")) multiplexing = query.queryItemValue("multiplexing");
        // mieru percent-encodes the base64 '+' and '/', which only FullyDecoded turns back.
        if (query.hasQueryItem("traffic-pattern")) traffic_pattern = query.queryItemValue("traffic-pattern", QUrl::FullyDecoded);
        mieruImportTuning(multiplexing, traffic_pattern);

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
            // sing-box also takes a single range as a plain string.
            if (const auto value = object["server_ports"]; value.isString()) {
                server_ports = value.toString().trimmed();
            } else {
                QStringList ports;
                for (const auto& v : value.toArray()) ports << v.toString().trimmed();
                server_ports = ports.join(",");
            }
        }
        if (ParseDiag::Active()) {
            transport = transport.trimmed().toUpper();
            mieruImportTuning(multiplexing, traffic_pattern);
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
        // mieru carries no authority port: every port is a repeated "port" item paired with a "protocol".
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
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
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

    bool mieru::ServerPortOptional()
    {
        return !splitServerPorts(server_ports).isEmpty();
    }

    void mieru::Validate(Validation::Issues &issues)
    {
        outbound::Validate(issues);
        using Validation::Severity;
        if (auto message = Validation::Required(username); !message.isEmpty()) issues.append({Severity::Error, "username", message});
        if (auto message = Validation::Required(password); !message.isEmpty()) issues.append({Severity::Error, "password", message});
        if (auto message = Validation::OneOf(transport, mieruTransports); !message.isEmpty()) issues.append({Severity::Error, "transport", message, transport});
        if (auto message = Validation::OneOf(multiplexing, mieruMultiplexingLevels()); !message.isEmpty())
            issues.append({Severity::Error, "multiplexing", message, multiplexing});
        if (auto message = Checks::MieruServerPorts(server_ports); !message.isEmpty()) issues.append({Severity::Error, "server_ports", message, server_ports});
        if (auto message = Checks::MieruTrafficPattern(traffic_pattern); !message.isEmpty()) issues.append({Severity::Error, "traffic_pattern", message});
    }

    SecurityInfo mieru::GetSecurity()
    {
        return {QObject::tr("Encrypted"), {}, SecurityLevel::Secure};
    }
}
