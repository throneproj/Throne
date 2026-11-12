#include "include/configs/ProcessRules.h"

#include <algorithm>

#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>

#include "include/database/DatabaseManager.h"
#include "include/database/TrafficStatsRepo.h"

namespace Configs::ProcessRules {
    namespace {
        bool isWindowsPath(const QString& path) {
            if (path.startsWith(QStringLiteral("\\\\"))) return true;
            return path.size() >= 3 && path[0].isLetter() && path[1] == u':' && path[2] == u'\\';
        }

        QString windowsDir(const char* variable, const QString& fallback) {
            QString dir = qEnvironmentVariable(variable, fallback);
            while (dir.endsWith(u'\\')) dir.chop(1);
            return dir;
        }

        bool isVersionName(const QString& part) {
            static const QRegularExpression re(QStringLiteral(R"(^(?:app-|v)?\d+(?:\.\d+)+(?:[-+_.][0-9A-Za-z.]+)?$)"));
            return re.match(part).hasMatch();
        }

        // name_1.21.2911.0_x64__8wekyb3d8bbwe: captures the package name and the publisher id.
        QRegularExpressionMatch windowsAppsPackage(const QString& part) {
            static const QRegularExpression re(QStringLiteral(R"(^(.+?)_\d+(?:\.\d+)+_[^_]*__([0-9A-Za-z]+)$)"));
            return re.match(part);
        }

        enum class Versioned { No, Folder, Package, AppImage };

        // parts[i] is a directory of the split path.
        Versioned versionedPart(const QStringList& parts, qsizetype i) {
            const QString& part = parts[i];
            if (i > 0 && parts[i - 1].compare(QStringLiteral("WindowsApps"), Qt::CaseInsensitive) == 0 && windowsAppsPackage(part).hasMatch())
                return Versioned::Package;
            if (part.startsWith(QStringLiteral(".mount_")) && part.size() > 13) return Versioned::AppImage;
            // /snap/<name>/<revision>/...
            if (i == 3 && parts[0].isEmpty() && parts[1] == QStringLiteral("snap")) {
                static const QRegularExpression revision(QStringLiteral(R"(^(?:\d+|x\d+|current)$)"));
                if (revision.match(part).hasMatch()) return Versioned::Folder;
            }
            return isVersionName(part) ? Versioned::Folder : Versioned::No;
        }

        struct Spellings {
            QHash<QString, QString> byLower;
            QSet<QString> exact;

            void add(const QString& value) {
                if (value.isEmpty()) return;
                exact.insert(value);
                byLower.insert(value.toLower(), value);
            }
        };

        QMutex spellingsMu;
        Spellings knownNames;
        Spellings knownPaths;
        qint64 appMetaLoadedAt = 0;

        void loadAppMetaLocked() {
            const qint64 now = QDateTime::currentSecsSinceEpoch();
            if (now - appMetaLoadedAt < 60) return;
            appMetaLoadedAt = now;
            if (dataManager == nullptr || dataManager->trafficStatsRepo == nullptr) return;
            for (const auto& row : dataManager->trafficStatsRepo->GetAllAppMeta()) {
                knownNames.add(row.process_name);
                knownPaths.add(row.last_path);
            }
        }
    }

    QString QuoteMeta(const QString& text) {
        static const QString special = QStringLiteral(R"(\.+*?()|[]{}^$)");
        QString out;
        out.reserve(text.size() + 8);
        for (const QChar c : text) {
            if (special.contains(c)) out += u'\\';
            out += c;
        }
        return out;
    }

    QString FolderPattern(const QString& path, QString* folder) {
        const QChar sep = isWindowsPath(path) ? u'\\' : u'/';
        const QString sepPattern = QuoteMeta(QString(sep));
        const QStringList parts = path.split(sep);
        if (parts.size() < 2) return {};
        const auto prefix = [&](qsizetype count) {
            const QString dir = parts.first(count).join(sep);
            if (folder != nullptr) *folder = dir;
            return QStringLiteral("^") + QuoteMeta(dir) + sepPattern;
        };

        const qsizetype lastDir = parts.size() - 2;
        for (qsizetype i = 0; i <= lastDir; ++i) {
            if (parts[i].endsWith(QStringLiteral(".app"), Qt::CaseInsensitive)) return prefix(i + 1);
        }
        for (qsizetype i = 1; i <= lastDir; ++i) {
            switch (versionedPart(parts, i)) {
                case Versioned::No:
                    continue;
                case Versioned::Folder:
                    return prefix(i);
                case Versioned::Package: {
                    const auto package = windowsAppsPackage(parts[i]);
                    const QString notSep = QStringLiteral("[^") + sepPattern + QStringLiteral("]+");
                    QString pattern = prefix(i) + QuoteMeta(package.captured(1) + u'_') + notSep + QuoteMeta(QStringLiteral("__") + package.captured(2)) + sepPattern;
                    if (folder != nullptr) *folder = parts.first(i + 1).join(sep);
                    return pattern;
                }
                case Versioned::AppImage: {
                    // .mount_ + the first six letters of the image's name + six random ones.
                    QString pattern = prefix(i) + QuoteMeta(parts[i].chopped(6)) + QStringLiteral("[^/]*/");
                    if (folder != nullptr) *folder = parts.first(i + 1).join(sep);
                    return pattern;
                }
            }
        }
        return prefix(parts.size() - 1);
    }

    bool HasVersionedFolder(const QString& path) {
        const QStringList parts = path.split(isWindowsPath(path) ? u'\\' : u'/');
        for (qsizetype i = 1; i + 1 < parts.size(); ++i) {
            if (versionedPart(parts, i) != Versioned::No) return true;
        }
        return false;
    }

    bool IsSharedHost(const QString& name) {
        static const QRegularExpression re(
            QStringLiteral(R"(^(?:python[0-9.]*w?|pythonw[0-9.]*|java|javaw|node|nodejs|bun|deno|ruby|perl|php|dotnet|pwsh|powershell|cmd|bash|sh|zsh|)"
                           R"(svchost|dllhost|rundll32|msedgewebview2|wscript|cscript|mshta|electron|com\.apple\.webkit\.networking|nsurlsessiond)(?:\.exe)?$)"),
            QRegularExpression::CaseInsensitiveOption);
        return re.match(name).hasMatch();
    }

    bool IsSystemProgram(const QString& path) {
        if (isWindowsPath(path)) {
            const QString root = windowsDir("SystemRoot", QStringLiteral("C:\\Windows"));
            return path.startsWith(root + u'\\', Qt::CaseInsensitive);
        }
        static const QStringList prefixes = {
            QStringLiteral("/usr/sbin/"), QStringLiteral("/sbin/"), QStringLiteral("/usr/lib/systemd/"), QStringLiteral("/lib/systemd/"),
            QStringLiteral("/usr/libexec/"), QStringLiteral("/System/"), QStringLiteral("/Library/Apple/"),
        };
        return std::any_of(prefixes.begin(), prefixes.end(), [&path](const QString& prefix) { return path.startsWith(prefix); });
    }

    bool IsSharedFolder(const QString& folder) {
        if (isWindowsPath(folder)) {
            const QString root = windowsDir("SystemRoot", QStringLiteral("C:\\Windows"));
            const QStringList shared = {
                root, root + QStringLiteral("\\System32"), root + QStringLiteral("\\SysWOW64"),
                windowsDir("ProgramFiles", QStringLiteral("C:\\Program Files")),
                windowsDir("ProgramFiles(x86)", QStringLiteral("C:\\Program Files (x86)")),
            };
            return std::any_of(shared.begin(), shared.end(), [&folder](const QString& dir) { return folder.compare(dir, Qt::CaseInsensitive) == 0; });
        }
        static const QSet<QString> shared = {
            QStringLiteral("/usr/bin"), QStringLiteral("/bin"), QStringLiteral("/usr/sbin"), QStringLiteral("/sbin"),
            QStringLiteral("/usr/local/bin"), QStringLiteral("/usr/libexec"), QStringLiteral("/usr/lib"), QStringLiteral("/usr/lib64"),
            QStringLiteral("/opt"), QStringLiteral("/Applications"), QStringLiteral("/tmp"),
        };
        return shared.contains(folder);
    }

    void Learn(const QStringList& names, const QStringList& paths) {
        QMutexLocker lock(&spellingsMu);
        for (const auto& name : names) knownNames.add(name);
        for (const auto& path : paths) knownPaths.add(path);
    }

    QString KnownSpelling(const QString& attr, const QString& value) {
        QMutexLocker lock(&spellingsMu);
        loadAppMetaLocked();
        const Spellings& known = attr == QStringLiteral("process_name") ? knownNames : knownPaths;
        if (known.exact.contains(value)) return {};
        return known.byLower.value(value.toLower());
    }
}
