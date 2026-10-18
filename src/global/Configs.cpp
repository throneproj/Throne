#include "include/global/Configs.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <utility>
#include <include/api/RPC.h>

#include "include/database/GroupsRepo.h"
#include "include/database/MarkersRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/scanner/DefaultIpLists.h"


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
        // The window repairs this when it lists the groups; without one (headless) imports would target no group.
        if (dataManager->groupsRepo->CurrentGroup() == nullptr) {
            if (const auto order = dataManager->groupsRepo->GetGroupsTabOrder(); !order.isEmpty()) {
                dataManager->settingsRepo->current_group = order.first();
                dataManager->settingsRepo->Save();
            }
        }
        if (dataManager->routesRepo->GetAllRouteProfileIds().empty()) {
            auto defaultRoute = RouteProfile::GetDefaultChain();
            dataManager->routesRepo->AddRouteProfile(defaultRoute);
        }
        // Once only, so lists the user deleted stay deleted.
        if (!dataManager->markersRepo->IsMarked(Markers::DefaultIpLists)) {
            Scanner::DefaultIpLists::EnsureAll();
            dataManager->markersRepo->Mark(Markers::DefaultIpLists);
        }
    }

    QString FindCoreRealPath() {
#ifdef NKR_CORE_IN_PATH
        // Packaged installs may launch the core through a privilege wrapper on PATH (NixOS security wrappers).
        const auto wrapped = QStandardPaths::findExecutable("ThroneCore");
        if (!wrapped.isEmpty()) return wrapped;
#endif
        auto fn = QCoreApplication::applicationDirPath() + "/ThroneCore";
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
        if (API::defaultClient == nullptr || !API::defaultClient->IsConnected()) return false;
        bool ok;
        const auto info = API::defaultClient->Privileges(&ok);
        if (!ok) return false;
        admin = info.privileged;
        SetKnownCapabilities(info);
#endif
        isAdminCache = admin;
        return admin;
    }

    std::optional<bool> KnownAdmin() {
        if (isAdminCache >= 0) return isAdminCache != 0;
#ifdef Q_OS_WIN
        // Local and cheap; unlike IsAdmin it leaves windows_set_admin alone.
        return Windows_IsInAdmin();
#else
        return std::nullopt;
#endif
    }

    void SetKnownAdmin(bool admin) {
        isAdminCache = admin ? 1 : 0;
    }

    namespace {
        std::optional<API::PrivilegeInfo> configsKnownCapabilities;
    } // namespace

    std::optional<API::PrivilegeInfo> KnownCapabilities() {
        return configsKnownCapabilities;
    }

    void SetKnownCapabilities(const API::PrivilegeInfo &info) {
        configsKnownCapabilities = info;
    }

    void RefreshStaleTunDevice(int timeoutMs) {
#ifdef Q_OS_WIN
        Q_UNUSED(timeoutMs);
#else
        // Only the device can appear later; a running core never gains CAP_NET_ADMIN.
        const auto caps = KnownCapabilities();
        if (!caps || caps->tunDevice != false) return;
        if (API::defaultClient == nullptr || !API::defaultClient->IsConnected()) return;
        bool ok = false;
        const auto info = API::defaultClient->Privileges(&ok, timeoutMs);
        if (!ok) return;
        SetKnownAdmin(info.privileged);
        SetKnownCapabilities(info);
#endif
    }

    namespace {
        QString basePathOverride;
    } // namespace

    void SetBasePath(const QString &path) {
        basePathOverride = path;
    }

    QString GetBasePath() {
        if (!basePathOverride.isEmpty()) return basePathOverride;
        if (Configs::dataManager->settingsRepo->flag_use_appdata) return QStandardPaths::writableLocation(
              QStandardPaths::AppConfigLocation);
        return QCoreApplication::applicationDirPath();
    }
} // namespace Configs
