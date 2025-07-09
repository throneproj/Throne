#pragma once

#include "AbstractBean.hpp"

namespace NekoGui_fmt {
    class CustomBean : public AbstractBean {
    public:
        QString core;
        QList<QString> command;
        QString config_suffix;
        QString config_simple;
        int mapping_port = 0;
        int socks_port = 0;

        CustomBean() : AbstractBean(0) {
            _add(new configItem("core", &core, itemType::string));
            _add(new configItem("cmd", &command, itemType::stringList));
            _add(new configItem("cs", &config_simple, itemType::string));
            _add(new configItem("cs_suffix", &config_suffix, itemType::string));
            _add(new configItem("mapping_port", &mapping_port, itemType::integer));
            _add(new configItem("socks_port", &socks_port, itemType::integer));
        };

        QString DisplayType() override {
            if (core == "internal") {
                auto type = QString2QJsonObject(config_simple)["type"].toString();
                if (!type.isEmpty()) type[0] = type[0].toUpper();
                return type.isEmpty() ? "Custom Outbound" : "Custom " + type + " Outbound";
            } else if (core == "internal-full") {
                return "Custom Config";
            }
            return core;
        };

        QString DisplayCoreType() override { return software_core_name; };

        QString DisplayAddress() override {
            if (core == "internal") {
                auto obj = QString2QJsonObject(config_simple);
                return ::DisplayAddress(obj["server"].toString(), obj["server_port"].toInt());
            } else if (core == "internal-full") {
                return {};
            }
            return AbstractBean::DisplayAddress();
        };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;
    };
} // namespace NekoGui_fmt