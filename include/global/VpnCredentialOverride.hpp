#pragma once

#include <QString>

namespace Configs
{
    struct VpnCredentials {
        QString username;
        QString password;
    };

    // In-memory only: dropped when the user stops the profile, never written to the database.
    void SetVpnCredentialOverride(int profileID, const VpnCredentials &credentials);

    void ClearVpnCredentialOverride(int profileID);

    VpnCredentials ResolveVpnCredentials(int profileID, const QString &username, const QString &password);

    // Thread-local. A test box dies with its RPC, so nothing there answers a live OTP challenge.
    void SetBuildingTestConfig(bool building);

    bool BuildingTestConfig();
}
