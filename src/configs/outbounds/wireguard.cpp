#include "include/configs/outbounds/wireguard.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

#include <limits>
#include <tuple>
#include <utility>

namespace Configs {
    namespace {
        constexpr qint64 kWireguardIntMax = std::numeric_limits<int>::max();
        constexpr qint64 kWireguardUint32Max = std::numeric_limits<quint32>::max();

        // Without a diagnostics scope (DB loads, clones) a parse keeps every value as it always did.
        bool wireguardKeep(const QString& field, const QString& value, const Validation::Rule& rule) {
            return !ParseDiag::Active() || AcceptOptional(field, rule(value), value);
        }

        // S1-S4 must match the server's, so a bad one refuses the profile; Jc, Jmin and Jmax only shape what this side sends.
        int wireguardAmneziaInt(const QString& field, const QString& value, qint64 max, bool mustMatchServer) {
            const auto message = Validation::Int(value, 0, max);
            if (message.isEmpty()) return value.trimmed().toInt();
            if (mustMatchServer) ParseDiag::Error(field, message, value);
            else AcceptOptional(field, message, value);
            return 0;
        }

        void wireguardReport(Validation::Issues& issues, const QString& field, const QString& message,
                             Validation::Severity severity = Validation::Severity::Error, const QString& value = {}) {
            if (!message.isEmpty()) issues << Validation::Issue{severity, field, message, value};
        }

        // Call after FixAddress(): what is left without a valid prefix the core cannot decode.
        void wireguardDropBadAddresses(QStringList& addresses) {
            if (!ParseDiag::Active()) return;
            QStringList kept;
            for (const auto& address : addresses) {
                if (AcceptOptional("address", Validation::Cidr(address), address)) kept << address;
            }
            addresses = kept;
        }
    }

    namespace Checks {
        QString WireguardReserved(const QString& text, QList<int>* bytes) {
            if (bytes != nullptr) bytes->clear();
            const QString t = text.trimmed();
            if (t.isEmpty()) return {};
            QList<int> values;
            for (const auto& part : QString(t).replace(u'-', u',').split(u',', Qt::SkipEmptyParts)) {
                const auto value = Validation::ToInt(part);
                if (!value || *value < 0 || *value > 255) {
                    values.clear();
                    break;
                }
                values << int(*value);
            }
            if (values.size() != 3) return QCoreApplication::translate("Validation", "Use three numbers from 0 to 255, for example 12,34,56.");
            if (bytes != nullptr) *bytes = values;
            return {};
        }

        QString WireguardKeepalive(const QString& text) {
            return Validation::IntOrRange(text, 0, kWireguardIntMax);
        }

        QString AmneziaRange(const QString& text) {
            return Validation::IntOrRange(text, 0, kWireguardUint32Max);
        }

        QString AmneziaHeaders(const QStringList& headers) {
            QList<std::pair<qint64, qint64>> ranges;
            for (qsizetype i = 0; i < 4; ++i) {
                const QString text = headers.value(i).trimmed();
                if (text.isEmpty()) {
                    ranges.append({i + 1, i + 1});
                    continue;
                }
                if (!AmneziaRange(text).isEmpty()) return {};
                const qsizetype dash = text.indexOf(u'-');
                const qint64 lo = *Validation::ToInt(dash < 0 ? text : text.left(dash));
                ranges.append({lo, dash < 0 ? lo : *Validation::ToInt(text.mid(dash + 1))});
            }
            for (qsizetype i = 0; i < ranges.size(); ++i) {
                for (qsizetype j = i + 1; j < ranges.size(); ++j) {
                    if (ranges[i].first <= ranges[j].second && ranges[j].first <= ranges[i].second) {
                        return QCoreApplication::translate("Validation", "H%1 and H%2 overlap; each magic header needs its own number or range (unset ones are 1, 2, 3 and 4).")
                            .arg(i + 1)
                            .arg(j + 1);
                    }
                }
            }
            return {};
        }

        QString AmneziaHeaderProtection(const QString& key, const QList<qint64>& paddings) {
            if (key.trimmed().isEmpty()) return {};
            for (qsizetype i = 0; i < paddings.size(); ++i) {
                if (paddings[i] < 12) {
                    return QCoreApplication::translate("Validation", "Header protection needs S1 to S4 of at least 12 each; S%1 is %2.").arg(i + 1).arg(paddings[i]);
                }
            }
            return {};
        }

        QString AmneziaJunkSizes(qint64 jmin, qint64 jmax) {
            if (jmin <= jmax) return {};
            return QCoreApplication::translate("Validation", "Jmin is larger than Jmax, so every junk packet will be Jmin bytes.");
        }

        QString AmneziaSignature(const QString& spec) {
            qsizetype pos = 0;
            while (true) {
                const qsizetype start = spec.indexOf(u'<', pos);
                if (start < 0) return {};
                const qsizetype end = spec.indexOf(u'>', start);
                if (end < 0) return QCoreApplication::translate("Validation", "A tag is missing its closing >.");
                const QStringList parts = spec.mid(start + 1, end - start - 1).simplified().split(u' ', Qt::SkipEmptyParts);
                pos = end + 1;
                if (parts.isEmpty()) {
                    return QCoreApplication::translate("Validation", "Empty tag <>. Use <b 0x...>, <r N>, <rc N>, <rd N>, <dz N>, <t>, <d> or <ds>.");
                }
                const QString tag = parts.first();
                const QString argument = parts.value(1);
                if (tag == QLatin1String("b")) {
                    const QString hex = argument.startsWith(QLatin1String("0x")) ? argument.mid(2) : argument;
                    if (hex.isEmpty() || !Validation::Hex(hex, 0, 0).isEmpty()) {
                        return QCoreApplication::translate("Validation", "<b> needs an even number of hex digits, for example <b 0x1a2b>.");
                    }
                } else if (tag == QLatin1String("r") || tag == QLatin1String("rc") || tag == QLatin1String("rd") || tag == QLatin1String("dz")) {
                    const auto length = Validation::ToInt(argument.startsWith(u'+') ? argument.mid(1) : argument);
                    if (!length || *length < 0) {
                        return QCoreApplication::translate("Validation", "<%1> needs a whole number of bytes, for example <%1 16>.").arg(tag);
                    }
                } else if (tag != QLatin1String("t") && tag != QLatin1String("d") && tag != QLatin1String("ds")) {
                    return QCoreApplication::translate("Validation", "Unknown tag <%1>. Use <b 0x...>, <r N>, <rc N>, <rd N>, <dz N>, <t>, <d> or <ds>.").arg(tag);
                }
            }
        }
    }

    // QUrlQuery defaults to PrettyDecoded, which keeps %2F/%2B/%2C encoded and corrupts base64 keys and CIDRs.
    static QString QueryValue(const QUrlQuery& query, const QString& key) {
        return query.queryItemValue(key, QUrl::FullyDecoded);
    }

    static bool ParseAmneziaBool(const QString& value) {
        auto trimmed = value.trimmed().toLower();
        return trimmed == "true" || trimmed == "1" || trimmed == "on" || trimmed == "yes";
    }

    bool Peer::ParseFromLink(const QString& link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        address = url.host(QUrl::FullyEncoded);
        port = url.port(51820);
        if (query.hasQueryItem("public_key")) public_key = QueryValue(query, "public_key");
        else if (query.hasQueryItem("publickey")) public_key = QueryValue(query, "publickey");
        else if (query.hasQueryItem("peer_public_key")) public_key = QueryValue(query, "peer_public_key");

        if (query.hasQueryItem("pre_shared_key")) pre_shared_key = QueryValue(query, "pre_shared_key");
        else if (query.hasQueryItem("preshared_key")) pre_shared_key = QueryValue(query, "preshared_key");
        else if (query.hasQueryItem("presharedkey")) pre_shared_key = QueryValue(query, "presharedkey");
        else if (query.hasQueryItem("psk")) pre_shared_key = QueryValue(query, "psk");

        if (query.hasQueryItem("reserved")) {
            const QString rawReserved = QueryValue(query, "reserved");
            // The core needs all three bytes, so a partial list is dropped rather than kept.
            if (const auto message = Checks::WireguardReserved(rawReserved, &reserved); !message.isEmpty()) {
                AcceptOptional("reserved", message, rawReserved);
            }
        }
        for (const auto key : {"persistent_keepalive_interval", "persistent_keepalive", "keepalive"}) {
            if (!query.hasQueryItem(key)) continue;
            const auto value = QueryValue(query, key).trimmed();
            if (wireguardKeep(key, value, Checks::WireguardKeepalive)) persistent_keepalive = value;
            break;
        }

        return true;
    }

    bool Peer::ParseFromJson(const QJsonObject& object) {
        if (object.isEmpty()) return false;
        if (object.contains("address")) address = object["address"].toString();
        if (object.contains("port")) port = object["port"].toInt();
        if (object.contains("public_key")) public_key = object["public_key"].toString();
        if (object.contains("pre_shared_key")) pre_shared_key = object["pre_shared_key"].toString();
        if (object.contains("reserved")) {
            const auto value = object["reserved"];
            QString text;
            if (value.isString()) {
                // Go decodes a JSON string into []uint8 as base64.
                text = value.toString();
                if (Validation::Base64Key(text, 3).isEmpty()) {
                    reserved.clear();
                    for (const auto byte : QByteArray::fromBase64(text.toLatin1())) reserved << uchar(byte);
                }
            } else {
                reserved = QJsonArray2QListInt(value.toArray());
            }
            if (ParseDiag::Active() && (!reserved.isEmpty() || !text.isEmpty())) {
                QStringList bytes;
                for (const auto byte : reserved) bytes << QString::number(byte);
                if (bytes.isEmpty()) bytes << text;
                if (const auto message = Checks::WireguardReserved(bytes.join(','), &reserved); !message.isEmpty()) {
                    AcceptOptional("reserved", message, bytes.join(','));
                }
            }
        }
        if (object.contains("persistent_keepalive_interval")) {
            auto value = object["persistent_keepalive_interval"];
            const auto text = value.isString() ? value.toString() : Int2String(value.toInt());
            if (wireguardKeep("persistent_keepalive_interval", text, Checks::WireguardKeepalive)) persistent_keepalive = text;
        }
        return true;
    }

    QString Peer::ExportToLink() {
        QUrlQuery query;
        if (!public_key.isEmpty()) query.addQueryItem("public_key", public_key);
        if (!pre_shared_key.isEmpty()) query.addQueryItem("pre_shared_key", pre_shared_key);
        if (!reserved.isEmpty()) {
            QStringList reservedStr;
            for (auto val : reserved) {
                reservedStr.append(QString::number(val));
            }
            query.addQueryItem("reserved", reservedStr.join("-"));
        }
        if (!persistent_keepalive.isEmpty()) query.addQueryItem("persistent_keepalive_interval", persistent_keepalive);
        return query.toString();
    }

    QJsonObject Peer::ExportToJson() {
        QJsonObject object;
        if (!address.isEmpty()) object["address"] = address;
        if (port > 0) object["port"] = port;
        if (!public_key.isEmpty()) object["public_key"] = public_key;
        if (!pre_shared_key.isEmpty()) object["pre_shared_key"] = pre_shared_key;
        if (!reserved.isEmpty()) object["reserved"] = QListInt2QJsonArray(reserved);
        WriteKeepalive(object);
        return object;
    }

    BuildResult Peer::Build() {
        QJsonObject object;
        if (!address.isEmpty()) object["address"] = address;
        if (port > 0) object["port"] = port;
        if (!public_key.isEmpty()) object["public_key"] = public_key;
        if (!pre_shared_key.isEmpty()) object["pre_shared_key"] = pre_shared_key;
        if (!reserved.isEmpty()) object["reserved"] = QListInt2QJsonArray(reserved);
        WriteKeepalive(object);
        object["allowed_ips"] = QListStr2QJsonArray({"0.0.0.0/0", "::/0"});
        return {object, ""};
    }

    // A range must be a string and a plain interval a number; anything else makes the core refuse to start.
    void Peer::WriteKeepalive(QJsonObject& object) const {
        if (persistent_keepalive.isEmpty()) return;
        bool numeric = false;
        int seconds = persistent_keepalive.toInt(&numeric);
        if (numeric) {
            if (seconds > 0) object["persistent_keepalive_interval"] = seconds;
            return;
        }
        static const QRegularExpression rangeRe("^\\d+-\\d+$");
        if (rangeRe.match(persistent_keepalive).hasMatch()) {
            object["persistent_keepalive_interval"] = persistent_keepalive;
        }
    }

    bool wireguard::ParseFromLink(const QString& link) {
        if (link.contains("[Interface]") && link.contains("[Peer]")) {
            auto lines = link.split("\n");
            int peers = 0;
            QStringList allowedIps;
            for (const auto& line : lines) {
                // wg and wg-quick cut every line at '#'; keys and signature tags never contain one.
                const qsizetype comment = line.indexOf(u'#');
                QString trimmed = (comment < 0 ? line : line.left(comment)).trimmed();
                if (trimmed.isEmpty() || trimmed.startsWith(';')) continue;
                if (trimmed == "[Peer]") {
                    ++peers;
                    continue;
                }
                if (trimmed == "[Interface]") continue;
                if (!trimmed.contains("=")) continue;
                auto eqIdx = trimmed.indexOf("=");
                const QString rawKey = trimmed.left(eqIdx).trimmed();
                QString key = rawKey.toLower();
                QString value = trimmed.mid(eqIdx + 1).trimmed();

                if (key == "privatekey") private_key = value;
                // wg-quick allows the key to repeat, e.g. one line per address family.
                else if (key == "address") address << value.replace(" ", "").split(",", Qt::SkipEmptyParts);
                else if (key == "mtu") {
                    if (wireguardKeep(rawKey, value, Validation::Rules::Int(0, kWireguardIntMax))) mtu = value.toInt();
                }
                else if (key == "publickey") peer->public_key = value;
                else if (key == "presharedkey" || key == "preshared_key" || key == "psk") peer->pre_shared_key = value;
                else if (key == "persistentkeepalive" || key == "persistent_keepalive") {
                    if (wireguardKeep(rawKey, value, Checks::WireguardKeepalive)) peer->persistent_keepalive = value;
                }
                else if (key == "endpoint") {
                    int lastColon = value.lastIndexOf(':');
                    if (lastColon != -1) {
                        QString host = value.left(lastColon).trimmed();
                        if (host.startsWith('[') && host.endsWith(']')) {
                            host = host.mid(1, host.length() - 2);
                        }
                        const QString portText = value.mid(lastColon + 1).trimmed();
                        if (const auto message = Validation::Port(portText); !message.isEmpty()) ParseDiag::Error(rawKey, message, value);
                        int port = portText.toInt();
                        peer->address = host;
                        peer->port = port > 0 ? port : 51820;
                        server = peer->address;
                        server_port = peer->port;
                    }
                }
                else if (key == "jc") jc = wireguardAmneziaInt(rawKey, value, kWireguardIntMax, false), enable_amnezia = true;
                else if (key == "jmin") jmin = wireguardAmneziaInt(rawKey, value, kWireguardIntMax, false), enable_amnezia = true;
                else if (key == "jmax") jmax = wireguardAmneziaInt(rawKey, value, kWireguardIntMax, false), enable_amnezia = true;
                else if (key == "s1") s1 = wireguardAmneziaInt(rawKey, value, 65535, true), enable_amnezia = true;
                else if (key == "s2") s2 = wireguardAmneziaInt(rawKey, value, 65535, true), enable_amnezia = true;
                else if (key == "s3") s3 = wireguardAmneziaInt(rawKey, value, 65535, true), enable_amnezia = true;
                else if (key == "s4") s4 = wireguardAmneziaInt(rawKey, value, 65535, true), enable_amnezia = true;
                else if (key == "h1") h1 = value, enable_amnezia = true;
                else if (key == "h2") h2 = value, enable_amnezia = true;
                else if (key == "h3") h3 = value, enable_amnezia = true;
                else if (key == "h4") h4 = value, enable_amnezia = true;
                else if (key == "i1") i1 = value, enable_amnezia = true;
                else if (key == "i2") i2 = value, enable_amnezia = true;
                else if (key == "i3") i3 = value, enable_amnezia = true;
                else if (key == "i4") i4 = value, enable_amnezia = true;
                else if (key == "i5") i5 = value, enable_amnezia = true;
                else if (key == "headerprotectionkey" || key == "header_protection_key") header_protection_key = value, enable_amnezia = true;
                else if (key == "contentpaddingaddition" || key == "content_padding_addition") content_padding_addition = value, enable_amnezia = true;
                else if (key == "rekeyaftertime" || key == "rekey_after_time") rekey_after_time = value, enable_amnezia = true;
                else if (key == "rekeytimeout" || key == "rekey_timeout") rekey_timeout = value, enable_amnezia = true;
                else if (key == "rejectaftertime" || key == "reject_after_time") reject_after_time = value, enable_amnezia = true;
                else if (key == "keepalivetimeout" || key == "keepalive_timeout") keepalive_timeout = value, enable_amnezia = true;
                else if (key == "maxhandshakeattempts" || key == "max_handshake_attempts") max_handshake_attempts = value, enable_amnezia = true;
                else if (key == "randomtrailers" || key == "random_trailers") random_trailers = ParseAmneziaBool(value), enable_amnezia = true;
                else if (key == "disablecookies" || key == "disable_cookies") disable_cookies = ParseAmneziaBool(value), enable_amnezia = true;
                else if (key == "dns") {
                    ParseDiag::Warn(rawKey, QCoreApplication::translate("Validation", "Ignored; Throne resolves names with its own DNS settings."), value);
                }
                else if (key == "allowedips") allowedIps << value.remove(' ').split(',', Qt::SkipEmptyParts);
                else if (key != "listenport" && key != "table" && key != "fwmark" && key != "saveconfig" && key != "preup" &&
                         key != "postup" && key != "predown" && key != "postdown") {
                    ParseDiag::Warn(rawKey, QCoreApplication::translate("Validation", "Unknown setting, ignored."));
                }
            }
            if (peers > 1) {
                ParseDiag::Warn("[Peer]", QCoreApplication::translate("Validation", "Only the last of the %1 [Peer] sections is used.").arg(peers));
            }
            if (!allowedIps.isEmpty() && !allowedIps.contains(QStringLiteral("0.0.0.0/0"))) {
                ParseDiag::Warn("AllowedIPs", QCoreApplication::translate("Validation", "Ignored; Throne sends all traffic of this profile through the tunnel."),
                                allowedIps.join(", "));
            }
            FixAddress();
            wireguardDropBadAddresses(address);
            if (private_key.isEmpty() || peer->public_key.isEmpty()) return false;
            if (server.isEmpty()) {
                ParseDiag::Error("Endpoint", QCoreApplication::translate("Validation", "The [Peer] section has no Endpoint such as vpn.example.com:51820."));
                return false;
            }
            return true;
        }
        
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);

        if (query.hasQueryItem("private_key")) private_key = QueryValue(query, "private_key");
        else if (query.hasQueryItem("privatekey")) private_key = QueryValue(query, "privatekey");
        else if (!url.userName().isEmpty()) private_key = url.userName();

        peer->ParseFromLink(link);
        server = peer->address;
        server_port = peer->port;
        
        if (query.hasQueryItem("local_address")) {
            address = QueryValue(query, "local_address").split("-", Qt::SkipEmptyParts);
        } else if (query.hasQueryItem("address")) {
            address = QueryValue(query, "address").replace(" ", "").split(",", Qt::SkipEmptyParts);
        } else if (query.hasQueryItem("ip")) {
            address = QueryValue(query, "ip").replace(" ", "").split(",", Qt::SkipEmptyParts);
        }
        
        for (const auto &[key, target] : {std::pair{"mtu", &mtu}, std::pair{"workers", &worker_count}}) {
            if (!query.hasQueryItem(key)) continue;
            const auto value = QueryValue(query, key).trimmed();
            if (wireguardKeep(key, value, Validation::Rules::Int(0, kWireguardIntMax))) *target = value.toInt();
        }
        if (query.hasQueryItem("use_system_interface")) system = QueryValue(query, "use_system_interface") == "true";
        if (query.hasQueryItem("udp_timeout")) {
            const auto value = QueryValue(query, "udp_timeout").trimmed();
            if (wireguardKeep("udp_timeout", value, Validation::Rules::Duration())) udp_timeout = value;
        }

        if (QueryValue(query, "enable_amnezia") == "true") enable_amnezia = true;
        for (const auto &[key, target, max, mustMatchServer] :
             {std::tuple{"jc", &jc, kWireguardIntMax, false}, std::tuple{"jmin", &jmin, kWireguardIntMax, false},
              std::tuple{"jmax", &jmax, kWireguardIntMax, false}, std::tuple{"s1", &s1, qint64(65535), true},
              std::tuple{"s2", &s2, qint64(65535), true}, std::tuple{"s3", &s3, qint64(65535), true},
              std::tuple{"s4", &s4, qint64(65535), true}}) {
            if (!query.hasQueryItem(key)) continue;
            *target = wireguardAmneziaInt(key, QueryValue(query, key), max, mustMatchServer);
            enable_amnezia = true;
        }
        if (query.hasQueryItem("h1")) h1 = QueryValue(query, "h1").trimmed(), enable_amnezia = true;
        if (query.hasQueryItem("h2")) h2 = QueryValue(query, "h2").trimmed(), enable_amnezia = true;
        if (query.hasQueryItem("h3")) h3 = QueryValue(query, "h3").trimmed(), enable_amnezia = true;
        if (query.hasQueryItem("h4")) h4 = QueryValue(query, "h4").trimmed(), enable_amnezia = true;
        if (query.hasQueryItem("i1")) i1 = QueryValue(query, "i1"), enable_amnezia = true;
        if (query.hasQueryItem("i2")) i2 = QueryValue(query, "i2"), enable_amnezia = true;
        if (query.hasQueryItem("i3")) i3 = QueryValue(query, "i3"), enable_amnezia = true;
        if (query.hasQueryItem("i4")) i4 = QueryValue(query, "i4"), enable_amnezia = true;
        if (query.hasQueryItem("i5")) i5 = QueryValue(query, "i5"), enable_amnezia = true;
        if (query.hasQueryItem("header_protection_key")) header_protection_key = QueryValue(query, "header_protection_key"), enable_amnezia = true;
        else if (query.hasQueryItem("headerprotectionkey")) header_protection_key = QueryValue(query, "headerprotectionkey"), enable_amnezia = true;
        if (query.hasQueryItem("content_padding_addition")) content_padding_addition = QueryValue(query, "content_padding_addition"), enable_amnezia = true;
        else if (query.hasQueryItem("contentpaddingaddition")) content_padding_addition = QueryValue(query, "contentpaddingaddition"), enable_amnezia = true;
        if (query.hasQueryItem("rekey_after_time")) rekey_after_time = QueryValue(query, "rekey_after_time"), enable_amnezia = true;
        if (query.hasQueryItem("rekey_timeout")) rekey_timeout = QueryValue(query, "rekey_timeout"), enable_amnezia = true;
        if (query.hasQueryItem("reject_after_time")) reject_after_time = QueryValue(query, "reject_after_time"), enable_amnezia = true;
        if (query.hasQueryItem("keepalive_timeout")) keepalive_timeout = QueryValue(query, "keepalive_timeout"), enable_amnezia = true;
        if (query.hasQueryItem("max_handshake_attempts")) max_handshake_attempts = QueryValue(query, "max_handshake_attempts"), enable_amnezia = true;
        if (query.hasQueryItem("random_trailers")) random_trailers = ParseAmneziaBool(QueryValue(query, "random_trailers")), enable_amnezia = true;
        if (query.hasQueryItem("disable_cookies")) disable_cookies = ParseAmneziaBool(QueryValue(query, "disable_cookies")), enable_amnezia = true;

        FixAddress();
        wireguardDropBadAddresses(address);

        return !(private_key.isEmpty() || peer->public_key.isEmpty() || server.isEmpty());
    }

    bool wireguard::ParseFromJson(const QJsonObject& object) {
        if (object.isEmpty() || object["type"].toString() != "wireguard") return false;
        outbound::ParseFromJson(object);
        if (object.contains("private_key")) private_key = object["private_key"].toString();
        if (object.contains("peers") && object["peers"].isArray() && !object["peers"].toArray().empty()) {
            const auto peers = object["peers"].toArray();
            peer->ParseFromJson(peers[0].toObject());
            if (peers.size() > 1 && ParseDiag::Active()) {
                ParseDiag::Warn("peers", QCoreApplication::translate("Validation", "Only the first of the %1 peers is used.").arg(peers.size()));
            }
        }
        // Listable: the core accepts a bare string as well as an array.
        if (object.contains("address")) address = JsonStringList(object["address"], "address");
        if (object.contains("mtu")) mtu = object["mtu"].toInt();
        if (object.contains("system")) system = object["system"].toBool();
        if (object.contains("workers")) worker_count = object["workers"].toInt();
        else if (object.contains("worker_count")) worker_count = object["worker_count"].toInt();
        if (object.contains("udp_timeout")) {
            const auto value = object["udp_timeout"].toString().trimmed();
            if (wireguardKeep("udp_timeout", value, Validation::Rules::Duration())) udp_timeout = value;
        }
        if (object.contains("amnezia_wg")) AmneziaFromJson(object["amnezia_wg"].toObject());
        FixAddress();
        wireguardDropBadAddresses(address);
        return true;
    }

    QString wireguard::ExportToLink() {
        QUrl url;
        QUrlQuery query;
        url.setScheme("wg");
        url.setHost(peer->address);
        url.setPort(peer->port);
        if (!name.isEmpty()) url.setFragment(name);

        if (!private_key.isEmpty()) query.addQueryItem("private_key", private_key);
        
        if (!address.isEmpty()) query.addQueryItem("local_address", address.join("-"));
        if (mtu > 0 && mtu != 1420) query.addQueryItem("mtu", QString::number(mtu));
        if (system) query.addQueryItem("use_system_interface", "true");
        if (worker_count > 0) query.addQueryItem("workers", QString::number(worker_count));
        if (!udp_timeout.isEmpty()) query.addQueryItem("udp_timeout", udp_timeout);

        if (enable_amnezia) {
            query.addQueryItem("enable_amnezia", "true");
            if (jc > 0) query.addQueryItem("jc", QString::number(jc));
            if (jmin > 0) query.addQueryItem("jmin", QString::number(jmin));
            if (jmax > 0) query.addQueryItem("jmax", QString::number(jmax));
            if (s1 > 0) query.addQueryItem("s1", QString::number(s1));
            if (s2 > 0) query.addQueryItem("s2", QString::number(s2));
            if (s3 > 0) query.addQueryItem("s3", QString::number(s3));
            if (s4 > 0) query.addQueryItem("s4", QString::number(s4));
            if (!h1.isEmpty()) query.addQueryItem("h1", h1);
            if (!h2.isEmpty()) query.addQueryItem("h2", h2);
            if (!h3.isEmpty()) query.addQueryItem("h3", h3);
            if (!h4.isEmpty()) query.addQueryItem("h4", h4);
            if (!i1.isEmpty()) query.addQueryItem("i1", i1);
            if (!i2.isEmpty()) query.addQueryItem("i2", i2);
            if (!i3.isEmpty()) query.addQueryItem("i3", i3);
            if (!i4.isEmpty()) query.addQueryItem("i4", i4);
            if (!i5.isEmpty()) query.addQueryItem("i5", i5);
            if (!header_protection_key.isEmpty()) query.addQueryItem("header_protection_key", header_protection_key);
            if (!content_padding_addition.isEmpty()) query.addQueryItem("content_padding_addition", content_padding_addition);
            if (!rekey_after_time.isEmpty()) query.addQueryItem("rekey_after_time", rekey_after_time);
            if (!rekey_timeout.isEmpty()) query.addQueryItem("rekey_timeout", rekey_timeout);
            if (!reject_after_time.isEmpty()) query.addQueryItem("reject_after_time", reject_after_time);
            if (!keepalive_timeout.isEmpty()) query.addQueryItem("keepalive_timeout", keepalive_timeout);
            if (!max_handshake_attempts.isEmpty()) query.addQueryItem("max_handshake_attempts", max_handshake_attempts);
            if (random_trailers) query.addQueryItem("random_trailers", "true");
            if (disable_cookies) query.addQueryItem("disable_cookies", "true");
        }

        mergeUrlQuery(query, outbound::ExportToLink());
        mergeUrlQuery(query, peer->ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject wireguard::ExportToJson() {
        QJsonObject object;
        object["type"] = "wireguard";
        if (!name.isEmpty()) object["tag"] = name;
        mergeJsonObjects(object, dialFields->ExportToJson());
        if (!private_key.isEmpty()) object["private_key"] = private_key;
        if (!address.isEmpty()) object["address"] = QListStr2QJsonArray(address);
        if (mtu > 0) object["mtu"] = mtu;
        if (system) object["system"] = system;
        if (worker_count > 0) object["worker_count"] = worker_count;
        if (!udp_timeout.isEmpty()) object["udp_timeout"] = udp_timeout;
        
        auto amneziaObj = AmneziaToJson();
        if (!amneziaObj.isEmpty()) object["amnezia_wg"] = amneziaObj;

        auto peerObj = peer->ExportToJson();
        if (!peerObj.isEmpty()) {
            object["peers"] = QJsonArray({peerObj});
        }
        
        return object;
    }

    BuildResult wireguard::Build() {
        QJsonObject object;
        object["type"] = "wireguard";
        if (!name.isEmpty()) object["tag"] = name;
        mergeJsonObjects(object, dialFields->Build().object);
        if (!private_key.isEmpty()) object["private_key"] = private_key;
        if (!address.isEmpty()) object["address"] = QListStr2QJsonArray(address);
        if (mtu > 0) object["mtu"] = mtu;
        if (system) object["system"] = system;
        if (worker_count > 0) object["workers"] = worker_count;
        if (!udp_timeout.isEmpty()) object["udp_timeout"] = udp_timeout;

        auto amneziaObj = AmneziaToJson();
        if (!amneziaObj.isEmpty()) object["amnezia_wg"] = amneziaObj;

        auto peerObj = peer->Build().object;
        if (!peerObj.isEmpty()) {
            object["peers"] = QJsonArray({peerObj});
        }
        return {object, ""};
    }

    void wireguard::SetAddress(QString newAddr) {
        peer->address = newAddr;
    }

    QString wireguard::GetAddress() {
        return peer->address;
    }

    void wireguard::SetPort(int newPort) {
        peer->port = newPort;
    }

    QString wireguard::GetPort() {
        return QString::number(peer->port);
    }

    QString wireguard::DisplayAddress() {
        return ::DisplayAddress(peer->address, peer->port);
    }

    QString wireguard::DisplayType() {
        return "WireGuard";
    }

    SecurityInfo wireguard::GetSecurity() {
        return {QObject::tr("Encrypted"), enable_amnezia ? "AmneziaWG" : QString(), SecurityLevel::Secure};
    }

    bool wireguard::IsEndpoint() {
        return true;
    }

    // Key lengths and every AmneziaWG value are only checked when the endpoint first dials, so this is the early signal.
    void wireguard::Validate(Validation::Issues& issues) {
        outbound::Validate(issues);
        wireguardReport(issues, "private_key", Validation::Base64Key(private_key, 32));
        wireguardReport(issues, "public_key", Validation::Base64Key(peer->public_key, 32));
        wireguardReport(issues, "pre_shared_key", Validation::Base64Key(peer->pre_shared_key, 32, false, true, Validation::Empty::Allowed));
        if (!peer->reserved.isEmpty()) {
            wireguardReport(issues, "reserved", Checks::WireguardReserved(QListInt2QListString(peer->reserved).join(',')));
        }
        wireguardReport(issues, "persistent_keepalive_interval", Checks::WireguardKeepalive(peer->persistent_keepalive), Validation::Severity::Error,
                        peer->persistent_keepalive);
        if (address.isEmpty()) {
            wireguardReport(issues, "address", QCoreApplication::translate("Validation", "At least one local address is required, for example 10.0.0.2/32."));
        } else {
            wireguardReport(issues, "address", Validation::EachItem(address.join(','), Validation::Split::Comma, Validation::Rules::Cidr()));
        }
        wireguardReport(issues, "udp_timeout", Validation::Duration(udp_timeout));
        if (enable_amnezia) issues << AmneziaIssues();
    }

    Validation::Issues wireguard::AmneziaIssues() const {
        using Validation::Severity;
        Validation::Issues issues;
        for (const auto &[field, value] : {std::pair{"s1", s1}, std::pair{"s2", s2}, std::pair{"s3", s3}, std::pair{"s4", s4}}) {
            // Only positive values are emitted.
            if (value > 65535) wireguardReport(issues, field, Validation::Int(QString::number(value), 0, 65535));
        }
        const QStringList headers = {h1, h2, h3, h4};
        for (qsizetype i = 0; i < headers.size(); ++i) {
            wireguardReport(issues, QStringLiteral("h%1").arg(i + 1), Checks::AmneziaRange(headers[i]), Severity::Error, headers[i]);
        }
        wireguardReport(issues, "h1", Checks::AmneziaHeaders(headers));
        const QStringList signatures = {i1, i2, i3, i4, i5};
        for (qsizetype i = 0; i < signatures.size(); ++i) {
            wireguardReport(issues, QStringLiteral("i%1").arg(i + 1), Checks::AmneziaSignature(signatures[i]));
        }
        wireguardReport(issues, "header_protection_key", Validation::Base64Key(header_protection_key, 32, false, true, Validation::Empty::Allowed));
        wireguardReport(issues, "header_protection_key", Checks::AmneziaHeaderProtection(header_protection_key, {s1, s2, s3, s4}));
        for (const auto &[field, value] : {std::pair{"content_padding_addition", &content_padding_addition}, std::pair{"rekey_after_time", &rekey_after_time},
                                           std::pair{"rekey_timeout", &rekey_timeout}, std::pair{"reject_after_time", &reject_after_time},
                                           std::pair{"keepalive_timeout", &keepalive_timeout}, std::pair{"max_handshake_attempts", &max_handshake_attempts}}) {
            wireguardReport(issues, field, Checks::AmneziaRange(*value), Severity::Error, *value);
        }
        wireguardReport(issues, "jmin", Checks::AmneziaJunkSizes(jmin, jmax), Severity::Warning);
        return issues;
    }

    QJsonObject wireguard::AmneziaToJson() {
        QJsonObject object;
        if (!enable_amnezia) return object;
        if (jc > 0) object["jc"] = jc;
        if (jmin > 0) object["jmin"] = jmin;
        if (jmax > 0) object["jmax"] = jmax;
        if (s1 > 0) object["s1"] = s1;
        if (s2 > 0) object["s2"] = s2;
        if (s3 > 0) object["s3"] = s3;
        if (s4 > 0) object["s4"] = s4;
        if (!h1.isEmpty()) object["h1"] = h1;
        if (!h2.isEmpty()) object["h2"] = h2;
        if (!h3.isEmpty()) object["h3"] = h3;
        if (!h4.isEmpty()) object["h4"] = h4;
        if (!i1.isEmpty()) object["i1"] = i1;
        if (!i2.isEmpty()) object["i2"] = i2;
        if (!i3.isEmpty()) object["i3"] = i3;
        if (!i4.isEmpty()) object["i4"] = i4;
        if (!i5.isEmpty()) object["i5"] = i5;
        if (!header_protection_key.isEmpty()) object["header_protection_key"] = header_protection_key;
        if (!content_padding_addition.isEmpty()) object["content_padding_addition"] = content_padding_addition;
        if (!rekey_after_time.isEmpty()) object["rekey_after_time"] = rekey_after_time;
        if (!rekey_timeout.isEmpty()) object["rekey_timeout"] = rekey_timeout;
        if (!reject_after_time.isEmpty()) object["reject_after_time"] = reject_after_time;
        if (!keepalive_timeout.isEmpty()) object["keepalive_timeout"] = keepalive_timeout;
        if (!max_handshake_attempts.isEmpty()) object["max_handshake_attempts"] = max_handshake_attempts;
        if (random_trailers) object["random_trailers"] = true;
        if (disable_cookies) object["disable_cookies"] = true;
        return object;
    }

    void wireguard::AmneziaFromJson(const QJsonObject& object) {
        if (object.isEmpty()) return;
        enable_amnezia = true;
        if (object.contains("jc")) jc = object["jc"].toInt();
        if (object.contains("jmin")) jmin = object["jmin"].toInt();
        if (object.contains("jmax")) jmax = object["jmax"].toInt();
        if (object.contains("s1")) s1 = object["s1"].toInt();
        if (object.contains("s2")) s2 = object["s2"].toInt();
        if (object.contains("s3")) s3 = object["s3"].toInt();
        if (object.contains("s4")) s4 = object["s4"].toInt();
        if (object.contains("h1")) h1 = object["h1"].toString().trimmed();
        if (object.contains("h2")) h2 = object["h2"].toString().trimmed();
        if (object.contains("h3")) h3 = object["h3"].toString().trimmed();
        if (object.contains("h4")) h4 = object["h4"].toString().trimmed();
        if (object.contains("i1")) i1 = object["i1"].toString();
        if (object.contains("i2")) i2 = object["i2"].toString();
        if (object.contains("i3")) i3 = object["i3"].toString();
        if (object.contains("i4")) i4 = object["i4"].toString();
        if (object.contains("i5")) i5 = object["i5"].toString();
        if (object.contains("header_protection_key")) header_protection_key = object["header_protection_key"].toString();
        if (object.contains("content_padding_addition")) content_padding_addition = AmneziaRangeFromJson(object["content_padding_addition"]);
        if (object.contains("rekey_after_time")) rekey_after_time = AmneziaRangeFromJson(object["rekey_after_time"]);
        if (object.contains("rekey_timeout")) rekey_timeout = AmneziaRangeFromJson(object["rekey_timeout"]);
        if (object.contains("reject_after_time")) reject_after_time = AmneziaRangeFromJson(object["reject_after_time"]);
        if (object.contains("keepalive_timeout")) keepalive_timeout = AmneziaRangeFromJson(object["keepalive_timeout"]);
        if (object.contains("max_handshake_attempts")) max_handshake_attempts = AmneziaRangeFromJson(object["max_handshake_attempts"]);
        if (object.contains("random_trailers")) random_trailers = object["random_trailers"].toBool();
        if (object.contains("disable_cookies")) disable_cookies = object["disable_cookies"].toBool();
    }

    // Ranges are written as strings, but sing-box also accepts a bare number.
    QString wireguard::AmneziaRangeFromJson(const QJsonValue& value) {
        return value.isString() ? value.toString() : Int2String(value.toInt());
    }

    void wireguard::FixAddress() {
        QStringList newAddresses;
        for (const auto& addr : address) {
            auto trimmed = addr.trimmed();
            if (trimmed.isEmpty()) continue;
            if (trimmed.contains("/")) {
                newAddresses.append(trimmed);
                continue;
            }
            if (trimmed.contains(":")) trimmed += "/128";
            else trimmed += "/32";
            newAddresses.append(trimmed);
        }
        address = newAddresses;
    }
}
