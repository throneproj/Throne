#include "include/ui/utils/ProfilesTableModel.h"
#include "include/global/Configs.hpp"
#include "include/database/entities/Profile.h"
#include "include/configs/common/Outbound.h"
#include <QApplication>
#include <QMimeData>
#include <QPalette>

#include "include/database/GroupsRepo.h"
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
        return Qt::ItemIsDragEnabled | defaultFlags;
    }
    return Qt::ItemIsDropEnabled | defaultFlags;
}

Qt::DropActions ProfilesTableModel::supportedDropActions() const {
    return Qt::MoveAction;
}

QStringList ProfilesTableModel::mimeTypes() const {
    return {"application/profile-row-number"};
}

QMimeData* ProfilesTableModel::mimeData(const QModelIndexList &indexes) const {
    auto *mimeData = new QMimeData;
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    if (!indexes.isEmpty()) {
        stream << indexes.at(0).row();
    }

    mimeData->setData("application/profile-row-number", encodedData);
    return mimeData;
}

void ProfilesTableModel::ensureCached(int profileId) const {
    if (m_cache.contains(profileId)) {
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

    while (m_cache.size() >= m_cacheSize && !m_lruOrder.isEmpty()) {
        evictOne();
    }
    m_cache[profileId] = profile;
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
        return {};
    }
    const int profileId = m_profileIds[index.row()];
    if (role == ProfileIdRole) {
        return profileId;
    }
    ensureCached(profileId);
    auto it = m_cache.constFind(profileId);
    if (it == m_cache.constEnd()) return {};
    const std::shared_ptr<Configs::Profile> &profile = it.value();
    if (!profile) return {};

    const int startedId = Configs::dataManager->settingsRepo->started_id;
    const bool isRunning = (profile->id == startedId);
    QColor linkColor = isRunning ? QApplication::palette().link().color() : QColor();

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return profile->outbound ? profile->outbound->DisplayType() : QString();
        case 1: return profile->outbound ? profile->outbound->DisplayAddress() : QString();
        case 2: return profile->outbound ? profile->outbound->name : QString();
        case 3: return profile->DisplayTestResult();
        case 4: return profile->DisplayTraffic();
        default: return {};
        }
    }
    if (role == Qt::ForegroundRole) {
        if (index.column() == 3) {
            QColor latencyColor = profile->DisplayLatencyColor();
            if (latencyColor.isValid()) return latencyColor;
        }
        if (isRunning && linkColor.isValid()) return linkColor;
        return {};
    }
    return {};
}

QVariant ProfilesTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) return {};
    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Type");
        case 1: return tr("Address");
        case 2: return tr("Name");
        case 3: return tr("Test Result");
        case 4: return tr("Traffic");
        default: return {};
        }
    }
    return {};
}

void ProfilesTableModel::setProfileIds(const QList<int> &ids) {
    beginResetModel();
    m_profileIds = ids;
    id2row.clear();
    int idx=0;
    for (const auto &id : ids) {
        id2row.insert(id, idx++);
    }
    m_cache.clear();
    m_lruOrder.clear();
    endResetModel();
}

void ProfilesTableModel::refreshTable(const QList<int> &ids, bool mayNeedReset) {
    if (m_profileIds.isEmpty() && ids.isEmpty()) return;

    bool needFullReset = (ids.length() != m_profileIds.length()) && mayNeedReset;
    if (!needFullReset && !ids.isEmpty() && mayNeedReset) {
        for (int i=0; i < ids.length(); i++) {
            if (ids[i] != m_profileIds[i]) {
                needFullReset = true;
                break;
            }
        }
    }

    if (needFullReset) {
        setProfileIds(ids);
    } else {
        QModelIndex topLeft = index(0, 0);
        QModelIndex bottomRight = index(m_profileIds.count() - 1, columnCount() - 1);

        emit dataChanged(topLeft, bottomRight);
    }
}

int ProfilesTableModel::profileIdAt(int row) const {
    if (row < 0 || row >= m_profileIds.size()) return -1;
    return m_profileIds[row];
}

void ProfilesTableModel::refreshProfileId(int profileId) {
    if (!id2row.contains(profileId)) return;
    auto r = id2row.value(profileId);
    QModelIndex top = index(r, 0);
    QModelIndex bottom = index(r, columnCount() - 1);
    emit dataChanged(top, bottom);
}

void ProfilesTableModel::emplaceProfiles(int row1, int row2) {
    if (m_profileIds.size() <= row1 || m_profileIds.size() <= row2) return;
    m_profileIds.insert(row2+1, m_profileIds[row1]);
    if (row1 < row2) m_profileIds.remove(row1);
    else m_profileIds.remove(row1+1);
    for (int i = std::max(std::min(row1, row2), 0); i <= std::max(row1, row2); ++i) {
        refreshProfileId(m_profileIds[i]);
    }
}

int ProfilesTableModel::indexOfProfile(int id) {
    if (id2row.contains(id)) return id2row.value(id);
    return -1;
}

QString ProfilesTableModel::rowLabel(int row) const {
    if (row < 0 || row >= m_profileIds.size()) return {};
    int id = m_profileIds[row];
    if (Configs::dataManager->settingsRepo->started_id == id) {
        return QStringLiteral("✓");
    }
    return QString::number(row + 1) + QStringLiteral("  ");
}
