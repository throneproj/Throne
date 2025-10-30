#include "include/configs/outbounds/trojan.h"

namespace Configs {
    bool Trojan::ParseFromLink(const QString& link) { return false; }
    bool Trojan::ParseFromJson(const QJsonObject& object) { return false; }
    QString Trojan::ExportToLink() { return {}; }
    QJsonObject Trojan::ExportToJson() { return {}; }
    BuildResult Trojan::Build() { return {}; }

    QString Trojan::DisplayAddress() { return {}; }
    QString Trojan::DisplayName() { return {}; }
    QString Trojan::DisplayType() { return {}; }
    QString Trojan::DisplayTypeAndName() { return {}; }
    bool Trojan::IsEndpoint() { return false; }
}


