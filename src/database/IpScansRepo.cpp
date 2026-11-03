#include "include/database/IpScansRepo.h"

#include <QJsonDocument>

#include <mutex>

namespace Configs {
    namespace {
        const std::string kIpScansRepoSelect =
            "SELECT id, name, kind, base_list_id, result_list_id, config_json, status, mode, snapshot_list_id, seed, "
            "cursor, total, spec_hash, rescan_snapshot_list_id, rescan_cursor, rescan_total, found, removed, last_error, "
            "started_at, finished_at FROM ip_scans";

        // Column order must match both the INSERT and the UPDATE; returns the next index.
        int ipScansRepoBindColumns(SQLite::Statement &stmt, int index, const IpScan &scan) {
            stmt.bind(index++, scan.name.toStdString());
            stmt.bind(index++, static_cast<int>(scan.kind));
            stmt.bind(index++, scan.base_list_id);
            stmt.bind(index++, scan.result_list_id);
            stmt.bind(index++, QJsonDocument(scan.config.ToJson()).toJson(QJsonDocument::Compact).toStdString());
            stmt.bind(index++, static_cast<int>(scan.status));
            stmt.bind(index++, static_cast<int>(scan.mode));
            stmt.bind(index++, scan.snapshot_list_id);
            stmt.bind(index++, static_cast<int64_t>(scan.seed));
            stmt.bind(index++, static_cast<int64_t>(scan.cursor));
            stmt.bind(index++, static_cast<int64_t>(scan.total));
            stmt.bind(index++, scan.spec_hash.toStdString());
            stmt.bind(index++, scan.rescan_snapshot_list_id);
            stmt.bind(index++, static_cast<int64_t>(scan.rescan_cursor));
            stmt.bind(index++, static_cast<int64_t>(scan.rescan_total));
            stmt.bind(index++, scan.found);
            stmt.bind(index++, scan.removed);
            stmt.bind(index++, scan.last_error.toStdString());
            stmt.bind(index++, static_cast<int64_t>(scan.started_at));
            stmt.bind(index++, static_cast<int64_t>(scan.finished_at));
            return index;
        }
    } // namespace

    IpScansRepo::IpScansRepo(Database &database) : db(database) {
        createTables();
    }

    void IpScansRepo::createTables() const {
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS ip_scans (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL DEFAULT '',
                kind INTEGER NOT NULL DEFAULT 0,
                sort_order INTEGER NOT NULL DEFAULT 0,
                base_list_id INTEGER NOT NULL DEFAULT -1,
                result_list_id INTEGER NOT NULL DEFAULT -1,
                config_json TEXT NOT NULL DEFAULT '{}',
                status INTEGER NOT NULL DEFAULT 0,
                mode INTEGER NOT NULL DEFAULT 0,
                snapshot_list_id INTEGER NOT NULL DEFAULT -1,
                seed INTEGER NOT NULL DEFAULT 0,
                cursor INTEGER NOT NULL DEFAULT 0,
                total INTEGER NOT NULL DEFAULT 0,
                spec_hash TEXT NOT NULL DEFAULT '',
                rescan_snapshot_list_id INTEGER NOT NULL DEFAULT -1,
                rescan_cursor INTEGER NOT NULL DEFAULT 0,
                rescan_total INTEGER NOT NULL DEFAULT 0,
                found INTEGER NOT NULL DEFAULT 0,
                removed INTEGER NOT NULL DEFAULT 0,
                last_error TEXT NOT NULL DEFAULT '',
                started_at INTEGER NOT NULL DEFAULT 0,
                finished_at INTEGER NOT NULL DEFAULT 0,
                created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
                updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
            )
        )");
    }

    bool IpScansRepo::ipScansColumnExists(const char *columnName) const {
        auto pragma = db.query("PRAGMA table_info(ip_scans)");
        if (!pragma) return false;
        while (pragma->executeStep()) {
            if (pragma->getColumn(1).getText() == std::string(columnName)) return true;
        }
        return false;
    }

    std::shared_ptr<IpScan> IpScansRepo::ipScanFromRow(SQLite::Statement &stmt) const {
        auto scan = std::make_shared<IpScan>();
        scan->id = stmt.getColumn(0).getInt();
        scan->name = QString::fromStdString(stmt.getColumn(1).getString());
        const int kind = stmt.getColumn(2).getInt();
        scan->kind = kind >= 0 && kind <= static_cast<int>(IpScan::Kind::Resolver) ? static_cast<IpScan::Kind>(kind) : IpScan::Kind::Generic;
        scan->base_list_id = stmt.getColumn(3).getInt();
        scan->result_list_id = stmt.getColumn(4).getInt();
        scan->config = IpScan::DefaultConfig(scan->kind);
        const auto config = QJsonDocument::fromJson(QByteArray::fromStdString(stmt.getColumn(5).getString()));
        if (config.isObject()) scan->config.FromJson(config.object());
        const int status = stmt.getColumn(6).getInt();
        scan->status = status >= 0 && status <= static_cast<int>(IpScan::Status::Failed) ? static_cast<IpScan::Status>(status)
                                                                                         : IpScan::Status::Idle;
        scan->mode = stmt.getColumn(7).getInt() == static_cast<int>(IpScan::Mode::RescanResult) ? IpScan::Mode::RescanResult
                                                                                                : IpScan::Mode::Initial;
        scan->snapshot_list_id = stmt.getColumn(8).getInt();
        scan->seed = static_cast<quint64>(stmt.getColumn(9).getInt64());
        scan->cursor = static_cast<quint64>(stmt.getColumn(10).getInt64());
        scan->total = static_cast<quint64>(stmt.getColumn(11).getInt64());
        scan->spec_hash = QString::fromStdString(stmt.getColumn(12).getString());
        scan->rescan_snapshot_list_id = stmt.getColumn(13).getInt();
        scan->rescan_cursor = static_cast<quint64>(stmt.getColumn(14).getInt64());
        scan->rescan_total = static_cast<quint64>(stmt.getColumn(15).getInt64());
        scan->found = stmt.getColumn(16).getInt();
        scan->removed = stmt.getColumn(17).getInt();
        scan->last_error = QString::fromStdString(stmt.getColumn(18).getString());
        scan->started_at = stmt.getColumn(19).getInt64();
        scan->finished_at = stmt.getColumn(20).getInt64();
        return scan;
    }

    int IpScansRepo::NewIpScanID() const {
        std::lock_guard lock(db.WriteMutex());
        try {
            const auto query = db.queryThrow("UPDATE entity_ids SET ip_scan_last_id = ip_scan_last_id + 1 RETURNING ip_scan_last_id");
            if (query->executeStep()) return query->getColumn(0).getInt();
        } catch (const std::exception &e) {
            NotifyError("IpScansRepo::NewIpScanID", e);
        }
        return 0;
    }

    std::shared_ptr<IpScan> IpScansRepo::NewIpScan(IpScan::Kind kind) {
        auto scan = std::make_shared<IpScan>();
        scan->kind = kind;
        scan->config = IpScan::DefaultConfig(kind);
        return scan;
    }

    bool IpScansRepo::AddIpScan(std::shared_ptr<IpScan> &scan) {
        if (!scan || scan->id >= 0) return false;
        std::lock_guard lock(db.WriteMutex());
        int id = 0;
        const bool ok = db.transaction("IpScansRepo::AddIpScan", [&] {
            id = NewIpScanID();
            if (id <= 0) return false;
            const auto stmt = db.queryThrow(
                "INSERT INTO ip_scans (id, name, kind, base_list_id, result_list_id, config_json, status, mode, "
                "snapshot_list_id, seed, cursor, total, spec_hash, rescan_snapshot_list_id, rescan_cursor, rescan_total, "
                "found, removed, last_error, started_at, finished_at, sort_order) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                "(SELECT COALESCE(MAX(sort_order), 0) + 1 FROM ip_scans))");
            stmt->bind(1, id);
            ipScansRepoBindColumns(*stmt, 2, *scan);
            stmt->exec();
            return true;
        });
        if (!ok) return false;
        scan->id = id;
        return true;
    }

    std::shared_ptr<IpScan> IpScansRepo::GetIpScan(int id) const {
        std::lock_guard lock(db.WriteMutex());
        try {
            const auto query = db.queryThrow(kIpScansRepoSelect + " WHERE id = ?", id);
            if (!query->executeStep()) return nullptr;
            return ipScanFromRow(*query);
        } catch (const std::exception &e) {
            NotifyError("IpScansRepo::GetIpScan", e);
            return nullptr;
        }
    }

    QList<std::shared_ptr<IpScan>> IpScansRepo::GetAllIpScans() const {
        QList<std::shared_ptr<IpScan>> scans;
        std::lock_guard lock(db.WriteMutex());
        try {
            const auto query = db.queryThrow(kIpScansRepoSelect + " ORDER BY sort_order, id");
            while (query->executeStep()) scans.append(ipScanFromRow(*query));
        } catch (const std::exception &e) {
            NotifyError("IpScansRepo::GetAllIpScans", e);
            scans.clear();
        }
        return scans;
    }

    bool IpScansRepo::Save(const std::shared_ptr<IpScan> &scan) {
        if (!scan || scan->id < 0) return false;
        std::lock_guard lock(db.WriteMutex());
        try {
            const auto stmt = db.queryThrow(
                "UPDATE ip_scans SET name = ?, kind = ?, base_list_id = ?, result_list_id = ?, config_json = ?, status = ?, "
                "mode = ?, snapshot_list_id = ?, seed = ?, cursor = ?, total = ?, spec_hash = ?, rescan_snapshot_list_id = ?, "
                "rescan_cursor = ?, rescan_total = ?, found = ?, removed = ?, last_error = ?, started_at = ?, finished_at = ?, "
                "updated_at = strftime('%s', 'now') WHERE id = ?");
            const int index = ipScansRepoBindColumns(*stmt, 1, *scan);
            stmt->bind(index, scan->id);
            return stmt->exec() > 0;
        } catch (const std::exception &e) {
            NotifyError("IpScansRepo::Save", e);
            return false;
        }
    }

    void IpScansRepo::SaveProgress(int id, IpScan::Mode mode, quint64 cursor, quint64 total, int found, int removed) {
        if (id < 0) return;
        std::lock_guard lock(db.WriteMutex());
        const bool rescan = mode == IpScan::Mode::RescanResult;
        db.exec(rescan ? "UPDATE ip_scans SET mode = ?, rescan_cursor = ?, rescan_total = ?, found = ?, removed = ?, "
                         "updated_at = strftime('%s', 'now') WHERE id = ?"
                       : "UPDATE ip_scans SET mode = ?, cursor = ?, total = ?, found = ?, removed = ?, "
                         "updated_at = strftime('%s', 'now') WHERE id = ?",
                static_cast<int>(mode), static_cast<qint64>(cursor), static_cast<qint64>(total), found, removed, id);
    }

    void IpScansRepo::SaveStatus(int id, IpScan::Status status, const QString &lastError) {
        if (id < 0) return;
        std::lock_guard lock(db.WriteMutex());
        db.exec("UPDATE ip_scans SET status = ?, last_error = ?, updated_at = strftime('%s', 'now') WHERE id = ?",
                static_cast<int>(status), lastError.toStdString(), id);
    }

    void IpScansRepo::DeleteIpScan(int id) {
        std::lock_guard lock(db.WriteMutex());
        db.exec("DELETE FROM ip_scans WHERE id = ?", id);
    }

    void IpScansRepo::UpdateIpScansOrder(const QList<int> &idsInOrder) {
        if (idsInOrder.isEmpty()) return;
        std::lock_guard lock(db.WriteMutex());
        db.transaction("IpScansRepo::UpdateIpScansOrder", [&] {
            const auto update = db.queryThrow("UPDATE ip_scans SET sort_order = ? WHERE id = ?");
            for (int i = 0; i < idsInOrder.size(); ++i) {
                update->bind(1, i + 1);
                update->bind(2, idsInOrder[i]);
                update->exec();
                update->reset();
            }
            return true;
        });
    }

    void IpScansRepo::NormalizeInterrupted() {
        std::lock_guard lock(db.WriteMutex());
        db.exec("UPDATE ip_scans SET status = ? WHERE status = ?",
                static_cast<int>(IpScan::Status::Paused), static_cast<int>(IpScan::Status::Running));
    }
} // namespace Configs
