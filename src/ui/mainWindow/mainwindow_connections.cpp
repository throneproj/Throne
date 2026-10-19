#include "include/ui/mainwindow.h"
#include "include/api/RPC.h"
#include "include/configs/outbounds/chain.h"
#include "include/configs/outbounds/custom.h"
#include "include/database/entities/RouteProfile.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/database/SettingsRepo.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/engine/services/StatsService.h"
#include "include/global/Common.h"
#include "include/global/LocalNetwork.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ConnectionsFilterHeader.h"
#include "include/ui/utils/ConnectionsTreeFilterProxyModel.h"
#include "include/ui/utils/ConnectionsTreeModel.h"

#include <QHostAddress>

#include <QAbstractItemView>
#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QTreeView>

namespace
{
    // tile.openstreetmap.org -> ["tile.openstreetmap.org", "openstreetmap.org", "org"]
    QStringList DomainLevels(const QString& host)
    {
        const auto labels = host.split('.', Qt::SkipEmptyParts);
        QStringList levels;
        for (qsizetype i = 0; i < labels.size(); ++i)
            levels << QStringList(labels.mid(i)).join('.');
        return levels;
    }

    struct RoutableServer { int id; QString label; QString fullLabel; };

    // What generate.cpp refuses as a rule's outbound or as a hop of one: extra cores and full configs.
    bool UnroutableProfile(const std::shared_ptr<Configs::Profile>& profile)
    {
        if (!profile || !profile->outbound || profile->outbound->IsExtraCore() || profile->outbound->IsXrayFullConfig()) return true;
        const auto* custom = profile->Custom();
        return custom && custom->type == Configs::Custom::CustomFullConfig;
    }

    // Every profile a routing rule can point at, as "[group] name" with groups in tab order, like the routing editor lists them.
    // Ones the core would refuse are left out, chains included when a hop of theirs is.
    // A long group name is cut short so the server name still fits; fullLabel keeps it whole.
    QList<RoutableServer> RoutableServers()
    {
        const auto& repo = Configs::dataManager->profilesRepo;
        auto mapped = repo->GetAllProfileIDNameMapped();
        QHash<int, QString> names;
        // An unnamed profile shows as its type and address, as in the tray.
        for (const auto& [id, name] : FixProfileDisplayName(mapped)) names.insert(id, name);
        for (const int id : repo->GetProfileIdsByType("extracore")) names.remove(id);
        for (const auto& profile : repo->GetProfileBatch(repo->GetProfileIdsByType("custom")))
            if (UnroutableProfile(profile)) names.remove(profile->id);

        const auto chains = repo->GetProfileBatch(repo->GetProfileIdsByType("chain"));
        QList<int> hopIDs;
        for (const auto& profile : chains)
            if (const auto* chain = profile->Chain()) hopIDs << chain->list;
        QHash<int, std::shared_ptr<Configs::Profile>> hops;
        for (const auto& hop : repo->GetProfileBatch(hopIDs)) hops.insert(hop->id, hop);
        for (const auto& profile : chains)
        {
            const auto* chain = profile->Chain();
            const bool usable = chain && !chain->list.isEmpty()
                && std::all_of(chain->list.cbegin(), chain->list.cend(), [&hops](int hopID) {
                       const auto hop = hops.value(hopID);
                       return !UnroutableProfile(hop) && hop->type != "chain";
                   });
            if (!usable) names.remove(profile->id);
        }

        QList<RoutableServer> servers;
        for (const int groupID : Configs::dataManager->groupsRepo->GetGroupsTabOrder())
        {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(groupID);
            if (!group) continue;
            const QString shortGroup = ShortDisplayName(group->name, 20);
            for (const int profileID : group->profiles)
                if (const auto it = names.constFind(profileID); it != names.cend())
                    servers << RoutableServer{profileID, "[" + shortGroup + "] " + it.value(), "[" + group->name + "] " + it.value()};
        }
        return servers;
    }

    // Named as in the server list: an unnamed profile by its type and address.
    QString ServerName(int id)
    {
        const auto profile = Configs::dataManager->profilesRepo->GetProfile(id);
        if (!profile) return QString("#%1").arg(id);
        return profile->name.trimmed().isEmpty() && profile->outbound ? profile->outbound->DisplayTypeAndName() : profile->name;
    }

    QIcon RecolorIcon(const QString& path, const QColor& color)
    {
        QPixmap pixmap(path);
        if (pixmap.isNull()) return QIcon(path);
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();
        return QIcon(pixmap);
    }

    // Two chevrons pointing apart (expand) or together (collapse), rendered per scale so they stay crisp.
    QIcon FoldIcon(bool expand, const QColor& color)
    {
        QIcon icon;
        for (const qreal scale : {1.0, 2.0, 3.0})
        {
            QPixmap pixmap(QSize(16, 16) * scale);
            pixmap.setDevicePixelRatio(scale);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            const auto chevron = [&painter](qreal y, bool up) {
                const qreal tip = up ? -1.5 : 1.5;
                painter.drawPolyline(QPolygonF{{4.0, y - tip}, {8.0, y + tip}, {12.0, y - tip}});
            };
            chevron(4.0, expand);
            chevron(12.0, !expand);
            painter.end();
            icon.addPixmap(pixmap);
        }
        return icon;
    }

    int ColumnForSort(Stats::ConnectionSort sort)
    {
        switch (sort)
        {
        case Stats::ByProcess:       return ConnectionsTreeModel::ColTarget;
        case Stats::BySource:        return ConnectionsTreeModel::ColSource;
        case Stats::ByProtocol:      return ConnectionsTreeModel::ColProtocol;
        case Stats::ByOutbound:      return ConnectionsTreeModel::ColOutbound;
        case Stats::ByTraffic:
        case Stats::ByDownload:
        case Stats::ByUpload:        return ConnectionsTreeModel::ColTraffic;
        case Stats::BySpeed:
        case Stats::ByDownloadSpeed:
        case Stats::ByUploadSpeed:   return ConnectionsTreeModel::ColSpeed;
        default:                     return -1;
        }
    }
}

void MainWindow::setupConnectionList()
{
    connectionsModel = new ConnectionsTreeModel(this);
    connectionsFilterModel = new ConnectionsTreeFilterProxyModel(this);
    connectionsFilterModel->setSourceModel(connectionsModel);
    ui->connections->setModel(connectionsFilterModel);

    // Order matters: setModel() after this would re-init the sections and drop the resize modes below.
    connectionFilterHeader = new ConnectionsFilterHeader(ui->connections);
    ui->connections->setHeader(connectionFilterHeader);
    // QTreeView::setHeader() re-applies setSortingEnabled(false), which switches section clicks back off.
    connectionFilterHeader->setSectionsClickable(true);

    auto* header = ui->connections->header();
    header->setHighlightSections(false);
    header->setSectionResizeMode(ConnectionsTreeModel::ColTarget, QHeaderView::Stretch);
    header->setSectionResizeMode(ConnectionsTreeModel::ColSource, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ConnectionsTreeModel::ColProtocol, QHeaderView::ResizeToContents);
    // Sized by the header itself, since it has to give way to a long Target.
    header->setSectionResizeMode(ConnectionsTreeModel::ColOutbound, QHeaderView::Fixed);
    header->setSectionResizeMode(ConnectionsTreeModel::ColTraffic, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ConnectionsTreeModel::ColSpeed, QHeaderView::ResizeToContents);

    header->setResizeContentsPrecision(20);
    ui->connections->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->connections->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->connections->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->connections->setAlternatingRowColors(true);
    ui->connections->setWordWrap(false);
    ui->connections->setUniformRowHeights(true);
    ui->connections->setAnimated(false);
    // A single click already toggles a process row; QTreeView's own double-click toggle would undo it.
    ui->connections->setExpandsOnDoubleClick(false);

    refreshConnectionIcons();
    restoreConnectionSort();
    setupConnectionSortMenu();
    setupConnectionFilter();

    ui->connections->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->connections, &QWidget::customContextMenuRequested, this, &MainWindow::onConnectionContextMenu);

    connect(ui->connections, &QAbstractItemView::clicked, this, [this](const QModelIndex& index)
    {
        if (!index.data(ConnectionsTreeModel::IsProcessRole).toBool()) return;
        // Expansion is keyed on column 0, so any other cell's index always reads as collapsed.
        const QModelIndex group = index.siblingAtColumn(0);
        ui->connections->setExpanded(group, !ui->connections->isExpanded(group));
    });
    connect(ui->connections, &QTreeView::collapsed, this, [this](const QModelIndex& index)
    {
        m_processExpanded.insert(index.data(ConnectionsTreeModel::ProcessNameRole).toString(), false);
        syncConnectionExpandButton();
    });
    connect(ui->connections, &QTreeView::expanded, this, [this](const QModelIndex& index)
    {
        m_processExpanded.insert(index.data(ConnectionsTreeModel::ProcessNameRole).toString(), true);
        syncConnectionExpandButton();
    });

    connect(header, &QHeaderView::sectionClicked, this, [this](int section)
    {
        Stats::ConnectionSort sort;
        switch (section)
        {
        case ConnectionsTreeModel::ColTarget:   sort = Stats::ByProcess; break;
        case ConnectionsTreeModel::ColSource:   sort = Stats::BySource; break;
        case ConnectionsTreeModel::ColProtocol: sort = Stats::ByProtocol; break;
        case ConnectionsTreeModel::ColOutbound: sort = Stats::ByOutbound; break;
        case ConnectionsTreeModel::ColTraffic:  sort = Stats::ByTraffic; break;
        case ConnectionsTreeModel::ColSpeed:    sort = Stats::BySpeed; break;
        default: return;
        }
        // A third click on the same header falls back to the default oldest-first order.
        if (Stats::connection_lister->getSort() == sort && Stats::connection_lister->isSortAscending()) sort = Stats::Default;
        applyConnectionSort(sort);
    });

    syncConnectionSourceColumn();
}

void MainWindow::restoreConnectionSort()
{
    const auto* settings = Configs::dataManager->settingsRepo.get();
    int stored = settings->connection_sort;
    if (stored < Stats::Default || stored > Stats::BySource) return;
    // The Source header is unreachable while its column is hidden, so that sort would be stuck for good.
    if (stored == Stats::BySource && !LocalNetwork::LanInboundEnabled()) stored = Stats::Default;
    // Runs before the first core connection spawns the lister thread, so writing the pair unguarded is safe.
    Stats::connection_lister->restoreSort(static_cast<Stats::ConnectionSort>(stored), settings->connection_sort_asc);
    const auto restored = Stats::connection_lister->getSort();
    connectionFilterHeader->setSortSection(ColumnForSort(restored), Stats::SortIsDescending(restored, Stats::connection_lister->isSortAscending()));
}

void MainWindow::applyConnectionSort(Stats::ConnectionSort sort)
{
    Stats::connection_lister->setSort(sort);
    auto* settings = Configs::dataManager->settingsRepo.get();
    settings->connection_sort = Stats::connection_lister->getSort();
    settings->connection_sort_asc = Stats::connection_lister->isSortAscending();
    settings->Save();
    Stats::connection_lister->ForceUpdate();

    const auto applied = Stats::connection_lister->getSort();
    connectionFilterHeader->setSortSection(ColumnForSort(applied), Stats::SortIsDescending(applied, Stats::connection_lister->isSortAscending()));
}

void MainWindow::setupConnectionFilter()
{
    auto* btnFilter = new QToolButton(this);
    btnFilter->setIcon(QIcon(":/icon/filter.png"));
    btnFilter->setToolTip(tr("Enable Filter"));
    btnFilter->setCheckable(true);
    connect(btnFilter, &QToolButton::toggled, connectionFilterHeader, &ConnectionsFilterHeader::setFiltersVisible);
    connect(connectionFilterHeader, &ConnectionsFilterHeader::closeRequested, btnFilter, [btnFilter] { btnFilter->setChecked(false); });

    connectionExpandButton = new QToolButton(this);
    connect(connectionExpandButton, &QToolButton::clicked, this, [this] { setConnectionGroupsExpanded(!connectionGroupsExpanded()); });

    connectionCloseAllButton = new QToolButton(this);
    connectionCloseAllButton->setIcon(connectionCloseIcon);
    connectionCloseAllButton->setToolTip(tr("Close every connection listed below"));
    connect(connectionCloseAllButton, &QToolButton::clicked, this, [this] { closeConnections(listedConnectionIds()); });

    auto buttonGroup = [this](std::initializer_list<QWidget*> buttons) {
        auto* group = new QWidget(this);
        auto* layout = new QHBoxLayout(group);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        for (auto* button : buttons) layout->addWidget(button);
        return group;
    };
    auto* connectionButtons = buttonGroup({btnFilter, connectionExpandButton, connectionCloseAllButton});
    // A tab widget has one top-right corner, so the Logs tab's buttons share it.
    auto* logButtons = buttonGroup({logJumpLatestButton, logFilterButton});
    auto* corner = buttonGroup({logButtons, connectionButtons});
    ui->stats_widget->setCornerWidget(corner, Qt::TopRightCorner);

    auto syncCorner = [=,this] {
        const auto* current = ui->stats_widget->currentWidget();
        connectionButtons->setVisible(current == ui->connections_tab);
        logButtons->setVisible(current == ui->Logs);
        corner->setVisible(current == ui->connections_tab || current == ui->Logs);
    };
    connect(ui->stats_widget, &QTabWidget::currentChanged, this, [syncCorner](int) { syncCorner(); });
    syncCorner();

    connectionFilterDebounce = new QTimer(this);
    connectionFilterDebounce->setSingleShot(true);
    connectionFilterDebounce->setInterval(50);
    connect(connectionFilterDebounce, &QTimer::timeout, this, [this] { applyConnectionFilters(); });
    connect(connectionFilterHeader, &ConnectionsFilterHeader::filtersChanged, this, [this] { connectionFilterDebounce->start(); });

    syncConnectionExpandButton();
}

void MainWindow::applyConnectionFilters()
{
    const auto filters = connectionFilterHeader->filters();
    connectionsFilterModel->setFilters(filters.source, filters.target, filters.protocol, filters.outbound);
    syncConnectionExpansion();
}

void MainWindow::syncConnectionSourceColumn()
{
    if (connectionsModel == nullptr) return;
    const bool show = LocalNetwork::LanInboundEnabled();
    if (ui->connections->isColumnHidden(ConnectionsTreeModel::ColSource) == !show) return;

    ui->connections->setColumnHidden(ConnectionsTreeModel::ColSource, !show);
    connectionFilterHeader->adjustPositions();
    // Both must be cleared here: a hidden header can be reached by neither the filter field nor a sort click.
    if (!show) {
        connectionFilterHeader->clearFilterFor(ConnectionsTreeModel::ColSource);
        if (Stats::connection_lister->getSort() == Stats::BySource) applyConnectionSort(Stats::Default);
    }
    applyConnectionFilters();
}

void MainWindow::setupConnectionSortMenu()
{
    auto* header = ui->connections->header();
    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QWidget::customContextMenuRequested, this, [=,this](const QPoint& pos)
    {
        const int columnIndex = header->logicalIndexAt(pos);
        const bool isTraffic = columnIndex == ConnectionsTreeModel::ColTraffic;
        const bool isSpeed = columnIndex == ConnectionsTreeModel::ColSpeed;
        if (!isTraffic && !isSpeed) return;

        struct SortOption { Stats::ConnectionSort value; QString label; };
        const QList<SortOption> options = isTraffic
            ? QList<SortOption>{
                { Stats::ByTraffic, tr("Total") },
                { Stats::ByDownload, tr("Downloaded") },
                { Stats::ByUpload, tr("Uploaded") } }
            : QList<SortOption>{
                { Stats::BySpeed, tr("Total") },
                { Stats::ByDownloadSpeed, tr("Download Speed") },
                { Stats::ByUploadSpeed, tr("Upload Speed") } };

        QMenu menu(this);
        auto* sortByLabel = menu.addAction(tr("Sort By:"));
        sortByLabel->setEnabled(false);

        const auto current = Stats::connection_lister->getSort();
        for (const auto& opt : options)
        {
            auto* act = menu.addAction(opt.label);
            act->setData(static_cast<int>(opt.value));
            act->setCheckable(true);
            act->setChecked(current == opt.value);
        }

        auto* chosen = menu.exec(header->mapToGlobal(pos));
        if (chosen == nullptr || !chosen->data().isValid()) return;

        applyConnectionSort(static_cast<Stats::ConnectionSort>(chosen->data().toInt()));
    });
}

void MainWindow::refreshConnectionIcons()
{
    const QColor color = palette().color(QPalette::ButtonText);
    connectionCloseIcon = RecolorIcon(":/icon/material/cancel.png", color);
    connectionExpandIcon = FoldIcon(true, color);
    connectionCollapseIcon = FoldIcon(false, color);
    if (connectionCloseAllButton != nullptr) connectionCloseAllButton->setIcon(connectionCloseIcon);
    syncConnectionExpandButton();
}

QStringList MainWindow::listedConnectionIds() const
{
    QStringList ids;
    const int groups = connectionsFilterModel->rowCount();
    for (int row = 0; row < groups; row++)
    {
        const QModelIndex group = connectionsFilterModel->index(row, 0);
        const int leaves = connectionsFilterModel->rowCount(group);
        for (int leaf = 0; leaf < leaves; leaf++)
            ids << connectionsFilterModel->index(leaf, 0, group).data(ConnectionsTreeModel::ConnIdsRole).toStringList();
    }
    return ids;
}

void MainWindow::closeConnections(const QStringList& ids)
{
    Engine::StatsService::closeConnections(ids);
}

void MainWindow::UpdateConnectionList(const QList<Stats::ConnectionMetadata>& connections)
{
    if (connectionsModel == nullptr) return;
    connectionsModel->setConnections(connections, Stats::connection_lister->getSort(), Stats::connection_lister->isSortAscending());
    syncConnectionExpansion();
    for (const int column : {ConnectionsTreeModel::ColTraffic, ConnectionsTreeModel::ColSpeed})
        connectionFilterHeader->growSection(column);
}

void MainWindow::syncConnectionExpansion()
{
    {
        // Blocked so the expanded/collapsed handlers only ever record the user's own choices.
        const QSignalBlocker blocker(ui->connections);
        // Rows keep their expansion across polls; only rows new to the view (or re-shown by a filter) arrive collapsed.
        for (int row = 0; row < connectionsFilterModel->rowCount(); row++)
        {
            const QModelIndex group = connectionsFilterModel->index(row, 0);
            const QString process = group.data(ConnectionsTreeModel::ProcessNameRole).toString();
            const bool expand = m_processExpanded.value(process, m_processesExpandedByDefault);
            if (ui->connections->isExpanded(group) != expand) ui->connections->setExpanded(group, expand);
        }
    }
    syncConnectionExpandButton();
}

void MainWindow::setConnectionGroupsExpanded(bool expanded)
{
    // Also decides how processes that show up later start out, until one is toggled by hand.
    m_processesExpandedByDefault = expanded;
    m_processExpanded.clear();
    {
        const QSignalBlocker blocker(ui->connections);
        if (expanded) ui->connections->expandAll();
        else ui->connections->collapseAll();
    }
    syncConnectionExpandButton();
}

bool MainWindow::connectionGroupsExpanded() const
{
    const int groups = connectionsFilterModel->rowCount();
    // With nothing listed, the button shows what the next processes will do.
    if (groups == 0) return m_processesExpandedByDefault;
    for (int row = 0; row < groups; row++)
        if (ui->connections->isExpanded(connectionsFilterModel->index(row, 0))) return true;
    return false;
}

void MainWindow::syncConnectionExpandButton()
{
    if (connectionExpandButton == nullptr) return;
    const bool expanded = connectionGroupsExpanded();
    connectionExpandButton->setIcon(expanded ? connectionCollapseIcon : connectionExpandIcon);
    connectionExpandButton->setToolTip(expanded ? tr("Collapse All") : tr("Expand All"));
}

QString MainWindow::routeRuleAppendBlocker() const
{
    const auto& dm = Configs::dataManager;
    const auto currentRoute = dm->routesRepo->GetRouteProfile(dm->settingsRepo->current_route_id);
    if (!currentRoute) return tr("No active routing profile found.");
    if (currentRoute->preventModifications) return tr("The current routing profile is locked against modifications.");
    if (currentRoute->isRaw) return tr("The current routing profile is raw JSON.");
    if (currentRoute->isRemote && currentRoute->autoUpdate) return tr("The current routing profile auto-updates from a URL.");
    return {};
}

MainWindow::RuleToggle MainWindow::toggleRuleInCurrentRoute(const QString& rawRule, const Configs::SimpleTarget& target)
{
    const auto nameOf = [](const Configs::SimpleTarget& t) {
        return t.isServer() ? ServerName(t.serverID) : Configs::simpleActionToString(t.action);
    };
    auto fail = [this](const QString& msg) {
        MW_show_log(msg);
        return RuleToggle::Failed;
    };

    if (const auto blocker = routeRuleAppendBlocker(); !blocker.isEmpty()) return fail(blocker);

    const auto& dm = Configs::dataManager;
    const auto currentRoute = dm->routesRepo->GetRouteProfile(dm->settingsRepo->current_route_id);
    if (!currentRoute) return fail(tr("No active routing profile found."));

    const QString name = nameOf(target);
    RuleToggle result;
    QString log;
    if (currentRoute->HasSimpleRule(rawRule, target))
    {
        currentRoute->RemoveSimpleRule(rawRule, target);
        result = RuleToggle::Removed;
        log = tr("Removed %1 from the %2 rules of \"%3\"").arg(rawRule, name, currentRoute->name);
    }
    else
    {
        if (!currentRoute->AppendSimpleRule(rawRule, target))
            return fail(tr("Failed to add routing rule: %1").arg(rawRule));

        // With one target in two lists the earlier rule silently wins, so taking it here pulls it out of the others.
        QList<Configs::SimpleTarget> others;
        for (const auto action : {Configs::bypass, Configs::proxy, Configs::block, Configs::warpBypass})
            others << Configs::SimpleTarget{action};
        for (const int server : currentRoute->ServersHoldingSimpleRule(rawRule))
            others << Configs::SimpleTarget{Configs::proxy, server};
        QStringList movedFrom;
        for (const auto& other : others)
            if (other != target && currentRoute->RemoveSimpleRule(rawRule, other))
                movedFrom << nameOf(other);

        result = movedFrom.isEmpty() ? RuleToggle::Added : RuleToggle::Moved;
        log = movedFrom.isEmpty()
                  ? tr("Appended %1 to the %2 rules of \"%3\"").arg(rawRule, name, currentRoute->name)
                  : tr("Moved %1 from the %2 to the %3 rules of \"%4\"")
                        .arg(rawRule, movedFrom.join(", "), name, currentRoute->name);
    }

    if (!dm->routesRepo->Save(currentRoute))
        return fail(tr("Failed to save routing rule: %1").arg(rawRule));

    MW_show_log(log);
    connection()->noteRestartNeeded(Engine::RestartReason::Routing);
    return result;
}

void MainWindow::onConnectionContextMenu(const QPoint& pos)
{
    QMenu menu(this);
    const QPoint globalPos = ui->connections->viewport()->mapToGlobal(pos);
    auto addExpandActions = [this, &menu] {
        connect(menu.addAction(tr("Expand All")), &QAction::triggered, this, [this] { setConnectionGroupsExpanded(true); });
        connect(menu.addAction(tr("Collapse All")), &QAction::triggered, this, [this] { setConnectionGroupsExpanded(false); });
    };

    const QModelIndex proxyIndex = ui->connections->indexAt(pos);
    if (!proxyIndex.isValid())
    {
        addExpandActions();
        menu.exec(globalPos);
        return;
    }

    const QModelIndex sourceIndex = connectionsFilterModel->mapToSource(proxyIndex);
    ui->connections->setCurrentIndex(proxyIndex);

    auto showTip = [this](const QString& text) {
        QToolTip::showText(QCursor::pos(), text, this);
        auto r = ++toolTipID;
        QTimer::singleShot(2000, this, [=, this] {
            if (r == toolTipID) QToolTip::hideText();
        });
    };

    struct RouteAction { Configs::simpleAction action; QString label; bool offered = true; };
    const RouteAction routeActions[] = {
        { Configs::bypass,     tr("Direct") },
        { Configs::proxy,      tr("Proxy") },
        { Configs::block,      tr("Block") },
        // Not offered from here, but a target already sitting in its list still has to show up as taken.
        { Configs::warpBypass, tr("Warp-bypass"), false },
    };

    // Looked up once per menu: the dialog asks for a server's name on every refresh.
    QHash<int, QString> serverNames;
    const auto targetLabel = [&routeActions, &serverNames](const Configs::SimpleTarget& t) {
        if (t.isServer())
        {
            auto it = serverNames.find(t.serverID);
            if (it == serverNames.end()) it = serverNames.insert(t.serverID, ServerName(t.serverID));
            return it.value();
        }
        for (const auto& ra : routeActions)
            if (ra.action == t.action) return ra.label;
        return QString();
    };

    // The menu only picks the action; the targets are chosen in a dialog that opens once the menu has closed.
    struct RouteTarget { QString label; QString rule; bool separatorBefore = false; };
    QList<RouteTarget> targets;
    const RouteAction* pickedAction = nullptr;
    bool pickedServer = false;

    auto addRouteSection = [&] {
        if (targets.isEmpty()) return;

        const QString blocker = routeRuleAppendBlocker();
        const auto& dm = Configs::dataManager;
        const auto currentRoute = blocker.isEmpty() ? dm->routesRepo->GetRouteProfile(dm->settingsRepo->current_route_id) : nullptr;

        auto* header = menu.addAction(currentRoute ? tr("Modify rules in \"%1\"").arg(currentRoute->name) : tr("Modify rules"));
        header->setEnabled(false);
        header->setToolTip(blocker);

        for (const auto& ra : routeActions)
        {
            if (!ra.offered) continue;
            auto* act = menu.addAction(ra.label);
            if (!currentRoute)
            {
                act->setEnabled(false);
                act->setToolTip(blocker);
                continue;
            }
            connect(act, &QAction::triggered, this, [&pickedAction, &ra] { pickedAction = &ra; });
        }

        // The server is picked in the dialog, where a search box copes with subscriptions of thousands.
        // Listing them waits for the dialog too, so a right click stays quick with a big subscription.
        auto* serverAct = menu.addAction(tr("Specific server"));
        if (!currentRoute)
        {
            serverAct->setEnabled(false);
            serverAct->setToolTip(blocker);
        }
        else
        {
            serverAct->setToolTip(tr("Route through another server"));
            connect(serverAct, &QAction::triggered, this, [&pickedServer] { pickedServer = true; });
        }
        menu.addSeparator();
    };

    // One checkbox per target, checked where the target already sits in the action's rules.
    // Only the boxes the user flipped are applied: checking adds (or moves from another list), unchecking removes.
    // Without an action the dialog routes to a server picked in it, and switching servers resets the boxes to its rules.
    auto showTargetsDialog = [&](const RouteAction* ra) {
        const QString blocker = routeRuleAppendBlocker();
        if (!blocker.isEmpty())
        {
            MW_show_log(blocker);
            return;
        }
        const auto& dm = Configs::dataManager;
        const auto currentRoute = dm->routesRepo->GetRouteProfile(dm->settingsRepo->current_route_id);
        if (!currentRoute) return;

        const auto servers = ra ? QList<RoutableServer>{} : RoutableServers();
        if (!ra && servers.isEmpty())
        {
            showTip(tr("There are no servers to route to."));
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(ra ? tr("%1 rules").arg(ra->label) : tr("Specific server rules"));
        auto* layout = new QVBoxLayout(&dialog);
        auto* hint = new QLabel(ra ? tr("Routing profile \"%1\". Checked targets go %2; unchecking one removes its rule.")
                                         .arg(currentRoute->name, ra->label)
                                   : tr("Routing profile \"%1\". Checked targets go through the chosen server; unchecking one removes its rule.")
                                         .arg(currentRoute->name), &dialog);
        hint->setWordWrap(true);
        layout->addWidget(hint);

        QComboBox* serverBox = nullptr;
        if (!ra)
        {
            auto* serverLabel = new QLabel(tr("Server (type part of its name to search):"), &dialog);
            layout->addWidget(serverLabel);
            serverBox = new QComboBox(&dialog);
            serverLabel->setBuddy(serverBox);
            serverBox->setEditable(true);
            serverBox->setInsertPolicy(QComboBox::NoInsert);
            // Sized to the longest name otherwise, which can stretch the dialog across the screen.
            serverBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            serverBox->setMinimumContentsLength(30);
            serverBox->completer()->setFilterMode(Qt::MatchContains);
            serverBox->completer()->setCompletionMode(QCompleter::PopupCompletion);
            serverBox->lineEdit()->setPlaceholderText(tr("Search servers"));
            for (const auto& server : servers)
            {
                serverBox->addItem(server.label, server.id);
                if (server.fullLabel != server.label) serverBox->setItemData(serverBox->count() - 1, server.fullLabel, Qt::ToolTipRole);
            }
            // Opens on a listed server that already holds one of the targets.
            int holderIndex = -1;
            for (qsizetype i = 0; i < targets.size() && holderIndex < 0; ++i)
                for (const int server : currentRoute->ServersHoldingSimpleRule(targets[i].rule))
                    if ((holderIndex = serverBox->findData(server)) >= 0) break;
            if (holderIndex >= 0) serverBox->setCurrentIndex(holderIndex);
            layout->addWidget(serverBox);
        }
        const auto chosen = [&]() -> Configs::SimpleTarget {
            if (ra) return {ra->action};
            return {Configs::proxy, serverBox->currentIndex() < 0 ? Configs::INVALID_ID : serverBox->currentData().toInt()};
        };

        QList<QPair<QCheckBox*, bool>> boxes;
        for (const auto& target : targets)
        {
            if (target.separatorBefore)
            {
                auto* line = new QFrame(&dialog);
                line->setFrameShape(QFrame::HLine);
                line->setFrameShadow(QFrame::Sunken);
                layout->addWidget(line);
            }
            auto* box = new QCheckBox(&dialog);
            layout->addWidget(box);
            boxes << qMakePair(box, false);
        }

        const auto refresh = [&] {
            const auto t = chosen();
            const QString label = targetLabel(t);
            for (qsizetype i = 0; i < boxes.size(); ++i)
            {
                const auto& target = targets[i];
                const bool here = currentRoute->HasSimpleRule(target.rule, t);
                QStringList elsewhere;
                for (const auto& other : routeActions)
                    if (Configs::SimpleTarget{other.action} != t && currentRoute->HasSimpleRule(target.rule, {other.action}))
                        elsewhere << other.label;
                for (const int server : currentRoute->ServersHoldingSimpleRule(target.rule))
                    if (const Configs::SimpleTarget other{Configs::proxy, server}; other != t) elsewhere << targetLabel(other);

                QStringList notes;
                QStringList tips{target.rule};
                if (!elsewhere.isEmpty())
                {
                    notes << tr("in %1").arg(elsewhere.join(", "));
                    tips << tr("Checking it moves the rule from %1 to %2").arg(elsewhere.join(", "), label);
                }

                // A broader rule of another action matched first would leave this one dead, so say which.
                Configs::SimpleTarget coveringTarget;
                if (const QString covering = currentRoute->CoveringSimpleRule(target.rule, t, &coveringTarget); !covering.isEmpty())
                {
                    const QString value = covering.section(':', 1);
                    const QString coveringLabel = covering.startsWith("keyword:") ? "*" + value + "*" : "*." + value;
                    const QString coveringList = targetLabel(coveringTarget);
                    notes << tr("covered by %1 in %2").arg(coveringLabel, coveringList);
                    tips << tr("%1 in %2 is matched first, so this rule would never apply").arg(covering, coveringList);
                }

                auto* box = boxes[i].first;
                box->setText(notes.isEmpty() ? target.label : tr("%1  (%2)").arg(target.label, notes.join("; ")));
                box->setChecked(here);
                box->setToolTip(tips.join('\n'));
                boxes[i].second = here;
            }
        };
        refresh();

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        if (serverBox)
        {
            connect(serverBox, &QComboBox::currentIndexChanged, &dialog, refresh);
            // Enter in the search field would otherwise press OK and apply the boxes to the server still selected.
            // Cancel too, or the dialog makes it the default once OK is not.
            for (auto* button : buttons->buttons())
                if (auto* push = qobject_cast<QPushButton*>(button)) push->setAutoDefault(false);
            // A search left half-typed would show one server while the boxes belong to another.
            connect(serverBox->lineEdit(), &QLineEdit::editingFinished, &dialog, [serverBox] {
                if (serverBox->currentIndex() >= 0 && serverBox->currentText() != serverBox->itemText(serverBox->currentIndex()))
                    serverBox->setEditText(serverBox->itemText(serverBox->currentIndex()));
            });
        }
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        layout->setSizeConstraint(QLayout::SetFixedSize);
        if (dialog.exec() != QDialog::Accepted) return;

        const auto t = chosen();
        if (!ra && !t.isServer()) return;
        QStringList changes;
        for (qsizetype i = 0; i < boxes.size(); ++i)
        {
            if (boxes[i].first->isChecked() == boxes[i].second) continue;
            const QString& rule = targets[i].rule;
            switch (toggleRuleInCurrentRoute(rule, t))
            {
                case RuleToggle::Added: changes << tr("Added %1").arg(rule); break;
                case RuleToggle::Moved: changes << tr("Moved %1").arg(rule); break;
                case RuleToggle::Removed: changes << tr("Removed %1").arg(rule); break;
                case RuleToggle::Failed: break;
            }
        }
        if (!changes.isEmpty()) showTip(tr("%1 rules:\n%2").arg(targetLabel(t), changes.join('\n')));
    };

    auto addCopyAction = [&](const QString& label, const QString& text) {
        connect(menu.addAction(label), &QAction::triggered, this, [text, showTip] {
            QApplication::clipboard()->setText(text);
            showTip(tr("Copied: %1").arg(text));
        });
    };

    auto addCloseAction = [&](const QString& label, const QStringList& ids) {
        connect(menu.addAction(label), &QAction::triggered, this, [this, ids] { closeConnections(ids); });
    };

    menu.setToolTipsVisible(true);

    const QString process = connectionsModel->processNameAt(sourceIndex);
    const QStringList ids = connectionsModel->connectionIdsAt(sourceIndex);
    if (const auto* meta = connectionsModel->metaAt(sourceIndex))
    {
        const QString domain = meta->domain.trimmed();
        const QString host = domain.isEmpty() ? Stats::EndpointHost(meta->dest.trimmed()) : domain;

        if (!host.isEmpty() && QHostAddress(host).isNull())
        {
            // Every level is a domain_suffix rule and so also covers whatever sits in front of it:
            // the leading "*." in the label says so, but never reaches the rule itself.
            // The bare TLD sits apart, since it reroutes a whole zone.
            // Lowercase, because sing-box lowercases the host it matches but takes rule values as written.
            const auto levels = DomainLevels(host.toLower());
            for (qsizetype i = 0; i < levels.size(); ++i)
                targets << RouteTarget{ "*." + levels[i], "suffix:" + levels[i], i > 0 && i == levels.size() - 1 };
            // The name in front of the TLD as a keyword rule, which also catches the service's other domains (githubusercontent.com).
            // Names under 4 letters are left out: co in bbc.co.uk or vk would catch far too much.
            if (levels.size() > 1)
            {
                const QString name = levels[levels.size() - 2].section('.', 0, 0);
                if (name.size() >= 4) targets.insert(targets.size() - 1, RouteTarget{ "*" + name + "*", "keyword:" + name });
            }
        }
        else if (!host.isEmpty())
        {
            targets << RouteTarget{ host, "ip:" + host };
        }
        if (!process.isEmpty()) targets << RouteTarget{ tr("Process %1").arg(process), "processName:" + process, true };
        addRouteSection();

        if (!host.isEmpty()) addCopyAction(tr("Copy Destination (%1)").arg(host), host);
        if (!process.isEmpty()) addCopyAction(tr("Copy Process Name (%1)").arg(process), process);

        menu.addSeparator();
        if (!ids.isEmpty())
            addCloseAction(ids.size() > 1 ? tr("Close all connections (%1)").arg(ids.size()) : tr("Close connection"), ids);
    }
    else
    {
        if (!process.isEmpty())
        {
            targets << RouteTarget{ tr("Process %1").arg(process), "processName:" + process };
            addRouteSection();
            addCopyAction(tr("Copy Process Name"), process);
        }

        menu.addSeparator();
        if (!ids.isEmpty())
        {
            addCloseAction(tr("Close all connections for \"%1\" (%2)")
                               .arg(ConnectionsTreeModel::displayProcessName(process), QString::number(ids.size())), ids);
        }
    }

    menu.addSeparator();
    addExpandActions();
    menu.exec(globalPos);
    if (pickedAction) showTargetsDialog(pickedAction);
    else if (pickedServer) showTargetsDialog(nullptr);
}
