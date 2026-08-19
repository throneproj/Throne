#pragma once

#include <QIcon>

namespace Icon {

    enum class TrayIconStatus {
        None,
        Running,
        SystemProxy,
        Vpn,
        Dns,
        SystemProxyDns,
    };

    QIcon GetTrayIcon(TrayIconStatus status);

    QIcon GetTaskbarIcon(TrayIconStatus status);

    // Drop cached icons so the next GetTrayIcon reloads from disk/resources.
    // Call when custom-icon files are replaced or the custom-icon setting flips.
    void InvalidateTrayIconCache();
} // namespace Icon
