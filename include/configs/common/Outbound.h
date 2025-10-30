#pragma once
#include "DialFields.h"
#include "include/configs/baseConfig.h"

namespace Configs
{
    class outbound : public baseConfig
    {
    public:
        QString name;
        QString server;
        int server_port = 0;
        bool invalid = false;
        std::shared_ptr<DialFields> dialFields = std::make_shared<DialFields>();

        outbound()
        {
            _add(new configItem("name", &name, string));
            _add(new configItem("server", &server, string));
            _add(new configItem("server_port", &server_port, integer));
            _add(new configItem("dial_fields", dynamic_cast<JsonStore *>(dialFields.get()), jsonStore));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
