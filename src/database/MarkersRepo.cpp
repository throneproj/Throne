#include "include/database/MarkersRepo.h"

namespace Configs {
    MarkersRepo::MarkersRepo(Database &database) : db(database) {
        createTables();
    }

    void MarkersRepo::createTables() const {
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS markers (
                key TEXT PRIMARY KEY,
                marked_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
            )
        )");
    }

    bool MarkersRepo::IsMarked(const QString &key) const {
        auto query = db.query("SELECT 1 FROM markers WHERE key = ?", key.toStdString());
        return query && query->executeStep();
    }

    void MarkersRepo::Mark(const QString &key) {
        db.exec("INSERT OR IGNORE INTO markers (key) VALUES (?)", key.toStdString());
    }
}
