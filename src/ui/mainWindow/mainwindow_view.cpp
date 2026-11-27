#include "include/ui/mainwindow.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "include/api/RPC.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/database/SettingsRepo.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/engine/vpn/VpnAuthController.h"
#include "include/engine/vpn/VpnEndpoints.h"
#include "include/global/LocalNetwork.hpp"
#include "include/global/RunningProfiles.hpp"
#include "include/stats/autoselector/AutoSelectorMonitor.hpp"
#include "include/sys/KillSwitch.hpp"
#include "include/ui/setting/Icon.hpp"
#include "include/ui/stats/dialog_auto_selector.h"
#include "include/ui/utils/ProfilesTableFilterHeader.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include "include/ui/widget/LiveChart.h"
#include "include/ui/widget/StartStopButton.hpp"

namespace {
    struct RunningContext {
        QString groupName;
        QString detail;
    };

    // The detail is the VPN endpoint's exit state while it reports one, else the exit country.
    RunningContext runningContextOf(const Configs::Profile *running, const QString &vpnExitState) {
        if (running == nullptr) return {};
        const auto group = Configs::dataManager->groupsRepo->GetGroup(running->gid);
        return {group != nullptr ? group->name : QString(), vpnExitState.isEmpty() ? running->runningCountryInfo : vpnExitState};
    }
}

// Mirrors the language switch in main.cpp; QLocale() alone follows the system, not an explicit setting.
bool MainWindow::usesTightLabels() const {
    static const QStringList byLanguageSetting = {"", "en", "zh_CN", "fa_IR", "ru_RU"};
    const int language = Configs::dataManager->settingsRepo->language;
    const QString locale = language == 0 ? QLocale().name() : byLanguageSetting.value(language);
    return locale.startsWith("zh") || locale.startsWith("ru");
}

void MainWindow::applyTopBarMetrics() {
    const QList<QToolButton*> menuButtons = {
        ui->toolButton_program, ui->toolButton_preferences, ui->toolButton_testing,
        ui->toolButton_routing, ui->toolButton_tools,
    };
    // Drop the previous run's floor: a stale minimum gets baked into minimumSizeHint() below.
    for (auto* b : menuButtons) b->setMinimumWidth(0);

    int uniformButtonWidth = 0;
    for (auto* b : menuButtons) {
        b->ensurePolished();
        uniformButtonWidth = qMax(uniformButtonWidth, b->sizeHint().width());
    }

    // CJK glyphs and long RU labels fill QToolButton's one-space slack and run into the chevron (#1665, #1829).
    if (usesTightLabels()) {
        uniformButtonWidth += 2 * fontMetrics().horizontalAdvance(' ');
    }
    for (auto* b : menuButtons) b->setMinimumWidth(uniformButtonWidth);

    auto* centralLayout = centralWidget()->layout();
    // Nested layouts keep stale minimums until the next layout pass.
    centralLayout->activate();
    const QMargins margins = centralLayout->contentsMargins();
    // Top bar only: the rows below show profile names, and an explicit minimum never shrinks back (#2081).
    const int topBarWidth = ui->horizontalLayout_2->minimumSize().width() + margins.left() + margins.right();
    setMinimumSize(qMax(designMinimumSize.width(), topBarWidth),
                   qMax(designMinimumSize.height(), minimumSizeHint().height()));
}

void MainWindow::UpdateDataView(bool force)
{
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (!force && now - lastUpdatedMs.load() < 100)
    {
        return;
    }
    auto content = dataViewHtmlGenerator_.build();
    runOnUiThread([=, this] {
        ui->data_view->setHtml(content.html);
        ui->data_view->setToolTip(content.toolTip);
    }, true);
    lastUpdatedMs.store(QDateTime::currentMSecsSinceEpoch());
}

void MainWindow::testPanelSeeded(Engine::TestKind kind, int totalProfiles, const QList<int> &profileIds)
{
    int column = -1;
    switch (kind) {
        case Engine::TestKind::Latency:
            dataViewHtmlGenerator_.seedLatencyTest(DataViewHtmlGenerator::LatencyTestPanelState::Kind::Url, totalProfiles);
            column = ProfilesTableModel::ColLatency;
            break;
        case Engine::TestKind::Ip:
            dataViewHtmlGenerator_.seedLatencyTest(DataViewHtmlGenerator::LatencyTestPanelState::Kind::Ip, totalProfiles);
            column = ProfilesTableModel::ColExitIP;
            break;
        case Engine::TestKind::Service:
            dataViewHtmlGenerator_.seedLatencyTest(DataViewHtmlGenerator::LatencyTestPanelState::Kind::Service, totalProfiles);
            column = ProfilesTableModel::ColExitIP;
            break;
        case Engine::TestKind::Speed:
            dataViewHtmlGenerator_.seedSpeedTest(totalProfiles);
            column = ProfilesTableModel::ColSpeed;
            break;
    }
    runOnUiThread([this] { ui->menu_stop_testing->setEnabled(true); });
    if (column < 0) return;
    runOnUiThread([this, kind, column, profileIds] {
        QSet<int> groups;
        for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileBatch(profileIds)) groups.insert(profile->gid);
        bool currentChanged = false;
        for (int gid : groups) {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
            if (!group || (group->visible_columns & (1 << column))) continue;
            // Service icons show in the Exit IP column only while the group includes them.
            if (kind == Engine::TestKind::Service && !group->show_services) continue;
            group->visible_columns |= 1 << column;
            Configs::dataManager->groupsRepo->Save(group);
            currentChanged |= gid == Configs::dataManager->settingsRepo->current_group;
        }
        if (currentChanged) refresh_proxy_list_column_size();
    });
}

void MainWindow::testProgressed()
{
    dataViewHtmlGenerator_.addTestProgress();
}

void MainWindow::testPanelsCleared()
{
    dataViewHtmlGenerator_.clearTestSections();
    runOnUiThread([this] { ui->menu_stop_testing->setEnabled(false); });
}

void MainWindow::testViewChanged(bool force)
{
    UpdateDataView(force);
}

void MainWindow::speedTestProgress(const QString &profileName, const libcore::SpeedTestResult &result)
{
    dataViewHtmlGenerator_.setSpeedtestProgress(profileName, result);
}

void MainWindow::testedProfilesChanged(const QList<int> &profileIds)
{
    refresh_proxy_list(profileIds);
}

QString MainWindow::restartReasonText(Engine::RestartReason reason)
{
    switch (reason) {
        case Engine::RestartReason::Settings: return tr("Settings");
        case Engine::RestartReason::Routing: return tr("Routing");
        case Engine::RestartReason::Profile: return tr("Profile");
        case Engine::RestartReason::Group: return tr("Group");
    }
    return QObject::tr("Settings");
}

void MainWindow::showRestartReasons()
{
    QStringList reasons;
    for (const auto reason : connection()->restartReasons()) reasons << restartReasonText(reason);
    dataViewHtmlGenerator_.setPendingRestartReasons(reasons);
    UpdateDataView(true);
}

void MainWindow::showVpnEndpointStatus(const QString &summary, const QString &detail, bool problem)
{
    dataViewHtmlGenerator_.setVpnEndpointStatus(summary, detail, problem);
    UpdateDataView(true);
}

void MainWindow::setDownloadReport(const DownloadProgressReport& report, bool show)
{
    dataViewHtmlGenerator_.setDownloadReport(report, show);
}

void MainWindow::refresh_auto_selector_view()
{
    const auto view = Stats::autoSelectorMonitor->Snapshot();
    dataViewHtmlGenerator_.setAutoSelectorStatus(view.valid ? view.summary() : QString(),
                                                 view.valid ? view.detail() : QString());
    ui->actionAuto_Selector->setVisible(view.valid);
    UpdateDataView();
    if (m_autoSelectorDialog != nullptr) m_autoSelectorDialog->refresh();
}

void MainWindow::updateLogFilterFields() {
    // Validated and compiled here, so the log thread neither compiles it nor warns on every line about an invalid pattern.
    QRegularExpression hide(Configs::dataManager->settingsRepo->log_exclude_regex.join("|"));
    if (hide.isValid()) hide.optimize();
    else hide = QRegularExpression();
    QMutexLocker locker(&logMutex);
    logHidePattern = hide;
}

void MainWindow::applyProfileFilters()
{
    if (!profilesFilterModel) return;
    profilesFilterModel->setFilters(typeFilterString, addressFilterString, nameFilterString, testFilterString);
    refresh_proxy_list_column_size();
}

void MainWindow::refresh_status(const QString &traffic_update) {
    const auto* settings = Configs::dataManager->settingsRepo.get();

    auto refresh_speed_label = [=,this] {
        if (traffic_update_cache == "") {
            ui->label_speed->setText(QObject::tr("Proxy: %1\nDirect: %2").arg("", ""));
        } else {
            ui->label_speed->setText(traffic_update_cache);
        }
    };

    if (!traffic_update.isEmpty()) {
        traffic_update_cache = traffic_update;
        if (traffic_update == "STOP") {
            traffic_update_cache = "";
        } else {
            refresh_speed_label();
            return;
        }
    }

    refresh_speed_label();

    const auto running = runningSnapshot();
    const auto [group_name, runningDetail] = runningContextOf(running.get(), vpnAuth()->exitStateText());

    if (QDateTime::currentSecsSinceEpoch() - last_test_time > 2) {
        QString runningLabelText;
        if (running) {
            runningLabelText = QString("[%1] %2").arg(group_name, running->outbound->DisplayName());
            if (!runningDetail.isEmpty()) {
                runningLabelText += "\n" + runningDetail;
            }
        } else {
            runningLabelText = tr("Not Running");
        }
        ui->label_running->setText(runningLabelText);
    }
    const auto inbound_disabled = settings->disable_mixed_inbound;
    auto display_socks = DisplayAddress(settings->inbound_address, settings->inbound_socks_port);
    QString inbound_tip;
    if (!inbound_disabled && LocalNetwork::LanInboundIsWildcard()) {
        if (const auto lan = LocalNetwork::LanAddress(); !lan.isEmpty()) {
            inbound_tip = tr("Listening on all interfaces (%1)").arg(display_socks);
            display_socks = DisplayAddress(lan, settings->inbound_socks_port);
        }
    }
    ui->label_inbound->setText(tr("Mixed: %1").arg(inbound_disabled ? tr("Disabled") : display_socks));
    ui->label_inbound->setToolTip(inbound_tip);
    syncConnectionSourceColumn();
    ui->checkBox_VPN->setChecked(settings->spmode_vpn);
    ui->checkBox_SystemProxy->setChecked(settings->spmode_system_proxy);
    if (select_mode) {
        ui->label_running->setText(tr("Select") + " *");
        ui->label_running->setToolTip(tr("Double-click or press Enter to pick a profile; Esc to cancel."));
    } else {
        ui->label_running->setToolTip({});
    }

    refresh_tray_tooltip();
    refresh_startstop_button();
}

// The window title stays the plain "Throne" from the .ui: this status shows in the window itself.
void MainWindow::refresh_tray_tooltip() {
    if (tray == nullptr) return;
    const auto* settings = Configs::dataManager->settingsRepo.get();

    const auto running = runningSnapshot();
    const auto [group_name, runningDetail] = runningContextOf(running.get(), vpnAuth()->exitStateText());
    const QString routeName = Configs::dataManager->routesRepo->GetRouteProfileName(Configs::ActiveRouteId()).value_or(QString());
    const QString activeRouteName = routeName != "Default" ? routeName : "";
    const auto killSwitchState = Sys::KillSwitch::instance()->state();
    const bool killSwitchBlocking = killSwitchState == Sys::KillSwitch::State::Failed && Sys::KillSwitch::instance()->failedForIdentity();

    QStringList tt;
    if (Configs::KnownAdmin().value_or(false)) tt << "[Admin]";
    if (select_mode) tt << "[" + tr("Select") + "]";
    if (!title_error.isEmpty()) tt << "[" + title_error + "]";
    if (settings->spmode_vpn && !settings->spmode_system_proxy) tt << "[Tun]";
    if (!settings->spmode_vpn && settings->spmode_system_proxy) tt << "[" + tr("System proxy") + "]";
    if (settings->spmode_vpn && settings->spmode_system_proxy) tt << "[Tun+" + tr("System proxy") + "]";
    if (settings->kill_switch && killSwitchState == Sys::KillSwitch::State::Armed) tt << "[" + tr("Kill switch") + "]";
    if (settings->kill_switch && killSwitchState == Sys::KillSwitch::State::Failed) {
        tt << "[" + (killSwitchBlocking ? tr("Kill switch is blocking traffic") : tr("Kill switch: not active")) + "]";
    }
    tt << software_name;
    if (!activeRouteName.isEmpty()) {
        tt << "[" + activeRouteName + "]";
    }
    if (running != nullptr) {
        tt << running->outbound->DisplayTypeAndName() + "@" + group_name;
        if (!runningDetail.isEmpty()) {
            tt << runningDetail;
        }
    }
    tray->setToolTip(tt.join("\n"));
}

void MainWindow::refresh_startstop_button() {
    auto *btn = ui->toolButton_startstop;
    if (btn == nullptr) return;

    const auto &settings = Configs::dataManager->settingsRepo;
    const auto running = runningSnapshot();
    const auto connState = connection()->connectionState();
    const bool connecting = connState == ConnectionState::Connecting;
    const bool disconnecting = connState == ConnectionState::Stopping;

    auto mode = StartStopButton::Mode::Off;
    if (running != nullptr) {
        if (settings->spmode_vpn) mode = StartStopButton::Mode::Tun;
        else if (settings->spmode_system_proxy) mode = StartStopButton::Mode::SystemProxy;
        else mode = StartStopButton::Mode::Core;
    }
    btn->setMode(mode);

    const bool stopHeld = connecting && connection()->stopPending();
    StartStopButton::State state;
    if (connecting && !stopHeld) state = StartStopButton::State::Connecting;
    else if (connecting || disconnecting) state = StartStopButton::State::Disconnecting;
    else if (running != nullptr) state = StartStopButton::State::Running;
    else if (get_profile_to_start() >= 0) state = StartStopButton::State::Idle;
    else state = StartStopButton::State::Disabled;
    btn->setState(state);

    auto lock = StartStopButton::Lock::Hidden;
    if (settings->kill_switch) {
        switch (Sys::KillSwitch::instance()->state()) {
            case Sys::KillSwitch::State::Disabled: break;
            case Sys::KillSwitch::State::Arming: lock = StartStopButton::Lock::Pending; break;
            case Sys::KillSwitch::State::Armed:
                lock = running != nullptr && settings->spmode_vpn ? StartStopButton::Lock::Passing : StartStopButton::Lock::Blocking;
                break;
            case Sys::KillSwitch::State::Failed: lock = StartStopButton::Lock::Fault; break;
        }
    }
    btn->setLock(lock);

    if (trayConnectAction != nullptr) {
        if (connecting && !stopHeld) {
            trayConnectAction->setText(tr("Connecting"));
        } else if (connecting || disconnecting) {
            trayConnectAction->setText(tr("Disconnecting"));
        } else {
            trayConnectAction->setText(running != nullptr ? tr("Disconnect") : tr("Connect"));
        }
        trayConnectAction->setEnabled(!connecting && !disconnecting &&
                                      (running != nullptr || get_profile_to_start() >= 0));
        refreshTrayMenu();
    }

    // Here rather than in refresh_status(): the connecting/disconnecting flags only ever refresh this button.
    auto iconStatus = Icon::TrayIconStatus::None;
    if (connecting || disconnecting) iconStatus = Icon::TrayIconStatus::Connecting;
    else if (mode == StartStopButton::Mode::Tun) iconStatus = Icon::TrayIconStatus::Vpn;
    else if (mode == StartStopButton::Mode::SystemProxy) iconStatus = Icon::TrayIconStatus::SystemProxy;
    else if (mode == StartStopButton::Mode::Core) iconStatus = Icon::TrayIconStatus::Running;
    if (iconStatus != icon_status) {
        QApplication::setWindowIcon(GetTaskbarIcon(iconStatus));
        if (tray != nullptr) tray->setIcon(Icon::GetTrayIcon(iconStatus));
        icon_status = iconStatus;
    }
}

void MainWindow::refresh_action_states() {
    qsizetype selectedRows = 0;
    if (const auto *selection = ui->profilesTableView->selectionModel()) {
        // The view selects whole rows and the ranges never overlap, so their heights add up to the row count.
        for (const auto &range : selection->selection()) {
            selectedRows += range.height();
            if (selectedRows > 1) break;
        }
    }
    const bool anySelected = selectedRows > 0;
    ui->actionSpeedtest_Selected->setEnabled(anySelected);
    ui->actionUrl_Test_Selected->setEnabled(anySelected);
    ui->menu_resolve_selected->setEnabled(anySelected);
    ui->actionResolve_Selected_Out_IP->setEnabled(anySelected);
    ui->actionService_Check_Selected->setEnabled(anySelected);
    ui->menu_find_warp_endpoint->setEnabled(selectedRows == 1 && warpFinderTarget() != nullptr);

    const bool running = runningSnapshot() != nullptr;
    ui->actionSpeedtest_Current->setEnabled(running);
    ui->actionUpdate_Rule_Sets->setEnabled(running);
}

void MainWindow::set_traffic_lines(const QList<Stats::TrafficLine> &lines) {
    m_trafficLines = lines;
    if (trafficChart == nullptr) return;
    const bool extras = Configs::dataManager->settingsRepo->traffic_graph_extras;
    m_trafficFolded = false;
    QList<LiveChartRow> rows;
    int nextSlot = 1;
    for (const auto &line : lines) {
        LiveChartRow row;
        row.key = line.key;
        switch (line.kind) {
            case Stats::TrafficLineKind::Proxy:
                row.label = tr("Proxy");
                row.paletteSlot = 0;
                row.wash = true;
                break;
            case Stats::TrafficLineKind::Route:
            case Stats::TrafficLineKind::Aux:
            case Stats::TrafficLineKind::Other:
                if (!extras) {
                    m_trafficFolded = true;
                    continue;
                }
                row.label = line.name;
                if (line.kind == Stats::TrafficLineKind::Route) row.kind = tr("route");
                else if (line.kind == Stats::TrafficLineKind::Aux) row.kind = tr("aux");
                row.paletteSlot = std::min(nextSlot++, 4);
                row.minor = true;
                break;
            case Stats::TrafficLineKind::Direct:
                row.label = tr("Direct");
                row.paletteSlot = -1;
                break;
        }
        rows << row;
    }
    trafficChart->setRows(rows);
}

void MainWindow::add_traffic_point(qint64 monoMs, bool coreRunning, const QVector<double> &values,
                                   double proxyDown, double proxyUp, double directDown, double directUp) {
    if (trafficChart == nullptr) return;
    if (coreRunning && values.size() != m_trafficLines.size() * 2) return;
    if (!coreRunning || !m_trafficFolded) {
        trafficChart->addPoint(monoMs, coreRunning, values);
        return;
    }
    QVector<double> folded(4, std::numeric_limits<double>::quiet_NaN());
    for (qsizetype line = 0; line < m_trafficLines.size(); ++line) {
        const int at = m_trafficLines[line].kind == Stats::TrafficLineKind::Direct ? 2 : 0;
        for (int k = 0; k < 2; ++k) {
            const double v = values[line * 2 + k];
            if (std::isnan(v)) continue;
            double &sum = folded[at + k];
            sum = std::isnan(sum) ? v : sum + v;
        }
    }
    trafficChart->addPoint(monoMs, true, folded);
}

void MainWindow::refresh_traffic_graph_settings() {
    const auto &settings = Configs::dataManager->settingsRepo;
    if (m_trafficStatsChartAction != nullptr) m_trafficStatsChartAction->setVisible(!settings->disable_traffic_aggregation);
}

void MainWindow::refresh_proxy_list_column_size() {
    const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (!group || !ui->profilesTableView->isVisible()) return;

    auto *hHeader = dynamic_cast<ProfilesTableFilterHeader*>(ui->profilesTableView->horizontalHeader());
    QTimer::singleShot(0, ui->profilesTableView, [=, this]() {
        // The resizeSection / scrollbar-policy changes below re-enter here via valueChanged.
        if (m_adjustingColumns || group != Configs::dataManager->groupsRepo->CurrentGroup()) return;
        m_adjustingColumns = true;
        QScrollBar *vBar = ui->profilesTableView->verticalScrollBar();
        const bool vBarBlocked = vBar->blockSignals(true);
        // Header signals stay live: QTableView repaints its cells from sectionResized.
        constexpr int columnCount = ProfilesTableModel::ColumnCount;
        for (int column = 0; column < columnCount; ++column) {
            ui->profilesTableView->setColumnHidden(column, !(group->visible_columns & (1 << column)));
        }
        if (!group->column_width.isEmpty() && group->column_width.size() != columnCount) {
            group->column_width.clear();
        }
        if (group->column_width.isEmpty()) {
            hHeader->setSectionResizeMode(ProfilesTableModel::ColType, QHeaderView::ResizeToContents);
            hHeader->setSectionResizeMode(ProfilesTableModel::ColAddress, QHeaderView::Stretch);
            hHeader->setSectionResizeMode(ProfilesTableModel::ColName, QHeaderView::Stretch);
            for (int col = ProfilesTableModel::ColLatency; col < columnCount; ++col) {
                hHeader->setSectionResizeMode(col, QHeaderView::ResizeToContents);
            }
            // ResizeToContents only measures on-screen rows, so pin these or they jitter while scrolling.
            for (int col : {ProfilesTableModel::ColType, ProfilesTableModel::ColLatency, ProfilesTableModel::ColExitIP,
                            ProfilesTableModel::ColSpeed, ProfilesTableModel::ColTraffic}) {
                if (hHeader->isSectionHidden(col)) continue;
                if (group->calculated_column_width.size() > col &&
                    group->calculated_column_width[col] > hHeader->sectionSize(col)) {
                    hHeader->setSectionResizeMode(col, QHeaderView::Fixed);
                    hHeader->resizeSection(col, group->calculated_column_width[col]);
                }
            }
            ui->profilesTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            group->clearCalculatedColumnWidth();
            for (int i = 0; i < columnCount; i++) {
                const auto size = hHeader->isSectionHidden(i) ? 0 : hHeader->sectionSize(i);
                hHeader->setSectionResizeMode(i, QHeaderView::Interactive);
                if (size > 0) hHeader->resizeSection(i, size);
                group->calculated_column_width << size;
            }
        } else {
            group->clearCalculatedColumnWidth();
            for (int i = 0; i < columnCount; i++) {
                hHeader->setSectionResizeMode(i, QHeaderView::Interactive);
                if (hHeader->isSectionHidden(i)) continue;
                const int width = group->column_width.at(i);
                if (width > 0) hHeader->resizeSection(i, width);
                else ui->profilesTableView->resizeColumnToContents(i);
            }
            ui->profilesTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        }
        hHeader->adjustPositions();
        vBar->blockSignals(vBarBlocked);
        m_adjustingColumns = false;
    });
}

void MainWindow::editProfileColumnOrder() {
    auto *header = static_cast<ProfilesTableFilterHeader*>(ui->profilesTableView->horizontalHeader());
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Reorder columns"));
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Drag the columns into the order you want."), &dialog));
    auto *list = new QListWidget(&dialog);
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    layout->addWidget(list);
    const auto fill = [this, list](const QList<int> &order) {
        list->clear();
        for (const int column : order) {
            auto *item = new QListWidgetItem(profilesTableModel->headerData(column, Qt::Horizontal).toString(), list);
            item->setData(Qt::UserRole, column);
        }
    };
    QList<int> order;
    for (int visual = 0; visual < header->count(); ++visual) order << header->logicalIndex(visual);
    fill(order);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, &dialog, [header, fill] {
        QList<int> defaults;
        for (int column = 0; column < header->count(); ++column) defaults << column;
        fill(defaults);
    });
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;
    order.clear();
    for (int row = 0; row < list->count(); ++row) order << list->item(row)->data(Qt::UserRole).toInt();
    header->setColumnOrder(order);
}

void MainWindow::refresh_proxy_list(const QList<int>& ids, bool mayNeedReset, RefreshAnchor anchor) {
    if (!Configs::dataManager->settingsRepo->refreshing_group) saveProfileFocusState();
    refresh_proxy_list_impl(ids, mayNeedReset);
    if (mayNeedReset) restoreProfileFocusState(anchor);
}

void MainWindow::refresh_proxy_list_impl(const QList<int>& ids, bool mayNeedReset) {
    const auto currentGroup = Configs::dataManager->groupsRepo->CurrentGroup();
    if (currentGroup == nullptr)
    {
        MW_show_log("Could not find current group!");
        return;
    }
    refresh_proxy_list_impl_refresh_data(ids, mayNeedReset);
    refresh_sort_indicator();
    refresh_proxy_list_column_size();
}

void MainWindow::refresh_sort_indicator() {
    const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    const int column = group == nullptr ? -1 : ProfilesTableModel::columnOfSort(*group);
    auto *header = static_cast<ProfilesTableFilterHeader*>(ui->profilesTableView->horizontalHeader());
    if (header->setSortSection(column, group != nullptr && group->sort.descending)) refresh_proxy_list_column_size();
}

void MainWindow::sortCurrentGroup(GroupSortAction action) {
    const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (group == nullptr) return;
    group->SetSort(action);
    Configs::dataManager->groupsRepo->Save(group);
    refresh_sort_indicator();
    Configs::ResortLater(group->id);
}

void MainWindow::restoreOriginalOrder() {
    const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (group == nullptr) return;
    group->RestoreOriginalOrder();
    Configs::dataManager->groupsRepo->Save(group);
    refresh_proxy_list({}, true);
}

void MainWindow::refresh_proxy_list_impl_refresh_data(const QList<int>& ids, bool mayNeedReset) {
    const auto currentGroup = Configs::dataManager->groupsRepo->CurrentGroup();
    if (currentGroup == nullptr) return;
    if (!ids.isEmpty()) {
        for (auto id:ids) profilesTableModel->refreshProfileId(id);
    } else {
        profilesTableModel->refreshTable(currentGroup->DisplayedProfiles(), currentGroup->PinnedCount(), mayNeedReset);
    }
}

void MainWindow::url_test_current() {
    last_test_time = QDateTime::currentSecsSinceEpoch();
    ui->label_running->setText(tr("Testing"));

    runOnNewThread([=,this] {
        libcore::TestReq req;
        req.test_current = true;
        req.url = Configs::dataManager->settingsRepo->test_latency_url.toStdString();

        bool rpcOK;
        auto result = API::defaultClient->Test(&rpcOK, req);
        if (!rpcOK || result.results.empty()) return;

        auto latency = result.results[0].latency_ms.value();
        last_test_time = QDateTime::currentSecsSinceEpoch();
        // Blocking RPC, so it has to resolve here rather than on the UI thread.
        const auto vpnText = latency <= 0 ? Stats::LiveVpnStateText() : QString();

        runOnUiThread([=,this] {
            if (!result.results[0].error.value().empty()) {
                MW_show_log(QString("URL test error: %1").arg(QString::fromStdString(result.results[0].error.value())));
            }
            if (latency <= 0) {
                ui->label_running->setText(tr("Test Result") + ": " + (vpnText.isEmpty() ? tr("Unavailable") : vpnText));
            } else if (latency > 0) {
                ui->label_running->setText(tr("Test Result") + ": " + QString("%1 ms").arg(latency));
            }
        });
    });
}
