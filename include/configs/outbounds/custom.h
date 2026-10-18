#pragma once
#include <QJsonArray>
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class Custom : public outbound
    {
    public:
        bool HasServerEndpoint() override { return false; }

        static constexpr auto CustomOutbound = "outbound";
        static constexpr auto CustomFullConfig = "fullconfig";
        static constexpr auto CustomXrayOutbound = "xrayoutbound";
        static constexpr auto CustomXrayFullConfig = "xrayfullconfig";

        QString config;
        QString type;

        // Transient bridge fields: Build() emits a socks outbound on this port; Xray gets the matching inbound.
        int bridgePort = 0;
        QString bridgeAuth;
        QString bridgeHost = "127.0.0.1";

        bool ParseFromJson(const QJsonObject &object) override {
            if (object.isEmpty()) return false;
            if (object.contains("name")) name = object["name"].toString();
            if (object.contains("subtype")) type = object["subtype"].toString().trimmed();
            if (object.contains("config")) config = object["config"].toString();
            return true;
        }

        QJsonObject ExportToJson() override {
            QJsonObject object;
            object["name"] = name;
            object["type"] = "custom";
            object["subtype"] = type;
            object["config"] = config;
            return object;
        }

        QString GetAddress() override;

        QString DisplayAddress() override;

        QString DisplayType() override
        {
            if (type == CustomOutbound) {
                auto outboundType = QString2QJsonObject(config)["type"].toString();
                if (!outboundType.isEmpty()) outboundType[0] = outboundType[0].toUpper();
                return outboundType.isEmpty() ? "Custom Outbound" : "Custom " + outboundType + " Outbound";
            } else if (type == CustomFullConfig) {
                return "Custom Config";
            } else if (type == CustomXrayOutbound) {
                auto protocol = QString2QJsonObject(config)["protocol"].toString();
                if (!protocol.isEmpty()) protocol[0] = protocol[0].toUpper();
                return protocol.isEmpty() ? "Custom Xray Outbound" : "Custom Xray " + protocol + " Outbound";
            } else if (type == CustomXrayFullConfig) {
                return "Custom Xray Config";
            }
            return type;
        };

        SecurityInfo GetSecurity() override;

        QJsonObject ExportIdentity() override;

        void StripForeignState(Validation::Issues &issues) override;

        void Validate(Validation::Issues &issues) override;

        bool IsEndpoint() override
        {
            // Only raw sing-box outbound JSON can describe an endpoint.
            if (type != CustomOutbound) return false;
            const auto t = QString2QJsonObject(config)["type"].toString();
            return t == "wireguard" || t == "tailscale" || t == "masque";
        }

        bool IsXray() override { return type == CustomXrayOutbound; }

        bool IsXrayFullConfig() override { return type == CustomXrayFullConfig; }

        // Raw addresses (callers filter literal IPs) for sing-box's direct-DNS carve-out.
        QStringList GetXrayFullConfigServerDomains() {
            QStringList domains;
            if (type != CustomXrayFullConfig) return domains;
            const auto outbounds = QString2QJsonObject(config)["outbounds"].toArray();
            for (const auto &v : outbounds) {
                auto settings = v.toObject()["settings"].toObject();
                auto collect = [&](const QString &key) {
                    for (const auto &s : settings[key].toArray()) {
                        auto addr = s.toObject()["address"].toString();
                        if (!addr.isEmpty()) domains << addr;
                    }
                };
                if (settings.contains("vnext")) collect("vnext");
                if (settings.contains("servers")) collect("servers");
                if (settings.contains("address")) {
                    auto addr = settings["address"].toString();
                    if (!addr.isEmpty()) domains << addr;
                }
            }
            return domains;
        }

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
                // Dummy outbound so sing-box CheckConfig passes; the real one is in BuildXray().
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
                // Domain resolution is wired on at instance creation (ThroneWiring), not as sockopt.domainStrategy.
                return {QString2QJsonObject(config), ""};
            }
            return {};
        }
    };

    namespace Checks {
        // Valid JSON (line and column on a syntax error) with what the subtype needs: a type, a protocol or an outbounds list.
        QString CustomConfig(const QString &subtype, const QString &text);
    }

    // Removes every spelling of masterKeyLog from an Xray stream's TLS and REALITY settings and its XHTTP download legs; true when any went.
    bool StripXrayStreamKeyLog(QJsonObject &stream, const QString &at, Validation::Issues &issues);
}
