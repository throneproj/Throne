#include "include/dataStore/ProxyEntity.hpp"
#include "include/configs/proxy/includes.h"

#include <QUrlQuery>

namespace Configs {

#define DECODE_V2RAY_N_1                                                                                                        \
    QString linkN = DecodeB64IfValid(SubStrBefore(SubStrAfter(link, "://"), "#"), QByteArray::Base64Option::Base64UrlEncoding); \
    if (linkN.isEmpty()) return false;                                                                                          \
    auto hasRemarks = link.contains("#");                                                                                       \
    if (hasRemarks) linkN += "#" + SubStrAfter(link, "#");                                                                      \
    auto url = QUrl("https://" + linkN);

    bool SocksHttpBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);

        if (link.startsWith("socks4")) socks_http_type = type_Socks4;
        if (link.startsWith("http")) socks_http_type = type_HTTP;
        name = url.fragment(QUrl::FullyDecoded);
        serverAddress = url.host();
        serverPort = url.port();
        username = url.userName();
        password = url.password();
        if (serverPort == -1) serverPort = socks_http_type == type_HTTP ? 443 : 1080;

        // v2rayN fmt
        if (password.isEmpty() && !username.isEmpty()) {
            QString n = DecodeB64IfValid(username);
            if (!n.isEmpty()) {
                username = SubStrBefore(n, ":");
                password = SubStrAfter(n, ":");
            }
        }

        stream->security = GetQueryValue(query, "security", "");
        stream->sni = GetQueryValue(query, "sni");
        if (link.startsWith("https")) stream->security = "tls";

        return !serverAddress.isEmpty();
    }

    bool AnyTlsBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);

        name = url.fragment(QUrl::FullyDecoded);
        serverAddress = url.host();
        serverPort = url.port();
        password = url.userName();
        if (serverPort == -1) serverPort = 443;

        // security

        stream->security = GetQueryValue(query, "security", "").replace("none", "");
        auto sni1 = GetQueryValue(query, "sni");
        auto sni2 = GetQueryValue(query, "peer");
        if (!sni1.isEmpty()) stream->sni = sni1;
        if (!sni2.isEmpty()) stream->sni = sni2;
        stream->alpn = GetQueryValue(query, "alpn");
        stream->allow_insecure = QStringList{"1", "true"}.contains(query.queryItemValue("insecure"));
        stream->reality_pbk = GetQueryValue(query, "pbk", "");
        stream->reality_sid = GetQueryValue(query, "sid", "");
        stream->utlsFingerprint = GetQueryValue(query, "fp", "");
        if (query.queryItemValue("fragment") == "1") stream->enable_tls_fragment = true;
        stream->tls_fragment_fallback_delay = query.queryItemValue("fragment_fallback_delay");
        if (query.queryItemValue("record_fragment") == "1") stream->enable_tls_record_fragment = true;
        if (stream->utlsFingerprint.isEmpty()) {
            stream->utlsFingerprint = dataStore->utlsFingerprint;
        }
        if (stream->security.isEmpty()) {
            if (!sni1.isEmpty() || !sni2.isEmpty()) stream->security = "tls";
            if (!stream->reality_pbk.isEmpty()) stream->security = "reality";
        }

        return !(password.isEmpty() || serverAddress.isEmpty());
    }

    bool TrojanVLESSBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);

        name = url.fragment(QUrl::FullyDecoded);
        serverAddress = url.host();
        serverPort = url.port();
        password = url.userName();
        if (serverPort == -1) serverPort = 443;

        // security

        auto type =  GetQueryValue(query, "type", "tcp");
        if (type == "h2") {
            type = "http";
        }
        stream->network = type;

        stream->security = GetQueryValue(query, "security", "").replace("none", "");
        auto sni1 = GetQueryValue(query, "sni");
        auto sni2 = GetQueryValue(query, "peer");
        if (!sni1.isEmpty()) stream->sni = sni1;
        if (!sni2.isEmpty()) stream->sni = sni2;
        stream->alpn = GetQueryValue(query, "alpn");
        if (!query.queryItemValue("allowInsecure").isEmpty()) stream->allow_insecure = true;
        stream->reality_pbk = GetQueryValue(query, "pbk", "");
        stream->reality_sid = GetQueryValue(query, "sid", "");
        stream->utlsFingerprint = GetQueryValue(query, "fp", "");
        if (query.queryItemValue("fragment") == "1") stream->enable_tls_fragment = true;
        stream->tls_fragment_fallback_delay = query.queryItemValue("fragment_fallback_delay");
        if (query.queryItemValue("record_fragment") == "1") stream->enable_tls_record_fragment = true;
        if (stream->utlsFingerprint.isEmpty()) {
            stream->utlsFingerprint = dataStore->utlsFingerprint;
        }
        if (stream->security.isEmpty()) {
            if (!sni1.isEmpty() || !sni2.isEmpty()) stream->security = "tls";
            if (!stream->reality_pbk.isEmpty()) stream->security = "reality";
        }

        // type
        if (stream->network == "ws") {
            stream->path = GetQueryValue(query, "path", "");
            stream->host = GetQueryValue(query, "host", "");
        } else if (stream->network == "http") {
            stream->path = GetQueryValue(query, "path", "");
            stream->host = GetQueryValue(query, "host", "").replace("|", ",");
            stream->method = GetQueryValue(query, "method", "");
        } else if (stream->network == "httpupgrade") {
            stream->path = GetQueryValue(query, "path", "");
            stream->host = GetQueryValue(query, "host", "");
        } else if (stream->network == "grpc") {
            stream->path = GetQueryValue(query, "serviceName", "");
        } else if (stream->network == "tcp") {
            if (GetQueryValue(query, "headerType") == "http") {
                stream->header_type = "http";
                stream->host = GetQueryValue(query, "host", "");
                stream->path = GetQueryValue(query, "path", "");
            }
        }

        // mux
        auto mux_str = GetQueryValue(query, "mux", "");
        if (mux_str == "true") {
            mux_state = 1;
        } else if (mux_str == "false") {
            mux_state = 2;
        }

        // protocol
        if (proxy_type == proxy_VLESS) {
            flow = GetQueryValue(query, "flow", "");
            stream->packet_encoding = GetQueryValue(query, "packetEncoding", "xudp");
        }

        return !(password.isEmpty() || serverAddress.isEmpty());
    }

    bool ShadowSocksBean::TryParseLink(const QString &link) {
        if (SubStrBefore(link, "#").contains("@")) {
            // SS
            auto url = QUrl(link);
            if (!url.isValid()) return false;

            name = url.fragment(QUrl::FullyDecoded);
            serverAddress = url.host();
            serverPort = url.port();

            if (url.password().isEmpty()) {
                // traditional format
                auto method_password = DecodeB64IfValid(url.userName(), QByteArray::Base64Option::Base64UrlEncoding);
                if (method_password.isEmpty()) return false;
                method = SubStrBefore(method_password, ":");
                password = SubStrAfter(method_password, ":");
            } else {
                // 2022 format
                method = url.userName();
                password = url.password();
            }

            auto query = GetQuery(url);
            plugin = query.queryItemValue("plugin").replace("simple-obfs;", "obfs-local;");
            auto mux_str = GetQueryValue(query, "mux", "");
            if (mux_str == "true") {
                mux_state = 1;
            } else if (mux_str == "false") {
                mux_state = 2;
            }

        } else {
            // v2rayN
            DECODE_V2RAY_N_1

            if (hasRemarks) name = url.fragment(QUrl::FullyDecoded);
            serverAddress = url.host();
            serverPort = url.port();
            method = url.userName();
            password = url.password();
        }
        return !(serverAddress.isEmpty() || method.isEmpty() || password.isEmpty());
    }

    bool VMessBean::TryParseLink(const QString &link) {
        // V2RayN Format
        auto linkN = DecodeB64IfValid(SubStrAfter(link, "vmess://"));
        if (!linkN.isEmpty()) {
            auto objN = QString2QJsonObject(linkN);
            if (objN.isEmpty()) return false;
            // REQUIRED
            uuid = objN["id"].toString();
            serverAddress = objN["add"].toString();
            serverPort = objN["port"].toVariant().toInt();
            // OPTIONAL
            name = objN["ps"].toString();
            aid = objN["aid"].toVariant().toInt();
            stream->host = objN["host"].toString();
            stream->path = objN["path"].toString();
            stream->sni = objN["sni"].toString();
            stream->header_type = objN["type"].toString();
            auto net = objN["net"].toString();
            if (!net.isEmpty()) {
                if (net == "h2") {
                    net = "http";
                }
                stream->network = net;
            }
            auto scy = objN["scy"].toString();
            if (!scy.isEmpty()) security = scy;
            // TLS (XTLS?)
            stream->security = objN["tls"].toString();
            // TODO quic & kcp
            return true;
        } else {
            // https://github.com/XTLS/Xray-core/discussions/716
            auto url = QUrl(link);
            if (!url.isValid()) return false;
            auto query = GetQuery(url);

            name = url.fragment(QUrl::FullyDecoded);
            serverAddress = url.host();
            serverPort = url.port();
            uuid = url.userName();
            if (serverPort == -1) serverPort = 443;

            aid = 0; // “此分享标准仅针对 VMess AEAD 和 VLESS。”
            security = GetQueryValue(query, "encryption", "auto");

            // security
            auto type = GetQueryValue(query, "type", "tcp");
            if (type == "h2") {
                type = "http";
            }
            stream->network = type;
            stream->security = GetQueryValue(query, "security", "tls").replace("reality", "tls");
            auto sni1 = GetQueryValue(query, "sni");
            auto sni2 = GetQueryValue(query, "peer");
            if (!sni1.isEmpty()) stream->sni = sni1;
            if (!sni2.isEmpty()) stream->sni = sni2;
            if (!query.queryItemValue("allowInsecure").isEmpty()) stream->allow_insecure = true;
            stream->reality_pbk = GetQueryValue(query, "pbk", "");
            stream->reality_sid = GetQueryValue(query, "sid", "");
            stream->utlsFingerprint = GetQueryValue(query, "fp", "");
            if (stream->utlsFingerprint.isEmpty()) {
                stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
            }
            if (query.queryItemValue("fragment") == "1") stream->enable_tls_fragment = true;
            stream->tls_fragment_fallback_delay = query.queryItemValue("fragment_fallback_delay");
            if (query.queryItemValue("record_fragment") == "1") stream->enable_tls_record_fragment = true;

            // mux
            auto mux_str = GetQueryValue(query, "mux", "");
            if (mux_str == "true") {
                mux_state = 1;
            } else if (mux_str == "false") {
                mux_state = 2;
            }

            // type
            if (stream->network == "ws") {
                stream->path = GetQueryValue(query, "path", "");
                stream->host = GetQueryValue(query, "host", "");
            } else if (stream->network == "http") {
                stream->path = GetQueryValue(query, "path", "");
                stream->host = GetQueryValue(query, "host", "").replace("|", ",");
            } else if (stream->network == "httpupgrade") {
                stream->path = GetQueryValue(query, "path", "");
                stream->host = GetQueryValue(query, "host", "");
            } else if (stream->network == "grpc") {
                stream->path = GetQueryValue(query, "serviceName", "");
            } else if (stream->network == "tcp") {
                if (GetQueryValue(query, "headerType") == "http") {
                    stream->header_type = "http";
                    stream->path = GetQueryValue(query, "path", "");
                    stream->host = GetQueryValue(query, "host", "");
                }
            }
            return !(uuid.isEmpty() || serverAddress.isEmpty());
        }

        return false;
    }

    bool QUICBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        auto query = QUrlQuery(url.query());
        if (url.host().isEmpty() || url.port() == -1) return false;

        if (url.scheme() == "hysteria") {
            // https://hysteria.network/docs/uri-scheme/
            if (!query.hasQueryItem("upmbps") || !query.hasQueryItem("downmbps")) return false;

            name = url.fragment(QUrl::FullyDecoded);
            serverAddress = url.host();
            serverPort = url.port();
            obfsPassword = QUrl::fromPercentEncoding(query.queryItemValue("obfsParam").toUtf8());
            allowInsecure = QStringList{"1", "true"}.contains(query.queryItemValue("insecure"));
            uploadMbps = query.queryItemValue("upmbps").toInt();
            downloadMbps = query.queryItemValue("downmbps").toInt();

            auto protocolStr = (query.hasQueryItem("protocol") ? query.queryItemValue("protocol") : "udp").toLower();
            if (protocolStr == "faketcp") {
                hyProtocol = Configs::QUICBean::hysteria_protocol_facktcp;
            } else if (protocolStr.startsWith("wechat")) {
                hyProtocol = Configs::QUICBean::hysteria_protocol_wechat_video;
            }

            if (query.hasQueryItem("auth")) {
                authPayload = query.queryItemValue("auth");
                authPayloadType = Configs::QUICBean::hysteria_auth_string;
            }

            alpn = query.queryItemValue("alpn");
            sni = FIRST_OR_SECOND(query.queryItemValue("peer"), query.queryItemValue("sni"));

            connectionReceiveWindow = query.queryItemValue("recv_window").toInt();
            streamReceiveWindow = query.queryItemValue("recv_window_conn").toInt();
        } else if (url.scheme() == "tuic") {
            // by daeuniverse
            // https://github.com/daeuniverse/dae/discussions/182

            name = url.fragment(QUrl::FullyDecoded);
            serverAddress = url.host();
            if (serverPort == -1) serverPort = 443;
            serverPort = url.port();

            uuid = url.userName();
            password = url.password();

            congestionControl = query.queryItemValue("congestion_control");
            alpn = query.queryItemValue("alpn");
            sni = query.queryItemValue("sni");
            udpRelayMode = query.queryItemValue("udp_relay_mode");
            allowInsecure = query.queryItemValue("allow_insecure") == "1";
            disableSni = query.queryItemValue("disable_sni") == "1";
        } else if (QStringList{"hy2", "hysteria2"}.contains(url.scheme())) {
            name = url.fragment(QUrl::FullyDecoded);
            serverAddress = url.host();
            serverPort = url.port();
            obfsPassword = QUrl::fromPercentEncoding(query.queryItemValue("obfs-password").toUtf8());
            allowInsecure = QStringList{"1", "true"}.contains(query.queryItemValue("insecure"));

            if (url.password().isEmpty()) {
                password = url.userName();
            } else {
                password = url.userName() + ":" + url.password();
            }
            if (query.hasQueryItem("server_ports"))
            {
                auto portList = query.queryItemValue("server_ports").split("-");
                for (int i=0;i<portList.size();i+=2)
                {
                    if (i+1 >= portList.size()) break;
                    serverPorts += portList[i]+":"+portList[i+1];
                }
            }
            hop_interval = query.queryItemValue("hop_interval");

            sni = query.queryItemValue("sni");
        }

        return true;
    }

    bool WireguardBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);

        name = url.fragment(QUrl::FullyDecoded);
        serverAddress = url.host();
        serverPort = url.port();
        privateKey = query.queryItemValue("private_key");
        publicKey = query.queryItemValue("peer_public_key");
        preSharedKey = query.queryItemValue("pre_shared_key");
        auto rawReserved = query.queryItemValue("reserved");
        if (!rawReserved.isEmpty()) {
            for (const auto &item: rawReserved.split("-")) reserved += item.toInt();
        }
        auto rawLocalAddr = query.queryItemValue("local_address");
        if (!rawLocalAddr.isEmpty()) {
            for (const auto &item: rawLocalAddr.split("-")) localAddress += item;
        }
        persistentKeepalive = query.queryItemValue("persistent_keepalive").toInt();
        MTU = query.queryItemValue("mtu").toInt();
        useSystemInterface = query.queryItemValue("use_system_interface") == "true";
        workerCount = query.queryItemValue("workers").toInt();

        enable_amenzia = query.queryItemValue("enable_amenzia") == "true";
        junk_packet_count = query.queryItemValue("junk_packet_count").toInt();
        junk_packet_min_size = query.queryItemValue("junk_packet_min_size").toInt();
        junk_packet_max_size = query.queryItemValue("junk_packet_max_size").toInt();
        init_packet_junk_size = query.queryItemValue("init_packet_junk_size").toInt();
        response_packet_junk_size = query.queryItemValue("response_packet_junk_size").toInt();
        init_packet_magic_header = query.queryItemValue("init_packet_magic_header").toInt();
        response_packet_magic_header = query.queryItemValue("response_packet_magic_header").toInt();
        underload_packet_magic_header = query.queryItemValue("underload_packet_magic_header").toInt();
        transport_packet_magic_header = query.queryItemValue("transport_packet_magic_header").toInt();

        return true;
    }

    bool SSHBean::TryParseLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = GetQuery(url);

        name = url.fragment(QUrl::FullyDecoded);
        serverAddress = url.host();
        serverPort = url.port();
        user = query.queryItemValue("user");
        password = query.queryItemValue("password");
        privateKey = QByteArray::fromBase64(query.queryItemValue("private_key").toUtf8(), QByteArray::OmitTrailingEquals);
        privateKeyPath = query.queryItemValue("private_key_path");
        privateKeyPass = query.queryItemValue("private_key_passphrase");
        auto hostKeysRaw = query.queryItemValue("host_key");
        for (const auto &item: hostKeysRaw.split("-")) {
            auto b64hostKey = QByteArray::fromBase64(item.toUtf8(), QByteArray::OmitTrailingEquals);
            if (!b64hostKey.isEmpty()) hostKey << QString(b64hostKey);
        }
        auto hostKeyAlgsRaw = query.queryItemValue("host_key_algorithms");
        for (const auto &item: hostKeyAlgsRaw.split("-")) {
            auto b64hostKeyAlg = QByteArray::fromBase64(item.toUtf8(), QByteArray::OmitTrailingEquals);
            if (!b64hostKeyAlg.isEmpty()) hostKeyAlgs << QString(b64hostKeyAlg);
        }
        clientVersion = query.queryItemValue("client_version");

        return true;
    }

    bool ExtraCoreBean::TryParseLink(const QString& link)
    {
        return false;
    }


} // namespace Configs