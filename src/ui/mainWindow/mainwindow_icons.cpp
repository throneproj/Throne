#include "include/ui/mainwindow.h"

#include "include/ui/utils/ThemedIcon.hpp"

void MainWindow::setupThemedIcons() {
    ui->toolButton_program->setIcon(ThemedIcon(QStringLiteral("menu"), 215));
    ui->toolButton_preferences->setIcon(ThemedIcon(QStringLiteral("sliders-horizontal"), 270));
    ui->toolButton_testing->setIcon(ThemedIcon(QStringLiteral("folders"), 35));
    ui->toolButton_routing->setIcon(ThemedIcon(QStringLiteral("route"), 140));
    ui->toolButton_tools->setIcon(ThemedIcon(QStringLiteral("wrench"), 350));
}
