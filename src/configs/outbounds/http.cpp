#include "include/configs/outbounds/http.h"

namespace Configs {
    bool http::ParseFromLink(const QString& link) { return false; }
    bool http::ParseFromJson(const QJsonObject& object) { return false; }
    QString http::ExportToLink() { return {}; }
    QJsonObject http::ExportToJson() { return {}; }
    BuildResult http::Build() { return {}; }

    QString http::DisplayAddress() { return {}; }
    QString http::DisplayName() { return {}; }
    QString http::DisplayType() { return {}; }
    QString http::DisplayTypeAndName() { return {}; }
    bool http::IsEndpoint() { return false; }
}


