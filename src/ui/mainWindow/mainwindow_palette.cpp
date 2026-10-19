#include "include/ui/mainwindow.h"

#include <QAction>
#include <QDialog>
#include <QMenu>

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

namespace {
    // Why an action cannot run right now, or empty when it can.
    using Blocker = std::function<QString(const QAction *)>;

    void collectCommands(QMenu *menu, const QString &section, const QString &path, const QAction *skip, const Blocker &blocker,
                         QSet<const QAction *> &seen, QList<CommandPalette::Entry> &out) {
        // Menus that fill or enable their actions on demand.
        emit menu->aboutToShow();
        for (QAction *action : menu->actions()) {
            if (action->isSeparator() || !action->isVisible() || action == skip) continue;
            if (auto *sub = action->menu()) {
                collectCommands(sub, section, path + " › " + CommandPalette::plainText(sub->title()), skip, blocker, seen, out);
                continue;
            }
            const QString label = CommandPalette::plainText(action->text());
            if (label.isEmpty() || seen.contains(action)) continue;
            seen.insert(action);

            CommandPalette::Entry entry;
            entry.kind = CommandPalette::Entry::Command;
            entry.label = label;
            entry.path = path;
            entry.section = section;
            entry.keywords = CommandPalette::keywordsFor(action->objectName());
            entry.shortcut = action->shortcut().toString(QKeySequence::NativeText);
            entry.enabled = action->isEnabled();
            if (const QString reason = blocker(action); !reason.isEmpty()) {
                entry.enabled = false;
                entry.path += " · " + reason;
            }
            entry.checkable = action->isCheckable();
            entry.checked = action->isChecked();
            // Dynamic menus rebuild their actions every time they open.
            QPointer<QAction> guard(action);
            entry.run = [guard] {
                if (guard && guard->isEnabled()) guard->trigger();
            };
            out.append(entry);
        }
    }
}

void MainWindow::setupCommandPalette() {
    commandPaletteAction = new QAction(tr("Search Commands and Settings"), this);
    commandPaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    connect(commandPaletteAction, &QAction::triggered, this, &MainWindow::showCommandPalette);
    addAction(commandPaletteAction);
    CommandPalette::keepShortcutFromTextFields(commandPaletteAction, this);
    const auto toolsActions = ui->menuTools->actions();
    ui->menuTools->insertAction(toolsActions.value(0), commandPaletteAction);
    if (!toolsActions.isEmpty()) ui->menuTools->insertSeparator(toolsActions.first());

    // Placed in the group tabs' corner by the constructor: in the top bar it would widen the window's minimum.
    commandPaletteLauncher = new CommandPaletteLauncher(this);
    connect(commandPaletteLauncher, &CommandPaletteLauncher::launchRequested, this, &MainWindow::showCommandPalette);
}

void MainWindow::updateCommandPaletteHint() {
    if (!commandPaletteLauncher) return;
    const QString shortcut = commandPaletteAction->shortcut().toString(QKeySequence::NativeText);
    const QString text = shortcut.isEmpty() ? tr("Search commands") : tr("Search commands (%1)").arg(shortcut);
    commandPaletteLauncher->setPlaceholderText(text);
    commandPaletteLauncher->setToolTip(commandPaletteAction->text());
    commandPaletteLauncher->setFixedWidth(commandPaletteLauncher->fontMetrics().horizontalAdvance(text) + 24);
}

void MainWindow::showCommandPalette() {
    if (!commandPalette) commandPalette = new CommandPalette(this);

    const QHash<QString, QAction *> openers = {
        {DialogBasicSettings::staticMetaObject.className(), ui->menu_basic_settings},
        {DialogManageRoutes::staticMetaObject.className(), ui->menu_routing_settings},
        {DialogVPNSettings::staticMetaObject.className(), ui->menu_vpn_settings},
        {DialogPresetSettings::staticMetaObject.className(), ui->menu_preset_settings},
        {DialogIntegration::staticMetaObject.className(), ui->menu_integration_settings},
        {DialogManageGroups::staticMetaObject.className(), ui->menu_manage_groups},
        {DialogOtpManager::staticMetaObject.className(), ui->menu_otp_manager},
        {DialogTrafficStats::staticMetaObject.className(), ui->actionTraffic_Stats},
    };

    // Only one of these dialogs opens at a time: while one is open, the others' entries say which one to close.
    QString openClass;
    QString closeFirst;
    if (dialog_is_using) {
        for (auto *dialog : findChildren<QDialog *>(QString(), Qt::FindDirectChildrenOnly)) {
            if (!dialog->isVisible() || !openers.contains(QLatin1String(dialog->metaObject()->className()))) continue;
            openClass = dialog->metaObject()->className();
            closeFirst = tr("close \"%1\" first").arg(dialog->windowTitle());
            break;
        }
    }
    const auto blockedBy = [&](const QString &dialogClass) {
        return openClass.isEmpty() || dialogClass == openClass ? QString() : closeFirst;
    };
    const Blocker blocker = [&](const QAction *action) { return blockedBy(openers.key(const_cast<QAction *>(action))); };

    QList<CommandPalette::Entry> entries;
    QSet<const QAction *> seen;
    const QList<QPair<QString, QMenu *>> menus = {
        {ui->toolButton_program->text(), ui->menu_program},
        {ui->toolButton_preferences->text(), ui->menu_preferences},
        {ui->toolButton_testing->text(), ui->menuTesting},
        {ui->toolButton_routing->text(), ui->menuRouting_Menu},
        {ui->toolButton_tools->text(), ui->menuTools},
        {ui->menu_server->title(), ui->menu_server},
    };
    for (const auto &menu : menus) {
        const QString section = CommandPalette::plainText(menu.first);
        collectCommands(menu.second, section, section, commandPaletteAction, blocker, seen, entries);
    }

    if (settingsIndex.isEmpty()) settingsIndex = SettingsIndex::Build();
    for (const auto &item : settingsIndex) {
        QAction *opener = openers.value(item.dialogClass);
        if (!opener || !opener->isVisible()) continue;
        const QString title = CommandPalette::plainText(opener->text());

        CommandPalette::Entry entry;
        entry.kind = CommandPalette::Entry::Setting;
        entry.label = item.label;
        entry.path = item.path.isEmpty() ? title : title + " › " + item.path;
        entry.section = title;
        entry.keywords = item.keywords;
        if (const QString reason = blockedBy(item.dialogClass); !reason.isEmpty()) {
            entry.enabled = false;
            entry.path += " · " + reason;
        }
        entry.run = [this, opener, item] {
            opener->trigger();
            revealSetting(item);
        };
        entries.append(entry);
    }
    commandPalette->setEntries(entries);

    const int width = qBound(360, this->width() - 48, 640);
    commandPalette->popup(ui->splitter->mapToGlobal(QPoint(ui->splitter->width() / 2, 8)), width);
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
