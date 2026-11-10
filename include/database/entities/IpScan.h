#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace Configs {
    struct ScanIcmpOptions {
        bool enabled = false;
        int timeoutMs = 1000;
        int count = 1;
    };

    struct ScanTcpOptions {
        bool enabled = true;
        int timeoutMs = 2000;
        int attempts = 1;
    };

    struct ScanHttpOptions {
        bool enabled = false;
        bool tls = true;
        // SNI; also the default Host header.
        QString serverName;
        QString host;
        QString path = QStringLiteral("/");
        // GET | HEAD | NONE (TLS handshake only)
        QString method = QStringLiteral("GET");
        // 1.1 | 2 | 3
        QString httpVersion = QStringLiteral("1.1");
        // Empty = derived from httpVersion.
        QStringList alpn;
        // "" | 1.0 | 1.1 | 1.2 | 1.3
        QString minVersion;
        QString maxVersion;
        // "" = Go TLS; otherwise a uTLS fingerprint (chrome, firefox, edge, safari, 360, qq, ios, android, random, randomized).
        QString fingerprint;
        bool insecure = false;
        bool disableSni = false;
        bool fragment = false;
        int fragmentFallbackDelayMs = 0;
        bool recordFragment = false;
        bool mixedCaseSni = false;
        int timeoutMs = 3000;
    };

    struct ScanConfigTestOptions {
        bool enabled = false;
        int profileId = -1;
        // Empty = the global test URL.
        QString url;
        int timeoutMs = 3000;
        bool warmLatency = true;
    };

    struct ScanWarpOptions {
        // wireguard | amneziawg | masque
        QString mode = QStringLiteral("wireguard");
        // masque: 0 = HTTP/3 then HTTP/2, 1 = HTTP/3 only, 2 = HTTP/2 only
        int httpMode = 0;
        // generated | profile | builtin
        QString identity = QStringLiteral("generated");
        int profileId = -1;
        // Bean ExportToJson of the device registered for this scan; "wireguard" or "masque" in generatedMode.
        QJsonObject generatedIdentity;
        QString generatedMode;
        int mtu = 1280;
        // masque; empty = consumer-masque.cloudflareclient.com
        QString sni;
        // Overrides the identity source's own dial option.
        bool quicOpener = false;
        // amneziawg junk; Cloudflare runs vanilla WireGuard, so s1..s4 and h1..h4 never apply.
        int jc = 0;
        int jmin = 0;
        int jmax = 0;
        QString i1, i2, i3, i4, i5;
        QString url;
        int timeoutMs = 5000;
        bool warmLatency = true;
    };

    struct ScanResolverOptions {
        int profileId = -1;
        // dnstt: poll | qname | handshake; masterdns: quick | full; xdns: poll | qname. Empty = the protocol's default.
        QString level;
        // Bounds one resolver's whole probe; 0 = the level's default.
        int timeoutMs = 0;
    };

    struct ScanConfig {
        // Ignore = only `ports`, Merge = the entry's own port plus `ports`, List = only the entry's own port.
        enum class PortMode { Ignore = 0, Merge = 1, List = 2 };

        QList<int> ports;
        PortMode portMode = PortMode::Merge;
        bool shuffle = true;
        bool scanIPv6 = false;
        // Completes once the result list holds this many entries; 0 = no limit.
        int stopAfter = 0;
        // Every phase, the config/WARP test included.
        int concurrency = 64;
        int spawnIntervalMs = 2;

        ScanIcmpOptions icmp;
        ScanTcpOptions tcp;
        ScanHttpOptions http;
        ScanConfigTestOptions config;
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
        enum class Kind { Generic = 0, Warp = 1, Resolver = 2 };
        enum class Status { Idle = 0, Running = 1, Paused = 2, Completed = 3, Failed = 4 };
        enum class Mode { Initial = 0, RescanResult = 1 };

        int id = -1;
        QString name;
        Kind kind = Kind::Generic;
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
