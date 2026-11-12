#pragma once

#include <QList>
#include <QString>

namespace Configs {
    struct IpListEntry {
        // Canonical: masked network, host addresses without a suffix ("1.2.3.4", "1.2.3.0/24", "2606:4700::/32").
        QString cidr;
        int port = 0;
        // Scan result lists only; 0 = unknown.
        int latencyMs = 0;
        // DNS resolver scan results only; 0 = unknown. maxQName: dnstt max_qname_len or xdns lenLimit the resolver forwards.
        int maxQName = 0;
        int uploadMtu = 0;
        int downloadMtu = 0;
    };

    class IpList {
    public:
        enum class Role { User = 0, ScanResult = 1, ScanSnapshot = 2 };
        enum class SourceKind { Manual = 0, Url = 1, RuleSet = 2 };

        int id = -1;
        QString name;
        // The scan that owns this list (result or snapshot); -1 for user lists.
        int related_test_id = -1;
        Role role = Role::User;
        SourceKind source_kind = SourceKind::Manual;
        // Url: an http(s) URL serving text or a rule-set. RuleSet: a built-in key ("geoip-ir") or a rule-set URL.
        QString source;
        bool auto_update = false;
        // Minutes; values below 30 are treated as 30.
        int update_interval = 1440;
        qint64 last_update = 0;
        QString last_error;

        // Header loads leave entries empty and fill entryCount; IpListsRepo::GetIpList fills both.
        QList<IpListEntry> entries;
        int entryCount = 0;
        bool entriesLoaded = false;

        [[nodiscard]] bool IsRemote() const { return source_kind != SourceKind::Manual && !source.isEmpty(); }
        [[nodiscard]] bool IsHidden() const { return role == Role::ScanSnapshot; }
    };
} // namespace Configs
