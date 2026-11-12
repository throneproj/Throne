#include "include/ui/scanner/IpEntriesModel.h"

#include <QHostAddress>
#include <limits>

namespace {
    QString ipEntriesAddressKey(const QString &cidr) {
        const auto slash = cidr.indexOf(QLatin1Char('/'));
        const QString address = slash < 0 ? cidr : cidr.left(slash);
        const QHostAddress host(address);
        if (host.protocol() == QAbstractSocket::IPv4Protocol) {
            const int prefix = slash < 0 ? 32 : cidr.mid(slash + 1).toInt();
            return QStringLiteral("4%1/%2").arg(host.toIPv4Address(), 8, 16, QLatin1Char('0')).arg(prefix, 3, 10, QLatin1Char('0'));
        }
        if (host.protocol() == QAbstractSocket::IPv6Protocol) {
            const int prefix = slash < 0 ? 128 : cidr.mid(slash + 1).toInt();
            const Q_IPV6ADDR bytes = host.toIPv6Address();
            QString key = QStringLiteral("6");
            key.reserve(1 + 32 + 4);
            for (const quint8 byte : bytes.c) key += QStringLiteral("%1").arg(uint(byte), 2, 16, QLatin1Char('0'));
            return key + QStringLiteral("/%1").arg(prefix, 3, 10, QLatin1Char('0'));
        }
        return QStringLiteral("9") + cidr;
    }
}

IpEntriesModel::IpEntriesModel(QObject *parent) : QAbstractTableModel(parent) {}

void IpEntriesModel::setEntries(QList<Configs::IpListEntry> entries) {
    beginResetModel();
    entries_ = std::move(entries);
    addressKeys_.clear();
    addressKeys_.reserve(entries_.size());
    hasMaxQName_ = hasUploadMtu_ = hasDownloadMtu_ = false;
    for (const auto &entry : entries_) {
        addressKeys_.append(ipEntriesAddressKey(entry.cidr));
        hasMaxQName_ = hasMaxQName_ || entry.maxQName > 0;
        hasUploadMtu_ = hasUploadMtu_ || entry.uploadMtu > 0;
        hasDownloadMtu_ = hasDownloadMtu_ || entry.downloadMtu > 0;
    }
    endResetModel();
}

bool IpEntriesModel::columnHasData(int column) const {
    switch (column) {
        case MaxQNameColumn: return hasMaxQName_;
        case UploadMtuColumn: return hasUploadMtu_;
        case DownloadMtuColumn: return hasDownloadMtu_;
        default: return true;
    }
}

int IpEntriesModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

int IpEntriesModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant IpEntriesModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) return {};
    const auto &entry = entries_.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case AddressColumn: return entry.cidr;
            case PortColumn: return entry.port > 0 ? QString::number(entry.port) : QString();
            case LatencyColumn: return entry.latencyMs > 0 ? tr("%1 ms").arg(entry.latencyMs) : QString();
            case MaxQNameColumn: return entry.maxQName > 0 ? QString::number(entry.maxQName) : QString();
            case UploadMtuColumn: return entry.uploadMtu > 0 ? QString::number(entry.uploadMtu) : QString();
            case DownloadMtuColumn: return entry.downloadMtu > 0 ? QString::number(entry.downloadMtu) : QString();
            default: return {};
        }
    }
    if (role == SortRole) {
        switch (index.column()) {
            case AddressColumn: return addressKeys_.at(index.row());
            case PortColumn: return entry.port;
            case LatencyColumn: return entry.latencyMs > 0 ? entry.latencyMs : std::numeric_limits<int>::max();
            case MaxQNameColumn: return entry.maxQName;
            case UploadMtuColumn: return entry.uploadMtu;
            case DownloadMtuColumn: return entry.downloadMtu;
            default: return {};
        }
    }
    if (role == Qt::TextAlignmentRole && index.column() != AddressColumn) {
        return QVariant::fromValue(Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));
    }
    return {};
}

QVariant IpEntriesModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::ToolTipRole) {
        switch (section) {
            case MaxQNameColumn: return tr("The longest query name the resolver relays: max_qname_len for DNSTT, lenLimit for XDNS.");
            case UploadMtuColumn: return tr("The largest payload per query the resolver relays.");
            case DownloadMtuColumn: return tr("The largest payload per answer the resolver relays.");
            default: break;
        }
    }
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return QAbstractTableModel::headerData(section, orientation, role);
    switch (section) {
        case AddressColumn: return tr("Address");
        case PortColumn: return tr("Port");
        case LatencyColumn: return tr("Latency");
        case MaxQNameColumn: return tr("Max QNAME");
        case UploadMtuColumn: return tr("Upload MTU");
        case DownloadMtuColumn: return tr("Download MTU");
        default: return {};
    }
}
