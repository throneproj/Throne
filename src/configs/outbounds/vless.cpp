#include "include/configs/outbounds/vless.h"

namespace Configs {
    bool vless::ParseFromLink(const QString& link) { return false; }
    bool vless::ParseFromJson(const QJsonObject& object) { return false; }
    QString vless::ExportToLink() { return {}; }
    QJsonObject vless::ExportToJson() { return {}; }
    BuildResult vless::Build() { return {}; }

    QString vless::DisplayAddress() { return {}; }
    QString vless::DisplayName() { return {}; }
    QString vless::DisplayType() { return {}; }
    QString vless::DisplayTypeAndName() { return {}; }
    bool vless::IsEndpoint() { return false; }
}


