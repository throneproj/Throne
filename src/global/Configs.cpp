#include "include/global/Configs.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <utility>
#include <include/api/RPC.h>

#include "include/database/GroupsRepo.h"
#include "include/database/RoutesRepo.h"


#ifdef Q_OS_WIN
#include "include/sys/windows/guihelper.h"
#else
#ifdef Q_OS_LINUX
#include <include/sys/linux/LinuxCap.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace Configs {
    void initDB(const std::string& dbPath) {
        dataManager = new DatabaseManager(dbPath);

        if (dataManager->groupsRepo->GetAllGroupIds().empty()) {
            auto defaultGroup = GroupsRepo::NewGroup();
            defaultGroup->name = QObject::tr("Default");
            dataManager->groupsRepo->AddGroup(defaultGroup);
        }
        if (dataManager->routesRepo->GetAllRouteProfileIds().empty()) {
            auto defaultRoute = RouteProfile::GetDefaultChain();
            dataManager->routesRepo->AddRouteProfile(defaultRoute);
        }
    }

    QString FindCoreRealPath() {
#ifdef NKR_CORE_IN_PATH
        // Packaged installs may launch the core through a privilege wrapper on PATH (NixOS security wrappers).
        const auto wrapped = QStandardPaths::findExecutable("ThroneCore");
        if (!wrapped.isEmpty()) return wrapped;
#endif
        auto fn = QApplication::applicationDirPath() + "/ThroneCore";
#ifdef Q_OS_WIN
        fn += ".exe";
#endif
        auto fi = QFileInfo(fn);
        QString path;
        if (fi.isSymLink()) path =  fi.symLinkTarget();
        path = fn;
#ifdef Q_OS_WIN
        path.replace("/", "\\");
#endif
        return path;
    }

    short isAdminCache = -1;

    bool isSetuidSet(const std::string& path) {
#ifdef Q_OS_MACOS
        struct stat fileInfo;

        if (stat(path.c_str(), &fileInfo) != 0) {
            return false;
        }

        if (fileInfo.st_mode & S_ISUID) {
            return true;
        } else {
            return false;
        }
#else
        return false;
#endif
    }

    // IsAdmin 主要判断：有无权限启动 Tun
    bool IsAdmin(bool forceRenew) {
        if (isAdminCache >= 0 && !forceRenew) return isAdminCache;

        bool admin = false;
#ifdef Q_OS_WIN
        admin = Windows_IsInAdmin();
        Configs::dataManager->settingsRepo->windows_set_admin = admin;
#else
        // Unknown until the core answers; caching that would pin "not elevated" for the session.
        if (API::defaultClient == nullptr) return false;
        bool ok;
        const auto isPrivileged = API::defaultClient->IsPrivileged(&ok);
        if (!ok) return false;
        admin = isPrivileged;
#endif
        isAdminCache = admin;
        return admin;
    }

    QString GetBasePath() {
        if (Configs::dataManager->settingsRepo->flag_use_appdata) return QStandardPaths::writableLocation(
              QStandardPaths::AppConfigLocation);
        return qApp->applicationDirPath();
    }
} // namespace Configs
