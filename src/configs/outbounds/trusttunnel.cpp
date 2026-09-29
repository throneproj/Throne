#include "include/configs/outbounds/trusttunnel.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // RFC 9000 variable-length integer, as used by the TrustTunnel deep link TLV payload.
        bool readVarInt(const QByteArray& data, int& pos, quint64& value)
        {
            if (pos >= data.size()) return false;
            const auto first = static_cast<quint8>(data[pos]);
            const int length = 1 << (first >> 6);
            if (pos + length > data.size()) return false;
            value = first & 0x3f;
            for (int i = 1; i < length; i++) value = (value << 8) | static_cast<quint8>(data[pos + i]);
            pos += length;
            return true;
        }

        // Splits concatenated DER certificates and returns them as PEM lines.
        QStringList derChainToPem(const QByteArray& chain)
        {
            QStringList lines;
            int pos = 0;
            while (pos + 2 <= chain.size() && static_cast<quint8>(chain[pos]) == 0x30) {
                int headerLength = 2;
                quint64 length = static_cast<quint8>(chain[pos + 1]);
                if (length & 0x80) {
                    const int lengthBytes = static_cast<int>(length & 0x7f);
                    if (lengthBytes == 0 || lengthBytes > 4 || pos + 2 + lengthBytes > chain.size()) return {};
                    length = 0;
                    for (int i = 0; i < lengthBytes; i++) length = (length << 8) | static_cast<quint8>(chain[pos + 2 + i]);
                    headerLength += lengthBytes;
                }
                const auto total = static_cast<qint64>(headerLength) + static_cast<qint64>(length);
                if (pos + total > chain.size()) return {};
                lines << "-----BEGIN CERTIFICATE-----";
                const auto encoded = chain.mid(pos, static_cast<int>(total)).toBase64();
                for (int i = 0; i < encoded.size(); i += 64) lines << QString::fromLatin1(encoded.mid(i, 64));
                lines << "-----END CERTIFICATE-----";
                pos += static_cast<int>(total);
            }
            return pos == chain.size() ? lines : QStringList{};
        }
    }

    // Official deep link: tt://?<base64url TLV payload>, see DEEP_LINK.md in TrustTunnel/TrustTunnel.
    bool trusttunnel::ParseFromDeepLink(const QString& payload)
    {
        const auto data = QByteArray::fromBase64(payload.toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        if (data.isEmpty()) return false;
        bool haveAddress = false;
        int pos = 0;
        while (pos < data.size()) {
            quint64 tag, length;
            if (!readVarInt(data, pos, tag) || !readVarInt(data, pos, length)) return false;
            if (length > static_cast<quint64>(data.size() - pos)) return false;
            const auto value = data.mid(pos, static_cast<int>(length));
            pos += static_cast<int>(length);
            const auto text = QString::fromUtf8(value);
            const bool flag = !value.isEmpty() && value[0] != 0;
            switch (tag) {
                case 0x00: {
                    int at = 0;
                    quint64 version = 0;
                    if (!readVarInt(value, at, version) || version > 1) return false;
                    break;
                }
                case 0x01: tls->server_name = text; break;
                case 0x02: {
                    // Only the first address is kept: a profile has one server.
                    if (haveAddress) break;
                    const QUrl address("tt://" + text);
                    if (!address.isValid() || address.host().isEmpty()) return false;
                    server = address.host(QUrl::FullyEncoded);
                    server_port = address.port(443);
                    haveAddress = true;
                    break;
                }
                case 0x03: custom_sni = text; break;
                case 0x05: username = text; break;
                case 0x06: password = text; break;
                case 0x07: tls->insecure = flag; break;
                case 0x08: {
                    tls->certificate = derChainToPem(value);
                    if (tls->certificate.isEmpty()) return false;
                    break;
                }
                case 0x09: quic = !value.isEmpty() && value[0] == 2; break;
                case 0x0A: {
                    // anti_dpi slows the handshake writes down so the ClientHello spans several segments.
                    if (flag) tls->saveFragmentState(1);
                    break;
                }
                case 0x0B: client_random = text; break;
                case 0x0C: name = text; break;
                default: break; // has_ipv6, dns_upstreams and unknown tags
            }
        }
        tls->enabled = true;
        return haveAddress && !tls->server_name.isEmpty() && !username.isEmpty() && !password.isEmpty();
    }

    bool trusttunnel::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        if (url.host().isEmpty() && url.hasQuery()) return ParseFromDeepLink(url.query(QUrl::FullyEncoded));
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        username = url.userName();
        password = url.password();
        
        if (query.hasQueryItem("health_check")) health_check = query.queryItemValue("health_check") == "true";
        if (query.hasQueryItem("congestion_control")) {
            quic = true;
            congestion_control = query.queryItemValue("congestion_control");
        }
        if (query.hasQueryItem("custom_sni")) custom_sni = query.queryItemValue("custom_sni");
        if (query.hasQueryItem("client_random")) client_random = query.queryItemValue("client_random");
        
        tls->ParseFromLink(link);
        tls->enabled = true; // TrustTunnel always uses tls
        
        if (server_port == 0) server_port = 443;

        return !(username.isEmpty() || password.isEmpty() || server.isEmpty());
    }

    bool trusttunnel::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "trusttunnel") return false;
        outbound::ParseFromJson(object);
        if (object.contains("username")) username = object["username"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("health_check")) health_check = object["health_check"].toBool();
        if (object.contains("quic")) quic = object["quic"].toBool();
        if (object.contains("quic_congestion_control")) congestion_control = object["quic_congestion_control"].toString();
        if (object.contains("custom_sni")) custom_sni = object["custom_sni"].toString();
        if (object.contains("client_random")) client_random = object["client_random"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        return true;
    }

    QString trusttunnel::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("tt");
        url.setUserName(username);
        url.setPassword(password);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        if (health_check) query.addQueryItem("health_check", "true");
        // The link carries QUIC only through congestion_control; sing-trusttunnel treats "" and "bbr" the same.
        if (quic) query.addQueryItem("congestion_control", congestion_control.isEmpty() ? "bbr" : congestion_control);
        if (!custom_sni.isEmpty()) query.addQueryItem("custom_sni", custom_sni);
        if (!client_random.isEmpty()) query.addQueryItem("client_random", client_random);
        
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject trusttunnel::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "trusttunnel";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (health_check) object["health_check"] = health_check;
        if (quic) {
            object["quic"] = quic;
            if (!congestion_control.isEmpty()) object["quic_congestion_control"] = congestion_control;
        }
        if (!custom_sni.isEmpty()) object["custom_sni"] = toAceHost(custom_sni);
        if (!client_random.isEmpty()) object["client_random"] = client_random;
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        return object;
    }

    BuildResult trusttunnel::Build()
    {
        QJsonObject object;
        object["type"] = "trusttunnel";
        mergeJsonObjects(object, outbound::Build().object);
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (health_check) object["health_check"] = health_check;
        if (quic) {
            object["quic"] = quic;
            if (!congestion_control.isEmpty()) object["quic_congestion_control"] = congestion_control;
        }
        if (!custom_sni.isEmpty()) object["custom_sni"] = toAceHost(custom_sni);
        if (!client_random.isEmpty()) object["client_random"] = client_random;
        if (tls->enabled) {
            auto tlsObject = tls->Build().object;
            // QUIC dials through qtls, which needs a std TLS config: uTLS and Reality fail there on every connection.
            if (quic) {
                tlsObject.remove("utls");
                tlsObject.remove("reality");
            }
            // The official client mimics Chrome by default; QUIC dials through qtls, where uTLS is unavailable.
            if (!quic && !tlsObject.contains("utls")) {
                tlsObject["utls"] = QJsonObject{{"enabled", true}, {"fingerprint", "chrome"}};
            }
            object["tls"] = tlsObject;
        }
        return {object, ""};
    }

    QString trusttunnel::DisplayType()
    {
        return "TrustTunnel";
    }

    SecurityInfo trusttunnel::GetSecurity()
    {
        return SecurityFromTLS(quic ? "QUIC" : QString());
    }
}
