#pragma once

#include <QList>
#include <QString>

namespace Configs_network {
    struct ExitInfo {
        QString ip;
        // Upper-case ISO 3166 alpha-2, or empty.
        QString countryCode;
        QString country;
        QString city;

        [[nodiscard]] bool hasCountry() const { return !countryCode.isEmpty() || !country.isEmpty(); }
    };

    struct ExitLookupProvider {
        // Persisted in exit_lookup_provider and shared with the Android app: never rename one.
        QString id;
        QString label;
        QString url;
    };

    inline const QString ExitLookupAutoId = QStringLiteral("auto");

    // In the order the settings combo lists them, after "Automatic".
    const QList<ExitLookupProvider> &ExitLookupProviders();

    // Blocking, through the mixed inbound; never on the UI thread. "auto" or an unknown id tries several providers in turn.
    // A failure can still carry the IP of an answer that named no country.
    ExitInfo LookupExit(const QString &providerId, QString *error = nullptr);

    // "<flag> <country>, <city>" without the parts that are unknown; empty when no country is.
    QString ExitLocationText(const ExitInfo &info);
} // namespace Configs_network
