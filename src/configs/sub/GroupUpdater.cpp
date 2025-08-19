#include "include/dataStore/ProfileFilter.hpp"
#include "include/configs/proxy/includes.h"
#include "include/global/HTTPRequestHelper.hpp"

#include "include/configs/sub/GroupUpdater.hpp"

#include <QInputDialog>
#include <QUrlQuery>

#include "3rdparty/fkYAML/node.hpp"

namespace Subscription {

    GroupUpdater *groupUpdater = new GroupUpdater;

    void RawUpdater_FixEnt(const std::shared_ptr<Configs::ProxyEntity> &ent) {
        if (ent == nullptr) return;
        auto stream = Configs::GetStreamSettings(ent->bean.get());
        if (stream == nullptr) return;
        // 1. "security"
        if (stream->security == "none" || stream->security == "0" || stream->security == "false") {
            stream->security = "";
        } else if (stream->security == "1" || stream->security == "true") {
            stream->security = "tls";
        }
        // 2. TLS SNI: v2rayN config builder generate sni like this, so set sni here for their format.
        if (stream->security == "tls" && IsIpAddress(ent->bean->serverAddress) && (!stream->host.isEmpty()) && stream->sni.isEmpty()) {
            stream->sni = stream->host;
        }
    }

    int JsonEndIdx(const QString &str, int begin) {
        int sz = str.length();
        int counter = 1;
        for (int i=begin+1;i<sz;i++) {
            if (str[i] == '{') counter++;
            if (str[i] == '}') counter--;
            if (counter==0) return i;
        }
        return -1;
    }

    QList<QString> Disect(const QString &str) {
        QList<QString> res = QList<QString>();
        int idx=0;
        int sz = str.size();
        while(idx < sz) {
            if (str[idx] == '\n') {
                idx++;
                continue;
            }
            if (str[idx] == '{') {
                int endIdx = JsonEndIdx(str, idx);
                if (endIdx == -1) return res;
                res.append(str.mid(idx, endIdx-idx + 1));
                idx = endIdx+1;
                continue;
            }
            int nlineIdx = str.indexOf('\n', idx);
            if (nlineIdx == -1) nlineIdx = sz;
            res.append(str.mid(idx, nlineIdx-idx));
            idx = nlineIdx+1;
        }
        return res;
    }

    void RawUpdater::update(const QString &str, bool needParse = true) {
        // Base64 encoded subscription
        if (auto str2 = DecodeB64IfValid(str); !str2.isEmpty()) {
            update(str2);
            return;
        }

        // Clash
        if (str.contains("proxies:")) {
            updateClash(str);
            return;
        }

        // SingBox
        if (str.contains("outbounds") || str.contains("endpoints"))
        {
            updateSingBox(str);
            return;
        }

        // Multi line
        if (str.count("\n") > 0 && needParse) {
            auto list = Disect(str);
            for (const auto &str2: list) {
                update(str2.trimmed(), false);
            }
            return;
        }

        // is comment or too short
        if (str.startsWith("//") || str.startsWith("#") || str.length() < 2) {
            return;
        }

        std::shared_ptr<Configs::ProxyEntity> ent;
        bool needFix = true;

        // Nekoray format
        if (str.startsWith("nekoray://")) {
            needFix = false;
            auto link = QUrl(str);
            if (!link.isValid()) return;
            ent = Configs::ProfileManager::NewProxyEntity(link.host());
            if (ent->bean->version == -114514) return;
            auto j = DecodeB64IfValid(link.fragment().toUtf8(), QByteArray::Base64UrlEncoding);
            if (j.isEmpty()) return;
            ent->bean->FromJsonBytes(j);
        }

        // Json
        if (str.startsWith('{')) {
            ent = Configs::ProfileManager::NewProxyEntity("custom");
            auto bean = ent->CustomBean();
            auto obj = QString2QJsonObject(str);
            if (obj.contains("outbounds")) {
                bean->core = "internal-full";
                bean->config_simple = str;
            } else if (obj.contains("server")) {
                bean->core = "internal";
                bean->config_simple = str;
            } else {
                return;
            }
        }

        // SOCKS
        if (str.startsWith("socks5://") || str.startsWith("socks4://") ||
            str.startsWith("socks4a://") || str.startsWith("socks://")) {
            ent = Configs::ProfileManager::NewProxyEntity("socks");
            auto ok = ent->SocksHTTPBean()->TryParseLink(str);
            if (!ok) return;
        }

        // HTTP
        if (str.startsWith("http://") || str.startsWith("https://")) {
            ent = Configs::ProfileManager::NewProxyEntity("http");
            auto ok = ent->SocksHTTPBean()->TryParseLink(str);
            if (!ok) return;
        }

        // ShadowSocks
        if (str.startsWith("ss://")) {
            ent = Configs::ProfileManager::NewProxyEntity("shadowsocks");
            auto ok = ent->ShadowSocksBean()->TryParseLink(str);
            if (!ok) return;
        }

        // VMess
        if (str.startsWith("vmess://")) {
            ent = Configs::ProfileManager::NewProxyEntity("vmess");
            auto ok = ent->VMessBean()->TryParseLink(str);
            if (!ok) return;
        }

        // VLESS
        if (str.startsWith("vless://")) {
            ent = Configs::ProfileManager::NewProxyEntity("vless");
            auto ok = ent->TrojanVLESSBean()->TryParseLink(str);
            if (!ok) return;
        }

        // Trojan
        if (str.startsWith("trojan://")) {
            ent = Configs::ProfileManager::NewProxyEntity("trojan");
            auto ok = ent->TrojanVLESSBean()->TryParseLink(str);
            if (!ok) return;
        }

        // AnyTLS
        if (str.startsWith("anytls://")) {
            ent = Configs::ProfileManager::NewProxyEntity("anytls");
            auto ok = ent->AnyTLSBean()->TryParseLink(str);
            if (!ok) return;
        }

        // Hysteria1
        if (str.startsWith("hysteria://")) {
            needFix = false;
            ent = Configs::ProfileManager::NewProxyEntity("hysteria");
            auto ok = ent->QUICBean()->TryParseLink(str);
            if (!ok) return;
        }

        // Hysteria2
        if (str.startsWith("hysteria2://") || str.startsWith("hy2://")) {
            needFix = false;
            ent = Configs::ProfileManager::NewProxyEntity("hysteria2");
            auto ok = ent->QUICBean()->TryParseLink(str);
            if (!ok) return;
        }

        // TUIC
        if (str.startsWith("tuic://")) {
            needFix = false;
            ent = Configs::ProfileManager::NewProxyEntity("tuic");
            auto ok = ent->QUICBean()->TryParseLink(str);
            if (!ok) return;
        }

        // Wireguard
        if (str.startsWith("wg://")) {
            needFix = false;
            ent = Configs::ProfileManager::NewProxyEntity("wireguard");
            auto ok = ent->WireguardBean()->TryParseLink(str);
            if (!ok) return;
        }

        // SSH
        if (str.startsWith("ssh://")) {
            needFix = false;
            ent = Configs::ProfileManager::NewProxyEntity("ssh");
            auto ok = ent->SSHBean()->TryParseLink(str);
            if (!ok) return;
        }

        if (ent == nullptr) return;

        // Fix
        if (needFix) RawUpdater_FixEnt(ent);

        // End
        updated_order += ent;
    }

    void RawUpdater::updateSingBox(const QString& str)
    {
        auto json = QString2QJsonObject(str);
        auto outbounds = json["outbounds"].toArray();
        auto endpoints = json["endpoints"].toArray();
        QJsonArray items;
        for (auto && outbound : outbounds)
        {
            if (!outbound.isObject()) continue;
            items.append(outbound.toObject());
        }
        for (auto && endpoint : endpoints)
        {
            if (!endpoint.isObject()) continue;
            items.append(endpoint.toObject());
        }

        for (auto o : items)
        {
            auto out = o.toObject();
            if (out.isEmpty())
            {
                MW_show_log("invalid outbound of type: " + o.type());
                continue;
            }

            std::shared_ptr<Configs::ProxyEntity> ent;

            // SOCKS
            if (out["type"] == "socks") {
                ent = Configs::ProfileManager::NewProxyEntity("socks");
                auto ok = ent->SocksHTTPBean()->TryParseJson(out);
                if (!ok) continue;
            }

            // HTTP
            if (out["type"] == "http") {
                auto ok = ent->SocksHTTPBean()->TryParseJson(out);
                if (!ok) continue;
            }

            // ShadowSocks
            if (out["type"] == "shadowsocks") {
                ent = Configs::ProfileManager::NewProxyEntity("shadowsocks");
                auto ok = ent->ShadowSocksBean()->TryParseJson(out);
                if (!ok) continue;
            }

            // VMess
            if (out["type"] == "vmess") {
                ent = Configs::ProfileManager::NewProxyEntity("vmess");
                auto ok = ent->VMessBean()->TryParseJson(out);
                if (!ok) continue;
            }

            // VLESS
            if (out["type"] == "vless") {
                ent = Configs::ProfileManager::NewProxyEntity("vless");
                auto ok = ent->TrojanVLESSBean()->TryParseJson(out);
                if (!ok) continue;
            }

            // Trojan
            if (out["type"] == "trojan") {
                ent = Configs::ProfileManager::NewProxyEntity("trojan");
                auto ok = ent->TrojanVLESSBean()->TryParseJson(out);
                if (!ok) continue;
            }

            // AnyTLS
            if (out["type"] == "anytls") {
                ent = Configs::ProfileManager::NewProxyEntity("anytls");
                auto ok = ent->AnyTLSBean()->TryParseJson(out);
                if (!ok) continue;
            }

            // Hysteria1
            if (out["type"] == "hysteria") {
                ent = Configs::ProfileManager::NewProxyEntity("hysteria");
                auto ok = ent->QUICBean()->TryParseJson(out);
                if (!ok) continue;
            }

            // Hysteria2
            if (out["type"] == "hysteria2") {
                ent = Configs::ProfileManager::NewProxyEntity("hysteria2");
                auto ok = ent->QUICBean()->TryParseJson(out);
                if (!ok) continue;
            }

            // TUIC
            if (out["type"] == "tuic") {
                ent = Configs::ProfileManager::NewProxyEntity("tuic");
                auto ok = ent->QUICBean()->TryParseJson(out);
                if (!ok) continue;
            }

            // Wireguard
            if (out["type"] == "wireguard") {
                ent = Configs::ProfileManager::NewProxyEntity("wireguard");
                auto ok = ent->WireguardBean()->TryParseJson(out);
                if (!ok) continue;
            }

            // SSH
            if (out["type"] == "ssh") {
                ent = Configs::ProfileManager::NewProxyEntity("ssh");
                auto ok = ent->SSHBean()->TryParseJson(out);
                if (!ok) continue;
            }

            if (ent == nullptr) continue;

            updated_order += ent;
        }
    }

    QString Node2QString(const fkyaml::node &n, const QString &def = "") {
        try {
            return n.as_str().c_str();
        } catch (const fkyaml::exception &ex) {
            qDebug() << ex.what();
            return def;
        }
    }

    QStringList Node2QStringList(const fkyaml::node &n) {
        try {
            if (n.is_sequence()) {
                QStringList list;
                for (auto item: n) {
                    list << item.as_str().c_str();
                }
                return list;
            } else {
                return {};
            }
        } catch (const fkyaml::exception &ex) {
            qDebug() << ex.what();
            return {};
        }
    }

    int Node2Int(const fkyaml::node &n, const int &def = 0) {
        try {
            if (n.is_integer())
                return n.as_int();
            else if (n.is_string())
                return atoi(n.as_str().c_str());
            return def;
        } catch (const fkyaml::exception &ex) {
            qDebug() << ex.what();
            return def;
        }
    }

    bool Node2Bool(const fkyaml::node &n, const bool &def = false) {
        try {
            return n.as_bool();
        } catch (const fkyaml::exception &ex) {
            try {
                return n.as_int();
            } catch (const fkyaml::exception &ex2) {
                ex2.what();
            }
            qDebug() << ex.what();
            return def;
        }
    }

    // NodeChild returns the first defined children or Null Node
    fkyaml::node NodeChild(const fkyaml::node &n, const std::list<std::string> &keys) {
        for (const auto &key: keys) {
            if (n.contains(key)) return n[key];
        }
        return {};
    }

    // https://github.com/Dreamacro/clash/wiki/configuration
    void RawUpdater::updateClash(const QString &str) {
        try {
            auto proxies = fkyaml::node::deserialize(str.toStdString())["proxies"];
            for (auto proxy: proxies) {
                auto type = Node2QString(proxy["type"]).toLower();
                auto type_clash = type;

                if (type == "ss" || type == "ssr") type = "shadowsocks";
                if (type == "socks5") type = "socks";

                auto ent = Configs::ProfileManager::NewProxyEntity(type);
                if (ent->bean->version == -114514) continue;
                bool needFix = false;

                // common
                ent->bean->name = Node2QString(proxy["name"]);
                ent->bean->serverAddress = Node2QString(proxy["server"]);
                ent->bean->serverPort = Node2Int(proxy["port"]);

                if (type_clash == "ss") {
                    auto bean = ent->ShadowSocksBean();
                    bean->method = Node2QString(proxy["cipher"]).replace("dummy", "none");
                    bean->password = Node2QString(proxy["password"]);

                    // UDP over TCP
                    if (Node2Bool(proxy["udp-over-tcp"])) {
                        bean->uot = Node2Int(proxy["udp-over-tcp-version"]);
                        if (bean->uot == 0) bean->uot = 2;
                    }

                    if (proxy.contains("plugin") && proxy.contains("plugin-opts")) {
                        auto plugin_n = proxy["plugin"];
                        auto pluginOpts_n = proxy["plugin-opts"];
                        QStringList ssPlugin;
                        auto plugin = Node2QString(plugin_n);
                        if (plugin == "obfs") {
                            ssPlugin << "obfs-local";
                            ssPlugin << "obfs=" + Node2QString(pluginOpts_n["mode"]);
                            ssPlugin << "obfs-host=" + Node2QString(pluginOpts_n["host"]);
                        } else if (plugin == "v2ray-plugin") {
                            auto mode = Node2QString(pluginOpts_n["mode"]);
                            auto host = Node2QString(pluginOpts_n["host"]);
                            auto path = Node2QString(pluginOpts_n["path"]);
                            ssPlugin << "v2ray-plugin";
                            if (!mode.isEmpty() && mode != "websocket") ssPlugin << "mode=" + mode;
                            if (Node2Bool(pluginOpts_n["tls"])) ssPlugin << "tls";
                            if (!host.isEmpty()) ssPlugin << "host=" + host;
                            if (!path.isEmpty()) ssPlugin << "path=" + path;
                            // clash only: skip-cert-verify
                            // clash only: headers
                            // clash: mux=?
                        }
                        bean->plugin = ssPlugin.join(";");
                    }

                    // sing-mux
                    auto smux = NodeChild(proxy, {"smux"});
                    if (!smux.is_null() && Node2Bool(smux["enabled"])) bean->mux_state = 1;
                } else if (type == "socks" || type == "http") {
                    auto bean = ent->SocksHTTPBean();
                    bean->username = Node2QString(proxy["username"]);
                    bean->password = Node2QString(proxy["password"]);
                    if (type == "http" && Node2Bool(proxy["tls"])) {
                        bean->stream->security = "tls";
                        if (Node2Bool(proxy["skip-cert-verify"])) bean->stream->allow_insecure = true;
                        bean->stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]), Node2QString(proxy["servername"]));
                        bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
                        bean->stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
                        if (bean->stream->utlsFingerprint.isEmpty()) {
                            bean->stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
                        }

                        auto reality = NodeChild(proxy, {"reality-opts"});
                        if (reality.is_mapping()) {
                            bean->stream->reality_pbk = Node2QString(reality["public-key"]);
                            bean->stream->reality_sid = Node2QString(reality["short-id"]);
                        }
                    }
                } else if (type == "trojan" || type == "vless") {
                    needFix = true;
                    auto bean = ent->TrojanVLESSBean();
                    if (type == "vless") {
                        bean->flow = Node2QString(proxy["flow"]);
                        bean->password = Node2QString(proxy["uuid"]);
                        // meta packet encoding
                        if (Node2Bool(proxy["packet-addr"])) {
                            bean->stream->packet_encoding = "packetaddr";
                        } else {
                            // For VLESS, default to use xudp
                            bean->stream->packet_encoding = "xudp";
                        }
                    } else {
                        bean->password = Node2QString(proxy["password"]);
                    }
                    bean->stream->security = "tls";
                    bean->stream->network = Node2QString(proxy["network"], "tcp");
                    bean->stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]), Node2QString(proxy["servername"]));
                    bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
                    bean->stream->allow_insecure = Node2Bool(proxy["skip-cert-verify"]);
                    bean->stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
                    if (bean->stream->utlsFingerprint.isEmpty()) {
                        bean->stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
                    }

                    // sing-mux
                    auto smux = NodeChild(proxy, {"smux"});
                    if (!smux.is_null() && Node2Bool(smux["enabled"])) bean->mux_state = 1;

                    // opts
                    auto ws = NodeChild(proxy, {"ws-opts", "ws-opt"});
                    if (ws.is_mapping()) {
                        auto headers = ws["headers"];
                        if (headers.is_mapping()) {
                            for (auto header: headers.as_map()) {
                                if (Node2QString(header.first).toLower() == "host") {
                                    bean->stream->host = Node2QString(header.second);
                                    break;
                                }
                            }
                        }
                        bean->stream->path = Node2QString(ws["path"]);
                        bean->stream->ws_early_data_length = Node2Int(ws["max-early-data"]);
                        bean->stream->ws_early_data_name = Node2QString(ws["early-data-header-name"]);
                    }

                    auto grpc = NodeChild(proxy, {"grpc-opts", "grpc-opt"});
                    if (grpc.is_mapping()) {
                        bean->stream->path = Node2QString(grpc["grpc-service-name"]);
                    }

                    auto reality = NodeChild(proxy, {"reality-opts"});
                    if (reality.is_mapping()) {
                        bean->stream->reality_pbk = Node2QString(reality["public-key"]);
                        bean->stream->reality_sid = Node2QString(reality["short-id"]);
                    }
                } else if (type == "vmess") {
                    needFix = true;
                    auto bean = ent->VMessBean();
                    bean->uuid = Node2QString(proxy["uuid"]);
                    bean->aid = Node2Int(proxy["alterId"]);
                    bean->security = Node2QString(proxy["cipher"], bean->security);
                    bean->stream->network = Node2QString(proxy["network"], "tcp").replace("h2", "http");
                    bean->stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]), Node2QString(proxy["servername"]));
                    bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
                    if (Node2Bool(proxy["tls"])) bean->stream->security = "tls";
                    if (Node2Bool(proxy["skip-cert-verify"])) bean->stream->allow_insecure = true;
                    bean->stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
                    if (bean->stream->utlsFingerprint.isEmpty()) {
                        bean->stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
                    }

                    // sing-mux
                    auto smux = NodeChild(proxy, {"smux"});
                    if (!smux.is_null() && Node2Bool(smux["enabled"])) bean->mux_state = 1;

                    // meta packet encoding
                    if (Node2Bool(proxy["xudp"])) bean->stream->packet_encoding = "xudp";
                    if (Node2Bool(proxy["packet-addr"])) bean->stream->packet_encoding = "packetaddr";

                    // opts
                    auto ws = NodeChild(proxy, {"ws-opts", "ws-opt"});
                    if (ws.is_mapping()) {
                        auto headers = ws["headers"];
                        if (headers.is_mapping()) {
                            for (auto header: headers.as_map()) {
                                if (Node2QString(header.first).toLower() == "host") {
                                    bean->stream->host = Node2QString(header.second);
                                    break;
                                }
                            }
                        }
                        bean->stream->path = Node2QString(ws["path"]);
                        bean->stream->ws_early_data_length = Node2Int(ws["max-early-data"]);
                        bean->stream->ws_early_data_name = Node2QString(ws["early-data-header-name"]);
                        // for Xray
                        if (Node2QString(ws["early-data-header-name"]) == "Sec-WebSocket-Protocol") {
                            bean->stream->path += "?ed=" + Node2QString(ws["max-early-data"]);
                        }
                    }

                    auto grpc = NodeChild(proxy, {"grpc-opts", "grpc-opt"});
                    if (grpc.is_mapping()) {
                        bean->stream->path = Node2QString(grpc["grpc-service-name"]);
                    }

                    auto h2 = NodeChild(proxy, {"h2-opts", "h2-opt"});
                    if (h2.is_mapping()) {
                        auto hosts = h2["host"];
                        for (auto host: hosts) {
                            bean->stream->host = Node2QString(host);
                            break;
                        }
                        bean->stream->path = Node2QString(h2["path"]);
                    }
                    auto tcp_http = NodeChild(proxy, {"http-opts", "http-opt"});
                    if (tcp_http.is_mapping()) {
                        bean->stream->network = "tcp";
                        bean->stream->header_type = "http";
                        auto headers = tcp_http["headers"];
                        if (headers.is_mapping()) {
                            for (auto header: headers.as_map()) {
                                if (Node2QString(header.first).toLower() == "host") {
                                    bean->stream->host = Node2QString(header.second[0]);
                                    break;
                                }
                            }
                        }
                        auto paths = tcp_http["path"];
                        for (auto path: paths) {
                            bean->stream->path = Node2QString(path);
                            break;
                        }
                    }
                } else if (type == "anytls") {
                    needFix = true;
                    auto bean = ent->AnyTLSBean();
                    bean->password = Node2QString(proxy["password"]);
                    if (Node2Bool(proxy["tls"])) bean->stream->security = "tls";
                    if (Node2Bool(proxy["skip-cert-verify"])) bean->stream->allow_insecure = true;
                    bean->stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]), Node2QString(proxy["servername"]));
                    bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
                    bean->stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
                    if (bean->stream->utlsFingerprint.isEmpty()) {
                        bean->stream->utlsFingerprint = Configs::dataStore->utlsFingerprint;
                    }

                    auto reality = NodeChild(proxy, {"reality-opts"});
                    if (reality.is_mapping()) {
                        bean->stream->reality_pbk = Node2QString(reality["public-key"]);
                        bean->stream->reality_sid = Node2QString(reality["short-id"]);
                    }
                } else if (type == "hysteria") {
                    auto bean = ent->QUICBean();

                    bean->allowInsecure = Node2Bool(proxy["skip-cert-verify"]);
                    auto alpn = Node2QStringList(proxy["alpn"]);
                    bean->caText = Node2QString(proxy["ca-str"]);
                    if (!alpn.isEmpty()) bean->alpn = alpn[0];
                    bean->sni = Node2QString(proxy["sni"]);

                    auto auth_str = FIRST_OR_SECOND(Node2QString(proxy["auth_str"]), Node2QString(proxy["auth-str"]));
                    auto auth = Node2QString(proxy["auth"]);
                    if (!auth_str.isEmpty()) {
                        bean->authPayloadType = Configs::QUICBean::hysteria_auth_string;
                        bean->authPayload = auth_str;
                    }
                    if (!auth.isEmpty()) {
                        bean->authPayloadType = Configs::QUICBean::hysteria_auth_base64;
                        bean->authPayload = auth;
                    }
                    bean->obfsPassword = Node2QString(proxy["obfs"]);

                    if (Node2Bool(proxy["disable_mtu_discovery"]) || Node2Bool(proxy["disable-mtu-discovery"])) bean->disableMtuDiscovery = true;
                    bean->streamReceiveWindow = Node2Int(proxy["recv-window"]);
                    bean->connectionReceiveWindow = Node2Int(proxy["recv-window-conn"]);

                    auto upMbps = Node2QString(proxy["up"]).split(" ")[0].toInt();
                    auto downMbps = Node2QString(proxy["down"]).split(" ")[0].toInt();
                    if (upMbps > 0) bean->uploadMbps = upMbps;
                    if (downMbps > 0) bean->downloadMbps = downMbps;
                } else if (type == "hysteria2") {
                    auto bean = ent->QUICBean();

                    bean->allowInsecure = Node2Bool(proxy["skip-cert-verify"]);
                    bean->caText = Node2QString(proxy["ca-str"]);
                    bean->sni = Node2QString(proxy["sni"]);

                    bean->obfsPassword = Node2QString(proxy["obfs-password"]);
                    bean->password = Node2QString(proxy["password"]);

                    bean->uploadMbps = Node2QString(proxy["up"]).split(" ")[0].toInt();
                    bean->downloadMbps = Node2QString(proxy["down"]).split(" ")[0].toInt();
                } else if (type == "tuic") {
                    auto bean = ent->QUICBean();

                    bean->uuid = Node2QString(proxy["uuid"]);
                    bean->password = Node2QString(proxy["password"]);

                    if (Node2Int(proxy["heartbeat-interval"]) != 0) {
                        bean->heartbeat = Int2String(Node2Int(proxy["heartbeat-interval"])) + "ms";
                    }

                    bean->udpRelayMode = Node2QString(proxy["udp-relay-mode"], bean->udpRelayMode);
                    bean->congestionControl = Node2QString(proxy["congestion-controller"], bean->congestionControl);

                    bean->disableSni = Node2Bool(proxy["disable-sni"]);
                    bean->zeroRttHandshake = Node2Bool(proxy["reduce-rtt"]);
                    bean->allowInsecure = Node2Bool(proxy["skip-cert-verify"]);
                    bean->alpn = Node2QStringList(proxy["alpn"]).join(",");
                    bean->caText = Node2QString(proxy["ca-str"]);
                    bean->sni = Node2QString(proxy["sni"]);

                    if (Node2Bool(proxy["udp-over-stream"])) bean->uos = true;

                    if (!Node2QString(proxy["ip"]).isEmpty()) {
                        if (bean->sni.isEmpty()) bean->sni = bean->serverAddress;
                        bean->serverAddress = Node2QString(proxy["ip"]);
                    }
                } else {
                    continue;
                }

                if (needFix) RawUpdater_FixEnt(ent);
                updated_order += ent;
            }
        } catch (const fkyaml::exception &ex) {
            runOnUiThread([=,this] {
                MessageBoxWarning("YAML Exception", ex.what());
            });
        }
    }

    // 在新的 thread 运行
    void GroupUpdater::AsyncUpdate(const QString &str, int _sub_gid, const std::function<void()> &finish) {
        auto content = str.trimmed();
        bool asURL = false;
        bool createNewGroup = false;

        if (_sub_gid < 0 && (content.startsWith("http://") || content.startsWith("https://"))) {
            auto items = QStringList{
                QObject::tr("Add profiles to this group"),
                QObject::tr("Create new subscription group"),
            };
            bool ok;
            auto a = QInputDialog::getItem(nullptr,
                                           QObject::tr("url detected"),
                                           QObject::tr("%1\nHow to update?").arg(content),
                                           items, 0, false, &ok);
            if (!ok) return;
            asURL = true;
            if (items.indexOf(a) == 1) createNewGroup = true;
        }

        runOnNewThread([=,this] {
            auto gid = _sub_gid;
            if (createNewGroup) {
                auto group = Configs::ProfileManager::NewGroup();
                group->name = QUrl(str).host();
                group->url = str;
                Configs::profileManager->AddGroup(group);
                gid = group->id;
                MW_dialog_message("SubUpdater", "NewGroup");
            }
            Update(str, gid, asURL);
            emit asyncUpdateCallback(gid);
            if (finish != nullptr) finish();
        });
    }

    void GroupUpdater::Update(const QString &_str, int _sub_gid, bool _not_sub_as_url) {
        // 创建 rawUpdater
        Configs::dataStore->imported_count = 0;
        auto rawUpdater = std::make_unique<RawUpdater>();
        rawUpdater->gid_add_to = _sub_gid;

        // 准备
        QString sub_user_info;
        bool asURL = _sub_gid >= 0 || _not_sub_as_url; // 把 _str 当作 url 处理（下载内容）
        auto content = _str.trimmed();
        auto group = Configs::profileManager->GetGroup(_sub_gid);
        if (group != nullptr && group->archive) return;

        // 网络请求
        if (asURL) {
            auto groupName = group == nullptr ? content : group->name;
            MW_show_log(">>>>>>>> " + QObject::tr("Requesting subscription: %1").arg(groupName));

            auto resp = NetworkRequestHelper::HttpGet(content);
            if (!resp.error.isEmpty()) {
                MW_show_log("<<<<<<<< " + QObject::tr("Requesting subscription %1 error: %2").arg(groupName, resp.error + "\n" + resp.data));
                return;
            }

            content = resp.data;
            sub_user_info = NetworkRequestHelper::GetHeader(resp.header, "Subscription-UserInfo");

            MW_show_log("<<<<<<<< " + QObject::tr("Subscription request fininshed: %1").arg(groupName));
        }

        QList<std::shared_ptr<Configs::ProxyEntity>> in;          // 更新前
        QList<std::shared_ptr<Configs::ProxyEntity>> out_all;     // 更新前 + 更新后
        QList<std::shared_ptr<Configs::ProxyEntity>> out;         // 更新后
        QList<std::shared_ptr<Configs::ProxyEntity>> only_in;     // 只在更新前有的
        QList<std::shared_ptr<Configs::ProxyEntity>> only_out;    // 只在更新后有的
        QList<std::shared_ptr<Configs::ProxyEntity>> update_del;  // 更新前后都有的，需要删除的新配置
        QList<std::shared_ptr<Configs::ProxyEntity>> update_keep; // 更新前后都有的，被保留的旧配置

        if (group != nullptr) {
            in = group->GetProfileEnts();
            group->sub_last_update = QDateTime::currentMSecsSinceEpoch() / 1000;
            group->info = sub_user_info;
            group->Save();
            //
            if (Configs::dataStore->sub_clear) {
                MW_show_log(QObject::tr("Clearing servers..."));
                Configs::profileManager->BatchDeleteProfiles(group->profiles);
            }
        }

        MW_show_log(">>>>>>>> " + QObject::tr("Processing subscription data..."));
        rawUpdater->update(content);
        Configs::profileManager->AddProfileBatch(rawUpdater->updated_order, rawUpdater->gid_add_to);
        MW_show_log(">>>>>>>> " + QObject::tr("Process complete, applying..."));

        if (group != nullptr) {
            out_all = group->GetProfileEnts();

            QString change_text;

            if (Configs::dataStore->sub_clear) {
                // all is new profile
                for (const auto &ent: out_all) {
                    change_text += "[+] " + ent->bean->DisplayTypeAndName() + "\n";
                }
            } else {
                // find and delete not updated profile by ProfileFilter
                Configs::ProfileFilter::OnlyInSrc_ByPointer(out_all, in, out);
                Configs::ProfileFilter::OnlyInSrc(in, out, only_in);
                Configs::ProfileFilter::OnlyInSrc(out, in, only_out);
                Configs::ProfileFilter::Common(in, out, update_keep, update_del, false);
                QString notice_added;
                QString notice_deleted;
                if (only_out.size() < 1000)
                {
                    for (const auto &ent: only_out) {
                        notice_added += "[+] " + ent->bean->DisplayTypeAndName() + "\n";
                    }
                } else
                {
                    notice_added += QString("[+] ") + "added " + Int2String(only_out.size()) + "\n";
                }
                if (only_in.size() < 1000)
                {
                    for (const auto &ent: only_in) {
                        notice_deleted += "[-] " + ent->bean->DisplayTypeAndName() + "\n";
                    }
                } else
                {
                    notice_deleted += QString("[-] ") + "deleted " + Int2String(only_in.size()) + "\n";
                }


                // sort according to order in remote
                group->profiles.clear();
                for (const auto &ent: rawUpdater->updated_order) {
                    auto deleted_index = update_del.indexOf(ent);
                    if (deleted_index >= 0) {
                        if (deleted_index >= update_keep.count()) continue; // should not happen
                        const auto& ent2 = update_keep[deleted_index];
                        group->profiles.append(ent2->id);
                    } else {
                        group->profiles.append(ent->id);
                    }
                }
                group->Save();

                // cleanup
                QList<int> del_ids;
                for (const auto &ent: out_all) {
                    if (!group->HasProfile(ent->id)) {
                        del_ids.append(ent->id);
                    }
                }
                Configs::profileManager->BatchDeleteProfiles(del_ids);

                change_text = "\n" + QObject::tr("Added %1 profiles:\n%2\nDeleted %3 Profiles:\n%4")
                                         .arg(only_out.length())
                                         .arg(notice_added)
                                         .arg(only_in.length())
                                         .arg(notice_deleted);
                if (only_out.length() + only_in.length() == 0) change_text = QObject::tr("Nothing");
            }

            MW_show_log("<<<<<<<< " + QObject::tr("Change of %1:").arg(group->name) + "\n" + change_text);
            MW_dialog_message("SubUpdater", "finish-dingyue");
        } else {
            Configs::dataStore->imported_count = rawUpdater->updated_order.count();
            MW_dialog_message("SubUpdater", "finish");
        }
    }
} // namespace Subscription

bool UI_update_all_groups_Updating = false;

#define should_skip_group(g) (g == nullptr || g->url.isEmpty() || g->archive || (onlyAllowed && g->skip_auto_update))

void serialUpdateSubscription(const QList<int> &groupsTabOrder, int _order, bool onlyAllowed) {
    if (_order >= groupsTabOrder.size()) {
        UI_update_all_groups_Updating = false;
        return;
    }

    // calculate this group
    auto group = Configs::profileManager->GetGroup(groupsTabOrder[_order]);
    if (group == nullptr || should_skip_group(group)) {
        serialUpdateSubscription(groupsTabOrder, _order + 1, onlyAllowed);
        return;
    }

    int nextOrder = _order + 1;
    while (nextOrder < groupsTabOrder.size()) {
        auto nextGid = groupsTabOrder[nextOrder];
        auto nextGroup = Configs::profileManager->GetGroup(nextGid);
        if (!should_skip_group(nextGroup)) {
            break;
        }
        nextOrder += 1;
    }

    // Async update current group
    UI_update_all_groups_Updating = true;
    Subscription::groupUpdater->AsyncUpdate(group->url, group->id, [=] {
        serialUpdateSubscription(groupsTabOrder, nextOrder, onlyAllowed);
    });
}

void UI_update_all_groups(bool onlyAllowed) {
    if (UI_update_all_groups_Updating) {
        MW_show_log("The last subscription update has not exited.");
        return;
    }

    auto groupsTabOrder = Configs::profileManager->groupsTabOrder;
    serialUpdateSubscription(groupsTabOrder, 0, onlyAllowed);
}
