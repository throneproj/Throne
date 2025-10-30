#include "include/configs/outbounds/tuic.h"

namespace Configs {
    bool tuic::ParseFromLink(const QString& link) { return false; }
    bool tuic::ParseFromJson(const QJsonObject& object) { return false; }
    QString tuic::ExportToLink() { return {}; }
    QJsonObject tuic::ExportToJson() { return {}; }
    BuildResult tuic::Build() { return {}; }

    QString tuic::DisplayAddress() { return {}; }
    QString tuic::DisplayName() { return {}; }
    QString tuic::DisplayType() { return {}; }
    QString tuic::DisplayTypeAndName() { return {}; }
    bool tuic::IsEndpoint() { return false; }
}


