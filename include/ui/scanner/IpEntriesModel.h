#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QString>

#include "include/database/entities/IpList.h"

class IpEntriesModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { AddressColumn, PortColumn, LossColumn, LatencyColumn, ColumnCount };

    static constexpr int SortRole = Qt::UserRole + 1;

    explicit IpEntriesModel(QObject *parent = nullptr);

    void setEntries(QList<Configs::IpListEntry> entries);

    [[nodiscard]] const QList<Configs::IpListEntry> &entries() const { return entries_; }

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    [[nodiscard]] int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    QList<Configs::IpListEntry> entries_;
    // Fixed-width keys, so a string compare orders addresses numerically (IPv4 before IPv6).
    QList<QString> addressKeys_;
};
