#include "include/ui/widget/TrayIcon.hpp"

#ifndef Q_OS_MAC
TrayIcon::TrayIcon(QObject *parent) : QObject(parent), m_tray(new QSystemTrayIcon(this)) {
    connect(m_tray, &QSystemTrayIcon::activated, this, &TrayIcon::activated);
}

TrayIcon::~TrayIcon() = default;

void TrayIcon::setIcon(const QIcon &icon) { m_tray->setIcon(icon); }
void TrayIcon::setToolTip(const QString &text) { m_tray->setToolTip(text); }
void TrayIcon::setContextMenu(QMenu *menu) { m_tray->setContextMenu(menu); }
void TrayIcon::setVisible(bool visible) { m_tray->setVisible(visible); }
bool TrayIcon::isVisible() const { return m_tray->isVisible(); }
#endif
