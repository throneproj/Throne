#include "include/sys/UrlScheme.hpp"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <shlobj.h>

// In QSettings NativeFormat the value name "Default" is a key's unnamed (Default) value, and '/' separates subkeys.

static const QString kClasses = "HKEY_CURRENT_USER\\Software\\Classes";
static const QString kProgId = "Throne.Config";

static const QStringList kConfigExtensions = {".json", ".conf", ".yaml", ".yml", ".ini", ".txt"};

static QString openCommand() {
    return "\"" + QDir::toNativeSeparators(QApplication::applicationFilePath()) + "\" \"%1\"";
}

// None of these is keyed by install path, so two portable copies write the same three keys and the last launched one wins.
static QStringList commandKeys() {
    const QString exeName = QFileInfo(QApplication::applicationFilePath()).fileName();
    return {
        kClasses + "\\throne",
        kClasses + "\\" + kProgId,
        kClasses + "\\Applications\\" + exeName,
    };
}

QString UrlScheme_DesiredState() {
    return "v2|" + openCommand();
}

bool UrlScheme_IsCurrent() {
    const QString command = openCommand();
    for (const QString &key : commandKeys()) {
        QSettings s(key, QSettings::NativeFormat);
        if (s.value("shell/open/command/Default").toString() != command) return false;
    }
    return true;
}

void UrlScheme_Apply() {
    const QString command = openCommand();
    const QString exe = QDir::toNativeSeparators(QApplication::applicationFilePath());

    QSettings scheme(kClasses + "\\throne", QSettings::NativeFormat);
    scheme.setValue("Default", "URL:Throne Protocol");
    scheme.setValue("URL Protocol", "");
    scheme.setValue("shell/open/command/Default", command);

    QSettings progId(kClasses + "\\" + kProgId, QSettings::NativeFormat);
    progId.setValue("Default", "Throne profile");
    progId.setValue("DefaultIcon/Default", exe + ",0");
    progId.setValue("shell/open/command/Default", command);

    // OpenWithProgids is the additive half of an association: the extension's own default is left alone.
    for (const QString &ext : kConfigExtensions) {
        QSettings assoc(kClasses + "\\" + ext + "\\OpenWithProgids", QSettings::NativeFormat);
        assoc.setValue(kProgId, "");
    }

    // Applications\<exe> is what "Open with > Choose another app" reads, the only route for an extensionless file.
    QSettings app(kClasses + "\\Applications\\" + QFileInfo(exe).fileName(), QSettings::NativeFormat);
    app.setValue("FriendlyAppName", "Throne");
    app.setValue("shell/open/command/Default", command);
    for (const QString &ext : kConfigExtensions) {
        app.setValue("SupportedTypes/" + ext, "");
    }

    // QSettings only reaches the registry on sync, so flush before SHChangeNotify.
    scheme.sync();
    progId.sync();
    app.sync();
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void UrlScheme_Remove() {
    // Removing from the parent key drops the whole subtree; QSettings::remove("") would only empty it and leave the node behind.
    QSettings classes(kClasses, QSettings::NativeFormat);
    classes.remove("throne");
    classes.remove(kProgId);
    classes.remove("Applications/" + QFileInfo(QApplication::applicationFilePath()).fileName());
    classes.sync();

    // Only our own progid goes; the extension's default was never ours to touch.
    for (const QString &ext : kConfigExtensions) {
        QSettings assoc(kClasses + "\\" + ext + "\\OpenWithProgids", QSettings::NativeFormat);
        assoc.remove(kProgId);
        assoc.sync();
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
