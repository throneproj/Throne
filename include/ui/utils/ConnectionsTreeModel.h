#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

#include "include/stats/connections/connectionLister.hpp"

namespace ConnectionsTree {

struct TreeItem {
    virtual ~TreeItem() = default;
    virtual bool isProcess() const = 0;
    int row = 0;
};

struct ProcessGroupItem;

struct ConnectionLeafItem : public TreeItem {
    bool isProcess() const override { return false; }
    ProcessGroupItem *parent = nullptr;
    Stats::ConnectionMetadata meta; // the first socket; the rest share every field that makes up `key`
    QString key;
    QString destText;
    QString protocolText;
    QString outboundText;
    int count = 0;
    QStringList connectionIds;
    long long upload = 0;
    long long download = 0;
    long long uploadSpeed = 0;
    long long downloadSpeed = 0;
};

struct ProcessGroupItem : public TreeItem {
    bool isProcess() const override { return true; }
    QString processName; // empty when the core could not attribute the sockets to a process
    std::vector<std::unique_ptr<ConnectionLeafItem>> children;
    long long totalUpload = 0;
    long long totalDownload = 0;
    long long totalUploadSpeed = 0;
    long long totalDownloadSpeed = 0;
    int totalConnections = 0;
    QString commonOutbound;
    QString commonOutboundText;
    bool sameOutbound = true;
    QString commonSource;
    bool sameSource = true;

    QStringList connectionIds() const {
        QStringList ids;
        for (const auto &child : children) ids.append(child->connectionIds);
        return ids;
    }
};

} // namespace ConnectionsTree

class ConnectionsTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Roles {
        ConnIdsRole = Qt::UserRole,
        IsProcessRole,
        ProcessNameRole,
    };

    enum Column {
        ColTarget = 0, // the process on top-level rows, the destination below them
        ColSource,
        ColProtocol,
        ColOutbound,
        ColTraffic,
        ColSpeed,
        ColumnCount
    };

    explicit ConnectionsTreeModel(QObject *parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setConnections(const QList<Stats::ConnectionMetadata> &connections, Stats::ConnectionSort sort, bool ascending);

    static QString displayProcessName(const QString &processName);
    // The menu's "Matched: <entries> → <proxy|direct|server>": the rule's entries that matched, else the whole rule,
    // else "final", shortened to maxGraphemes.
    static QString matchedRuleText(const Stats::ConnectionMetadata &meta, int maxGraphemes);
    // The same in full, naming the server and its tag.
    static QString matchedRuleToolTip(const Stats::ConnectionMetadata &meta);

    bool isProcessIndex(const QModelIndex &index) const;
    QString processNameAt(const QModelIndex &index) const;
    const Stats::ConnectionMetadata *metaAt(const QModelIndex &index) const;
    QStringList connectionIdsAt(const QModelIndex &index) const;
    const ConnectionsTree::ProcessGroupItem *groupAt(int row) const;

private:
    QModelIndex indexOf(ConnectionsTree::TreeItem *item, int column) const;

    std::vector<std::unique_ptr<ConnectionsTree::ProcessGroupItem>> m_groups;
};
