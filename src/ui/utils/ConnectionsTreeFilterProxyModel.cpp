#include "include/ui/utils/ConnectionsTreeFilterProxyModel.h"
#include "include/ui/utils/ConnectionsTreeModel.h"

#include <algorithm>

ConnectionsTreeFilterProxyModel::ConnectionsTreeFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
}

ConnectionsTreeModel *ConnectionsTreeFilterProxyModel::treeModel() const {
    return qobject_cast<ConnectionsTreeModel *>(sourceModel());
}

bool ConnectionsTreeFilterProxyModel::hasActiveFilter() const {
    return !m_source.isEmpty() || !m_target.isEmpty() || !m_protocol.isEmpty() || !m_outbound.isEmpty();
}

void ConnectionsTreeFilterProxyModel::setFilters(const QString &source, const QString &target,
                                                 const QString &protocol, const QString &outbound) {
    if (m_source == source && m_target == target && m_protocol == protocol && m_outbound == outbound) return;
    m_source = source;
    m_target = target;
    m_protocol = protocol;
    m_outbound = outbound;
    invalidateRowsFilter();
}

bool ConnectionsTreeFilterProxyModel::leafMatches(const ConnectionsTree::ProcessGroupItem *group,
                                                  const ConnectionsTree::ConnectionLeafItem *leaf) const {
    if (!m_source.isEmpty() && !leaf->meta.sourceDisplay.contains(m_source, Qt::CaseInsensitive)) return false;
    if (!m_protocol.isEmpty() && !leaf->protocolText.contains(m_protocol, Qt::CaseInsensitive)) return false;
    if (!m_outbound.isEmpty() && !leaf->meta.outboundName.contains(m_outbound, Qt::CaseInsensitive)
        && !leaf->meta.outbound.contains(m_outbound, Qt::CaseInsensitive)) return false;
    return m_target.isEmpty() || leaf->destText.contains(m_target, Qt::CaseInsensitive)
           || ConnectionsTreeModel::displayProcessName(group->processName).contains(m_target, Qt::CaseInsensitive);
}

bool ConnectionsTreeFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    if (!hasActiveFilter()) return true;
    const auto *model = treeModel();
    if (model == nullptr) return true;

    const auto *group = model->groupAt(sourceParent.isValid() ? sourceParent.row() : sourceRow);
    if (group == nullptr) return false;
    if (!sourceParent.isValid()) {
        return std::any_of(group->children.begin(), group->children.end(),
                           [&](const auto &leaf) { return leafMatches(group, leaf.get()); });
    }
    return sourceRow >= 0 && sourceRow < static_cast<int>(group->children.size())
           && leafMatches(group, group->children[sourceRow].get());
}
