#include "include/ui/mainwindow.h"

#include <QAction>
#include <QCursor>
#include <QMenu>
#include <QShortcut>

#include "include/sys/GlobalHotkeys.hpp"

QStringList MainWindow::RegisterHotkey(bool unregister) {
    if (!globalHotkeys) {
        globalHotkeys = new GlobalHotkeys(this);
        connect(globalHotkeys, &GlobalHotkeys::activated, this, &MainWindow::HotkeyEvent);
    }
    if (unregister || Configs::dataManager->settingsRepo->prepare_exit) {
        globalHotkeys->setActions({});
        return {};
    }

    const auto &settings = Configs::dataManager->settingsRepo;
    const QList<GlobalHotkeys::Action> actions{
        {"show-main-window", tr("Trigger main window"), QKeySequence(settings->hotkey_mainwindow)},
        {"show-groups", tr("Show groups"), QKeySequence(settings->hotkey_group)},
        {"show-routes", tr("Show routes"), QKeySequence(settings->hotkey_route)},
        {"proxy-mode-menu", tr("Proxy mode"), QKeySequence(settings->hotkey_system_proxy_menu)},
        {"toggle-system-proxy", tr("Toggle System Proxy"), QKeySequence(settings->hotkey_toggle_system_proxy)},
        {"toggle-connection", tr("Start/Stop Profile"), QKeySequence(settings->hotkey_toggle_connection)},
        {"toggle-tun", tr("Toggle Tun Mode"), QKeySequence(settings->hotkey_toggle_tun)},
    };
    const auto failures = globalHotkeys->setActions(actions);

    QStringList lines;
    for (const auto &action : actions) {
        if (!failures.contains(action.id)) continue;
        lines << QString("%1 (%2): %3").arg(action.text, action.key.toString(QKeySequence::NativeText), failures[action.id]);
        MW_show_log(tr("Global hotkey not registered: %1").arg(lines.last()));
    }
    return lines;
}

bool MainWindow::IsGlobalHotkeySupported() const {
    return globalHotkeys && globalHotkeys->isSupported();
}

void MainWindow::collectMenuShortcuts(QMenu *menu, QSet<QKeySequence> &out) {
    for (const auto &action: menu->actions()) {
        if (auto *sub = action->menu()) {
            collectMenuShortcuts(sub, out);
        } else {
            for (const auto &seq : action->shortcuts()) out.insert(seq);
        }
    }
}

void MainWindow::registerMenuShortcuts(QMenu *menu, QSet<QKeySequence> &claimed) {
    for (const auto &action: menu->actions()) {
        if (auto *sub = action->menu()) {
            registerMenuShortcuts(sub, claimed);
        } else {
            for (const auto &seq : action->shortcuts()) {
                if (claimed.contains(seq)) continue;
                claimed.insert(seq);
                hiddenMenuShortcuts.append(new QShortcut(seq, this, [=,this](){
                    action->trigger();
                }));
            }
        }
    }
}

void MainWindow::RegisterHiddenMenuShortcuts(bool unregister) {
    for (const auto s : hiddenMenuShortcuts) s->deleteLater();
    hiddenMenuShortcuts.clear();

    if (unregister) return;

    // A native menubar keeps its actions' shortcuts live even while hidden, so a second binding makes both ambiguous and kills them.
    if (ui->menubar->isNativeMenuBar()) return;

    // Menus on visible toolButtons already register their actions' shortcuts; seed to avoid duplicates.
    QSet<QKeySequence> claimed;
    collectMenuShortcuts(ui->menu_program, claimed);
    collectMenuShortcuts(ui->menu_preferences, claimed);
    collectMenuShortcuts(ui->menuRouting_Menu, claimed);
    collectMenuShortcuts(ui->menuTesting, claimed);
    collectMenuShortcuts(ui->menuTools, claimed);

    registerMenuShortcuts(ui->menuHidden_menu, claimed);
    registerMenuShortcuts(ui->menu_server, claimed);
}

void MainWindow::setActionsData()
{
    // Ids are the keys shortcuts are saved and restored under.
    ui->menu_add_from_input->setData(QString("m2"));
    ui->menu_clear_test_result->setData(QString("m3"));
    ui->menu_clone->setData(QString("m4"));
    ui->menu_delete_repeat->setData(QString("m6"));
    ui->menu_export_config->setData(QString("m7"));
    ui->menu_qr->setData(QString("m8"));
    ui->menu_remove_invalid->setData(QString("m9"));
    ui->menu_remove_unavailable->setData(QString("m10"));
    ui->menu_reset_traffic->setData(QString("m11"));
    ui->menu_resolve_domain->setData(QString("m12"));
    ui->menu_resolve_selected->setData(QString("m13"));
    ui->menu_scan_qr->setData(QString("m14"));
    ui->menu_stop_testing->setData(QString("m15"));
    ui->menu_update_subscription->setData(QString("m16"));
    ui->actionSpeedtest_Current->setData(QString("m18"));
    ui->actionSpeedtest_Group->setData(QString("m19"));
    ui->actionSpeedtest_Selected->setData(QString("m20"));
    ui->actionUrl_Test_Group->setData(QString("m21"));
    ui->actionUrl_Test_Selected->setData(QString("m22"));
    ui->actionHide_window->setData(QString("m23"));
    ui->actionAdd_profile_from_File->setData(QString("m24"));
    ui->actionRefresh_Column_Widths->setData(QString("m25"));
    ui->actionResolve_Out_IP->setData(QString("m26"));
    ui->actionResolve_Selected_Out_IP->setData(QString("m27"));
    ui->actionCopy_Test_Result->setData(QString("m28"));
    ui->actionClear_Test_Result->setData(QString("m29"));
    ui->menu_remove_insecure->setData(QString("m30"));
    ui->actionUpdate_All_Subscriptions->setData(QString("m31"));
    ui->menu_scanner->setData(QString("m32"));
    commandPaletteAction->setData(QString("m33"));
}

QList<QAction*> MainWindow::getActionsForShortcut()
{
    QList<QAction*> list;
    QList<QAction *> actions = findChildren<QAction *>();

    for (QAction *action : actions) {
        if (action->data().isNull() || action->data().toString().isEmpty()) continue;
        list.append(action);
    }
    return list;
}

void MainWindow::loadShortcuts()
{
    auto mp = Configs::dataManager->settingsRepo->shortcuts;
    for (QList<QAction *> actions = findChildren<QAction *>(); QAction *action : actions)
    {
        if (action->data().isNull() || action->data().toString().isEmpty()) continue;
        if (mp.count(action->data().toString()) > 0) {
            action->setShortcut(mp[action->data().toString()]);
        }
    }

#ifdef Q_OS_MACOS
    // After the loop: setShortcut above replaces the list.
    auto hideShortcuts = ui->actionHide_window->shortcuts();
    const QKeySequence hideSeq(Qt::ControlModifier | Qt::Key_W);
    if (!hideShortcuts.contains(hideSeq)) {
        hideShortcuts.append(hideSeq);
        ui->actionHide_window->setShortcuts(hideShortcuts);
    }
#endif

    RegisterHiddenMenuShortcuts();
}

void MainWindow::HotkeyEvent(const QString &id) {
    if (id == "show-main-window") {
        trayClickEvent();
    } else if (id == "show-groups") {
        on_menu_manage_groups_triggered();
    } else if (id == "show-routes") {
        on_menu_routing_settings_triggered();
    } else if (id == "proxy-mode-menu") {
        ui->menu_spmode->popup(QCursor::pos());
    } else if (id == "toggle-system-proxy") {
        toggle_system_proxy();
    } else if (id == "toggle-connection") {
        toggle_connection();
    } else if (id == "toggle-tun") {
        toggle_tun();
    }
}
