#include "include/dataStore/ProxyEntity.hpp"
#include "include/configs/proxy/includes.h"

#include <QUrlQuery>

namespace Configs {
    QString SocksHttpBean::ToShareLink() {
        QUrl url;
        if (socks_http_type == type_HTTP) { // http
            if (stream->security == "tls") {
                url.setScheme("https");
            } else {
                url.setScheme("http");
            }
        } else {
            url.setScheme(QString("socks%1").arg(socks_http_type));
        }
        if (!name.isEmpty()) url.setFragment(name);
        if (!username.isEmpty()) url.setUserName(username);
        if (!password.isEmpty()) url.setPassword(password);
        url.setHost(serverAddress);
        url.setPort(serverPort);
        return url.toString(QUrl::FullyEncoded);
    }

    QString AnyTLSBean::ToShareLink() {
        QUrl url;
        QUrlQuery query;
        url.setScheme("anytls");
        url.setUserName(password);
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);

        //  security
        auto security = stream->security;
        if (security == "tls" && !stream->reality_pbk.trimmed().isEmpty()) security = "reality";
        query.addQueryItem("security", security == "" ? "none" : security);

        if (!stream->sni.isEmpty()) query.addQueryItem("sni", stream->sni);
        if (!stream->alpn.isEmpty()) query.addQueryItem("alpn", stream->alpn);
        if (stream->allow_insecure) query.addQueryItem("insecure", "1");
        if (!stream->utlsFingerprint.isEmpty()) query.addQueryItem("fp", stream->utlsFingerprint);
        if (stream->enable_tls_fragment) query.addQueryItem("fragment", "1");
        if (!stream->tls_fragment_fallback_delay.isEmpty()) query.addQueryItem("fragment_fallback_delay", stream->tls_fragment_fallback_delay);
        if (stream->enable_tls_record_fragment) query.addQueryItem("record_fragment", "1");

        if (security == "reality") {
            query.addQueryItem("pbk", stream->reality_pbk);
            if (!stream->reality_sid.isEmpty()) query.addQueryItem("sid", stream->reality_sid);
        }

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QString TrojanVLESSBean::ToShareLink() {
        QUrl url;
        QUrlQuery query;
        url.setScheme(proxy_type == proxy_VLESS ? "vless" : "trojan");
        url.setUserName(password);
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);

        //  security
        auto security = stream->security;
        if (security == "tls" && !stream->reality_pbk.trimmed().isEmpty()) security = "reality";
        query.addQueryItem("security", security == "" ? "none" : security);

        if (!stream->sni.isEmpty()) query.addQueryItem("sni", stream->sni);
        if (!stream->alpn.isEmpty()) query.addQueryItem("alpn", stream->alpn);
        if (stream->allow_insecure) query.addQueryItem("allowInsecure", "1");
        if (!stream->utlsFingerprint.isEmpty()) query.addQueryItem("fp", stream->utlsFingerprint);
        if (stream->enable_tls_fragment) query.addQueryItem("fragment", "1");
        if (!stream->tls_fragment_fallback_delay.isEmpty()) query.addQueryItem("fragment_fallback_delay", stream->tls_fragment_fallback_delay);
        if (stream->enable_tls_record_fragment) query.addQueryItem("record_fragment", "1");

        if (security == "reality") {
            query.addQueryItem("pbk", stream->reality_pbk);
            if (!stream->reality_sid.isEmpty()) query.addQueryItem("sid", stream->reality_sid);
        }

        // type
        query.addQueryItem("type", stream->network);

        if (stream->network == "ws" || stream->network == "httpupgrade") {
            if (!stream->path.isEmpty()) query.addQueryItem("path", stream->path);
            if (!stream->host.isEmpty()) query.addQueryItem("host", stream->host);
        } else if (stream->network == "http" ) {
            if (!stream->path.isEmpty()) query.addQueryItem("path", stream->path);
            if (!stream->host.isEmpty()) query.addQueryItem("host", stream->host);
            if (!stream->method.isEmpty()) query.addQueryItem("method", stream->method);
        } else if (stream->network == "grpc") {
            if (!stream->path.isEmpty()) query.addQueryItem("serviceName", stream->path);
        } else if (stream->network == "tcp") {
            if (stream->header_type == "http") {
                if (!stream->path.isEmpty()) query.addQueryItem("path", stream->path);
                query.addQueryItem("headerType", "http");
                query.addQueryItem("host", stream->host);
            }
        }

        // mux
        if (mux_state == 1) {
            query.addQueryItem("mux", "true");
        } else if (mux_state == 2) {
            query.addQueryItem("mux", "false");
        }

        // protocol
        if (proxy_type == proxy_VLESS) {
            if (!flow.isEmpty()) {
                query.addQueryItem("flow", flow);
            }
            if (!stream->packet_encoding.isEmpty()) {
                query.addQueryItem("packetEncoding", stream->packet_encoding);
            }
            query.addQueryItem("encryption", "none");
        }

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    const char* fixShadowsocksUserNameEncodeMagic = "fixShadowsocksUserNameEncodeMagic-holder-for-QUrl";

    QString ShadowSocksBean::ToShareLink() {
        QUrl url;
        url.setScheme("ss");
        if (method.startsWith("2022-")) {
            url.setUserName(fixShadowsocksUserNameEncodeMagic);
        } else {
            auto method_password = method + ":" + password;
            url.setUserName(method_password.toUtf8().toBase64(QByteArray::Base64Option::Base64UrlEncoding));
        }
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);
        QUrlQuery q;
        if (!plugin.isEmpty()) q.addQueryItem("plugin", plugin);

        // mux
        if (mux_state == 1) {
            q.addQueryItem("mux", "true");
        } else if (mux_state == 2) {
            q.addQueryItem("mux", "false");
        }
        // uot
        if (uot == 1) {
            q.addQueryItem("uot", "1");
        } else if (uot == 2) {
            q.addQueryItem("uot", "2");
        }

        if (!q.isEmpty()) url.setQuery(q);
        //
        auto link = url.toString(QUrl::FullyEncoded);
        link = link.replace(fixShadowsocksUserNameEncodeMagic, method + ":" + QUrl::toPercentEncoding(password));
        return link;
    }

    QString VMessBean::ToShareLink() {
        QUrl url;
        QUrlQuery query;
        url.setScheme("vmess");
        url.setUserName(uuid);
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);

        query.addQueryItem("encryption", security);

        //  security
        auto security = stream->security;
        if (security == "tls" && !stream->reality_pbk.trimmed().isEmpty()) security = "reality";
        query.addQueryItem("security", security == "" ? "none" : security);

        if (!stream->sni.isEmpty()) query.addQueryItem("sni", stream->sni);
        if (stream->allow_insecure) query.addQueryItem("allowInsecure", "1");
        if (stream->utlsFingerprint.isEmpty()) {
            query.addQueryItem("fp", Configs::dataStore->utlsFingerprint);
        } else {
            query.addQueryItem("fp", stream->utlsFingerprint);
        }
        if (stream->enable_tls_fragment) query.addQueryItem("fragment", "1");
        if (!stream->tls_fragment_fallback_delay.isEmpty()) query.addQueryItem("fragment_fallback_delay", stream->tls_fragment_fallback_delay);
        if (stream->enable_tls_record_fragment) query.addQueryItem("record_fragment", "1");

        if (security == "reality") {
            query.addQueryItem("pbk", stream->reality_pbk);
            if (!stream->reality_sid.isEmpty()) query.addQueryItem("sid", stream->reality_sid);
        }

        // type
        query.addQueryItem("type", stream->network);

        if (stream->network == "ws" || stream->network == "http" || stream->network == "httpupgrade") {
            if (!stream->path.isEmpty()) query.addQueryItem("path", stream->path);
            if (!stream->host.isEmpty()) query.addQueryItem("host", stream->host);
        } else if (stream->network == "grpc") {
            if (!stream->path.isEmpty()) query.addQueryItem("serviceName", stream->path);
        } else if (stream->network == "tcp") {
            if (stream->header_type == "http") {
                query.addQueryItem("headerType", "http");
                query.addQueryItem("host", stream->host);
            }
        }

        // mux
        if (mux_state == 1) {
            query.addQueryItem("mux", "true");
        } else if (mux_state == 2) {
            query.addQueryItem("mux", "false");
        }

        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QString QUICBean::ToShareLink() {
        QUrl url;
        if (proxy_type == proxy_Hysteria) {
            url.setScheme("hysteria");
            url.setHost(serverAddress);
            url.setPort(serverPort);
            QUrlQuery q;
            q.addQueryItem("upmbps", Int2String(uploadMbps));
            q.addQueryItem("downmbps", Int2String(downloadMbps));
            if (!obfsPassword.isEmpty()) {
                q.addQueryItem("obfs", "xplus");
                q.addQueryItem("obfsParam", QUrl::toPercentEncoding(obfsPassword));
            }
            if (authPayloadType == hysteria_auth_string) q.addQueryItem("auth", authPayload);
            if (hyProtocol == hysteria_protocol_facktcp) q.addQueryItem("protocol", "faketcp");
            if (hyProtocol == hysteria_protocol_wechat_video) q.addQueryItem("protocol", "wechat-video");
            if (allowInsecure) q.addQueryItem("insecure", "1");
            if (!sni.isEmpty()) q.addQueryItem("peer", sni);
            if (!alpn.isEmpty()) q.addQueryItem("alpn", alpn);
            if (connectionReceiveWindow > 0) q.addQueryItem("recv_window", Int2String(connectionReceiveWindow));
            if (streamReceiveWindow > 0) q.addQueryItem("recv_window_conn", Int2String(streamReceiveWindow));
            if (!q.isEmpty()) url.setQuery(q);
            if (!name.isEmpty()) url.setFragment(name);
        } else if (proxy_type == proxy_TUIC) {
            url.setScheme("tuic");
            url.setUserName(uuid);
            url.setPassword(password);
            url.setHost(serverAddress);
            url.setPort(serverPort);

            QUrlQuery q;
            if (!congestionControl.isEmpty()) q.addQueryItem("congestion_control", congestionControl);
            if (!alpn.isEmpty()) q.addQueryItem("alpn", alpn);
            if (!sni.isEmpty()) q.addQueryItem("sni", sni);
            if (!udpRelayMode.isEmpty()) q.addQueryItem("udp_relay_mode", udpRelayMode);
            if (allowInsecure) q.addQueryItem("allow_insecure", "1");
            if (disableSni) q.addQueryItem("disable_sni", "1");
            if (!q.isEmpty()) url.setQuery(q);
            if (!name.isEmpty()) url.setFragment(name);
        } else if (proxy_type == proxy_Hysteria2) {
            url.setScheme("hy2");
            url.setHost(serverAddress);
            url.setPort(serverPort);
            if (password.contains(":")) {
                url.setUserName(SubStrBefore(password, ":"));
                url.setPassword(SubStrAfter(password, ":"));
            } else {
                url.setUserName(password);
            }
            QUrlQuery q;
            if (!obfsPassword.isEmpty()) {
                q.addQueryItem("obfs", "salamander");
                q.addQueryItem("obfs-password", QUrl::toPercentEncoding(obfsPassword));
            }
            if (allowInsecure) q.addQueryItem("insecure", "1");
            if (!sni.isEmpty()) q.addQueryItem("sni", sni);
            if (!serverPorts.empty())
            {
                QStringList portList;
                for (const auto& range : serverPorts)
                {
                    portList.append(range.split(":"));
                }
                q.addQueryItem("server_ports", portList.join("-"));
            }
            if (!hop_interval.isEmpty()) q.addQueryItem("hop_interval", hop_interval);
            if (!q.isEmpty()) url.setQuery(q);
            if (!name.isEmpty()) url.setFragment(name);
        }
        return url.toString(QUrl::FullyEncoded);
    }

    QString WireguardBean::ToShareLink() {
        QUrl url;
        url.setScheme("wg");
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);
        QUrlQuery q;
        q.addQueryItem("private_key", privateKey);
        q.addQueryItem("peer_public_key", publicKey);
        q.addQueryItem("pre_shared_key", preSharedKey);
        q.addQueryItem("reserved", FormatReserved());
        q.addQueryItem("persistent_keepalive", Int2String(persistentKeepalive));
        q.addQueryItem("mtu", Int2String(MTU));
        q.addQueryItem("use_system_interface", useSystemInterface ? "true":"false");
        q.addQueryItem("local_address", localAddress.join("-"));
        q.addQueryItem("workers", Int2String(workerCount));
        if (enable_amnezia)
        {
            q.addQueryItem("enable_amnezia", "true");
            q.addQueryItem("junk_packet_count", Int2String(junk_packet_count));
            q.addQueryItem("junk_packet_min_size", Int2String(junk_packet_min_size));
            q.addQueryItem("junk_packet_max_size", Int2String(junk_packet_max_size));
            q.addQueryItem("init_packet_junk_size", Int2String(init_packet_junk_size));
            q.addQueryItem("response_packet_junk_size", Int2String(response_packet_junk_size));
            q.addQueryItem("init_packet_magic_header", Int2String(init_packet_magic_header));
            q.addQueryItem("response_packet_magic_header", Int2String(response_packet_magic_header));
            q.addQueryItem("underload_packet_magic_header", Int2String(underload_packet_magic_header));
            q.addQueryItem("transport_packet_magic_header", Int2String(transport_packet_magic_header));
        }
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }

    QString TailscaleBean::ToShareLink()
    {
        QUrl url;
        url.setScheme("ts");
        url.setHost("tailscale");
        if (!name.isEmpty()) url.setFragment(name);
        QUrlQuery q;
        q.addQueryItem("state_directory", QUrl::toPercentEncoding(state_directory));
        q.addQueryItem("auth_key", QUrl::toPercentEncoding(auth_key));
        q.addQueryItem("control_url", QUrl::toPercentEncoding(control_url));
        q.addQueryItem("ephemeral", ephemeral ? "true" : "false");
        q.addQueryItem("hostname", QUrl::toPercentEncoding(hostname));
        q.addQueryItem("accept_routes", accept_routes ? "true" : "false");
        q.addQueryItem("exit_node", exit_node);
        q.addQueryItem("exit_node_allow_lan_access", exit_node_allow_lan_access ? "true" : "false");
        q.addQueryItem("advertise_routes", QUrl::toPercentEncoding(advertise_routes.join(",")));
        q.addQueryItem("advertise_exit_node", advertise_exit_node ? "true" : "false");
        q.addQueryItem("global_dns", globalDNS ? "true" : "false");
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }

    QString SSHBean::ToShareLink() {
        QUrl url;
        url.setScheme("ssh");
        url.setHost(serverAddress);
        url.setPort(serverPort);
        if (!name.isEmpty()) url.setFragment(name);
        QUrlQuery q;
        q.addQueryItem("user", user);
        q.addQueryItem("password", password);
        q.addQueryItem("private_key", privateKey.toUtf8().toBase64(QByteArray::OmitTrailingEquals));
        q.addQueryItem("private_key_path", privateKeyPath);
        q.addQueryItem("private_key_passphrase", privateKeyPass);
        QStringList b64HostKeys = {};
        for (const auto& item: hostKey) {
            b64HostKeys << item.toUtf8().toBase64(QByteArray::OmitTrailingEquals);
        }
        q.addQueryItem("host_key", b64HostKeys.join("-"));
        QStringList b64HostKeyAlgs = {};
        for (const auto& item: hostKeyAlgs) {
            b64HostKeyAlgs << item.toUtf8().toBase64(QByteArray::OmitTrailingEquals);
        }
        q.addQueryItem("host_key_algorithms", b64HostKeyAlgs.join("-"));
        q.addQueryItem("client_version", clientVersion);
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }

    QString ExtraCoreBean::ToShareLink()
    {
        return "Unsupported for now";
    }

} // namespace Configs