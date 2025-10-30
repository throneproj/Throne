#include "include/configs/common/Outbound.h"

namespace Configs {
    bool OutboundCommons::ParseFromLink(const QString& link) { return false; }
    bool OutboundCommons::ParseFromJson(const QJsonObject& object) { return false; }
    QString OutboundCommons::ExportToLink() { return {}; }
    QJsonObject OutboundCommons::ExportToJson() { return {}; }
    BuildResult OutboundCommons::Build() { return {}; }
}


