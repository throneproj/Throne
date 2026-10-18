#include "include/configs/common/QUICFields.h"

#include <algorithm>
#include <QCoreApplication>
#include <QUrlQuery>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // sing-box MemoryBytes: a JSON integer, or a string that must carry a unit ("1m"); a bare digit string fails.
        QJsonValue memoryBytesJson(const QString& text)
        {
            const bool digitsOnly = !text.isEmpty() && std::all_of(text.cbegin(), text.cend(), [](const QChar c) { return c >= '0' && c <= '9'; });
            if (!digitsOnly) return text;
            bool ok = false;
            const auto value = text.toLongLong(&ok);
            return ok ? QJsonValue(value) : QJsonValue(text);
        }

        QString memoryBytesText(const QJsonObject& object, const QString& key)
        {
            const auto value = object[key];
            if (!value.isDouble()) return value.toString();
            if (const auto bytes = value.toInteger(-1); bytes >= 0) return QString::number(bytes);
            ParseDiag::Warn(key, QCoreApplication::translate("Validation", "The receive window must be a whole number of bytes or a size such as 8m, so it was ignored."));
            return {};
        }
    }

    bool QUICFields::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        // Hysteria port-hopping links carry a port range, which QUrl reports as invalid.
        if (!url.isValid() && !url.errorString().startsWith("Invalid port")) return false;
        auto query = QUrlQuery(url.query());

        for (const auto& [key, target] : {std::pair{QStringLiteral("quic_idle_timeout"), &idle_timeout},
                                          std::pair{QStringLiteral("quic_keep_alive_period"), &keep_alive_period}}) {
            if (!query.hasQueryItem(key)) continue;
            if (const auto value = query.queryItemValue(key).trimmed(); AcceptOptional(key, Validation::Duration(value), value)) *target = value;
        }
        for (const auto& [key, target] : {std::pair{QStringLiteral("quic_stream_receive_window"), &stream_receive_window},
                                          std::pair{QStringLiteral("quic_connection_receive_window"), &connection_receive_window}}) {
            if (!query.hasQueryItem(key)) continue;
            if (const auto value = query.queryItemValue(key).trimmed(); AcceptOptional(key, Validation::MemoryBytes(value, true), value)) *target = value;
        }
        ReadQueryInt(query, "quic_max_concurrent_streams", max_concurrent_streams);
        if (query.hasQueryItem("quic_initial_packet_size")) {
            const auto value = query.queryItemValue("quic_initial_packet_size").trimmed();
            if (AcceptOptional("quic_initial_packet_size", Checks::QuicInitialPacketSize(value), value)) initial_packet_size = value.toInt();
        }
        if (query.hasQueryItem("quic_disable_path_mtu_discovery"))
        {
            disable_path_mtu_discovery = query.queryItemValue("quic_disable_path_mtu_discovery") == "true";
            disable_path_mtu_discovery_unspecified = false;
        }
        else disable_path_mtu_discovery_unspecified = true;
        return true;
    }
    bool QUICFields::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;

        // ParseFromJson is also the DB loader: only an import drops values.
        const bool importing = ParseDiag::Active();
        for (const auto& [key, target] : {std::pair{QStringLiteral("idle_timeout"), &idle_timeout},
                                          std::pair{QStringLiteral("keep_alive_period"), &keep_alive_period}}) {
            if (!object.contains(key)) continue;
            const auto value = object[key].toString().trimmed();
            if (!importing || AcceptOptional(key, Validation::Duration(value), value)) *target = value;
        }
        for (const auto& [key, target] : {std::pair{QStringLiteral("stream_receive_window"), &stream_receive_window},
                                          std::pair{QStringLiteral("connection_receive_window"), &connection_receive_window}}) {
            if (!object.contains(key)) continue;
            const auto value = memoryBytesText(object, key);
            if (!importing || AcceptOptional(key, Validation::MemoryBytes(value, true), value)) *target = value;
        }
        if (object.contains("max_concurrent_streams")) max_concurrent_streams = object["max_concurrent_streams"].toInt();
        if (object.contains("initial_packet_size")) {
            const auto value = object["initial_packet_size"].toInt();
            if (!importing || AcceptOptional("initial_packet_size", Checks::QuicInitialPacketSize(QString::number(value)), QString::number(value))) {
                initial_packet_size = value;
            }
        }
        if (object.contains("disable_path_mtu_discovery"))
        {
            disable_path_mtu_discovery = object["disable_path_mtu_discovery"].toBool();
            disable_path_mtu_discovery_unspecified = false;
        }
        else disable_path_mtu_discovery_unspecified = true;
        return true;
    }
    QString QUICFields::ExportToLink()
    {
        QUrlQuery query;
        if (!idle_timeout.isEmpty()) query.addQueryItem("quic_idle_timeout", idle_timeout);
        if (!keep_alive_period.isEmpty()) query.addQueryItem("quic_keep_alive_period", keep_alive_period);
        if (!stream_receive_window.isEmpty()) query.addQueryItem("quic_stream_receive_window", stream_receive_window);
        if (!connection_receive_window.isEmpty()) query.addQueryItem("quic_connection_receive_window", connection_receive_window);
        if (max_concurrent_streams > 0) query.addQueryItem("quic_max_concurrent_streams", QString::number(max_concurrent_streams));
        if (initial_packet_size > 0) query.addQueryItem("quic_initial_packet_size", QString::number(initial_packet_size));
        if (!disable_path_mtu_discovery_unspecified) query.addQueryItem("quic_disable_path_mtu_discovery", disable_path_mtu_discovery ? "true" : "false");
        return query.toString();
    }
    QJsonObject QUICFields::ExportToJson()
    {
        QJsonObject object;
        if (!idle_timeout.isEmpty()) object["idle_timeout"] = idle_timeout;
        if (!keep_alive_period.isEmpty()) object["keep_alive_period"] = keep_alive_period;
        if (!stream_receive_window.isEmpty()) object["stream_receive_window"] = stream_receive_window;
        if (!connection_receive_window.isEmpty()) object["connection_receive_window"] = connection_receive_window;
        if (max_concurrent_streams > 0) object["max_concurrent_streams"] = max_concurrent_streams;
        if (initial_packet_size > 0) object["initial_packet_size"] = initial_packet_size;
        // persist On (true) / Off (false) explicitly so an Off override survives a round-trip
        if (!disable_path_mtu_discovery_unspecified) object["disable_path_mtu_discovery"] = disable_path_mtu_discovery;
        return object;
    }
    BuildResult QUICFields::Build()
    {
        auto object = ExportToJson();
        const auto &settings = *dataManager->settingsRepo;
        const auto idleTimeout = settings.h2_idle_timeout.trimmed();
        const auto keepAlivePeriod = settings.h2_keep_alive_period.trimmed();
        const auto streamReceiveWindow = settings.h2_stream_receive_window.trimmed();
        const auto connectionReceiveWindow = settings.h2_connection_receive_window.trimmed();
        if (idle_timeout.isEmpty() && !idleTimeout.isEmpty()) object["idle_timeout"] = idleTimeout;
        if (keep_alive_period.isEmpty() && !keepAlivePeriod.isEmpty()) object["keep_alive_period"] = keepAlivePeriod;
        if (stream_receive_window.isEmpty() && !streamReceiveWindow.isEmpty()) object["stream_receive_window"] = streamReceiveWindow;
        if (connection_receive_window.isEmpty() && !connectionReceiveWindow.isEmpty()) object["connection_receive_window"] = connectionReceiveWindow;
        if (max_concurrent_streams <= 0 && settings.h2_max_concurrent_streams > 0) object["max_concurrent_streams"] = settings.h2_max_concurrent_streams;
        if (initial_packet_size <= 0 && settings.quic_initial_packet_size > 0) object["initial_packet_size"] = settings.quic_initial_packet_size;
        if (disable_path_mtu_discovery_unspecified && settings.quic_disable_path_mtu_discovery) object["disable_path_mtu_discovery"] = true;
        for (const QString key : {QStringLiteral("stream_receive_window"), QStringLiteral("connection_receive_window")}) {
            if (object.contains(key)) object[key] = memoryBytesJson(object[key].toString().trimmed());
        }
        return {object, ""};
    }

    namespace Checks {
        QString QuicInitialPacketSize(const QString& text)
        {
            const auto value = Validation::ToInt(text);
            if (text.trimmed().isEmpty() || value == 0 || (value && *value >= 1200 && *value <= 1452)) return {};
            return QCoreApplication::translate("Validation", "Must be 0 (the default) or from 1200 to 1452.");
        }
    }

    void QUICFields::Validate(Validation::Issues& issues)
    {
        using Validation::Severity;
        const auto add = [&issues](Severity severity, const QString& field, const QString& message, const QString& value) {
            if (!message.isEmpty()) issues.append({severity, field, message, value});
        };
        add(Severity::Error, "idle_timeout", Validation::Duration(idle_timeout), idle_timeout);
        add(Severity::Error, "keep_alive_period", Validation::Duration(keep_alive_period), keep_alive_period);
        add(Severity::Error, "stream_receive_window", Validation::MemoryBytes(stream_receive_window, true), stream_receive_window);
        add(Severity::Error, "connection_receive_window", Validation::MemoryBytes(connection_receive_window, true), connection_receive_window);
        const auto packetSize = QString::number(initial_packet_size);
        add(Severity::Error, "initial_packet_size", Checks::QuicInitialPacketSize(packetSize), packetSize);
    }
}
