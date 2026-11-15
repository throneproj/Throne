#include "include/ui/mainwindow.h"

#include "include/ui/utils/ThemedIcon.hpp"

void MainWindow::setupThemedIcons() {
    ui->toolButton_program->setIcon(ThemedIcon(QStringLiteral("menu")));
    ui->toolButton_preferences->setIcon(ThemedIcon(QStringLiteral("sliders-horizontal")));
    ui->toolButton_testing->setIcon(ThemedIcon(QStringLiteral("folders")));
    ui->toolButton_routing->setIcon(ThemedIcon(QStringLiteral("route")));
    ui->toolButton_tools->setIcon(ThemedIcon(QStringLiteral("wrench")));
}
