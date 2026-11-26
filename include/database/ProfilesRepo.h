#pragma once

#include "Database.h"
#include "include/database/entities/Profile.h"
#include <3rdparty/SQLiteCpp/include/SQLiteCpp.h>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <map>
#include <QFlags>
#include <QSet>
#include <QString>
#include <QJsonObject>
#include <QThreadPool>

namespace Configs {
    struct HopReference;

    // Test-result columns for ProfilesRepo::SaveColumns.
    enum class ProfileColumn {
        Latency = 0x1,       // latency, latency_at
        ExitIp = 0x2,        // ip_out, test_country
        Speed = 0x4,         // dl_speed, ul_speed
        ServiceChecks = 0x8, // service_checks
        CustomTests = 0x10,  // custom_test_results
    };
    Q_DECLARE_FLAGS(ProfileColumns, ProfileColumn)

    class ProfilesRepo {
    private:
        Database& db;
        mutable std::mutex mutex;
        mutable std::map<int, std::weak_ptr<Profile>> identityMap;
        std::atomic<quint64> revision{0};

        std::shared_ptr<Profile> profileFromJson(const QJsonObject& json) const;

        void saveToDatabase(const Profile* profile, int id) const;

        // Must stay column-compatible with saveToDatabase.
        ProfileInsertRow profileToInsertRow(const Profile* profile, int id, int gid) const;

        // Updates an existing row with saveToDatabase's columns; the rows it changed, or -1 after a reported error.
        int updateRow(const ProfileInsertRow& row) const;

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

        // One thread, so queued writes land in the order they were queued; drained by the destructor.
        QThreadPool writer;

    public:
        explicit ProfilesRepo(Database& database);

        ~ProfilesRepo();

        ProfilesRepo(const ProfilesRepo&) = delete;
        ProfilesRepo& operator=(const ProfilesRepo&) = delete;

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
        // A missing group is skipped; groups lose the deleted ones as front/landing proxy. False when the rows could not be deleted.
        bool BatchDeleteProfiles(QList<int>& ids, bool stopRunningProfile = false, bool interactive = true,
                                 QList<HopReference>* clearedHops = nullptr);

        // Runs deleteRows under this repo's lock; when it returns true, the ids it filled leave the identity map.
        // Lock order: this repo, then GroupsRepo.
        bool DeleteAndForget(const std::function<bool(QList<int>& deletedIds)>& deleteRows);
        
        QList<int> GetAllProfileIds() const;

        QList<int> GetProfileIdsByType(const QString& type) const;

        // A cheap prefilter: needle is matched ASCII case-insensitively against the stored outbound JSON.
        QList<int> GetProfileIdsByTypeContaining(const QString& type, const QString& needle) const;

        // Updates the existing row only: false for id < 0 or a deleted id, which is not written back.
        bool Save(const std::shared_ptr<Profile>& profile);

        // Queued on the writer with the values of now; a deleted id matches no row.
        bool SaveTraffic(const std::shared_ptr<Profile>& profile);

        void SaveTrafficBatch(const QList<std::shared_ptr<Profile>>& profiles);

        // Serialized and written on the writer, in one transaction; skips null profiles, id < 0 and deleted ids.
        void SaveBatch(const QList<std::shared_ptr<Profile>>& profiles);

        // Only these columns, with the values of now, in one transaction on the writer; skips null profiles, id < 0 and
        // deleted ids, and leaves the identity map alone.
        void SaveColumns(const QList<std::shared_ptr<Profile>>& profiles, ProfileColumns columns);

        // Returns once the writes queued before the call have landed, false after timeoutMs. The repo is never destroyed, so
        // exit waits here too.
        bool WaitForWrites(int timeoutMs);

        // Any thread; moves with every write except the traffic-only ones (SaveTraffic, SaveTrafficBatch).
        quint64 Revision() const { return revision.load(); }
    };
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Configs::ProfileColumns)
