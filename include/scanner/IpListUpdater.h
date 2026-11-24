#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <functional>

#include "include/database/entities/IpList.h"

namespace Scanner {
    struct ImportOutcome {
        QString error;
        QList<Configs::IpListEntry> entries;
        int rejected = 0;
        QStringList samples;
    };

    // Blocking network: worker thread only.
    ImportOutcome FetchEntries(Configs::IpList::SourceKind kind, const QString &source);

    ImportOutcome ParseImportBytes(const QByteArray &bytes);

    class IpListUpdater : public QObject {
        Q_OBJECT

    public:
        using Finish = std::function<void(const QString &error)>;

        static IpListUpdater *instance();

        // finish may fire on any thread.
        void Refresh(int listId, const Finish &finish = nullptr);

        void RefreshAll(bool onlyAuto);

        // UI thread only.
        void CheckAutoUpdate();

        [[nodiscard]] bool IsRefreshing(int listId) const;

        // Any thread.
        void NotifyListsChanged();

    signals:
        // Emitted from the worker; connect queued.
        void listUpdated(int listId, const QString &error);
        void listsChanged();

    private:
        explicit IpListUpdater(QObject *parent);

        void drain();

        mutable QMutex mutex;
        QList<int> queue;
        QHash<int, QList<Finish>> finishers;
        QSet<int> active;
        bool running = false;
        // UI thread only: in-memory, so a restart retries a failing list once.
        QHash<int, qint64> autoAttempts;
    };
} // namespace Scanner
