#pragma once

#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include "include/database/entities/Group.h"

namespace Configs {
    class Profile;
}

// Per-subscription profile filters (#2088): name and type rules while a fetch is parsed, country rules after an IP test.
namespace Subscription {
    // id is what sub_options_json stores; types are the Profile::type values it covers.
    struct FilterProtocol {
        QString id;
        // QT_TRANSLATE_NOOP in the DialogEditGroupAdvanced context.
        const char *label;
        QStringList types;
    };

    const QList<FilterProtocol> &FilterProtocols();

    // Empty when the pattern compiles.
    QString NamePatternProblem(const QString &pattern);

    struct CountryRules {
        QSet<QString> keep;
        QSet<QString> drop;

        [[nodiscard]] bool active() const { return !keep.isEmpty() || !drop.isEmpty(); }

        // An empty country is accepted: a profile that was never tested, or failed its test, has no verdict.
        [[nodiscard]] bool accepts(const QString &country) const;
    };

    // "HK,JP" keeps only those, "!RU" drops one, "UK" reads as GB. Items that are not two letters are skipped and listed in problems.
    CountryRules ParseCountryRules(const QString &text, QStringList *problems = nullptr);

    // Empty when every item is a two-letter code.
    QString CountryRulesProblem(const QString &text);

    // "HK,JP,!RU": upper-case, de-duplicated, keeps before drops.
    QString NormalizeCountryRules(const QString &text);

    class ImportFilter {
    public:
        enum class Verdict { Keep, Name, Type, Country };

        // A stored pattern that no longer compiles is skipped and reported in warnings(), never read as matching everything.
        explicit ImportFilter(const Configs::SubscriptionOptions &options);

        [[nodiscard]] bool active() const { return hasNameFilter || hasNameExclude || !excludedTypes.isEmpty() || rules.active(); }

        [[nodiscard]] const CountryRules &countries() const { return rules; }

        [[nodiscard]] const QStringList &warnings() const { return problems; }

        // country: the profile's exit country from an earlier test, empty when unknown.
        [[nodiscard]] Verdict judge(Configs::Profile &ent, const QString &country) const;

    private:
        QRegularExpression nameFilter;
        QRegularExpression nameExclude;
        bool hasNameFilter = false;
        bool hasNameExclude = false;
        QSet<QString> excludedTypes;
        CountryRules rules;
        QStringList problems;
    };
}
