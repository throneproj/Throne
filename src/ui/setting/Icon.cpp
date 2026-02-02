#include "include/ui/setting/Icon.hpp"

#include "include/global/Configs.hpp"

#include <QPainter>


QPixmap Icon::GetTrayIcon(TrayIconStatus status) {
    QPixmap pixmap;
    QPixmap pixmap_read;

    if (status == NONE)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Off" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Throne/") + "Off" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == RUNNING)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Throne" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Throne/") + "Throne" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == SYSTEM_PROXY_DNS)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Proxy-Dns" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Throne/") + "Proxy-Dns" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == SYSTEM_PROXY)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Proxy" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Throne/") + "Proxy" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == DNS)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Dns" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Throne/") + "Dns" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else if (status == VPN)
    {
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Tun" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Throne/") + "Tun" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    } else
    {
        MW_show_log("Icon::GetTrayIcon: Unknown status");
        if (Configs::dataManager->settingsRepo->use_custom_icons) {
            pixmap_read = QPixmap(QString("icons/") + "Off" + ".png");
        }
        if (pixmap_read.isNull()) {
            pixmap_read = QPixmap(QString(":/Throne/") + "Off" + ".png");
        }
        if (!pixmap_read.isNull()) pixmap = pixmap_read;
    }

    return pixmap;
}
