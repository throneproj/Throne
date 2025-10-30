#include "include/configs/outbounds/vmess.h"

namespace Configs {
    bool vmess::ParseFromLink(const QString& link) { return false; }
    bool vmess::ParseFromJson(const QJsonObject& object) { return false; }
    QString vmess::ExportToLink() { return {}; }
    QJsonObject vmess::ExportToJson() { return {}; }
    BuildResult vmess::Build() { return {}; }

    QString vmess::DisplayAddress() { return {}; }
    QString vmess::DisplayName() { return {}; }
    QString vmess::DisplayType() { return {}; }
    QString vmess::DisplayTypeAndName() { return {}; }
    bool vmess::IsEndpoint() { return false; }
}


