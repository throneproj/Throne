#pragma once

#include <QString>

#include "generate.h"
#include "include/global/ConfigItem.hpp"

namespace Configs
{
    struct BuildResult {
        QJsonObject object;
        QString error;
    };

    class baseConfig : public JsonStore
    {
    public:
        virtual bool ParseFromLink(const QString& link);

        virtual bool ParseFromJson(const QJsonObject& object);

        virtual QString ExportToLink() {
            return {};
        }

        virtual QJsonObject ExportToJson() {
            return {};
        }

        virtual BuildResult Build();
    };

    class outboundMeta
    {
        public:
        virtual ~outboundMeta() = default;

        void ResolveDomainToIP(const std::function<void()> &onFinished);

        virtual QString DisplayAddress();

        virtual QString DisplayName();

        virtual QString DisplayType() { return {}; };

        virtual QString DisplayTypeAndName();

        virtual bool IsEndpoint() { return false; };
    };
}
