#include "include/configs/outbounds/hysteria.h"

namespace Configs {
    bool hysteria::ParseFromLink(const QString& link) { return false; }
    bool hysteria::ParseFromJson(const QJsonObject& object) { return false; }
    QString hysteria::ExportToLink() { return {}; }
    QJsonObject hysteria::ExportToJson() { return {}; }
    BuildResult hysteria::Build() { return {}; }

    QString hysteria::DisplayAddress() { return {}; }
    QString hysteria::DisplayName() { return {}; }
    QString hysteria::DisplayType() { return {}; }
    QString hysteria::DisplayTypeAndName() { return {}; }
    bool hysteria::IsEndpoint() { return false; }
}


