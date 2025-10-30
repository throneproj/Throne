#include "include/configs/outbounds/hysteria2.h"

namespace Configs {
    bool hysteria2::ParseFromLink(const QString& link) { return false; }
    bool hysteria2::ParseFromJson(const QJsonObject& object) { return false; }
    QString hysteria2::ExportToLink() { return {}; }
    QJsonObject hysteria2::ExportToJson() { return {}; }
    BuildResult hysteria2::Build() { return {}; }

    QString hysteria2::DisplayAddress() { return {}; }
    QString hysteria2::DisplayName() { return {}; }
    QString hysteria2::DisplayType() { return {}; }
    QString hysteria2::DisplayTypeAndName() { return {}; }
    bool hysteria2::IsEndpoint() { return false; }
}


