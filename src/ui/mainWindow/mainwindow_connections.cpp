#include "include/ui/mainwindow.h"
#include "include/api/RPC.h"
#include "include/database/entities/RouteProfile.h"
#include "include/database/RoutesRepo.h"
#include "include/database/SettingsRepo.h"
#include "include/global/LocalNetwork.hpp"
#include "include/ui/utils/ConnectionsFilterHeader.h"
#include "include/ui/utils/ConnectionsTreeFilterProxyModel.h"
#include "include/ui/utils/ConnectionsTreeModel.h"

#include <QHostAddress>

#include <QAbstractItemView>
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QHeaderView>
#include <QIcon>
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
    header->setSectionResizeMode(ConnectionsTreeModel::ColOutbound, QHeaderView::ResizeToContents);
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
    // Runs before setup_rpc() spawns the lister thread, so writing the pair unguarded is safe.
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

    auto* corner = new QWidget(this);
    auto* cornerLayout = new QHBoxLayout(corner);
    cornerLayout->setContentsMargins(0, 0, 0, 0);
    cornerLayout->setSpacing(2);
    cornerLayout->addWidget(btnFilter);
    cornerLayout->addWidget(connectionExpandButton);
    cornerLayout->addWidget(connectionCloseAllButton);
    ui->stats_widget->setCornerWidget(corner, Qt::TopRightCorner);

    auto syncCorner = [=,this] { corner->setVisible(ui->stats_widget->currentWidget() == ui->connections_tab); };
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
    if (ids.isEmpty()) return;
    runOnNewThread([ids] {
        bool rpcOK = false;
        const auto err = API::defaultClient->CloseConnections(&rpcOK, ids);
        if (!rpcOK || !err.isEmpty())
        {
            MW_show_log(tr("Failed to close connections: %1").arg(err.isEmpty() ? tr("IPC error") : err));
            return;
        }
        Stats::connection_lister->ForceUpdate();
    });
}

void MainWindow::UpdateConnectionList(const QList<Stats::ConnectionMetadata>& connections)
{
    if (connectionsModel == nullptr) return;
    connectionsModel->setConnections(connections, Stats::connection_lister->getSort(), Stats::connection_lister->isSortAscending());
    syncConnectionExpansion();
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

MainWindow::RuleToggle MainWindow::toggleRuleInCurrentRoute(const QString& rawRule, Configs::simpleAction action)
{
    auto fail = [this](const QString& msg) {
        MW_show_log(msg);
        return RuleToggle::Failed;
    };

    if (const auto blocker = routeRuleAppendBlocker(); !blocker.isEmpty()) return fail(blocker);

    const auto& dm = Configs::dataManager;
    const auto currentRoute = dm->routesRepo->GetRouteProfile(dm->settingsRepo->current_route_id);
    if (!currentRoute) return fail(tr("No active routing profile found."));

    const QString target = Configs::simpleActionToString(action);
    RuleToggle result;
    QString log;
    if (currentRoute->HasSimpleRule(rawRule, action))
    {
        currentRoute->RemoveSimpleRule(rawRule, action);
        result = RuleToggle::Removed;
        log = tr("Removed %1 from the %2 rules of \"%3\"").arg(rawRule, target, currentRoute->name);
    }
    else
    {
        if (!currentRoute->AppendSimpleRule(rawRule, action))
            return fail(tr("Failed to add routing rule: %1").arg(rawRule));

        // With one target in two lists the earlier rule silently wins, so taking it here pulls it out of the others.
        QStringList movedFrom;
        for (const auto other : {Configs::bypass, Configs::proxy, Configs::block, Configs::warpBypass})
            if (other != action && currentRoute->RemoveSimpleRule(rawRule, other))
                movedFrom << Configs::simpleActionToString(other);

        result = movedFrom.isEmpty() ? RuleToggle::Added : RuleToggle::Moved;
        log = movedFrom.isEmpty()
                  ? tr("Appended %1 to the %2 rules of \"%3\"").arg(rawRule, target, currentRoute->name)
                  : tr("Moved %1 from the %2 to the %3 rules of \"%4\"")
                        .arg(rawRule, movedFrom.join(", "), target, currentRoute->name);
    }

    if (!dm->routesRepo->Save(currentRoute))
        return fail(tr("Failed to save routing rule: %1").arg(rawRule));

    MW_show_log(log);
    noteRestartNeeded(tr("Routing"));
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

    // The action is picked first and the target second, so every target stays two clicks away however many there are.
    struct RouteTarget { QString label; QString rule; bool separatorBefore = false; };
    QList<RouteTarget> targets;

    auto addRouteSection = [&] {
        if (targets.isEmpty()) return;

        const QString blocker = routeRuleAppendBlocker();
        const auto& dm = Configs::dataManager;
        const auto currentRoute = blocker.isEmpty() ? dm->routesRepo->GetRouteProfile(dm->settingsRepo->current_route_id) : nullptr;

        auto* header = menu.addAction(currentRoute ? tr("Add rule to \"%1\"").arg(currentRoute->name) : tr("Add rule"));
        header->setEnabled(false);
        header->setToolTip(blocker);

        for (const auto& ra : routeActions)
        {
            if (!ra.offered) continue;
            auto* sub = menu.addMenu(ra.label);
            if (!currentRoute)
            {
                sub->setEnabled(false);
                sub->menuAction()->setToolTip(blocker);
                continue;
            }
            sub->setToolTipsVisible(true);

            for (const auto& target : targets)
            {
                if (target.separatorBefore) sub->addSeparator();
                auto* act = sub->addAction(target.label);

                const bool here = currentRoute->HasSimpleRule(target.rule, ra.action);
                QStringList elsewhere;
                for (const auto& other : routeActions)
                    if (other.action != ra.action && currentRoute->HasSimpleRule(target.rule, other.action))
                        elsewhere << other.label;

                if (!elsewhere.isEmpty()) act->setText(tr("%1  (in %2)").arg(target.label, elsewhere.join(", ")));
                act->setCheckable(here);
                act->setChecked(here);
                if (here)
                    act->setToolTip(tr("Already in the %1 rules, click to remove it").arg(ra.label));
                else if (!elsewhere.isEmpty())
                    act->setToolTip(tr("Moves the rule from %1 to %2").arg(elsewhere.join(", "), ra.label));

                connect(act, &QAction::triggered, this, [this, target, ra, showTip] {
                    switch (toggleRuleInCurrentRoute(target.rule, ra.action))
                    {
                        case RuleToggle::Added: showTip(tr("Appended to the %1 rules:\n%2").arg(ra.label, target.rule)); break;
                        case RuleToggle::Moved: showTip(tr("Moved to the %1 rules:\n%2").arg(ra.label, target.rule)); break;
                        case RuleToggle::Removed: showTip(tr("Removed from the %1 rules:\n%2").arg(ra.label, target.rule)); break;
                        case RuleToggle::Failed: break;
                    }
                });
            }
        }
        menu.addSeparator();
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
}
