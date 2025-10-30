#include "include/configs/outbounds/ssh.h"

namespace Configs {
    bool ssh::ParseFromLink(const QString& link) { return false; }
    bool ssh::ParseFromJson(const QJsonObject& object) { return false; }
    QString ssh::ExportToLink() { return {}; }
    QJsonObject ssh::ExportToJson() { return {}; }
    BuildResult ssh::Build() { return {}; }

    QString ssh::DisplayAddress() { return {}; }
    QString ssh::DisplayName() { return {}; }
    QString ssh::DisplayType() { return {}; }
    QString ssh::DisplayTypeAndName() { return {}; }
    bool ssh::IsEndpoint() { return false; }
}


