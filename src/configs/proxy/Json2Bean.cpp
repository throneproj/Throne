

#include <include/configs/proxy/QUICBean.hpp>
#include <include/configs/proxy/ShadowSocksBean.hpp>
#include <include/configs/proxy/SocksHttpBean.hpp>
#include <include/configs/proxy/TrojanVLESSBean.hpp>
#include <include/configs/proxy/VMessBean.hpp>
#include <include/configs/proxy/AnyTlsBean.hpp>
#include <include/configs/proxy/WireguardBean.h>

#include "include/configs/proxy/ExtraCore.h"
#include "include/configs/proxy/SSHBean.h"

namespace Configs
{
    bool QUICBean::TryParseJson(const QJsonObject& obj)
    {
        auto type = obj["type"].toString();
        if (type == "hysteria")
        {
            name = obj["tag"].toString();
            proxy_type = proxy_Hysteria;

            serverAddress = obj["server"].isString() ? obj["server"].toString() : "127.0.0.1";
            serverPort = obj["server_port"].isDouble() ? obj["server_port"].toInt() : 1080;
            hop_interval = obj["hop_interval"].toString();
            uploadMbps = obj["up_mbps"].isDouble() ? obj["up_mbps"].toInt() : 0;
            downloadMbps = obj["down_mbps"].isDouble() ? obj["down_mbps"].toInt() : 0;
            obfsPassword = obj["obfs"].toString();
            authPayloadType = obj["auth"].isString() ? hysteria_auth_base64 : hysteria_auth_string;
            authPayload = obj["auth"].isString() ? obj["auth"].toString() : obj["auth_str"].toString();
            disableMtuDiscovery = obj["disable_mtu_discovery"].toBool();
            connectionReceiveWindow = obj["recv_window_conn"].toInt();
            streamReceiveWindow = obj["recv_window"].toInt();
            alpn = obj["tls"].toObject()["alpn"].isArray() ? QJsonArray2QListString(obj["tls"].toObject()["alpn"].toArray()).join(",") : obj["tls"].toObject()["alpn"].toString();
            sni = obj["tls"].toObject()["server_name"].toString();
            disableSni = obj["tls"].toObject()["disable_sni"].toBool();
            allowInsecure = obj["tls"].toObject()["insecure"].toBool();
            return true;
        }
        if (type == "hysteria2")
        {
            name = obj["tag"].toString();
            proxy_type = proxy_Hysteria2;

            serverAddress = obj["server"].isString() ? obj["server"].toString() : "127.0.0.1";
            serverPort = obj["server_port"].isDouble() ? obj["server_port"].toInt() : 1080;
            serverPorts = obj["server_ports"].isArray() ? QJsonArray2QListString(obj["server_ports"].toArray()) : QStringList();
            hop_interval = obj["hop_interval"].toString();
            uploadMbps = obj["up_mbps"].isDouble() ? obj["up_mbps"].toInt() : 0;
            downloadMbps = obj["down_mbps"].isDouble() ? obj["down_mbps"].toInt() : 0;
            password = obj["password"].toString();
            obfsPassword = obj["obfs"].toObject()["password"].toString();
            alpn = obj["tls"].toObject()["alpn"].isArray() ? QJsonArray2QListString(obj["tls"].toObject()["alpn"].toArray()).join(",") : obj["tls"].toObject()["alpn"].toString();
            sni = obj["tls"].toObject()["server_name"].toString();
            disableSni = obj["tls"].toObject()["disable_sni"].toBool();
            allowInsecure = obj["tls"].toObject()["insecure"].toBool();
            return true;
        }
        if (type == "tuic")
        {
            name = obj["tag"].toString();
            proxy_type = proxy_TUIC;

            serverAddress = obj["server"].isString() ? obj["server"].toString() : "127.0.0.1";
            serverPort = obj["server_port"].isDouble() ? obj["server_port"].toInt() : 1080;
            uuid = obj["uuid"].toString();
            password = obj["password"].toString();
            congestionControl = obj["congestion_control"].toString();
            udpRelayMode = obj["udp_relay_mode"].toString();
            uos = obj["udp_over_stream"].toBool();
            zeroRttHandshake = obj["zero_rtt_handshake"].toBool();
            heartbeat = obj["heartbeat"].toString();
            alpn = obj["tls"].toObject()["alpn"].isArray() ? QJsonArray2QListString(obj["tls"].toObject()["alpn"].toArray()).join(",") : obj["tls"].toObject()["alpn"].toString();
            sni = obj["tls"].toObject()["server_name"].toString();
            disableSni = obj["tls"].toObject()["disable_sni"].toBool();
            allowInsecure = obj["tls"].toObject()["insecure"].toBool();
            return true;
        }
        return false;
    }

    bool ShadowSocksBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        method = obj["method"].toString();
        password = obj["password"].toString();
        plugin = obj["plugin"].toString();
        uot = obj["udp_over_tcp"].toBool();
        mux_state = obj["multiplex"].isObject() ? (obj["multiplex"].toObject()["enabled"].toBool() ? 1 : 2) : 0;
        return true;
    }

    bool SocksHttpBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        socks_http_type = obj["type"] == "http" ? type_HTTP : type_Socks5;
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        username = obj["username"].toString();
        password = obj["password"].toString();
        stream->security = obj["tls"].isObject() ? "tls" : "";
        stream->sni = obj["tls"].toObject()["server_name"].toString();
        stream->allow_insecure = obj["tls"].toObject()["insecure"].toBool();
        return true;
    }

    bool SSHBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        user = obj["user"].toString();
        password = obj["password"].toString();
        privateKey = obj["private_key"].toString();
        privateKeyPath = obj["private_key_path"].toString();
        privateKeyPass = obj["private_key_passphrase"].toString();
        hostKey = QJsonArray2QListString(obj["host_key"].toArray());
        hostKeyAlgs = QJsonArray2QListString(obj["host_key_algorithms"].toArray());
        clientVersion = obj["client_version"].toString();
        return true;
    }

    bool TrojanVLESSBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        proxy_type = obj["type"].toString() == "trojan" ? proxy_Trojan : proxy_VLESS;
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        password = obj["password"].toString();
        if (proxy_type == proxy_VLESS) password = obj["uuid"].toString();
        flow = obj["flow"].toString();
        stream->packet_encoding = obj["packet_encoding"].toString();
        mux_state = obj["multiplex"].isObject() ? (obj["multiplex"].toObject()["enabled"].toBool() ? 1 : 2) : 0;
        stream->security = obj["tls"].isObject() ? "tls" : "";
        if (obj["tls"].toObject()["reality"].toObject()["enabled"].toBool())
        {
            stream->security = "reality";
        }
        stream->reality_pbk = obj["tls"].toObject()["reality"].toObject()["public_key"].toString();
        stream->reality_sid = obj["tls"].toObject()["reality"].toObject()["short_id"].toString();
        stream->utlsFingerprint = obj["tls"].toObject()["utls"].toObject()["fingerprint"].toString();
        stream->enable_tls_fragment = obj["tls"].toObject()["fragment"].toBool();
        stream->tls_fragment_fallback_delay = obj["tls"].toObject()["fragment_fallback_delay"].toString();
        stream->enable_tls_record_fragment = obj["tls"].toObject()["record_fragment"].toBool();
        stream->sni = obj["tls"].toObject()["server_name"].toString();
        stream->alpn = obj["tls"].toObject()["alpn"].isArray() ? QJsonArray2QListString(obj["tls"].toObject()["alpn"].toArray()).join(",") : obj["tls"].toObject()["alpn"].toString();
        stream->allow_insecure = obj["tls"].toObject()["insecure"].toBool();
        stream->network = obj["transport"].toObject()["type"].toString();
        if (stream->network == "ws")
        {
            stream->path = obj["transport"].toObject()["path"].toString();
            stream->host = obj["transport"].toObject()["host"].isArray() ? QJsonArray2QListString(obj["transport"].toObject()["host"].toArray()).join(",") : obj["transport"].toObject()["host"].toString();
        } else if (stream->network == "http")
        {
            stream->path = obj["transport"].toObject()["path"].toString();
            stream->host = obj["transport"].toObject()["host"].isArray() ? QJsonArray2QListString(obj["transport"].toObject()["host"].toArray()).join(",") : obj["transport"].toObject()["host"].toString();
            stream->method = obj["transport"].toObject()["method"].toString();
        } else if (stream->network == "httpupgrade")
        {
            stream->path = obj["transport"].toObject()["path"].toString();
            stream->host = obj["transport"].toObject()["host"].isArray() ? QJsonArray2QListString(obj["transport"].toObject()["host"].toArray()).join(",") : obj["transport"].toObject()["host"].toString();
        } else if (stream->network == "grpc")
        {
            stream->path = obj["transport"].toObject()["service_name"].toString();
        }
        return true;
    }

    bool VMessBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        uuid = obj["uuid"].toString();
        security = obj["security"].toString();
        aid = obj["alter_id"].toInt();
        stream->packet_encoding = obj["packet_encoding"].toString();
        mux_state = obj["multiplex"].isObject() ? (obj["multiplex"].toObject()["enabled"].toBool() ? 1 : 2) : 0;
        stream->security = obj["tls"].isObject() ? "tls" : "";
        if (obj["tls"].toObject()["reality"].toObject()["enabled"].toBool())
        {
            stream->security = "reality";
        }
        stream->reality_pbk = obj["tls"].toObject()["reality"].toObject()["public_key"].toString();
        stream->reality_sid = obj["tls"].toObject()["reality"].toObject()["short_id"].toString();
        stream->utlsFingerprint = obj["tls"].toObject()["utls"].toObject()["fingerprint"].toString();
        stream->enable_tls_fragment = obj["tls"].toObject()["fragment"].toBool();
        stream->tls_fragment_fallback_delay = obj["tls"].toObject()["fragment_fallback_delay"].toString();
        stream->enable_tls_record_fragment = obj["tls"].toObject()["record_fragment"].toBool();
        stream->sni = obj["tls"].toObject()["server_name"].toString();
        stream->alpn = obj["tls"].toObject()["alpn"].isArray() ? QJsonArray2QListString(obj["tls"].toObject()["alpn"].toArray()).join(",") : obj["tls"].toObject()["alpn"].toString();
        stream->allow_insecure = obj["tls"].toObject()["insecure"].toBool();
        stream->network = obj["transport"].toObject()["type"].toString();
        if (stream->network == "ws")
        {
            stream->path = obj["transport"].toObject()["path"].toString();
            stream->host = obj["transport"].toObject()["host"].isArray() ? QJsonArray2QListString(obj["transport"].toObject()["host"].toArray()).join(",") : obj["transport"].toObject()["host"].toString();
        } else if (stream->network == "http")
        {
            stream->path = obj["transport"].toObject()["path"].toString();
            stream->host = obj["transport"].toObject()["host"].isArray() ? QJsonArray2QListString(obj["transport"].toObject()["host"].toArray()).join(",") : obj["transport"].toObject()["host"].toString();
            stream->method = obj["transport"].toObject()["method"].toString();
        } else if (stream->network == "httpupgrade")
        {
            stream->path = obj["transport"].toObject()["path"].toString();
            stream->host = obj["transport"].toObject()["host"].isArray() ? QJsonArray2QListString(obj["transport"].toObject()["host"].toArray()).join(",") : obj["transport"].toObject()["host"].toString();
        } else if (stream->network == "grpc")
        {
            stream->path = obj["transport"].toObject()["service_name"].toString();
        }
        return true;
    }

    bool AnyTlsBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        serverAddress = obj["server"].toString();
        serverPort = obj["server_port"].toInt();
        password = obj["password"].toString();
        idle_session_check_interval = obj["idle_session_check_interval"].toInt();
        idle_session_timeout = obj["idle_session_timeout"].toInt();
        min_idle_session = obj["min_idle_session"].toInt();
        stream->security = obj["tls"].isObject() ? "tls" : "";
        if (obj["tls"].toObject()["reality"].toObject()["enabled"].toBool())
        {
            stream->security = "reality";
        }
        stream->reality_pbk = obj["tls"].toObject()["reality"].toObject()["public_key"].toString();
        stream->reality_sid = obj["tls"].toObject()["reality"].toObject()["short_id"].toString();
        stream->utlsFingerprint = obj["tls"].toObject()["utls"].toObject()["fingerprint"].toString();
        stream->enable_tls_fragment = obj["tls"].toObject()["fragment"].toBool();
        stream->tls_fragment_fallback_delay = obj["tls"].toObject()["fragment_fallback_delay"].toString();
        stream->enable_tls_record_fragment = obj["tls"].toObject()["record_fragment"].toBool();
        stream->sni = obj["tls"].toObject()["server_name"].toString();
        stream->alpn = obj["tls"].toObject()["alpn"].isArray() ? QJsonArray2QListString(obj["tls"].toObject()["alpn"].toArray()).join(",") : obj["tls"].toObject()["alpn"].toString();
        stream->allow_insecure = obj["tls"].toObject()["insecure"].toBool();
        return true;
    }

    bool WireguardBean::TryParseJson(const QJsonObject& obj)
    {
        name = obj["tag"].toString();
        auto peers = obj["peers"].toArray();
        if (peers.empty()) return false;
        serverAddress = peers[0].toObject()["address"].toString();
        serverPort = peers[0].toObject()["port"].toInt();
        publicKey = peers[0].toObject()["public_key"].toString();
        reserved = QJsonArray2QListInt(peers[0].toObject()["reserved"].toArray());
        persistentKeepalive = peers[0].toObject()["persistent_keepalive_interval"].toInt();
        workerCount = obj["workers"].toInt();
        privateKey = obj["private_key"].toString();
        localAddress = QJsonArray2QListString(obj["address"].toArray());
        MTU = obj["mtu"].toInt();
        useSystemInterface = obj["system"].toBool();

        junk_packet_count = obj["junk_packet_count"].toInt();
        junk_packet_min_size = obj["junk_packet_min_size"].toInt();
        junk_packet_max_size = obj["junk_packet_max_size"].toInt();
        init_packet_junk_size = obj["init_packet_junk_size"].toInt();
        response_packet_junk_size = obj["response_packet_junk_size"].toInt();
        init_packet_magic_header = obj["init_packet_magic_header"].toInt();
        response_packet_magic_header = obj["response_packet_magic_header"].toInt();
        underload_packet_magic_header = obj["underload_packet_magic_header"].toInt();
        transport_packet_magic_header = obj["transport_packet_magic_header"].toInt();
        if (junk_packet_count > 0 || junk_packet_min_size > 0 || junk_packet_max_size > 0)
        {
            enable_amenzia = true;
        }

        return true;
    }

    bool ExtraCoreBean::TryParseJson(const QJsonObject& obj)
    {
        return false;
    }

}
