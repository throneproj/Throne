#include "include/configs/outbounds/shadowsocks.h"

namespace Configs {
    bool shadowsocks::ParseFromLink(const QString& link) { return false; }
    bool shadowsocks::ParseFromJson(const QJsonObject& object) { return false; }
    QString shadowsocks::ExportToLink() { return {}; }
    QJsonObject shadowsocks::ExportToJson() { return {}; }
    BuildResult shadowsocks::Build() { return {}; }

    QString shadowsocks::DisplayAddress() { return {}; }
    QString shadowsocks::DisplayName() { return {}; }
    QString shadowsocks::DisplayType() { return {}; }
    QString shadowsocks::DisplayTypeAndName() { return {}; }
    bool shadowsocks::IsEndpoint() { return false; }
}


