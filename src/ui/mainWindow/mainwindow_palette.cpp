#include "include/ui/mainwindow.h"

#include <QAction>
#include <QDialog>
#include <QMenu>

#include "include/ui/app/dialog_about.h"
#include "include/ui/group/dialog_manage_groups.h"
#include "include/ui/mainWindow/MainWindowInternal.h"
#include "include/ui/setting/dialog_basic_settings.h"
#include "include/ui/setting/dialog_integration.h"
#include "include/ui/setting/dialog_manage_routes.h"
#include "include/ui/setting/dialog_otp_manager.h"
#include "include/ui/setting/dialog_preset_settings.h"
#include "include/ui/setting/dialog_vpn_settings.h"
#include "include/ui/stats/dialog_traffic_stats.h"
#include "include/ui/utils/CommandPalette.h"
#include "include/ui/utils/SettingsIndex.h"

void MainWindow::setupCommandPalette() {
    commandPaletteAction = new QAction(tr("Search Settings"), this);
    commandPaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(commandPaletteAction, &QAction::triggered, this, &MainWindow::showCommandPalette);
    addAction(commandPaletteAction);
    CommandPalette::keepShortcutFromTextFields(commandPaletteAction, this);
    const auto toolsActions = ui->menuTools->actions();
    ui->menuTools->insertAction(toolsActions.value(0), commandPaletteAction);
    if (!toolsActions.isEmpty()) ui->menuTools->insertSeparator(toolsActions.first());
}

void MainWindow::createCommandPalette() {
    settingsDialogOpeners = {
        {DialogBasicSettings::staticMetaObject.className(), ui->menu_basic_settings},
        {DialogManageRoutes::staticMetaObject.className(), ui->menu_routing_settings},
        {DialogVPNSettings::staticMetaObject.className(), ui->menu_vpn_settings},
        {DialogPresetSettings::staticMetaObject.className(), ui->menu_preset_settings},
        {DialogIntegration::staticMetaObject.className(), ui->menu_integration_settings},
        {DialogManageGroups::staticMetaObject.className(), ui->menu_manage_groups},
        {DialogOtpManager::staticMetaObject.className(), ui->menu_otp_manager},
        {DialogTrafficStats::staticMetaObject.className(), ui->actionTraffic_Stats},
        {DialogAbout::staticMetaObject.className(), ui->actionAbout},
    };

    commandPalette = new CommandPalette(this,
                                        {
                                            {ui->toolButton_program->text(), ui->menu_program},
                                            {ui->toolButton_preferences->text(), ui->menu_preferences},
                                            {ui->toolButton_testing->text(), ui->menuTesting},
                                            {ui->toolButton_routing->text(), ui->menuRouting_Menu},
                                            {ui->toolButton_tools->text(), ui->menuTools},
                                            {ui->menu_server->title(), ui->menu_server},
                                        },
                                        commandPaletteAction);

    // Reading the settings forms costs a few milliseconds, so it waits for the first open.
    QList<CommandPalette::Entry> settings;
    for (const auto &item : SettingsIndex::Build()) {
        QAction *opener = settingsDialogOpeners.value(item.dialogClass);
        if (!opener) continue;
        const QString title = CommandPalette::plainText(opener->text());

        CommandPalette::Entry entry;
        entry.kind = CommandPalette::Entry::Setting;
        entry.label = item.label;
        entry.path = item.path.isEmpty() ? title : title + " › " + item.path;
        entry.section = title;
        entry.keywords = item.keywords;
        entry.action = opener;
        entry.run = [this, opener, item] {
            opener->trigger();
            revealSetting(item);
        };
        settings.append(entry);
    }
    commandPalette->setSettings(settings);
}

void MainWindow::showCommandPalette() {
    if (commandPalette && commandPalette->isVisible()) {
        commandPalette->raise();
        commandPalette->activateWindow();
        return;
    }
    if (!commandPalette) createCommandPalette();

    // Only one of these dialogs opens at a time: while one is open, the others' entries say which one to close.
    // The palette stays open beside the main window and asks again each time setDialogInUse() flips the slot.
    commandPalette->present([this]() -> CommandPalette::Blocker {
        if (!dialog_is_using) return {};
        QString openClass;
        QString closeFirst = tr("close the open settings window first");
        for (auto *dialog : findChildren<QDialog *>(QString(), Qt::FindDirectChildrenOnly)) {
            if (!dialog->isVisible() || !settingsDialogOpeners.contains(QLatin1String(dialog->metaObject()->className()))) continue;
            openClass = dialog->metaObject()->className();
            closeFirst = tr("close \"%1\" first").arg(dialog->windowTitle());
            break;
        }
        return [openers = settingsDialogOpeners, openClass, closeFirst](const QAction *action) {
            const QString dialogClass = openers.key(const_cast<QAction *>(action));
            return dialogClass.isEmpty() || dialogClass == openClass ? QString() : closeFirst;
        };
    });
}

void MainWindow::setDialogInUse(bool inUse) {
    dialog_is_using = inUse;
    if (commandPalette) commandPalette->blockersChanged();
}

void MainWindow::revealSetting(const SettingsIndex::Item &item) {
    // Settings dialogs are parented to the main window; a busy slot leaves the open one in place.
    for (auto *dialog : findChildren<QDialog *>(QString(), Qt::FindDirectChildrenOnly)) {
        if (!dialog->isVisible() || item.dialogClass != QLatin1String(dialog->metaObject()->className())) continue;
        dialog->raise();
        dialog->activateWindow();
        SettingsIndex::Reveal(dialog, item.objectName);
        return;
    }
}
