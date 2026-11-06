#include "include/ui/utils/ProfilesTableModel.h"
#include "include/global/Configs.hpp"
#include "include/database/entities/Profile.h"
#include "include/configs/common/Outbound.h"
#include "include/ui/setting/ThemeManager.hpp"
#include <QApplication>
#include <QMimeData>
#include <QPalette>

#include "include/configs/generate.h"
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
    return ColumnCount;
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
    m_addressCache.remove(id);
}

QVariant ProfilesTableModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_profileIds.size()
        || index.column() < 0 || index.column() >= ColumnCount) {
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
    const ThemeTokens &tokens = themeManager()->tokens;
    QColor linkColor = !isRunning                  ? QColor()
                       : tokens.runningRow.isValid() ? tokens.onRunningRow
                                                     : QApplication::palette().link().color();

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColType: {
            if (!profile->outbound) return QString();
            auto type = profile->outbound->DisplayType();
            if (Configs::dataManager->settingsRepo->show_config_security) {
                auto sec = profile->outbound->DisplaySecurity();
                if (!sec.isEmpty()) type += QStringLiteral(" (%1)").arg(sec);
            }
            return type;
        }
        case ColAddress: {
            auto address = m_addressCache.constFind(profileId);
            if (address == m_addressCache.constEnd()) address = m_addressCache.insert(profileId, Configs::DisplayEffectiveAddress(profile));
            return address.value();
        }
        case ColName: return profile->outbound ? profile->outbound->name : QString();
        case ColLatency: return profile->DisplayLatency();
        case ColExitIP: return profile->DisplayExitIP();
        case ColSpeed: return profile->DisplaySpeed();
        case ColRiskFactor: return profile->DisplayRiskFactor();
        case ColTraffic: return profile->DisplayTraffic();
        default: return {};
        }
    }
    if (role == Qt::ToolTipRole) {
        if (index.column() == ColType && Configs::dataManager->settingsRepo->show_config_security && profile->outbound) {
            const auto security = profile->outbound->EffectiveSecurity();
            if (security.compromised) return tr("Certificate checks are turned off by the \"Skip TLS certificate authentication\" setting.");
            if (security.isDangerous()) return tr("This config's traffic is not properly protected.");
        }
        return {};
    }
    if (role == Qt::ForegroundRole) {
        if (index.column() == ColLatency) {
            const QColor latencyColor = profile->DisplayLatencyColor();
            return latencyColor.isValid() ? QVariant(latencyColor) : QVariant();
        }
        if (index.column() == ColExitIP || index.column() == ColSpeed) return {};
        if (index.column() == ColRiskFactor) {
            if (profile->ip_risk_factor < 0) return {};
            if (profile->ip_risk_factor <= 30) return tokens.success;
            if (profile->ip_risk_factor <= 70) return QColor(Qt::darkYellow);
            return tokens.danger;
        }
        if (isRunning && linkColor.isValid()) return linkColor;
        return {};
    }
    if (role == Qt::BackgroundRole && isRunning && tokens.runningRow.isValid()) {
        return tokens.runningRow;
    }
    return {};
}

QVariant ProfilesTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) return {};
    if (orientation == Qt::Horizontal) {
        switch (section) {
        case ColType: return tr("Type");
        case ColAddress: return tr("Address");
        case ColName: return tr("Name");
        case ColLatency: return tr("Latency");
        case ColExitIP: return tr("Exit IP");
        case ColSpeed: return tr("Speed");
        case ColRiskFactor: return tr("Risk factor");
        case ColTraffic: return tr("Traffic");
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
    m_addressCache.clear();
    m_filterKeys.clear();
    m_filterIndexBuilt = false;
    endResetModel();
}

namespace {
    ProfilesTableModel::FilterKey makeFilterKey(const std::shared_ptr<Configs::Profile> &profile) {
        ProfilesTableModel::FilterKey key;
        key.type = profile->type;
        key.country = profile->test_country;
        if (profile->outbound) {
            key.address = profile->outbound->server;
            key.effectiveAddress = Configs::EffectiveEndpointHost(profile);
            key.name = profile->outbound->name;
            key.port = profile->outbound->server_port;
        }
        return key;
    }
}

void ProfilesTableModel::ensureFilterIndex() const {
    if (m_filterIndexBuilt) return;
    m_filterIndexBuilt = true;
    m_filterKeys.clear();
    m_filterKeys.reserve(m_profileIds.size());
    for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileBatch(m_profileIds)) {
        if (profile) m_filterKeys.insert(profile->id, makeFilterKey(profile));
    }
}

const ProfilesTableModel::FilterKey *ProfilesTableModel::filterKeyAt(int row) const {
    if (row < 0 || row >= m_profileIds.size()) return nullptr;
    ensureFilterIndex();
    auto it = m_filterKeys.constFind(m_profileIds[row]);
    return it == m_filterKeys.constEnd() ? nullptr : &it.value();
}

void ProfilesTableModel::refreshTable(const QList<int> &ids, bool mayNeedReset) {
    if (m_profileIds.isEmpty() && ids.isEmpty()) return;

    const bool needFullReset = mayNeedReset && (
    ids.size() != m_profileIds.size() ||
    !std::equal(ids.begin(), ids.end(), m_profileIds.begin())
    );

    if (needFullReset) {
        setProfileIds(ids);
    } else {
        m_addressCache.clear();
        m_filterKeys.clear();
        m_filterIndexBuilt = false;
        QModelIndex topLeft = index(0, 0);
        QModelIndex bottomRight = index(m_profileIds.count() - 1, columnCount() - 1);

        emit dataChanged(topLeft, bottomRight);
    }
}

void ProfilesTableModel::refreshProfileId(int profileId) {
    if (!id2row.contains(profileId)) return;
    m_addressCache.remove(profileId);
    const auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId);
    if (m_cache.contains(profileId)) {
        if (profile) m_cache[profileId] = profile;
        else {
            m_cache.remove(profileId);
            m_lruOrder.removeAll(profileId);
        }
    }
    // Update filter keys before observers process dataChanged.
    if (m_filterIndexBuilt) {
        if (profile) m_filterKeys.insert(profileId, makeFilterKey(profile));
        else m_filterKeys.remove(profileId);
    }
    auto r = id2row.value(profileId);
    QModelIndex top = index(r, 0);
    QModelIndex bottom = index(r, columnCount() - 1);
    emit dataChanged(top, bottom);
}

void ProfilesTableModel::invalidateAddresses() {
    m_addressCache.clear();
    m_filterKeys.clear();
    m_filterIndexBuilt = false;
    if (m_profileIds.isEmpty()) return;
    emit dataChanged(index(0, ColAddress), index(static_cast<int>(m_profileIds.size()) - 1, ColAddress), {Qt::DisplayRole});
}

void ProfilesTableModel::emplaceProfiles(int row1, int row2) {
    if (row1 < 0 || row1 >= m_profileIds.size() || row2 < -1 || row2 >= m_profileIds.size()) return;
    const int destination = row2 + 1;
    if (destination == row1 || destination == row1 + 1) return;
    if (!beginMoveRows(QModelIndex(), row1, row1, QModelIndex(), destination)) return;
    const int newRow = destination > row1 ? destination - 1 : destination;
    m_profileIds.move(row1, newRow);
    const int from = std::min(row1, newRow);
    const int to = std::max(row1, newRow);
    for (int i = from; i <= to; ++i) id2row[m_profileIds[i]] = i;
    endMoveRows();
}

int ProfilesTableModel::indexOfProfile(int id) {
    if (id2row.contains(id)) return id2row.value(id);
    return -1;
}

int ProfilesTableModel::columnOfSort(const Configs::Group &group) {
    switch (group.sort.method) {
    case GroupSortMethod::ByType:
    case GroupSortMethod::BySecurity: return ColType;
    case GroupSortMethod::ByAddress: return ColAddress;
    case GroupSortMethod::ByName: return ColName;
    case GroupSortMethod::ByLatency: return ColLatency;
    case GroupSortMethod::ByTestResult:
        switch (group.test_sort_by) {
        case Configs::testBy::latency: return ColLatency;
        case Configs::testBy::ipOut: return ColExitIP;
        case Configs::testBy::dlSpeed:
        case Configs::testBy::ulSpeed: return ColSpeed;
        case Configs::testBy::riskFactor: return ColRiskFactor;
        default: return -1;
        }
    case GroupSortMethod::ByTraffic: return ColTraffic;
    default: return -1;
    }
}

GroupSortMethod::GroupSortMethod ProfilesTableModel::sortOfColumn(int column) {
    switch (column) {
    case ColType: return GroupSortMethod::ByType;
    case ColAddress: return GroupSortMethod::ByAddress;
    case ColName: return GroupSortMethod::ByName;
    case ColLatency: return GroupSortMethod::ByLatency;
    case ColExitIP:
    case ColSpeed:
    case ColRiskFactor: return GroupSortMethod::ByTestResult;
    case ColTraffic: return GroupSortMethod::ByTraffic;
    default: return GroupSortMethod::Raw;
    }
}

bool ProfilesTableModel::isStartedRow(int sourceRow) const {
    return sourceRow >= 0 && sourceRow < m_profileIds.size()
        && m_profileIds[sourceRow] == Configs::dataManager->settingsRepo->started_id;
}

QString ProfilesTableModel::rowLabel(int sourceRow, int displayRow) const {
    if (sourceRow < 0 || sourceRow >= m_profileIds.size()) return {};
    if (isStartedRow(sourceRow)) {
        return QStringLiteral("✓");
    }
    return QString::number(displayRow + 1) + QStringLiteral("  ");
}
