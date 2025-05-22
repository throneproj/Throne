#pragma once

#include "AbstractBean.hpp"

namespace NekoGui_fmt {
    class ExtraCoreBean : public AbstractBean {
    public:
        QString socksAddress = "127.0.0.1";
        int socksPort;
        QString extraCorePath;
        QString extraCoreArgs;
        QString extraCoreConf;
        bool noLogs;

        ExtraCoreBean() : AbstractBean(0) {
            _add(new configItem("socks_address", &socksAddress, itemType::string));
            _add(new configItem("socks_port", &socksPort, itemType::integer));
            _add(new configItem("extra_core_path", &extraCorePath, itemType::string));
            _add(new configItem("extra_core_args", &extraCoreArgs, itemType::string));
            _add(new configItem("extra_core_conf", &extraCoreConf, itemType::string));
            _add(new configItem("no_logs", &noLogs, itemType::boolean));
        };

        QString DisplayType() override { return "ExtraCore"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
}
