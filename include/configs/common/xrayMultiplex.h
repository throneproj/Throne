#pragma once
#include "include/configs/baseConfig.h"

namespace Configs {
    class xrayMultiplex : public baseConfig {
        public:
        bool enabled = false;
        bool useDefault = true;
        int concurrency = 0;
        int xudpConcurrency = 16;

        int getMuxState() {
            if (enabled) return 1;
            if (!useDefault) return 2;
            return 0;
        }

        void saveMuxState(int state) {
            useDefault = false;
            if (state == 1) {
                enabled = true;
                return;
            }
            enabled = false;
            if (state == 0) useDefault = true;
        }

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
