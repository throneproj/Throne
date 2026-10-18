#include "include/engine/services/Dashboard.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include "include/configs/generate.h"

namespace {
    bool dashboardCopyOut(const QString &from, const QString &to) {
        QFile::remove(to);
        if (!QFile::copy(from, to)) return false;
        // Resource files are read-only, and QFile::copy carries that onto the copy.
        return QFile::setPermissions(to, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                         QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }

    bool dashboardUnpackBundled(const QDir &dest) {
        if (!QFile::exists(":/dashboard/index.html")) return false;
        const QDir bundle(":/dashboard");
        QDirIterator it(bundle.path(), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const auto source = it.next();
            const auto target = dest.filePath(bundle.relativeFilePath(source));
            if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !dashboardCopyOut(source, target)) return false;
        }
        return true;
    }
} // namespace

namespace Engine::Dashboard {
    void SeedClashUi() {
        if (auto dashDir = QDir("dashboard"); !dashDir.exists() && QDir().mkdir("dashboard")) {
            if (auto dashFile = QFile(":/Throne/dashboard-notice.html"); dashFile.exists() && dashFile.open(QIODevice::ReadOnly))
            {
                auto data = dashFile.readAll();
                if (auto dest = QFile("dashboard/index.html"); dest.open(QIODevice::Truncate | QIODevice::WriteOnly))
                {
                    dest.write(data);
                    dest.close();
                }
                dashFile.close();
            }
        }
    }

    void SeedApi() {
        QDir dashDir(Configs::apiDashboardDir);
        if (!dashDir.exists() && !QDir().mkpath(Configs::apiDashboardDir)) return;
        if (!QFile::exists(dashDir.filePath("index.html"))) dashboardUnpackBundled(dashDir);
        // Reinstalling replaces the whole directory, so this cannot be a one-time copy.
        auto src = QFile(":/Throne/dashboard-bootstrap.html");
        if (!src.open(QIODevice::ReadOnly)) return;
        const auto data = src.readAll();
        src.close();
        if (auto dest = QFile(dashDir.filePath("throne.html")); dest.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
            dest.write(data);
            dest.close();
        }
    }
} // namespace Engine::Dashboard
