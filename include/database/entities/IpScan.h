#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace Configs {
    struct ScanWarpOptions {
        // The WARP profile probed with its own settings; nothing in the scan overrides them.
        int profileId = -1;
    };

    struct ScanResolverOptions {
        int profileId = -1;
    };

    // WARP scans use only warp.profileId: their flow fixes ports, concurrency and the stop rule.
    struct ScanConfig {
        // Ignore = only `ports`, Merge = the entry's own port plus `ports`, List = only the entry's own port.
        enum class PortMode { Ignore = 0, Merge = 1, List = 2 };

        // Not editable; stored values stay so TargetSpecHash keeps paused scans resumable.
        QList<int> ports;
        PortMode portMode = PortMode::Merge;
        bool shuffle = true;
        bool scanIPv6 = false;
        // Completes after this many hits since the run started; 0 = no limit.
        int stopAfter = 0;
        int concurrency = 64;
        int spawnIntervalMs = 2;

        ScanWarpOptions warp;
        ScanResolverOptions resolver;

        [[nodiscard]] QString PortModeKey() const;

        [[nodiscard]] QJsonObject ToJson() const;
        // Missing keys keep the defaults of the object it is called on.
        void FromJson(const QJsonObject &object);
        // Hash of everything that decides the target order; a resume is only valid while it is unchanged.
        [[nodiscard]] QString TargetSpecHash(int baseListId) const;
    };

    class IpScan {
    public:
        enum class Kind { Warp = 1, Resolver = 2 };
        enum class Status { Idle = 0, Running = 1, Paused = 2, Completed = 3, Failed = 4 };
        enum class Mode { Initial = 0, RescanResult = 1 };

        int id = -1;
        QString name;
        Kind kind = Kind::Warp;
        int base_list_id = -1;
        int result_list_id = -1;
        ScanConfig config;

        Status status = Status::Idle;
        Mode mode = Mode::Initial;

        // Initial pass: iterates a snapshot of the base list taken when the pass started.
        int snapshot_list_id = -1;
        quint64 seed = 0;
        quint64 cursor = 0;
        quint64 total = 0;
        QString spec_hash;

        // Rescan pass: iterates a snapshot of the result list; failures are removed from the result list.
        int rescan_snapshot_list_id = -1;
        quint64 rescan_cursor = 0;
        quint64 rescan_total = 0;

        int found = 0;
        int removed = 0;
        QString last_error;
        qint64 started_at = 0;
        qint64 finished_at = 0;

        static ScanConfig DefaultConfig(Kind kind);

        [[nodiscard]] quint64 ActiveCursor() const { return mode == Mode::RescanResult ? rescan_cursor : cursor; }
        [[nodiscard]] quint64 ActiveTotal() const { return mode == Mode::RescanResult ? rescan_total : total; }
        [[nodiscard]] bool CanResumeInitial() const { return snapshot_list_id >= 0 && total > 0 && cursor < total; }
    };
} // namespace Configs
