#pragma once

#include "AbstractBean.hpp"
#include "V2RayStreamSettings.hpp"
#include "Preset.hpp"

namespace Configs {
    class VMessBean : public AbstractBean {
    public:
        QString uuid = "";
        int aid = 0;
        QString security = "auto";

        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        VMessBean() : AbstractBean(0) {
            _add(new configItem("id", &uuid, itemType::string));
            _add(new configItem("aid", &aid, itemType::integer));
            _add(new configItem("sec", &security, itemType::string));
            _add(new configItem("stream", dynamic_cast<JsonStore *>(stream.get()), itemType::jsonStore));
        };

        QString DisplayType() override { return "VMess"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
} // namespace Configs
