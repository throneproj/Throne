#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

// Diff of a subscription refresh over per-profile digests.
namespace Subscription {
    struct ProfileKeys {
        QByteArray content;
        QByteArray identity;
    };

    struct OldEntry {
        int id = -1;
        ProfileKeys keys;
        QString display;
        QString name;
    };

    struct NewEntry {
        int id = -1;
        bool reused = false;
        bool contentKnown = false;
        ProfileKeys keys;
        QString display;
        // Left empty where no name match is possible: reused arrivals, imports without an index.
        QString name;
    };

    // Each old profile is claimed at most once, in group order, by identical arrivals in arrival order (#1775).
    class ContentIndex {
    public:
        explicit ContentIndex(const QList<OldEntry> &old);

        bool knows(const QByteArray &content) const { return known.contains(content); }
        std::optional<int> claim(const QByteArray &content);
        void noteArrival(const QByteArray &content) { arrivals.insert(content); }
        bool arrived(const QByteArray &content) const { return arrivals.contains(content); }
        bool isClaimed(int id) const { return claimedIds.contains(id); }

    private:
        QHash<QByteArray, QList<int>> unclaimed;
        QSet<QByteArray> known;
        QSet<QByteArray> arrivals;
        QSet<int> claimedIds;
    };

    struct ProfileUpdate {
        int oldId = -1;
        int newId = -1;
        // Paired by name alone, so possibly another server or protocol.
        bool byName = false;
    };

    struct ReconcilePlan {
        QList<int> order;
        QList<ProfileUpdate> updates;
        QList<int> stale;
        QStringList added;
        QStringList updated;
        QStringList deleted;
    };

    ReconcilePlan Reconcile(const QList<OldEntry> &old, const QList<NewEntry> &incoming, const ContentIndex &index);
}
