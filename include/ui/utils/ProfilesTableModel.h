#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QHash>
#include <QColor>
#include <memory>

namespace Configs {
    class Profile;
}

// On-demand profile list model with configurable LRU cache.
// Holds only profile IDs; cell data is loaded via ProfilesRepo when requested.
class ProfilesTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum { ProfileIdRole = Qt::UserRole };

    explicit ProfilesTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Set the list of profile IDs (filtered, display order). Emits layoutChanged.
    void refreshTable(const QList<int> &ids = {}, bool mayNeedReset = false);
    QList<int> profileIds() const { return m_profileIds; }

    // Profile ID for a given row (for selection / double-click).
    int profileIdAt(int row) const;

    // Invalidate one row so the view repaints (e.g. after latency/traffic update).
    void refreshProfileId(int profileId);

    void emplaceProfiles(int row1, int row2);

    int indexOfProfile(int id);

    // Row label for vertical header: "✓" for running row, else "row+1  ".
    QString rowLabel(int row) const;

private:
    void ensureCached(int profileId) const;
    void evictOne() const;
    void setProfileIds(const QList<int> &ids);

    QList<int> m_profileIds;
    mutable QHash<int, int> id2row;
    mutable QHash<int, std::shared_ptr<Configs::Profile>> m_cache;
    mutable QList<int> m_lruOrder;
    int m_cacheSize = 100;
};
