#include "include/configs/outbounds/xrayVless.h"

#include <QCoreApplication>
#include <QUrlQuery>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"
#include "include/configs/outbounds/custom.h"
#include "include/global/Utils.hpp"

#include <algorithm>
#include <optional>

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

        // Share links leave encryption empty for none, which Xray refuses.
        QString xrayVlessImportEncryption(const QString &raw) {
            const QString value = raw.trimmed();
            return value.isEmpty() || value.compare("none", Qt::CaseInsensitive) == 0 ? QStringLiteral("none") : value;
        }

        QString xrayVlessFlowProblem(const xrayVless &v) {
            static const QStringList flows = QStringList{"", "none"} + xrayFlows;
            if (auto message = Validation::OneOf(v.flow, flows); !message.isEmpty()) return message;
            const auto &stream = *v.streamSetting;
            if (auto message = Checks::XrayVlessVision(v.flow, v.encryption, stream.network, stream.security); !message.isEmpty()) return message;
            return Checks::XrayXDriveFlow(v.flow, stream.network);
        }

        // No flow is Xray's default; an unknown one stops it and a misplaced Vision fails every connection.
        void dropUnusableXrayVlessFlow(xrayVless &v) {
            if (!AcceptOptional("flow", xrayVlessFlowProblem(v), v.flow)) v.flow.clear();
        }

        // FIPS 203 modulus check, as Go's mlkem.NewEncapsulationKey768: the first 1152 bytes pack 768 12-bit coefficients, each below q.
        bool mlkem768KeyValid(const QByteArray &key) {
            constexpr int q = 3329;
            if (key.size() != 1184) return false;
            const auto *b = reinterpret_cast<const uchar *>(key.constData());
            for (int i = 0; i < 1152; i += 3) {
                if ((b[i] | ((b[i + 1] & 0x0F) << 8)) >= q || ((b[i + 1] >> 4) | (b[i + 2] << 4)) >= q) return false;
            }
            return true;
        }

        // strconv.Atoi: digits with an optional sign, and '-' is the separator here.
        std::optional<qint64> xrayPaddingNumber(const QString &text) {
            const QStringView digits = text.startsWith(u'+') ? QStringView(text).mid(1) : QStringView(text);
            if (digits.isEmpty()) return std::nullopt;
            for (const QChar c : digits) {
                if (c < u'0' || c > u'9') return std::nullopt;
            }
            bool ok = false;
            const qint64 value = digits.toLongLong(&ok);
            return ok ? std::optional(value) : std::nullopt;
        }

        // Xray's ParsePadding: parts alternate chance-min-max lengths and gaps; only lengths count toward the cap.
        QString xrayVlessPaddingProblem(const QStringList &padding) {
            constexpr qint64 maxTotal = 18 + 65535;
            qint64 total = 0;
            for (qsizetype i = 0; i < padding.size(); ++i) {
                const QStringList fields = padding[i].split(u'-');
                qint64 y[3]{};
                for (int j = 0; j < 3; ++j) {
                    const auto value = j < fields.size() ? xrayPaddingNumber(fields[j]) : std::nullopt;
                    if (!value) return QCoreApplication::translate("Validation", "Each padding part must be three numbers joined by dashes, like 100-111-1111.");
                    y[j] = *value;
                }
                if (i == 0 && (y[0] < 100 || y[1] < 18 + 17 || y[2] < 18 + 17))
                    return QCoreApplication::translate("Validation", "The first padding part needs a chance of at least 100 and lengths of at least 35, like 100-111-1111.");
                if (i % 2 == 0) total = std::min(total + std::min(std::max(y[1], y[2]), maxTotal + 1), maxTotal + 1);
            }
            if (total > maxTotal) return QCoreApplication::translate("Validation", "The padding lengths can add up to more than 65553 bytes.");
            return {};
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
            // Parts under 20 characters are padding; Xray cuts their total length off the front, so they must lead.
            QStringList padding;
            qsizetype keys = 0;
            for (qsizetype i = 3; i < parts.size(); ++i) {
                if (parts[i].size() < 20) {
                    if (keys > 0) return QCoreApplication::translate("Validation", "Put the padding parts before the keys.");
                    padding << parts[i];
                    continue;
                }
                ++keys;
                if (Validation::Base64Key(parts[i], 32, true, false).isEmpty()) continue;
                if (!Validation::Base64Key(parts[i], 1184, true, false).isEmpty())
                    return QCoreApplication::translate("Validation", "Each key in the string must be URL-safe base64 of 32 or 1184 bytes, without padding.");
                const auto key = QByteArray::fromBase64(parts[i].trimmed().toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
                if (!mlkem768KeyValid(key))
                    return QCoreApplication::translate("Validation", "A 1184-byte key in the string is not a valid ML-KEM-768 key: copy the string from the server again.");
            }
            if (keys == 0) return QCoreApplication::translate("Validation", "The string has no key: copy the whole string the server gives.");
            return xrayVlessPaddingProblem(padding);
        }

        QString XrayVlessVision(const QString &flow, const QString &encryption, const QString &network, const QString &security) {
            if (!flow.startsWith("xtls-rprx-vision") || IsVlessEncrypted(encryption)) return {};
            const QString normalized = NormalizeXrayNetwork(network);
            if ((normalized.isEmpty() || normalized == "raw") && (security == "tls" || security == "reality")) return {};
            return QCoreApplication::translate("Validation", "XTLS Vision needs the raw network with TLS or REALITY, or VLESS encryption: change the network or security, or clear the flow.");
        }

        QString XrayXDriveEncryption(const QString &encryption, const QString &network) {
            if (NormalizeXrayNetwork(network.trimmed()) != "xdrive" || IsVlessEncrypted(encryption.trimmed())) return {};
            return QCoreApplication::translate("Validation", "XDRIVE needs VLESS encryption: enter the mlkem768x25519plus string the server gives.");
        }

        QString XrayXDriveFlow(const QString &flow, const QString &network) {
            const auto f = flow.trimmed();
            if (NormalizeXrayNetwork(network.trimmed()) != "xdrive" || f.isEmpty() || f.compare("none", Qt::CaseInsensitive) == 0) return {};
            return QCoreApplication::translate("Validation", "XDRIVE does not support a flow; clear it.");
        }

        QString XrayXDriveMux(int muxState, const QString &network) {
            if (NormalizeXrayNetwork(network.trimmed()) != "xdrive" || muxState != 2) return {};
            return QCoreApplication::translate("Validation", "XDRIVE needs Multiplex: set it to On or Keep default.");
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
        dropUnusableXrayVlessFlow(*this);
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
            dropUnusableXrayVlessFlow(*this);
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
        dropUnusableXrayVlessFlow(*this);
        return true;
    }

    QString xrayVless::ExportToLink() {
        // Callers fall back to ExportJsonLink, which keeps the storage settings a vless:// link cannot carry.
        if (NormalizeXrayNetwork(streamSetting->network) == "xdrive") return {};
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
        const auto stream = streamSetting->Build(DisplayTypeAndName());
        if (!stream.error.isEmpty()) return {{}, stream.error};
        auto streamObj = withBindInterface(stream.object, dialFields->bind_interface);
        if (!streamObj.isEmpty()) object["streamSettings"] = streamObj;
        // Not in ExportToJson: a copied interface would outlive clearing the bind in the Advanced dialog.
        BindXrayDownloadLeg(object, dialFields->bind_interface);
        // Xray refuses XDRIVE without Mux, so Keep Default turns it on there.
        const bool muxRequired = NormalizeXrayNetwork(streamSetting->network) == "xdrive";
        // Each XDNS connection is its own mKCP session that opens a socket per resolver; Mux shares one.
        const bool muxDefaultOn = muxRequired || streamSetting->IsXdns() || Configs::dataManager->settingsRepo->xray_mux_default_on;
        if (auto muxObj = multiplex->BuildMux(muxDefaultOn); !muxObj.isEmpty()) {
            // It also refuses any traffic leaving the mux there, which a preset may ask for.
            if (muxRequired) {
                if (muxObj["concurrency"].toInt() < 0) muxObj.remove("concurrency");
                if (muxObj["xudpConcurrency"].toInt() < 0) muxObj.remove("xudpConcurrency");
                if (muxObj["xudpProxyUDP443"].toString() == "skip") muxObj.remove("xudpProxyUDP443");
            }
            object["mux"] = muxObj;
        }
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
        StripXrayFinalmaskKeyLog(streamSetting->finalmask, QStringLiteral("streamSettings.finalmask."), issues);
    }

    void xrayVless::Validate(Validation::Issues &issues) {
        outbound::Validate(issues);
        using Validation::Severity;
        if (auto message = Validation::XrayId(uuid); !message.isEmpty()) issues.append({Severity::Error, "id", message});
        if (auto message = Checks::XrayVlessEncryption(encryption); !message.isEmpty()) issues.append({Severity::Error, "encryption", message});
        if (auto message = xrayVlessFlowProblem(*this); !message.isEmpty()) issues.append({Severity::Error, "flow", message, flow});
        if (auto message = Checks::XrayXDriveEncryption(encryption, streamSetting->network); !message.isEmpty()) {
            issues.append({Severity::Error, "encryption", message});
        }
        if (auto message = Checks::XrayXDriveMux(multiplex->getMuxState(), streamSetting->network); !message.isEmpty()) {
            issues.append({Severity::Error, "mux", message});
        }
        multiplex->Validate(issues);
    }
}
