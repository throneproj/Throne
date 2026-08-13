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
#include <QCheckBox>
#include <QSemaphore>
#include <QMutex>
#include <QThreadPool>
#include <QLocalServer>
#include <QLocalSocket>
#include <atomic>
#include <memory>

#include "group/GroupSort.hpp"
#include "include/global/GuiUtils.hpp"
#include "include/ui/utils/DataViewHtmlGenerator.h"
#include "include/ui/utils/ProfilesFilterProxyModel.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include "ui_mainwindow.h"

#endif

namespace Configs_sys {
    class CoreProcess;
    class KillSwitchController;
}

class TrayProfileSelector;
class TestRunner;

namespace Qv2ray::ui { class SyntaxHighlighter; }
#ifdef Q_OS_WIN
class WindowsWfpKillSwitchBackend;
#endif

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

enum class RefreshAnchor {
    // Re-select the same profiles by id; select nothing if they are gone.
    KeepPlace,
    // As above, but if all of them were deleted select whatever took their row.
    Removal,
};

// What the app launches in place of itself once on_menu_exit_triggered() has torn
// it down. Doubles as get_elevated_permissions()'s reason: on Windows that exits
// and relaunches elevated with the matching flag.
enum class ExitReason {
    None,
    RunUpdater,
    Restart,
    RestartWithTun,
    RestartWithDns,
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    ~MainWindow() override;

    // Runtime Stats panel helpers, read on the UI thread. GetCorePid returns 0
    // when the core process isn't running; GetRunningConfigName is empty when no
    // profile is active.
    qint64 GetCorePid();
    QString GetRunningConfigName();

    bool prepare_exit();

    void refresh_proxy_list(const QList<int> &ids = {}, bool mayNeedReset = false,
                            RefreshAnchor anchor = RefreshAnchor::KeepPlace);

    void show_group(int gid);

    void refresh_groups();

    void refresh_status(const QString &traffic_update = "");

    void update_traffic_graph(int proxyDl, int proxyUp, int directDl, int directUp);

    void profile_start(int _id = -1);

    // Returns false only when the stop was refused or, for a blocking call,
    // failed. Asynchronous callers receive true once the protected stop was
    // accepted.
    bool profile_stop(bool crash = false, bool block = false, bool manual = false);

    int get_profile_to_start();

    void set_spmode_system_proxy(bool enable, bool save = true);

    void toggle_system_proxy();

    void set_spmode_vpn(bool enable, bool save = true);

    bool get_elevated_permissions(ExitReason reason = ExitReason::RestartWithTun);

    void start_select_mode(QObject *context, const std::function<void(int)> &callback);

    void RegisterHotkey(bool unregister);

    // Force-stops ThroneCore only after fail-closed policy is prepared. When
    // clearStartedProfile is true the previous profile is not reconnected.
    bool StopVPNProcess(bool clearStartedProfile = false);

    // Applies the OS policy before changing the persisted preference. On
    // failure the current protection state is retained and error is populated.
    bool setKillSwitchEnabled(bool enable, QString *error = nullptr);

    // Called as soon as QProcess observes a core exit; persistent blocking is
    // retained while obsolete TUN permissions are removed.
    void killSwitchCoreTerminated(bool reconnectPlanned);

    void UpdateConnectionList(const QMap<QString, Stats::ConnectionMetadata>& toUpdate, const QMap<QString, Stats::ConnectionMetadata>& toAdd);

    void UpdateConnectionListWithRecreate(const QList<Stats::ConnectionMetadata>& connections);

    void UpdateDataView(bool force = false);

    // Pushes the auto-selector snapshot into the data view, toggles the Tools
    // entry, and refreshes the dialog if it is open.
    void refresh_auto_selector_view();

    // Non-owning: cleared by the dialog's finished() handler.
    class DialogAutoSelector *m_autoSelectorDialog = nullptr;

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
    // Monotonic, and invalid while the window is active or was never activated; see trayClickEvent().
    QElapsedTimer sinceWindowDeactivated;
    ProfilesTableModel *profilesTableModel = nullptr;
    // What the view is attached to: rows from the view or its selection model are
    // proxy rows, not profilesTableModel rows.
    ProfilesFilterProxyModel *profilesFilterModel = nullptr;
    QSystemTrayIcon *tray;
    QMenu *trayMenu = nullptr;    // tray context menu
    // Tray "Select Server"/"Select Routing" open this small Qt-drawn popup instead of a
    // submenu, because a tray submenu isn't painted by Qt on Linux (SNI/DBusMenu) or macOS
    // (native NSMenu) and so can't reliably expand a dynamic list. Recreated on each open.
    QPointer<TrayProfileSelector> traySelector;
    void openTraySelector(bool routing);
    QShortcut *shortcut_esc = new QShortcut(QKeySequence::Cancel, this);
    //
    // Shared by the test sweeps and the batch profile scans (remove-invalid).
    QThreadPool *parallelCoreCallPool = new QThreadPool(this);
    std::unique_ptr<TestRunner> testRunner;
    // Held across every TestRunner session. Kill-switch policy changes acquire
    // the same gate so a test config cannot be built under one policy and run
    // under another. QSemaphore permits acquisition and release on different
    // worker/UI threads.
    QSemaphore testActivityGate{1};
    //
    Configs_sys::CoreProcess *core_process = nullptr;
    QMutex coreProcessMutex; // serializes core_process init (DS_cores) vs IPC newConnection (UI)
    QLocalServer *core_server = nullptr;
    bool rpc_started = false;
    qint64 vpn_pid = 0;
#ifdef Q_OS_WIN
    std::unique_ptr<WindowsWfpKillSwitchBackend> killSwitchBackend;
    std::unique_ptr<Configs_sys::KillSwitchController> killSwitchController;
    bool killSwitchPreviousProfileUsedTun = false;
    bool killSwitchPreviousTunIpv6 = false;
#endif
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
    // Single-flight guard for the Xray geo-asset (geoip.dat/geosite.dat) download
    // prompt: a batch test can surface the missing-asset error for many profiles at
    // once, and we only want one prompt/download. Touched on the UI thread only.
    bool m_xrayGeoAssetBusy = false;
    QString traffic_update_cache;
    qint64 last_test_time = 0;
    //
    int proxy_last_order = -1;
    bool select_mode = false;
    QSemaphore mu_starting{1};
    QSemaphore mu_stopping{1};
    QMutex mu_exit;
    ExitReason exit_reason = ExitReason::None;
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
    // Repaint throttle, in ms since epoch. Atomic: worker threads drive it too.
    std::atomic<qint64> lastUpdatedMs = QDateTime::currentMSecsSinceEpoch();
    DataViewHtmlGenerator dataViewHtmlGenerator_;

    // shortcuts
    QList<QShortcut*> hiddenMenuShortcuts;

    // search
    QString addressFilterString;
    QString nameFilterString;
    QString typeFilterString;
    QString countryFilterString;

    QTimer *m_filterRefreshDebounce = nullptr;

    // Only meaningful between a saveProfileFocusState() and its restore.
    bool m_profilesTableHadFocus = false;
    int m_profilesScrollValue = 0;

    // log
    QStringList includeKeywords;
    QStringList excludeKeywords;
    QRegularExpression includeCombined;
    QRegularExpression excludeCombined;
    QMutex logMutex;
    QQueue<QString> logQueue;
    QWaitCondition logWaiter;
    Qv2ray::ui::SyntaxHighlighter *logHighlighter = nullptr;

    // Immutable snapshot of the log filter fields. The log thread copies these
    // under logMutex (Qt containers are copy-on-write, so it's O(1)) and then
    // filters without holding the lock, so producers calling append_log() are
    // never blocked on the regex/keyword work.
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

    bool should_print_log(const QString &log, const LogFilter &filter);

    void updateLogFilterFields();

    // (Re)installs the log syntax highlighter, deleting any previous one so
    // highlighters don't stack up (and keep re-highlighting) on theme changes.
    void setLogHighlighter(bool darkMode);

    void applyProfileFilters();

    QList<int> get_now_selected_list();
    void refresh_startstop_button();

    QList<int> get_selected_or_group();

    void set_system_proxy(bool enable);

    void saveProfileFocusState();

    void restoreProfileFocusState(RefreshAnchor anchor);

    void selectProfileRows(const QList<int> &rows);

    void focusProfilesTable(bool selectFirst);

    void clearUnavailableProfiles(bool confirm = true, QList<int> profileIDs = {});

    void dialog_message_impl(MwMessage cmd, const QStringList &args);

    void handle_deeplink_impl(const QString &url);

    void handle_addsub(const QString &url, const QString &name);

    void handle_import_route(const QString &url);

    // throne://remoteRoute?data=<...> : add one or more remote routing profiles. The data is
    // (base64 of) a JSON array of {url, auto_update[, name]} objects.
    void handle_add_remote_routes(const QString &url);

    // Routes user-supplied text: throne:// links go to the deeplink handler, the
    // rest to the subscription/profile importer.
    void import_or_handle_deeplink(const QString &text);

    void refresh_proxy_list_column_size();

    void refresh_proxy_list_impl(const QList<int> &ids = {}, bool mayNeedReset = false);

    void refresh_proxy_list_impl_refresh_data(const QList<int>& ids = {}, bool mayNeedReset = false);

    void parseQrImage(const QPixmap *image);

    // Imports local files picked from the file dialog or dropped on the window.
    // What each file is gets decided from its bytes, never from its name: config
    // files arrive as .json, .conf, .txt or with no extension at all.
    void importFromFiles(const QStringList &paths);

    void trayClickEvent();

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

    // Re-derives the top bar's sizing from the current font and translation, and
    // raises the window's minimum to whatever the layout actually needs. Called
    // at startup and on every font change.
    void applyTopBarMetrics();

    // The window minimum the .ui was designed with; applyTopBarMetrics() only ever
    // grows past this, so a smaller font returns to the designed floor.
    QSize designMinimumSize;

    // Debounced refresh_proxy_list trigger for font/theme/resize events.
    QTimer *m_proxyListRefreshDebounce = nullptr;
    void scheduleProxyListRefresh();

    bool m_adjustingColumns = false;

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

    // Measures the members of an auto selector that have no test result yet
    // (plus `stale`, whose stored result is known to be out of date) and
    // rewrites its ranked pool. Blocks — call from a worker thread.
    void rank_auto_selector(const std::shared_ptr<Configs::Profile>& ent, const QList<int>& stale = {});

    // Every running member of the auto selector died: re-rank and restart on
    // the next batch of good ones.
    void on_auto_selector_exhausted(int profileID);

    // A subscription refresh rewrote the servers of `gid`. Drops ids that no
    // longer exist from every selector tracking that group, and rebuilds the
    // running one only if the refresh touched a member it actually built.
    // `disturbed` holds the profiles the refresh deleted or replaced in place.
    void on_subscription_group_changed(int gid, const QList<int>& disturbed);

    // Guards the re-entrant profile_start used to rank before building.
    bool auto_selector_ranked = false;

    // If `error` reports missing Xray geo assets (geoip.dat / geosite.dat), prompt
    // once (guarded by m_xrayGeoAssetBusy) and download the missing .dat files in
    // the background. Shared by profile start and the test paths. `contextName` is
    // the profile/config name shown in the prompt. Returns true when the error was
    // a geo-asset error (and thus handled), false otherwise.
    bool handleXrayGeoAssetError(const QString& error, const QString& contextName);

    void url_test_current();

    bool set_system_dns(bool set, bool save_set = true);

    bool initializeKillSwitch();

    [[nodiscard]] bool killSwitchActive() const;

    bool prepareKillSwitchProfileStart(bool switching, quint64 *operationId,
                                       QString *error);

    bool finishKillSwitchProfileStart(quint64 operationId, QString *error);

    void failKillSwitchProfileStart(quint64 operationId, const QString &error,
                                    bool coreInstanceMayBeRunning = false);

    bool prepareKillSwitchProfileStop(QString *error);

    void finishKillSwitchProfileStop();

    void failKillSwitchProfileStop(const QString &error);

    bool prepareKillSwitchExit(QString *error);

    void CheckUpdate();

    void setupConnectionList();

    void setupConnectionSortMenu();

    // The window's own component, not an outside caller: it drives the data
    // view, the profile table and the geo-asset prompt while a sweep runs.
    friend class TestRunner;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

#endif // MW_INTERFACE
};

inline MainWindow *GetMainWindow() {
    return (MainWindow *) mainwindow;
}

[[nodiscard]] bool UI_InitMainWindow();

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
