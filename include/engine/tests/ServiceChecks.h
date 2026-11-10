#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace libcore { struct ServiceTestResult; }   // struct in the generated libcore.pb.h

namespace Engine::ServiceChecks {
    struct Category {
        QString id;
        QString name;
        QString icon;
    };

    struct Definitions {
        // ServiceTestRequest.checks; empty runs the core's embedded copy.
        QByteArray text;
        // In definition order.
        QList<Category> categories;
        // Set when the core could not describe any definitions; nothing can run then.
        QString error;
    };

    // Blocking, never on the UI thread. Refreshes the downloaded copy at most once a day, then picks it over the core's
    // embedded copy only when its revision is newer.
    Definitions Prepare();

    // The profiles.service_checks document for one profile's result.
    QString ResultJson(const libcore::ServiceTestResult &result, const QList<Category> &categories, qint64 checkedAt);
} // namespace Engine::ServiceChecks
