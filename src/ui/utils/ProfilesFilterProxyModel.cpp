#include "include/ui/utils/ProfilesFilterProxyModel.h"
#include "include/ui/utils/ProfilesTableModel.h"

#include <QRegularExpression>

namespace {
    const QString portPrefix = QStringLiteral("port=");

    using TestTerm = ProfilesFilterProxyModel::TestTerm;

    // An unknown region never matches, so a failed region read doesn't pass as "not RU".
    bool regionMatches(const ProfilesTableModel::ServiceFilterKey &service, const TestTerm &term) {
        if (term.region.isEmpty()) return true;
        return !service.region.isEmpty() && (service.region == term.region) != term.regionNegated;
    }

    // Every service the term picks must work; "!term" needs one of them refused. Unpicked terms fall back to the country.
    bool testTermMatches(const ProfilesTableModel &model, const ProfilesTableModel::FilterKey &key, const TestTerm &term) {
        int picked = 0, working = 0, refusing = 0;
        for (const auto &service : key.services) {
            if (!ProfilesTableModel::termPicksService(service.names, service.words, term.text)) continue;
            ++picked;
            if (!regionMatches(service, term)) continue;
            if (service.state > 0) ++working;
            else if (service.state < 0) ++refusing;
        }
        const bool countryTerm = term.region.isEmpty();
        if (term.refused) {
            if (refusing > 0) return true;
            // Checked table-wide: "!ai" on a profile without results must not turn into "not Anguilla".
            return countryTerm && term.text.size() == 2 && !key.country.isEmpty() &&
                   !key.country.contains(term.text, Qt::CaseInsensitive) && !model.termPicksAnyService(term.text);
        }
        if (picked > 0 && working == picked) return true;
        return countryTerm && key.country.contains(term.text, Qt::CaseInsensitive);
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
        TestTerm term;
        term.refused = word.startsWith(u'!');
        if (term.refused) word.remove(0, 1);
        if (const auto plus = word.indexOf(u'+'); plus >= 0) {
            term.region = word.mid(plus + 1);
            word.truncate(plus);
            term.regionNegated = term.region.startsWith(u'!');
            if (term.regionNegated) term.region.remove(0, 1);
        }
        term.text = word;
        if (!term.text.isEmpty()) m_testTerms.append(term);
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
        if (!testTermMatches(*model, *key, term)) return false;
    }
    return true;
}
