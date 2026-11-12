#pragma once

#include <QList>
#include <QMutex>
#include <memory>

#include "Database.h"
#include "include/database/entities/IpScan.h"

namespace Configs {
    // Stateless; safe from any thread.
    class IpScansRepo {
    private:
        // Writes hold db.WriteMutex(), shared with IpListsRepo on the same connection.
        Database &db;

        void createTables() const;

        [[nodiscard]] bool ipScansColumnExists(const char *columnName) const;

        [[nodiscard]] std::shared_ptr<IpScan> ipScanFromRow(SQLite::Statement &stmt) const;

        [[nodiscard]] int NewIpScanID() const;

    public:
        explicit IpScansRepo(Database &database);

        [[nodiscard]] static std::shared_ptr<IpScan> NewIpScan(IpScan::Kind kind);

        bool AddIpScan(std::shared_ptr<IpScan> &scan);

        [[nodiscard]] std::shared_ptr<IpScan> GetIpScan(int id) const;

        [[nodiscard]] QList<std::shared_ptr<IpScan>> GetAllIpScans() const;

        bool Save(const std::shared_ptr<IpScan> &scan);

        void SaveProgress(int id, IpScan::Mode mode, quint64 cursor, quint64 total, int found, int removed);

        void SaveStatus(int id, IpScan::Status status, const QString &lastError);

        // Empty clears it.
        void SaveTuneState(int id, const QJsonObject &state);

        // Rows only; ScanManager::DeleteScan also removes the lists the scan owns.
        void DeleteIpScan(int id);

        void UpdateIpScansOrder(const QList<int> &idsInOrder);

        // Rows still Running after a crash become Paused; call once at startup.
        void NormalizeInterrupted();
    };
} // namespace Configs
