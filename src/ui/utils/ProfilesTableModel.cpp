#include "include/ui/utils/ProfilesTableModel.h"
#include "include/global/Configs.hpp"
#include "include/database/entities/Profile.h"
#include "include/configs/common/Outbound.h"
#include "include/stats/traffic/TrafficData.hpp"
#include <QApplication>
#include <QPalette>

#include "include/database/ProfilesRepo.h"

ProfilesTableModel::ProfilesTableModel(QObject *parent)
    : QAbstractTableModel(parent) {}

int ProfilesTableModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_profileIds.size();
}

int ProfilesTableModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return 5;
}

Qt::ItemFlags ProfilesTableModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags defaultFlags = QAbstractTableModel::flags(index);
    if (index.isValid()) {
        return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
    }
    // Allow dropping on empty space (to move to the end)
    return Qt::ItemIsDropEnabled | defaultFlags;
}

void ProfilesTableModel::ensureCached(int profileId) const {
    if (m_cache.contains(profileId)) {
        m_cache[profileId].lastAccessed = ++m_accessCounter;
        for (int i = 0; i < m_lruOrder.size(); ++i) {
            if (m_lruOrder[i] == profileId) {
                m_lruOrder.move(i, m_lruOrder.size() - 1);
                break;
            }
        }
        return;
    }

    auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId);
    if (!profile) return;

    const int startedId = Configs::dataManager->settingsRepo->started_id;
    const bool isRunning = (profile->id == startedId);
    QColor linkColor = isRunning ? QApplication::palette().link().color() : QColor();

    CachedRow row;
    row.type = profile->outbound ? profile->outbound->DisplayType() : QString();
    row.address = profile->outbound ? profile->outbound->DisplayAddress() : QString();
    row.name = profile->outbound ? profile->outbound->name : QString();
    if (profile->full_test_report.isEmpty()) {
        row.testResult = profile->DisplayTestResult();
        row.latencyColor = profile->DisplayLatencyColor();
    } else {
        row.testResult = profile->full_test_report;
        row.latencyColor = QColor();
    }
    row.traffic = profile->traffic_data ? profile->traffic_data->DisplayTraffic() : QString();
    row.foreground = isRunning ? linkColor : QColor();
    row.lastAccessed = ++m_accessCounter;

    while (m_cache.size() >= m_cacheSize && !m_lruOrder.isEmpty()) {
        evictOne();
    }
    m_cache[profileId] = row;
    m_lruOrder.append(profileId);
}

void ProfilesTableModel::evictOne() const {
    if (m_lruOrder.isEmpty()) return;
    int id = m_lruOrder.takeFirst();
    m_cache.remove(id);
}

QVariant ProfilesTableModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_profileIds.size()
        || index.column() < 0 || index.column() >= 5) {
        return QVariant();
    }
    const int profileId = m_profileIds[index.row()];
    if (role == ProfileIdRole) {
        return profileId;
    }
    ensureCached(profileId);
    auto it = m_cache.constFind(profileId);
    if (it == m_cache.constEnd()) return QVariant();
    const CachedRow &row = it.value();

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return row.type;
        case 1: return row.address;
        case 2: return row.name;
        case 3: return row.testResult;
        case 4: return row.traffic;
        default: return QVariant();
        }
    }
    if (role == Qt::ForegroundRole) {
        if (index.column() == 3 && row.latencyColor.isValid()) {
            return row.latencyColor;
        }
        if (row.foreground.isValid()) {
            return row.foreground;
        }
        return QVariant();
    }
    return QVariant();
}

QVariant ProfilesTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) return QVariant();
    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Type");
        case 1: return tr("Address");
        case 2: return tr("Name");
        case 3: return tr("Test Result");
        case 4: return tr("Traffic");
        default: return QVariant();
        }
    }
    return QVariant();
}

void ProfilesTableModel::setProfileIds(const QList<int> &ids) {
    beginResetModel();
    m_profileIds = ids;
    m_cache.clear();
    m_lruOrder.clear();
    endResetModel();
}

int ProfilesTableModel::profileIdAt(int row) const {
    if (row < 0 || row >= m_profileIds.size()) return -1;
    return m_profileIds[row];
}

void ProfilesTableModel::refreshProfileId(int profileId) {
    for (int r = 0; r < m_profileIds.size(); ++r) {
        if (m_profileIds[r] == profileId) {
            m_cache.remove(profileId);
            m_lruOrder.removeAll(profileId);
            QModelIndex top = index(r, 0);
            QModelIndex bottom = index(r, columnCount() - 1);
            emit dataChanged(top, bottom);
            return;
        }
    }
}

void ProfilesTableModel::setCacheSize(int size) {
    if (size <= 0) return;
    m_cacheSize = size;
    while (m_cache.size() > m_cacheSize && !m_lruOrder.isEmpty()) {
        evictOne();
    }
}

QString ProfilesTableModel::rowLabel(int row) const {
    if (row < 0 || row >= m_profileIds.size()) return QString();
    int id = m_profileIds[row];
    if (Configs::dataManager->settingsRepo->started_id == id) {
        return QStringLiteral("✓");
    }
    return QString::number(row + 1) + QStringLiteral("  ");
}
