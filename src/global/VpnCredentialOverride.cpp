#include "include/global/VpnCredentialOverride.hpp"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

namespace Configs {
    namespace {
        QMutex vpnCredentialOverrideMu;
        QHash<int, VpnCredentials> vpnCredentialOverrides;
        thread_local bool buildingTestConfig = false;
    }

    void SetBuildingTestConfig(const bool building)
    {
        buildingTestConfig = building;
    }

    bool BuildingTestConfig()
    {
        return buildingTestConfig;
    }

    void SetVpnCredentialOverride(int profileID, const VpnCredentials &credentials)
    {
        if (profileID < 0) return;
        QMutexLocker lk(&vpnCredentialOverrideMu);
        vpnCredentialOverrides.insert(profileID, credentials);
    }

    void ClearVpnCredentialOverride(int profileID)
    {
        QMutexLocker lk(&vpnCredentialOverrideMu);
        vpnCredentialOverrides.remove(profileID);
    }

    VpnCredentials ResolveVpnCredentials(int profileID, const QString &username, const QString &password)
    {
        if (profileID >= 0) {
            QMutexLocker lk(&vpnCredentialOverrideMu);
            if (const auto it = vpnCredentialOverrides.constFind(profileID); it != vpnCredentialOverrides.constEnd()) {
                return *it;
            }
        }
        return {username, password};
    }
}
