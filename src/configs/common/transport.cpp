#include "include/configs/common/transport.h"

namespace Configs {
    bool Transport::ParseFromLink(const QString& link) { return false; }
    bool Transport::ParseFromJson(const QJsonObject& object) { return false; }
    QString Transport::ExportToLink() { return {}; }
    QJsonObject Transport::ExportToJson() { return {}; }
    BuildResult Transport::Build() { return {}; }
}


