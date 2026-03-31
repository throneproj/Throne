#include "include/ui/mainwindow.h"

#include <QAbstractItemView>
#include <QMenu>
#include "include/global/Utils.hpp"
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
#include "include/configs/generate.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"


#include "include/database/RoutesRepo.h"

#include "include/ui/utils/ProfilesTableFilterHeader.h"

#include "include/ui/group/dialog_edit_group.h"

#ifdef Q_OS_WIN
#include "3rdparty/WinCommander.hpp"
#include "include/sys/windows/WinVersion.h"
#else
#ifdef Q_OS_LINUX
#include "include/sys/linux/LinuxCap.h"
#include <QDBusInterface>
#include <QDBusReply>
#include <QUuid>
#endif
#include <unistd.h>
#endif

#include <QClipboard>
#include <QModelIndex>
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
#include <QFileDialog>
#include <QToolTip>
#include <QMimeData>
#include <random>
#include <3rdparty/QHotkey/qhotkey.h>
#include <3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.hpp>
#include <include/global/HTTPRequestHelper.hpp>
#include "include/global/DeviceDetailsHelper.hpp"

#include "include/sys/macos/MacOS.h"

void UI_InitMainWindow() {
    mainwindow = new MainWindow;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    mainwindow = this;
    setAcceptDrops(true);
    MW_dialog_message = [=,this](const QString &a, const QString &b) {
        runOnUiThread([=,this]
        {
            dialog_message_impl(a, b);
        });
    };

    // handle AutoRun migration and privilege matching
    AutoRun_FixPrivilegeIfNeeded();
    AutoRun_MigrateIfNeeded();

    // Setup misc UI
    // migrate old themes
    bool isNum;
    Configs::dataManager->settingsRepo->theme.toInt(&isNum);
    if (isNum) {
        Configs::dataManager->settingsRepo->theme = "System";
    }
    themeManager->ApplyTheme(Configs::dataManager->settingsRepo->theme);
    ui->setupUi(this);

    // init shortcuts
    setActionsData();
    loadShortcuts();

    // geometry remembering
    if (!Configs::dataManager->settingsRepo->mainWindowGeometry.isEmpty()) {
        auto geo = DecodeB64IfValid(Configs::dataManager->settingsRepo->mainWindowGeometry);
        this->restoreGeometry(geo);
    }

    // setup log
    ui->splitter->restoreState(DecodeB64IfValid(Configs::dataManager->settingsRepo->splitter_state));
    new SyntaxHighlighter(Configs::dataManager->settingsRepo->theme.toLower().contains("vista") ? false : (Configs::dataManager->settingsRepo->theme.toLower().contains("qdarkstyle") ? true : isDarkMode()), qvLogDocument);
    qvLogDocument->setUndoRedoEnabled(false);
    qvLogDocument->setMaximumBlockCount(Configs::dataManager->settingsRepo->max_log_line);
    ui->masterLogBrowser->setUndoRedoEnabled(false);
    ui->masterLogBrowser->setDocument(qvLogDocument);
    ui->masterLogBrowser->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    updateLogFilterFields();
    runOnThread([=, this] {
        log_process_loop();
    }, LogThread);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [=,this](const Qt::ColorScheme& scheme) {
        new SyntaxHighlighter(scheme == Qt::ColorScheme::Dark, qvLogDocument);
        themeManager->ApplyTheme(Configs::dataManager->settingsRepo->theme, true);
    });
#endif
    connect(themeManager, &ThemeManager::themeChanged, this, [=,this](const QString& theme){
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
    connect(ui->masterLogBrowser->verticalScrollBar(), &QSlider::valueChanged, this, [=,this](int value) {
        if (ui->masterLogBrowser->verticalScrollBar()->maximum() == value)
            qvLogAutoScoll = true;
        else
            qvLogAutoScoll = false;
    });
    connect(ui->masterLogBrowser, &QTextBrowser::textChanged, this, [=,this]() {
        if (!qvLogAutoScoll)
            return;
        auto bar = ui->masterLogBrowser->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
    MW_show_log = [=,this](const QString &log) {
        append_log(log);
    };

    // Listen port if random
    if (Configs::dataManager->settingsRepo->random_inbound_port)
    {
        Configs::dataManager->settingsRepo->inbound_socks_port = MkPort();
    }

    //init HWID data
    runOnNewThread([=, this] {GetDeviceDetails(); });

    // Prepare core
    Configs::dataManager->settingsRepo->core_port = MkPort();
    if (Configs::dataManager->settingsRepo->core_port <= 0) Configs::dataManager->settingsRepo->core_port = 19810;

    auto core_path = QApplication::applicationDirPath() + "/";
    core_path += "ThroneCore";

    QStringList args;
    args.push_back("-port");
    args.push_back(Int2String(Configs::dataManager->settingsRepo->core_port));
    if (Configs::dataManager->settingsRepo->log_level == "debug") args.push_back("-debug");

    // Start core
    runOnThread(
        [=,this] {
            core_process = new Configs_sys::CoreProcess(core_path, args);
            // Remember last started
            if (Configs::dataManager->settingsRepo->remember_enable && Configs::dataManager->settingsRepo->remember_id >= 0) {
                core_process->start_profile_when_core_is_up = Configs::dataManager->settingsRepo->remember_id;
            }
            // Setup
            setup_rpc();
            core_process->Start();
        },
        DS_cores);

#ifdef Q_OS_LINUX
    for (int i=0;i<20;i++)
    {
        QThread::msleep(100);
        if (Configs::dataManager->settingsRepo->core_running) break;
    }
    if (!Configs::dataManager->settingsRepo->core_running) qDebug() << "[Warn] Core is taking too much time to start";
#endif

    if (!Configs::dataManager->settingsRepo->font.isEmpty()) {
        auto font = qApp->font();
        font.setFamily(Configs::dataManager->settingsRepo->font);
        qApp->setFont(font);
    }
    if (Configs::dataManager->settingsRepo->font_size != 0) {
        auto font = qApp->font();
        font.setPointSize(Configs::dataManager->settingsRepo->font_size);
        qApp->setFont(font);
    }

    parallelCoreCallPool->setMaxThreadCount(10); // constant value
    //
    connect(ui->menu_start, &QAction::triggered, this, [=,this]() { profile_start(); });
    connect(ui->menu_stop, &QAction::triggered, this, [=,this]() { profile_stop(false, false, true); });
    connect(ui->tabWidget->tabBar(), &QTabBar::tabMoved, this, [=,this](int from, int to) {
        // use tabData to track tab & gid
        QList<int> tabOrder;
        for (int i = 0; i < ui->tabWidget->tabBar()->count(); i++) {
            tabOrder += ui->tabWidget->tabBar()->tabData(i).toInt();
        }
        Configs::dataManager->groupsRepo->SetGroupsTabOrder(tabOrder);
        on_tabWidget_currentChanged(ui->tabWidget->tabBar()->currentIndex());
    });
    ui->label_running->installEventFilter(this);
    ui->label_inbound->installEventFilter(this);
    ui->splitter->installEventFilter(this);
    ui->tabWidget->installEventFilter(this);
    //
    auto btnFilter = new QToolButton(this);
    btnFilter->setIcon(QIcon(":/icon/filter.png"));
    btnFilter->setToolTip(QString("%1\n%2").arg(tr("Enable Filter"), QKeySequence(QKeySequence::Find).toString(QKeySequence::NativeText)));
    btnFilter->setShortcut(QKeySequence::Find);
    btnFilter->setCheckable(true);
    connect(btnFilter, &QToolButton::toggled, static_cast<ProfilesTableFilterHeader*>(ui->profilesTableView->horizontalHeader()), &ProfilesTableFilterHeader::setFiltersVisible);
    ui->tabWidget->setCornerWidget(btnFilter, Qt::TopRightCorner);
    //
    RegisterHotkey(false);
    //
    auto last_size = Configs::dataManager->settingsRepo->mw_size.split("x");
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
    if (auto dashDir = QDir("dashboard"); !dashDir.exists() && QDir().mkdir("dashboard")) {
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
    if (auto iconsDir = QDir("icons"); !iconsDir.exists()) {
        QDir().mkdir("icons") ? qDebug("created icons dir") : qDebug("Failed to create icons dir");
    }

    // top bar
    ui->toolButton_program->setMenu(ui->menu_program);
    ui->toolButton_preferences->setMenu(ui->menu_preferences);
    ui->toolButton_server->setMenu(ui->menu_server);
    ui->toolButton_routing->setMenu(ui->menuRouting_Menu);
    ui->menubar->setVisible(false);
    connect(ui->toolButton_update, &QToolButton::clicked, this, [=,this] { runOnNewThread([=,this] { CheckUpdate(); }); });
    if (!QFile::exists(QApplication::applicationDirPath() + "/updater") && !QFile::exists(QApplication::applicationDirPath() + "/updater.exe"))
    {
        ui->toolButton_update->hide();
    }

    // setup connection UI
    setupConnectionList();
    ui->stats_widget->tabBar()->setCurrentIndex(Configs::dataManager->settingsRepo->stats_tab);
    connect(ui->stats_widget->tabBar(), &QTabBar::currentChanged, this, [=,this](int index)
    {
        Configs::dataManager->settingsRepo->stats_tab = ui->stats_widget->tabBar()->currentIndex();
    });
    connect(ui->connections->horizontalHeader(), &QHeaderView::sectionClicked, this, [=,this](int index)
    {
            Stats::ConnectionSort sortType;

            switch (index)
            {
            case 1: sortType = Stats::ByProcess; break;
            case 2: sortType = Stats::ByProtocol; break;
            case 3: sortType = Stats::ByOutbound; break;
            case 4: sortType = Stats::ByTraffic; break;
            default: sortType = Stats::Default; break;
            }

            Stats::connection_lister->setSort(sortType);
            Stats::connection_lister->ForceUpdate();
    });

    // setup Speed Chart
    speedChartWidget = new SpeedWidget(this);
    ui->graph_tab->layout()->addWidget(speedChartWidget);

    // table UI: model-backed view with on-demand row data
    profilesTableModel = new ProfilesTableModel(this);
    ui->profilesTableView->setModel(profilesTableModel);
    ui->profilesTableView->rowsSwapped = [=,this](int row1, int row2)
    {
        if (!addressFilterString.isEmpty() || !nameFilterString.isEmpty() || !typeFilterString.isEmpty() || !countryFilterString.isEmpty()) return;
        if (row1 == row2) return;
        auto group = Configs::dataManager->groupsRepo->CurrentGroup();
        group->EmplaceProfile(row1, row2);
        profilesTableModel->emplaceProfiles(row1, row2);
        Configs::dataManager->groupsRepo->Save(group);
    };
    connect(ui->profilesTableView->horizontalHeader(), &QHeaderView::sectionClicked, this, [=, this](int logicalIndex) {
        GroupSortAction action;
        if (proxy_last_order == logicalIndex) {
            action.descending = true;
            proxy_last_order = -1;
        } else {
            proxy_last_order = logicalIndex;
        }
        if (logicalIndex == 0) {
            action.method = GroupSortMethod::ByType;
        } else if (logicalIndex == 1) {
            action.method = GroupSortMethod::ByAddress;
        } else if (logicalIndex == 2) {
            action.method = GroupSortMethod::ByName;
        } else if (logicalIndex == 3) {
            action.method = GroupSortMethod::ByTestResult;
        } else if (logicalIndex == 4) {
            action.method = GroupSortMethod::ByTraffic;
        } else {
            return;
        }
        runOnNewThread([=, this] {
            auto currGroup = Configs::dataManager->groupsRepo->CurrentGroup();
            if (currGroup == nullptr) return;
            if (!currGroup->SortProfiles(action)) {
                runOnUiThread([=] {
                    MessageBoxWarning("Action already in progress", "A sort action is already in progress");
                });
                return;
            }
            Configs::dataManager->groupsRepo->Save(Configs::dataManager->groupsRepo->CurrentGroup());
            runOnUiThread([=, this] {
                refresh_proxy_list({}, true);
            });
        });
    });
    connect(ui->profilesTableView->horizontalHeader(), &QHeaderView::sectionResized, this, [=, this](int, int, int) {
        auto group = Configs::dataManager->groupsRepo->CurrentGroup();
        if (Configs::dataManager->settingsRepo->refreshing_group || group == nullptr) return;
        group->column_width.clear();
        for (int i = 0; i < ui->profilesTableView->horizontalHeader()->count(); i++) {
            group->column_width.push_back(ui->profilesTableView->horizontalHeader()->sectionSize(i));
        }
        Configs::dataManager->groupsRepo->Save(Configs::dataManager->groupsRepo->CurrentGroup());
    });
    ui->profilesTableView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->profilesTableView->horizontalHeader(), &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* header = ui->profilesTableView->horizontalHeader();
        int columnIndex = header->logicalIndexAt(pos);
        auto group = Configs::dataManager->groupsRepo->CurrentGroup();
        if (group == nullptr) return;
        if (columnIndex == 3) {
            QMenu menu(this);
            auto* includeLabel = menu.addAction(tr("Include:"));
            includeLabel->setEnabled(false);

            auto* actionShowOutIP = menu.addAction(tr("Out IP"));
            actionShowOutIP->setCheckable(true);
            actionShowOutIP->setChecked(group->test_items_to_show == Configs::testShowItems::all ||
                group->test_items_to_show == Configs::testShowItems::ipOnly);

            auto* actionShowSpeed = menu.addAction(tr("Speed"));
            actionShowSpeed->setCheckable(true);
            actionShowSpeed->setChecked(group->test_items_to_show == Configs::testShowItems::all ||
                group->test_items_to_show == Configs::testShowItems::speedOnly);

            auto updateTestItemsToShow = [this, group, actionShowOutIP, actionShowSpeed] {
                    const bool ip = actionShowOutIP->isChecked();
                    const bool speed = actionShowSpeed->isChecked();
                    if (ip && speed) group->test_items_to_show = Configs::testShowItems::all;
                    else if (ip) group->test_items_to_show = Configs::testShowItems::ipOnly;
                    else if (speed) group->test_items_to_show = Configs::testShowItems::speedOnly;
                    else group->test_items_to_show = Configs::testShowItems::none;
                    Configs::dataManager->groupsRepo->Save(group);
                    if (group->calculated_column_width.size() > 3) {
                        group->calculated_column_width[3] = 0;
                    }
                    refresh_proxy_list();
                };

            connect(actionShowOutIP, &QAction::triggered, this, updateTestItemsToShow);
            connect(actionShowSpeed, &QAction::triggered, this, updateTestItemsToShow);

            menu.addSeparator();
            auto* sortByLabel = menu.addAction(tr("Sort By:"));
            sortByLabel->setEnabled(false);

            struct SortOption { int value; QString label; };
            QList<SortOption> options = {
                { static_cast<int>(Configs::testBy::latency), tr("Latency") },
                { static_cast<int>(Configs::testBy::dlSpeed), tr("Download Speed") },
                { static_cast<int>(Configs::testBy::ulSpeed), tr("Upload Speed") },
                { static_cast<int>(Configs::testBy::ipOut), tr("IP Out") }
            };
            for (const auto& opt : options) {
                auto* act = menu.addAction(opt.label);
                act->setData(opt.value);
                act->setCheckable(true);
                act->setChecked(static_cast<int>(group->test_sort_by) == opt.value);
            }

            auto* chosen = menu.exec(header->mapToGlobal(pos));
            if (chosen == nullptr || !chosen->data().isValid()) return;

            int testSortBy = chosen->data().toInt();
            group->test_sort_by = static_cast<Configs::testBy>(testSortBy);
            Configs::dataManager->groupsRepo->Save(group);
            GroupSortAction action;
            action.method = GroupSortMethod::ByTestResult;
            action.descending = false;
            runOnNewThread([=, this] {
                auto currGroup = Configs::dataManager->groupsRepo->CurrentGroup();
                if (currGroup == nullptr) return;
                if (!currGroup->SortProfiles(action)) {
                    runOnUiThread([=] {
                        MessageBoxWarning("Action already in progress", "A sort action is already in progress");
                        });
                    return;
                }
                Configs::dataManager->groupsRepo->Save(Configs::dataManager->groupsRepo->CurrentGroup());
                runOnUiThread([=, this] {
                    refresh_proxy_list();
                    });
                });
            return;
        }
        if (columnIndex == 4) {
            QMenu menu(this);
            auto* sortByLabel = menu.addAction(tr("Sort By:"));
            sortByLabel->setEnabled(false);

            struct TrafficSortOption { int value; QString label; };
            QList<TrafficSortOption> options = {
                { 0, tr("Total") },
                { 1, tr("Downloaded") },
                { 2, tr("Uploaded") }
            };

            for (const auto& opt : options) {
                auto* act = menu.addAction(opt.label);
                act->setData(opt.value);
                act->setCheckable(true);
                act->setChecked(static_cast<int>(group->traffic_sort_by) == opt.value);
            }

            auto* chosen = menu.exec(header->mapToGlobal(pos));
            if (chosen == nullptr || !chosen->data().isValid()) return;

            int trafficSortBy = chosen->data().toInt();
            group->traffic_sort_by = static_cast<Configs::trafficBy>(trafficSortBy);
            Configs::dataManager->groupsRepo->Save(group);
            GroupSortAction action;
            action.method = GroupSortMethod::ByTraffic;
            action.descending = false;
            runOnNewThread([=, this] {
                auto currGroup = Configs::dataManager->groupsRepo->CurrentGroup();
                if (currGroup == nullptr) return;
                if (!currGroup->SortProfiles(action)) {
                    runOnUiThread([=] {
                        MessageBoxWarning("Action already in progress", "A sort action is already in progress");
                        });
                    return;
                }
                Configs::dataManager->groupsRepo->Save(Configs::dataManager->groupsRepo->CurrentGroup());
                runOnUiThread([=, this] {
                    refresh_proxy_list();
                    });
                });
            return;
        }
    });
    ui->profilesTableView->verticalHeader()->setStretchLastSection(false);
    ui->profilesTableView->verticalHeader()->setDefaultSectionSize(24);
    ui->profilesTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->profilesTableView->setTabKeyNavigation(false);
    ui->profilesTableView->horizontalHeader()->setResizeContentsPrecision(0);

    connect(ui->profilesTableView->verticalScrollBar(), &QScrollBar::valueChanged, ui->profilesTableView, [=, this] {
        refresh_proxy_list_column_size();
    });

    // search box
    connect(static_cast<ProfilesTableFilterHeader*>(ui->profilesTableView->horizontalHeader()), &ProfilesTableFilterHeader::typeFilterChanged, this, [=,this](const QString& currentText)
    {
       typeFilterString = currentText;
       refresh_proxy_list({}, true);
    });
    connect(static_cast<ProfilesTableFilterHeader*>(ui->profilesTableView->horizontalHeader()), &ProfilesTableFilterHeader::addressFilterChanged, this, [=,this](const QString& currentText)
    {
       addressFilterString = currentText;
       refresh_proxy_list({}, true);
    });
    connect(static_cast<ProfilesTableFilterHeader*>(ui->profilesTableView->horizontalHeader()), &ProfilesTableFilterHeader::nameFilterChanged, this, [=,this](const QString& currentText)
    {
       nameFilterString = currentText;
       refresh_proxy_list({}, true);
    });
    connect(static_cast<ProfilesTableFilterHeader*>(ui->profilesTableView->horizontalHeader()), &ProfilesTableFilterHeader::testFilterChanged, this, [=,this](const QString& currentText)
    {
       countryFilterString = currentText;
       refresh_proxy_list({}, true);
    });

    // refresh
    this->refresh_groups();

    // Setup Tray
    tray = new QSystemTrayIcon(nullptr);
    tray->setIcon(GetTrayIcon(Icon::NONE));
    QApplication::setWindowIcon(Icon::GetTrayIcon(Icon::NONE));
    auto *trayMenu = new QMenu();
    trayMenu->addAction(ui->actionShow_window);
    trayMenu->addSeparator();
    trayMenu->addAction(ui->actionStart_with_system);
    trayMenu->addAction(ui->actionRemember_last_proxy);
    trayMenu->addAction(ui->actionAllow_LAN);
    trayMenu->addSeparator();
    // Select Server submenu (dynamically populated with pagination)
    constexpr int PAGE_SIZE = 15;
    trayServerMenu = new QMenu(tr("Select Server"));
    trayMenu->addMenu(trayServerMenu);
    connect(trayServerMenu, &QMenu::aboutToShow, this, [=, this]() {
        trayServerMenu->clear();
        // Stop action if a profile is running
        if (running) {
            auto *stopAction = trayServerMenu->addAction(tr("Stop: %1").arg(running->name));
            connect(stopAction, &QAction::triggered, this, [=, this]() { profile_stop(false, false, true); });
            trayServerMenu->addSeparator();
        }
        // Build flat list of profiles, starting from the group of the running profile or currentGroup
        int startGroupId = Configs::dataManager->settingsRepo->current_group;
        if (running) startGroupId = running->gid;
        auto groupIds = Configs::dataManager->groupsRepo->GetGroupsTabOrder();
        // Reorder groupIds so startGroupId comes first
        int startIdx = groupIds.indexOf(startGroupId);
        if (startIdx > 0) {
            QList<int> reordered = groupIds.mid(startIdx) + groupIds.mid(0, startIdx);
            groupIds = reordered;
        }
        QList<int> allProfileIDs;
        for (auto gid : groupIds) {
            auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
            allProfileIDs.append(group->Profiles());
        }
        int totalProfiles = allProfileIDs.size();
        // Clamp page
        int maxPage = qMax(0, (totalProfiles - 1) / PAGE_SIZE);
        trayServerPage = qBound(0, trayServerPage, maxPage);
        int offset = trayServerPage * PAGE_SIZE;
        int end = qMin(offset + PAGE_SIZE, totalProfiles);
        // Show ↑ if not on first page
        if (trayServerPage > 0) {
            auto *upAction = trayServerMenu->addAction(QStringLiteral("\u2191"));
            connect(upAction, &QAction::triggered, this, [=, this]() {
                trayServerPage--;
                trayServerMenu->popup(trayServerMenu->pos());
            });
        }
        // Show profiles for current page
        auto neededProfilesIDNames = Configs::dataManager->profilesRepo->GetProfileIDNameMappedBatch(allProfileIDs.sliced(offset, end - offset));
        for (const auto&[id, name] : neededProfilesIDNames) {
            auto *action = trayServerMenu->addAction(name);
            action->setCheckable(true);
            action->setChecked(running && running->id == id);
            connect(action, &QAction::triggered, this, [=, this]() { profile_start(id); });
        }
        // Show ↓ if not on last page
        if (trayServerPage < maxPage) {
            auto *downAction = trayServerMenu->addAction(QStringLiteral("\u2193"));
            connect(downAction, &QAction::triggered, this, [=, this]() {
                trayServerPage++;
                trayServerMenu->popup(trayServerMenu->pos());
            });
        }
    });
    trayMenu->addSeparator();
    // MacOS cannot reuse menus across different parents properly
    if (getOS() == Darwin) {
        auto* traySpmodeMenu = new QMenu(ui->menu_spmode->title(), trayMenu);
        traySpmodeMenu->addAction(ui->menu_spmode_system_proxy);
        traySpmodeMenu->addAction(ui->menu_spmode_vpn);
        traySpmodeMenu->addAction(ui->menu_spmode_disabled);
        connect(traySpmodeMenu, &QMenu::aboutToShow, this, [=,this]() {
            ui->menu_spmode_disabled->setChecked(!(Configs::dataManager->settingsRepo->spmode_system_proxy || Configs::dataManager->settingsRepo->spmode_vpn));
            ui->menu_spmode_system_proxy->setChecked(Configs::dataManager->settingsRepo->spmode_system_proxy);
            ui->menu_spmode_vpn->setChecked(Configs::dataManager->settingsRepo->spmode_vpn);
        });
        trayMenu->addMenu(traySpmodeMenu);
    } else {
        trayMenu->addMenu(ui->menu_spmode);
    }
    trayMenu->addSeparator();
    trayMenu->addAction(ui->actionRestart_Proxy);
    trayMenu->addAction(ui->actionRestart_Program);
    trayMenu->addAction(ui->menu_exit);
    tray->setVisible(!Configs::dataManager->settingsRepo->disable_tray);
    tray->setContextMenu(trayMenu);
    connect(trayMenu, &QMenu::aboutToShow, this, [=,this]() {
       trayServerPage = 0;
    });
    connect(tray, &QSystemTrayIcon::activated, qApp, [=, this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger && getOS() != Darwin) {
            ActivateWindow(this);
            refresh_proxy_list_column_size();
        }
    });

    // Misc menu
    ui->actionRemember_last_proxy->setChecked(Configs::dataManager->settingsRepo->remember_enable);
    ui->actionStart_with_system->setChecked(AutoRun_IsEnabled());
    ui->actionAllow_LAN->setChecked(QStringList{"::", "0.0.0.0"}.contains(Configs::dataManager->settingsRepo->inbound_address));

    connect(ui->actionHide_window, &QAction::triggered, this, [=, this](){ HideWindow(this); });
    connect(ui->menu_open_config_folder, &QAction::triggered, this, [=,this] { QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::currentPath())); });
    connect(ui->menu_add_from_clipboard2, &QAction::triggered, ui->menu_add_from_clipboard, &QAction::trigger);
    connect(ui->actionRestart_Proxy, &QAction::triggered, this, [=,this] {
        runOnThread([=, this] {
            profile_stop(true, true, true);
            core_process->Kill();
        }, DS_cores);
    });
    connect(ui->actionRestart_Program, &QAction::triggered, this, [=,this] { MW_dialog_message("", "RestartProgram"); });
    connect(ui->actionShow_window, &QAction::triggered, this, [=,this] { ActivateWindow(this); });
    connect(ui->actionRemember_last_proxy, &QAction::triggered, this, [=,this](bool checked) {
        Configs::dataManager->settingsRepo->remember_enable = checked;
        ui->actionRemember_last_proxy->setChecked(checked);
        Configs::dataManager->settingsRepo->Save();
    });
    connect(ui->actionStart_with_system, &QAction::triggered, this, [=,this](bool checked) {
        AutoRun_SetEnabled(checked);
        ui->actionStart_with_system->setChecked(checked);
    });
    connect(ui->actionAllow_LAN, &QAction::triggered, this, [=,this](bool checked) {
        Configs::dataManager->settingsRepo->inbound_address = checked ? "::" : "127.0.0.1";
        ui->actionAllow_LAN->setChecked(checked);
        MW_dialog_message("", "UpdateConfigs::dataManager->settingsRepo");
    });
    //
    connect(ui->checkBox_VPN, &QCheckBox::clicked, this, [=,this](bool checked) { set_spmode_vpn(checked); });
    connect(ui->checkBox_SystemProxy, &QCheckBox::clicked, this, [=,this](bool checked) { set_spmode_system_proxy(checked); });
    connect(ui->menu_spmode, &QMenu::aboutToShow, this, [=,this]() {
        ui->menu_spmode_disabled->setChecked(!(Configs::dataManager->settingsRepo->spmode_system_proxy || Configs::dataManager->settingsRepo->spmode_vpn));
        ui->menu_spmode_system_proxy->setChecked(Configs::dataManager->settingsRepo->spmode_system_proxy);
        ui->menu_spmode_vpn->setChecked(Configs::dataManager->settingsRepo->spmode_vpn);
    });
    connect(ui->menu_spmode_system_proxy, &QAction::triggered, this, [=,this](bool checked) { set_spmode_system_proxy(checked); });
    connect(ui->menu_spmode_vpn, &QAction::triggered, this, [=,this](bool checked) { set_spmode_vpn(checked); });
    connect(ui->menu_spmode_disabled, &QAction::triggered, this, [=,this]() {
        set_spmode_system_proxy(false);
        set_spmode_vpn(false);
    });
    connect(ui->menu_qr, &QAction::triggered, this, [=,this]() { display_qr_link(false); });
    connect(ui->system_dns, &QCheckBox::clicked, this, [=,this](bool checked) {
        if (const auto ok = set_system_dns(checked); !ok) {
            ui->system_dns->setChecked(!checked);
        } else {
            refresh_status();
        }
    });
    if (Configs::dataManager->settingsRepo->show_system_dns) ui->system_dns->show();
    else ui->system_dns->hide();

    connect(ui->menu_server, &QMenu::aboutToShow, this, [=,this](){
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
            ui->actionResolve_Selected_Out_IP->setEnabled(false);
        } else
        {
            ui->actionSpeedtest_Selected->setEnabled(true);
            ui->actionUrl_Test_Selected->setEnabled(true);
            ui->menu_resolve_selected->setEnabled(true);
            ui->actionResolve_Selected_Out_IP->setEnabled(true);
        }
        if (!speedtestRunning.tryLock()) {
            ui->menu_server->addAction(ui->menu_stop_testing);
        } else {
            speedtestRunning.unlock();
            ui->menu_server->removeAction(ui->menu_stop_testing);
        }
    });

    auto getRemoteRouteProfiles = [=,this]
    {
        auto resp = NetworkRequestHelper::HttpGet("https://api.github.com/repos/throneproj/routeprofiles/git/trees/profile");
        if (resp.error.isEmpty()) {
            QStringList newRemoteRouteProfiles;
            QJsonObject release = QString2QJsonObject(resp.data);
            for (const QJsonValue asset : release["tree"].toArray()) {
                auto profile = asset["path"].toString();
                if (profile.section('.', -1) == QString("json") && (profile.startsWith("bypass",Qt::CaseInsensitive) || profile.startsWith("proxy",Qt::CaseInsensitive))) {
                    profile.chop(5);
                    newRemoteRouteProfiles.push_back(profile);
                }
            }
            mu_remoteRouteProfiles.lock();
            remoteRouteProfiles = newRemoteRouteProfiles;
            mu_remoteRouteProfiles.unlock();
        }
    };
    runOnNewThread(getRemoteRouteProfiles);

    connect(ui->actionRefresh_Column_Widths, &QAction::triggered, this, [=, this] {
        auto ent = Configs::dataManager->groupsRepo->CurrentGroup();
        ent->column_width.clear();
        Configs::dataManager->groupsRepo->Save(ent);
        show_group(ent->id);
    });

    connect(ui->menuRouting_Menu, &QMenu::aboutToShow, this, [=,this]()
    {
        if(remoteRouteProfiles.isEmpty())
            runOnNewThread(getRemoteRouteProfiles);
        ui->menuRouting_Menu->clear();
        ui->menuRouting_Menu->addAction(ui->menu_routing_settings);

        auto* actionAdblock = new QAction(ui->menuRouting_Menu);
        actionAdblock->setText("Enable AdBlock");
        actionAdblock->setCheckable(true);
        actionAdblock->setChecked(Configs::dataManager->settingsRepo->adblock_enable);
        connect(actionAdblock, &QAction::triggered, this, [=,this](bool checked) {
            Configs::dataManager->settingsRepo->adblock_enable = checked;
            actionAdblock->setChecked(checked);
            Configs::dataManager->settingsRepo->Save();
            if (Configs::dataManager->settingsRepo->started_id >= 0) profile_start(Configs::dataManager->settingsRepo->started_id);
        });
        ui->menuRouting_Menu->addAction(actionAdblock);

        auto* actionWarp = new QAction(ui->menuRouting_Menu);
        actionWarp->setText("Enable Warp");
        actionWarp->setCheckable(true);
        actionWarp->setChecked(Configs::dataManager->settingsRepo->enable_warp);
        connect(actionWarp, &QAction::triggered, this, [=,this](bool checked) {
            Configs::dataManager->settingsRepo->enable_warp = checked;
            actionWarp->setChecked(checked);
            Configs::dataManager->settingsRepo->Save();
            if (Configs::dataManager->settingsRepo->started_id >= 0) profile_start(Configs::dataManager->settingsRepo->started_id);
        });
        ui->menuRouting_Menu->addAction(actionWarp);

        mu_remoteRouteProfiles.lock();
        if(!remoteRouteProfiles.isEmpty()) {
            QMenu* profilesMenu = ui->menuRouting_Menu->addMenu(QObject::tr("Download Profiles"));
            for (const auto& profile : remoteRouteProfiles)
            {
                auto* action = new QAction(profilesMenu);
                action->setText(profile);
                connect(action, &QAction::triggered, this, [=,this]()
                {
                    auto resp = NetworkRequestHelper::HttpGet(Configs::get_jsdelivr_link("https://raw.githubusercontent.com/throneproj/routeprofiles/profile/" + profile + ".json"));
                    if (!resp.error.isEmpty()) {
                        runOnUiThread([=] {
                            MessageBoxWarning(QObject::tr("Download Profiles"), QObject::tr("Requesting profile error: %1").arg(resp.error + "\n" + resp.data));
                        });
                        return;
                    }
                    auto err = new QString;
                    auto parsed = Configs::RouteProfile::parseJsonArray(QString2QJsonArray(resp.data), err);
                    if (!err->isEmpty()) {
                        runOnUiThread([=]
                        {
                            MessageBoxInfo(tr("Invalid JSON Array"), tr("The provided input cannot be parsed to a valid route rule array:\n") + *err);
                        });
                        return;
                    }
                    auto chain = Configs::dataManager->routesRepo->NewRouteProfile();
                    chain->name = QString(profile).replace('_', ' ');
                    chain->defaultOutboundID = profile.startsWith("bypass",Qt::CaseInsensitive) ? Configs::proxyID : Configs::directID;
                    chain->Rules.clear();
                    chain->Rules << parsed;
                    Configs::dataManager->routesRepo->AddRouteProfile(chain);
                });
                profilesMenu->addAction(action);
            }
        }
        mu_remoteRouteProfiles.unlock();

        ui->menuRouting_Menu->addSeparator();
        for (const auto& route : Configs::dataManager->routesRepo->GetAllRouteProfiles())
        {
            auto* action = new QAction(ui->menuRouting_Menu);
            action->setText(route->name);
            action->setData(route->id);
            action->setCheckable(true);
            action->setChecked(Configs::dataManager->settingsRepo->current_route_id == route->id);
            connect(action, &QAction::triggered, this, [=,this]()
            {
                auto routeID = action->data().toInt();
                if (Configs::dataManager->settingsRepo->current_route_id == routeID) return;
                Configs::dataManager->settingsRepo->current_route_id = routeID;
                Configs::dataManager->settingsRepo->Save();
                if (Configs::dataManager->settingsRepo->started_id >= 0) profile_start(Configs::dataManager->settingsRepo->started_id);
            });
            ui->menuRouting_Menu->addAction(action);
        }
    });
    connect(ui->actionUrl_Test_Selected, &QAction::triggered, this, [=,this]() {
        urltest_current_group(get_now_selected_list());
    });
    connect(ui->actionUrl_Test_Group, &QAction::triggered, this, [=,this]() {
        urltest_current_group(Configs::dataManager->groupsRepo->CurrentGroup()->Profiles());
    });
    connect(ui->actionSpeedtest_Current, &QAction::triggered, this, [=,this]()
    {
        if (running != nullptr)
        {
            speedtest_current_group({}, true);
        }
    });
    connect(ui->actionSpeedtest_Selected, &QAction::triggered, this, [=,this]()
    {
        speedtest_current_group(get_now_selected_list());
    });
    connect(ui->actionSpeedtest_Group, &QAction::triggered, this, [=,this]()
    {
        speedtest_current_group(Configs::dataManager->groupsRepo->CurrentGroup()->Profiles());
    });
    connect(ui->actionResolve_Selected_Out_IP, &QAction::triggered, this, [=,this]() {
        iptest_current_group(get_now_selected_list());
    });
    connect(ui->actionResolve_Out_IP, &QAction::triggered, this, [=,this]() {
        iptest_current_group(Configs::dataManager->groupsRepo->CurrentGroup()->Profiles());
    });
    connect(ui->menu_stop_testing, &QAction::triggered, this, [=,this]() { stopTests(); });
    //
    auto set_selected_or_group = [=,this](int mode) {
        // 0=group 1=select 2=unknown(menu is hide)
        ui->menu_server->setProperty("selected_or_group", mode);
    };
    connect(ui->menu_server, &QMenu::aboutToHide, this, [=,this] {
        setTimeout([=,this] { set_selected_or_group(2); }, this, 200);
    });
    set_selected_or_group(2);
    //
    connect(ui->menu_share_item, &QMenu::aboutToShow, this, [=,this] {
        QString name;
        auto selected = get_now_selected_list();

        ui->menu_export_config->setVisible(false);
        ui->actionExport_Xray_config->setVisible(false);
        if (selected.isEmpty()) return;

        auto profile = Configs::dataManager->profilesRepo->GetProfile(selected.first());
        if (!profile) return;

        ui->menu_export_config->setVisible(true);
        if (profile->outbound->IsXray()) ui->actionExport_Xray_config->setVisible(true);
    });
    connect(ui->actionExport_Xray_config, &QAction::triggered, this, [=,this]() {
        auto ents = get_now_selected_list();
        if (ents.count() != 1) return;
        auto ent = Configs::dataManager->profilesRepo->GetProfile(ents.first());

        auto result = Configs::BuildSingBoxConfig(ent);
        if (!result->error.isEmpty()) {
            MessageBoxWarning("Build config error", result->error);
            return;
        }
        QString config_core = QJsonObject2QString(result->xrayConfig, true);
        QApplication::clipboard()->setText(config_core);

        QMessageBox msg(QMessageBox::Information, tr("Config copied"), config_core);
        QPushButton *button_1 = msg.addButton(tr("Copy core config"), QMessageBox::YesRole);
        QPushButton *button_2 = msg.addButton(tr("Copy test config"), QMessageBox::YesRole);
        msg.addButton(QMessageBox::Ok);
        msg.setEscapeButton(QMessageBox::Ok);
        msg.setDefaultButton(QMessageBox::Ok);
        msg.exec();
        if (msg.clickedButton() == button_1) {
            QApplication::clipboard()->setText(config_core);
        } else if (msg.clickedButton() == button_2) {
            auto res = Configs::BuildTestConfig({ent});
            if (!res->error.isEmpty()) {
                MessageBoxWarning("Build Test config error", res->error);
                return;
            }
            config_core = QJsonObject2QString(res->xrayConfig, true);
            QApplication::clipboard()->setText(config_core);
        }
    });
    connect(ui->actionAdd_profile_from_File, &QAction::triggered, this, [=,this]()
    {
        auto path = QFileDialog::getOpenFileName();
        if (path.isEmpty())
        {
            return;
        }
        auto file = QFile(path);
        if (!file.exists()) return;
        if (file.size() > 50 * 1024 * 1024)
        {
            MW_show_log("File too large, will not process it");
            return;
        }
        file.open(QIODevice::ReadOnly);
        auto contents = file.readAll();
        file.close();
        Subscription::groupUpdater->AsyncUpdate(contents);
    });

    connect(qApp, &QGuiApplication::commitDataRequest, this, &MainWindow::on_commitDataRequest);

    auto t = new QTimer;
    connect(t, &QTimer::timeout, this, [=,this]() { refresh_status(); });
    t->start(2000);

    t = new QTimer;
    connect(t, &QTimer::timeout, this, [&] { Configs_sys::logCounter.fetchAndStoreRelaxed(0); });
    t->start(1000);

    // auto update timer
    TM_auto_update_subsctiption = new QTimer;
    TM_auto_update_subsctiption_Reset_Minute = [&](int m) {
        TM_auto_update_subsctiption->stop();
        if (m >= 30) TM_auto_update_subsctiption->start(m * 60 * 1000);
    };
    connect(TM_auto_update_subsctiption, &QTimer::timeout, this, [&] { UI_update_all_groups(true); });
    TM_auto_update_subsctiption_Reset_Minute(Configs::dataManager->settingsRepo->sub_auto_update);

    if (!Configs::dataManager->settingsRepo->flag_tray) show();

    ui->data_view->setStyleSheet("background: transparent; border: none;");
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (tray->isVisible()) {
        HideWindow(this);
        event->ignore();
    } else {
        on_menu_exit_triggered();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    auto mimeData = event->mimeData();

    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        for (const QUrl &url : urlList) {
            if (url.isLocalFile()) {
                if (auto qpx = QPixmap(url.toLocalFile()); !qpx.isNull())
                {
                    parseQrImage(&qpx);
                } else if (auto file = QFile(url.toLocalFile()); file.exists())
                {
                    file.open(QFile::ReadOnly);
                    if (file.size() > 50 * 1024 * 1024)
                    {
                        file.close();
                        MW_show_log("File size is larger than 50MB, will not parse it");
                        event->acceptProposedAction();
                        return;
                    }
                    auto contents = file.readAll();
                    file.close();
                    Subscription::groupUpdater->AsyncUpdate(contents);
                }
            }
        }
        event->acceptProposedAction();
        return;
    }

    if (mimeData->hasText()) {
        Subscription::groupUpdater->AsyncUpdate(mimeData->text());
        event->acceptProposedAction();
        return;
    }

    event->ignore();
}

MainWindow::~MainWindow() {
    delete ui;
}

// Group tab manage

inline int tabIndex2GroupId(int index) {
    auto tabOrder = Configs::dataManager->groupsRepo->GetGroupsTabOrder();
    if (tabOrder.length() <= index) return -1;
    return tabOrder[index];
}

inline int groupId2TabIndex(int gid) {
    auto tabOrder = Configs::dataManager->groupsRepo->GetGroupsTabOrder();
    for (int key = 0; key < tabOrder.count(); key++) {
        if (tabOrder[key] == gid) return key;
    }
    return 0;
}

void MainWindow::on_tabWidget_currentChanged(int index) {
    if (Configs::dataManager->settingsRepo->refreshing_group_list) return;
    auto gid = tabIndex2GroupId(index);
    if (gid == Configs::dataManager->settingsRepo->current_group) return;
    show_group(gid);
}

void MainWindow::show_group(int gid) {
    if (Configs::dataManager->settingsRepo->refreshing_group) return;
    Configs::dataManager->settingsRepo->refreshing_group = true;

    auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
    if (group == nullptr) {
        MessageBoxWarning(tr("Error"), QString("No such group: %1").arg(gid));
        Configs::dataManager->settingsRepo->refreshing_group = false;
        return;
    }

    if (Configs::dataManager->settingsRepo->current_group != gid) {
        if (auto lastGroup = Configs::dataManager->groupsRepo->CurrentGroup()) {
            lastGroup->scroll_last_profile = ui->profilesTableView->firstVisibleRow();
            Configs::dataManager->groupsRepo->Save(lastGroup);
        }
        Configs::dataManager->settingsRepo->current_group = gid;
        Configs::dataManager->settingsRepo->Save();
    }

    ui->tabWidget->widget(groupId2TabIndex(gid))->layout()->addWidget(ui->profilesTableView);

    // show proxies
    refresh_proxy_list({}, true);

    int rowCount = profilesTableModel->rowCount();
    int targetRow = group->scroll_last_profile;
    if (targetRow >= rowCount && rowCount > 0) targetRow = rowCount - 1;
    QTimer::singleShot(0, ui->profilesTableView, [=, this]() {
        if (targetRow >= 0) {
            if (QModelIndex idx = profilesTableModel->index(targetRow, 0); idx.isValid()) {
                ui->profilesTableView->scrollTo(idx, QAbstractItemView::PositionAtTop);
            }
        }
        refresh_proxy_list_column_size();
    });

    Configs::dataManager->settingsRepo->refreshing_group = false;
}

// callback

void MainWindow::dialog_message_impl(const QString &sender, const QString &info) {
    // info
    if (info.contains("UpdateTrayIcon")) {
        icon_status = -1;
        refresh_status();
    }
    if (info.contains("UpdateConfigs::dataManager->settingsRepo")) {
        updateLogFilterFields();
        if (info.contains("UpdateMaxLogLines")) {
            qvLogDocument->setMaximumBlockCount(Configs::dataManager->settingsRepo->max_log_line);
        }
        if (info.contains("UpdateDisableTray")) {
            tray->setVisible(!Configs::dataManager->settingsRepo->disable_tray);
        }
        if (info.contains("UpdateSystemDns"))
        {
            if (Configs::dataManager->settingsRepo->show_system_dns) ui->system_dns->show();
            else ui->system_dns->hide();
        }
        if (info.contains("NeedChoosePort"))
        {
            Configs::dataManager->settingsRepo->inbound_socks_port = MkPort();
            if (Configs::dataManager->settingsRepo->spmode_system_proxy)
            {
                set_spmode_system_proxy(false);
                set_spmode_system_proxy(true);
            }
        }
        if (info.contains("UpdateDisableAdmin")) {
            AutoRun_FixPrivilegeIfNeeded();
        }
        auto suggestRestartProxy = Configs::dataManager->settingsRepo->Save();
        if (info.contains("RouteChanged")) {
            Configs::dataManager->settingsRepo->Save();
            suggestRestartProxy = true;
        }
        if (info.contains("NeedRestart")) {
            suggestRestartProxy = false;
        }
        if (info.contains("VPNChanged") && Configs::dataManager->settingsRepo->spmode_vpn) {
            MessageBoxWarning(tr("Tun Settings changed"), tr("Restart Tun to take effect."));
        }
        if ((info.contains("NeedChoosePort") || suggestRestartProxy) && Configs::dataManager->settingsRepo->started_id >= 0 &&
            QMessageBox::question(GetMessageBoxParent(), tr("Confirmation"), tr("Settings changed, restart proxy?")) == QMessageBox::StandardButton::Yes) {
            profile_start(Configs::dataManager->settingsRepo->started_id);
        }
        refresh_status();
    }
    if (info.contains("DNSServerChanged"))
    {
        if (Configs::dataManager->settingsRepo->system_dns_set)
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
    if (info == "UpdateShortcuts")
    {
        loadShortcuts();
    }
    // sender
    if (sender == Dialog_DialogEditProfile) {
        auto msg = info.split(",");
        if (msg.contains("accept")) {
            refresh_proxy_list({}, true);
            if (msg.contains("restart")) {
                if (QMessageBox::question(GetMessageBoxParent(), tr("Confirmation"), tr("Settings changed, restart proxy?")) == QMessageBox::StandardButton::Yes) {
                    profile_start(Configs::dataManager->settingsRepo->started_id);
                }
            }
        }
    } else if (sender == Dialog_DialogManageGroups) {
        if (info.startsWith("refresh")) {
            this->refresh_groups();
        }
    } else if (sender == "SubUpdater") {
        if (info.startsWith("finish")) {
            refresh_proxy_list({}, true);
            if (!info.contains("dingyue")) {
                MW_show_log(tr("Imported %1 profile(s)").arg(Configs::dataManager->settingsRepo->imported_count));
            }
        } else if (info == "NewGroup") {
            refresh_groups();
        }
    } else if (sender == "ExternalProcess") {
        if (info == "Crashed") {
            profile_stop();
        } else if (info.startsWith("CoreStarted")) {
            Configs::IsAdmin(true);
            if (Configs::dataManager->settingsRepo->remember_spmode.contains("system_proxy")) {
                set_spmode_system_proxy(true, false);
            }
            if (Configs::dataManager->settingsRepo->remember_spmode.contains("vpn") || Configs::dataManager->settingsRepo->flag_restart_tun_on) {
                set_spmode_vpn(true, false);
            }
            if (Configs::dataManager->settingsRepo->flag_dns_set) {
                set_system_dns(true);
            }
            if (auto id = info.split(",")[1].toInt(); id >= 0)
            {
                profile_start(id);
            }
            if (Configs::dataManager->settingsRepo->system_dns_set) {
                set_system_dns(true);
                ui->system_dns->setChecked(true);
            }
            refresh_status();
        }
    }
}

// top bar & tray menu

inline bool dialog_is_using = false;

#define USE_DIALOG(a)                               \
    if (dialog_is_using) return;                    \
    dialog_is_using = true;                         \
    auto dialog = new a(this);                      \
    connect(dialog, &QDialog::finished, this, [=,this] { \
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
    if (dialog_is_using) return;
    dialog_is_using = true;
    auto dialog = new DialogManageRoutes(this);
    connect(dialog, &QDialog::finished, this, [=,this] {
        dialog->deleteLater();
        dialog_is_using = false;
    });
    dialog->show();
}

void MainWindow::on_menu_vpn_settings_triggered() {
    USE_DIALOG(DialogVPNSettings)
}

void MainWindow::on_menu_hotkey_settings_triggered() {
    if (dialog_is_using) return;
    dialog_is_using = true;
    auto dialog = new DialogHotkey(this, getActionsForShortcut());
    connect(dialog, &QDialog::finished, this, [=,this]
    {
        dialog->deleteLater();
        dialog_is_using = false;
    });
    dialog->show();
}

void MainWindow::on_commitDataRequest() {
    qDebug() << "Start of data save";
    //
    Configs::dataManager->settingsRepo->mainWindowGeometry = this->saveGeometry().toBase64(QByteArray::Base64Encoding);
    if (!isMaximized()) {
        auto olds = Configs::dataManager->settingsRepo->mw_size;
        auto news = QString("%1x%2").arg(size().width()).arg(size().height());
        if (olds != news) {
            Configs::dataManager->settingsRepo->mw_size = news;
        }
    }
    //
    Configs::dataManager->settingsRepo->splitter_state = ui->splitter->saveState().toBase64();
    //
    auto last_id = Configs::dataManager->settingsRepo->started_id;
    if (Configs::dataManager->settingsRepo->remember_enable && last_id >= 0) {
        Configs::dataManager->settingsRepo->remember_id = last_id;
    }
    //
    Configs::dataManager->settingsRepo->Save();
    qDebug() << "End of data save";
}

void MainWindow::prepare_exit()
{
    qDebug() << "prepare for exit...";
    mu_exit.lock();
    if (Configs::dataManager->settingsRepo->prepare_exit)
    {
        qDebug() << "prepare exit had already succeeded, ignoring...";
        mu_exit.unlock();
        return;
    }
    Configs::dataManager->settingsRepo->prepare_exit = true;
    //
    set_spmode_system_proxy(false, false);
    if (Configs::dataManager->settingsRepo->system_dns_set) set_system_dns(false, false);
    RegisterHiddenMenuShortcuts(true);
    RegisterHotkey(true);
    //
    on_commitDataRequest();
    //
    Configs::dataManager->settingsRepo->noSave = true; // don't change Configs::dataManager->settingsRepo after this line
    profile_stop(false, true);

    runOnThread([=, this]()
    {
        core_process->Kill();
    }, DS_cores, true);
    HideWindow(this);

    mu_exit.unlock();
    qDebug() << "prepare exit done!";
}

void MainWindow::on_menu_exit_triggered() {
    prepare_exit();
    //
    if (exit_reason == 1) {
        QDir::setCurrent(QApplication::applicationDirPath());
#ifdef Q_OS_WIN
        QFile::copy("./updater.exe", "./updater.old");
        QProcess::startDetached("./updater.old", QStringList{});
#else
        QProcess::startDetached("./updater", QStringList{});
#endif
    } else if (exit_reason == 2 || exit_reason == 3 || exit_reason == 4) {
        QDir::setCurrent(QApplication::applicationDirPath());

        auto arguments = Configs::dataManager->settingsRepo->argv;
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
    auto currentState = Configs::dataManager->settingsRepo->spmode_system_proxy;
    if (currentState) {
        set_spmode_system_proxy(false);
    } else {
        set_spmode_system_proxy(true);
    }
}

bool MainWindow::get_elevated_permissions(int reason) {
    if (Configs::dataManager->settingsRepo->disable_privilege_req)
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
        runOnNewThread([=,this]
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
    if (Configs::isSetuidSet(Configs::FindCoreRealPath().toStdString()))
    {
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
    if (enable == Configs::dataManager->settingsRepo->spmode_vpn) return;

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
        Configs::dataManager->settingsRepo->remember_spmode.removeAll("vpn");
        if (enable) {
            Configs::dataManager->settingsRepo->remember_spmode.append("vpn");
        }
        Configs::dataManager->settingsRepo->Save();
    }

    Configs::dataManager->settingsRepo->spmode_vpn = enable;
    refresh_status();

    if (Configs::dataManager->settingsRepo->started_id >= 0) profile_start(Configs::dataManager->settingsRepo->started_id);
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
        qint64 count = 0;
        if(currentDownloadReport.totalSize > 0)
            count = 10 * currentDownloadReport.downloadedSize / currentDownloadReport.totalSize;
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
    "<p style='text-align:center;margin:0;'>Server: %4%5, %6</p>"
        ).arg(currentSptProfileName,
            currentTestResult.dl_speed.value().c_str(),
            currentTestResult.ul_speed.value().c_str(),
            CountryCodeToFlag(CountryNameToCode(QString::fromStdString(currentTestResult.server_country.value()))),
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
    connect(ui->connections, &QTableWidget::cellClicked, this, [=,this](int row, int column)
    {
        if (column > 3) return;
        auto selected = ui->connections->item(row, column);
        QApplication::clipboard()->setText(selected->text());
        QPoint pos = ui->connections->mapToGlobal(ui->connections->visualItemRect(selected).center());
        QToolTip::showText(pos, "Copied!", this);
        auto r = ++toolTipID;
        QTimer::singleShot(1500, [=,this] {
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
    connectionListMu.lock();
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
    connectionListMu.unlock();
}

void MainWindow::UpdateConnectionListWithRecreate(const QList<Stats::ConnectionMetadata>& connections)
{
    connectionListMu.lock();
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
    connectionListMu.unlock();
}

void MainWindow::updateLogFilterFields() {
    QMutexLocker locker(&logMutex);
    includeKeywords.clear();
    excludeKeywords.clear();
    for (const auto& inKeyword : Configs::dataManager->settingsRepo->log_include_keyword) includeKeywords.append(inKeyword);
    for (const auto& exKeyword : Configs::dataManager->settingsRepo->log_exclude_keyword) excludeKeywords.append(exKeyword);
    includeCombined.setPattern(Configs::dataManager->settingsRepo->log_include_regex.join("|"));
    excludeCombined.setPattern(Configs::dataManager->settingsRepo->log_exclude_regex.join("|"));
    includeCombined.optimize();
    excludeCombined.optimize();
}

QList<int> MainWindow::filterProfilesList(const QList<int>& profileIDs)
{
    if (addressFilterString.isEmpty() && nameFilterString.isEmpty() && typeFilterString.isEmpty() && countryFilterString.isEmpty()) return profileIDs;
    QList<int> res;
    auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(profileIDs);
    for (const auto& profile : profiles)
    {
        if (!profile)
        {
            MW_show_log("Null profile, maybe data is corrupted");
            continue;
        }
        if ((addressFilterString.isEmpty() || profile->outbound->server.contains(addressFilterString, Qt::CaseInsensitive))
            && (nameFilterString.isEmpty() || profile->outbound->name.contains(nameFilterString, Qt::CaseInsensitive))
            && (typeFilterString.isEmpty() || profile->type.contains(typeFilterString, Qt::CaseInsensitive))
            && (countryFilterString.isEmpty() || profile->test_country.contains(countryFilterString, Qt::CaseInsensitive)))
            res.append(profile->id);
    }
    return res;
}

void MainWindow::refresh_status(const QString &traffic_update) {
    auto refresh_speed_label = [=,this] {
        if (Configs::dataManager->settingsRepo->disable_traffic_stats) {
            ui->label_speed->setText("");
        }
        else if (traffic_update_cache == "") {
            ui->label_speed->setText(QObject::tr("Proxy: %1\nDirect: %2").arg("", ""));
        } else {
            ui->label_speed->setText(traffic_update_cache);
        }
    };

    // From TrafficLooper
    if (!traffic_update.isEmpty() && !Configs::dataManager->settingsRepo->disable_traffic_stats) {
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
        auto group = Configs::dataManager->groupsRepo->GetGroup(running->gid);
        if (group != nullptr) group_name = group->name;
    }

    if (QDateTime::currentSecsSinceEpoch() - last_test_time > 2) {
        QString runningLabelText;
        if (running) {
            runningLabelText = QString("[%1] %2").arg(group_name, running->outbound->DisplayName());
            if (!running->runningCountryInfo.isEmpty()) {
                runningLabelText += "\n" + running->runningCountryInfo;
            }
        } else {
            runningLabelText = tr("Not Running");
        }
        ui->label_running->setText(runningLabelText);
    }
    //
    auto display_socks = DisplayAddress(Configs::dataManager->settingsRepo->inbound_address, Configs::dataManager->settingsRepo->inbound_socks_port);
    auto inbound_txt = QString("Mixed: %1").arg(display_socks);
    ui->label_inbound->setText(inbound_txt);
    //
    ui->checkBox_VPN->setChecked(Configs::dataManager->settingsRepo->spmode_vpn);
    ui->checkBox_SystemProxy->setChecked(Configs::dataManager->settingsRepo->spmode_system_proxy);
    if (select_mode) {
        ui->label_running->setText(tr("Select") + " *");
        ui->label_running->setToolTip(tr("Select mode, double-click or press Enter to select a profile, press ESC to exit."));
    } else {
        ui->label_running->setToolTip({});
    }

    auto make_title = [=,this](bool isTray) {
        QStringList tt;
        if (!isTray && Configs::IsAdmin()) tt << "[Admin]";
        if (select_mode) tt << "[" + tr("Select") + "]";
        if (!title_error.isEmpty()) tt << "[" + title_error + "]";
        if (Configs::dataManager->settingsRepo->spmode_vpn && !Configs::dataManager->settingsRepo->spmode_system_proxy) tt << "[Tun]";
        if (!Configs::dataManager->settingsRepo->spmode_vpn && Configs::dataManager->settingsRepo->spmode_system_proxy) tt << "[" + tr("System Proxy") + "]";
        if (Configs::dataManager->settingsRepo->spmode_vpn && Configs::dataManager->settingsRepo->spmode_system_proxy) tt << "[Tun+" + tr("System Proxy") + "]";
        tt << software_name;
        if (!isTray) tt << QString(NKR_VERSION);
        if (!Configs::dataManager->settingsRepo->active_routing.isEmpty() && Configs::dataManager->settingsRepo->active_routing != "Default") {
            tt << "[" + Configs::dataManager->settingsRepo->active_routing + "]";
        }
        if (running != nullptr) {
            tt << running->outbound->DisplayTypeAndName() + "@" + group_name;
            if (!running->runningCountryInfo.isEmpty()) {
                tt << running->runningCountryInfo;
            }
        }
        return tt.join(isTray ? "\n" : " ");
    };

    auto icon_status_new = Icon::NONE;

    if (running != nullptr) {
        if (Configs::dataManager->settingsRepo->spmode_vpn) {
            icon_status_new = Icon::VPN;
        } else if (Configs::dataManager->settingsRepo->system_dns_set && Configs::dataManager->settingsRepo->spmode_system_proxy) {
            icon_status_new = Icon::SYSTEM_PROXY_DNS;
        } else if (Configs::dataManager->settingsRepo->system_dns_set) {
            icon_status_new = Icon::DNS;
        } else if (Configs::dataManager->settingsRepo->spmode_system_proxy) {
            icon_status_new = Icon::SYSTEM_PROXY;
        } else {
            icon_status_new = Icon::RUNNING;
        }
    }

    // refresh title & window icon
    setWindowTitle(make_title(false));
    if (icon_status_new != icon_status) QApplication::setWindowIcon(GetTrayIcon(icon_status_new));

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
    Configs::dataManager->settingsRepo->refreshing_group_list = true;

    // refresh group?
    for (int i = ui->tabWidget->count() - 1; i > 0; i--) {
        ui->tabWidget->removeTab(i);
    }

    int index = 0;
    for (const auto &gid: Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
        auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
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
    if (Configs::dataManager->groupsRepo->CurrentGroup() == nullptr) {
        Configs::dataManager->settingsRepo->current_group = -1;
        ui->tabWidget->setCurrentIndex(groupId2TabIndex(0));
        show_group(Configs::dataManager->groupsRepo->GetGroupsTabOrder().count() > 0 ? Configs::dataManager->groupsRepo->GetGroupsTabOrder().first() : 0);
    } else {
        ui->tabWidget->setCurrentIndex(groupId2TabIndex(Configs::dataManager->settingsRepo->current_group));
        show_group(Configs::dataManager->settingsRepo->current_group);
    }

    Configs::dataManager->settingsRepo->refreshing_group_list = false;
}

void MainWindow::refresh_proxy_list_column_size() {
    auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (!group) return;

    auto *hHeader = dynamic_cast<ProfilesTableFilterHeader*>(ui->profilesTableView->horizontalHeader());
    QTimer::singleShot(0, ui->profilesTableView, [=, this]() {
        hHeader->blockSignals(true);
        if (group->column_width.isEmpty()) {
            hHeader->setSectionResizeMode(0, QHeaderView::ResizeToContents);
            hHeader->setSectionResizeMode(1, QHeaderView::Stretch);
            hHeader->setSectionResizeMode(2, QHeaderView::Stretch);
            hHeader->setSectionResizeMode(3, QHeaderView::ResizeToContents);
            hHeader->setSectionResizeMode(4, QHeaderView::ResizeToContents);
            if (!group->calculated_column_width.empty() && group->calculated_column_width[0] > hHeader->sectionSize(0)) {
                hHeader->setSectionResizeMode(0, QHeaderView::Fixed);
                hHeader->resizeSection(0, group->calculated_column_width[0]);
            }
            if (group->calculated_column_width.size() > 3 && group->calculated_column_width[3] > hHeader->sectionSize(3)) {
                hHeader->setSectionResizeMode(3, QHeaderView::Fixed);
                hHeader->resizeSection(3, group->calculated_column_width[3]);
            }
            if (group->calculated_column_width.size() > 4 && group->calculated_column_width[4] > hHeader->sectionSize(4)) {
                hHeader->setSectionResizeMode(4, QHeaderView::Fixed);
                hHeader->resizeSection(4, group->calculated_column_width[4]);
            }
            ui->profilesTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            group->clearCalculatedColumnWidth();
            for (int i=0;i<=4;i++) {
                auto size = hHeader->sectionSize(i);
                hHeader->setSectionResizeMode(i, QHeaderView::Interactive);
                hHeader->resizeSection(i, size);
                group->calculated_column_width << size;
            }
        } else {
            group->clearCalculatedColumnWidth();
            for (int i=0;i<=4;i++) {
                hHeader->setSectionResizeMode(i, QHeaderView::Interactive);
                hHeader->resizeSection(i, group->column_width.at(i));
            }
            ui->profilesTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        }
        hHeader->adjustPositions();
        hHeader->blockSignals(false);
    });
}

void MainWindow::refresh_proxy_list(const QList<int>& ids, bool mayNeedReset) {
    refresh_proxy_list_impl(ids, mayNeedReset);
}

void MainWindow::refresh_proxy_list_impl(const QList<int>& ids, bool mayNeedReset) {
    auto currentGroup = Configs::dataManager->groupsRepo->CurrentGroup();
    if (currentGroup == nullptr)
    {
        MW_show_log("Could not find current group!");
        return;
    }
    // refresh data
    refresh_proxy_list_impl_refresh_data(ids, mayNeedReset);
    // now refresh column sizes
    refresh_proxy_list_column_size();
}

void MainWindow::refresh_proxy_list_impl_refresh_data(const QList<int>& ids, bool mayNeedReset) {
    auto currentGroup = Configs::dataManager->groupsRepo->CurrentGroup();
    if (currentGroup == nullptr) return;
    if (!ids.isEmpty()) {
        if (filterProfilesList(ids).isEmpty())
            return;
        for (auto id:ids) profilesTableModel->refreshProfileId(id);
    } else {
        auto profileIDs = filterProfilesList(currentGroup->profiles);
        profilesTableModel->refreshTable(profileIDs, mayNeedReset);
    }
}

// table菜单相关

void MainWindow::on_profilesTableView_doubleClicked(const QModelIndex &index) {
    if (!index.isValid() || !profilesTableModel) return;
    int id = profilesTableModel->data(index, ProfilesTableModel::ProfileIdRole).toInt();
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
    auto dialog = new DialogEditProfile("socks", Configs::dataManager->settingsRepo->current_group, this);
    connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
}

void MainWindow::on_menu_add_from_clipboard_triggered() {
    auto clipboard = QApplication::clipboard()->text();
    Subscription::groupUpdater->AsyncUpdate(clipboard);
}

void MainWindow::on_menu_clone_triggered() {
    auto entIDs = get_now_selected_list();
    if (entIDs.isEmpty()) return;

    auto btn = QMessageBox::question(this, tr("Clone"), tr("Clone %1 item(s)").arg(entIDs.count()));
    if (btn != QMessageBox::Yes) return;

    QStringList sls;
    auto ents = Configs::dataManager->profilesRepo->GetProfileBatch(entIDs);
    for (const auto &ent: ents) {
        sls << ent->outbound->ExportJsonLink();
    }

    Subscription::groupUpdater->AsyncUpdate(sls.join("\n"));
}

void  MainWindow::on_menu_delete_repeat_triggered () {
    QList<std::shared_ptr<Configs::Profile>> out;
    QList<std::shared_ptr<Configs::Profile>> out_del;

    Configs::ProfileFilter::Uniq (Configs::dataManager->profilesRepo->GetProfileBatch(Configs::dataManager->groupsRepo->CurrentGroup()->Profiles()), out,  false );
    Configs::ProfileFilter::OnlyInSrc_ByPointer (Configs::dataManager->profilesRepo->GetProfileBatch(Configs::dataManager->groupsRepo->CurrentGroup()->Profiles()), out, out_del);

    int  remove_display_count =  0 ;
    QString remove_display;
    for  ( const  auto  &ent: out_del) {
        remove_display += ent-> outbound -> DisplayTypeAndName () +  " \n " ;
        if  (++remove_display_count ==  20 ) {
            remove_display +=  " ... " ;
            break ;
        }
    }

    if  (!out_del.empty()  &&
        (Configs::dataManager->settingsRepo->skip_delete_confirmation || QMessageBox::question( this , tr("Confirmation"),tr( "Remove %1 item(s) ?").arg(out_del.length()) + "\n" + remove_display)==QMessageBox::StandardButton::Yes)) {
        QList<int> del_ids;
        for (const auto &ent: out_del) {
            del_ids += ent->id;
        }
        Configs::dataManager->profilesRepo->BatchDeleteProfiles(del_ids);
        refresh_proxy_list({}, true);
    }
}

void MainWindow::on_menu_delete_triggered() {
    auto entIDs = get_now_selected_list();
    if (entIDs.count() == 0) return;
    if (Configs::dataManager->settingsRepo->skip_delete_confirmation || QMessageBox::question(this, tr("Confirmation"), QString(tr("Remove %1 item(s) ?")).arg(entIDs.count()))==QMessageBox::StandardButton::Yes) {
        Configs::dataManager->profilesRepo->BatchDeleteProfiles(entIDs);
        refresh_proxy_list({}, true);
    }
}

void MainWindow::on_menu_reset_traffic_triggered() {
    auto entIDs = get_now_selected_list();
    if (entIDs.count() == 0) return;
    auto ents = Configs::dataManager->profilesRepo->GetProfileBatch(entIDs);
    if (ents.empty()) return;
    for (const auto& ent: ents) {
        ent->ResetTraffic();
        Configs::dataManager->profilesRepo->SaveTraffic(ent);
    }
    if (auto group = Configs::dataManager->groupsRepo->GetGroup(ents.first()->gid); group &&
        group->calculated_column_width.size() > 4) group->calculated_column_width[4] = 0;
    refresh_proxy_list(entIDs);
}

void MainWindow::on_menu_copy_links_triggered() {
    if (ui->masterLogBrowser->hasFocus()) {
        ui->masterLogBrowser->copy();
        return;
    }
    auto entIDs = get_now_selected_list();
    QStringList links;
    auto ents = Configs::dataManager->profilesRepo->GetProfileBatch(entIDs);
    for (const auto &ent: ents) {
        links += ent->outbound->ExportToLink();
    }
    if (links.length() == 0) return;
    QApplication::clipboard()->setText(links.join("\n"));
    MW_show_log(tr("Copied %1 item(s)").arg(links.length()));
}

void MainWindow::on_menu_copy_links_nkr_triggered() {
    auto entIDs = get_now_selected_list();
    QStringList links;
    auto ents = Configs::dataManager->profilesRepo->GetProfileBatch(entIDs);
    for (const auto &ent: ents) {
        links += ent->outbound->ExportJsonLink();
    }
    if (links.length() == 0) return;
    QApplication::clipboard()->setText(links.join("\n"));
    MW_show_log(tr("Copied %1 item(s)").arg(links.length()));
}

void MainWindow::on_menu_export_config_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() != 1) return;
    auto ent = Configs::dataManager->profilesRepo->GetProfile(ents.first());

    auto result = Configs::BuildSingBoxConfig(ent);
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
        result = BuildSingBoxConfig(ent);
        if (!result->error.isEmpty()) {
            MessageBoxWarning("Build config error", result->error);
            return;
        }
        config_core = QJsonObject2QString(result->coreConfig, true);
        QApplication::clipboard()->setText(config_core);
    } else if (msg.clickedButton() == button_2) {
        auto res = Configs::BuildTestConfig({ent});
        if (!res->error.isEmpty()) {
            MessageBoxWarning("Build Test config error", res->error);
            return;
        }
        config_core = QJsonObject2QString(res->coreConfig, true);
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

    auto ent = Configs::dataManager->profilesRepo->GetProfile(ents.first());
    auto link = ent->outbound->ExportToLink();
    auto link_nk = ent->outbound->ExportToLink();
    auto w = new W(link, link_nk);
    w->setWindowTitle(ent->outbound->DisplayTypeAndName());
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
    if (qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland" || qEnvironmentVariable("WAYLAND_DISPLAY").contains("wayland", Qt::CaseInsensitive)) {
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

void MainWindow::parseQrImage(const QPixmap *image)
{
    const QVector<QString> texts = QrDecoder().decode(image->toImage().convertToFormat(QImage::Format_Grayscale8));
    if (texts.isEmpty()) {
        MessageBoxInfo(software_name, tr("QR Code not found"));
    } else {
        for (const QString &text : texts) {
            MW_show_log("QR Code Result:\n" + text);
            Subscription::groupUpdater->AsyncUpdate(text);
        }
    }
}

void MainWindow::on_menu_scan_qr_triggered() {
    hide();
    QThread::sleep(1);

    bool ok = true;
    QPixmap qpx(grabScreen(QGuiApplication::primaryScreen(), ok));

    show();
    if (ok) {
        parseQrImage(&qpx);
    }
    else {
        MessageBoxInfo(software_name, tr("Unable to capture screen"));
    }
}

void MainWindow::on_menu_clear_test_result_triggered() {
    auto entIDs = get_selected_or_group();
    auto ents = Configs::dataManager->profilesRepo->GetProfileBatch(entIDs);
    if (ents.empty()) return;
    for (const auto &ent: ents) {
        ent->ClearTestResults();
    }
    Configs::dataManager->profilesRepo->SaveBatch(ents);
    if (auto group = Configs::dataManager->groupsRepo->GetGroup(ents.first()->gid); group &&
        group->calculated_column_width.size() > 3) group->calculated_column_width[3] = 0;
    refresh_proxy_list();
}

void MainWindow::on_menu_select_all_triggered() {
    if (ui->masterLogBrowser->hasFocus()) {
        ui->masterLogBrowser->selectAll();
        return;
    }
    ui->profilesTableView->selectAll();
}

bool mw_sub_updating = false;

void MainWindow::on_menu_update_subscription_triggered() {
    auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (group->url.isEmpty()) return;
    if (mw_sub_updating) return;
    mw_sub_updating = true;
    Subscription::groupUpdater->AsyncUpdate(group->url, group->id, [&] { mw_sub_updating = false; });
}

void MainWindow::on_menu_remove_unavailable_triggered() {
    clearUnavailableProfiles();
}

void MainWindow::on_menu_remove_invalid_triggered() {
    runOnNewThread([=,this]
    {
        QList<std::shared_ptr<Configs::Profile>> out_del;

     auto currentGroup = Configs::dataManager->groupsRepo->CurrentGroup();
     if (currentGroup == nullptr) return;
     std::atomic counter(0);
     QMutex mu;
     QMutex access;
     int profileSize = currentGroup->Profiles().size();
     mu.lock();
     for (const auto& profileID : currentGroup->Profiles()) {
         auto profile = Configs::dataManager->profilesRepo->GetProfile(profileID);
         parallelCoreCallPool->start([&out_del, profile, &counter, &mu, profileSize, &access]
         {
             if (!IsValid(profile))
             {
                 access.lock();
                 out_del += profile;
                 access.unlock();
             }
             if (++counter == profileSize) mu.unlock();
         });
     }
     mu.lock();
     mu.unlock();

     int remove_display_count = 0;
     QString remove_display;
     for (const auto &ent: out_del) {
         remove_display += ent->outbound->DisplayTypeAndName() + "\n";
         if (++remove_display_count == 20) {
             remove_display += "...";
             break;
         }
     }

     runOnUiThread([=,this]
     {
         if (!out_del.empty() &&
         (Configs::dataManager->settingsRepo->skip_delete_confirmation || QMessageBox::question(this, tr("Confirmation"), tr("Remove %1 Invalid item(s) ?").arg(out_del.length()) + "\n" + remove_display) == QMessageBox::StandardButton::Yes)) {
         QList<int> del_ids;
         for (const auto &ent: out_del) {
             del_ids += ent->id;
         }
         Configs::dataManager->profilesRepo->BatchDeleteProfiles(del_ids);
         refresh_proxy_list({}, true);
     }
     });
    });
}

void MainWindow::on_menu_resolve_selected_triggered() {
    auto profiles = get_now_selected_list();
    if (profiles.isEmpty()) return;

    if (mw_sub_updating) return;
    mw_sub_updating = true;
    auto resolve_count = std::atomic<int>(0);
    Configs::dataManager->settingsRepo->resolve_count = profiles.count();

    auto ents = Configs::dataManager->profilesRepo->GetProfileBatch(profiles);
    for (const auto &profile: ents) {
        profile->outbound->ResolveDomainToIP([=,this] {
            Configs::dataManager->profilesRepo->Save(profile);
            refresh_proxy_list({profile->id});
            if (--Configs::dataManager->settingsRepo->resolve_count != 0) return;
            mw_sub_updating = false;
        });
    }
}

void MainWindow::on_menu_resolve_domain_triggered() {
    auto currGroup = Configs::dataManager->groupsRepo->GetGroup(Configs::dataManager->settingsRepo->current_group);
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
    Configs::dataManager->settingsRepo->resolve_count = profiles.count();

    for (const auto id: profiles) {
        auto profile = Configs::dataManager->profilesRepo->GetProfile(id);
        profile->outbound->ResolveDomainToIP([=,this] {
            Configs::dataManager->profilesRepo->Save(profile);
            refresh_proxy_list({profile->id});
            if (--Configs::dataManager->settingsRepo->resolve_count != 0) return;
            mw_sub_updating = false;
        });
    }
}

void MainWindow::on_profilesTableView_customContextMenuRequested(const QPoint &pos) {
    ui->menu_server->popup(ui->profilesTableView->viewport()->mapToGlobal(pos));
}

QList<int> MainWindow::get_now_selected_list() {
    QList<int> list;
    if (!profilesTableModel) return list;
    QModelIndexList indices = ui->profilesTableView->selectionModel()->selectedRows(0);
    for (const QModelIndex &idx : indices) {
        int id = profilesTableModel->data(idx, ProfilesTableModel::ProfileIdRole).toInt();
        list << id;
    }
    return list;
}

QList<int> MainWindow::get_selected_or_group() {
    auto selected_or_group = ui->menu_server->property("selected_or_group").toInt();
    QList<int> profileIDs;
    if (selected_or_group > 0) {
        profileIDs = get_now_selected_list();
        if (profileIDs.isEmpty() && selected_or_group == 2) profileIDs = Configs::dataManager->groupsRepo->CurrentGroup()->Profiles();
    } else {
        profileIDs = Configs::dataManager->groupsRepo->CurrentGroup()->Profiles();
    }
    return profileIDs;
}

void MainWindow::clearUnavailableProfiles(bool confirm, QList<int> profileIDs) {
    QList<int> del_ids;
    int remove_display_count = 0;
    QString remove_display;

    auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (!group) return;

    if (profileIDs.isEmpty()) profileIDs = group->Profiles();

    auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(profileIDs);
    for (const auto &profile: profiles) {
        if (profile->latency < 0) {
            del_ids += profile->id;
            if (++remove_display_count == 20) {
                remove_display += "...";
            }else if (remove_display_count < 20) remove_display += profile->outbound->DisplayTypeAndName() + "\n";
        }
    }

    auto clearFunc = [=, this] {
        Configs::dataManager->profilesRepo->BatchDeleteProfiles(del_ids);
        refresh_proxy_list({}, true);
    };

    if (!del_ids.isEmpty()) {
        if (confirm && !Configs::dataManager->settingsRepo->skip_delete_confirmation) {
            if (QMessageBox::question(this, tr("Confirmation"), tr("Remove %1 Unavailable item(s) ?").arg(del_ids.length()) + "\n" + remove_display) == QMessageBox::StandardButton::Yes) {
                clearFunc();
            }
        } else {
            clearFunc();
        }
    }
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

void MainWindow::append_log(const QString &log) {
    if (log.size() > 20000) {
        append_log(QString("TRUNCATED LONG LOG: ") + log.first(1000) + "...");
        return;
    }
    QMutexLocker locker(&logMutex);
    if (logQueue.size() > 1000) {
        // log is overloaded, just discard it
        return;
    }
    logQueue.enqueue(log);
    if (logQueue.size() == 1) logWaiter.wakeOne();
}

void MainWindow::log_process_loop() {
    while (true) {
        logMutex.lock();
        while (logQueue.isEmpty()) {
            logWaiter.wait(&logMutex);
        }
        auto logLines = logQueue.dequeue().split("\n");

        QString batchToPrint;
        for (const auto& logLine : logLines) {
            if (should_print_log(logLine)) {
                batchToPrint += logLine + "\n";
            }
        }
        logMutex.unlock();

        if (!batchToPrint.isEmpty()) {
            runOnUiThread([=, this] {
               FastAppendTextDocument(batchToPrint.trimmed(), qvLogDocument);
            });
        }
    }
}

bool MainWindow::should_print_log(const QString &log) {
    if (log.trimmed().isEmpty()) return false;
    bool result = true;
    if (Configs::dataManager->settingsRepo->log_enable_include) {
        result = false;
        for (const auto& includeKeyword : includeKeywords) {
            if (log.contains(includeKeyword)) {
                result = true;
                break;
            }
        }
        if (!includeCombined.pattern().isEmpty() && includeCombined.match(log).hasMatch()) {
            result = true;
        }
    }
    if (result && Configs::dataManager->settingsRepo->log_enable_exclude) {
        for (const auto& excludeKeyword : excludeKeywords) {
            if (log.contains(excludeKeyword)) {
                result = false;
                break;
            }
        }
        if (!excludeCombined.pattern().isEmpty() && excludeCombined.match(log).hasMatch()) {
            result = false;
        }
    }
    return result;
}

void MainWindow::on_masterLogBrowser_customContextMenuRequested(const QPoint &pos) {
    QMenu *menu = ui->masterLogBrowser->createStandardContextMenu();

    auto sep = new QAction(this);
    sep->setSeparator(true);
    menu->addAction(sep);

    auto action_clear = new QAction(this);
    action_clear->setText(tr("Clear"));
    connect(action_clear, &QAction::triggered, this, [=,this] {
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
        connect(addAction, &QAction::triggered, this, [=,this]{
            auto ent = Configs::dataManager->groupsRepo->NewGroup();
            auto dialog = new DialogEditGroup(ent, this);
            int ret = dialog->exec();
            dialog->deleteLater();

            if (ret == QDialog::Accepted) {
                Configs::dataManager->groupsRepo->AddGroup(ent);
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
    connect(addAction, &QAction::triggered, this, [=,this]{
        auto ent = Configs::GroupsRepo::NewGroup();
        auto dialog = new DialogEditGroup(ent, this);
        int ret = dialog->exec();
        dialog->deleteLater();

        if (ret == QDialog::Accepted) {
            Configs::dataManager->groupsRepo->AddGroup(ent);
            MW_dialog_message(Dialog_DialogManageGroups, "refresh-1");
        }
    });
    connect(deleteAction, &QAction::triggered, this, [=,this] {
        auto id = Configs::dataManager->groupsRepo->GetGroupsTabOrder()[clickedIndex];
        if (QMessageBox::question(this, tr("Confirmation"), tr("Remove %1?").arg(Configs::dataManager->groupsRepo->GetGroup(id)->name)) ==
            QMessageBox::StandardButton::Yes) {
            if (running != nullptr) {
                if (running->gid == id) profile_stop(false, true, false);
            }
            Configs::dataManager->groupsRepo->DeleteGroup(id);
            MW_dialog_message(Dialog_DialogManageGroups, "refresh-1");
        }
    });
    connect(editAction, &QAction::triggered, this, [=,this]{
        auto id = Configs::dataManager->groupsRepo->GetGroupsTabOrder()[clickedIndex];
        auto ent = Configs::dataManager->groupsRepo->GetGroup(id);
        auto dialog = new DialogEditGroup(ent, this);
        connect(dialog, &QDialog::finished, this, [=,this] {
            if (dialog->result() == QDialog::Accepted) {
                Configs::dataManager->groupsRepo->Save(ent);
                MW_dialog_message(Dialog_DialogManageGroups, "refresh" + Int2String(ent->id));
            }
            dialog->deleteLater();
        });
        dialog->show();
    });
    menu->addAction(ui->actionRefresh_Column_Widths);
    menu->addAction(addAction);
    menu->addAction(editAction);
    auto group = Configs::dataManager->groupsRepo->GetGroup(Configs::dataManager->settingsRepo->current_group);
    if (Configs::dataManager->groupsRepo->GetAllGroupIds().size() > 1) menu->addAction(deleteAction);
    if (!group->Profiles().empty()) {
        menu->addAction(ui->actionUrl_Test_Group);
        menu->addAction(ui->actionSpeedtest_Group);
        menu->addAction(ui->actionResolve_Out_IP);
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
    if (unregister || Configs::dataManager->settingsRepo->prepare_exit) return;

    QStringList regstr{
        Configs::dataManager->settingsRepo->hotkey_mainwindow,
        Configs::dataManager->settingsRepo->hotkey_group,
        Configs::dataManager->settingsRepo->hotkey_route,
        Configs::dataManager->settingsRepo->hotkey_system_proxy_menu,
        Configs::dataManager->settingsRepo->hotkey_toggle_system_proxy,
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
            connect(hk.get(), &QHotkey::activated, this, [=,this] { HotkeyEvent(key); });
        } else {
            hk->deleteLater();
        }
    }
}

void MainWindow::RegisterHiddenMenuShortcuts(bool unregister) {
    for (const auto s : hiddenMenuShortcuts) s->deleteLater();
    hiddenMenuShortcuts.clear();

    if (unregister) return;

    for (const auto &action: ui->menuHidden_menu->actions()) {
        if (!action->shortcut().toString().isEmpty())
        {
            hiddenMenuShortcuts.append(new QShortcut(action->shortcut(), this, [=,this](){
                action->trigger();
            }));
        }
    }
}

void MainWindow::setActionsData()
{
    // assign ids to menu actions so that we can save and restore them
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
        // Only apply saved shortcut if user has defined one; preserve default UI shortcuts otherwise
        if (mp.count(action->data().toString()) > 0) {
            action->setShortcut(mp[action->data().toString()]);
        }
    }

    RegisterHiddenMenuShortcuts();
}


void MainWindow::HotkeyEvent(const QString &key) {
    if (key.isEmpty()) return;
    runOnUiThread([=,this] {
        if (key == Configs::dataManager->settingsRepo->hotkey_mainwindow) {
            tray->activated(QSystemTrayIcon::ActivationReason::Trigger);
        } else if (key == Configs::dataManager->settingsRepo->hotkey_group) {
            on_menu_manage_groups_triggered();
        } else if (key == Configs::dataManager->settingsRepo->hotkey_route) {
            on_menu_routing_settings_triggered();
        } else if (key == Configs::dataManager->settingsRepo->hotkey_system_proxy_menu) {
            ui->menu_spmode->popup(QCursor::pos());
        } else if (key == Configs::dataManager->settingsRepo->hotkey_toggle_system_proxy) {
            toggle_system_proxy();
        }
    });
}

bool MainWindow::StopVPNProcess() {
    runOnThread([=, this]
    {
        core_process->Kill();
    }, DS_cores, true);

    return true;
}

bool isNewer(QString assetName) {
    if (QString(NKR_VERSION).isEmpty()) return false;
    assetName = assetName.mid(7); // take out Throne-
    QString version;
    auto spl = assetName.split('-');
    version += spl[0]; // version: 1.2.3
    if (spl[1].contains("beta") || spl[1].contains("alpha") || spl[1].contains("rc")) version += "."+spl[1]; // .beta.13
    auto parts = version.split("."); // [1,2,3,beta,13]
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
		MW_show_log("There are no updates. You have the latest version - " + QString(NKR_VERSION));
        return false;
    }
    return false;
}

void MainWindow::CheckUpdate() {
    QString search;
#ifdef Q_OS_WIN
#  ifdef Q_PROCESSOR_ARM_64
    search = "windows-arm64";
#  else
#    ifdef Q_OS_WIN64
        if (WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_10_1809))
            search = "windows64";
        else
	        search = "windowslegacy64";
#    else
	    search = "windows32";
#    endif
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
        runOnUiThread([=,this] {
            MessageBoxWarning(QObject::tr("Update"), QObject::tr("Not official support platform"));
        });
        return;
    }

    auto resp = NetworkRequestHelper::HttpGet("https://api.github.com/repos/throneproj/Throne/releases");
    if (!resp.error.isEmpty()) {
        runOnUiThread([=,this] {
            MessageBoxWarning(QObject::tr("Update"), QObject::tr("Requesting update error: %1").arg(resp.error + "\n" + resp.data));
        });
        return;
    }

    QString assets_name, release_download_url, release_url, release_note, note_pre_release;
    bool exitFlag = false;
    QJsonArray array = QString2QJsonArray(resp.data);
    for (const QJsonValue value : array) {
        QJsonObject release = value.toObject();
        if (release["prerelease"].toBool() && !Configs::dataManager->settingsRepo->allow_beta_update) continue;
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
        runOnUiThread([=,this] {
            MessageBoxInfo(QObject::tr("Update"), QObject::tr("No update"));
        });
        return;
    }

    runOnUiThread([=,this] {
        auto allow_updater = !Configs::dataManager->settingsRepo->flag_use_appdata;
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
            runOnNewThread([=,this] {
                if (!mu_download_update.tryLock()) {
                    runOnUiThread([=,this](){
                        MessageBoxWarning(tr("Cannot start"), tr("Last download request has not finished yet"));
                    });
                    return;
                }
                QString errors;
                if (!release_download_url.isEmpty()) {
                    auto res = NetworkRequestHelper::DownloadAsset(release_download_url, "Throne.zip");
                    if (!res.isEmpty()) {
                        errors += res;
                    }
                }
                mu_download_update.unlock();
                runOnUiThread([=,this] {
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
