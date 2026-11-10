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

namespace {
    // 界面显示顺序: 0:名称, 1:类型, 2:测试结果, 3:流量, 4:地址
    constexpr int DisplayToLogical[ProfilesTableModel::ColumnCount] = {
        ProfilesTableModel::ColName,
        ProfilesTableModel::ColType,
        ProfilesTableModel::ColTestResult,
        ProfilesTableModel::ColTraffic,
        ProfilesTableModel::ColAddress
    };

    constexpr int LogicalToDisplay[ProfilesTableModel::ColumnCount] = {
        1, // ColType -> 第 1 列
        4, // ColAddress -> 第 4 列
        0, // ColName -> 第 0 列
        2, // ColTestResult -> 第 2 列
        3  // ColTraffic -> 第 3 列
    };
}

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
    const int col = DisplayToLogical[index.column()];
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
        switch (col) {
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
        case ColTestResult: return profile->DisplayTestResult();
        case ColTraffic: return profile->DisplayTraffic();
        default: return {};
        }
    }
    if (role == Qt::ToolTipRole) {
        if (col == ColType && Configs::dataManager->settingsRepo->show_config_security && profile->outbound) {
            const auto security = profile->outbound->EffectiveSecurity();
            if (security.compromised) return tr("Certificate checks are turned off by the \"Skip TLS certificate authentication\" setting.");
            if (security.isDangerous()) return tr("This config's traffic is not properly protected.");
        }
        return {};
    }
    if (role == Qt::ForegroundRole) {
        if (col == ColTestResult) {
            QColor latencyColor = profile->DisplayLatencyColor();
            if (latencyColor.isValid()) return latencyColor;
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
        if (section < 0 || section >= ColumnCount) return {};
        const int col = DisplayToLogical[section];
        switch (col) {
        case ColType: return tr("Type");
        case ColAddress: return tr("Address");
        case ColName: return tr("Name");
        case ColTestResult: return tr("Test Result");
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
    // Keep the filter key in step before dataChanged makes the proxy re-test the row.
    if (m_filterIndexBuilt) {
        if (auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId)) {
            m_filterKeys.insert(profileId, makeFilterKey(profile));
        }
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
    const int displayCol = LogicalToDisplay[ColAddress];
    emit dataChanged(index(0, displayCol), index(static_cast<int>(m_profileIds.size()) - 1, displayCol), {Qt::DisplayRole});
}

void ProfilesTableModel::emplaceProfiles(int row1, int row2) {
    if (m_profileIds.size() <= row1 || m_profileIds.size() <= row2) return;
    m_profileIds.insert(row2+1, m_profileIds[row1]);
    if (row1 < row2) m_profileIds.remove(row1);
    else m_profileIds.remove(row1+1);

    const int from = std::max(std::min(row1, row2), 0);
    const int to = std::min(std::max(row1, row2), static_cast<int>(m_profileIds.size()) - 1);
    for (int i = from; i <= to; ++i) id2row[m_profileIds[i]] = i;
    for (int i = from; i <= to; ++i) refreshProfileId(m_profileIds[i]);
}

int ProfilesTableModel::indexOfProfile(int id) {
    if (id2row.contains(id)) return id2row.value(id);
    return -1;
}

int ProfilesTableModel::columnOfSort(GroupSortMethod::GroupSortMethod method) {
    int logicalCol = -1;
    switch (method) {
    case GroupSortMethod::ByType:
    case GroupSortMethod::BySecurity: logicalCol = ColType; break;
    case GroupSortMethod::ByAddress: logicalCol = ColAddress; break;
    case GroupSortMethod::ByName: logicalCol = ColName; break;
    case GroupSortMethod::ByTestResult:
    case GroupSortMethod::ByLatency: logicalCol = ColTestResult; break;
    case GroupSortMethod::ByTraffic: logicalCol = ColTraffic; break;
    default: return -1;
    }
    return LogicalToDisplay[logicalCol];
}

GroupSortMethod::GroupSortMethod ProfilesTableModel::sortOfColumn(int column) {
    if (column < 0 || column >= ColumnCount) return GroupSortMethod::Raw;
    int logicalCol = DisplayToLogical[column];
    switch (logicalCol) {
    case ColType: return GroupSortMethod::ByType;
    case ColAddress: return GroupSortMethod::ByAddress;
    case ColName: return GroupSortMethod::ByName;
    case ColTestResult: return GroupSortMethod::ByTestResult;
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
