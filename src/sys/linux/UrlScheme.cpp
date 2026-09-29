#include "include/sys/UrlScheme.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTextStream>

// Both associations live in this one entry, told apart by its MimeType list.
static const QString kDesktopId = "throne-url-handler.desktop";

static const QStringList kLinkTypes = {"x-scheme-handler/throne"};
static const QStringList kConfigTypes = {"application/json", "application/yaml", "text/yaml"};

static const QStringList &typesOf(Association a) {
    return a == Association::Links ? kLinkTypes : kConfigTypes;
}

// AppImage: point at the outer image ($APPIMAGE), not the extracted binary, which disappears after exit.
static QString execTarget() {
#ifdef NKR_DESKTOP_EXEC
    return QStringLiteral(NKR_DESKTOP_EXEC);
#else
    auto env = QProcessEnvironment::systemEnvironment();
    if (env.contains("APPIMAGE")) return env.value("APPIMAGE");
    return QApplication::applicationFilePath();
#endif
}

static QString execLine() {
    return "Exec=\"" + execTarget() + "\" %U";
}

static QString appsDir() {
    return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
}

static QString desktopFilePath() {
    return appsDir() + "/" + kDesktopId;
}

// "throne" is in no icon theme for the /opt and AppImage layouts, so unpack a copy and use an absolute path.
static QString iconTarget() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString path = dir + "/throne.png";
    QDir().mkpath(dir);
    QFile::remove(path);
    return QFile::copy(":/Throne/Throne.png", path) ? path : QStringLiteral("throne");
}

struct DesktopEntry {
    bool current = false;
    QStringList types;
};

// iconTarget() has side effects, so the entry is parsed rather than regenerated to compare it; types we no longer claim are dropped.
static DesktopEntry readEntry() {
    DesktopEntry entry;
    QFile f(desktopFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return entry;
    const QString exec = execLine();
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line == exec) entry.current = true;
        if (!line.startsWith("MimeType=")) continue;
        for (const QString &type : line.mid(9).split(';', Qt::SkipEmptyParts)) {
            if (kLinkTypes.contains(type) || kConfigTypes.contains(type)) entry.types << type;
        }
    }
    return entry;
}

static void writeEntry(const QStringList &types) {
    const QString path = desktopFilePath();
    if (types.isEmpty()) {
        QFile::remove(path);
        const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QFile::remove(dataDir + "/throne.png");
        QDir().rmdir(dataDir);
    } else {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << "[Desktop Entry]\n"
               << "Type=Application\n"
               << "Name=Throne\n"
               << "Icon=" << iconTarget() << "\n"
               << execLine() << "\n"
               << "MimeType=" << types.join(';') << ";\n"
               << "Terminal=false\n"
               << "NoDisplay=true\n";
            ts.flush();
            f.close();
        }
    }

    // mimeinfo.cache alone makes the association resolve; `xdg-mime default` is deliberately not called, it only ever wrote us into the shared mimeapps.list.
    // May be absent on minimal systems; execute() just returns nonzero then.
    QProcess::execute("update-desktop-database", {appsDir()});
}

QString UrlScheme_DesiredState(Association a) {
    return (a == Association::Links ? "v4|" : "v1|") + execTarget();
}

bool UrlScheme_AutoRegisterByDefault() {
    return true;
}

bool UrlScheme_IsCurrent(Association a) {
    const DesktopEntry entry = readEntry();
    return entry.current && entry.types.contains(typesOf(a).first());
}

void UrlScheme_Apply(Association a) {
    QStringList types = readEntry().types;
    for (const QString &type : typesOf(a)) {
        if (!types.contains(type)) types << type;
    }
    writeEntry(types);
}

// xdg writes a desktop-prefixed list when XDG_CURRENT_DESKTOP is set, and the unprefixed one otherwise.
static QStringList mimeappsLists() {
    const QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);

    QStringList paths;
    const auto desktops = QProcessEnvironment::systemEnvironment().value("XDG_CURRENT_DESKTOP").split(':', Qt::SkipEmptyParts);
    for (const QString &de : desktops) {
        paths << cfgDir + "/" + de.toLower() + "-mimeapps.list";
        paths << appsDir() + "/" + de.toLower() + "-mimeapps.list";
    }
    paths << cfgDir + "/mimeapps.list" << appsDir() + "/mimeapps.list";
    return paths;
}

// xdg-mime has no "unset", so the associations it wrote are stripped by hand; handlers sharing the line are kept.
static void stripFromMimeapps(const QString &path, const QStringList &types) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    const QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
    f.close();

    QStringList out;
    bool changed = false;
    for (const QString &line : lines) {
        const int eq = line.indexOf('=');
        if (eq < 0 || line.trimmed().startsWith('[') || !types.contains(line.left(eq).trimmed()) || !line.contains(kDesktopId)) {
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

void UrlScheme_Remove(Association a) {
    QStringList types = readEntry().types;
    for (const QString &type : typesOf(a)) types.removeAll(type);
    writeEntry(types);

    for (const QString &path : mimeappsLists()) stripFromMimeapps(path, typesOf(a));
}
