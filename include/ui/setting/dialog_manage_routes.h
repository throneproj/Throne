#pragma once

#include <QDialog>
#include <QMenu>

#include <atomic>

#include "3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.hpp"
#include "include/global/Configs.hpp"
#include "include/ui/setting/RouteItem.h"
#include "ui_dialog_manage_routes.h"
#include "include/database/entities/RouteProfile.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogManageRoutes;
}
QT_END_NAMESPACE

class DialogManageRoutes : public QDialog {
    Q_OBJECT

public:
    explicit DialogManageRoutes(QWidget *parent = nullptr);

    ~DialogManageRoutes() override;

private:
    Ui::DialogManageRoutes *ui;

    RouteItem* routeChainWidget;

    void reloadProfileItems();

    void applyImportedProfile(const std::shared_ptr<Configs::RouteProfile>& profile, bool wasOldArray);

    // If `text` is a throne://remoteRoute deep link, confirm and add its remote profiles to the
    // in-memory list (fetching them via the Update flow) and return true. Returns false when it
    // isn't such a link, so the caller can fall through to single-profile import.
    bool tryImportRemoteRoutesLink(const QString& text);

    // Fetch the given remote profiles from their URLs on a worker thread and refresh the list.
    // Changes stay in-memory and are persisted on accept(), like clone/import. While a batch
    // runs the Update button shows progress and its click offers Cancel instead of the menu.
    void updateRemoteProfiles(const QList<std::shared_ptr<Configs::RouteProfile>>& profiles);

    // True while an update batch is in flight (UI-thread only). routeUpdateCancel is set from
    // the UI thread and polled by the worker between profiles to stop early.
    bool routeUpdateRunning = false;
    std::atomic<bool> routeUpdateCancel{false};

    QList<std::shared_ptr<Configs::RouteProfile>> chainList;

    std::shared_ptr<Configs::RouteProfile> currentRoute;

    int tooltipID = 0;

    void set_dns_hijack_enability(bool enable) const;

    static bool validate_dns_rules(const QString &rawString);

    QShortcut* deleteShortcut;

    AutoCompleteTextEdit* rule_editor;
public slots:
    void accept() override;

    void updateCurrentRouteProfile(int idx);

    void on_new_route_clicked();

    void on_export_route_clicked();

    void on_import_route_clicked();

    void on_clone_route_clicked();

    void on_edit_route_clicked();

    void on_delete_route_clicked();

    void on_update_route_clicked();
};
