#include "include/configs/common/TLS.h"

namespace Configs {
    bool uTLS::ParseFromLink(const QString& link) { return false; }
    bool uTLS::ParseFromJson(const QJsonObject& object) { return false; }
    QString uTLS::ExportToLink() { return {}; }
    QJsonObject uTLS::ExportToJson() { return {}; }
    BuildResult uTLS::Build() { return {}; }

    bool ECH::ParseFromLink(const QString& link) { return false; }
    bool ECH::ParseFromJson(const QJsonObject& object) { return false; }
    QString ECH::ExportToLink() { return {}; }
    QJsonObject ECH::ExportToJson() { return {}; }
    BuildResult ECH::Build() { return {}; }

    bool Reality::ParseFromLink(const QString& link) { return false; }
    bool Reality::ParseFromJson(const QJsonObject& object) { return false; }
    QString Reality::ExportToLink() { return {}; }
    QJsonObject Reality::ExportToJson() { return {}; }
    BuildResult Reality::Build() { return {}; }

    bool TLS::ParseFromLink(const QString& link) { return false; }
    bool TLS::ParseFromJson(const QJsonObject& object) { return false; }
    QString TLS::ExportToLink() { return {}; }
    QJsonObject TLS::ExportToJson() { return {}; }
    BuildResult TLS::Build() { return {}; }
}


