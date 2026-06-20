#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class Custom : public outbound
    {
    public:
        static constexpr auto CustomOutbound = "outbound";
        static constexpr auto CustomFullConfig = "fullconfig";
        static constexpr auto CustomXrayOutbound = "xrayoutbound";
        static constexpr auto CustomXrayFullConfig = "xrayfullconfig";

        QString config;
        QString type;

        // Transient bridge fields, populated during build for CustomXrayFullConfig.
        // Build() returns a sing-box socks outbound pointing at this port; the
        // generated Xray config receives a matching socks inbound.
        int bridgePort = 0;
        QString bridgeAuth;
        QString bridgeHost = "127.0.0.1";

        QJsonObject XrayDisplayOutbound() const {
            auto obj = QString2QJsonObject(config);
            if (type == CustomXrayOutbound) return obj;
            if (type != CustomXrayFullConfig) return {};
            for (const auto &item : obj["outbounds"].toArray()) {
                if (!item.isObject()) continue;
                auto outbound = item.toObject();
                const auto protocol = outbound["protocol"].toString();
                if (protocol == "freedom" || protocol == "blackhole" || protocol == "dns" || protocol == "loopback") continue;
                return outbound;
            }
            return {};
        }

        QJsonObject XrayDisplayServer() const {
            auto settings = XrayDisplayOutbound()["settings"].toObject();
            if (settings.contains("vnext")) return settings["vnext"].toArray().first().toObject();
            if (settings.contains("servers")) return settings["servers"].toArray().first().toObject();
            if (settings.contains("address")) return settings;
            return {};
        }

        QString XrayProtocolDisplayType() const {
            auto protocol = XrayDisplayOutbound()["protocol"].toString();
            if (import_source == "xrayjson") {
                if (protocol == "vless") return "VLESS (Xray)";
                if (protocol == "hysteria" || protocol == "hysteria2") return "Hysteria";
                if (!protocol.isEmpty()) protocol[0] = protocol[0].toUpper();
                return protocol.isEmpty() ? "Xray" : protocol;
            }
            if (!protocol.isEmpty()) protocol[0] = protocol[0].toUpper();
            return protocol.isEmpty() ? "Custom Xray" : "Custom Xray " + protocol;
        }

        bool ParseFromJson(const QJsonObject &object) override {
            if (object.isEmpty()) return false;
            if (object.contains("name")) name = object["name"].toString();
            if (object.contains("import_source")) import_source = object["import_source"].toString();
            if (object.contains("subtype")) type = object["subtype"].toString();
            if (object.contains("config")) config = object["config"].toString();
            return true;
        }

        QJsonObject ExportToJson() override {
            QJsonObject object;
            object["name"] = name;
            object["type"] = "custom";
            if (!import_source.isEmpty()) object["import_source"] = import_source;
            object["subtype"] = type;
            object["config"] = config;
            return object;
        }

        QString GetAddress() override
        {
            if (type == CustomOutbound) {
                auto obj = QString2QJsonObject(config);
                return obj["server"].toString();
            }
            if (type == CustomXrayOutbound || type == CustomXrayFullConfig) {
                return XrayDisplayServer()["address"].toString();
            }
            return {};
        }

        QString DisplayAddress() override
        {
            if (type == CustomOutbound) {
                auto obj = QString2QJsonObject(config);
                return ::DisplayAddress(obj["server"].toString(), obj["server_port"].toInt());
            }
            if (type == CustomXrayOutbound || type == CustomXrayFullConfig) {
                auto server = XrayDisplayServer();
                if (!server.isEmpty()) return ::DisplayAddress(server["address"].toString(), server["port"].toInt());
            }
            return {};
        }

        QString DisplayType() override
        {
            if (type == CustomOutbound) {
                auto outboundType = QString2QJsonObject(config)["type"].toString();
                if (!outboundType.isEmpty()) outboundType[0] = outboundType[0].toUpper();
                return outboundType.isEmpty() ? "Custom Outbound" : "Custom " + outboundType + " Outbound";
            } else if (type == CustomFullConfig) {
                return "Custom Config";
            } else if (type == CustomXrayOutbound) {
                auto displayType = XrayProtocolDisplayType();
                return import_source == "xrayjson" ? displayType : displayType + " Outbound";
            } else if (type == CustomXrayFullConfig) {
                auto displayType = XrayProtocolDisplayType();
                return import_source == "xrayjson" ? displayType : displayType + " Config";
            }
            return type;
        };

        bool IsXray() override { return type == CustomXrayOutbound; }

        bool IsXrayFullConfig() override { return type == CustomXrayFullConfig; }

        BuildResult Build() override
        {
            if (type == CustomXrayFullConfig) {
                return {QJsonObject{
                            {"type", "socks"},
                            {"server", bridgeHost},
                            {"server_port", bridgePort},
                            {"username", bridgeAuth},
                            {"password", bridgeAuth},
                        }, ""};
            }
            if (type == CustomXrayOutbound) {
                // Dummy sing-box outbound so sing-box CheckConfig accepts the
                // config during validation. The real outbound is in BuildXray().
                return {QJsonObject{
                            {"type", "socks"},
                            {"server", "127.0.0.1"},
                        }, ""};
            }
            return {QString2QJsonObject(config), ""};
        }

        BuildResult BuildXray() override
        {
            if (type == CustomXrayOutbound) {
                return {QString2QJsonObject(config), ""};
            }
            return {};
        }
    };
}
