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
    // `test` words must all match: a country or a working service; "!word" = refused or another country; "service+XX"/"+!XX" = its region.
    void setFilters(const QString &type, const QString &address, const QString &name, const QString &test);
    bool hasActiveFilter() const;

    ProfilesTableModel *profilesModel() const;

    int toSourceRow(int proxyRow) const;
    int toProxyRow(int sourceRow) const;

    struct TestTerm {
        QString text;
        QString region;
        bool refused = false;
        bool regionNegated = false;
    };

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool portMatches(int port) const;

    QString m_type;
    QString m_address;
    QString m_name;
    QString m_test;
    QList<TestTerm> m_testTerms;
};
