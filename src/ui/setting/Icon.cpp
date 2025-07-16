#include "include/ui/setting/Icon.hpp"

#include "include/global/Configs.hpp"

#include <QPainter>

QPixmap Icon::GetTrayIcon(TrayIconStatus status) {
    QPixmap pixmap;

    if (status == NONE)
    {
        auto pixmap_read = QPixmap(QString(":/Throne/") + "Off" + ".png");
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == RUNNING)
    {
        auto pixmap_read = QPixmap(QString(":/Throne/") + "Throne" + ".png");
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == SYSTEM_PROXY_DNS)
    {
        auto pixmap_read = QPixmap(QString(":/Throne/") + "Proxy-Dns" + ".png");
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == SYSTEM_PROXY)
    {
        auto pixmap_read = QPixmap(QString(":/Throne/") + "Proxy" + ".png");
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == DNS)
    {
        auto pixmap_read = QPixmap(QString(":/Throne/") + "Dns" + ".png");
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == VPN)
    {
        auto pixmap_read = QPixmap(QString(":/Throne/") + "Tun" + ".png");
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else
    {
        MW_show_log("Icon::GetTrayIcon: Unknown status");
        auto pixmap_read = QPixmap(QString(":/Throne/") + "Off" + ".png");
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    }

    return pixmap;
}