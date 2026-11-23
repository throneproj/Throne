#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QHash>
#include <QSet>
#include <QColor>
#include <memory>

#include "include/database/entities/GroupSort.h"

namespace Configs {
    class Profile;
    class Group;
}

class ProfilesTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    // ServiceItemsRole: the test column's QList<ServiceIcons::Item>.
    enum { ProfileIdRole = Qt::UserRole, PinnedRole, ServiceItemsRole };

    // Persisted as bits in Group::visible_columns; append new columns without renumbering these.
    enum Column {
        ColType = 0,
        ColAddress,
        ColName,
        ColLatency,
        ColExitIP,
        ColSpeed,
        ColTraffic,
        ColumnCount,
    };

    // One service check result; names, words and region are lowercase.
    struct ServiceFilterKey {
        QStringList names;  // check id and name, category id and name
        QStringList words;
        QString region;     // country code the service reported, empty when unknown
        int state = 0;      // 1 available or partial, -1 restricted or blocked, 0 inconclusive
    };

    // Two letters could be a country code, so they only pick a service by its exact id or category ("ai").
    template <typename Names, typename Words>
    static bool termPicksService(const Names &names, const Words &words, const QString &term) {
        if (names.contains(term)) return true;
        if (term.size() < 3) return false;
        for (const auto &word : words) {
            if (word.startsWith(term)) return true;
        }
        return false;
    }

    struct FilterKey {
        QString type;
        QString address;
        // The host an endpoint source makes the profile dial; empty when it keeps its own.
        QString effectiveAddress;
        QString name;
        QString country;
        QList<ServiceFilterKey> services;
        int port = 0;
    };

    explicit ProfilesTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // The first pinnedCount ids are the group's pinned profiles.
    void refreshTable(const QList<int> &ids, int pinnedCount, bool mayNeedReset = false);

    void refreshProfileId(int profileId);

    // Re-resolves the Address column after IP lists or group endpoints change.
    void invalidateAddresses();

    int indexOfProfile(int id);

    bool isStartedRow(int sourceRow) const;

    int pinnedCount() const { return m_pinnedCount; }

    bool isPinnedRow(int sourceRow) const { return sourceRow >= 0 && sourceRow < m_pinnedCount; }

    // A pinned row followed by an unpinned one in `index`'s model (this one or a proxy over it): the divider goes under it.
    static bool endsPinnedBlock(const QModelIndex &index);

    QString rowLabel(int sourceRow, int displayRow) const;

    // Null if the profile could not be loaded; valid until the next model change.
    const FilterKey *filterKeyAt(int row) const;

    bool termPicksAnyService(const QString &term) const;

    // -1 for a method no column shows.
    static int columnOfSort(const Configs::Group &group);

    // Raw for a column that does not sort.
    static GroupSortMethod::GroupSortMethod sortOfColumn(int column);

signals:
    void pinnedCountChanged();

private:
    void ensureCached(int profileId) const;
    void evictOne() const;
    void setProfileIds(const QList<int> &ids, int pinnedCount);
    void setPinnedCount(int count);
    void ensureFilterIndex() const;
    void insertFilterKey(int profileId, FilterKey key) const;

    QList<int> m_profileIds;
    int m_pinnedCount = 0;
    mutable QHash<int, int> id2row;
    mutable QHash<int, std::shared_ptr<Configs::Profile>> m_cache;
    mutable QList<int> m_lruOrder;
    int m_cacheSize = 100;
    // Resolving an endpoint source can query the IP lists, so it runs once per row, not per paint.
    mutable QHash<int, QString> m_addressCache;

    mutable QHash<int, FilterKey> m_filterKeys;
    mutable QSet<QString> m_serviceNames;
    mutable QSet<QString> m_serviceWords;
    mutable bool m_filterIndexBuilt = false;
};
