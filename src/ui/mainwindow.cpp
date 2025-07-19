#include "include/ui/mainwindow.h"

#include "include/dataStore/ProfileFilter.hpp"
#include "include/configs/ConfigBuilder.hpp"
#include "include/configs/sub/GroupUpdater.hpp"
#include "include/sys/Process.hpp"
#include "include/sys/AutoRun.hpp"

#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/Icon.hpp"
#include "include/ui/profile/dialog_edit_profile.h"
#include "include/ui/setting/dialog_basic_settings.h"
#include "include/ui/group/dialog_manage_groups.h"
#include "include/ui/setting/dialog_manage_routes.h"
#include "include/ui/setting/dialog_vpn_settings.h"
#include "include/ui/setting/dialog_hotkey.h"

#include "3rdparty/qrcodegen.hpp"
#include "3rdparty/qv2ray/v2/ui/LogHighlighter.hpp"
#include "3rdparty/QrDecoder.h"
#include "include/ui/group/dialog_edit_group.h"

#ifdef Q_OS_WIN
#include "3rdparty/WinCommander.hpp"
#include <sdkddkver.h>
#include <minwindef.h>
#include <winbase.h>
#include <VersionHelpers.h>
#else
#ifdef Q_OS_LINUX
#include "include/sys/linux/LinuxCap.h"
#include "include/sys/linux/desktopinfo.h"
#include <QDBusInterface>
#include <QDBusReply>
#include <QUuid>
#endif
#include <unistd.h>
#endif

#include <QClipboard>
#include <QLabel>
#include <QTextBlock>
#include <QScrollBar>
#include <QScreen>
#include <QDesktopServices>
#include <QInputDialog>
#include <QThread>
#include <QTimer>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif
#include <QToolTip>
#include <random>
#include <3rdparty/QHotkey/qhotkey.h>
#include <include/api/gRPC.h>
#include <include/global/HTTPRequestHelper.hpp>

#include "include/sys/macos/MacOS.h"

void UI_InitMainWindow() {
    mainwindow = new MainWindow;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    mainwindow = this;
    MW_dialog_message = [=](const QString &a, const QString &b) {
        runOnUiThread([=]
        {
            dialog_message_impl(a, b);
        });
    };

    // Load Manager
    Configs::profileManager->LoadManager();

    // Setup misc UI
    // migrate old themes
    bool isNum;
    Configs::dataStore->theme.toInt(&isNum);
    if (isNum) {
        Configs::dataStore->theme = "System";
    }
    themeManager->ApplyTheme(Configs::dataStore->theme);
    ui->setupUi(this);

    // setup log
    ui->splitter->restoreState(DecodeB64IfValid(Configs::dataStore->splitter_state));
    new SyntaxHighlighter(isDarkMode() || Configs::dataStore->theme.toLower() == "qdarkstyle", qvLogDocument);
    qvLogDocument->setUndoRedoEnabled(false);
    ui->masterLogBrowser->setUndoRedoEnabled(false);
    ui->masterLogBrowser->setDocument(qvLogDocument);
    ui->masterLogBrowser->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [=](const Qt::ColorScheme& scheme) {
        new SyntaxHighlighter(scheme == Qt::ColorScheme::Dark, qvLogDocument);
        themeManager->ApplyTheme(Configs::dataStore->theme, true);
    });
#endif
    connect(themeManager, &ThemeManager::themeChanged, this, [=](const QString& theme){
        if (theme.toLower().contains("vista")) {
            // light themes
            new SyntaxHighlighter(false, qvLogDocument);
        } else if (theme.toLower().contains("qdarkstyle")) {
            // dark themes
            new SyntaxHighlighter(true, qvLogDocument);
        } else {
            // bi-mode themes, follow system preference
            new SyntaxHighlighter(isDarkMode(), qvLogDocument);
        }
    });
    connect(ui->masterLogBrowser->verticalScrollBar(), &QSlider::valueChanged, this, [=](int value) {
        if (ui->masterLogBrowser->verticalScrollBar()->maximum() == value)
            qvLogAutoScoll = true;
        else
            qvLogAutoScoll = false;
    });
    connect(ui->masterLogBrowser, &QTextBrowser::textChanged, this, [=]() {
        if (!qvLogAutoScoll)
            return;
        auto bar = ui->masterLogBrowser->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
    MW_show_log = [=](const QString &log) {
        runOnUiThread([=] { show_log_impl(log); });
    };

    // Prepare core
    Configs::dataStore->core_port = MkPort();
    if (Configs::dataStore->core_port <= 0) Configs::dataStore->core_port = 19810;

    auto core_path = QApplication::applicationDirPath() + "/";
    core_path += "Core";

    QStringList args;
    args.push_back("-port");
    args.push_back(Int2String(Configs::dataStore->core_port));
    if (Configs::dataStore->flag_debug) args.push_back("-debug");

    // Start core
    runOnUiThread(
        [=] {
            core_process = new Configs_sys::CoreProcess(core_path, args);
            // Remember last started
            if (Configs::dataStore->remember_enable && Configs::dataStore->remember_id >= 0) {
                core_process->start_profile_when_core_is_up = Configs::dataStore->remember_id;
            }
            // Setup
            setup_grpc();
            core_process->Start();
        },
        DS_cores);

    if (!Configs::dataStore->font.isEmpty()) {
        auto font = qApp->font();
        font.setFamily(Configs::dataStore->font);
        qApp->setFont(font);
    }
    if (Configs::dataStore->font_size != 0) {
        auto font = qApp->font();
        font.setPointSize(Configs::dataStore->font_size);
        qApp->setFont(font);
    }

    speedTestThreadPool->setMaxThreadCount(10); // constant value
    //
    connect(ui->menu_start, &QAction::triggered, this, [=]() { profile_start(); });
    connect(ui->menu_stop, &QAction::triggered, this, [=]() { profile_stop(false, false, true); });
    connect(ui->tabWidget->tabBar(), &QTabBar::tabMoved, this, [=](int from, int to) {
        // use tabData to track tab & gid
        Configs::profileManager->groupsTabOrder.clear();
        for (int i = 0; i < ui->tabWidget->tabBar()->count(); i++) {
            Configs::profileManager->groupsTabOrder += ui->tabWidget->tabBar()->tabData(i).toInt();
        }
        Configs::profileManager->SaveManager();
    });
    ui->label_running->installEventFilter(this);
    ui->label_inbound->installEventFilter(this);
    ui->splitter->installEventFilter(this);
    ui->tabWidget->installEventFilter(this);
    //
    RegisterHotkey(false);
    RegisterShortcuts();
    //
    auto last_size = Configs::dataStore->mw_size.split("x");
    if (last_size.length() == 2) {
        auto w = last_size[0].toInt();
        auto h = last_size[1].toInt();
        if (w > 0 && h > 0) {
            resize(w, h);
        }
    }

    // software_name
    software_name = "Throne";
    software_core_name = "sing-box";
    //
    if (auto dashDir = QDir("dashboard"); !dashDir.exists("dashboard") && QDir().mkdir("dashboard")) {
        if (auto dashFile = QFile(":/Throne/dashboard-notice.html"); dashFile.exists() && dashFile.open(QIODevice::ReadOnly))
        {
            auto data = dashFile.readAll();
            if (auto dest = QFile("dashboard/index.html"); dest.open(QIODevice::Truncate | QIODevice::WriteOnly))
            {
                dest.write(data);
                dest.close();
            }
            dashFile.close();
        }
    }

    // top bar
    ui->toolButton_program->setMenu(ui->menu_program);
    ui->toolButton_preferences->setMenu(ui->menu_preferences);
    ui->toolButton_server->setMenu(ui->menu_server);
    ui->toolButton_routing->setMenu(ui->menuRouting_Menu);
    ui->menubar->setVisible(false);
    connect(ui->toolButton_update, &QToolButton::clicked, this, [=] { runOnNewThread([=] { CheckUpdate(); }); });

    // setup connection UI
    setupConnectionList();
    for (int i=0;i<ui->stats_widget->tabBar()->count();i++)
    {
        if (ui->stats_widget->tabBar()->tabText(i) == Configs::dataStore->stats_tab)
        {
            ui->stats_widget->tabBar()->setCurrentIndex(i);
            break;
        }
    }
    connect(ui->stats_widget->tabBar(), &QTabBar::currentChanged, this, [=](int index)
    {
        auto tabText = ui->stats_widget->tabBar()->tabText(index);
        Configs::dataStore->stats_tab = tabText;
    });
    connect(ui->connections->horizontalHeader(), &QHeaderView::sectionClicked, this, [=](int index)
    {
        // TODO this is a very bad idea to hardcode it like this, need to refactor it later
        if (index == 0)
        {
            Stats::connection_lister->setSort(Stats::Default);
            Stats::connection_lister->ForceUpdate();
        }
        if (index == 2 || index == 3)
        {
            // ignore
            return;
        }
        if (index == 1)
        {
            Stats::connection_lister->setSort(Stats::ByProcess);
            Stats::connection_lister->ForceUpdate();
        }
        if (index == 4)
        {
            Stats::connection_lister->setSort(Stats::ByTraffic);
            Stats::connection_lister->ForceUpdate();
        }
    });

    // setup Speed Chart
    speedChartWidget = new SpeedWidget(this);
    ui->graph_tab->layout()->addWidget(speedChartWidget);

    // table UI
    ui->proxyListTable->rowsSwapped = [=](int row1, int row2)
    {
        auto group = Configs::profileManager->CurrentGroup();
        group->SwapProfiles(row1, row2);
        refresh_proxy_list(group->profiles[row1]);
        refresh_proxy_list(group->profiles[row2]);
        group->Save();
    };
    if (auto button = ui->proxyListTable->findChild<QAbstractButton *>(QString(), Qt::FindDirectChildrenOnly)) {
        // Corner Button
        connect(button, &QAbstractButton::clicked, this, [=] { refresh_proxy_list_impl(-1, {GroupSortMethod::ById}); });
    }
    connect(ui->proxyListTable->horizontalHeader(), &QHeaderView::sectionClicked, this, [=](int logicalIndex) {
        GroupSortAction action;
        if (proxy_last_order == logicalIndex) {
            action.descending = true;
            proxy_last_order = -1;
        } else {
            proxy_last_order = logicalIndex;
        }
        action.save_sort = true;
        if (logicalIndex == 0) {
            action.method = GroupSortMethod::ByType;
        } else if (logicalIndex == 1) {
            action.method = GroupSortMethod::ByAddress;
        } else if (logicalIndex == 2) {
            action.method = GroupSortMethod::ByName;
        } else if (logicalIndex == 3) {
            action.method = GroupSortMethod::ByLatency;
        } else {
            return;
        }
        refresh_proxy_list_impl(-1, action);
        Configs::profileManager->CurrentGroup()->Save();
    });
    connect(ui->proxyListTable->horizontalHeader(), &QHeaderView::sectionResized, this, [=](int logicalIndex, int oldSize, int newSize) {
        auto group = Configs::profileManager->CurrentGroup();
        if (Configs::dataStore->refreshing_group || group == nullptr || !group->manually_column_width) return;
        // save manually column width
        group->column_width.clear();
        for (int i = 0; i < ui->proxyListTable->horizontalHeader()->count(); i++) {
            group->column_width.push_back(ui->proxyListTable->horizontalHeader()->sectionSize(i));
        }
        group->column_width[logicalIndex] = newSize;
        group->Save();
    });
    ui->proxyListTable->verticalHeader()->setDefaultSectionSize(24);
    ui->proxyListTable->setTabKeyNavigation(false);

    // search box
    connect(shortcut_esc, &QShortcut::activated, this, [=] {
        if (select_mode) {
            emit profile_selected(-1);
            select_mode = false;
            refresh_status();
        }
    });

    // refresh
    this->refresh_groups();

    // Setup Tray
    tray = new QSystemTrayIcon(nullptr);
    tray->setIcon(GetTrayIcon(Icon::NONE));
    auto *trayMenu = new QMenu();
    trayMenu->addAction(ui->actionShow_window);
    trayMenu->addSeparator();
    trayMenu->addAction(ui->actionStart_with_system);
    trayMenu->addAction(ui->actionRemember_last_proxy);
    trayMenu->addAction(ui->actionAllow_LAN);
    trayMenu->addSeparator();
    trayMenu->addMenu(ui->menu_spmode);
    trayMenu->addSeparator();
    trayMenu->addAction(ui->actionRestart_Proxy);
    trayMenu->addAction(ui->actionRestart_Program);
    trayMenu->addAction(ui->menu_exit);
    tray->setVisible(!Configs::dataStore->disable_tray);
    tray->setContextMenu(trayMenu);
    connect(tray, &QSystemTrayIcon::activated, qApp, [=](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            ActivateWindow(this);
        }
    });

    // Misc menu
    ui->actionRemember_last_proxy->setChecked(Configs::dataStore->remember_enable);
    ui->actionStart_with_system->setChecked(AutoRun_IsEnabled());
    ui->actionAllow_LAN->setChecked(QStringList{"::", "0.0.0.0"}.contains(Configs::dataStore->inbound_address));

    connect(ui->menu_open_config_folder, &QAction::triggered, this, [=] { QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::currentPath())); });
    connect(ui->menu_add_from_clipboard2, &QAction::triggered, ui->menu_add_from_clipboard, &QAction::trigger);
    connect(ui->actionRestart_Proxy, &QAction::triggered, this, [=] { if (Configs::dataStore->started_id>=0) profile_start(Configs::dataStore->started_id); });
    connect(ui->actionRestart_Program, &QAction::triggered, this, [=] { MW_dialog_message("", "RestartProgram"); });
    connect(ui->actionShow_window, &QAction::triggered, this, [=] { ActivateWindow(this); });
    connect(ui->actionRemember_last_proxy, &QAction::triggered, this, [=](bool checked) {
        Configs::dataStore->remember_enable = checked;
        ui->actionRemember_last_proxy->setChecked(checked);
        Configs::dataStore->Save();
    });
    connect(ui->actionStart_with_system, &QAction::triggered, this, [=](bool checked) {
        AutoRun_SetEnabled(checked);
        ui->actionStart_with_system->setChecked(checked);
    });
    connect(ui->actionAllow_LAN, &QAction::triggered, this, [=](bool checked) {
        Configs::dataStore->inbound_address = checked ? "::" : "127.0.0.1";
        ui->actionAllow_LAN->setChecked(checked);
        MW_dialog_message("", "UpdateDataStore");
    });
    //
    connect(ui->checkBox_VPN, &QCheckBox::clicked, this, [=](bool checked) { set_spmode_vpn(checked); });
    connect(ui->checkBox_SystemProxy, &QCheckBox::clicked, this, [=](bool checked) { set_spmode_system_proxy(checked); });
    connect(ui->menu_spmode, &QMenu::aboutToShow, this, [=]() {
        ui->menu_spmode_disabled->setChecked(!(Configs::dataStore->spmode_system_proxy || Configs::dataStore->spmode_vpn));
        ui->menu_spmode_system_proxy->setChecked(Configs::dataStore->spmode_system_proxy);
        ui->menu_spmode_vpn->setChecked(Configs::dataStore->spmode_vpn);
    });
    connect(ui->menu_spmode_system_proxy, &QAction::triggered, this, [=](bool checked) { set_spmode_system_proxy(checked); });
    connect(ui->menu_spmode_vpn, &QAction::triggered, this, [=](bool checked) { set_spmode_vpn(checked); });
    connect(ui->menu_spmode_disabled, &QAction::triggered, this, [=]() {
        set_spmode_system_proxy(false);
        set_spmode_vpn(false);
    });
    connect(ui->menu_qr, &QAction::triggered, this, [=]() { display_qr_link(false); });
    connect(ui->system_dns, &QCheckBox::clicked, this, [=](bool checked) {
        if (const auto ok = set_system_dns(checked); !ok) {
            ui->system_dns->setChecked(!checked);
        } else {
            refresh_status();
        }
    });
    // only windows is supported for now
#ifndef Q_OS_WIN
    ui->system_dns->hide();
#endif

    connect(ui->menu_server, &QMenu::aboutToShow, this, [=](){
        if (running)
        {
            ui->actionSpeedtest_Current->setEnabled(true);
        } else
        {
            ui->actionSpeedtest_Current->setEnabled(false);
        }
        if (auto selected = get_now_selected_list(); selected.empty())
        {
            ui->actionSpeedtest_Selected->setEnabled(false);
            ui->actionUrl_Test_Selected->setEnabled(false);
            ui->menu_resolve_selected->setEnabled(false);
        } else
        {
            ui->actionSpeedtest_Selected->setEnabled(true);
            ui->actionUrl_Test_Selected->setEnabled(true);
            ui->menu_resolve_selected->setEnabled(true);
        }
        if (!speedtestRunning.tryLock()) {
            ui->menu_server->addAction(ui->menu_stop_testing);
        } else {
            speedtestRunning.unlock();
            ui->menu_server->removeAction(ui->menu_stop_testing);
        }
    });
    connect(ui->menuRouting_Menu, &QMenu::aboutToShow, this, [=]()
    {
        ui->menuRouting_Menu->clear();
        ui->menuRouting_Menu->addAction(ui->menu_routing_settings);
        ui->menuRouting_Menu->addSeparator();
        for (const auto& route : Configs::profileManager->routes)
        {
            auto* action = new QAction(ui->menuRouting_Menu);
            action->setText(route.second->name);
            action->setData(route.second->id);
            action->setCheckable(true);
            action->setChecked(Configs::dataStore->routing->current_route_id == route.first);
            connect(action, &QAction::triggered, this, [=]()
            {
                auto routeID = action->data().toInt();
                if (Configs::dataStore->routing->current_route_id == routeID) return;
                Configs::dataStore->routing->current_route_id = routeID;
                Configs::dataStore->routing->Save();
                if (Configs::dataStore->started_id >= 0) profile_start(Configs::dataStore->started_id);
            });
            ui->menuRouting_Menu->addAction(action);
        }
    });
    connect(ui->actionUrl_Test_Selected, &QAction::triggered, this, [=]() {
        urltest_current_group(get_now_selected_list());
    });
    connect(ui->actionUrl_Test_Group, &QAction::triggered, this, [=]() {
        urltest_current_group(Configs::profileManager->CurrentGroup()->GetProfileEnts());
    });
    connect(ui->actionSpeedtest_Current, &QAction::triggered, this, [=]()
    {
        if (running != nullptr)
        {
            speedtest_current_group({}, true);
        }
    });
    connect(ui->actionSpeedtest_Selected, &QAction::triggered, this, [=]()
    {
        speedtest_current_group(get_now_selected_list());
    });
    connect(ui->actionSpeedtest_Group, &QAction::triggered, this, [=]()
    {
        speedtest_current_group(Configs::profileManager->CurrentGroup()->GetProfileEnts());
    });
    connect(ui->menu_stop_testing, &QAction::triggered, this, [=]() { stopTests(); });
    //
    auto set_selected_or_group = [=](int mode) {
        // 0=group 1=select 2=unknown(menu is hide)
        ui->menu_server->setProperty("selected_or_group", mode);
    };
    connect(ui->menu_server, &QMenu::aboutToHide, this, [=] {
        setTimeout([=] { set_selected_or_group(2); }, this, 200);
    });
    set_selected_or_group(2);
    //
    connect(ui->menu_share_item, &QMenu::aboutToShow, this, [=] {
        QString name;
        auto selected = get_now_selected_list();
        if (!selected.isEmpty()) {
            auto ent = selected.first();
            name = ent->bean->DisplayCoreType();
        }
        ui->menu_export_config->setVisible(name == software_core_name);
        ui->menu_export_config->setText(tr("Export %1 config").arg(name));
    });
    refresh_status();

    connect(qApp, &QGuiApplication::commitDataRequest, this, &MainWindow::on_commitDataRequest);

    auto t = new QTimer;
    connect(t, &QTimer::timeout, this, [=]() { refresh_status(); });
    t->start(2000);

    t = new QTimer;
    connect(t, &QTimer::timeout, this, [&] { Configs_sys::logCounter.fetchAndStoreRelaxed(0); });
    t->start(1000);

    TM_auto_update_subsctiption = new QTimer;
    TM_auto_update_subsctiption_Reset_Minute = [&](int m) {
        TM_auto_update_subsctiption->stop();
        if (m >= 30) TM_auto_update_subsctiption->start(m * 60 * 1000);
    };
    connect(TM_auto_update_subsctiption, &QTimer::timeout, this, [&] { UI_update_all_groups(true); });
    TM_auto_update_subsctiption_Reset_Minute(Configs::dataStore->sub_auto_update);

    if (!Configs::dataStore->flag_tray) show();

    if (Configs::NeedGeoAssets()) {
        auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Geo Assets are missing, want to download them now?"), QMessageBox::Yes | QMessageBox::No);
        if (n == QMessageBox::Yes) {
            runOnNewThread([=]
            {
                DownloadAssets(!Configs::dataStore->geoip_download_url.isEmpty() ? Configs::dataStore->geoip_download_url : Configs::GeoAssets::GeoIPURLs[0],
                !Configs::dataStore->geosite_download_url.isEmpty() ? Configs::dataStore->geosite_download_url : Configs::GeoAssets::GeoSiteURLs[0]);
            });
        }
    }
    ui->data_view->setStyleSheet("background: transparent; border: none;");
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (tray->isVisible()) {
        hide();
        event->ignore();
    } else {
        on_menu_exit_triggered();
    }
}

MainWindow::~MainWindow() {
    delete ui;
}

// Group tab manage

inline int tabIndex2GroupId(int index) {
    if (Configs::profileManager->groupsTabOrder.length() <= index) return -1;
    return Configs::profileManager->groupsTabOrder[index];
}

inline int groupId2TabIndex(int gid) {
    for (int key = 0; key < Configs::profileManager->groupsTabOrder.count(); key++) {
        if (Configs::profileManager->groupsTabOrder[key] == gid) return key;
    }
    return 0;
}

void MainWindow::on_tabWidget_currentChanged(int index) {
    if (Configs::dataStore->refreshing_group_list) return;
    if (tabIndex2GroupId(index) == Configs::dataStore->current_group) return;
    show_group(tabIndex2GroupId(index));
}

void MainWindow::show_group(int gid) {
    if (Configs::dataStore->refreshing_group) return;
    Configs::dataStore->refreshing_group = true;

    auto group = Configs::profileManager->GetGroup(gid);
    if (group == nullptr) {
        MessageBoxWarning(tr("Error"), QString("No such group: %1").arg(gid));
        Configs::dataStore->refreshing_group = false;
        return;
    }

    if (Configs::dataStore->current_group != gid) {
        Configs::dataStore->current_group = gid;
        Configs::dataStore->Save();
    }

    ui->tabWidget->widget(groupId2TabIndex(gid))->layout()->addWidget(ui->proxyListTable);

    if (group->manually_column_width) {
        for (int i = 0; i <= 4; i++) {
            ui->proxyListTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
            auto size = group->column_width.value(i);
            if (size <= 0) size = ui->proxyListTable->horizontalHeader()->defaultSectionSize();
            ui->proxyListTable->horizontalHeader()->resizeSection(i, size);
        }
    } else {
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    }

    // show proxies
    GroupSortAction gsa;
    gsa.scroll_to_started = true;
    refresh_proxy_list_impl(-1, gsa);

    Configs::dataStore->refreshing_group = false;
}

// callback

void MainWindow::dialog_message_impl(const QString &sender, const QString &info) {
    // info
    if (info.contains("UpdateIcon")) {
        icon_status = -1;
        refresh_status();
    }
    if (info.contains("UpdateDataStore")) {
        if (info.contains("UpdateDisableTray")) {
            tray->setVisible(!Configs::dataStore->disable_tray);
        }
        auto suggestRestartProxy = Configs::dataStore->Save();
        if (info.contains("RouteChanged")) {
            Configs::dataStore->routing->Save();
            suggestRestartProxy = true;
        }
        if (info.contains("NeedRestart")) {
            suggestRestartProxy = false;
        }
        refresh_proxy_list();
        if (info.contains("VPNChanged") && Configs::dataStore->spmode_vpn) {
            MessageBoxWarning(tr("Tun Settings changed"), tr("Restart Tun to take effect."));
        }
        if (suggestRestartProxy && Configs::dataStore->started_id >= 0 &&
            QMessageBox::question(GetMessageBoxParent(), tr("Confirmation"), tr("Settings changed, restart proxy?")) == QMessageBox::StandardButton::Yes) {
            profile_start(Configs::dataStore->started_id);
        }
        refresh_status();
    }
    if (info.contains("DNSServerChanged"))
    {
        if (Configs::dataStore->system_dns_set)
        {
            auto oldAddr = info.split(",")[1];
            set_system_dns(false);
            set_system_dns(true);
        }
    }
    if (info.contains("NeedRestart")) {
        auto n = QMessageBox::warning(GetMessageBoxParent(), tr("Settings changed"), tr("Restart the program to take effect."), QMessageBox::Yes | QMessageBox::No);
        if (n == QMessageBox::Yes) {
            this->exit_reason = 2;
            on_menu_exit_triggered();
        }
    }
    if (info.contains("DownloadAssets")) {
        auto splitted = info.split(";");
        runOnNewThread([=](){
            DownloadAssets(splitted[1], splitted[2]);
        });
    }
    //
    if (info == "RestartProgram") {
        this->exit_reason = 2;
        on_menu_exit_triggered();
    }
    if (info == "Raise") {
        ActivateWindow(this);
    }
    if (info == "NeedAdmin") {
        get_elevated_permissions();
    }
    // sender
    if (sender == Dialog_DialogEditProfile) {
        auto msg = info.split(",");
        if (msg.contains("accept")) {
            refresh_proxy_list();
            if (msg.contains("restart")) {
                if (QMessageBox::question(GetMessageBoxParent(), tr("Confirmation"), tr("Settings changed, restart proxy?")) == QMessageBox::StandardButton::Yes) {
                    profile_start(Configs::dataStore->started_id);
                }
            }
        }
    } else if (sender == Dialog_DialogManageGroups) {
        if (info.startsWith("refresh")) {
            this->refresh_groups();
        }
    } else if (sender == "SubUpdater") {
        if (info.startsWith("finish")) {
            refresh_proxy_list();
            if (!info.contains("dingyue")) {
                show_log_impl(tr("Imported %1 profile(s)").arg(Configs::dataStore->imported_count));
            }
        } else if (info == "NewGroup") {
            refresh_groups();
        }
    } else if (sender == "ExternalProcess") {
        if (info == "Crashed") {
            profile_stop();
        } else if (info == "CoreCrashed") {
            profile_stop(true);
        } else if (info.startsWith("CoreStarted")) {
            Configs::IsAdmin(true);
            if (Configs::dataStore->remember_enable || Configs::dataStore->flag_restart_tun_on) {
                if (Configs::dataStore->remember_spmode.contains("system_proxy")) {
                    set_spmode_system_proxy(true, false);
                }
                if (Configs::dataStore->remember_spmode.contains("vpn") || Configs::dataStore->flag_restart_tun_on) {
                    set_spmode_vpn(true, false);
                }
                if (Configs::dataStore->flag_dns_set) {
                    set_system_dns(true);
                }
            }
            if (auto id = info.split(",")[1].toInt(); id >= 0)
            {
                profile_start(id);
            }
            if (Configs::dataStore->system_dns_set) {
                set_system_dns(true);
                ui->system_dns->setChecked(true);
            }
        }
    }
}

// top bar & tray menu

inline bool dialog_is_using = false;

#define USE_DIALOG(a)                               \
    if (dialog_is_using) return;                    \
    dialog_is_using = true;                         \
    auto dialog = new a(this);                      \
    connect(dialog, &QDialog::finished, this, [=] { \
        dialog->deleteLater();                      \
        dialog_is_using = false;                    \
    });                                             \
    dialog->show();

void MainWindow::on_menu_basic_settings_triggered() {
    USE_DIALOG(DialogBasicSettings)
}

void MainWindow::on_menu_manage_groups_triggered() {
    USE_DIALOG(DialogManageGroups)
}

void MainWindow::on_menu_routing_settings_triggered() {
    USE_DIALOG(DialogManageRoutes)
}

void MainWindow::on_menu_vpn_settings_triggered() {
    USE_DIALOG(DialogVPNSettings)
}

void MainWindow::on_menu_hotkey_settings_triggered() {
    USE_DIALOG(DialogHotkey)
}

void MainWindow::on_commitDataRequest() {
    qDebug() << "Handling DNS setting";
    if (Configs::dataStore->system_dns_set) set_system_dns(false, false);
    qDebug() << "Done handling DNS setting";
    qDebug() << "Start of data save";
    //
    if (!isMaximized()) {
        auto olds = Configs::dataStore->mw_size;
        auto news = QString("%1x%2").arg(size().width()).arg(size().height());
        if (olds != news) {
            Configs::dataStore->mw_size = news;
        }
    }
    //
    Configs::dataStore->splitter_state = ui->splitter->saveState().toBase64();
    //
    auto last_id = Configs::dataStore->started_id;
    if (Configs::dataStore->remember_enable && last_id >= 0) {
        Configs::dataStore->remember_id = last_id;
    }
    //
    Configs::dataStore->Save();
    Configs::profileManager->SaveManager();
    qDebug() << "End of data save";
}

void MainWindow::prepare_exit()
{
    qDebug() << "prepare for exit...";
    mu_exit.lock();
    if (Configs::dataStore->prepare_exit)
    {
        qDebug() << "prepare exit had already succeeded, ignoring...";
        mu_exit.unlock();
        return;
    }
    hide();
    tray->hide();
    Configs::dataStore->prepare_exit = true;
    //
    set_spmode_system_proxy(false, false);
    RegisterHotkey(true);
    //
    on_commitDataRequest();
    //
    if (running) running->Save();
    Configs::dataStore->save_control_no_save = true; // don't change datastore after this line
    API::defaultClient->Exit();
    mu_exit.unlock();
    qDebug() << "prepare exit done!";
}

void MainWindow::on_menu_exit_triggered() {
    prepare_exit();
    //
    if (exit_reason == 1) {
        QDir::setCurrent(QApplication::applicationDirPath());
        QProcess::startDetached("./updater", QStringList{});
    } else if (exit_reason == 2 || exit_reason == 3 || exit_reason == 4) {
        QDir::setCurrent(QApplication::applicationDirPath());

        auto arguments = Configs::dataStore->argv;
        if (arguments.length() > 0) {
            arguments.removeFirst();
            arguments.removeAll("-tray");
            arguments.removeAll("-flag_restart_tun_on");
            arguments.removeAll("-flag_restart_dns_set");
        }
        auto program = QApplication::applicationFilePath();

        if (exit_reason == 3 || exit_reason == 4) {
            if (exit_reason == 3) arguments << "-flag_restart_tun_on";
            if (exit_reason == 4) arguments << "-flag_restart_dns_set";
#ifdef Q_OS_WIN
            WinCommander::runProcessElevated(program, arguments, "", WinCommander::SW_NORMAL, false);
#else
            QProcess::startDetached(program, arguments);
#endif
        } else {
            QProcess::startDetached(program, arguments);
        }
    }
    QCoreApplication::quit();
}

void MainWindow::toggle_system_proxy() {
    auto currentState = Configs::dataStore->spmode_system_proxy;
    if (currentState) {
        set_spmode_system_proxy(false);
    } else {
        set_spmode_system_proxy(true);
    }
}

bool MainWindow::get_elevated_permissions(int reason) {
    if (Configs::dataStore->disable_privilege_req)
    {
        MW_show_log(tr("User opted for no privilege req, some features may not work"));
        return true;
    }
    if (Configs::IsAdmin()) return true;
#ifdef Q_OS_LINUX
    if (!Linux_HavePkexec()) {
        MessageBoxWarning(software_name, "Please install \"pkexec\" first.");
        return false;
    }
    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please give the core root privileges"), QMessageBox::Yes | QMessageBox::No);
    if (n == QMessageBox::Yes) {
        runOnNewThread([=]
        {
            auto chownArgs = QString("root:root " + Configs::FindCoreRealPath());
            auto ret = Linux_Run_Command("chown", chownArgs);
            if (ret != 0) {
                MW_show_log(QString("Failed to run chown %1 code is %2").arg(chownArgs).arg(ret));
            }
            auto chmodArgs = QString("u+s " + Configs::FindCoreRealPath());
            ret = Linux_Run_Command("chmod", chmodArgs);
            if (ret == 0) {
                StopVPNProcess();
            } else {
                MW_show_log(QString("Failed to run chmod %1").arg(chmodArgs));
            }
        });
        return false;
    }
#endif
#ifdef Q_OS_WIN
    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please run Throne as admin"), QMessageBox::Yes | QMessageBox::No);
    if (n == QMessageBox::Yes) {
        this->exit_reason = reason;
        on_menu_exit_triggered();
    }
#endif

#ifdef Q_OS_MACOS
    if (Configs::IsAdmin(true))
    {
        Configs::IsAdmin(true);
        StopVPNProcess();
        return true;
    }
    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please give the core root privileges"), QMessageBox::Yes | QMessageBox::No);
    if (n == QMessageBox::Yes)
    {
        auto Command = QString("sudo chown root:wheel " + Configs::FindCoreRealPath() + " && " + "sudo chmod u+s "+Configs::FindCoreRealPath());
        auto ret = Mac_Run_Command(Command);
        if (ret == 0) {
            MessageBoxInfo(tr("Requesting permission"), tr("Please Enter your password in the opened terminal, then try again"));
            return false;
        } else {
            MW_show_log(QString("Failed to run %1 with %2").arg(Command).arg(ret));
            return false;
        }
    }
#endif
    return false;
}

void MainWindow::set_spmode_vpn(bool enable, bool save) {
    if (enable == Configs::dataStore->spmode_vpn) return;

    if (enable) {
        bool requestPermission = !Configs::IsAdmin();
        if (requestPermission) {
            if (!get_elevated_permissions()) {
                refresh_status();
                return;
            }
        }
    }

    if (save) {
        Configs::dataStore->remember_spmode.removeAll("vpn");
        if (enable && Configs::dataStore->remember_enable) {
            Configs::dataStore->remember_spmode.append("vpn");
        }
        Configs::dataStore->Save();
    }

    Configs::dataStore->spmode_vpn = enable;
    refresh_status();

    if (Configs::dataStore->started_id >= 0) profile_start(Configs::dataStore->started_id);
}

void MainWindow::UpdateDataView(bool force)
{
    if (!force && lastUpdated.msecsTo(QDateTime::currentDateTime()) < 100)
    {
        return;
    }
    QString html;
    if (showDownloadData)
    {
        qint64 count = 10*currentDownloadReport.downloadedSize / currentDownloadReport.totalSize;
        QString progressText;
        for (int i = 0; i < 10; i++)
        {
            if (count--; count >=0) progressText += "#";
            else progressText += "-";
        }
        QString stat = ReadableSize(currentDownloadReport.downloadedSize) + "/" + ReadableSize(currentDownloadReport.totalSize);
        html = QString("<p style='text-align:center;margin:0;'>Downloading %1: %2 %3</p>").arg(currentDownloadReport.fileName, stat, progressText);
    }
    if (showSpeedtestData)
    {
        html += QString(
    "<p style='text-align:center;margin:0;'>Running Speedtest: %1</p>"
    "<div style='text-align: center;'>"
    "<span style='color: #3299FF;'>Dl↓ %2</span>  "
    "<span style='color: #86C43F;'>Ul↑ %3</span>"
    "</div>"
    "<p style='text-align:center;margin:0;'>Server: %4, %5</p>"
        ).arg(currentSptProfileName,
            currentTestResult.dl_speed.value().c_str(),
            currentTestResult.ul_speed.value().c_str(),
            currentTestResult.server_country.value().c_str(),
            currentTestResult.server_name.value().c_str());
    }
    ui->data_view->setHtml(html);
    lastUpdated = QDateTime::currentDateTime();
}

void MainWindow::setDownloadReport(const DownloadProgressReport& report, bool show)
{
    showDownloadData = show;
    currentDownloadReport = report;
}


void MainWindow::setupConnectionList()
{
    ui->connections->horizontalHeader()->setHighlightSections(false);
    ui->connections->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->connections->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->connections->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->connections->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->connections->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->connections->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->connections->verticalHeader()->hide();
    connect(ui->connections, &QTableWidget::cellClicked, this, [=](int row, int column)
    {
        if (column > 3) return;
        auto selected = ui->connections->item(row, column);
        QApplication::clipboard()->setText(selected->text());
        QPoint pos = ui->connections->mapToGlobal(ui->connections->visualItemRect(selected).center());
        QToolTip::showText(pos, "Copied!", this);
        auto r = ++toolTipID;
        QTimer::singleShot(1500, [=] {
            if (r != toolTipID)
            {
                return;
            }
            QToolTip::hideText();
        });
    });
}

void MainWindow::UpdateConnectionList(const QMap<QString, Stats::ConnectionMetadata>& toUpdate, const QMap<QString, Stats::ConnectionMetadata>& toAdd)
{
    ui->connections->setUpdatesEnabled(false);
    for (int row=0;row<ui->connections->rowCount();row++)
    {
        auto key = ui->connections->item(row, 0)->data(Stats::IDKEY).toString();
        if (!toUpdate.contains(key))
        {
            ui->connections->removeRow(row);
            row--;
            continue;
        }

        auto conn = toUpdate[key];
        // C0: Dest (Domain)
        ui->connections->item(row, 0)->setText(DisplayDest(conn.dest, conn.domain));

        // C1: Process
        ui->connections->item(row, 1)->setText(conn.process);

        // C2: Protocol
        auto prot = conn.network;
        if (!conn.protocol.isEmpty()) prot += " ("+conn.protocol+")";
        ui->connections->item(row, 2)->setText(prot);

        // C3: Outbound
        ui->connections->item(row, 3)->setText(conn.outbound);

        // C4: Traffic
        ui->connections->item(row, 4)->setText(ReadableSize(conn.upload) + "↑" + " " + ReadableSize(conn.download) + "↓");
    }
    int row = ui->connections->rowCount();
    for (const auto& conn : toAdd)
    {
        ui->connections->insertRow(row);
        auto f0 = std::make_unique<QTableWidgetItem>();
        f0->setData(Stats::IDKEY, conn.id);

        // C0: Dest (Domain)
        auto f = f0->clone();
        f->setText(DisplayDest(conn.dest, conn.domain));
        ui->connections->setItem(row, 0, f);

        // C1: Process
        f = f0->clone();
        f->setText(conn.process);
        ui->connections->setItem(row, 1, f);

        // C2: Protocol
        f = f0->clone();
        auto prot = conn.network;
        if (!conn.protocol.isEmpty()) prot += " ("+conn.protocol+")";
        f->setText(prot);
        ui->connections->setItem(row, 2, f);

        // C3: Outbound
        f = f0->clone();
        f->setText(conn.outbound);
        ui->connections->setItem(row, 3, f);

        // C4: Traffic
        f = f0->clone();
        f->setText(ReadableSize(conn.upload) + "↑" + " " + ReadableSize(conn.download) + "↓");
        ui->connections->setItem(row, 4, f);

        row++;
    }
    ui->connections->setUpdatesEnabled(true);
}

void MainWindow::UpdateConnectionListWithRecreate(const QList<Stats::ConnectionMetadata>& connections)
{
    ui->connections->setUpdatesEnabled(false);
    ui->connections->setRowCount(0);
    int row=0;
    for (const auto& conn : connections)
    {
        ui->connections->insertRow(row);
        auto f0 = std::make_unique<QTableWidgetItem>();
        f0->setData(Stats::IDKEY, conn.id);

        // C0: Dest (Domain)
        auto f = f0->clone();
        f->setText(DisplayDest(conn.dest, conn.domain));
        ui->connections->setItem(row, 0, f);

        // C1: Process
        f = f0->clone();
        f->setText(conn.process);
        ui->connections->setItem(row, 1, f);

        // C2: Protocol
        f = f0->clone();
        auto prot = conn.network;
        if (!conn.protocol.isEmpty()) prot += " ("+conn.protocol+")";
        f->setText(prot);
        ui->connections->setItem(row, 2, f);

        // C3: Outbound
        f = f0->clone();
        f->setText(conn.outbound);
        ui->connections->setItem(row, 3, f);

        // C4: Traffic
        f = f0->clone();
        f->setText(ReadableSize(conn.upload) + "↑" + " " + ReadableSize(conn.download) + "↓");
        ui->connections->setItem(row, 4, f);

        row++;
    }
    ui->connections->setUpdatesEnabled(true);
}

void MainWindow::refresh_status(const QString &traffic_update) {
    auto refresh_speed_label = [=] {
        if (Configs::dataStore->disable_traffic_stats) {
            ui->label_speed->setText("");
        }
        else if (traffic_update_cache == "") {
            ui->label_speed->setText(QObject::tr("Proxy: %1\nDirect: %2").arg("", ""));
        } else {
            ui->label_speed->setText(traffic_update_cache);
        }
    };

    // From TrafficLooper
    if (!traffic_update.isEmpty() && !Configs::dataStore->disable_traffic_stats) {
        traffic_update_cache = traffic_update;
        if (traffic_update == "STOP") {
            traffic_update_cache = "";
        } else {
            refresh_speed_label();
            return;
        }
    }

    refresh_speed_label();

    // From UI
    QString group_name;
    if (running != nullptr) {
        auto group = Configs::profileManager->GetGroup(running->gid);
        if (group != nullptr) group_name = group->name;
    }

    if (last_test_time.addSecs(2) < QTime::currentTime()) {
        auto txt = running == nullptr ? tr("Not Running")
                                      : QString("[%1] %2").arg(group_name, running->bean->DisplayName()).left(30);
        ui->label_running->setText(txt);
    }
    //
    auto display_socks = DisplayAddress(Configs::dataStore->inbound_address, Configs::dataStore->inbound_socks_port);
    auto inbound_txt = QString("Mixed: %1").arg(display_socks);
    ui->label_inbound->setText(inbound_txt);
    //
    ui->checkBox_VPN->setChecked(Configs::dataStore->spmode_vpn);
    ui->checkBox_SystemProxy->setChecked(Configs::dataStore->spmode_system_proxy);
    if (select_mode) {
        ui->label_running->setText(tr("Select") + " *");
        ui->label_running->setToolTip(tr("Select mode, double-click or press Enter to select a profile, press ESC to exit."));
    } else {
        ui->label_running->setToolTip({});
    }

    auto make_title = [=](bool isTray) {
        QStringList tt;
        if (!isTray && Configs::IsAdmin()) tt << "[Admin]";
        if (select_mode) tt << "[" + tr("Select") + "]";
        if (!title_error.isEmpty()) tt << "[" + title_error + "]";
        if (Configs::dataStore->spmode_vpn && !Configs::dataStore->spmode_system_proxy) tt << "[Tun]";
        if (!Configs::dataStore->spmode_vpn && Configs::dataStore->spmode_system_proxy) tt << "[" + tr("System Proxy") + "]";
        if (Configs::dataStore->spmode_vpn && Configs::dataStore->spmode_system_proxy) tt << "[Tun+" + tr("System Proxy") + "]";
        tt << software_name;
        if (!isTray) tt << QString(NKR_VERSION);
        if (!Configs::dataStore->active_routing.isEmpty() && Configs::dataStore->active_routing != "Default") {
            tt << "[" + Configs::dataStore->active_routing + "]";
        }
        if (running != nullptr) tt << running->bean->DisplayTypeAndName() + "@" + group_name;
        return tt.join(isTray ? "\n" : " ");
    };

    auto icon_status_new = Icon::NONE;

    if (running != nullptr) {
        if (Configs::dataStore->spmode_vpn) {
            icon_status_new = Icon::VPN;
        } else if (Configs::dataStore->system_dns_set && Configs::dataStore->spmode_system_proxy) {
            icon_status_new = Icon::SYSTEM_PROXY_DNS;
        } else if (Configs::dataStore->system_dns_set) {
            icon_status_new = Icon::DNS;
        } else if (Configs::dataStore->spmode_system_proxy) {
            icon_status_new = Icon::SYSTEM_PROXY;
        } else {
            icon_status_new = Icon::RUNNING;
        }
    }

    // refresh title & window icon
    setWindowTitle(make_title(false));
    if (icon_status_new != icon_status) QApplication::setWindowIcon(GetTrayIcon(Icon::RUNNING));

    // refresh tray
    if (tray != nullptr) {
        tray->setToolTip(make_title(true));
        if (icon_status_new != icon_status) tray->setIcon(Icon::GetTrayIcon(icon_status_new));
    }

    icon_status = icon_status_new;
}

void MainWindow::update_traffic_graph(int proxyDl, int proxyUp, int directDl, int directUp)
{
    if (speedChartWidget) {
        QMap<SpeedWidget::GraphType, long> pointData;
        pointData[SpeedWidget::OUTBOUND_PROXY_UP] = proxyUp;
        pointData[SpeedWidget::OUTBOUND_PROXY_DOWN] = proxyDl;
        pointData[SpeedWidget::OUTBOUND_DIRECT_UP] = directUp;
        pointData[SpeedWidget::OUTBOUND_DIRECT_DOWN] = directDl;

        speedChartWidget->AddPointData(pointData);
    }
}

// table显示

// refresh_groups -> show_group -> refresh_proxy_list
void MainWindow::refresh_groups() {
    Configs::dataStore->refreshing_group_list = true;

    // refresh group?
    for (int i = ui->tabWidget->count() - 1; i > 0; i--) {
        ui->tabWidget->removeTab(i);
    }

    int index = 0;
    for (const auto &gid: Configs::profileManager->groupsTabOrder) {
        auto group = Configs::profileManager->GetGroup(gid);
        if (index == 0) {
            ui->tabWidget->setTabText(0, group->name);
        } else {
            auto widget2 = new QWidget();
            auto layout2 = new QVBoxLayout();
            layout2->setContentsMargins(QMargins());
            layout2->setSpacing(0);
            widget2->setLayout(layout2);
            ui->tabWidget->addTab(widget2, group->name);
        }
        ui->tabWidget->tabBar()->setTabData(index, gid);
        index++;
    }

    // show after group changed
    if (Configs::profileManager->CurrentGroup() == nullptr) {
        Configs::dataStore->current_group = -1;
        ui->tabWidget->setCurrentIndex(groupId2TabIndex(0));
        show_group(Configs::profileManager->groupsTabOrder.count() > 0 ? Configs::profileManager->groupsTabOrder.first() : 0);
    } else {
        ui->tabWidget->setCurrentIndex(groupId2TabIndex(Configs::dataStore->current_group));
        show_group(Configs::dataStore->current_group);
    }

    Configs::dataStore->refreshing_group_list = false;
}

void MainWindow::refresh_proxy_list(const int &id) {
    refresh_proxy_list_impl(id, {});
}

void MainWindow::refresh_proxy_list_impl(const int &id, GroupSortAction groupSortAction) {
    ui->proxyListTable->setUpdatesEnabled(false);
    if (id < 0) {
        auto currentGroup = Configs::profileManager->CurrentGroup();
        if (currentGroup == nullptr)
        {
            MW_show_log("Could not find current group!");
            return;
        }
        switch (groupSortAction.method) {
            case GroupSortMethod::Raw: {
                break;
            }
            case GroupSortMethod::ById: {
                break;
            }
            case GroupSortMethod::ByAddress:
            case GroupSortMethod::ByName:
            case GroupSortMethod::ByLatency:
            case GroupSortMethod::ByType: {
                std::sort(currentGroup->profiles.begin(), currentGroup->profiles.end(),
                          [=](int a, int b) {
                              QString ms_a;
                              QString ms_b;
                              if (groupSortAction.method == GroupSortMethod::ByType) {
                                  ms_a = Configs::profileManager->GetProfile(a)->bean->DisplayType();
                                  ms_b = Configs::profileManager->GetProfile(b)->bean->DisplayType();
                              } else if (groupSortAction.method == GroupSortMethod::ByName) {
                                  ms_a = Configs::profileManager->GetProfile(a)->bean->name;
                                  ms_b = Configs::profileManager->GetProfile(b)->bean->name;
                              } else if (groupSortAction.method == GroupSortMethod::ByAddress) {
                                  ms_a = Configs::profileManager->GetProfile(a)->bean->DisplayAddress();
                                  ms_b = Configs::profileManager->GetProfile(b)->bean->DisplayAddress();
                              } else if (groupSortAction.method == GroupSortMethod::ByLatency) {
                                  ms_a = Configs::profileManager->GetProfile(a)->full_test_report;
                                  ms_b = Configs::profileManager->GetProfile(b)->full_test_report;
                              }
                              auto get_latency_for_sort = [](int id) {
                                  auto i = Configs::profileManager->GetProfile(id)->latency;
                                  if (i == 0) i = 100000;
                                  if (i < 0) i = 99999;
                                  return i;
                              };
                              if (groupSortAction.descending) {
                                  if (groupSortAction.method == GroupSortMethod::ByLatency) {
                                      if (ms_a.isEmpty() && ms_b.isEmpty()) {
                                          // compare latency if full_test_report is empty
                                          return get_latency_for_sort(a) > get_latency_for_sort(b);
                                      }
                                  }
                                  return ms_a > ms_b;
                              } else {
                                  if (groupSortAction.method == GroupSortMethod::ByLatency) {
                                      auto int_a = Configs::profileManager->GetProfile(a)->latency;
                                      auto int_b = Configs::profileManager->GetProfile(b)->latency;
                                      if (ms_a.isEmpty() && ms_b.isEmpty()) {
                                          // compare latency if full_test_report is empty
                                          return get_latency_for_sort(a) < get_latency_for_sort(b);
                                      }
                                  }
                                  return ms_a < ms_b;
                              }
                          });
                break;
            }
        }

        ui->proxyListTable->setRowCount(currentGroup->profiles.count());
    }

    // refresh data
    refresh_proxy_list_impl_refresh_data(id);
}

void MainWindow::refresh_proxy_list_impl_refresh_data(const int &id, bool stopping) {
    ui->proxyListTable->setUpdatesEnabled(false);
    auto currentGroup = Configs::profileManager->CurrentGroup();
    if (id >= 0)
    {
        if (!currentGroup->HasProfile(id))
        {
            ui->proxyListTable->setUpdatesEnabled(true);
            return;
        }
        auto rowID = currentGroup->profiles.indexOf(id);
        auto profile = Configs::profileManager->GetProfile(id);
        refresh_table_item(rowID, profile, stopping);
    } else
    {
        ui->proxyListTable->blockSignals(true);
        int row = 0;
        for (const auto profileId : currentGroup->profiles) {
            auto profile = Configs::profileManager->GetProfile(profileId);
            refresh_table_item(row++, profile, stopping);
        }
        ui->proxyListTable->blockSignals(false);
    }
    ui->proxyListTable->setUpdatesEnabled(true);
}

void MainWindow::refresh_table_item(const int row, const std::shared_ptr<Configs::ProxyEntity>& profile, bool stopping)
{
    if (profile == nullptr) return;

    auto isRunning = profile->id == Configs::dataStore->started_id && !stopping;
    auto f0 = std::make_unique<QTableWidgetItem>();
    f0->setData(114514, profile->id);

    // Check state
    auto check = f0->clone();
    check->setText(isRunning ? "✓" : Int2String(row + 1) + "  ");
    ui->proxyListTable->setVerticalHeaderItem(row, check);

    // C0: Type
    auto f = f0->clone();
    f->setText(profile->bean->DisplayType());
    if (isRunning) f->setForeground(palette().link());
    ui->proxyListTable->setItem(row, 0, f);

    // C1: Address+Port
    f = f0->clone();
    f->setText(profile->bean->DisplayAddress());
    if (isRunning) f->setForeground(palette().link());
    ui->proxyListTable->setItem(row, 1, f);

    // C2: Name
    f = f0->clone();
    f->setText(profile->bean->name);
    if (isRunning) f->setForeground(palette().link());
    ui->proxyListTable->setItem(row, 2, f);

    // C3: Test Result
    f = f0->clone();
    if (profile->full_test_report.isEmpty()) {
        auto color = profile->DisplayLatencyColor();
        if (color.isValid()) f->setForeground(color);
        f->setText(profile->DisplayTestResult());
    } else {
        f->setText(profile->full_test_report);
    }
    ui->proxyListTable->setItem(row, 3, f);

    // C4: Traffic
    f = f0->clone();
    f->setText(profile->traffic_data->DisplayTraffic());
    ui->proxyListTable->setItem(row, 4, f);
}

// table菜单相关

void MainWindow::on_proxyListTable_itemDoubleClicked(QTableWidgetItem *item) {
    auto id = item->data(114514).toInt();
    if (select_mode) {
        emit profile_selected(id);
        select_mode = false;
        refresh_status();
        return;
    }
    auto dialog = new DialogEditProfile("", id, this);
    connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
}

void MainWindow::on_menu_add_from_input_triggered() {
    auto dialog = new DialogEditProfile("socks", Configs::dataStore->current_group, this);
    connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
}

void MainWindow::on_menu_add_from_clipboard_triggered() {
    auto clipboard = QApplication::clipboard()->text();
    Subscription::groupUpdater->AsyncUpdate(clipboard);
}

void MainWindow::on_menu_clone_triggered() {
    auto ents = get_now_selected_list();
    if (ents.isEmpty()) return;

    auto btn = QMessageBox::question(this, tr("Clone"), tr("Clone %1 item(s)").arg(ents.count()));
    if (btn != QMessageBox::Yes) return;

    QStringList sls;
    for (const auto &ent: ents) {
        sls << ent->bean->ToNekorayShareLink(ent->type);
    }

    Subscription::groupUpdater->AsyncUpdate(sls.join("\n"));
}

void  MainWindow::on_menu_delete_repeat_triggered () {
    QList<std::shared_ptr<Configs::ProxyEntity>> out;
    QList<std::shared_ptr<Configs::ProxyEntity>> out_del;

    Configs::ProfileFilter::Uniq (Configs::profileManager-> CurrentGroup ()-> GetProfileEnts (), out,  true ,  false );
    Configs::ProfileFilter::OnlyInSrc_ByPointer (Configs::profileManager-> CurrentGroup ()-> GetProfileEnts (), out, out_del);

    int  remove_display_count =  0 ;
    QString remove_display;
    for  ( const  auto  &ent: out_del) {
        remove_display += ent-> bean -> DisplayTypeAndName () +  " \n " ;
        if  (++remove_display_count ==  20 ) {
            remove_display +=  " ... " ;
            break ;
        }
    }

    if  (!out_del.empty()  &&
        QMessageBox::question ( this ,  tr ( " Confirmation " ),  tr ( " Remove %1 item(s) ? " ). arg (out_del. length ()) +  " \n "  + remove_display) == QMessageBox::StandardButton::Yes) {
        for  ( const  auto  &ent: out_del) {
            Configs::profileManager->DeleteProfile(ent-> id );
        }
        refresh_proxy_list();
    }
}

void MainWindow::on_menu_delete_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() == 0) return;
    if (QMessageBox::question(this, tr("Confirmation"), QString(tr("Remove %1 item(s) ?")).arg(ents.count())) ==
        QMessageBox::StandardButton::Yes) {
        for (const auto &ent: ents) {
            Configs::profileManager->DeleteProfile(ent->id);
        }
        refresh_proxy_list();
    }
}

void MainWindow::on_menu_reset_traffic_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() == 0) return;
    for (const auto &ent: ents) {
        ent->traffic_data->Reset();
        ent->Save();
        refresh_proxy_list(ent->id);
    }
}

void MainWindow::on_menu_copy_links_triggered() {
    if (ui->masterLogBrowser->hasFocus()) {
        ui->masterLogBrowser->copy();
        return;
    }
    auto ents = get_now_selected_list();
    QStringList links;
    for (const auto &ent: ents) {
        links += ent->bean->ToShareLink();
    }
    if (links.length() == 0) return;
    QApplication::clipboard()->setText(links.join("\n"));
    show_log_impl(tr("Copied %1 item(s)").arg(links.length()));
}

void MainWindow::on_menu_copy_links_nkr_triggered() {
    auto ents = get_now_selected_list();
    QStringList links;
    for (const auto &ent: ents) {
        links += ent->bean->ToNekorayShareLink(ent->type);
    }
    if (links.length() == 0) return;
    QApplication::clipboard()->setText(links.join("\n"));
    show_log_impl(tr("Copied %1 item(s)").arg(links.length()));
}

void MainWindow::on_menu_export_config_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() != 1) return;
    auto ent = ents.first();
    if (ent->bean->DisplayCoreType() != software_core_name) return;

    auto result = BuildConfig(ent, false, true);
    QString config_core = QJsonObject2QString(result->coreConfig, true);
    QApplication::clipboard()->setText(config_core);

    QMessageBox msg(QMessageBox::Information, tr("Config copied"), config_core);
    QPushButton *button_1 = msg.addButton(tr("Copy core config"), QMessageBox::YesRole);
    QPushButton *button_2 = msg.addButton(tr("Copy test config"), QMessageBox::YesRole);
    msg.addButton(QMessageBox::Ok);
    msg.setEscapeButton(QMessageBox::Ok);
    msg.setDefaultButton(QMessageBox::Ok);
    msg.exec();
    if (msg.clickedButton() == button_1) {
        result = BuildConfig(ent, false, false);
        config_core = QJsonObject2QString(result->coreConfig, true);
        QApplication::clipboard()->setText(config_core);
    } else if (msg.clickedButton() == button_2) {
        result = BuildConfig(ent, true, false);
        config_core = QJsonObject2QString(result->coreConfig, true);
        QApplication::clipboard()->setText(config_core);
    }
}

void MainWindow::display_qr_link(bool nkrFormat) {
    auto ents = get_now_selected_list();
    if (ents.count() != 1) return;

    class W : public QDialog {
    public:
        QLabel *l = nullptr;
        QCheckBox *cb = nullptr;
        //
        QPlainTextEdit *l2 = nullptr;
        QImage im;
        //
        QString link;
        QString link_nk;

        void show_qr(const QSize &size) const {
            auto side = size.height() - 20 - l2->size().height() - cb->size().height();
            l->setPixmap(QPixmap::fromImage(im.scaled(side, side, Qt::KeepAspectRatio, Qt::FastTransformation),
                                            Qt::MonoOnly));
            l->resize(side, side);
        }

        void refresh(bool is_nk) {
            auto link_display = is_nk ? link_nk : link;
            l2->setPlainText(link_display);
            constexpr qint32 qr_padding = 2;
            //
            try {
                qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(link_display.toUtf8().data(), qrcodegen::QrCode::Ecc::MEDIUM);
                qint32 sz = qr.getSize();
                im = QImage(sz + qr_padding * 2, sz + qr_padding * 2, QImage::Format_RGB32);
                QRgb black = qRgb(0, 0, 0);
                QRgb white = qRgb(255, 255, 255);
                im.fill(white);
                for (int y = 0; y < sz; y++)
                    for (int x = 0; x < sz; x++)
                        if (qr.getModule(x, y))
                            im.setPixel(x + qr_padding, y + qr_padding, black);
                show_qr(size());
            } catch (const std::exception &ex) {
                QMessageBox::warning(nullptr, "error", ex.what());
            }
        }

        W(const QString &link_, const QString &link_nk_) {
            link = link_;
            link_nk = link_nk_;
            //
            setLayout(new QVBoxLayout);
            setMinimumSize(256, 256);
            QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            sizePolicy.setHeightForWidth(true);
            setSizePolicy(sizePolicy);
            //
            l = new QLabel();
            l->setMinimumSize(256, 256);
            l->setMargin(6);
            l->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            l->setScaledContents(true);
            layout()->addWidget(l);
            cb = new QCheckBox;
            cb->setText("Neko Links");
            layout()->addWidget(cb);
            l2 = new QPlainTextEdit();
            l2->setReadOnly(true);
            layout()->addWidget(l2);
            //
            connect(cb, &QCheckBox::toggled, this, &W::refresh);
            refresh(false);
        }

        void resizeEvent(QResizeEvent *resizeEvent) override {
            show_qr(resizeEvent->size());
        }
    };

    auto link = ents.first()->bean->ToShareLink();
    auto link_nk = ents.first()->bean->ToNekorayShareLink(ents.first()->type);
    auto w = new W(link, link_nk);
    w->setWindowTitle(ents.first()->bean->DisplayTypeAndName());
    w->exec();
    w->deleteLater();
}

#ifdef Q_OS_LINUX
OrgFreedesktopPortalRequestInterface::OrgFreedesktopPortalRequestInterface(
  const QString& service,
  const QString& path,
  const QDBusConnection& connection,
  QObject* parent)
  : QDBusAbstractInterface(service,
                           path,
                           "org.freedesktop.portal.Request",
                           connection,
                           parent)
{}

OrgFreedesktopPortalRequestInterface::~OrgFreedesktopPortalRequestInterface() {}
#endif

QPixmap grabScreen(QScreen* screen, bool& ok)
{
    QPixmap p;
    QRect geom = screen->geometry();
#ifdef Q_OS_LINUX
    DesktopInfo m_info;
    if (m_info.waylandDetected()) {
        QDBusInterface screenshotInterface(
          QStringLiteral("org.freedesktop.portal.Desktop"),
          QStringLiteral("/org/freedesktop/portal/desktop"),
          QStringLiteral("org.freedesktop.portal.Screenshot"));

        // unique token
        QString token =
          QUuid::createUuid().toString().remove('-').remove('{').remove('}');

        // premake interface
        auto* request = new OrgFreedesktopPortalRequestInterface(
          QStringLiteral("org.freedesktop.portal.Desktop"),
          "/org/freedesktop/portal/desktop/request/" +
            QDBusConnection::sessionBus().baseService().remove(':').replace('.','_') +
            "/" + token,
          QDBusConnection::sessionBus());

        QEventLoop loop;
        const auto gotSignal = [&p, &loop](uint status, const QVariantMap& map) {
            if (status == 0) {
                // Parse this as URI to handle unicode properly
                QUrl uri = map.value("uri").toString();
                QString uriString = uri.toLocalFile();
                p = QPixmap(uriString);
                p.setDevicePixelRatio(qApp->devicePixelRatio());
                QFile imgFile(uriString);
                imgFile.remove();
            }
            loop.quit();
        };

        // prevent racy situations and listen before calling screenshot
        QMetaObject::Connection conn = QObject::connect(
          request, &org::freedesktop::portal::Request::Response, gotSignal);

        screenshotInterface.call(
          QStringLiteral("Screenshot"),
          "",
          QMap<QString, QVariant>({ { "handle_token", QVariant(token) },
                                    { "interactive", QVariant(false) } }));

        loop.exec();
        QObject::disconnect(conn);
        request->Close().waitForFinished();
        request->deleteLater();

        if (p.isNull()) {
            ok = false;
        }
	return p;
    } else
#endif
        return screen->grabWindow(0, geom.x(), geom.y(), geom.width(), geom.height());
}

void MainWindow::on_menu_scan_qr_triggered() {
    hide();
    QThread::sleep(1);

    bool ok = true;
    QPixmap qpx(grabScreen(QGuiApplication::primaryScreen(), ok));

    show();
    if (ok) {
        const QVector<QString> texts = QrDecoder().decode(qpx.toImage().convertToFormat(QImage::Format_Grayscale8));
        if (texts.isEmpty()) {
            MessageBoxInfo(software_name, tr("QR Code not found"));
        } else {
            for (const QString &text : texts) {
                show_log_impl("QR Code Result:\n" + text);
                Subscription::groupUpdater->AsyncUpdate(text);
            }
        }
    }
    else {
        MessageBoxInfo(software_name, tr("Unable to capture screen"));
    }
}

void MainWindow::on_menu_clear_test_result_triggered() {
    for (const auto &profile: get_selected_or_group()) {
        profile->latency = 0;
        profile->dl_speed.clear();
        profile->ul_speed.clear();
        profile->full_test_report = "";
        profile->Save();
    }
    refresh_proxy_list();
}

void MainWindow::on_menu_select_all_triggered() {
    if (ui->masterLogBrowser->hasFocus()) {
        ui->masterLogBrowser->selectAll();
        return;
    }
    ui->proxyListTable->selectAll();
}

bool mw_sub_updating = false;

void MainWindow::on_menu_update_subscription_triggered() {
    auto group = Configs::profileManager->CurrentGroup();
    if (group->url.isEmpty()) return;
    if (mw_sub_updating) return;
    mw_sub_updating = true;
    Subscription::groupUpdater->AsyncUpdate(group->url, group->id, [&] { mw_sub_updating = false; });
}

void MainWindow::on_menu_remove_unavailable_triggered() {
    QList<std::shared_ptr<Configs::ProxyEntity>> out_del;

    for (const auto &[_, profile]: Configs::profileManager->profiles) {
        if (Configs::dataStore->current_group != profile->gid) continue;
        if (profile->latency < 0) out_del += profile;
    }

    int remove_display_count = 0;
    QString remove_display;
    for (const auto &ent: out_del) {
        remove_display += ent->bean->DisplayTypeAndName() + "\n";
        if (++remove_display_count == 20) {
            remove_display += "...";
            break;
        }
    }

    if (!out_del.empty() &&
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1 Unavailable item(s) ?").arg(out_del.length()) + "\n" + remove_display) == QMessageBox::StandardButton::Yes) {
        for (const auto &ent: out_del) {
            Configs::profileManager->DeleteProfile(ent->id);
        }
        refresh_proxy_list();
    }
}

void MainWindow::on_menu_remove_invalid_triggered() {
    QList<std::shared_ptr<Configs::ProxyEntity>> out_del;

    auto currentGroup = Configs::profileManager->GetGroup(Configs::dataStore->current_group);
    if (currentGroup == nullptr) return;
    for (const auto &profile : currentGroup->GetProfileEnts()) {
        if (!IsValid(profile)) out_del += profile;
    }

    int remove_display_count = 0;
    QString remove_display;
    for (const auto &ent: out_del) {
        remove_display += ent->bean->DisplayTypeAndName() + "\n";
        if (++remove_display_count == 20) {
            remove_display += "...";
            break;
        }
    }

    if (!out_del.empty() &&
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1 Invalid item(s) ?").arg(out_del.length()) + "\n" + remove_display) == QMessageBox::StandardButton::Yes) {
        for (const auto &ent: out_del) {
            Configs::profileManager->DeleteProfile(ent->id);
        }
        refresh_proxy_list();
    }
}

void MainWindow::on_menu_resolve_selected_triggered() {
    auto profiles = get_now_selected_list();
    if (profiles.isEmpty()) return;

    if (mw_sub_updating) return;
    mw_sub_updating = true;
    auto resolve_count = std::atomic<int>(0);
    Configs::dataStore->resolve_count = profiles.count();

    for (const auto &profile: profiles) {
        profile->bean->ResolveDomainToIP([=] {
            profile->Save();
            if (--Configs::dataStore->resolve_count != 0) return;
            refresh_proxy_list();
            mw_sub_updating = false;
        });
    }
}

void MainWindow::on_menu_resolve_domain_triggered() {
    auto currGroup = Configs::profileManager->GetGroup(Configs::dataStore->current_group);
    if (currGroup == nullptr) return;

    auto profiles = currGroup->Profiles();
    if (profiles.isEmpty()) return;

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              tr("Replace domain server addresses with their resolved IPs?")) != QMessageBox::StandardButton::Yes) {
        return;
    }
    if (mw_sub_updating) return;
    mw_sub_updating = true;
    auto resolve_count = std::atomic<int>(0);
    Configs::dataStore->resolve_count = profiles.count();

    for (const auto id: profiles) {
        auto profile = Configs::profileManager->GetProfile(id);
        profile->bean->ResolveDomainToIP([=] {
            profile->Save();
            if (--Configs::dataStore->resolve_count != 0) return;
            refresh_proxy_list();
            mw_sub_updating = false;
        });
    }
}

void MainWindow::on_proxyListTable_customContextMenuRequested(const QPoint &pos) {
    ui->menu_server->popup(ui->proxyListTable->viewport()->mapToGlobal(pos)); // 弹出菜单
}

QList<std::shared_ptr<Configs::ProxyEntity>> MainWindow::get_now_selected_list() {
    auto items = ui->proxyListTable->selectedItems();
    QList<std::shared_ptr<Configs::ProxyEntity>> list;
    for (auto item: items) {
        auto id = item->data(114514).toInt();
        auto ent = Configs::profileManager->GetProfile(id);
        if (ent != nullptr && !list.contains(ent)) list += ent;
    }
    return list;
}

QList<std::shared_ptr<Configs::ProxyEntity>> MainWindow::get_selected_or_group() {
    auto selected_or_group = ui->menu_server->property("selected_or_group").toInt();
    QList<std::shared_ptr<Configs::ProxyEntity>> profiles;
    if (selected_or_group > 0) {
        profiles = get_now_selected_list();
        if (profiles.isEmpty() && selected_or_group == 2) profiles = Configs::profileManager->CurrentGroup()->GetProfileEnts();
    } else {
        profiles = Configs::profileManager->CurrentGroup()->GetProfileEnts();
    }
    return profiles;
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
        case Qt::Key_Escape:
            // take over by shortcut_esc
            break;
        case Qt::Key_Enter:
            profile_start();
            break;
        default:
            QMainWindow::keyPressEvent(event);
    }
}

// Log

inline void FastAppendTextDocument(const QString &message, QTextDocument *doc) {
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::End);
    cursor.beginEditBlock();
    cursor.insertBlock();
    cursor.insertText(message);
    cursor.endEditBlock();
}

void MainWindow::show_log_impl(const QString &log) {
    if (log.size() > 20000)
    {
        show_log_impl("Ignored massive log of size:" + Int2String(log.size()));
        return;
    }
    auto trimmed = log.trimmed();
    if (trimmed.isEmpty()) return;

    FastAppendTextDocument(trimmed, qvLogDocument);
    // qvLogDocument->setPlainText(qvLogDocument->toPlainText() + log);
    // From https://gist.github.com/jemyzhang/7130092
    auto block = qvLogDocument->begin();

    while (block.isValid()) {
        if (qvLogDocument->blockCount() > Configs::dataStore->max_log_line) {
            QTextCursor cursor(block);
            block = block.next();
            cursor.select(QTextCursor::BlockUnderCursor);
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            continue;
        }
        break;
    }
}

void MainWindow::on_masterLogBrowser_customContextMenuRequested(const QPoint &pos) {
    QMenu *menu = ui->masterLogBrowser->createStandardContextMenu();

    auto sep = new QAction(this);
    sep->setSeparator(true);
    menu->addAction(sep);

    auto action_clear = new QAction(this);
    action_clear->setText(tr("Clear"));
    connect(action_clear, &QAction::triggered, this, [=] {
        qvLogDocument->clear();
        ui->masterLogBrowser->clear();
    });
    menu->addAction(action_clear);

    menu->exec(ui->masterLogBrowser->viewport()->mapToGlobal(pos)); // 弹出菜单
}

void MainWindow::on_tabWidget_customContextMenuRequested(const QPoint &p) {
    int clickedIndex = ui->tabWidget->tabBar()->tabAt(p);
    if (clickedIndex == -1) {
        auto* menu = new QMenu(this);
        auto* addAction = new QAction(tr("Add new Group"), this);
        connect(addAction, &QAction::triggered, this, [=]{
            auto ent = Configs::ProfileManager::NewGroup();
            auto dialog = new DialogEditGroup(ent, this);
            int ret = dialog->exec();
            dialog->deleteLater();

            if (ret == QDialog::Accepted) {
                Configs::profileManager->AddGroup(ent);
                MW_dialog_message(Dialog_DialogManageGroups, "refresh-1");
            }
        });

        menu->addAction(addAction);
        menu->exec(ui->tabWidget->tabBar()->mapToGlobal(p));
        return;
    }

    ui->tabWidget->setCurrentIndex(clickedIndex);
    auto* menu = new QMenu(this);

    auto* addAction = new QAction(tr("Add new Group"), this);
    auto* deleteAction = new QAction(tr("Delete selected Group"), this);
    auto* editAction = new QAction(tr("Edit selected Group"), this);
    connect(addAction, &QAction::triggered, this, [=]{
        auto ent = Configs::ProfileManager::NewGroup();
        auto dialog = new DialogEditGroup(ent, this);
        int ret = dialog->exec();
        dialog->deleteLater();

        if (ret == QDialog::Accepted) {
            Configs::profileManager->AddGroup(ent);
            MW_dialog_message(Dialog_DialogManageGroups, "refresh-1");
        }
    });
    connect(deleteAction, &QAction::triggered, this, [=] {
        auto id = Configs::profileManager->groupsTabOrder[clickedIndex];
        if (QMessageBox::question(this, tr("Confirmation"), tr("Remove %1?").arg(Configs::profileManager->groups[id]->name)) ==
            QMessageBox::StandardButton::Yes) {
            Configs::profileManager->DeleteGroup(id);
            MW_dialog_message(Dialog_DialogManageGroups, "refresh-1");
        }
    });
    connect(editAction, &QAction::triggered, this, [=]{
        auto id = Configs::profileManager->groupsTabOrder[clickedIndex];
        auto ent = Configs::profileManager->groups[id];
        auto dialog = new DialogEditGroup(ent, this);
        connect(dialog, &QDialog::finished, this, [=] {
            if (dialog->result() == QDialog::Accepted) {
                ent->Save();
                MW_dialog_message(Dialog_DialogManageGroups, "refresh" + Int2String(ent->id));
            }
            dialog->deleteLater();
        });
        dialog->show();
    });
    menu->addAction(addAction);
    menu->addAction(editAction);
    auto group = Configs::profileManager->GetGroup(Configs::dataStore->current_group);
    if (Configs::profileManager->groups.size() > 1) menu->addAction(deleteAction);
    if (!group->Profiles().empty()) {
        menu->addAction(ui->actionUrl_Test_Group);
        menu->addAction(ui->actionSpeedtest_Group);
        menu->addAction(ui->menu_resolve_domain);
        menu->addAction(ui->menu_clear_test_result);
        menu->addAction(ui->menu_delete_repeat);
        menu->addAction(ui->menu_remove_unavailable);
        menu->addAction(ui->menu_remove_invalid);
    }
    if (!group->url.isEmpty()) menu->addAction(ui->menu_update_subscription);
    if (!speedtestRunning.tryLock()) {
        menu->addAction(ui->menu_stop_testing);
    } else {
        speedtestRunning.unlock();
        menu->removeAction(ui->menu_stop_testing);
    }
    menu->exec(ui->tabWidget->tabBar()->mapToGlobal(p));
    return;
}

// eventFilter

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
        if (obj == ui->label_running && mouseEvent->button() == Qt::LeftButton && running != nullptr) {
            url_test_current();
            return true;
        } else if (obj == ui->label_inbound && mouseEvent->button() == Qt::LeftButton) {
            on_menu_basic_settings_triggered();
            return true;
        } else if (obj == ui->tabWidget && mouseEvent->button() == Qt::RightButton) {
            on_tabWidget_customContextMenuRequested(mouseEvent->position().toPoint());
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonDblClick) {
        if (obj == ui->splitter) {
            auto size = ui->splitter->size();
            ui->splitter->setSizes({size.height() / 2, size.height() / 2});
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// profile selector

void MainWindow::start_select_mode(QObject *context, const std::function<void(int)> &callback) {
    select_mode = true;
    connectOnce(this, &MainWindow::profile_selected, context, callback);
    refresh_status();
}

// 连接列表

inline QJsonArray last_arr; // format is nekoray_connections_json

// Hotkey

inline QList<std::shared_ptr<QHotkey>> RegisteredHotkey;

void MainWindow::RegisterHotkey(bool unregister) {
    while (!RegisteredHotkey.isEmpty()) {
        auto hk = RegisteredHotkey.takeFirst();
        hk->deleteLater();
    }
    if (unregister) return;

    QStringList regstr{
        Configs::dataStore->hotkey_mainwindow,
        Configs::dataStore->hotkey_group,
        Configs::dataStore->hotkey_route,
        Configs::dataStore->hotkey_system_proxy_menu,
        Configs::dataStore->hotkey_toggle_system_proxy,
    };

    for (const auto &key: regstr) {
        if (key.isEmpty()) continue;
        if (regstr.count(key) > 1) return; // Conflict hotkey
    }
    for (const auto &key: regstr) {
        QKeySequence k(key);
        if (k.isEmpty()) continue;
        auto hk = std::make_shared<QHotkey>(k, true);
        if (hk->isRegistered()) {
            RegisteredHotkey += hk;
            connect(hk.get(), &QHotkey::activated, this, [=] { HotkeyEvent(key); });
        } else {
            hk->deleteLater();
        }
    }
}

void MainWindow::RegisterShortcuts() {
    for (const auto &action: ui->menuHidden_menu->actions()) {
        new QShortcut(action->shortcut(), this, [=](){
            action->trigger();
        });
    }
}

void MainWindow::HotkeyEvent(const QString &key) {
    if (key.isEmpty()) return;
    runOnUiThread([=] {
        if (key == Configs::dataStore->hotkey_mainwindow) {
            tray->activated(QSystemTrayIcon::ActivationReason::Trigger);
        } else if (key == Configs::dataStore->hotkey_group) {
            on_menu_manage_groups_triggered();
        } else if (key == Configs::dataStore->hotkey_route) {
            on_menu_routing_settings_triggered();
        } else if (key == Configs::dataStore->hotkey_system_proxy_menu) {
            ui->menu_spmode->popup(QCursor::pos());
        } else if (key == Configs::dataStore->hotkey_toggle_system_proxy) {
            toggle_system_proxy();
        }
    });
}

bool MainWindow::StopVPNProcess() {
    vpn_pid = core_process->processId();
    if (vpn_pid != 0) {
        bool ok;
#ifdef Q_OS_WIN
        auto ret = WinCommander::runProcessElevated("taskkill", {"/F", "/PID", Int2String(vpn_pid)});
        ok = ret == 0;
#endif

#ifdef Q_OS_MACOS
        auto ret = system(QString("osascript -e 'do shell script \"kill -9 %1\"' with administrator privileges").arg(vpn_pid).toStdString().c_str());;
        ok = ret == 0;
#endif
#ifdef Q_OS_LINUX
        QProcess p;
        p.start("pkexec", {"kill", "-9", Int2String(vpn_pid)});
        p.waitForFinished();
        ok = p.exitCode() == 0;
#endif
        ok ? vpn_pid = 0 : MessageBoxWarning(tr("Error"), tr("Failed to stop Tun process"));
        return ok;
    }
    return true;
}

void MainWindow::DownloadAssets(const QString &geoipUrl, const QString &geositeUrl) {
    if (!mu_download_assets.tryLock()) {
        runOnUiThread([=](){
            MessageBoxWarning(tr("Cannot start"), tr("Last download request has not finished yet"));
        });
        return;
    }
    MW_show_log("Start downloading...");
    QString errors;
    if (!geoipUrl.isEmpty()) {
        auto resp = NetworkRequestHelper::DownloadAsset(geoipUrl, "geoip.db");
        if (!resp.isEmpty()) {
            MW_show_log(QString(tr("Failed to download geoip: %1")).arg(resp));
            errors += "geoip: " + resp;
        }
    }
    if (!geositeUrl.isEmpty()) {
        auto resp = NetworkRequestHelper::DownloadAsset(geositeUrl, "geosite.db");
        if (!resp.isEmpty()) {
            MW_show_log(QString(tr("Failed to download geosite: %1")).arg(resp));
            errors += "\ngeosite: " + resp;
        }
    }
    mu_download_assets.unlock();
    if (!errors.isEmpty()) {
        runOnUiThread([=](){
            MessageBoxWarning(tr("Failed to download geo assets"), errors);
        });
    }
    MW_show_log(tr("Geo Asset update completed!"));
}

// to parse versions of format Throne-1.2.3-beta.2 or Throne-1.2.3
bool isNewer(QString version) {
    if (QString(NKR_VERSION).isEmpty()) return false;
    version = version.mid(7); // take out Throne-
    auto parts = version.replace("-", ".").split('.');
    auto currentParts = QString(NKR_VERSION).replace("-", ".").split('.');
    if (parts.size() < 3 || currentParts.size() < 3)
    {
        MW_show_log("Version strings seem to be invalid" + QString(NKR_VERSION) + " and " + version);
        return false;
    }
    std::vector<int> verNums;
    std::vector<int> currNums;
    // add base version first
    verNums.push_back(parts[0].toInt());
    verNums.push_back(parts[1].toInt());
    verNums.push_back(parts[2].toInt());
    if (parts.size() > 3)
    {
        if (parts[3] == "alpha") verNums.push_back(1);
        if (parts[3] == "beta") verNums.push_back(2);
        if (parts[3] == "rc") verNums.push_back(3);
        if (parts.size() > 4) verNums.push_back(parts[4].toInt());
    }

    currNums.push_back(currentParts[0].toInt());
    currNums.push_back(currentParts[1].toInt());
    currNums.push_back(currentParts[2].toInt());
    if (currentParts.size() > 3)
    {
        if (currentParts[3] == "alpha") currNums.push_back(1);
        if (currentParts[3] == "beta") currNums.push_back(2);
        if (currentParts[3] == "rc") currNums.push_back(3);
        if (currentParts.size() > 4) currNums.push_back(currentParts[4].toInt());
    }

    if (verNums.size() < 3 || currNums.size() < 3)
    {
        MW_show_log("Version strings seem to be invalid" + QString(NKR_VERSION) + " and " + version);
        return false;
    }

    for (int i=0;i<3;i++)
    {
        if (verNums[i] > currNums[i]) return true;
        if (verNums[i] < currNums[i]) return false;
    }

    // equal base version, check beta-ness
    if (verNums.size() == 5 && currNums.size() == 3) return false;
    if (verNums.size() == 3 && currNums.size() == 5) return true;
    if (verNums.size() == 5 && currNums.size() == 5)
    {
        for (int i=3;i<5;i++)
        {
            if (verNums[i] > currNums[i]) return true;
            if (verNums[i] < currNums[i]) return false;
        }
    } else
    {
        MW_show_log("Version strings seem to be invalid" + QString(NKR_VERSION) + " and " + version);
        return false;
    }
    return false;
}

void MainWindow::CheckUpdate() {
    QString search;
#ifdef Q_OS_WIN32
#  ifdef Q_OS_WIN64
    if (IsWindows10OrGreater())
        search = "windows64";
    else
	search = "windowslegacy64";
#  else
	search = "windows32";
#  endif
#endif
#ifdef Q_OS_LINUX
#  ifdef Q_PROCESSOR_X86_64
        search = "linux-amd64";
#  else
        search = "linux-arm64";
#  endif
#endif
#ifdef Q_OS_MACOS
#  ifdef Q_PROCESSOR_X86_64
	search = "macos-amd64";
#  else
	search = "macos-arm64";
#  endif
#endif
    if (search.isEmpty()) {
        runOnUiThread([=] {
            MessageBoxWarning(QObject::tr("Update"), QObject::tr("Not official support platform"));
        });
        return;
    }

    auto resp = NetworkRequestHelper::HttpGet("https://api.github.com/repos/throneproj/Throne/releases");
    if (!resp.error.isEmpty()) {
        runOnUiThread([=] {
            MessageBoxWarning(QObject::tr("Update"), QObject::tr("Requesting update error: %1").arg(resp.error + "\n" + resp.data));
        });
        return;
    }

    QString assets_name, release_download_url, release_url, release_note, note_pre_release;
    bool exitFlag = false;
    QJsonArray array = QString2QJsonArray(resp.data);
    for (const QJsonValue value : array) {
        QJsonObject release = value.toObject();
        for (const QJsonValue asset : release["assets"].toArray()) {
            if (asset["name"].toString().contains(search) && asset["name"].toString().section('.', -1) == QString("zip")) {
                note_pre_release = release["prerelease"].toBool() ? " (Pre-release)" : "";
                release_url = release["html_url"].toString();
                release_note = release["body"].toString();
                assets_name = asset["name"].toString();
                release_download_url = asset["browser_download_url"].toString();
                exitFlag = true;
                break;
            }
        }
        if (exitFlag) break;
    }

    if (release_download_url.isEmpty() || !isNewer(assets_name)) {
        runOnUiThread([=] {
            MessageBoxInfo(QObject::tr("Update"), QObject::tr("No update"));
        });
        return;
    }

    runOnUiThread([=] {
        auto allow_updater = !Configs::dataStore->flag_use_appdata;
        QMessageBox box(QMessageBox::Question, QObject::tr("Update") + note_pre_release,
                        QObject::tr("Update found: %1\nRelease note:\n%2").arg(assets_name, release_note));
        //
        QAbstractButton *btn1 = nullptr;
        if (allow_updater) {
            btn1 = box.addButton(QObject::tr("Update"), QMessageBox::AcceptRole);
        }
        QAbstractButton *btn2 = box.addButton(QObject::tr("Open in browser"), QMessageBox::AcceptRole);
        box.addButton(QObject::tr("Close"), QMessageBox::RejectRole);
        box.exec();
        //
        if (btn1 == box.clickedButton() && allow_updater) {
            // Download Update
            runOnNewThread([=] {
                if (!mu_download_update.tryLock()) {
                    runOnUiThread([=](){
                        MessageBoxWarning(tr("Cannot start"), tr("Last download request has not finished yet"));
                    });
                    return;
                }
                QString errors;
                if (!release_download_url.isEmpty()) {
                    auto res = NetworkRequestHelper::DownloadAsset(release_download_url, "nekoray.zip");
                    if (!res.isEmpty()) {
                        errors += res;
                    }
                }
                mu_download_update.unlock();
                runOnUiThread([=] {
                    if (errors.isEmpty()) {
                        auto q = QMessageBox::question(nullptr, QObject::tr("Update"),
                                                       QObject::tr("Update is ready, restart to install?"));
                        if (q == QMessageBox::StandardButton::Yes) {
                            this->exit_reason = 1;
                            on_menu_exit_triggered();
                        }
                    } else {
                        MessageBoxWarning(tr("Failed to download update assets"), errors);
                    }
                });
            });
        } else if (btn2 == box.clickedButton()) {
            QDesktopServices::openUrl(QUrl(release_url));
        }
    });
}
