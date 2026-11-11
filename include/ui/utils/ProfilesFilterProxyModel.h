#pragma once

#include <QList>
#include <QSortFilterProxyModel>
#include <QString>

class ProfilesTableModel;

class ProfilesFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit ProfilesFilterProxyModel(QObject *parent = nullptr);

    // `address` also takes "port=N", "port=MIN:MAX", "port=MIN:", "port=:MAX".
    // `test` is words that must all match: an exit country, or a service (or category) that works; "!word" = refused.
    void setFilters(const QString &type, const QString &address, const QString &name, const QString &test);
    bool hasActiveFilter() const;

    ProfilesTableModel *profilesModel() const;

    int toSourceRow(int proxyRow) const;
    int toProxyRow(int sourceRow) const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    struct TestTerm {
        QString text;
        bool refused = false;
    };

    bool portMatches(int port) const;

    QString m_type;
    QString m_address;
    QString m_name;
    QString m_test;
    QList<TestTerm> m_testTerms;
};
