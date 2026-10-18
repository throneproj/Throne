#include "include/ui/mainwindow.h"
#include "NkrVersion.h"

#include <algorithm>

#include <QApplication>
#include <QHeaderView>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>

#include "include/api/RPC.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/database/SettingsRepo.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/engine/vpn/VpnAuthController.h"
#include "include/engine/vpn/VpnEndpoints.h"
#include "include/global/LocalNetwork.hpp"
#include "include/stats/autoselector/AutoSelectorMonitor.hpp"
#include "include/sys/KillSwitch.hpp"
#include "include/ui/setting/Icon.hpp"
#include "include/ui/stats/dialog_auto_selector.h"
#include "include/ui/utils/ProfilesTableFilterHeader.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include "include/ui/widget/LiveChart.h"
#include "include/ui/widget/StartStopButton.hpp"

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

    const QSize contentMin = minimumSizeHint();
    setMinimumSize(qMax(designMinimumSize.width(), contentMin.width()),
                   qMax(designMinimumSize.height(), contentMin.height()));
}

void MainWindow::UpdateDataView(bool force)
{
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (!force && now - lastUpdatedMs.load() < 100)
    {
        return;
    }
    auto html = dataViewHtmlGenerator_.buildHtml();
    runOnUiThread([=, this] {
        ui->data_view->setHtml(html);
    }, true);
    lastUpdatedMs.store(QDateTime::currentMSecsSinceEpoch());
}

void MainWindow::testPanelSeeded(Engine::TestKind kind, int totalProfiles)
{
    switch (kind) {
        case Engine::TestKind::Latency:
            dataViewHtmlGenerator_.seedLatencyTest(DataViewHtmlGenerator::LatencyTestPanelState::Kind::Url, totalProfiles);
            break;
        case Engine::TestKind::Ip:
            dataViewHtmlGenerator_.seedLatencyTest(DataViewHtmlGenerator::LatencyTestPanelState::Kind::Ip, totalProfiles);
            break;
        case Engine::TestKind::Speed:
            dataViewHtmlGenerator_.seedSpeedTest(totalProfiles);
            break;
    }
}

void MainWindow::testProgressed()
{
    dataViewHtmlGenerator_.addTestProgress();
}

void MainWindow::testPanelsCleared()
{
    dataViewHtmlGenerator_.clearTestSections();
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

void MainWindow::applyProfileFilters()
{
    if (!profilesFilterModel) return;
    profilesFilterModel->setFilters(typeFilterString, addressFilterString, nameFilterString, countryFilterString);
    refresh_proxy_list_column_size();
}

void MainWindow::refresh_status(const QString &traffic_update) {
    const auto* settings = Configs::dataManager->settingsRepo.get();

    auto refresh_speed_label = [=,this] {
        if (settings->disable_traffic_stats) {
            ui->label_speed->setText("");
        }
        else if (traffic_update_cache == "") {
            ui->label_speed->setText(QObject::tr("Proxy: %1\nDirect: %2").arg("", ""));
        } else {
            ui->label_speed->setText(traffic_update_cache);
        }
    };

    if (!traffic_update.isEmpty() && !settings->disable_traffic_stats) {
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
    QString group_name;
    if (running != nullptr) {
        auto group = Configs::dataManager->groupsRepo->GetGroup(running->gid);
        if (group != nullptr) group_name = group->name;
    }

    const QString vpnExitState = vpnAuth()->exitStateText();
    const QString runningDetail = vpnExitState.isEmpty()
                                      ? (running ? running->runningCountryInfo : QString())
                                      : vpnExitState;

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
    ui->label_inbound->setText(QString("Mixed: %1").arg(inbound_disabled ? "Disabled" : display_socks));
    ui->label_inbound->setToolTip(inbound_tip);
    syncConnectionSourceColumn();
    ui->checkBox_VPN->setChecked(settings->spmode_vpn);
    ui->checkBox_SystemProxy->setChecked(settings->spmode_system_proxy);
    if (select_mode) {
        ui->label_running->setText(tr("Select") + " *");
        ui->label_running->setToolTip(tr("Select mode, double-click or press Enter to select a profile, press ESC to exit."));
    } else {
        ui->label_running->setToolTip({});
    }

    const auto route = Configs::dataManager->routesRepo->GetRouteProfile(settings->current_route_id);
    const QString activeRouteName = (route && route->name != "Default") ? route->name : "";
    const auto killSwitchState = Sys::KillSwitch::instance()->state();
    const bool killSwitchBlocking = killSwitchState == Sys::KillSwitch::State::Failed && Sys::KillSwitch::instance()->failedForIdentity();

    auto make_title = [=,this](bool isTray) {
        QStringList tt;
        if (!isTray && Configs::KnownAdmin().value_or(false)) tt << "[Admin]";
        if (select_mode) tt << "[" + tr("Select") + "]";
        if (!title_error.isEmpty()) tt << "[" + title_error + "]";
        if (settings->spmode_vpn && !settings->spmode_system_proxy) tt << "[Tun]";
        if (!settings->spmode_vpn && settings->spmode_system_proxy) tt << "[" + tr("System Proxy") + "]";
        if (settings->spmode_vpn && settings->spmode_system_proxy) tt << "[Tun+" + tr("System Proxy") + "]";
        if (settings->kill_switch && killSwitchState == Sys::KillSwitch::State::Armed) tt << "[" + tr("Kill switch") + "]";
        if (settings->kill_switch && killSwitchState == Sys::KillSwitch::State::Failed) {
            tt << "[" + (killSwitchBlocking ? tr("Kill switch is blocking traffic") : tr("Kill switch: not active")) + "]";
        }
        tt << software_name;
        if (!isTray) tt << QString(NKR_VERSION);
        if (!activeRouteName.isEmpty()) {
            tt << "[" + activeRouteName + "]";
        }
        if (running != nullptr) {
            tt << running->outbound->DisplayTypeAndName() + "@" + group_name;
            if (!runningDetail.isEmpty()) {
                tt << runningDetail;
            }
        }
        return tt.join(isTray ? "\n" : " ");
    };

    setWindowTitle(make_title(false));
    if (tray != nullptr) tray->setToolTip(make_title(true));

    refresh_startstop_button();
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

    StartStopButton::State state;
    if (connecting) state = StartStopButton::State::Connecting;
    else if (disconnecting) state = StartStopButton::State::Disconnecting;
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
        if (connecting) {
            trayConnectAction->setText(tr("Connecting"));
        } else if (disconnecting) {
            trayConnectAction->setText(tr("Disconnecting"));
        } else {
            trayConnectAction->setText(running != nullptr ? tr("Disconnect") : tr("Connect"));
        }
        trayConnectAction->setEnabled(!connecting && !disconnecting &&
                                      (running != nullptr || get_profile_to_start() >= 0));
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

void MainWindow::set_traffic_lines(const QList<Stats::TrafficLine> &lines) {
    m_trafficLines = lines;
    if (trafficChart == nullptr) return;
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
                row.label = line.name;
                row.kind = line.kind == Stats::TrafficLineKind::Route ? tr("route") : tr("aux");
                row.paletteSlot = std::min(nextSlot++, 4);
                break;
            case Stats::TrafficLineKind::Other:
                row.label = line.name;
                row.paletteSlot = std::min(nextSlot++, 4);
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
    trafficChart->addPoint(monoMs, coreRunning, values);
}

void MainWindow::addRunMarker(qint64 monoMs, Engine::RunMarker kind, const QString &name) {
    if (trafficChart == nullptr) return;
    auto marker = LiveChart::Marker::Started;
    switch (kind) {
        case Engine::RunMarker::Started: marker = LiveChart::Marker::Started; break;
        case Engine::RunMarker::Switched: marker = LiveChart::Marker::Switched; break;
        case Engine::RunMarker::Stopped: marker = LiveChart::Marker::Stopped; break;
    }
    trafficChart->addMarker(monoMs, marker, name);
}

void MainWindow::refresh_traffic_graph_settings() {
    const auto &settings = Configs::dataManager->settingsRepo;
    if (m_trafficStatsChartAction != nullptr) m_trafficStatsChartAction->setVisible(!settings->disable_traffic_aggregation);
    if (trafficChart == nullptr) return;
    if (settings->disable_traffic_stats) {
        trafficChart->setBlocked(tr("Traffic statistics are disabled"),
                                 tr("Untick “Disable Traffic Stats” in Basic Settings → Miscellaneous to record speeds again."));
    } else {
        trafficChart->setBlocked({});
    }
}

void MainWindow::refresh_proxy_list_column_size() {
    const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (!group || !ui->profilesTableView->isVisible()) return;

    auto *hHeader = dynamic_cast<ProfilesTableFilterHeader*>(ui->profilesTableView->horizontalHeader());
    QTimer::singleShot(0, ui->profilesTableView, [=, this]() {
        // The resizeSection / scrollbar-policy changes below re-enter here via valueChanged.
        if (m_adjustingColumns) return;
        m_adjustingColumns = true;
        QScrollBar *vBar = ui->profilesTableView->verticalScrollBar();
        const bool vBarBlocked = vBar->blockSignals(true);
        // Header signals stay live: QTableView repaints its cells from sectionResized.
        constexpr int columnCount = ProfilesTableModel::ColumnCount;
        if (!group->column_width.isEmpty() && group->column_width.size() != columnCount) {
            group->column_width.clear();
        }
        if (group->column_width.isEmpty()) {
            hHeader->setSectionResizeMode(ProfilesTableModel::ColType, QHeaderView::ResizeToContents);
            hHeader->setSectionResizeMode(ProfilesTableModel::ColAddress, QHeaderView::Stretch);
            hHeader->setSectionResizeMode(ProfilesTableModel::ColName, QHeaderView::Stretch);
            hHeader->setSectionResizeMode(ProfilesTableModel::ColTestResult, QHeaderView::ResizeToContents);
            hHeader->setSectionResizeMode(ProfilesTableModel::ColTraffic, QHeaderView::ResizeToContents);
            // ResizeToContents only measures on-screen rows, so pin these or they jitter while scrolling.
            for (int col : {ProfilesTableModel::ColType,
                            ProfilesTableModel::ColTestResult, ProfilesTableModel::ColTraffic}) {
                if (group->calculated_column_width.size() > col &&
                    group->calculated_column_width[col] > hHeader->sectionSize(col)) {
                    hHeader->setSectionResizeMode(col, QHeaderView::Fixed);
                    hHeader->resizeSection(col, group->calculated_column_width[col]);
                }
            }
            ui->profilesTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            group->clearCalculatedColumnWidth();
            for (int i = 0; i < columnCount; i++) {
                auto size = hHeader->sectionSize(i);
                hHeader->setSectionResizeMode(i, QHeaderView::Interactive);
                hHeader->resizeSection(i, size);
                group->calculated_column_width << size;
            }
        } else {
            group->clearCalculatedColumnWidth();
            for (int i = 0; i < columnCount; i++) {
                hHeader->setSectionResizeMode(i, QHeaderView::Interactive);
                hHeader->resizeSection(i, group->column_width.at(i));
            }
            ui->profilesTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        }
        hHeader->adjustPositions();
        vBar->blockSignals(vBarBlocked);
        m_adjustingColumns = false;
    });
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
    const int column = group == nullptr ? -1 : ProfilesTableModel::columnOfSort(group->sort.method);
    const auto order = group != nullptr && group->sort.descending ? Qt::DescendingOrder : Qt::AscendingOrder;
    // Section -1 hides the indicator.
    ui->profilesTableView->horizontalHeader()->setSortIndicator(column, order);
}

void MainWindow::sortCurrentGroup(GroupSortAction action) {
    const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (group == nullptr) return;
    group->SetSort(action);
    Configs::dataManager->groupsRepo->Save(group);
    refresh_sort_indicator();
    Configs::ResortLater(group->id);
}

void MainWindow::stopSortingCurrentGroup() {
    const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (group == nullptr) return;
    group->SetSort({});
    Configs::dataManager->groupsRepo->Save(group);
    refresh_sort_indicator();
}

void MainWindow::refresh_proxy_list_impl_refresh_data(const QList<int>& ids, bool mayNeedReset) {
    const auto currentGroup = Configs::dataManager->groupsRepo->CurrentGroup();
    if (currentGroup == nullptr) return;
    if (!ids.isEmpty()) {
        for (auto id:ids) profilesTableModel->refreshProfileId(id);
    } else {
        profilesTableModel->refreshTable(currentGroup->profiles, mayNeedReset);
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
                MW_show_log(QString("UrlTest error: %1").arg(QString::fromStdString(result.results[0].error.value())));
            }
            if (latency <= 0) {
                ui->label_running->setText(tr("Test Result") + ": " + (vpnText.isEmpty() ? tr("Unavailable") : vpnText));
            } else if (latency > 0) {
                ui->label_running->setText(tr("Test Result") + ": " + QString("%1 ms").arg(latency));
            }
        });
    });
}
