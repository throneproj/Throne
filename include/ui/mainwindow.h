#pragma once

#include <QMainWindow>
#include <include/global/HTTPRequestHelper.hpp>
#ifndef Q_MOC_RUN
#include <core/server/gen/libcore.pb.h>
#endif

#include "include/global/Configs.hpp"
#include "include/stats/connections/connectionLister.hpp"
#include "3rdparty/qv2ray/v2/ui/widgets/speedchart/SpeedWidget.hpp"
#include "include/database/entities/Profile.h"
#ifdef Q_OS_LINUX
#include <QtDBus>
#endif

#ifndef MW_INTERFACE

#include <QKeyEvent>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QQueue>
#include <QWaitCondition>
#include <QProcess>
#include <QTextDocument>
#include <QShortcut>
#include <QKeySequence>
#include <QSet>
#include <QCheckBox>
#include <QSemaphore>
#include <QMutex>
#include <QThreadPool>
#include <QLocalServer>
#include <QLocalSocket>

#include "group/GroupSort.hpp"
#include "include/global/GuiUtils.hpp"
#include "include/ui/utils/DataViewHtmlGenerator.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include "ui_mainwindow.h"

#endif

namespace Configs_sys {
    class CoreProcess;
}

class StayOpenMenu;

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    ~MainWindow() override;

    void prepare_exit();

    void refresh_proxy_list(const QList<int> &ids = {}, bool mayNeedReset = false);

    void show_group(int gid);

    void refresh_groups();

    void refresh_status(const QString &traffic_update = "");

    void update_traffic_graph(int proxyDl, int proxyUp, int directDl, int directUp);

    void profile_start(int _id = -1);

    void profile_stop(bool crash = false, bool block = false, bool manual = false);

    int get_profile_to_start();

    void set_spmode_system_proxy(bool enable, bool save = true);

    void toggle_system_proxy();

    void set_spmode_vpn(bool enable, bool save = true);

    bool get_elevated_permissions(int reason = 3);

    void start_select_mode(QObject *context, const std::function<void(int)> &callback);

    void RegisterHotkey(bool unregister);

    bool StopVPNProcess();

    void UpdateConnectionList(const QMap<QString, Stats::ConnectionMetadata>& toUpdate, const QMap<QString, Stats::ConnectionMetadata>& toAdd);

    void UpdateConnectionListWithRecreate(const QList<Stats::ConnectionMetadata>& connections);

    void UpdateDataView(bool force = false);

    void setDownloadReport(const DownloadProgressReport& report, bool show);

signals:

    void profile_selected(int id);

public slots:

    void on_commitDataRequest();

    void on_menu_exit_triggered();

#ifndef MW_INTERFACE

private slots:

    void on_masterLogBrowser_customContextMenuRequested(const QPoint &pos);

    void on_menu_basic_settings_triggered();

    void on_menu_routing_settings_triggered();

    void on_menu_vpn_settings_triggered();

    void on_menu_hotkey_settings_triggered();

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

    void on_menu_resolve_selected_triggered();

    void on_menu_resolve_domain_triggered();

    void on_menu_update_subscription_triggered();

    void on_profilesTableView_doubleClicked(const QModelIndex &index);

    void on_profilesTableView_customContextMenuRequested(const QPoint &pos);

    void on_tabWidget_currentChanged(int index);

    void on_tabWidget_customContextMenuRequested(const QPoint& p);

private:
    Ui::MainWindow *ui;
    ProfilesTableModel *profilesTableModel = nullptr;
    QSystemTrayIcon *tray;
    QMenu *trayMenu = nullptr;    // tray context menu (parent of trayServerMenu)
    StayOpenMenu *trayServerMenu = nullptr;
    int trayServerPage = 0;       // current profile page within the open group
    int trayServerGroupId = -1;   // -1 = showing the group list; else the group whose profiles are shown
    // Rebuilds the tray "Select Server" menu in place from trayServerGroupId/trayServerPage
    // (click-to-navigate, paginated). Non-macOS only; macOS uses nested submenus.
    void rebuildTrayServerMenu();
    // Keeps the (already visible) tray server menu within the screen's available
    // area after an in-place rebuild, so a taller page can't spill its bottom
    // items off-screen / under the taskbar where they aren't clickable.
    void fitTrayServerMenuOnScreen();
    QShortcut *shortcut_esc = new QShortcut(QKeySequence::Cancel, this);
    //
    QThreadPool *parallelCoreCallPool = new QThreadPool(this);
    std::atomic<bool> stopSpeedtest = false;
    QMutex speedtestRunning;
    std::atomic<bool> currentUnderTest = false;
    // Speed-test byte accounting. Tests bypass the clash tracker (they dial the
    // outbound directly), so their traffic is counted only here: the core reports
    // each test's cumulative bytes, and we diff against the last reported value
    // per outbound tag to credit the delta. Guarded so the live micro-poll and
    // the final reconciliation pass don't race.
    QMutex speedtestCreditMu_;
    QHash<QString, QPair<qint64, qint64>> speedtestCredited_;
    //
    Configs_sys::CoreProcess *core_process = nullptr;
    QMutex coreProcessMutex; // serializes core_process init (DS_cores) vs IPC newConnection (UI)
    QLocalServer *core_server = nullptr;
    bool rpc_started = false;
    QMutex defaultClientMutex;
    qint64 vpn_pid = 0;
    //
    QTextDocument *qvLogDocument = new QTextDocument(this);
    //
    QString title_error;
    int icon_status = -1;
    std::shared_ptr<Configs::Profile> running;
    int last_running_profile_id = -1;
    // True from the moment a profile start is kicked off until it succeeds or
    // fails; drives the start/stop button's transient "Connecting" state.
    bool m_profileConnecting = false;
    // True while a profile stop is in progress; drives the "Disconnecting" state.
    bool m_profileDisconnecting = false;
    QString traffic_update_cache;
    qint64 last_test_time = 0;
    //
    int proxy_last_order = -1;
    bool select_mode = false;
    QMutex mu_starting;
    QMutex mu_stopping;
    QMutex mu_exit;
    int exit_reason = 0;
    //
    QMutex mu_download_update;
    //
    QMutex connectionListMu;
    //
    int toolTipID;
    //
    SpeedWidget *speedChartWidget;
    //
    // for data view
    QDateTime lastUpdated = QDateTime::currentDateTime();
    DataViewHtmlGenerator dataViewHtmlGenerator_;

    // shortcuts
    QList<QShortcut*> hiddenMenuShortcuts;

    QStringList remoteRouteProfiles;
    QMutex mu_remoteRouteProfiles;

    // search
    bool searchEnabled = false;
    QString addressFilterString;
    QString nameFilterString;
    QString typeFilterString;
    QString countryFilterString;

    // log
    QStringList includeKeywords;
    QStringList excludeKeywords;
    QRegularExpression includeCombined;
    QRegularExpression excludeCombined;
    QMutex logMutex;
    QQueue<QString> logQueue;
    QWaitCondition logWaiter;

    void append_log(const QString &log);

    void log_process_loop();

    bool should_print_log(const QString &log);

    void updateLogFilterFields();

    QList<int> filterProfilesList(const QList<int>& profileIDs);

    QList<int> get_now_selected_list();
    void refresh_startstop_button();

    QList<int> get_selected_or_group();

    void set_system_proxy(bool enable);

    void saveProfileFocusState();

    void restoreProfileFocusState();

    void clearUnavailableProfiles(bool confirm = true, QList<int> profileIDs = {});

    void dialog_message_impl(MwMessage cmd, const QStringList &args);

    void handle_deeplink_impl(const QString &url);

    void handle_addsub(const QString &url, const QString &name, bool autoUpdate);

    void handle_import_route(const QString &url);

    // Routes user-supplied text: throne:// links go to the deeplink handler, the
    // rest to the subscription/profile importer.
    void import_or_handle_deeplink(const QString &text);

    void refresh_proxy_list_column_size();

    void refresh_proxy_list_impl(const QList<int> &ids = {}, bool mayNeedReset = false);

    void refresh_proxy_list_impl_refresh_data(const QList<int>& ids = {}, bool mayNeedReset = false);

    void parseQrImage(const QPixmap *image);

    void keyPressEvent(QKeyEvent *event) override;

    void closeEvent(QCloseEvent *event) override;

    void changeEvent(QEvent *event) override;

    void showEvent(QShowEvent *event) override;

    void hideEvent(QHideEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

    // Tell the connection lister whether its tab is actually on screen (stats tab
    // selected, window neither minimized nor hidden to tray) so it can drop to a
    // relaxed poll cadence when nobody is looking. Recomputed on tab/visibility
    // changes.
    void syncConnectionViewState();

    void dragEnterEvent(QDragEnterEvent *event);

    void dropEvent(QDropEvent* event) override;

    void applyLogBrowserFont();

    // Debounced refresh_proxy_list trigger for font/theme/resize events.
    QTimer *m_proxyListRefreshDebounce = nullptr;
    void scheduleProxyListRefresh();

    // Watches the physical default-route interface while a profile whose Xray
    // egress is interface-bound (sockopt.interface) is running. A static bind is
    // baked at build time, so when the default route flips (Wi-Fi<->Ethernet,
    // VPN up/down) the name is stale and we rebuild+restart the profile. Inactive
    // while m_boundEgressInterface is empty (no interface-bound egress).
    QTimer *m_defaultInterfaceWatch = nullptr;
    QString m_boundEgressInterface;
    int m_ifcChangeStreak = 0;
    void checkDefaultInterfaceChange();

    //

    void HotkeyEvent(const QString &key);

    void RegisterHiddenMenuShortcuts(bool unregister = false);
    // Register a QShortcut for every action in `menu` (recursing into submenus),
    // appending them to hiddenMenuShortcuts. Needed because the menubar is hidden,
    // so actions reachable only through popup menus get no shortcut on their own.
    // `claimed` holds the key sequences already handled (either by Qt automatically
    // or by an earlier call); shortcuts already in it are skipped to avoid the
    // ambiguous-shortcut conflict that breaks actions shared with other menus.
    void registerMenuShortcuts(QMenu *menu, QSet<QKeySequence> &claimed);
    // Collect the shortcut key sequences of every action in `menu` (recursing into
    // submenus) into `out`, without registering anything.
    void collectMenuShortcuts(QMenu *menu, QSet<QKeySequence> &out);

    void setActionsData();

    QList<QAction*> getActionsForShortcut();

    void loadShortcuts();

    // rpc

    void setup_rpc(QLocalSocket *socket);

    bool verify_core_pid(QLocalSocket *socket);

    void urltest_current_group(const QList<int>& profileIDs);

    void iptest_current_group(const QList<int>& profileIDs);

    void stopTests();

    void runURLTest(const QString& config, const QString& xrayConfig, bool useDefault, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID = -1);

    void runIPTest(const QString& config, const QString& xrayConfig, bool useDefault, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID = -1);

    void url_test_current();

    void speedtest_current_group(const QList<int>& profileIDs, bool testCurrent = false);

    void runSpeedTest(const QString& config, const QString& xrayConfig, bool useDefault, bool testCurrent, const QStringList& outboundTags, const QMap<QString, int>& tag2entID, int entID = -1);

    bool set_system_dns(bool set, bool save_set = true);

    void CheckUpdate();

    void setupConnectionList();

    void setupConnectionSortMenu();

    void querySpeedtest(const QMap<QString, int>& tag2entID, bool testCurrent);

    // Credit the delta between a test's cumulative bytes (curUp/curDown) and the
    // last reported values for `tag`. Feeds the time-series stats (the tested
    // config + a synthetic "Speedtest" app) and the legacy per-profile total.
    // Speed tests bypass the clash tracker, so the looper never sees these bytes;
    // this is the only place they are counted, for both a selected-profile test
    // and a current-instance test.
    void creditSpeedtestTraffic(const std::shared_ptr<Configs::Profile>& profile, const QString& tag, qint64 curUp, qint64 curDown);

    void queryCountryTest(const QMap<QString, int>& tag2entID, bool testCurrent);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

#endif // MW_INTERFACE
};

inline MainWindow *GetMainWindow() {
    return (MainWindow *) mainwindow;
}

void UI_InitMainWindow();

#ifdef Q_OS_LINUX
/*
 * Proxy class for interface org.freedesktop.portal.Request
 */
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

Q_SIGNALS: // SIGNALS
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
