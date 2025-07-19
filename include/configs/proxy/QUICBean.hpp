#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class QUICBean : public AbstractBean {
    public:
        static constexpr int proxy_Hysteria = 0;
        static constexpr int proxy_TUIC = 1;
        static constexpr int proxy_Hysteria2 = 3;
        int proxy_type = proxy_Hysteria;

        bool forceExternal = false;

        // Hysteria 1

        static constexpr int hysteria_protocol_udp = 0;
        static constexpr int hysteria_protocol_facktcp = 1;
        static constexpr int hysteria_protocol_wechat_video = 2;
        int hyProtocol = 0;

        static constexpr int hysteria_auth_none = 0;
        static constexpr int hysteria_auth_string = 1;
        static constexpr int hysteria_auth_base64 = 2;
        int authPayloadType = 0;
        QString authPayload = "";

        // Hysteria 1&2

        QString obfsPassword = "";

        int uploadMbps = 100;
        int downloadMbps = 100;

        qint64 streamReceiveWindow = 0;
        qint64 connectionReceiveWindow = 0;
        bool disableMtuDiscovery = false;

        // Hysteria 2
        QStringList serverPorts;
        QString hop_interval;

        // TUIC

        QString uuid = "";
        QString congestionControl = "bbr";
        QString udpRelayMode = "native";
        bool zeroRttHandshake = false;
        QString heartbeat = "10s";
        bool uos = false;

        // HY2&TUIC

        QString password = "";

        // TLS

        bool allowInsecure = false;
        QString sni = "";
        QString alpn = "";
        QString caText = "";
        bool disableSni = false;

        explicit QUICBean(int _proxy_type) : AbstractBean(0) {
            proxy_type = _proxy_type;
            if (proxy_type == proxy_Hysteria || proxy_type == proxy_Hysteria2) {
                _add(new configItem("authPayload", &authPayload, itemType::string));
                _add(new configItem("obfsPassword", &obfsPassword, itemType::string));
                _add(new configItem("uploadMbps", &uploadMbps, itemType::integer));
                _add(new configItem("downloadMbps", &downloadMbps, itemType::integer));
                _add(new configItem("streamReceiveWindow", &streamReceiveWindow, itemType::integer64));
                _add(new configItem("connectionReceiveWindow", &connectionReceiveWindow, itemType::integer64));
                _add(new configItem("disableMtuDiscovery", &disableMtuDiscovery, itemType::boolean));
                if (proxy_type == proxy_Hysteria) { // hy1
                    _add(new configItem("authPayloadType", &authPayloadType, itemType::integer));
                    _add(new configItem("protocol", &hyProtocol, itemType::integer));
                } else { // hy2
                    uploadMbps = 0;
                    downloadMbps = 0;
                    _add(new configItem("password", &password, itemType::string));
                    _add(new configItem("server_ports", &serverPorts, itemType::stringList));
                    _add(new configItem("hop_interval", &hop_interval, itemType::string));
                }
            } else if (proxy_type == proxy_TUIC) {
                _add(new configItem("uuid", &uuid, itemType::string));
                _add(new configItem("password", &password, itemType::string));
                _add(new configItem("congestionControl", &congestionControl, itemType::string));
                _add(new configItem("udpRelayMode", &udpRelayMode, itemType::string));
                _add(new configItem("zeroRttHandshake", &zeroRttHandshake, itemType::boolean));
                _add(new configItem("heartbeat", &heartbeat, itemType::string));
                _add(new configItem("uos", &uos, itemType::boolean));
            }
            _add(new configItem("forceExternal", &forceExternal, itemType::boolean));
            // TLS
            _add(new configItem("allowInsecure", &allowInsecure, itemType::boolean));
            _add(new configItem("sni", &sni, itemType::string));
            _add(new configItem("alpn", &alpn, itemType::string));
            _add(new configItem("caText", &caText, itemType::string));
            _add(new configItem("disableSni", &disableSni, itemType::boolean));
        };

        QString DisplayAddress() override {
            return ::DisplayAddress(serverAddress, serverPort);
        }

        QString DisplayType() override {
            if (proxy_type == proxy_TUIC) {
                return "TUIC";
            } else if (proxy_type == proxy_Hysteria) {
                return "Hysteria1";
            } else {
                return "Hysteria2";
            }
        };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs