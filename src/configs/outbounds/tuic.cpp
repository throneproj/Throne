#include "include/configs/outbounds/tuic.h"

#include <QCoreApplication>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // Older builds saved translated combo text ("BBR", "New Reno", "нативный"); the core matches exact identifiers.
        QString canonicalTuicCongestion(const QString& value)
        {
            return value.trimmed().toLower().replace(' ', '_');
        }

        QString canonicalTuicRelayMode(const QString& value)
        {
            const auto mode = value.trimmed().toLower();
            return mode == "native" || mode == "quic" ? mode : QString();
        }

        // Without a diagnostics scope (DB loads, clones) a parse keeps every value as it always did.
        bool tuicKeep(const QString& field, const QString& value, const Validation::Rule& rule)
        {
            return !ParseDiag::Active() || AcceptOptional(field, rule(value), value);
        }

        QString tuicImportCongestion(const QString& raw)
        {
            const auto value = canonicalTuicCongestion(raw);
            return value.isEmpty() || tuicKeep("congestion_control", value, Validation::Rules::OneOf(ccAlgorithms)) ? value : QString();
        }

        // The core treats an unknown mode as native, so it is dropped with a note.
        QString tuicImportRelayMode(const QString& raw)
        {
            const auto mode = canonicalTuicRelayMode(raw);
            if (mode.isEmpty() && !raw.trimmed().isEmpty() && ParseDiag::Active()) {
                AcceptOptional("udp_relay_mode", Validation::OneOf(raw.trimmed(), {"native", "quic"}), raw);
            }
            return mode;
        }

        QString tuicImportHeartbeat(const QString& raw)
        {
            const auto value = raw.trimmed();
            return tuicKeep("heartbeat", value, Validation::Rules::Duration()) ? value : QString();
        }
    }

    bool tuic::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        uuid = url.userName();
        password = url.password();

        if (query.hasQueryItem("congestion_control")) congestion_control = tuicImportCongestion(query.queryItemValue("congestion_control"));
        if (query.hasQueryItem("udp_relay_mode")) udp_relay_mode = tuicImportRelayMode(query.queryItemValue("udp_relay_mode"));
        if (query.hasQueryItem("udp_over_stream")) udp_over_stream = query.queryItemValue("udp_over_stream") == "true";
        if (query.hasQueryItem("zero_rtt_handshake")) zero_rtt_handshake = query.queryItemValue("zero_rtt_handshake") == "true";
        if (query.hasQueryItem("heartbeat")) heartbeat = tuicImportHeartbeat(query.queryItemValue("heartbeat"));
        
        tls->ParseFromLink(link);
        tls->enabled = true; // TUIC always uses tls
        quic->ParseFromLink(link);

        if (server_port == 0) server_port = 443;

        return !(uuid.isEmpty() || server.isEmpty());
    }

    bool tuic::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "tuic") return false;
        outbound::ParseFromJson(object);
        if (object.contains("uuid")) uuid = object["uuid"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("congestion_control")) congestion_control = tuicImportCongestion(object["congestion_control"].toString());
        if (object.contains("udp_relay_mode")) udp_relay_mode = tuicImportRelayMode(object["udp_relay_mode"].toString());
        if (object.contains("udp_over_stream")) udp_over_stream = object["udp_over_stream"].toBool();
        if (object.contains("zero_rtt_handshake")) zero_rtt_handshake = object["zero_rtt_handshake"].toBool();
        if (object.contains("heartbeat")) heartbeat = tuicImportHeartbeat(object["heartbeat"].toString());
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        quic->ParseFromJson(object);
        return true;
    }

    bool tuic::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "tuic") return false;
        outbound::ParseFromClash(object);
        uuid = QString::fromStdString(object.uuid);
        if (uuid.trimmed().isEmpty()) {
            ParseDiag::Error("uuid", QCoreApplication::translate("Validation", "Missing UUID. TUIC v4 (token authentication) is not supported; only TUIC v5 servers work."));
            return false;
        }
        password = QString::fromStdString(object.password);
        if (!object.congestion_controller.empty()) congestion_control = tuicImportCongestion(QString::fromStdString(object.congestion_controller));
        if (!object.udp_relay_mode.empty()) udp_relay_mode = tuicImportRelayMode(QString::fromStdString(object.udp_relay_mode));
        zero_rtt_handshake = object.reduce_rtt;
        if (object.heartbeat_interval > 0) heartbeat = QString::number(object.heartbeat_interval) + "ms";
        if (!object.ip.empty()) server = QString::fromStdString(object.ip);

        tls->ParseFromClash(object);
        tls->enabled = true;
        return true;
    }

    void tuic::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        const auto report = [&issues](const QString& field, const QString& message, const QString& value = {}) {
            if (!message.isEmpty()) issues << Validation::Issue{Validation::Severity::Error, field, message, value};
        };
        report("uuid", Validation::Uuid(uuid));
        report("heartbeat", Validation::Duration(heartbeat));
        report("congestion_control", Validation::OneOf(canonicalTuicCongestion(congestion_control), ccAlgorithms), congestion_control);
    }

    QString tuic::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("tuic");
        url.setUserName(uuid);
        if (!password.isEmpty()) url.setPassword(password);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        if (!congestion_control.isEmpty()) query.addQueryItem("congestion_control", congestion_control);
        if (!udp_relay_mode.isEmpty() && !udp_over_stream) query.addQueryItem("udp_relay_mode", udp_relay_mode);
        if (udp_over_stream) query.addQueryItem("udp_over_stream", "true");
        if (zero_rtt_handshake) query.addQueryItem("zero_rtt_handshake", "true");
        if (!heartbeat.isEmpty()) query.addQueryItem("heartbeat", heartbeat);
        
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, quic->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject tuic::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "tuic";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!password.isEmpty()) object["password"] = password;
        if (!congestion_control.isEmpty()) object["congestion_control"] = congestion_control;
        if (!udp_relay_mode.isEmpty() && !udp_over_stream) object["udp_relay_mode"] = udp_relay_mode;
        if (udp_over_stream) object["udp_over_stream"] = udp_over_stream;
        if (zero_rtt_handshake) object["zero_rtt_handshake"] = zero_rtt_handshake;
        if (!heartbeat.isEmpty()) object["heartbeat"] = heartbeat;
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        mergeJsonObjects(object, quic->ExportToJson());
        return object;
    }

    BuildResult tuic::Build()
    {
        QJsonObject object;
        object["type"] = "tuic";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!password.isEmpty()) object["password"] = password;
        if (const auto congestion = canonicalTuicCongestion(congestion_control); !congestion.isEmpty()) object["congestion_control"] = congestion;
        if (const auto relayMode = canonicalTuicRelayMode(udp_relay_mode); !udp_over_stream && !relayMode.isEmpty()) object["udp_relay_mode"] = relayMode;
        if (udp_over_stream) object["udp_over_stream"] = udp_over_stream;
        if (zero_rtt_handshake) object["zero_rtt_handshake"] = zero_rtt_handshake;
        if (!heartbeat.isEmpty()) object["heartbeat"] = heartbeat;
        if (tls->enabled) {
            const auto tlsResult = tls->Build();
            if (!tlsResult.error.isEmpty()) return {{}, tlsResult.error};
            object["tls"] = tlsResult.object;
        }
        const auto quicResult = quic->Build();
        if (!quicResult.error.isEmpty()) return {{}, quicResult.error};
        mergeJsonObjects(object, quicResult.object);
        return {object, ""};
    }

    QString tuic::DisplayType()
    {
        return "TUIC";
    }

    SecurityInfo tuic::GetSecurity()
    {
        return SecurityFromTLS("QUIC");
    }
}
