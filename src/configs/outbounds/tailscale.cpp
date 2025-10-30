#include "include/configs/outbounds/tailscale.h"

namespace Configs {
    bool tailscale::ParseFromLink(const QString& link) { return false; }
    bool tailscale::ParseFromJson(const QJsonObject& object) { return false; }
    QString tailscale::ExportToLink() { return {}; }
    QJsonObject tailscale::ExportToJson() { return {}; }
    BuildResult tailscale::Build() { return {}; }

    QString tailscale::DisplayAddress() { return {}; }
    QString tailscale::DisplayName() { return {}; }
    QString tailscale::DisplayType() { return {}; }
    QString tailscale::DisplayTypeAndName() { return {}; }
    bool tailscale::IsEndpoint() { return false; }
}


