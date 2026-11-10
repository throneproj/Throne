#include "include/configs/sub/SubscriptionFilter.hpp"

#include "include/database/entities/Profile.h"

#include <QCoreApplication>

namespace Subscription {
    namespace {
        // Two ASCII letters, upper-cased; empty otherwise.
        QString countryCode(const QString &item) {
            const auto code = item.trimmed().toUpper();
            if (code.size() != 2) return {};
            for (const QChar c : code) {
                if (c < QLatin1Char('A') || c > QLatin1Char('Z')) return {};
            }
            return code == QLatin1String("UK") ? QStringLiteral("GB") : code;
        }

        // Items in input order, with the drop marker kept.
        QList<std::pair<QString, bool>> countryItems(const QString &text, QStringList *problems) {
            QList<std::pair<QString, bool>> items;
            for (const auto &raw : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                auto item = raw.trimmed();
                if (item.isEmpty()) continue;
                const bool drop = item.startsWith(QLatin1Char('!'));
                const auto code = countryCode(drop ? item.mid(1) : item);
                if (code.isEmpty()) {
                    if (problems != nullptr) *problems << item;
                    continue;
                }
                items.append({code, drop});
            }
            return items;
        }
    }

    const QList<FilterProtocol> &FilterProtocols() {
        static const QList<FilterProtocol> protocols = {
            {QStringLiteral("shadowsocks"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Shadowsocks"), {QStringLiteral("shadowsocks")}},
            {QStringLiteral("vmess"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "VMess"), {QStringLiteral("vmess")}},
            {QStringLiteral("vless"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "VLESS"), {QStringLiteral("vless"), QStringLiteral("xrayvless")}},
            {QStringLiteral("trojan"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Trojan"), {QStringLiteral("trojan")}},
            {QStringLiteral("hysteria"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Hysteria / Hysteria2"), {QStringLiteral("hysteria")}},
            {QStringLiteral("tuic"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "TUIC"), {QStringLiteral("tuic")}},
            {QStringLiteral("anytls"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "AnyTLS"), {QStringLiteral("anytls")}},
            {QStringLiteral("shadowtls"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "ShadowTLS"), {QStringLiteral("shadowtls")}},
            {QStringLiteral("wireguard"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "WireGuard / AmneziaWG"), {QStringLiteral("wireguard")}},
            {QStringLiteral("socks"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "SOCKS"), {QStringLiteral("socks")}},
            {QStringLiteral("http"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "HTTP"), {QStringLiteral("http")}},
            {QStringLiteral("ssh"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "SSH"), {QStringLiteral("ssh")}},
            {QStringLiteral("snell"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Snell"), {QStringLiteral("snell")}},
            {QStringLiteral("juicity"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Juicity"), {QStringLiteral("juicity")}},
            {QStringLiteral("mieru"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Mieru"), {QStringLiteral("mieru")}},
            {QStringLiteral("naive"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "NaiveProxy"), {QStringLiteral("naive")}},
            {QStringLiteral("trusttunnel"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "TrustTunnel"), {QStringLiteral("trusttunnel")}},
            {QStringLiteral("masque"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "MASQUE"), {QStringLiteral("masque")}},
            {QStringLiteral("dnstt"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "DNSTT"), {QStringLiteral("dnstt")}},
            {QStringLiteral("masterdns"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "MasterDNS"), {QStringLiteral("masterdns")}},
            {QStringLiteral("openvpn"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "OpenVPN"), {QStringLiteral("openvpn")}},
            {QStringLiteral("openconnect"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "OpenConnect"), {QStringLiteral("openconnect")}},
            {QStringLiteral("custom"), QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Custom (JSON)"), {QStringLiteral("custom")}},
        };
        return protocols;
    }

    QString NamePatternProblem(const QString &pattern) {
        const auto trimmed = pattern.trimmed();
        if (trimmed.isEmpty()) return {};
        const QRegularExpression regex(trimmed, QRegularExpression::CaseInsensitiveOption);
        if (regex.isValid()) return {};
        const auto leading = pattern.indexOf(trimmed);
        return QCoreApplication::translate("Validation", "Not a valid regular expression: %1 (at character %2).")
            .arg(regex.errorString())
            .arg(leading + regex.patternErrorOffset() + 1);
    }

    bool CountryRules::accepts(const QString &country) const {
        if (country.isEmpty()) return true;
        const auto code = country.trimmed().toUpper();
        if (drop.contains(code)) return false;
        return keep.isEmpty() || keep.contains(code);
    }

    CountryRules ParseCountryRules(const QString &text, QStringList *problems) {
        CountryRules rules;
        for (const auto &[code, drop] : countryItems(text, problems)) (drop ? rules.drop : rules.keep).insert(code);
        return rules;
    }

    QString CountryRulesProblem(const QString &text) {
        QStringList problems;
        countryItems(text, &problems);
        if (problems.isEmpty()) return {};
        return QCoreApplication::translate("Validation", "Not a two-letter country code: %1. Use codes such as HK,JP to keep only those countries, or !RU to remove one.")
            .arg(problems.join(QStringLiteral(", ")));
    }

    QString NormalizeCountryRules(const QString &text) {
        QStringList keep;
        QStringList drop;
        for (const auto &[code, dropped] : countryItems(text, nullptr)) {
            auto &list = dropped ? drop : keep;
            if (!list.contains(code)) list << code;
        }
        for (const auto &code : drop) keep << QLatin1Char('!') + code;
        return keep.join(QLatin1Char(','));
    }

    ImportFilter::ImportFilter(const Configs::SubscriptionOptions &options) {
        const auto compile = [this](const QString &pattern, QRegularExpression &regex, bool &enabled, const QString &field) {
            const auto trimmed = pattern.trimmed();
            if (trimmed.isEmpty()) return;
            regex = QRegularExpression(trimmed, QRegularExpression::CaseInsensitiveOption);
            if (!regex.isValid()) {
                problems << QCoreApplication::translate("Validation", "%1 is not a valid regular expression, so it was ignored: %2").arg(field, regex.errorString());
                return;
            }
            regex.optimize();
            enabled = true;
        };
        compile(options.name_filter, nameFilter, hasNameFilter, QCoreApplication::translate("Validation", "Keep only matching"));
        compile(options.name_exclude, nameExclude, hasNameExclude, QCoreApplication::translate("Validation", "Remove matching"));

        for (const auto &id : options.type_exclude) {
            bool known = false;
            for (const auto &protocol : FilterProtocols()) {
                if (protocol.id != id) continue;
                known = true;
                for (const auto &type : protocol.types) excludedTypes.insert(type);
            }
            if (!known) problems << QCoreApplication::translate("Validation", "Unknown protocol in Remove types, ignored: %1").arg(id);
        }

        QStringList badCodes;
        rules = ParseCountryRules(options.country_filter, &badCodes);
        if (!badCodes.isEmpty()) {
            problems << QCoreApplication::translate("Validation", "Not a two-letter country code, ignored: %1").arg(badCodes.join(QStringLiteral(", ")));
        }
    }

    ImportFilter::Verdict ImportFilter::judge(Configs::Profile &ent, const QString &country) const {
        if (excludedTypes.contains(ent.type)) return Verdict::Type;
        if (hasNameFilter || hasNameExclude) {
            const auto name = ent.outbound != nullptr ? ent.outbound->DisplayName() : ent.name;
            if (hasNameExclude && nameExclude.match(name).hasMatch()) return Verdict::Name;
            if (hasNameFilter && !nameFilter.match(name).hasMatch()) return Verdict::Name;
        }
        if (!rules.accepts(country)) return Verdict::Country;
        return Verdict::Keep;
    }
}
