#include "include/database/TrafficStatsRepo.h"
#include "include/global/Logger.hpp"
#include "include/global/Notice.hpp"

#include <QFile>
#include <QObject>

namespace Configs {

    TrafficStatsRepo::TrafficStatsRepo(Database& database) : db(database) {
        createTables();
    }

    std::string TrafficStatsRepo::bucketExpr(long long bucketSecs, long long utcOffsetSecs) {
        const std::string b = std::to_string(bucketSecs);
        const std::string off = std::to_string(utcOffsetSecs);
        // floor((bucket_start + off) / b) * b - off; the parens around off keep a negative (west-of-UTC) offset valid.
        return "((bucket_start + (" + off + ")) / " + b + ") * " + b + " - (" + off + ")";
    }

    void TrafficStatsRepo::createTables() {
        write("createTables", [&] {
            db.execThrow(R"(
                CREATE TABLE IF NOT EXISTS config_traffic_minute (
                    bucket_start INTEGER NOT NULL,
                    profile_id   INTEGER NOT NULL,
                    up           INTEGER NOT NULL DEFAULT 0,
                    down         INTEGER NOT NULL DEFAULT 0,
                    PRIMARY KEY (bucket_start, profile_id)
                )
            )");
            db.execThrow(R"(
                CREATE TABLE IF NOT EXISTS config_traffic_hour (
                    bucket_start INTEGER NOT NULL,
                    profile_id   INTEGER NOT NULL,
                    up           INTEGER NOT NULL DEFAULT 0,
                    down         INTEGER NOT NULL DEFAULT 0,
                    PRIMARY KEY (bucket_start, profile_id)
                )
            )");
            db.execThrow(R"(
                CREATE TABLE IF NOT EXISTS app_traffic_minute (
                    bucket_start INTEGER NOT NULL,
                    process_name TEXT NOT NULL,
                    up           INTEGER NOT NULL DEFAULT 0,
                    down         INTEGER NOT NULL DEFAULT 0,
                    PRIMARY KEY (bucket_start, process_name)
                )
            )");
            db.execThrow(R"(
                CREATE TABLE IF NOT EXISTS app_traffic_hour (
                    bucket_start INTEGER NOT NULL,
                    process_name TEXT NOT NULL,
                    up           INTEGER NOT NULL DEFAULT 0,
                    down         INTEGER NOT NULL DEFAULT 0,
                    PRIMARY KEY (bucket_start, process_name)
                )
            )");
            db.execThrow(R"(
                CREATE TABLE IF NOT EXISTS config_meta (
                    profile_id     INTEGER PRIMARY KEY,
                    name           TEXT,
                    group_name     TEXT,
                    type           TEXT,
                    server_address TEXT,
                    first_seen     INTEGER NOT NULL DEFAULT 0,
                    last_seen      INTEGER NOT NULL DEFAULT 0
                )
            )");
            db.execThrow(R"(
                CREATE TABLE IF NOT EXISTS app_meta (
                    process_name TEXT PRIMARY KEY,
                    last_path    TEXT,
                    first_seen   INTEGER NOT NULL DEFAULT 0,
                    last_seen    INTEGER NOT NULL DEFAULT 0
                )
            )");
        });
    }

    void TrafficStatsRepo::UpsertConfigMinuteBatch(const QList<ConfigTrafficRow>& rows) {
        if (rows.isEmpty()) return;
        write("UpsertConfigMinuteBatch", [&] {
            for (const auto& r : rows) {
                db.execThrow(
                    "INSERT INTO config_traffic_minute (bucket_start, profile_id, up, down) "
                    "VALUES (?, ?, ?, ?) "
                    "ON CONFLICT(bucket_start, profile_id) DO UPDATE SET "
                    "up = up + excluded.up, down = down + excluded.down",
                    r.bucket_start, r.profile_id, r.up, r.down);
            }
        });
    }

    void TrafficStatsRepo::UpsertAppMinuteBatch(const QList<AppTrafficRow>& rows) {
        if (rows.isEmpty()) return;
        write("UpsertAppMinuteBatch", [&] {
            for (const auto& r : rows) {
                db.execThrow(
                    "INSERT INTO app_traffic_minute (bucket_start, process_name, up, down) "
                    "VALUES (?, ?, ?, ?) "
                    "ON CONFLICT(bucket_start, process_name) DO UPDATE SET "
                    "up = up + excluded.up, down = down + excluded.down",
                    r.bucket_start, r.process_name.toStdString(), r.up, r.down);
            }
        });
    }

    void TrafficStatsRepo::UpsertConfigMeta(const ConfigMetaRow& m) {
        // On conflict last_seen and the mutable fields refresh, but the original first_seen is deliberately kept.
        write("UpsertConfigMeta", [&] {
            db.execThrow(
                "INSERT INTO config_meta "
                "(profile_id, name, group_name, type, server_address, first_seen, last_seen) "
                "VALUES (?, ?, ?, ?, ?, ?, ?) "
                "ON CONFLICT(profile_id) DO UPDATE SET "
                "name = excluded.name, group_name = excluded.group_name, type = excluded.type, "
                "server_address = excluded.server_address, last_seen = excluded.last_seen",
                m.profile_id, m.name.toStdString(), m.group_name.toStdString(), m.type.toStdString(),
                m.server_address.toStdString(), m.first_seen, m.last_seen);
        });
    }

    void TrafficStatsRepo::UpsertAppMeta(const QString& processName, const QString& lastPath, long long nowSecs) {
        write("UpsertAppMeta", [&] {
            db.execThrow(
                "INSERT INTO app_meta (process_name, last_path, first_seen, last_seen) "
                "VALUES (?, ?, ?, ?) "
                "ON CONFLICT(process_name) DO UPDATE SET "
                "last_path = excluded.last_path, last_seen = excluded.last_seen",
                processName.toStdString(), lastPath.toStdString(), nowSecs, nowSecs);
        });
    }

    void TrafficStatsRepo::RollupMinuteToHour(long long olderThanSecs) {
        write("RollupMinuteToHour", [&] {
            db.execThrow(
                "INSERT INTO config_traffic_hour (bucket_start, profile_id, up, down) "
                "SELECT (bucket_start / 3600) * 3600, profile_id, SUM(up), SUM(down) "
                "FROM config_traffic_minute WHERE bucket_start < ? "
                "GROUP BY (bucket_start / 3600) * 3600, profile_id "
                "ON CONFLICT(bucket_start, profile_id) DO UPDATE SET "
                "up = up + excluded.up, down = down + excluded.down",
                olderThanSecs);
            db.execThrow("DELETE FROM config_traffic_minute WHERE bucket_start < ?", olderThanSecs);
            db.execThrow(
                "INSERT INTO app_traffic_hour (bucket_start, process_name, up, down) "
                "SELECT (bucket_start / 3600) * 3600, process_name, SUM(up), SUM(down) "
                "FROM app_traffic_minute WHERE bucket_start < ? "
                "GROUP BY (bucket_start / 3600) * 3600, process_name "
                "ON CONFLICT(bucket_start, process_name) DO UPDATE SET "
                "up = up + excluded.up, down = down + excluded.down",
                olderThanSecs);
            db.execThrow("DELETE FROM app_traffic_minute WHERE bucket_start < ?", olderThanSecs);
        });
    }

    void TrafficStatsRepo::PruneHour(long long olderThanSecs) {
        write("PruneHour", [&] {
            db.execThrow("DELETE FROM config_traffic_hour WHERE bucket_start < ?", olderThanSecs);
            db.execThrow("DELETE FROM app_traffic_hour WHERE bucket_start < ?", olderThanSecs);
        });
    }

    QList<ConfigUsage> TrafficStatsRepo::QueryConfigUsage(long long fromSecs, long long toSecs) {
        QList<ConfigUsage> out;
        read("QueryConfigUsage", [&] {
            auto q = db.queryThrow(
                "SELECT profile_id, SUM(u), SUM(d) FROM ("
                "  SELECT profile_id, up AS u, down AS d FROM config_traffic_minute "
                "    WHERE bucket_start >= ? AND bucket_start < ? "
                "  UNION ALL "
                "  SELECT profile_id, up AS u, down AS d FROM config_traffic_hour "
                "    WHERE bucket_start >= ? AND bucket_start < ? "
                ") GROUP BY profile_id",
                fromSecs, toSecs, fromSecs, toSecs);
            while (q->executeStep()) {
                ConfigUsage u;
                u.profile_id = q->getColumn(0).getInt();
                u.up = q->getColumn(1).getInt64();
                u.down = q->getColumn(2).getInt64();
                out.append(u);
            }
        });
        return out;
    }

    QList<AppUsage> TrafficStatsRepo::QueryAppUsage(long long fromSecs, long long toSecs) {
        QList<AppUsage> out;
        read("QueryAppUsage", [&] {
            auto q = db.queryThrow(
                "SELECT process_name, SUM(u), SUM(d) FROM ("
                "  SELECT process_name, up AS u, down AS d FROM app_traffic_minute "
                "    WHERE bucket_start >= ? AND bucket_start < ? "
                "  UNION ALL "
                "  SELECT process_name, up AS u, down AS d FROM app_traffic_hour "
                "    WHERE bucket_start >= ? AND bucket_start < ? "
                ") GROUP BY process_name",
                fromSecs, toSecs, fromSecs, toSecs);
            while (q->executeStep()) {
                AppUsage u;
                u.process_name = QString::fromUtf8(q->getColumn(0).getText());
                u.up = q->getColumn(1).getInt64();
                u.down = q->getColumn(2).getInt64();
                out.append(u);
            }
        });
        return out;
    }

    QList<TrafficSeriesPoint> TrafficStatsRepo::QueryConfigSeries(long long fromSecs, long long toSecs, long long bucketSecs, long long utcOffsetSecs) {
        QList<TrafficSeriesPoint> out;
        if (bucketSecs <= 0) return out;
        // bucketSecs/utcOffsetSecs are internal numbers, safe to inline; shifting before the floor-divide snaps each bucket to the local calendar boundary, subtracting back returns that boundary's epoch.
        const std::string bkt = bucketExpr(bucketSecs, utcOffsetSecs);
        read("QueryConfigSeries", [&] {
            auto q = db.queryThrow(
                "SELECT " + bkt + " AS bkt, SUM(u), SUM(d) FROM ("
                "  SELECT bucket_start, up AS u, down AS d FROM config_traffic_minute "
                "    WHERE bucket_start >= ? AND bucket_start < ? "
                "  UNION ALL "
                "  SELECT bucket_start, up AS u, down AS d FROM config_traffic_hour "
                "    WHERE bucket_start >= ? AND bucket_start < ? "
                ") GROUP BY bkt ORDER BY bkt",
                fromSecs, toSecs, fromSecs, toSecs);
            while (q->executeStep()) {
                TrafficSeriesPoint p;
                p.bucket_start = q->getColumn(0).getInt64();
                p.up = q->getColumn(1).getInt64();
                p.down = q->getColumn(2).getInt64();
                out.append(p);
            }
        });
        return out;
    }

    QList<TrafficSeriesPoint> TrafficStatsRepo::QueryAppSeries(long long fromSecs, long long toSecs, long long bucketSecs, long long utcOffsetSecs) {
        QList<TrafficSeriesPoint> out;
        if (bucketSecs <= 0) return out;
        const std::string bkt = bucketExpr(bucketSecs, utcOffsetSecs);
        read("QueryAppSeries", [&] {
            auto q = db.queryThrow(
                "SELECT " + bkt + " AS bkt, SUM(u), SUM(d) FROM ("
                "  SELECT bucket_start, up AS u, down AS d FROM app_traffic_minute "
                "    WHERE bucket_start >= ? AND bucket_start < ? "
                "  UNION ALL "
                "  SELECT bucket_start, up AS u, down AS d FROM app_traffic_hour "
                "    WHERE bucket_start >= ? AND bucket_start < ? "
                ") GROUP BY bkt ORDER BY bkt",
                fromSecs, toSecs, fromSecs, toSecs);
            while (q->executeStep()) {
                TrafficSeriesPoint p;
                p.bucket_start = q->getColumn(0).getInt64();
                p.up = q->getColumn(1).getInt64();
                p.down = q->getColumn(2).getInt64();
                out.append(p);
            }
        });
        return out;
    }

    QList<ConfigMetaRow> TrafficStatsRepo::GetAllConfigMeta() {
        QList<ConfigMetaRow> out;
        read("GetAllConfigMeta", [&] {
            auto q = db.queryThrow(
                "SELECT profile_id, name, group_name, type, server_address, first_seen, last_seen FROM config_meta");
            while (q->executeStep()) {
                ConfigMetaRow m;
                m.profile_id = q->getColumn(0).getInt();
                m.name = QString::fromUtf8(q->getColumn(1).getText());
                m.group_name = QString::fromUtf8(q->getColumn(2).getText());
                m.type = QString::fromUtf8(q->getColumn(3).getText());
                m.server_address = QString::fromUtf8(q->getColumn(4).getText());
                m.first_seen = q->getColumn(5).getInt64();
                m.last_seen = q->getColumn(6).getInt64();
                out.append(m);
            }
        });
        return out;
    }

    QList<AppMetaRow> TrafficStatsRepo::GetAllAppMeta() {
        QList<AppMetaRow> out;
        read("GetAllAppMeta", [&] {
            auto q = db.queryThrow("SELECT process_name, last_path, first_seen, last_seen FROM app_meta");
            while (q->executeStep()) {
                AppMetaRow m;
                m.process_name = QString::fromUtf8(q->getColumn(0).getText());
                m.last_path = QString::fromUtf8(q->getColumn(1).getText());
                m.first_seen = q->getColumn(2).getInt64();
                m.last_seen = q->getColumn(3).getInt64();
                out.append(m);
            }
        });
        return out;
    }

    void TrafficStatsRepo::onFailure(const char* op, const DbError& err, bool trip) {
        NotifyError(op, err);
        if (!trip || disabled.exchange(true)) return;

        const bool rebuild = IsFatalDbError(err);
        if (rebuild) {
            QFile marker(QString::fromStdString(DbRebuildMarkerPath(db.Path())));
            if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) marker.write(err.what.c_str());
            else LOG_WARN(QString("could not write %1").arg(marker.fileName()));
        }

        const QString what = QString::fromStdString(err.what);
        LOG_ERROR(QString("traffic statistics paused for this session after %1 failed: %2%3")
                      .arg(QString::fromUtf8(op), what,
                           rebuild ? QString("; the database will be rebuilt at the next start") : QString()));
        PostNotice({NoticeLevel::Warning, QObject::tr("Traffic statistics paused"),
                    rebuild
                        ? QObject::tr("The statistics database is unusable (%1). Statistics are paused for this session and the file will be rebuilt when Throne restarts.").arg(what)
                        : QObject::tr("Writing statistics keeps failing (%1). Statistics are paused until Throne restarts.").arg(what)});
    }
}
