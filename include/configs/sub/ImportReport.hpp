#pragma once

#include "include/configs/sub/SubscriptionParser.hpp"

#include <QHash>
#include <QStringList>

namespace Subscription {
    // Aggregates per-item parse reports: identical (severity, field, message) issues collapse into one line with a count.
    class ImportReport {
    public:
        struct Line {
            Validation::Severity severity;
            QString field;
            QString message;
            int count = 0;
            // Source labels, never the raw link.
            QStringList examples;
        };

        void add(const ParseReport &report);
        void addFatal(const QString &reason);
        void setImported(int count);

        [[nodiscard]] int imported() const { return importedCount; }
        [[nodiscard]] int failed() const { return failedCount; }
        [[nodiscard]] int warned() const { return warnedCount; }
        [[nodiscard]] bool hasProblems() const;
        // A refused item, a fatal reason or an Error issue.
        [[nodiscard]] bool hasErrors() const;

        // "Imported 12 profile(s). 3 could not be imported. 5 imported with warnings." then one fatal reason per line.
        [[nodiscard]] QString summary() const;
        // Grouped lines, errors first: "[error] port: Must be a number from 1 to 65535. (3×, e.g. a, b, c)".
        [[nodiscard]] QString details(int maxLines = 50) const;
        // The grouped issues, errors first.
        [[nodiscard]] QList<Line> lines() const;
        [[nodiscard]] QStringList fatalReasons() const { return fatal; }

    private:
        using Group = Line;

        int importedCount = 0;
        int failedCount = 0;
        int warnedCount = 0;
        QStringList fatal;
        QList<Group> groups;
        QHash<QString, qsizetype> groupIndex;
    };

    // Thread-safe: queues a non-modal box on the UI thread; a repeat with the same title updates the open box. Without a GUI it logs the report.
    void ShowImportReport(const QString &title, const ImportReport &report);
}
