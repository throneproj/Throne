#include "include/configs/outbounds/wireguard.h"

namespace Configs {
    bool Peer::ParseFromLink(const QString& link) { return false; }
    bool Peer::ParseFromJson(const QJsonObject& object) { return false; }
    QString Peer::ExportToLink() { return {}; }
    QJsonObject Peer::ExportToJson() { return {}; }
    BuildResult Peer::Build() { return {}; }

    bool wireguard::ParseFromLink(const QString& link) { return false; }
    bool wireguard::ParseFromJson(const QJsonObject& object) { return false; }
    QString wireguard::ExportToLink() { return {}; }
    QJsonObject wireguard::ExportToJson() { return {}; }
    BuildResult wireguard::Build() { return {}; }

    QString wireguard::DisplayAddress() { return {}; }
    QString wireguard::DisplayName() { return {}; }
    QString wireguard::DisplayType() { return {}; }
    QString wireguard::DisplayTypeAndName() { return {}; }
    bool wireguard::IsEndpoint() { return false; }
}


