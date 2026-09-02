#include "include/sys/UrlScheme.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTextStream>

static const QString kDesktopId = "throne-url-handler.desktop";

// AppImage: point at the outer image ($APPIMAGE), not the extracted binary, which disappears after exit.
static QString execTarget() {
    auto env = QProcessEnvironment::systemEnvironment();
    if (env.contains("APPIMAGE")) return env.value("APPIMAGE");
    return QApplication::applicationFilePath();
}

static QString desktopFilePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    return dir + "/" + kDesktopId;
}

// "throne" is in no icon theme for the /opt and AppImage layouts, so unpack a copy and use an absolute path.
static QString iconTarget() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString path = dir + "/throne.png";
    QDir().mkpath(dir);
    QFile::remove(path);
    return QFile::copy(":/Throne/Throne.png", path) ? path : QStringLiteral("throne");
}

QString UrlScheme_DesiredState() {
    return "v4|" + execTarget();
}

// iconTarget() has side effects, so match the Exec line rather than regenerating the entry to compare it.
bool UrlScheme_IsCurrent() {
    QFile f(desktopFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QString expected = "Exec=\"" + execTarget() + "\" %U";
    while (!f.atEnd()) {
        if (QString::fromUtf8(f.readLine()).trimmed() == expected) return true;
    }
    return false;
}

void UrlScheme_Apply() {
    const QString path = desktopFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << "[Desktop Entry]\n"
           << "Type=Application\n"
           << "Name=Throne\n"
           << "Icon=" << iconTarget() << "\n"
           << "Exec=\"" << execTarget() << "\" %U\n"
           << "MimeType=x-scheme-handler/throne;application/json;application/yaml;text/yaml;\n"
           << "Terminal=false\n"
           << "NoDisplay=true\n";
        ts.flush();
        f.close();
    }

    // mimeinfo.cache alone makes the association resolve; `xdg-mime default` is deliberately not called, it only ever wrote us into the shared mimeapps.list.
    // May be absent on minimal systems; execute() just returns nonzero then.
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QProcess::execute("update-desktop-database", {appsDir});
}

// xdg writes a desktop-prefixed list when XDG_CURRENT_DESKTOP is set, and the unprefixed one otherwise.
static QStringList mimeappsLists() {
    const QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);

    QStringList paths;
    const auto desktops = QProcessEnvironment::systemEnvironment().value("XDG_CURRENT_DESKTOP").split(':', Qt::SkipEmptyParts);
    for (const QString &de : desktops) {
        paths << cfgDir + "/" + de.toLower() + "-mimeapps.list";
        paths << appsDir + "/" + de.toLower() + "-mimeapps.list";
    }
    paths << cfgDir + "/mimeapps.list" << appsDir + "/mimeapps.list";
    return paths;
}

// xdg-mime has no "unset", so the associations it wrote are stripped by hand; handlers sharing the line are kept.
static void stripFromMimeapps(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    const QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
    f.close();

    QStringList out;
    bool changed = false;
    for (const QString &line : lines) {
        const int eq = line.indexOf('=');
        if (eq < 0 || line.trimmed().startsWith('[') || !line.contains(kDesktopId)) {
            out << line;
            continue;
        }

        QStringList kept;
        bool hit = false;
        for (const QString &app : line.mid(eq + 1).split(';', Qt::SkipEmptyParts)) {
            if (app.trimmed() == kDesktopId) hit = true;
            else kept << app;
        }
        if (!hit) {
            out << line;
            continue;
        }
        changed = true;
        if (!kept.isEmpty()) out << line.left(eq + 1) + kept.join(';') + ";";
    }
    if (!changed) return;

    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(out.join('\n').toUtf8());
        f.close();
    }
}

void UrlScheme_Remove() {
    QFile::remove(desktopFilePath());

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile::remove(dataDir + "/throne.png");
    QDir().rmdir(dataDir);

    for (const QString &path : mimeappsLists()) stripFromMimeapps(path);

    QProcess::execute("update-desktop-database", {QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)});
}
