#include "include/configs/outbounds/xrayVless.h"

#include <QCoreApplication>
#include <QUrlQuery>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"
#include "include/configs/outbounds/custom.h"
#include "include/global/Utils.hpp"

namespace Configs {
    namespace {
        // Xray has no dial fields: bind_interface travels as streamSettings.sockopt.interface.
        QJsonObject withBindInterface(QJsonObject stream, const QString &iface) {
            if (iface.isEmpty()) return stream;
            auto sockopt = stream.value("sockopt").toObject();
            sockopt["interface"] = iface;
            stream["sockopt"] = sockopt;
            return stream;
        }

        // Xray compares flow names and "none" exactly.
        QString xrayVlessImportFlow(const QString &raw) {
            return raw.trimmed().toLower();
        }

        QString xrayVlessImportEncryption(const QString &raw) {
            const QString value = raw.trimmed();
            return value.compare("none", Qt::CaseInsensitive) == 0 ? QStringLiteral("none") : value;
        }
    }

    namespace Checks {
        QString XrayVlessEncryption(const QString &encryption) {
            if (encryption.isEmpty()) return QCoreApplication::translate("Validation", "Write none when the server does not use VLESS encryption.");
            if (encryption == "none") return {};
            const QStringList parts = encryption.split(u'.');
            if (parts.size() < 4 || parts[0] != "mlkem768x25519plus")
                return QCoreApplication::translate("Validation", "Must be none, or the mlkem768x25519plus string the server gives.");
            if (parts[1] != "native" && parts[1] != "xorpub" && parts[1] != "random")
                return QCoreApplication::translate("Validation", "The part after mlkem768x25519plus must be native, xorpub or random.");
            if (parts[2] != "1rtt" && parts[2] != "0rtt")
                return QCoreApplication::translate("Validation", "The part after the mode must be 1rtt or 0rtt.");
            // Shorter parts are padding specs.
            for (qsizetype i = 3; i < parts.size(); ++i) {
                if (parts[i].size() < 20) continue;
                if (Validation::Base64Key(parts[i], 32, true, false).isEmpty() || Validation::Base64Key(parts[i], 1184, true, false).isEmpty()) continue;
                return QCoreApplication::translate("Validation", "Each key in the string must be URL-safe base64 of 32 or 1184 bytes, without padding.");
            }
            return {};
        }

        QString XrayVlessVision(const QString &flow, const QString &encryption, const QString &network, const QString &security) {
            if (!flow.startsWith("xtls-rprx-vision") || IsVlessEncrypted(encryption)) return {};
            const QString normalized = NormalizeXrayNetwork(network);
            if ((normalized.isEmpty() || normalized == "raw") && (security == "tls" || security == "reality")) return {};
            return QCoreApplication::translate("Validation", "XTLS Vision needs the raw network with TLS or REALITY, or VLESS encryption: change the network or security, or clear the flow.");
        }
    }

    bool xrayVless::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        uuid = url.userName();
        encryption = xrayVlessImportEncryption(GetQueryValue(query, "encryption", "none"));
        flow = xrayVlessImportFlow(query.queryItemValue("flow"));
        if (!streamSetting->ParseFromLink(link)) return false;
        multiplex->ParseFromLink(link);
        return !(uuid.isEmpty() || server.isEmpty());
    }

    bool xrayVless::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty() || object["protocol"].toString() != "vless") return false;
        if (object.contains("tag")) name = object["tag"].toString();
        if (auto settingsObj = object["settings"].toObject(); !settingsObj.isEmpty()) {
            if (settingsObj.contains("address")) server = settingsObj["address"].toString();
            if (settingsObj.contains("port")) server_port = settingsObj["port"].toInt();
            if (settingsObj.contains("flow")) flow = settingsObj["flow"].toString();
            if (settingsObj.contains("id")) uuid = settingsObj["id"].toString();
            if (settingsObj.contains("encryption")) encryption = settingsObj["encryption"].toString();
        }
        bool streamParsed = true;
        if (auto streamSettings = object["streamSettings"].toObject(); !streamSettings.isEmpty()) {
            streamParsed = streamSetting->ParseFromJson(streamSettings);
        }
        // Moved, not copied: an interface cleared in the Advanced dialog must not come back from sockopt.
        if (auto iface = streamSetting->sockopt.take("interface").toString(); !iface.isEmpty()) {
            dialFields->bind_interface = iface;
        }
        if (auto muxObj = object["mux"].toObject(); !muxObj.isEmpty()) {
            multiplex->ParseFromJson(muxObj);
        }
        if (ParseDiag::Active()) {
            flow = xrayVlessImportFlow(flow);
            encryption = xrayVlessImportEncryption(encryption);
        }
        // False lets the importer fall back to a lossless custom Xray outbound; the DB loader ignores the result.
        return streamParsed;
    }

    bool xrayVless::ParseFromClash(const clash::Proxies& object) {
        if (object.type != "vless") return false;
        outbound::ParseFromClash(object);
        uuid = QString::fromStdString(object.uuid);
        if (!object.flow.empty()) flow = xrayVlessImportFlow(QString::fromStdString(object.flow));
        if (!object.encryption.empty()) encryption = xrayVlessImportEncryption(QString::fromStdString(object.encryption));
        streamSetting->ParseFromClash(object);
        multiplex->ParseFromClash(object);
        return true;
    }

    QString xrayVless::ExportToLink() {
        QUrl url;
        QUrlQuery query;
        url.setScheme("vless");
        url.setUserName(uuid);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        query.addQueryItem("encryption", encryption);
        if (!flow.isEmpty()) query.addQueryItem("flow", flow);

        mergeUrlQuery(query, streamSetting->ExportToLink());
        mergeUrlQuery(query, multiplex->ExportToLink());

        if (!query.isEmpty()) url.setQuery(query.toString(QUrl::FullyEncoded).replace('+', "%2B"), QUrl::TolerantMode);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayVless::ExportToJson() {
        QJsonObject object;
        if (!name.isEmpty()) object["tag"] = name;
        object["protocol"] = "vless";
        QJsonObject settings;
        settings["address"] = server;
        settings["port"] = server_port;
        settings["id"] = uuid;
        settings["encryption"] = encryption;
        if (!flow.isEmpty() && flow != "none") settings["flow"] = flow;
        object["settings"] = settings;
        auto streamObj = withBindInterface(streamSetting->ExportToJson(), dialFields->bind_interface);
        if (!streamObj.isEmpty()) object["streamSettings"] = streamObj;
        if (auto muxObj = multiplex->ExportToJson(); !muxObj.isEmpty()) object["mux"] = muxObj;
        return object;
    }

    BuildResult xrayVless::Build() {
        QJsonObject object;
        object["type"] = "socks";
        object["server"] = "127.0.0.1";
        return {object, ""};
    }

    BuildResult xrayVless::BuildXray() {
        QJsonObject object;
        object["protocol"] = "vless";
        QJsonObject settings;
        settings["address"] = server;
        settings["port"] = server_port;
        settings["id"] = uuid;
        settings["encryption"] = encryption;
        if (!flow.isEmpty() && flow != "none") settings["flow"] = flow;
        object["settings"] = settings;
        auto streamObj = withBindInterface(streamSetting->Build().object, dialFields->bind_interface);
        if (!streamObj.isEmpty()) object["streamSettings"] = streamObj;
        // Not in ExportToJson: a copied interface would outlive clearing the bind in the Advanced dialog.
        BindXrayDownloadLeg(object, dialFields->bind_interface);
        if (auto muxObj = multiplex->Build().object; !muxObj.isEmpty()) object["mux"] = muxObj;
        return {object, ""};
    }

    SecurityInfo xrayVless::GetSecurity() {
        auto info = outbound::GetSecurity();
        // VLESS Encryption wraps the whole XHTTP connection, so the download leg cannot weaken it.
        if (IsVlessEncrypted(encryption)) {
            if (info.level == SecurityLevel::None) {
                info.label = QObject::tr("Encrypted");
                info.level = SecurityLevel::Secure;
            }
            return info;
        }
        if (NormalizeXrayNetwork(streamSetting->network) != "xhttp") return info;
        const QJsonObject stream{{"network", "xhttp"}, {"xhttpSettings", streamSetting->xhttp->ExportToJson()}};
        return WithXrayDownloadLeg(info, GetAddress(), XrayDownloadLeg(QJsonObject{{"streamSettings", stream}}));
    }

    // The XHTTP download leg is a whole Xray stream kept as written, so it can carry a TLS key log and the source's dialer settings.
    void xrayVless::StripForeignState(Validation::Issues &issues) {
        auto &x = *streamSetting->xhttp;
        const QString at = QStringLiteral("streamSettings.xhttpSettings.extra.");
        const auto strip = [&](QJsonObject &download, const QString &downloadAt) {
            const bool keyLog = StripXrayStreamKeyLog(download, downloadAt, issues);
            return StripXrayDialerState(download) || keyLog;
        };
        if (auto download = QString2QJsonObject(x.downloadSettings);
            !download.isEmpty() && strip(download, at + QStringLiteral("downloadSettings."))) {
            x.downloadSettings = QJsonObject2QString(download, true);
        }
        for (const auto &key : x.rawExtra.keys()) {
            if (key.compare(QLatin1String("downloadSettings"), Qt::CaseInsensitive) != 0) continue;
            auto download = x.rawExtra.value(key).toObject();
            if (strip(download, at + key + QLatin1Char('.'))) x.rawExtra[key] = download;
        }
    }

    void xrayVless::Validate(Validation::Issues &issues) {
        outbound::Validate(issues);
        using Validation::Severity;
        if (auto message = Validation::XrayId(uuid); !message.isEmpty()) issues.append({Severity::Error, "id", message});
        if (auto message = Checks::XrayVlessEncryption(encryption); !message.isEmpty()) issues.append({Severity::Error, "encryption", message});
        static const QStringList flows = QStringList{"", "none"} + xrayFlows;
        if (auto message = Validation::OneOf(flow, flows); !message.isEmpty()) {
            issues.append({Severity::Error, "flow", message, flow});
        } else if (auto message = Checks::XrayVlessVision(flow, encryption, streamSetting->network, streamSetting->security); !message.isEmpty()) {
            issues.append({Severity::Error, "flow", message, flow});
        }
    }
}
