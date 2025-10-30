#include "include/configs/outbounds/anyTLS.h"

namespace Configs {
    bool anyTLS::ParseFromLink(const QString& link) { return false; }
    bool anyTLS::ParseFromJson(const QJsonObject& object) { return false; }
    QString anyTLS::ExportToLink() { return {}; }
    QJsonObject anyTLS::ExportToJson() { return {}; }
    BuildResult anyTLS::Build() { return {}; }

    QString anyTLS::DisplayAddress() { return {}; }
    QString anyTLS::DisplayName() { return {}; }
    QString anyTLS::DisplayType() { return {}; }
    QString anyTLS::DisplayTypeAndName() { return {}; }
    bool anyTLS::IsEndpoint() { return false; }
}


