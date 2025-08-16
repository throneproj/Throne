#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class AnyTLSBean : public AbstractBean {
    public:
        QString password = "";
        int idle_session_check_interval = 30;
        int idle_session_timeout = 30;
        int min_idle_session = 0;

        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        AnyTLSBean() : AbstractBean(0) {
            _add(new configItem("password", &password, itemType::string));
            _add(new configItem("idle_session_check_interval", &idle_session_check_interval, itemType::integer));
            _add(new configItem("idle_session_timeout", &idle_session_timeout, itemType::integer));
            _add(new configItem("min_idle_session", &min_idle_session, itemType::integer));
            _add(new configItem("stream", dynamic_cast<JsonStore *>(stream.get()), itemType::jsonStore));
        };

        QString DisplayType() override { return "AnyTLS"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs
