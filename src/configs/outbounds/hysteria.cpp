#include "include/configs/outbounds/hysteria.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace Configs {
    namespace {
        constexpr qint64 kHysteriaMinHopNs = 5LL * 1000 * 1000 * 1000;

        // Without a diagnostics scope (DB loads, clones) a parse keeps every value as it always did.
        bool hysteriaKeep(const QString &field, const QString &value, const Validation::Rule &rule) {
            return !ParseDiag::Active() || AcceptOptional(field, rule(value), value);
        }

        void hysteriaReport(Validation::Issues &issues, const QString &field, const QString &message,
                            Validation::Severity severity = Validation::Severity::Error, const QString &value = {}) {
            if (!message.isEmpty()) issues << Validation::Issue{severity, field, message, value};
        }

        std::optional<Validation::PortRangeStyle> hysteriaRangeStyle(const QString &item) {
            // The core needs ':' (and reports start > end as no ports, not an error); portsToPorts() rewrites '-'.
            if (item.contains(u':')) return Validation::PortRangeStyle{Validation::RangeSep::Colon, false, true, false};
            if (item.contains(u'-')) return Validation::PortRangeStyle{Validation::RangeSep::Dash, false, true, false};
            return std::nullopt;
        }

        void hysteriaDropBadGeckoSizes(int &minSize, int &maxSize) {
            if (!ParseDiag::Active()) return;
            const auto message = Checks::HysteriaGeckoSizes(minSize, maxSize);
            if (AcceptOptional("obfs.min_packet_size", message, QStringLiteral("%1/%2").arg(minSize).arg(maxSize))) return;
            minSize = 0;
            maxSize = 0;
        }

        // sing byteformats NetworkBytesCompat units: only "<prefix>bps" counts bits; every other unit is bytes.
        double hysteriaSpeedUnit(const QString &unit) {
            static const QString prefixes = QStringLiteral("KMGTPE");
            if (unit == QLatin1String("Bps")) return 1;
            if (unit.size() == 4 && unit.endsWith(QLatin1String("ps")) && prefixes.contains(unit[0])) {
                const double scale = std::pow(1000.0, prefixes.indexOf(unit[0]) + 1);
                if (unit[1] == u'B') return scale;
                if (unit[1] == u'b') return scale / 8;
            }
            const QString lower = unit.toLower();
            if (lower == QLatin1String("b")) return 1;
            if (lower.isEmpty() || !prefixes.contains(lower[0].toUpper())) return 0;
            const int power = int(prefixes.indexOf(lower[0].toUpper())) + 1;
            const QStringView rest = QStringView(lower).mid(1);
            if (rest.isEmpty() || rest == u"b") return std::pow(1000.0, power);
            if (rest == u"i" || rest == u"ib") return std::pow(1024.0, power);
            return 0;
        }

        // A JSON number is bytes per second; a string is digits followed by a unit.
        std::optional<int> hysteriaJsonSpeedMbps(const QJsonValue &value) {
            double bytesPerSecond = 0;
            if (value.isDouble()) {
                bytesPerSecond = value.toDouble();
                if (bytesPerSecond < 0 || bytesPerSecond != std::floor(bytesPerSecond)) return std::nullopt;
            } else if (value.isString()) {
                const QString text = value.toString();
                qsizetype digits = 0;
                while (digits < text.size() && text[digits] >= u'0' && text[digits] <= u'9') ++digits;
                if (digits == 0 || digits == text.size()) return std::nullopt;
                bool ok = false;
                const quint64 number = text.left(digits).toULongLong(&ok);
                const double unit = hysteriaSpeedUnit(text.mid(digits).trimmed());
                if (!ok || unit == 0) return std::nullopt;
                bytesPerSecond = double(number) * unit;
            } else {
                return std::nullopt;
            }
            if (bytesPerSecond <= 0) return 0;
            const double mbps = std::min(bytesPerSecond / 125000.0, double(std::numeric_limits<int>::max()));
            return int(std::max<long long>(1, std::llround(mbps)));
        }
    }

    namespace Checks {
        QString HysteriaPortItem(const QString &item) {
            const QString t = item.trimmed();
            if (t.isEmpty()) return {};
            const auto style = hysteriaRangeStyle(t);
            return style ? Validation::PortRange(t, *style) : Validation::Port(t);
        }

        QString HysteriaPortItemOrder(const QString &item) {
            const QString t = item.trimmed();
            const auto style = hysteriaRangeStyle(t);
            return style ? Validation::PortRangeOrder(t, *style) : QString();
        }

        // sing-quic hysteria/hop.go: checked on every dial, never at start.
        QString HysteriaHopMinimum(bool hopping, const QString &interval) {
            if (!hopping) return {};
            const auto ns = Validation::ParseDurationNs(interval);
            if (!ns || *ns <= 0 || *ns >= kHysteriaMinHopNs) return {};
            return QCoreApplication::translate("Validation", "Port hopping needs at least 5s between hops, for example 30s.");
        }

        QString HysteriaHopMaximum(bool hopping, const QString &interval, const QString &intervalMax) {
            if (!hopping || interval.trimmed().isEmpty()) return {};
            const auto maxNs = Validation::ParseDurationNs(intervalMax);
            if (!maxNs || *maxNs <= 0) return {};
            const auto minNs = Validation::ParseDurationNs(interval);
            if (!minNs) return {};
            if (*minNs == 0)
                return QCoreApplication::translate("Validation", "Needs a Hop Interval above 0 as well, for example 30s with a maximum of 60s.");
            if (*minNs > *maxNs) return QCoreApplication::translate("Validation", "Must not be shorter than Hop Interval.");
            return {};
        }

        // Build() leaves the maximum out without an interval, so the core's 30s default applies.
        QString HysteriaHopMaximumIgnored(bool hopping, const QString &interval, const QString &intervalMax) {
            if (!hopping || !interval.trimmed().isEmpty()) return {};
            const auto maxNs = Validation::ParseDurationNs(intervalMax);
            if (!maxNs || *maxNs <= 0) return {};
            return QCoreApplication::translate("Validation", "Ignored without a Hop Interval; set one as well, for example 30s with a maximum of 60s.");
        }

        QString HysteriaV1Speed(qint64 upMbps, qint64 downMbps) {
            if (upMbps > 0 && downMbps > 0) return {};
            return QCoreApplication::translate("Validation", "Hysteria v1 needs both Up Mbps and Down Mbps, for example 100.");
        }

        QString HysteriaGeckoSizes(qint64 minSize, qint64 maxSize) {
            if (minSize < 0 || maxSize < 0 || minSize > 2048 || maxSize > 2048)
                return QCoreApplication::translate("Validation", "Packet sizes must be from 0 to 2048; 0 uses the default.");
            if ((minSize == 0 ? 512 : minSize) <= (maxSize == 0 ? 1200 : maxSize)) return {};
            return QCoreApplication::translate("Validation", "The minimum packet size must not exceed the maximum; 0 means the default, 512 for the minimum and 1200 for the maximum.");
        }

        QString HysteriaAuth(const QString &type, const QString &auth) {
            if (type != QLatin1String("BASE64") || auth.isEmpty()) return {};
            // The core decodes it as a JSON []byte: strict standard base64, padding included.
            if (auth == auth.trimmed() && auth.size() % 4 == 0 && Validation::Base64(auth).isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Not valid base64. Choose STRING as the authentication type for a plain-text payload.");
        }

        QString HysteriaAuthType(const QString &type, const QString &auth) {
            if (!type.isEmpty() || auth.isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Choose an authentication type; without one the payload is not sent.");
        }

        QString HysteriaRealm(const QString &url, const QString &id, const QStringList &stunServers, bool portMapping,
                              int ipVersion, const QString &timeout, const QString &lifetime) {
            if (url.trimmed().isEmpty() || id.trimmed().isEmpty() || stunServers.isEmpty())
                return QCoreApplication::translate("Validation", "Realm needs a URL, an ID and at least one STUN server.");
            if (const auto message = Validation::Url(url, {"http", "https"}, Validation::Empty::Required); !message.isEmpty())
                return QCoreApplication::translate("Validation", "Realm URL: %1").arg(message);
            if (const auto message = Validation::EachItem(stunServers.join(u','), Validation::Split::Comma, Validation::Rules::HostPort(false));
                !message.isEmpty())
                return QCoreApplication::translate("Validation", "STUN Servers: %1").arg(message);
            if (portMapping && ipVersion != 6) {
                if (const auto message = Validation::Duration(timeout); !message.isEmpty())
                    return QCoreApplication::translate("Validation", "Mapping Timeout: %1").arg(message);
                if (const auto message = Validation::Duration(lifetime); !message.isEmpty())
                    return QCoreApplication::translate("Validation", "Mapping Lifetime: %1").arg(message);
            }
            return {};
        }
    }

    QStringList portsToPorts(const QStringList &ports) {
        static const QRegularExpression separator("[:-]");
        QStringList result;
        for (const QString &raw : ports) {
            const QString v = raw.trimmed();
            // An empty item would become ":", every port.
            if (v.isEmpty()) continue;
            const QStringList range = v.split(separator);
            if (range.size() == 2) {
                result.append(QString("%1:%2").arg(range[0], range[1]));
            } else {
                result.append(QString("%1:%2").arg(v, v));
            }
        }
        return result;
    }

    // The core only checks realm's required fields when it first dials, so emit them unconditionally.
    static QJsonObject buildRealmObject(const hysteria &out)
    {
        QJsonObject realm;
        realm["server_url"] = out.realm_server_url;
        if (!out.realm_token.isEmpty()) realm["token"] = out.realm_token;
        realm["realm_id"] = out.realm_id;
        if (!out.realm_stun_servers.isEmpty()) realm["stun_servers"] = QListStr2QJsonArray(out.realm_stun_servers);
        if (out.realm_ip_version == 4 || out.realm_ip_version == 6) realm["ip_version"] = out.realm_ip_version;
        // Hole punching over IPv6 has no gateway mapping to make; the core rejects the pair.
        if (out.realm_port_mapping && out.realm_ip_version != 6) {
            QJsonObject portMapping{{"enabled", true}};
            if (!out.realm_port_mapping_timeout.isEmpty()) portMapping["timeout"] = out.realm_port_mapping_timeout;
            if (!out.realm_port_mapping_lifetime.isEmpty()) portMapping["lifetime"] = out.realm_port_mapping_lifetime;
            realm["port_mapping"] = portMapping;
        }
        if (!out.realm_http_client.isEmpty()) realm["http_client"] = out.realm_http_client;
        return realm;
    }

    // realm replaces the server address; the core refuses a config carrying both (hop_interval is fine).
    static void applyRealmObject(QJsonObject &object, const hysteria &out)
    {
        object.remove("server");
        object.remove("server_port");
        object.remove("server_ports");
        object["realm"] = buildRealmObject(out);
    }

    // A bare number is Mbps; only the final b/B tells bits from bytes, so "100 Mbps" is 100 and "50 MB/s" is 400.
    static int clashBandwidthToMbps(const QString &text)
    {
        static const QRegularExpression re(R"(^(\d+(?:\.\d+)?)\s*([KMGTkmgt]?)(?:([bB])(?:ps|/s)?|ps)?$)");
        const auto match = re.match(text.trimmed());
        if (!match.hasMatch()) return 0;

        bool ok = false;
        const double value = match.captured(1).toDouble(&ok);
        if (!ok || !std::isfinite(value)) return 0;

        const auto prefix = match.captured(2).toUpper();
        const auto unit = match.captured(3);
        double mbps = value;
        if (!prefix.isEmpty() || !unit.isEmpty()) {
            double scale = 1e-6;
            if (prefix == "K") scale = 1e-3;
            else if (prefix == "M") scale = 1.0;
            else if (prefix == "G") scale = 1e3;
            else if (prefix == "T") scale = 1e6;
            if (unit == "B") scale *= 8.0;
            mbps = value * scale;
        }
        if (mbps <= 0) return 0;
        const auto rounded = std::llround(std::min(mbps, static_cast<double>(std::numeric_limits<int>::max())));
        return static_cast<int>(std::max<long long>(1, rounded));
    }

    static int hysteriaClashSpeed(const QString &field, const std::string &raw)
    {
        const auto text = QString::fromStdString(raw);
        const int mbps = clashBandwidthToMbps(text);
        if (mbps == 0 && Validation::ToInt(text) != 0) {
            ParseDiag::Warn(field, QCoreApplication::translate("Validation", "Not a bandwidth such as 100 Mbps or 50 MB/s, so it was ignored."), text);
        }
        return mbps;
    }

    static void hysteriaRejectTransport(const QString &field, const QString &transport)
    {
        if (transport.trimmed().isEmpty() || transport.trimmed().compare("udp", Qt::CaseInsensitive) == 0) return;
        ParseDiag::Error(field, QCoreApplication::translate("Validation", "Only the udp protocol is supported, so this server cannot be reached."), transport);
    }

    bool hysteria::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid())
        {
            if(!url.errorString().startsWith("Invalid port"))
                return false;
            QString authority = url.authority();
            int portStartIndex = link.indexOf(authority) + authority.length();
            QRegularExpressionMatch match = QRegularExpression("^:([\\d,\\-]+)").match(link.mid(portStartIndex));
            if (match.hasMatch()) {
                server_ports = portsToPorts(match.captured(1).split(",", Qt::SkipEmptyParts));
            }
        }
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);

        if (url.scheme() == "hysteria") {
            protocol_version = "1";
            if (query.hasQueryItem("protocol")) hysteriaRejectTransport("protocol", query.queryItemValue("protocol"));
            if (query.hasQueryItem("obfsParam")) obfs = query.queryItemValue("obfsParam", QUrl::FullyDecoded);
            if (query.hasQueryItem("auth")) {
                auth = query.queryItemValue("auth");
                auth_type = "STRING";
            }
            if (query.hasQueryItem("recv_window_conn")) recv_window_conn = query.queryItemValue("recv_window_conn").toInt();
            if (query.hasQueryItem("recv_window")) recv_window = query.queryItemValue("recv_window").toInt();
            if (query.hasQueryItem("disable_mtu_discovery")) disable_mtu_discovery = query.queryItemValue("disable_mtu_discovery") == "true";
        } else {
            protocol_version = "2";
            if (url.password().isEmpty()) {
                password = url.userName();
            } else {
                password = url.userName() + ":" + url.password();
            }
            if (query.hasQueryItem("obfs-password")) {
                obfs = query.queryItemValue("obfs-password", QUrl::FullyDecoded);
            }
            if (query.hasQueryItem("obfs")) {
                obfs_type = query.queryItemValue("obfs").trimmed().toLower();
            }
            else if (query.hasQueryItem("min_packet_size") || query.hasQueryItem("max_packet_size")) {
                obfs_type = "gecko";
            }
            else {
                obfs_type = "salamander";
            }
            for (const auto &[key, target] : {std::pair{"min_packet_size", &min_packet_size}, std::pair{"max_packet_size", &max_packet_size}}) {
                if (!query.hasQueryItem(key)) continue;
                const auto value = query.queryItemValue(key).trimmed();
                if (hysteriaKeep(key, value, Validation::Rules::Int(0, 2048))) *target = value.toInt();
            }
            if (obfs_type == "gecko") hysteriaDropBadGeckoSizes(min_packet_size, max_packet_size);
            if (query.hasQueryItem("hop_interval_max")) {
                const auto value = query.queryItemValue("hop_interval_max").trimmed();
                if (hysteriaKeep("hop_interval_max", value, Validation::Rules::Duration())) hop_interval_max = value;
            }
            if (query.hasQueryItem("bbr_profile")) {
                const auto value = query.queryItemValue("bbr_profile").trimmed();
                if (hysteriaKeep("bbr_profile", value, Validation::Rules::OneOf(hysteriaBBRProfiles))) bbr_profile = value;
            }
            if (query.hasQueryItem("disable_chrome_parrot")) disable_chrome_parrot = query.queryItemValue("disable_chrome_parrot") == "true";
        }

        for (const auto &[key, target] : {std::pair{"upmbps", &up_mbps}, std::pair{"downmbps", &down_mbps}}) {
            if (!query.hasQueryItem(key)) continue;
            const auto value = query.queryItemValue(key).trimmed();
            if (hysteriaKeep(key, value, Validation::Rules::Int(0, std::numeric_limits<int>::max()))) *target = value.toInt();
        }
        if (query.hasQueryItem("hop_interval")) {
            const auto value = query.queryItemValue("hop_interval").trimmed();
            if (hysteriaKeep("hop_interval", value, Validation::Rules::Duration())) hop_interval = value;
        }
        if (query.hasQueryItem("mport")) {
            server_ports = portsToPorts(query.queryItemValue("mport", QUrl::FullyDecoded).split(",", Qt::SkipEmptyParts));
        }

        tls->ParseFromLink(link);
        tls->enabled = true; // Hysteria always uses tls
        // The official hysteria2 URI pins a certificate's SHA-256 in hex, as sing-box's certificate_sha256 does in base64.
        if (query.hasQueryItem("pinSHA256")) {
            bool pinned = false;
            for (auto pin : query.queryItemValue("pinSHA256", QUrl::FullyDecoded).split(u',', Qt::SkipEmptyParts)) {
                pin = pin.trimmed();
                // The official client's normalizeCertHash strips both separators.
                const auto hex = QString(pin).remove(u':').remove(u'-');
                QString hash;
                if (Validation::Hex(hex, 64, 64, true, Validation::Empty::Required).isEmpty()) {
                    hash = QString::fromLatin1(QByteArray::fromHex(hex.toLatin1()).toBase64());
                } else if (Validation::Base64Key(pin, 32).isEmpty() || Validation::Base64Key(pin, 32, false, false).isEmpty()) {
                    hash = QString::fromLatin1(QByteArray::fromBase64(pin.toLatin1()).toBase64());
                } else {
                    ParseDiag::Error("pinSHA256", QCoreApplication::translate("Validation", "The certificate pin must be a SHA-256 hash in hex (64 characters) or base64 (44 characters)."), pin);
                    continue;
                }
                if (!tls->certificate_sha256.contains(hash)) tls->certificate_sha256 << hash;
                pinned = true;
            }
            // The official client accepts a pin of any certificate in the chain; sing-box checks only the server's own.
            if (pinned) {
                ParseDiag::Warn("pinSHA256", QCoreApplication::translate("Validation", "Only the server's own certificate is checked against this pin; if it is the hash of an intermediate or root certificate, the connection will fail."));
            }
        }
        quic->ParseFromLink(link);

        if (server_port == 0 && server_ports.isEmpty()) server_port = 443;

        return true;
    }

    bool hysteria::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object["type"].toString() == "hysteria") {
            protocol_version = "1";
        } else if (object["type"].toString() == "hysteria2") {
            protocol_version = "2";
        } else {
            return false;
        }
        outbound::ParseFromJson(object);
        if (object.contains("server_ports")) {
            // LegacyListable: a bare string is a single range.
            server_ports = JsonStringList(object["server_ports"], "server_ports");
        }
        if (object.contains("hop_interval")) {
            const auto value = object["hop_interval"].toString().trimmed();
            if (hysteriaKeep("hop_interval", value, Validation::Rules::Duration())) hop_interval = value;
        }
        if (object.contains("up_mbps")) up_mbps = object["up_mbps"].toInt();
        if (object.contains("down_mbps")) down_mbps = object["down_mbps"].toInt();
        if (protocol_version == "1") {
            // The core prefers up/down over up_mbps/down_mbps when they are set.
            for (const auto &[key, target] : {std::pair{"up", &up_mbps}, std::pair{"down", &down_mbps}}) {
                if (!object.contains(key)) continue;
                const auto value = object[key];
                if (const auto mbps = hysteriaJsonSpeedMbps(value)) {
                    if (*mbps > 0) *target = *mbps;
                } else {
                    ParseDiag::Warn(key, QCoreApplication::translate("Validation", "Not a speed such as 100 Mbps or 12 MBps, so it was ignored."),
                                    value.isString() ? value.toString() : QString::number(value.toDouble()));
                }
            }
            if (object.contains("obfs")) obfs = object["obfs"].toString();
            if (object.contains("auth")) {
                auth = object["auth"].toString();
                auth_type = "BASE64";
            }
            if (object.contains("auth_str")) {
                auth = object["auth_str"].toString();
                auth_type = "STRING";
            }
            if (object.contains("recv_window_conn")) recv_window_conn = object["recv_window_conn"].toInt();
            if (object.contains("recv_window")) recv_window = object["recv_window"].toInt();
            if (object.contains("disable_mtu_discovery")) disable_mtu_discovery = object["disable_mtu_discovery"].toBool();
        } else {
            if (object.contains("obfs")) {
                auto obfsObj = object["obfs"].toObject();
                if (obfsObj.contains("password")) obfs = obfsObj["password"].toString();
                if (obfsObj.contains("type")) obfs_type = obfsObj["type"].toString().trimmed().toLower();
                else obfs_type = "salamander";
                for (const auto &[key, target] : {std::pair{"min_packet_size", &min_packet_size}, std::pair{"max_packet_size", &max_packet_size}}) {
                    if (!obfsObj.contains(key)) continue;
                    const int value = obfsObj[key].toInt();
                    if (hysteriaKeep(QStringLiteral("obfs.") + key, QString::number(value), Validation::Rules::Int(0, 2048))) *target = value;
                }
                if (obfs_type == "gecko") hysteriaDropBadGeckoSizes(min_packet_size, max_packet_size);
            }
            if (object.contains("obfsPassword")) obfs = object["obfsPassword"].toString();
            if (object.contains("password")) password = object["password"].toString();
            if (object.contains("hop_interval_max")) {
                const auto value = object["hop_interval_max"].toString().trimmed();
                if (hysteriaKeep("hop_interval_max", value, Validation::Rules::Duration())) hop_interval_max = value;
            }
            if (object.contains("bbr_profile")) {
                const auto value = object["bbr_profile"].toString().trimmed();
                if (hysteriaKeep("bbr_profile", value, Validation::Rules::OneOf(hysteriaBBRProfiles))) bbr_profile = value;
            }
            if (object.contains("disable_chrome_parrot")) disable_chrome_parrot = object["disable_chrome_parrot"].toBool();
            if (object["realm"].isObject()) {
                auto realmObj = object["realm"].toObject();
                realm_enabled = true;
                realm_server_url = realmObj["server_url"].toString().trimmed();
                realm_token = realmObj["token"].toString();
                realm_id = realmObj["realm_id"].toString();
                // Listable: the core accepts a bare string as well as an array.
                if (realmObj["stun_servers"].isArray()) realm_stun_servers = QJsonArray2QListString(realmObj["stun_servers"].toArray());
                else if (realmObj.contains("stun_servers")) realm_stun_servers = {realmObj["stun_servers"].toString()};
                for (auto &stun : realm_stun_servers) stun = stun.trimmed();
                if (const int ipVersion = realmObj["ip_version"].toInt();
                    hysteriaKeep("realm.ip_version", QString::number(ipVersion), Validation::Rules::OneOf({"0", "4", "6"}))) {
                    realm_ip_version = ipVersion;
                }
                auto portMapping = realmObj["port_mapping"].toObject();
                realm_port_mapping = portMapping["enabled"].toBool();
                if (const auto value = portMapping["timeout"].toString().trimmed(); hysteriaKeep("realm.port_mapping.timeout", value, Validation::Rules::Duration())) {
                    realm_port_mapping_timeout = value;
                }
                if (const auto value = portMapping["lifetime"].toString().trimmed(); hysteriaKeep("realm.port_mapping.lifetime", value, Validation::Rules::Duration())) {
                    realm_port_mapping_lifetime = value;
                }
                realm_http_client = realmObj["http_client"].toObject();
            }
        }
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        quic->ParseFromJson(object);
        return true;
    }

    bool hysteria::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type == "hysteria") {
            protocol_version = "1";
        } else if (object.type == "hysteria2") {
            protocol_version = "2";
        } else {
            return false;
        }
        outbound::ParseFromClash(object);

        if (!object.ports.empty()) server_ports = portsToPorts(QString::fromStdString(object.ports).split(QRegularExpression("[,/]"), Qt::SkipEmptyParts));
        if (!object.up.empty()) up_mbps = hysteriaClashSpeed("up", object.up);
        if (!object.down.empty()) down_mbps = hysteriaClashSpeed("down", object.down);
        if (protocol_version == "1") {
            hysteriaRejectTransport("protocol", QString::fromStdString(object.protocol));
            if (!object.auth_str.empty()) {
                auth = QString::fromStdString(object.auth_str);
                auth_type = "STRING";
            } else if (!object.auth_str1.empty()) {
                auth = QString::fromStdString(object.auth_str1);
                auth_type = "STRING";
            }
            if (!object.obfs.empty()) obfs = QString::fromStdString(object.obfs);
            if (object.recv_window > 0) {
                recv_window = object.recv_window;
            } else if (object.recv_window1 > 0) {
                recv_window = object.recv_window1;
            }
            if (object.recv_window_conn > 0) {
                recv_window_conn = object.recv_window_conn;
            } else if (object.recv_window_conn1 > 0) {
                recv_window_conn = object.recv_window_conn1;
            }
            disable_mtu_discovery = object.disable_mtu_discovery;
        } else {
            if (!object.password.empty()) password = QString::fromStdString(object.password);
            if (!object.obfs.empty()) obfs_type = QString::fromStdString(object.obfs).trimmed().toLower();
            if (!object.obfs_password.empty()) obfs = QString::fromStdString(object.obfs_password);
        }

        tls->ParseFromClash(object);
        tls->enabled = true;
        return true;
    }

    void hysteria::Validate(Validation::Issues &issues)
    {
        using Validation::Severity;
        outbound::Validate(issues);
        const bool realm = RealmActive();
        if (!realm) {
            for (qsizetype i = 0; i < server_ports.size(); ++i) {
                const auto item = server_ports[i].trimmed();
                const auto numbered = [&](const QString &message) {
                    return QCoreApplication::translate("Validation", "Item %1 (\"%2\"): %3").arg(QString::number(i + 1), item, message);
                };
                if (const auto message = Checks::HysteriaPortItem(item); !message.isEmpty()) {
                    hysteriaReport(issues, "server_ports", numbered(message));
                } else if (const auto order = Checks::HysteriaPortItemOrder(item); !order.isEmpty()) {
                    hysteriaReport(issues, "server_ports", numbered(order), Severity::Warning);
                }
            }
        }
        const bool hopping = !realm && !server_ports.isEmpty();
        if (!realm) hysteriaReport(issues, "hop_interval", Validation::Duration(hop_interval));
        hysteriaReport(issues, "hop_interval", Checks::HysteriaHopMinimum(hopping, hop_interval));

        if (protocol_version == "1") {
            hysteriaReport(issues, "up_mbps", Checks::HysteriaV1Speed(up_mbps, down_mbps));
            hysteriaReport(issues, "auth", Checks::HysteriaAuth(auth_type, auth));
            hysteriaReport(issues, "auth", Checks::HysteriaAuthType(auth_type, auth), Severity::Warning);
            return;
        }

        if (!realm) hysteriaReport(issues, "hop_interval_max", Validation::Duration(hop_interval_max));
        hysteriaReport(issues, "hop_interval_max", Checks::HysteriaHopMaximum(hopping, hop_interval, hop_interval_max));
        hysteriaReport(issues, "hop_interval_max", Checks::HysteriaHopMaximumIgnored(hopping, hop_interval, hop_interval_max), Severity::Warning);
        if (!obfs.isEmpty()) {
            const auto type = obfs_type.trimmed().toLower();
            if (const auto message = Validation::OneOf(type, hysteriaObfsTypes); !message.isEmpty()) {
                hysteriaReport(issues, "obfs.type", message, Severity::Error, obfs_type);
            } else if (type == "gecko") {
                hysteriaReport(issues, "obfs.min_packet_size", Checks::HysteriaGeckoSizes(min_packet_size, max_packet_size));
            }
        }
        if (realm) {
            hysteriaReport(issues, "realm", Checks::HysteriaRealm(realm_server_url, realm_id, realm_stun_servers, realm_port_mapping, realm_ip_version,
                                                                  realm_port_mapping_timeout, realm_port_mapping_lifetime));
        }
    }

    QString hysteria::ExportToLink()
    {
        // No hysteria2:// form carries realm, so callers fall back to the throne:// JSON link.
        if (RealmActive()) return {};

        QUrl url;
        QUrlQuery query;
        url.setScheme(protocol_version == "1" ? "hysteria" : "hysteria2");
        url.setHost(server);
        
        if (!name.isEmpty()) url.setFragment(name);

        if (protocol_version == "1") {
            if (!obfs.isEmpty()) {
                query.addQueryItem("obfsParam", QUrl::toPercentEncoding(obfs));
            }
            if (auth_type == "STRING" && !auth.isEmpty()) query.addQueryItem("auth", auth);
            if (recv_window_conn > 0) query.addQueryItem("recv_window_conn", QString::number(recv_window_conn));
            if (recv_window > 0) query.addQueryItem("recv_window", QString::number(recv_window));
            if (disable_mtu_discovery) query.addQueryItem("disable_mtu_discovery", "true");
        } else {
            if (password.contains(":")) {
                url.setUserName(SubStrBefore(password, ":"));
                url.setPassword(SubStrAfter(password, ":"));
            } else {
                url.setUserName(password);
            }
            if (!obfs.isEmpty()) {
                query.addQueryItem("obfs", QUrl::toPercentEncoding(obfs_type));
                query.addQueryItem("obfs-password", QUrl::toPercentEncoding(obfs));
                if (min_packet_size > 0) query.addQueryItem("min_packet_size", QString::number(min_packet_size));
                if (max_packet_size > 0) query.addQueryItem("max_packet_size", QString::number(max_packet_size));
            }
            if (!hop_interval_max.isEmpty()) query.addQueryItem("hop_interval_max", hop_interval_max);
            if (!bbr_profile.isEmpty()) query.addQueryItem("bbr_profile", bbr_profile);
            if (disable_chrome_parrot) query.addQueryItem("disable_chrome_parrot", "true");
        }
        
        if (up_mbps > 0) query.addQueryItem("upmbps", QString::number(up_mbps));
        if (down_mbps > 0) query.addQueryItem("downmbps", QString::number(down_mbps));
        
        if (!hop_interval.isEmpty()) query.addQueryItem("hop_interval", hop_interval);
        
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, quic->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        
        QString result;
        if (!server_ports.isEmpty()) {
            QStringList portList;
            for (const auto& port : server_ports) {
                QString modified = port;
                modified.replace(":", "-");
                portList.append(modified);
            }
            result = url.toString(QUrl::FullyEncoded);
            QString authority = url.authority(QUrl::FullyEncoded);
            int portStartIndex = result.indexOf(authority) + authority.length();
            result.insert(portStartIndex, ":" + portList.join(","));
        } else {
            url.setPort(server_port);
            result = url.toString(QUrl::FullyEncoded);
        }
        
        return result;
    }

    QJsonObject hysteria::ExportToJson()
    {
        QJsonObject object;
        object["type"] = protocol_version == "1" ? "hysteria" : "hysteria2";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!server_ports.isEmpty()) object["server_ports"] = QListStr2QJsonArray(portsToPorts(server_ports));
        if (!hop_interval.isEmpty()) object["hop_interval"] = hop_interval;
        if (up_mbps > 0) object["up_mbps"] = up_mbps;
        if (down_mbps > 0) object["down_mbps"] = down_mbps;
        if (protocol_version == "1") {
            if (!obfs.isEmpty()) object["obfs"] = obfs;
            if (!auth.isEmpty()) {
                if (auth_type == "BASE64") object["auth"] = auth;
                if (auth_type == "STRING") object["auth_str"] = auth;
            }
            if (recv_window_conn > 0) object["recv_window_conn"] = recv_window_conn;
            if (recv_window > 0) object["recv_window"] = recv_window;
            if (disable_mtu_discovery) object["disable_mtu_discovery"] = disable_mtu_discovery;
        } else {
            if (!obfs.isEmpty()) {
                object["obfs"] = QJsonObject{
                    {"type", obfs_type},
                    {"password", obfs},
                    {"min_packet_size", min_packet_size},
                    {"max_packet_size", max_packet_size},
                };
            }
            if (!password.isEmpty()) object["password"] = password;
            if (!hop_interval_max.isEmpty()) object["hop_interval_max"] = hop_interval_max;
            if (!bbr_profile.isEmpty()) object["bbr_profile"] = bbr_profile;
            if (disable_chrome_parrot) object["disable_chrome_parrot"] = true;
            if (RealmActive()) applyRealmObject(object, *this);
        }
        object["tls"] = tls->ExportToJson();
        mergeJsonObjects(object, quic->ExportToJson());
        return object;
    }

    QJsonObject hysteria::ExportIdentity()
    {
        if (RealmActive()) {
            // outbound::ExportIdentity() falls back to the whole config when there is no server.
            QJsonObject object{
                {"protocol_version", protocol_version},
                {"realm_server_url", realm_server_url},
                {"realm_id", realm_id},
            };
            if (auto t = tls->ExportIdentity(); !t.isEmpty()) object["tls"] = t;
            return object;
        }

        auto object = outbound::ExportIdentity();
        object["protocol_version"] = protocol_version;
        if (!server_ports.isEmpty()) object["server_ports"] = server_ports.join(",");
        if (!obfs.isEmpty()) object["obfs"] = true;
        return object;
    }

    BuildResult hysteria::Build()
    {
        QJsonObject object;
        object["type"] = protocol_version == "1" ? "hysteria" : "hysteria2";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        const bool realm = RealmActive();
        if (!server_ports.isEmpty()) object["server_ports"] = QListStr2QJsonArray(portsToPorts(server_ports));
        // Unused under realm, where the editor stops checking it, yet the core would still parse it.
        if (!hop_interval.isEmpty() && !realm) object["hop_interval"] = hop_interval;
        if (up_mbps > 0) object["up_mbps"] = up_mbps;
        if (down_mbps > 0) object["down_mbps"] = down_mbps;
        if (protocol_version == "1") {
            if (!obfs.isEmpty()) object["obfs"] = obfs;
            if (!auth.isEmpty()) {
                if (auth_type == "BASE64") object["auth"] = auth;
                if (auth_type == "STRING") object["auth_str"] = auth;
            }
            if (recv_window_conn > 0) object["recv_window_conn"] = recv_window_conn;
            if (recv_window > 0) object["recv_window"] = recv_window;
            if (disable_mtu_discovery) object["disable_mtu_discovery"] = disable_mtu_discovery;
        } else {
            if (!obfs.isEmpty()) {
                const auto obfsType = obfs_type.trimmed().toLower();
                if (obfsType == "salamander") {
                    object["obfs"] = QJsonObject{
                        {"type", obfsType},
                        {"password", obfs},
                    };
                }
                else if (obfsType == "gecko") {
                    object["obfs"] = QJsonObject{
                        {"type", obfsType},
                        {"password", obfs},
                        {"min_packet_size", min_packet_size},
                        {"max_packet_size", max_packet_size},
                    };
                }
                else {
                    object["obfs"] = QJsonObject{
                        {"type", "salamander"},
                        {"password", obfs},
                    };
                }
            }
            if (!password.isEmpty()) object["password"] = password;
            // sing-box randomizes the hop interval within [hop_interval, hop_interval_max]; it rejects a max without a min.
            if (!hop_interval_max.isEmpty() && !hop_interval.isEmpty() && !realm) object["hop_interval_max"] = hop_interval_max;
            // An unknown profile makes sing-box refuse to start, so drop anything a subscription made up.
            if (hysteriaBBRProfiles.contains(bbr_profile)) object["bbr_profile"] = bbr_profile;
            if (disable_chrome_parrot) object["disable_chrome_parrot"] = true;
            if (realm) applyRealmObject(object, *this);
        }
        const auto tlsResult = tls->Build();
        if (!tlsResult.error.isEmpty()) return {{}, tlsResult.error};
        object["tls"] = tlsResult.object;
        const auto quicResult = quic->Build();
        if (!quicResult.error.isEmpty()) return {{}, quicResult.error};
        mergeJsonObjects(object, quicResult.object);
        return {object, ""};
    }

    QString hysteria::DisplayAddress()
    {
        if (!RealmActive()) return outbound::DisplayAddress();
        auto host = QUrl(realm_server_url).host();
        if (host.isEmpty()) host = realm_server_url;
        return realm_id.isEmpty() ? host : realm_id + "@" + host;
    }

    QString hysteria::DisplayType()
    {
        return "Hysteria";
    }

    SecurityInfo hysteria::GetSecurity()
    {
        return SecurityFromTLS("QUIC");
    }
}
