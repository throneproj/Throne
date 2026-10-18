#pragma once

#include <QMainWindow>
#include <include/global/HTTPRequestHelper.hpp>
#ifndef Q_MOC_RUN
#include <core/gen/libcore.pb.h>
#endif

#include "include/global/Configs.hpp"
#include "include/stats/connections/connectionLister.hpp"
#include "include/stats/traffic/TrafficLine.hpp"
#include "include/database/entities/Profile.h"
#ifdef Q_OS_LINUX
#include <QtDBus>
#endif

#include <deque>
#include <optional>
#include <QKeyEvent>
#include "include/ui/widget/TrayIcon.hpp"
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QQueue>
#include <QWaitCondition>
#include <QProcess>
#include <QTextDocument>
#include <QShortcut>
#include <QKeySequence>
#include <QSet>
#include <QHash>
#include <QIcon>
#include <QPixmap>
#include <QToolButton>
#include <QCheckBox>
#include <QSemaphore>
#include <QMutex>
#include <QThreadPool>

#include "include/database/entities/GroupSort.h"
#include "include/engine/EnginePolicy.h"
#include "include/engine/EngineTypes.h"
#include "include/engine/tests/TestObserver.h"
#include "include/ui/setting/Icon.hpp"
#include "include/ui/utils/DataViewHtmlGenerator.h"
#include "include/ui/utils/GuiUtils.hpp"
#include "include/ui/utils/ProfilesFilterProxyModel.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include "ui_mainwindow.h"

namespace Configs {
    enum simpleAction : int;
}

namespace Engine {
    class ConnectionManager;
    class CoreSupervisor;
    class ExitController;
    class TestRunner;
    class VpnAuthController;
}

class QMessageBox;
class TrayProfileSelector;
class TrayOtpCodes;
class GlobalHotkeys;
class DialogVpnAuth;
class DialogScanner;
class DialogIpLists;

namespace Qv2ray::ui { class SyntaxHighlighter; }

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

enum class RefreshAnchor {
    KeepPlace,
    Removal,
};

using ExitReason = Engine::ExitReason;

class MainWindow : public QMainWindow, public Engine::EnginePolicy, public Engine::TestObserver {
    Q_OBJECT

public:
    using ConnectionState = Engine::ConnectionState;
    using StartOutcome = Engine::StartOutcome;
    using StartRequest = Engine::StartRequest;
    using ModeChange = Engine::ModeChange;

    explicit MainWindow(QWidget *parent = nullptr);

    ~MainWindow() override;

    void refresh_proxy_list(const QList<int> &ids = {}, bool mayNeedReset = false,
                            RefreshAnchor anchor = RefreshAnchor::KeepPlace);

    void show_group(int gid);

    void show_group_tab_menu(const QPoint &tabBarPos);

    void refresh_groups();

    void updateTabToolTip(int gid);

    void refresh_status(const QString &traffic_update = "");

    // Lines of the config about to run; must arrive before its first add_traffic_point.
    void set_traffic_lines(const QList<Stats::TrafficLine> &lines);

    // values: [down, up] per line of the last set_traffic_lines, in its order.
    void add_traffic_point(qint64 monoMs, bool coreRunning, const QVector<double> &values,
                           double proxyDown, double proxyUp, double directDown, double directUp);

    // User start actions only: in select mode the profile becomes the pick. Engine restarts call the ConnectionManager directly.
    void profile_start(int _id = -1);

    void profile_stop(bool crash = false, bool block = false, bool manual = false, bool interactive = true);

    int get_profile_to_start();

    // Safe from any thread.
    std::shared_ptr<Configs::Profile> runningSnapshot() const;

    void toggle_system_proxy();

    void start_select_mode(QObject *context, const std::function<void(int)> &callback);

    // Returns a line per global hotkey that could not be registered.
    QStringList RegisterHotkey(bool unregister);

    bool IsGlobalHotkeySupported() const;

    bool StopVPNProcess();

    // Whole poll snapshot in the lister's order, never a delta. UI thread only.
    void UpdateConnectionList(const QList<Stats::ConnectionMetadata>& connections);

    void UpdateDataView(bool force = false);

    void refresh_auto_selector_view();

    class DialogAutoSelector *m_autoSelectorDialog = nullptr;

    void setDownloadReport(const DownloadProgressReport& report, bool show);

    void showIpListsDialog(int selectListId = -1);

    void showScannerDialog();

    void refreshScannerDataView(bool force = false);

signals:

    void profile_selected(int id);

public slots:

    void on_commitDataRequest();

    void on_menu_exit_triggered();

private slots:

    void on_masterLogBrowser_customContextMenuRequested(const QPoint &pos);

    void on_menu_basic_settings_triggered();

    void on_menu_routing_settings_triggered();

    void on_menu_vpn_settings_triggered();

    void on_menu_preset_settings_triggered();

    void on_menu_otp_manager_triggered();

    void on_menu_scanner_triggered();

    void on_menu_integration_settings_triggered();

    void on_menu_add_from_input_triggered();

    void on_menu_add_from_clipboard_triggered();

    void on_menu_clone_triggered();

    void on_menu_delete_repeat_triggered();

    void on_menu_delete_triggered();

    void on_menu_reset_traffic_triggered();

    void on_menu_copy_links_triggered();

    void on_menu_copy_links_nkr_triggered();

    void on_menu_export_config_triggered();

    void display_qr_link(bool nkrFormat = false);

    void on_menu_scan_qr_triggered();

    void on_menu_clear_test_result_triggered();

    void on_menu_manage_groups_triggered();

    void on_menu_select_all_triggered();

    void on_menu_remove_unavailable_triggered();

    void on_menu_remove_invalid_triggered();

    void on_menu_remove_insecure_triggered();

    void on_menu_resolve_selected_triggered();

    void on_menu_resolve_domain_triggered();

    void on_menu_update_subscription_triggered();

    void on_profilesTableView_doubleClicked(const QModelIndex &index);

    void on_profilesTableView_customContextMenuRequested(const QPoint &pos);

    void on_tabWidget_currentChanged(int index);

    void on_tabWidget_customContextMenuRequested(const QPoint& p);

private:
    Ui::MainWindow *ui;
    QElapsedTimer sinceWindowDeactivated;
    ProfilesTableModel *profilesTableModel = nullptr;

    ProfilesFilterProxyModel *profilesFilterModel = nullptr;
    TrayIcon *tray = nullptr;
    QMenu *trayMenu = nullptr;
    QAction *trayConnectAction = nullptr;
    QPointer<TrayProfileSelector> traySelector;
    void openTraySelector(bool routing);
    QPointer<TrayOtpCodes> trayOtpCodes;
    void openTrayOtpCodes();
    QThreadPool *parallelCoreCallPool = new QThreadPool(this);
    // Engine::host()'s objects, in mainwindow_engine_control.cpp.
    Engine::ConnectionManager *connection() const;
    Engine::CoreSupervisor *supervisor() const;
    Engine::TestRunner *tests() const;
    Engine::VpnAuthController *vpnAuth() const;
    Engine::ExitController *exitController() const;
    QTextDocument *qvLogDocument = new QTextDocument(this);
    QString title_error;
    std::optional<Icon::TrayIconStatus> icon_status;
    QString traffic_update_cache;
    qint64 last_test_time = 0;
    bool select_mode = false;
    // Esc leaves select mode; enabled only meanwhile, so the profile table keeps Esc for clearing its selection.
    QShortcut *shortcut_select_cancel = nullptr;
    void finish_select_mode(int profileId);
    ExitReason exit_reason = ExitReason::None;
    // Set by request_unattended_exit: no dialog during the teardown, and a restart stays in the tray when the window was hidden.
    bool m_exitUnattended = false;
    bool m_exitKeepTray = false;
    QMutex mu_download_update;
    QMutex mu_download_dashboard;
    class ConnectionsTreeModel *connectionsModel = nullptr;
    class ConnectionsTreeFilterProxyModel *connectionsFilterModel = nullptr;
    class ConnectionsFilterHeader *connectionFilterHeader = nullptr;
    QHash<QString, bool> m_processExpanded; // per-process choices; the rest follow m_processesExpandedByDefault
    bool m_processesExpandedByDefault = false;
    QTimer *connectionFilterDebounce = nullptr;
    QToolButton *connectionExpandButton = nullptr;
    QToolButton *connectionCloseAllButton = nullptr;
    QIcon connectionCloseIcon;
    QIcon connectionExpandIcon;
    QIcon connectionCollapseIcon;
    int toolTipID;
    class LiveChart *trafficChart = nullptr;
    QList<Stats::TrafficLine> m_trafficLines;
    QTimer *m_trafficGraphSaveDebounce = nullptr;
    QAction *m_trafficStatsChartAction = nullptr;
    using LiveRates = Engine::LiveRates;
    class RuntimeStatsWidget *runtimeStatsWidget = nullptr;
    std::atomic<qint64> lastUpdatedMs = QDateTime::currentMSecsSinceEpoch();
    DataViewHtmlGenerator dataViewHtmlGenerator_;

    QList<QShortcut*> hiddenMenuShortcuts;
    GlobalHotkeys *globalHotkeys = nullptr;

    QString addressFilterString;
    QString nameFilterString;
    QString typeFilterString;
    QString countryFilterString;

    QTimer *m_filterRefreshDebounce = nullptr;

    bool m_profilesTableHadFocus = false;
    int m_profilesScrollValue = 0;

    QStringList includeKeywords;
    QStringList excludeKeywords;
    QRegularExpression includeCombined;
    QRegularExpression excludeCombined;
    QMutex logMutex;
    QQueue<QString> logQueue;
    QWaitCondition logWaiter;
    Qv2ray::ui::SyntaxHighlighter *logHighlighter = nullptr;

    QMutex logPendingMutex;
    QString logPendingText;
    bool logFlushScheduled = false;

    // UI-thread view state. m_logLines is what the view renders (capped at max_log_line);
    // while the user is scrolled up, arrivals wait in m_logHeld so the view stays frozen.
    struct LogLine {
        QString text;
        bool visible = true;
    };
    std::deque<LogLine> m_logLines;
    std::deque<QString> m_logHeld;
    bool m_logFollow = true;
    QRegularExpression m_logSearch;
    QTimer *m_logSearchDebounce = nullptr;
    QToolButton *logFilterButton = nullptr;
    QToolButton *logJumpLatestButton = nullptr;

    struct LogFilter {
        bool enableInclude = false;
        bool enableExclude = false;
        QStringList includeKeywords;
        QStringList excludeKeywords;
        QRegularExpression includeCombined;
        QRegularExpression excludeCombined;
    };

    void append_log(const QString &log);

    void log_process_loop();

    // UI thread only.
    void flush_log_batch();

    void setupLogView();

    void releaseHeldLogs();

    // Caps both queues at max_log_line; returns how many dropped lines were visible (rendered blocks).
    int trimLogLines();

    void rebuildLogView();

    void applyLogSearch();

    void setLogFilterVisible(bool visible);

    void updateLogStatus();

    bool should_print_log(const QString &log, const LogFilter &filter);

    void updateLogFilterFields();

    void setLogHighlighter(bool darkMode);

    void applyProfileFilters();

    QList<int> get_now_selected_list();
    void refresh_startstop_button();

    // View slots of ConnectionManager and VpnAuthController, in mainwindow_view.cpp.
    void addRunMarker(qint64 monoMs, Engine::RunMarker kind, const QString &name);

    void showRestartReasons();

    void showVpnEndpointStatus(const QString &summary, const QString &detail, bool problem);

    static QString restartReasonText(Engine::RestartReason reason);

    QList<int> get_selected_or_group();

    void saveProfileFocusState();

    void restoreProfileFocusState(RefreshAnchor anchor);

    void selectProfileRows(const QList<int> &rows);

    void focusProfilesTable(bool selectFirst);

    void clearUnavailableProfiles(bool confirm = true, QList<int> profileIDs = {});

    // Returns how many profiles it cleared.
    int clear_test_results(const QList<int> &profileIds);

    // Geometry, size and splitter into the settings, unsaved.
    void saveWindowState();

    void request_unattended_exit(ExitReason reason);

    // Runs after the user confirmed; title heads the warning when Engine::GroupService refuses.
    void delete_group(int gid, const QString &title);

    void dialog_message_impl(MwMessage cmd, const QStringList &args);

    void handle_deeplink_impl(const QString &url);

    void handle_addsub(const QString &url, const QString &name);

    void handle_import_route(const QString &url);

    void handle_add_remote_routes(const QString &url);

    void import_or_handle_deeplink(const QString &text);

    void import_text(const QString &text);

    void refresh_proxy_list_column_size();

    void refresh_sort_indicator();

    // Keeps the current group sorted by `action` from now on.
    void sortCurrentGroup(GroupSortAction action);

    void stopSortingCurrentGroup();

    void refresh_proxy_list_impl(const QList<int> &ids = {}, bool mayNeedReset = false);

    void refresh_proxy_list_impl_refresh_data(const QList<int>& ids = {}, bool mayNeedReset = false);

    void parseQrImage(const QPixmap *image);

    void importFromFiles(const QStringList &paths);

    void trayClickEvent();

    void keyPressEvent(QKeyEvent *event) override;

    void closeEvent(QCloseEvent *event) override;

    void changeEvent(QEvent *event) override;

    void showEvent(QShowEvent *event) override;

    void hideEvent(QHideEvent *event) override;

    void syncConnectionViewState();

    void dragEnterEvent(QDragEnterEvent *event);

    void dropEvent(QDropEvent* event) override;

    void applyLogBrowserFont();

    void applyTopBarMetrics();
    bool usesTightLabels() const;

    QSize designMinimumSize;

    QTimer *m_proxyListRefreshDebounce = nullptr;
    void scheduleProxyListRefresh();

    bool m_adjustingColumns = false;

    void HotkeyEvent(const QString &id);

    void toggle_connection();

    void toggle_tun();

    void RegisterHiddenMenuShortcuts(bool unregister = false);
    void registerMenuShortcuts(QMenu *menu, QSet<QKeySequence> &claimed);
    void collectMenuShortcuts(QMenu *menu, QSet<QKeySequence> &out);

    void setActionsData();

    QList<QAction*> getActionsForShortcut();

    void loadShortcuts();

    void url_test_current();

    void show_startstop_menu();

    void confirm_disable_kill_switch();

    QPointer<QMessageBox> m_killSwitchDialog;

    QPointer<DialogVpnAuth> m_vpnAuthDialog;
    QPointer<DialogScanner> m_scannerDialog;
    QPointer<DialogIpLists> m_ipListsDialog;
    QHash<int, QString> m_scannerNames;
    bool m_scannerPanelShown = false;

    void CheckUpdate();

    void OpenDashboard();

    void setupConnectionList();

    void setupTrafficGraph();

    void refresh_traffic_graph_settings();

    void setupConnectionSortMenu();

    void onConnectionContextMenu(const QPoint &pos);

    QString routeRuleAppendBlocker() const;

    enum class RuleToggle { Failed, Added, Moved, Removed };

    // Adds rawRule to the action's simple rules of the current profile, or takes it out when it is already there.
    RuleToggle toggleRuleInCurrentRoute(const QString &rawRule, Configs::simpleAction action);

    void setupConnectionFilter();

    void restoreConnectionSort();

    void applyConnectionSort(Stats::ConnectionSort sort);

    void applyConnectionFilters();

    void syncConnectionSourceColumn();

    void syncConnectionExpansion();

    void setConnectionGroupsExpanded(bool expanded);

    bool connectionGroupsExpanded() const;

    void syncConnectionExpandButton();

    void closeConnections(const QStringList &ids);

    QStringList listedConnectionIds() const;

    void refreshConnectionIcons();

    // EnginePolicy, in mainwindow_policy.cpp; reached through EngineHost::policy().
    void geoAssetMissingCategory(const QString &contextName, const QString &needed, const QString &whichFile) override;

    bool confirmGeoAssetDownload(const QString &contextName) override;

    void geoAssetDownloadFinished(const QString &error) override;

    void killSwitchProblem() override;

    void killSwitchProblemResolved() override;

    void extraCoreNotPermitted() override;

    void startFailed(const StartRequest &request, StartOutcome outcome, const QString &title, const QString &error) override;

    void strictRouteUnavailable(const QString &error) override;

    bool offerCoreReset(const QString &error) override;

    void stopFailed(const QString &error) override;

    Engine::SlowOperation slowOperation(Engine::SlowOperationKind kind, int delayMs) override;

    bool requestCorePrivileges() override;

    void systemProxyNeedsMixedInbound() override;

    bool vpnPromptOpen() const override;

    void vpnChallenge(const VpnAuthChallenge &challenge) override;

    void vpnCredentialsRejected(int profileId, const VpnAuthChallenge &challenge,
                                const std::function<void(const QString &username, const QString &password)> &retryWith) override;

    void vpnChallengesCleared() override;

    void settingsNeedTunRestart() override;

    bool confirmRestartProgram() override;

    void testSessionBusy(const QString &text) override;

    void exitProgram(ExitReason reason, bool unattended) override;

    // TestObserver, in mainwindow_view.cpp; synchronous, so a panel seed never lands after the next session's clear.
    void testPanelSeeded(Engine::TestKind kind, int totalProfiles) override;

    void testProgressed() override;

    void testPanelsCleared() override;

    void testViewChanged(bool force) override;

    void speedTestProgress(const QString &profileName, const libcore::SpeedTestResult &result) override;

    void testedProfilesChanged(const QList<int> &profileIds) override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

inline MainWindow *GetMainWindow() {
    return qobject_cast<MainWindow *>(mainwindow);
}

MainWindow *UI_InitMainWindow();

#ifdef Q_OS_LINUX
class OrgFreedesktopPortalRequestInterface : public QDBusAbstractInterface
{
    Q_OBJECT
public:
    OrgFreedesktopPortalRequestInterface(const QString& service,
                                         const QString& path,
                                         const QDBusConnection& connection,
                                         QObject* parent = nullptr);

    ~OrgFreedesktopPortalRequestInterface();

public Q_SLOTS:
    inline QDBusPendingReply<> Close()
    {
        QList<QVariant> argumentList;
        return asyncCallWithArgumentList(QStringLiteral("Close"), argumentList);
    }

Q_SIGNALS:
    void Response(uint response, QVariantMap results);
};

namespace org {
namespace freedesktop {
namespace portal {
typedef ::OrgFreedesktopPortalRequestInterface Request;
}
}
}
#endif
