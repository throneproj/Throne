#include "include/configs/common/xrayMultiplex.h"

#include <QCoreApplication>
#include <QUrlQuery>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        void readQueryCount(const QUrlQuery& query, const QString& key, std::optional<int>& target) {
            if (!query.hasQueryItem(key)) return;
            const auto text = query.queryItemValue(key).trimmed();
            if (!AcceptOptional(key, Validation::Int(text, -1, kXrayMuxMaxCount), text)) return;
            if (const auto value = Validation::ToInt(text)) target = static_cast<int>(*value);
        }

        // ParseFromJson is also the DB loader: only an import drops values.
        void readJsonCount(const QJsonObject& object, const QString& key, std::optional<int>& target) {
            if (!object.contains(key)) return;
            const auto value = object[key].toInt();
            const auto text = QString::number(value);
            if (ParseDiag::Active() && !AcceptOptional("mux." + key, Validation::Int(text, -1, kXrayMuxMaxCount), text)) return;
            target = value;
        }
    }

    // Every On profile used to store the then built-in xudpConcurrency 16; alone it is no setting of its own.
    void xrayMultiplex::dropLegacyXudpDefault() {
        if (!concurrency && !maxConnections && xudpProxyUDP443.isEmpty() && xudpConcurrency == 16) xudpConcurrency.reset();
    }

    bool xrayMultiplex::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("mux")) enabled = query.queryItemValue("mux").replace("1", "true") == "true", useDefault = false;
        readQueryCount(query, "mux_concurrency", concurrency);
        readQueryCount(query, "mux_max_connections", maxConnections);
        readQueryCount(query, "mux_xudp_concurrency", xudpConcurrency);
        if (query.hasQueryItem("mux_xudp_proxy_udp443")) {
            const auto value = query.queryItemValue("mux_xudp_proxy_udp443").trimmed();
            if (AcceptOptional("mux_xudp_proxy_udp443", Checks::XrayXudpProxyUDP443(value), value)) xudpProxyUDP443 = value;
        }
        dropLegacyXudpDefault();
        return true;
    }

    bool xrayMultiplex::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool(), useDefault = false;
        readJsonCount(object, "concurrency", concurrency);
        readJsonCount(object, "maxConnections", maxConnections);
        readJsonCount(object, "xudpConcurrency", xudpConcurrency);
        if (object.contains("xudpProxyUDP443")) {
            const auto value = object["xudpProxyUDP443"].toString();
            if (!ParseDiag::Active() || AcceptOptional("mux.xudpProxyUDP443", Checks::XrayXudpProxyUDP443(value), value)) xudpProxyUDP443 = value;
        }
        dropLegacyXudpDefault();
        return true;
    }

    bool xrayMultiplex::ParseFromClash(const clash::Proxies& object) {
        enabled = object.smux.enabled;
        useDefault = false;
        if (object.smux.max_streams > 0) {
            concurrency = object.smux.max_streams.value;
        } else if (object.smux.max_connections > 0) {
            // Under a cap, concurrency is when another connection opens: sing-mux's min_streams.
            maxConnections = object.smux.max_connections.value;
            if (object.smux.min_streams > 0) concurrency = object.smux.min_streams.value;
        }
        return true;
    }

    QString xrayMultiplex::ExportToLink() {
        QUrlQuery query;
        if (useDefault) return "";
        query.addQueryItem("mux", enabled ? "true" : "false");
        if (enabled) {
            if (concurrency) query.addQueryItem("mux_concurrency", QString::number(*concurrency));
            if (maxConnections) query.addQueryItem("mux_max_connections", QString::number(*maxConnections));
            if (xudpConcurrency) query.addQueryItem("mux_xudp_concurrency", QString::number(*xudpConcurrency));
            if (!xudpProxyUDP443.isEmpty()) query.addQueryItem("mux_xudp_proxy_udp443", xudpProxyUDP443);
        }
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayMultiplex::ExportToJson() {
        QJsonObject object;
        // Tri-state: omit only for "Keep Default" (useDefault); On/Off are persisted explicitly.
        if (useDefault) return object;
        object["enabled"] = enabled;
        if (!enabled) return object;
        if (concurrency) object["concurrency"] = *concurrency;
        if (maxConnections) object["maxConnections"] = *maxConnections;
        if (xudpConcurrency) object["xudpConcurrency"] = *xudpConcurrency;
        if (!xudpProxyUDP443.isEmpty()) object["xudpProxyUDP443"] = xudpProxyUDP443;
        return object;
    }

    BuildResult xrayMultiplex::Build() {
        return {BuildMux(Configs::dataManager->settingsRepo->xray_mux_default_on), ""};
    }

    QJsonObject xrayMultiplex::BuildMux(bool defaultOn) {
        if (useDefault ? !defaultOn : !enabled) return {};
        return (IsCustom() ? CustomSettings() : XrayMuxSettings::Preset()).Build();
    }

    bool xrayMultiplex::HasOwnSettings() const {
        return concurrency || maxConnections || xudpConcurrency || !xudpProxyUDP443.isEmpty();
    }

    XrayMuxSettings xrayMultiplex::CustomSettings() const {
        auto settings = XrayMuxSettings::Preset();
        if (concurrency) settings.concurrency = *concurrency;
        if (maxConnections) settings.max_connections = *maxConnections;
        if (xudpConcurrency) settings.xudp_concurrency = *xudpConcurrency;
        if (!xudpProxyUDP443.isEmpty()) settings.xudp_proxy_udp443 = xudpProxyUDP443;
        return settings;
    }

    void xrayMultiplex::SetCustomSettings(const XrayMuxSettings& settings) {
        concurrency = settings.concurrency;
        maxConnections = settings.max_connections;
        xudpConcurrency = settings.xudp_concurrency;
        xudpProxyUDP443 = settings.xudp_proxy_udp443;
    }

    void xrayMultiplex::ClearOwnSettings() {
        concurrency.reset();
        maxConnections.reset();
        xudpConcurrency.reset();
        xudpProxyUDP443.clear();
    }

    void xrayMultiplex::Validate(Validation::Issues& issues) {
        using Validation::Severity;
        if (!IsCustom()) return;
        for (const auto& [key, value]: {std::pair{QStringLiteral("mux.concurrency"), concurrency},
                                        std::pair{QStringLiteral("mux.maxConnections"), maxConnections},
                                        std::pair{QStringLiteral("mux.xudpConcurrency"), xudpConcurrency}}) {
            if (!value) continue;
            const auto text = QString::number(*value);
            if (const auto message = Validation::Int(text, -1, kXrayMuxMaxCount); !message.isEmpty()) issues.append({Severity::Error, key, message, text});
        }
        if (const auto message = Checks::XrayXudpProxyUDP443(xudpProxyUDP443); !message.isEmpty()) {
            issues.append({Severity::Error, "mux.xudpProxyUDP443", message, xudpProxyUDP443});
        }
    }

    XrayMuxSettings XrayMuxSettings::Preset() {
        const auto& repo = *Configs::dataManager->settingsRepo;
        XrayMuxSettings settings;
        settings.concurrency = repo.xray_mux_concurrency;
        settings.max_connections = repo.xray_mux_max_connections;
        settings.xudp_concurrency = repo.xray_mux_xudp_concurrency;
        settings.xudp_proxy_udp443 = repo.xray_mux_xudp_proxy_udp443;
        return settings;
    }

    void XrayMuxSettings::SaveAsPreset() const {
        auto& repo = *Configs::dataManager->settingsRepo;
        repo.xray_mux_concurrency = concurrency;
        repo.xray_mux_max_connections = max_connections;
        repo.xray_mux_xudp_concurrency = xudp_concurrency;
        repo.xray_mux_xudp_proxy_udp443 = xudp_proxy_udp443;
    }

    QJsonObject XrayMuxSettings::Build() const {
        QJsonObject object;
        object["enabled"] = true;
        if (concurrency != 0) object["concurrency"] = concurrency;
        if (max_connections > 0) object["maxConnections"] = max_connections;
        if (xudp_concurrency != 0) object["xudpConcurrency"] = xudp_concurrency;
        if (!xudp_proxy_udp443.isEmpty()) object["xudpProxyUDP443"] = xudp_proxy_udp443;
        return object;
    }

    namespace Checks {
        QString XrayXudpProxyUDP443(const QString& mode) {
            return Validation::OneOf(mode, xrayXudpProxyUDP443Modes);
        }
    }
}
