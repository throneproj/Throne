#include "include/ui/utils/ProcessIcon.h"

#include <QFileIconProvider>
#include <QFileInfo>
#include <QHash>

#ifdef Q_OS_WIN
#include "include/sys/windows/guihelper.h"
#endif

namespace {
    // Never destroyed: icons must not outlive the application object at exit.
    QFileIconProvider &processIconProvider() {
        static auto *provider = new QFileIconProvider;
        return *provider;
    }

    QIcon genericProgramIcon() {
#ifdef Q_OS_WIN
        if (QIcon icon = Windows_DefaultProgramIcon(); !icon.isNull()) return icon;
#elif !defined(Q_OS_MACOS)
        const QString executable = QStringLiteral("application-x-executable");
        if (QIcon::hasThemeIcon(executable)) return QIcon::fromTheme(executable);
#endif
        return processIconProvider().icon(QAbstractFileIconProvider::File);
    }

    QIcon lookupProgramIcon(const QString &path) {
        if (path.isEmpty()) return {};
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
        // A missing file would give an icon that paints nothing.
        if (!QFileInfo::exists(path)) return {};
#ifdef Q_OS_MACOS
        // Only a bundle carries the app's icon, and a helper app nested inside another one shows the outer one.
        if (const qsizetype bundle = path.indexOf(QStringLiteral(".app/"), 0, Qt::CaseInsensitive); bundle > 0)
            return processIconProvider().icon(QFileInfo(path.left(bundle + 4)));
#endif
        return processIconProvider().icon(QFileInfo(path));
#else
        // Binaries carry no icon here, and icon themes usually name an app's icon after its binary.
        const QString name = QFileInfo(path).fileName();
        for (const QString &candidate : {name, name.toLower()}) {
            if (QIcon::hasThemeIcon(candidate)) return QIcon::fromTheme(candidate);
        }
        return {};
#endif
    }
} // namespace

QIcon ProcessIcon::ForPath(const QString &path) {
    static auto *cache = new QHash<QString, QIcon>;
    if (const auto cached = cache->constFind(path); cached != cache->cend()) return *cached;
    QIcon icon = lookupProgramIcon(path);
    if (icon.isNull()) {
        static const auto *generic = new QIcon(genericProgramIcon());
        icon = *generic;
    }
    cache->insert(path, icon);
    return icon;
}
