#include "include/configs/outbounds/tor.h"

namespace Configs {
    QString tor::DisplayType() {
        return QStringLiteral("Tor");
    }

    BuildResult tor::Build() {
        return {QJsonObject{{"type", "tor"}}, {}};
    }
}
