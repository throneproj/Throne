#include "include/configs/common/multiplex.h"
#include "include/configs/common/utils.h"

#include <QCoreApplication>

#include "include/configs/ParseDiagnostics.h"

namespace Configs {
    bool TcpBrutal::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("brutal_enabled")) enabled = query.queryItemValue("brutal_enabled") == "true";
        ReadQueryInt(query, "brutal_up_mbps", up_mbps);
        ReadQueryInt(query, "brutal_down_mbps", down_mbps);
        return true;
    }
    bool TcpBrutal::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool();
        if (object.contains("up_mbps")) up_mbps = object["up_mbps"].toInt();
        if (object.contains("down_mbps")) down_mbps = object["down_mbps"].toInt();
        return true;
    }
    QString TcpBrutal::ExportToLink()
    {
        QUrlQuery query;
        if (!enabled) return "";
        query.addQueryItem("brutal_enabled", "true");
        if (up_mbps > 0) query.addQueryItem("brutal_up_mbps", QString::number(up_mbps));
        if (down_mbps > 0) query.addQueryItem("brutal_down_mbps", QString::number(down_mbps));
        return QUrlQuery(query).toString();
    }
    QJsonObject TcpBrutal::ExportToJson()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        // sing-box expects both values it seems
        object["up_mbps"] = up_mbps <= 0 ? 1 : up_mbps;
        object["down_mbps"] = down_mbps <= 0 ? 1 : down_mbps;
        return object;
    }
    BuildResult TcpBrutal::Build()
    {
        return {ExportToJson(), ""};
    }

    bool Multiplex::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("mux")) { enabled = query.queryItemValue("mux") == "true"; unspecified = false; }
        else unspecified = true;
        if (query.hasQueryItem("mux_protocol")) {
            const auto value = query.queryItemValue("mux_protocol").trimmed();
            if (AcceptOptional("mux_protocol", Checks::MuxProtocol(value), value)) protocol = value;
        }
        ReadQueryInt(query, "mux_max_connections", max_connections);
        ReadQueryInt(query, "mux_min_streams", min_streams);
        ReadQueryInt(query, "mux_max_streams", max_streams);
        if (query.hasQueryItem("mux_padding")) padding = query.queryItemValue("mux_padding") == "true";
        brutal->ParseFromLink(link);
        return true;
    }
    bool Multiplex::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty())
        {
            unspecified = true;
            return false;
        }
        if (object.contains("enabled")) { enabled = object["enabled"].toBool(); unspecified = false; }
        else unspecified = true;
        if (object.contains("protocol")) {
            const auto value = object["protocol"].toString();
            // ParseFromJson is also the DB loader: only an import drops values.
            if (!ParseDiag::Active() || AcceptOptional("multiplex.protocol", Checks::MuxProtocol(value), value)) protocol = value;
        }
        if (object.contains("max_connections")) max_connections = object["max_connections"].toInt();
        if (object.contains("min_streams")) min_streams = object["min_streams"].toInt();
        if (object.contains("max_streams")) max_streams = object["max_streams"].toInt();
        if (object.contains("padding")) padding = object["padding"].toBool();
        if (object.contains("brutal")) brutal->ParseFromJson(object["brutal"].toObject());
        return true;
    }
    bool Multiplex::ParseFromClash(const clash::Proxies& object)
    {
        enabled = object.smux.enabled;
        unspecified = false;
        if (const auto value = QString::fromStdString(object.smux.protocol).trimmed();
            !value.isEmpty() && AcceptOptional("smux.protocol", Checks::MuxProtocol(value), value)) {
            protocol = value;
        }
        if (object.smux.max_streams > 0) {
            max_streams = object.smux.max_streams;
        } else {
            max_connections = object.smux.max_connections;
            min_streams = object.smux.min_streams;
        }
        padding = object.smux.padding;
        return true;
    }
    QString Multiplex::ExportToLink()
    {
        QUrlQuery query;
        if (unspecified) return "";
        query.addQueryItem("mux", enabled ? "true" : "false");
        if (enabled)
        {
            if (!protocol.isEmpty()) query.addQueryItem("mux_protocol", protocol);
            if (max_connections > 0) query.addQueryItem("mux_max_connections", QString::number(max_connections));
            if (min_streams > 0) query.addQueryItem("mux_min_streams", QString::number(min_streams));
            if (max_streams > 0) query.addQueryItem("mux_max_streams", QString::number(max_streams));
            if (padding) query.addQueryItem("mux_padding", "true");
            mergeUrlQuery(query, brutal->ExportToLink());
        }
        return query.toString();
    }
    QJsonObject Multiplex::ExportToJson()
    {
        QJsonObject object;
        // Tri-state: omit only for "Keep Default"; On/Off are persisted explicitly.
        if (unspecified) return object;
        object["enabled"] = enabled;
        if (!enabled) return object;
        if (!protocol.isEmpty()) object["protocol"] = protocol;
        if (max_connections > 0) object["max_connections"] = max_connections;
        if (min_streams > 0) object["min_streams"] = min_streams;
        if (max_streams > 0) object["max_streams"] = max_streams;
        if (padding) object["padding"] = padding;
        if (brutal->enabled) object["brutal"] = brutal->ExportToJson();
        return object;
    }
    BuildResult Multiplex::Build()
    {
        if (!EffectivelyOn()) return {{}, ""};
        return {(IsCustom() ? CustomSettings() : MuxSettings::Preset()).Build(), ""};
    }

    bool Multiplex::EffectivelyOn() const
    {
        return enabled || (unspecified && Configs::dataManager->settingsRepo->mux_default_on);
    }

    bool Multiplex::HasOwnSettings() const
    {
        return !protocol.isEmpty() || max_connections > 0 || min_streams > 0 || max_streams > 0 || padding || brutal->enabled;
    }

    MuxSettings Multiplex::CustomSettings() const
    {
        auto settings = MuxSettings::Preset();
        if (!HasOwnSettings()) return settings;
        if (!protocol.isEmpty()) settings.protocol = protocol;
        // The core lets max_connections win over max_streams; with neither, the preset's pool applies.
        if (max_connections > 0) {
            settings.by_connections = true;
            settings.max_connections = max_connections;
            settings.min_streams = min_streams;
        } else if (max_streams > 0) {
            settings.by_connections = false;
            settings.max_streams = max_streams;
        }
        settings.padding = padding;
        settings.brutal = brutal->enabled;
        if (brutal->enabled) {
            settings.brutal_up_mbps = brutal->up_mbps;
            settings.brutal_down_mbps = brutal->down_mbps;
        }
        return settings;
    }

    void Multiplex::SetCustomSettings(const MuxSettings& settings)
    {
        protocol = settings.protocol;
        max_connections = settings.by_connections ? settings.max_connections : 0;
        min_streams = settings.by_connections ? settings.min_streams : 0;
        max_streams = settings.by_connections ? 0 : settings.max_streams;
        padding = settings.padding;
        brutal->enabled = settings.brutal;
        brutal->up_mbps = settings.brutal_up_mbps;
        brutal->down_mbps = settings.brutal_down_mbps;
    }

    MuxSettings MuxSettings::Preset()
    {
        const auto& repo = *Configs::dataManager->settingsRepo;
        MuxSettings settings;
        settings.protocol = repo.mux_protocol;
        settings.by_connections = repo.mux_mode == "max_connections";
        settings.max_streams = repo.mux_concurrency;
        settings.max_connections = repo.mux_max_connections;
        settings.min_streams = repo.mux_min_streams;
        settings.padding = repo.mux_padding;
        settings.brutal = repo.mux_brutal;
        settings.brutal_up_mbps = repo.mux_brutal_up_mbps;
        settings.brutal_down_mbps = repo.mux_brutal_down_mbps;
        return settings;
    }

    void MuxSettings::SaveAsPreset() const
    {
        auto& repo = *Configs::dataManager->settingsRepo;
        repo.mux_protocol = protocol;
        repo.mux_mode = by_connections ? "max_connections" : "max_streams";
        repo.mux_concurrency = max_streams;
        repo.mux_max_connections = max_connections;
        repo.mux_min_streams = min_streams;
        repo.mux_padding = padding;
        repo.mux_brutal = brutal;
        repo.mux_brutal_up_mbps = brutal_up_mbps;
        repo.mux_brutal_down_mbps = brutal_down_mbps;
    }

    QJsonObject MuxSettings::Build() const
    {
        QJsonObject object;
        object["enabled"] = true;
        if (!protocol.isEmpty()) object["protocol"] = protocol;
        if (by_connections) {
            if (max_connections > 0) object["max_connections"] = max_connections;
            if (min_streams > 0) object["min_streams"] = min_streams;
        } else if (max_streams > 0) {
            object["max_streams"] = max_streams;
        }
        if (padding) object["padding"] = true;
        if (brutal) {
            TcpBrutal options;
            options.enabled = true;
            options.up_mbps = brutal_up_mbps;
            options.down_mbps = brutal_down_mbps;
            object["brutal"] = options.ExportToJson();
        }
        return object;
    }

    namespace Checks {
        QString MuxProtocol(const QString& protocol)
        {
            return Validation::OneOf(protocol, muxProtocols);
        }

        QString BrutalNeedsMux(bool brutal, bool muxOn)
        {
            if (!brutal || muxOn) return {};
            return QCoreApplication::translate("Validation", "TCP Brutal only works with a custom multiplex. Set Multiplex to Custom.");
        }
    }

    void Multiplex::Validate(Validation::Issues& issues)
    {
        using Validation::Severity;
        // Only a Custom multiplex builds from the profile's own settings.
        const bool custom = IsCustom();
        if (const auto message = Checks::BrutalNeedsMux(brutal->enabled, custom); !message.isEmpty()) {
            issues.append({Severity::Warning, "multiplex.brutal", message});
        }
        if (!custom) return;
        if (const auto message = Checks::MuxProtocol(protocol); !message.isEmpty()) issues.append({Severity::Error, "multiplex.protocol", message, protocol});
        if (brutal->enabled && (brutal->up_mbps <= 0 || brutal->down_mbps <= 0)) {
            issues.append({Severity::Warning, "multiplex.brutal",
                           QCoreApplication::translate("Validation", "TCP Brutal needs both speeds; a missing one is sent as 1 Mbps.")});
        }
    }
}


