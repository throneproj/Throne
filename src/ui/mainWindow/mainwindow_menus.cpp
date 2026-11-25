#include "include/ui/mainwindow.h"

#include <QActionGroup>
#include <QCoreApplication>
#include <QItemSelectionModel>
#include <QMenu>
#include <QStyleOptionMenuItem>

#include "include/configs/generate.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/engine/EditLocks.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/engine/services/GroupService.h"
#include "include/engine/services/ImportService.h"
#include "include/engine/services/ProfileService.h"
#include "include/engine/services/SettingsService.h"
#include "include/engine/services/WarpService.h"
#include "include/global/RunningProfiles.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/group/dialog_edit_group.h"
#include "include/ui/setting/dialog_rule_tester.h"
#include "include/ui/utils/GuiUtils.hpp"

namespace {
    QString moveRefusalText(const Engine::ServiceError &error) {
        if (error.code == QLatin1String("editing")) return MainWindow::tr("A selected profile is open in an editor. Close it first.");
        if (error.code == QLatin1String("busy")) return QObject::tr("This group's subscription is being updated. Try again when it finishes.");
        if (error.code == QLatin1String("subscription_group")) {
            return MainWindow::tr("A subscription group's next update removes profiles moved into it. Choose another group.");
        }
        return ImportTargetRefusalText(error);
    }

    // Empty when no message is due: Throne is exiting.
    QString warpRefusalText(const Engine::ServiceError &refusal) {
        if (refusal.code == QLatin1String("exiting")) return {};
        return refusal.code == QLatin1String("editing")
                   ? MainWindow::tr("Routing Settings is open; close it and try again.")
                   : MainWindow::tr("WARP has no config yet: click Generate WARP config in Routing Settings > Exit.");
    }

    // Cut by characters, not pixels, since the shell may draw the tray menu; '&' doubled so it shows as itself.
    QString trayMenuName(QString name) {
        constexpr qsizetype kMaxLength = 40;
        if (name.size() > kMaxLength) {
            qsizetype cut = kMaxLength - 1;
            if (name.at(cut - 1).isHighSurrogate()) --cut;
            name = name.left(cut) + QChar(0x2026);
        }
        return name.replace(QLatin1Char('&'), QStringLiteral("&&"));
    }

    QString trayProfileName(const std::shared_ptr<Configs::Profile> &profile) {
        return trayMenuName(profile->outbound != nullptr ? profile->outbound->DisplayName() : profile->name);
    }

    constexpr char trayConnectedProperty[] = "trayConnected";

    // Marks the connected status line :read-only, which the theme sheet tints green; a native or D-Bus tray menu shows it plain.
    class TrayMenu : public QMenu {
    protected:
        void initStyleOption(QStyleOptionMenuItem *option, const QAction *action) const override {
            QMenu::initStyleOption(option, action);
            if (action->property(trayConnectedProperty).toBool()) option->state |= QStyle::State_ReadOnly;
        }
    };
}

void MainWindow::showProfilesContextMenu(const QPoint &pos) {
    auto *view = ui->profilesTableView;
    const QPoint globalPos = view->viewport()->mapToGlobal(pos);
    const QModelIndex index = view->indexAt(pos);
    if (!index.isValid()) {
        ui->menu_profiles_space->popup(globalPos);
        return;
    }

    auto *selection = view->selectionModel();
    if (!selection->isSelected(index)) selection->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    const auto running = runningSnapshot();
    const bool runningSelected = running != nullptr && get_now_selected_list().contains(running->id);
    ui->menu_start->setVisible(!runningSelected);
    ui->menu_stop->setVisible(runningSelected);
    ui->menu_find_warp_endpoint->setVisible(warpFinderTarget() != nullptr);

    const auto moveTo = [this](const QList<int> &ids, int gid) {
        if (const auto refusal = Engine::ProfileService::MoveToGroup(ids, gid)) MessageBoxWarning(tr("Move to group"), moveRefusalText(*refusal));
    };
    auto *moveMenu = ui->menu_move_to_group;
    moveMenu->clear();
    const int currentGid = Configs::dataManager->settingsRepo->current_group;
    for (const int gid : Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
        // Lists only the targets the service accepts: no subscription or archived groups.
        const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (group == nullptr || gid == currentGid || Engine::ImportService::CheckTarget(gid)) continue;
        connect(moveMenu->addAction(QString(group->name).replace(QLatin1Char('&'), QStringLiteral("&&"))), &QAction::triggered, this,
                [this, moveTo, gid] { moveTo(get_now_selected_list(), gid); });
    }
    moveMenu->addSeparator();
    connect(moveMenu->addAction(tr("New group")), &QAction::triggered, this, [this, moveTo] {
        const auto ids = get_now_selected_list();
        auto ent = Configs::dataManager->groupsRepo->NewGroup();
        auto dialog = new DialogEditGroup(ent, this);
        int ret = dialog->exec();
        dialog->deleteLater();

        if (ret == QDialog::Accepted) {
            if (const auto error = Engine::GroupService::Add(ent)) {
                MW_show_log(error->message);
                return;
            }
            moveTo(ids, ent->id);
        }
    });

    ui->menu_server->popup(globalPos);
}

void MainWindow::on_menu_server_aboutToHide() {
    // Shortcuts trigger hidden actions all the same; shown again so the command palette lists both.
    ui->menu_start->setVisible(true);
    ui->menu_stop->setVisible(true);
    ui->menu_find_warp_endpoint->setVisible(true);
}

void MainWindow::on_menu_edit_triggered() {
    const auto *selection = ui->profilesTableView->selectionModel();
    if (selection == nullptr || !selection->hasSelection()) return;
    QModelIndex index = selection->currentIndex();
    if (!selection->isSelected(index)) index = selection->selectedRows().value(0);
    on_profilesTableView_doubleClicked(index);
}

void MainWindow::on_menu_add_subscription_triggered() {
    auto ent = Configs::dataManager->groupsRepo->NewGroup();
    auto dialog = new DialogEditGroup(ent, this);
    dialog->StartAsSubscription();
    int ret = dialog->exec();
    dialog->deleteLater();

    if (ret == QDialog::Accepted) {
        if (const auto error = Engine::GroupService::Add(ent)) MW_show_log(error->message);
    }
}

void MainWindow::setupRoutingMenu() {
    auto *menu = ui->menuRouting_Menu;

    // A shortcut triggers these without the menu's aboutToShow resync, so the checked state may be stale.
    connect(ui->actionAdblock, &QAction::triggered, this, [this] {
        const bool enable = !Configs::dataManager->settingsRepo->adblock_enable;
        Engine::SettingsService::ApplyResult result;
        Engine::SettingsService::Update([enable](Configs::SettingsValues &values) { values.adblock_enable = enable; }, {.interactive = true}, &result);
        ui->actionAdblock->setChecked(Configs::dataManager->settingsRepo->adblock_enable);
        if (!result.changed.isEmpty()) connection()->applyPendingRestart();
    });

    connect(ui->actionWarp, &QAction::triggered, this, [this] {
        const auto warpIsExit = [] { return Configs::ExitOf(Configs::dataManager->settingsRepo->exit_through) == Configs::ExitThrough::Warp; };
        Engine::WarpService::ChangeResult result;
        const auto refused = Engine::WarpService::Apply({.enabled = !warpIsExit()}, {}, &result);
        ui->actionWarp->setChecked(warpIsExit());
        if (refused) {
            if (const QString reason = warpRefusalText(*refused); !reason.isEmpty()) MessageBoxWarning(ui->actionWarp->text(), reason);
            return;
        }
        if (result.restartNeeded) connection()->applyPendingRestart();
    });

    const QList<std::pair<QAction *, Configs::ExitThrough>> exitActions{
        {ui->menu_exit_server, Configs::ExitThrough::Server},
        {ui->menu_exit_warp, Configs::ExitThrough::Warp},
    };
    const auto syncExitChecks = [exitActions] {
        const auto exit = Configs::ExitOf(Configs::dataManager->settingsRepo->exit_through);
        for (const auto &[action, value] : exitActions) action->setChecked(value == exit);
    };
    const auto setExit = [this](Configs::ExitThrough exit) {
        Engine::SettingsService::ApplyResult result;
        Engine::SettingsService::Update([exit](Configs::SettingsValues &values) { values.exit_through = Configs::ExitValue(exit); },
                                        {.interactive = true}, &result);
        if (!result.changed.isEmpty()) connection()->applyPendingRestart();
    };
    const auto chooseExit = [this, syncExitChecks, setExit](QAction *clicked, Configs::ExitThrough wanted) {
        // The exclusive group has already checked the clicked item.
        syncExitChecks();
        if (Configs::ExitOf(Configs::dataManager->settingsRepo->exit_through) == wanted) return;
        if (Engine::IsBeingEdited(Engine::EditTarget::Routes)) {
            MessageBoxWarning(clicked->text(), tr("Routing Settings is open; close it and try again."));
            return;
        }
        if (wanted == Configs::ExitThrough::Warp) {
            Engine::WarpService::ChangeResult warpResult;
            if (const auto refused = Engine::WarpService::Apply({.enabled = true}, {}, &warpResult)) {
                if (const QString reason = warpRefusalText(*refused); !reason.isEmpty()) MessageBoxWarning(clicked->text(), reason);
                return;
            }
            if (warpResult.restartNeeded) connection()->applyPendingRestart();
        } else {
            setExit(wanted);
        }
        syncExitChecks();
    };
    auto *exitMenu = new QMenu(QCoreApplication::translate("DialogManageRoutes", "Exit through"), menu);
    auto *exitGroup = new QActionGroup(exitMenu);
    for (const auto &entry : exitActions) {
        QAction *item = entry.first;
        const auto exit = entry.second;
        exitGroup->addAction(item);
        exitMenu->addAction(item);
        connect(item, &QAction::triggered, this, [chooseExit, item, exit] { chooseExit(item, exit); });
    }

    auto *downloadMenu = new QMenu(tr("Download routing profile"), menu);
    const QList<QPair<QString, QString>> routeCountries{{"China", tr("China")}, {"Iran", tr("Iran")}, {"Russia", tr("Russia")}};
    for (const auto &entry : routeCountries) {
        const QString country = entry.first;
        const QString label = entry.second;
        auto *action = downloadMenu->addAction(label);
        connect(action, &QAction::triggered, this, [this, action, country, label] {
            action->setEnabled(false);
            action->setText(tr("%1 (downloading)").arg(label));
            runOnNewThread([this, guard = QPointer<QAction>(action), country, label] {
                const auto resp = NetworkRequestHelper::HttpGet(Configs::get_jsdelivr_link("https://raw.githubusercontent.com/throneproj/routeprofiles/profile/Profile_" + country));
                runOnUiThread([this, guard, label, resp] {
                    if (!guard) return;
                    guard->setText(label);
                    guard->setEnabled(true);
                    if (!resp.error.isEmpty()) {
                        MessageBoxWarning(tr("Download routing profile"), QObject::tr("Requesting profile error: %1").arg(resp.error + "\n" + resp.data));
                        return;
                    }
                    handle_add_remote_routes(resp.data);
                });
            });
        });
    }

    connect(ui->menu_edit_route, &QAction::triggered, this, [this] { openRouteProfileEditor(Configs::ActiveRouteId()); });
    connect(ui->actionRule_Tester, &QAction::triggered, this, [this] {
        const auto selected = get_now_selected_list();
        const int preferred = selected.size() == 1 ? selected.first() : -1;
        if (m_ruleTesterDialog == nullptr) {
            m_ruleTesterDialog = new DialogRuleTester(connection(), preferred, this);
            m_ruleTesterDialog->setAttribute(Qt::WA_DeleteOnClose);
        } else {
            m_ruleTesterDialog->setPreferredProfile(preferred);
        }
        ActivateWindow(m_ruleTesterDialog);
    });

    auto *routesHeading = new QAction(tr("Choose profile:"), menu);
    routesHeading->setEnabled(false);
    routesHeading->setProperty("menuHeading", true);
    auto *moreRoutesMenu = new QMenu(tr("More"), menu);
    moreRoutesMenu->installEventFilter(this);

    menu->addAction(ui->menu_routing_settings);
    menu->addAction(ui->menu_edit_route);
    menu->addMenu(exitMenu);
    menu->addAction(ui->actionAdblock);
    menu->addSeparator();
    menu->addAction(routesHeading);
    QAction *moreRoutesAction = menu->addMenu(moreRoutesMenu);
    menu->addSeparator();
    menu->addAction(ui->actionRule_Tester);
    menu->addAction(ui->actionUpdate_Rule_Sets);
    menu->addMenu(downloadMenu);

    auto *routeGroup = new QActionGroup(menu);
    const auto syncRouteChecks = [routeGroup] {
        const int current = Configs::dataManager->settingsRepo->current_route_id;
        for (auto *action : routeGroup->actions()) action->setChecked(action->data().toInt() == current);
    };
    connect(menu, &QMenu::aboutToShow, this, [this, menu, routesHeading, moreRoutesMenu, moreRoutesAction, routeGroup, syncExitChecks, syncRouteChecks] {
        ui->actionAdblock->setChecked(Configs::dataManager->settingsRepo->adblock_enable);
        syncExitChecks();
        refresh_action_states();

        // Kept across shows unless the profiles' ids or names changed: the command palette holds on to these actions.
        auto routeActions = routeGroup->actions();
        const auto routes = Configs::dataManager->routesRepo->GetAllRouteProfiles();
        bool changed = routes.size() != routeActions.size();
        for (int i = 0; !changed && i < routes.size(); ++i) {
            changed = routeActions[i]->data().toInt() != routes[i]->id || routeActions[i]->text() != routes[i]->name;
        }
        if (changed) {
            for (auto *action : routeActions) delete action;
            for (const auto &route : routes) {
                auto *action = new QAction(route->name, menu);
                action->setData(route->id);
                action->setProperty("routeProfileId", route->id);
                action->setCheckable(true);
                routeGroup->addAction(action);
                connect(action, &QAction::triggered, this, [this, action, syncRouteChecks] {
                    connection()->chooseRoute(action->data().toInt());
                    syncRouteChecks();
                });
            }
            routeActions = routeGroup->actions();
        }

        // At most five rows, "More" counting as one; the current profile always keeps a row.
        constexpr qsizetype maxRouteRows = 5;
        const qsizetype shownCount = routeActions.size() <= maxRouteRows ? routeActions.size() : maxRouteRows - 1;
        QList<QAction *> shown = routeActions.mid(0, shownCount);
        QList<QAction *> hidden = routeActions.mid(shownCount);
        const int current = Configs::dataManager->settingsRepo->current_route_id;
        for (qsizetype i = 0; i < hidden.size(); ++i) {
            if (hidden[i]->data().toInt() != current) continue;
            QAction *pinned = hidden.takeAt(i);
            hidden.prepend(shown.takeLast());
            shown.append(pinned);
            break;
        }
        const auto menuActions = menu->actions();
        const qsizetype first = menuActions.indexOf(routesHeading) + 1;
        if (menuActions.mid(first, menuActions.indexOf(moreRoutesAction) - first) != shown || moreRoutesMenu->actions() != hidden) {
            for (auto *action : routeActions) {
                menu->removeAction(action);
                moreRoutesMenu->removeAction(action);
            }
            menu->insertActions(moreRoutesAction, shown);
            moreRoutesMenu->addActions(hidden);
        }
        routesHeading->setVisible(!routeActions.isEmpty());
        moreRoutesAction->setVisible(!hidden.isEmpty());
        syncRouteChecks();
    });
}

void MainWindow::setupTrayMenu() {
    trayMenu = new TrayMenu();
    trayStatusAction = new QAction(trayMenu);
    trayStatusAction->setEnabled(false);
    trayMenu->addAction(trayStatusAction);

    trayConnectAction = new QAction(tr("Connect"), trayMenu);
    connect(trayConnectAction, &QAction::triggered, this, [this]() {
        if (runningSnapshot() != nullptr) profile_stop(false, false, true);
        else profile_start();
    });
    trayMenu->addAction(trayConnectAction);
    trayMenu->addSeparator();

    trayProfileAction = new QAction(trayMenu);
    connect(trayProfileAction, &QAction::triggered, this, [this]() { openTraySelector(false); });
    trayMenu->addAction(trayProfileAction);
    trayRoutingAction = new QAction(trayMenu);
    connect(trayRoutingAction, &QAction::triggered, this, [this]() { openTraySelector(true); });
    trayMenu->addAction(trayRoutingAction);
    trayMenu->addSeparator();

    trayMenu->addAction(ui->menu_spmode_system_proxy);
    trayMenu->addAction(ui->menu_spmode_vpn);
    trayMenu->addSeparator();

    auto *actOtpCodes = new QAction(tr("OTP codes"), trayMenu);
    connect(actOtpCodes, &QAction::triggered, this, [this]() { openTrayOtpCodes(); });
    trayMenu->addAction(actOtpCodes);
    trayMenu->addAction(ui->actionShow_window);
    auto *preferencesMenu = trayMenu->addMenu(tr("Program"));
    preferencesMenu->addAction(ui->actionStart_with_system);
    preferencesMenu->addAction(ui->actionRemember_last_proxy);
    preferencesMenu->addAction(ui->actionAllow_LAN);
    auto *restartMenu = trayMenu->addMenu(tr("Restart"));
    restartMenu->addAction(ui->actionRestart_Proxy);
    restartMenu->addAction(ui->actionRestart_Program);
    trayMenu->addAction(ui->menu_exit);

    refreshTrayMenu();
}

void MainWindow::refreshTrayMenu() {
    if (trayMenu == nullptr) return;
    const auto &settings = Configs::dataManager->settingsRepo;
    const auto running = runningSnapshot();
    trayStatusAction->setProperty(trayConnectedProperty, running != nullptr);
    trayStatusAction->setText(running != nullptr ? tr("Connected · %1").arg(trayProfileName(running)) : tr("Not connected"));

    auto profile = running;
    if (profile == nullptr) {
        if (const int id = get_profile_to_start(); id >= 0) profile = Configs::dataManager->profilesRepo->GetProfile(id);
    }
    trayProfileAction->setText(profile != nullptr ? tr("Profile: %1").arg(trayProfileName(profile)) : tr("Select profile"));

    auto routeName = Configs::dataManager->routesRepo->GetRouteProfileName(Configs::ActiveRouteId());
    if (!routeName) routeName = Configs::dataManager->routesRepo->GetRouteProfileName(settings->current_route_id);
    trayRoutingAction->setText(tr("Routing: %1").arg(routeName ? trayMenuName(*routeName) : QString()));

    ui->menu_spmode_system_proxy->setChecked(settings->spmode_system_proxy);
    ui->menu_spmode_vpn->setChecked(settings->spmode_vpn);
}
