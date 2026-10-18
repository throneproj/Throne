#pragma once

#include "Database.h"
#include "include/database/entities/Profile.h"
#include <3rdparty/SQLiteCpp/include/SQLiteCpp.h>
#include <functional>
#include <memory>
#include <mutex>
#include <map>
#include <QSet>
#include <QString>
#include <QJsonObject>

namespace Configs {
    class ProfilesRepo {
    private:
        Database& db;
        mutable std::mutex mutex;
        mutable std::map<int, std::weak_ptr<Profile>> identityMap;
        // Deleted ids: never reused, so Save/SaveBatch refuse them for the rest of the process.
        QSet<int> tombstones;

        std::shared_ptr<Profile> profileFromJson(const QJsonObject& json) const;
        
        void saveToDatabase(const Profile* profile, int id) const;

        // Must stay column-compatible with saveToDatabase.
        ProfileInsertRow profileToInsertRow(const Profile* profile, int id, int gid) const;

        std::shared_ptr<Profile> loadFromDatabase(int id) const;
        
        // Must stay column-compatible with loadFromDatabase.
        std::shared_ptr<Profile> profileFromRow(SQLite::Statement& stmt) const;

        // Does not touch the identity map.
        std::map<int, std::shared_ptr<Profile>> loadProfilesByIdsChunk(const QList<int>& ids) const;
        
        void createTables() const;

        bool profilesColumnExists(const char* columnName) const;

        int NewProfileID() const;
        // Contiguous block starting at the returned id; atomic in the DB, so no lock is required.
        int NewProfileIDRange(int n) const;

    public:
        explicit ProfilesRepo(Database& database);
        
        // Not saved yet: id stays -1 until AddProfile.
        [[nodiscard]] static std::shared_ptr<Profile> NewProfile(const QString &type);
        
        bool AddProfile(std::shared_ptr<Profile>& profile, int gid = -1);
        
        bool AddProfileBatch(QList<std::shared_ptr<Profile>>& profiles, int gid = -1);
        
        std::shared_ptr<Profile> GetProfile(int id) const;

        QList<std::shared_ptr<Profile>> GetProfileBatch(QList<int> ids);

        QList<std::pair<int, QString> > GetProfileIDNameMappedBatch(QList<int> ids);

        std::shared_ptr<Profile> GetProfileByName(const QString &name);

        QList<std::pair<int, QString> > GetAllProfileIDNameMapped();

        QStringList GetAllProfileNames();
        
        // A running profile in ids is stopped (interactive per the flag) when stopRunningProfile, else dropped from ids.
        // A missing group is skipped. False when the rows could not be deleted.
        bool BatchDeleteProfiles(QList<int>& ids, bool stopRunningProfile = false, bool interactive = true);

        // Runs deleteRows under this repo's lock; when it returns true, the ids it filled leave the identity map and are
        // tombstoned. Lock order: this repo, then GroupsRepo.
        bool DeleteAndForget(const std::function<bool(QList<int>& deletedIds)>& deleteRows);
        
        QList<int> GetAllProfileIds() const;

        QList<int> GetProfileIdsByType(const QString& type) const;
        
        // No-op unless id >= 0; false for a deleted id, which is not written back.
        bool Save(const std::shared_ptr<Profile>& profile);

        // Blind UPDATE of the traffic columns: no existence check.
        bool SaveTraffic(const std::shared_ptr<Profile>& profile);

        void SaveTrafficBatch(const QList<std::shared_ptr<Profile>>& profiles);

        // Runs on a new thread; skips null profiles, id < 0 and deleted ids.
        void SaveBatch(const QList<std::shared_ptr<Profile>>& profiles);
    };
}
