#include "include/configs/outbounds/socks.h"

namespace Configs {
    bool socks::ParseFromLink(const QString& link) { return false; }
    bool socks::ParseFromJson(const QJsonObject& object) { return false; }
    QString socks::ExportToLink() { return {}; }
    QJsonObject socks::ExportToJson() { return {}; }
    BuildResult socks::Build() { return {}; }

    QString socks::DisplayAddress() { return {}; }
    QString socks::DisplayName() { return {}; }
    QString socks::DisplayType() { return {}; }
    QString socks::DisplayTypeAndName() { return {}; }
    bool socks::IsEndpoint() { return false; }
}


