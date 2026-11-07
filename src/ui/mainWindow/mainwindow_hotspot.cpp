#include "include/ui/mainwindow.h"

#ifdef Q_OS_WIN
#include "include/engine/connection/ConnectionManager.h"
#include "include/sys/WindowsHotspot.h"
#include "include/global/Logger.hpp"
#include <QMessageBox>
#include <QSignalBlocker>
#endif

void MainWindow::setupHotspot() {
#ifdef Q_OS_WIN
    m_hotspot = new WindowsHotspot(this);
    const auto refresh = [this] {
        const QSignalBlocker blocker(ui->checkBox_Hotspot);
        ui->checkBox_Hotspot->setChecked(m_hotspot->checked());
        ui->checkBox_Hotspot->setEnabled(m_hotspot->available() && !m_hotspot->busy());
        ui->checkBox_Hotspot->setToolTip(m_hotspot->hint());
    };
    connect(m_hotspot, &WindowsHotspot::changed, this, refresh);
    connect(m_hotspot, &WindowsHotspot::logLine, this, [](const QString &line) { MW_show_log(line); });
    connect(m_hotspot, &WindowsHotspot::problem, this, [this](const QString &error) {
        QMessageBox::warning(this, tr("Mobile hotspot"), error);
    });
    connect(ui->checkBox_Hotspot, &QCheckBox::clicked, m_hotspot, &WindowsHotspot::setEnabled);
    const auto syncTun = [this] {
        m_hotspot->setTunActive(Configs::dataManager->settingsRepo->spmode_vpn &&
                               connection()->connectionState() == ConnectionState::Running);
    };
    connect(connection(), &Engine::ConnectionManager::connectionStateChanged, this, [syncTun](Engine::ConnectionState) { syncTun(); });
    connect(connection(), &Engine::ConnectionManager::statusChanged, this, syncTun);
    syncTun();
    refresh();
#else
    ui->checkBox_Hotspot->hide();
#endif
}

bool MainWindow::prepareForTunStop() {
#ifdef Q_OS_WIN
    return m_hotspot == nullptr || m_hotspot->ensureStopped();
#else
    return true;
#endif
}
