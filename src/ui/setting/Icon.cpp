#include "include/ui/setting/Icon.hpp"

#include "include/global/Configs.hpp"

#include <QFile>
#include <QHash>

namespace {
    QHash<Icon::TrayIconStatus, QIcon> g_trayIcons;
    bool g_trayIconsCustom = false;
    bool g_trayIconsCustomKnown = false;

    const QHash<Icon::TrayIconStatus, QString> &statusNames() {
        static const QHash<Icon::TrayIconStatus, QString> names = {
            {Icon::TrayIconStatus::None, QStringLiteral("Off")},
            {Icon::TrayIconStatus::Running, QStringLiteral("Throne")},
            {Icon::TrayIconStatus::SystemProxy, QStringLiteral("Proxy")},
            {Icon::TrayIconStatus::Vpn, QStringLiteral("Tun")},
            {Icon::TrayIconStatus::Dns, QStringLiteral("Dns")},
            {Icon::TrayIconStatus::SystemProxyDns, QStringLiteral("Proxy-Dns")},
        };
        return names;
    }

    QIcon loadNamedIcon(const QString &name, bool useCustom) {
        if (useCustom) {
            const QString customPath = QStringLiteral("icons/") + name + QStringLiteral(".png");
            if (QFile::exists(customPath)) {
                QIcon icon(customPath);
                if (!icon.isNull()) return icon;
            }
        }
        return QIcon(QStringLiteral(":/Throne/") + name + QStringLiteral(".png"));
    }
} // namespace

void Icon::InvalidateTrayIconCache() {
    g_trayIcons.clear();
}

QIcon Icon::GetTrayIcon(TrayIconStatus status) {
    const bool useCustom = Configs::dataManager->settingsRepo->use_custom_icons;
    if (!g_trayIconsCustomKnown || g_trayIconsCustom != useCustom) {
        g_trayIcons.clear();
        g_trayIconsCustom = useCustom;
        g_trayIconsCustomKnown = true;
    }

    if (const auto it = g_trayIcons.constFind(status); it != g_trayIcons.cend()) {
        return it.value();
    }

    const auto &names = statusNames();
    QString name = names.value(status);
    if (name.isEmpty()) {
        MW_show_log("Icon::GetTrayIcon: Unknown status");
        name = QStringLiteral("Off");
    }

    const QIcon icon = loadNamedIcon(name, useCustom);
    g_trayIcons.insert(status, icon);
    return icon;
}

QIcon Icon::GetTaskbarIcon(TrayIconStatus status) {
    const auto &settings = Configs::dataManager->settingsRepo;
    // The bundled icon only: a custom PNG has no room for the padding macOS adds in the dock.
    if (settings->use_custom_icons && !settings->follow_status_in_taskbar) {
        return QIcon(QStringLiteral(":/Throne/Throne.png"));
    }
    return GetTrayIcon(status);
}
