#include "include/configs/outbounds/tor.h"

#include <QDir>
#include <QFileInfo>

namespace Configs {
    QString tor::ExecutablePath() {
#ifdef Q_OS_WIN
        return QDir(QStringLiteral("tor")).absoluteFilePath(QStringLiteral("tor.exe"));
#else
        return QDir(QStringLiteral("tor")).absoluteFilePath(QStringLiteral("tor"));
#endif
    }

    bool tor::Installed() {
        return QFileInfo(ExecutablePath()).isFile();
    }

    QString tor::DisplayType() {
        return QStringLiteral("Tor");
    }

    BuildResult tor::Build() {
        return {QJsonObject{{"type", "tor"}}, {}};
    }
}
