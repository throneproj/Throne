#include "include/ui/utils/ProfilesFilterProxyModel.h"
#include "include/ui/utils/ProfilesTableModel.h"

#include <QRegularExpression>

namespace {
    const QString portPrefix = QStringLiteral("port=");

    // Two letters could be a country code, so they only pick a service by its exact id or category ("ai").
    bool serviceMatches(const ProfilesTableModel::ServiceFilterKey &service, const QString &term) {
        if (service.names.contains(term)) return true;
        if (term.size() < 3) return false;
        for (const auto &word : service.words) {
            if (word.startsWith(term)) return true;
        }
        return false;
    }

    // Every service the term picks must work; "!term" needs one of them refused. Unpicked terms fall back to the country.
    bool testTermMatches(const ProfilesTableModel::FilterKey &key, const QString &term, bool refused) {
        int picked = 0, working = 0, refusing = 0;
        for (const auto &service : key.services) {
            if (!serviceMatches(service, term)) continue;
            ++picked;
            if (service.state > 0) ++working;
            else if (service.state < 0) ++refusing;
        }
        if (refused) return refusing > 0;
        if (picked > 0 && working == picked) return true;
        return key.country.contains(term, Qt::CaseInsensitive);
    }
}

ProfilesFilterProxyModel::ProfilesFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
}

ProfilesTableModel *ProfilesFilterProxyModel::profilesModel() const {
    return qobject_cast<ProfilesTableModel *>(sourceModel());
}

bool ProfilesFilterProxyModel::hasActiveFilter() const {
    return !m_type.isEmpty() || !m_address.isEmpty() || !m_name.isEmpty() || !m_testTerms.isEmpty();
}

void ProfilesFilterProxyModel::setFilters(const QString &type, const QString &address,
                                          const QString &name, const QString &test) {
    if (m_type == type && m_address == address && m_name == name && m_test == test) return;
    m_type = type;
    m_address = address;
    m_name = name;
    m_test = test;
    m_testTerms.clear();
    static const QRegularExpression separators(QStringLiteral("[\\s,]+"));
    for (auto word : test.toLower().split(separators, Qt::SkipEmptyParts)) {
        const bool refused = word.startsWith(u'!');
        if (refused) word.remove(0, 1);
        if (!word.isEmpty()) m_testTerms.append({word, refused});
    }
    invalidateRowsFilter();
}

int ProfilesFilterProxyModel::toSourceRow(int proxyRow) const {
    const QModelIndex idx = mapToSource(index(proxyRow, 0));
    return idx.isValid() ? idx.row() : -1;
}

int ProfilesFilterProxyModel::toProxyRow(int sourceRow) const {
    if (!sourceModel()) return -1;
    const QModelIndex idx = mapFromSource(sourceModel()->index(sourceRow, 0));
    return idx.isValid() ? idx.row() : -1;
}

bool ProfilesFilterProxyModel::portMatches(int port) const {
    const QString val = m_address.mid(portPrefix.size());
    if (!val.contains(':')) return val.isEmpty() ? false : port == val.toInt();
    const QStringList parts = val.split(':');
    const bool minOk = parts[0].isEmpty() || port >= parts[0].toInt();
    const bool maxOk = (parts.size() < 2 || parts[1].isEmpty()) || port <= parts[1].toInt();
    return minOk && maxOk;
}

bool ProfilesFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &) const {
    if (!hasActiveFilter()) return true;

    auto *model = profilesModel();
    if (!model) return true;

    // A profile that failed to load stays visible rather than becoming unreachable.
    const auto *key = model->filterKeyAt(sourceRow);
    if (!key) return true;

    if (!m_address.isEmpty()) {
        if (m_address.startsWith(portPrefix)) {
            if (!portMatches(key->port)) return false;
        } else if (!key->address.contains(m_address, Qt::CaseInsensitive) &&
                   !key->effectiveAddress.contains(m_address, Qt::CaseInsensitive)) {
            return false;
        }
    }
    if (!m_name.isEmpty() && !key->name.contains(m_name, Qt::CaseInsensitive)) return false;
    if (!m_type.isEmpty() && !key->type.contains(m_type, Qt::CaseInsensitive)) return false;
    for (const auto &term : m_testTerms) {
        if (!testTermMatches(*key, term.text, term.refused)) return false;
    }
    return true;
}
