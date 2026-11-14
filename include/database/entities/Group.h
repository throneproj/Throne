#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <algorithm>

#include <optional>

#include "include/database/entities/EndpointSource.h"
#include "include/database/entities/GroupSort.h"

namespace Configs
{
    struct SubUserInfo {
        bool valid = false;
        bool has_quota = false;
        qint64 upload = 0;
        qint64 download = 0;
        qint64 total = 0;
        qint64 expire = 0;
        QString title;
        QString web_url;
        QString support_url;
        QString announce;
        int server_interval = 0;

        [[nodiscard]] qint64 used() const { return upload + download; }
        [[nodiscard]] qint64 remaining() const { return (total > used()) ? (total - used()) : 0; }
        [[nodiscard]] double percentUsed() const {
            if (total <= 0) return 0.0;
            return std::clamp((static_cast<double>(used()) / static_cast<double>(total)) * 100.0, 0.0, 100.0);
        }
        [[nodiscard]] bool isExpired() const {
            if (expire <= 0) return false;
            return QDateTime::currentSecsSinceEpoch() > expire;
        }

        [[nodiscard]] QJsonObject toJson() const;
        static SubUserInfo fromJson(const QJsonObject &json);
    };

    SubUserInfo ParseSubUserInfo(const QString &info);

    enum class testBy : int {
        latency = 0,
        dlSpeed,
        ulSpeed,
        ipOut
    };

    enum class trafficBy : int {
        total = 0,
        dl,
        ul
    };

    enum class typeBy : int {
        byType = 0,
        bySecurity
    };

    // Doubles as the index of the Advanced dialog's send_hwid combo.
    enum class sendHwid : int {
        keepDefault = 0,
        on,
        off
    };

    // Persisted as ints: append only.
    enum class subTlsVersion : int {
        automatic = 0,
        tls12,
        tls13
    };

    enum class subHttpVersion : int {
        automatic = 0,
        http11
    };

    // Empty values inherit the global subscription settings.
    struct SubscriptionOptions {
        QString user_agent;
        std::optional<subTlsVersion> tls_version;
        std::optional<subHttpVersion> http_version;
        sendHwid send_hwid = sendHwid::keepDefault;
        QString hwid;
        QString hwid_os;
        QString hwid_os_version;
        QString hwid_model;
        // Minutes; 0 follows the global interval.
        int update_interval = 0;
        std::optional<bool> respect_server_interval;
        bool keep_working = false;
        bool remove_duplicates = false;
        bool remove_insecure = false;
        bool remove_invalid = false;
        bool url_test = false;
        // Follow-ups of url_test: ignored while it is off.
        bool remove_unavailable = false;
        bool sort_by_latency = false;
        // Case-insensitive regexes on the profile name, applied as a fetch is parsed; the exclusion wins.
        QString name_filter;
        QString name_exclude;
        // Subscription::FilterProtocols() ids.
        QStringList type_exclude;
        // "HK,JP" keeps, "!RU" drops; judged by an IP test after each update.
        QString country_filter;

        [[nodiscard]] bool SameFilters(const SubscriptionOptions &other) const;

        [[nodiscard]] QJsonObject ToJson() const;

        static SubscriptionOptions FromJson(const QJsonObject &json);
    };

    // A speed-test result such as "48.3 Mbps"; -1 for "N/A", 0 when empty.
    double bitrateToBps(const QString &str);

    // What changed under a group, so only a kept sort that reads it runs again.
    enum class SortInput {
        Membership,
        Config,
        TestResult,
        Traffic,
    };

    class Group {
    public:
        QMutex mutex;
        int id = -1;
        bool archive = false;
        bool skip_auto_update = false;
        bool auto_clear_unavailable = false;
        QString name = "";
        QString url = "";
        QString info = "";
        SubUserInfo sub_info;
        qint64 sub_last_update = 0;
        SubscriptionOptions sub_options;
        int front_proxy_id = -1;
        int landing_proxy_id = -1;
        // The routing profile this group's profiles are built with; -1 follows the global one.
        int route_id = -1;
        // Keep a routing profile from what the subscription ships (RouteProfile::source_gid names the group).
        bool import_sub_route = false;
        // Inherit (profiles keep their own addresses), Address or IpList.
        EndpointSource endpoint;

        QList<int> column_width;
        // Persisted column bits: type, address, name and traffic are visible by default.
        int visible_columns = 0x47;
        QList<int> calculated_column_width; // memory only, no need to save to db
        QList<int> profiles;
        // Last custom arrangement of all profiles; pinning does not change it.
        QList<int> custom_order;
        // Pinned membership and custom order, independent from the ordinary profiles.
        QList<int> pinned;
        int scroll_last_profile = -1;
        testBy test_sort_by = testBy::latency;
        trafficBy traffic_sort_by = trafficBy::total;
        typeBy type_sort_by = typeBy::byType;
        // The underlying order maintained by the active sort; custom_order is its saved baseline.
        GroupSortAction sort;
        // Memory only. Pairs of (profileID, row as displayed).
        QList<std::pair<int, int>> selectedProfilesIdIdxPairs;

        Group() = default;

        void clearCalculatedColumnWidth();

        [[nodiscard]] QList<int> Profiles() const;

        [[nodiscard]] QList<int> DisplayedProfiles() const;

        [[nodiscard]] bool SortReads(SortInput input) const;

        [[nodiscard]] bool KeepsSorted() const;

        void SetSort(GroupSortAction action);

        void RestoreOriginalOrder();

        // The sorts below load profiles, so call them off the UI thread. Each is true when the order moved.

        // Sorts by `action` and keeps the group sorted that way.
        bool SortProfiles(GroupSortAction action);

        // Re-applies the kept sort to all profiles.
        bool Resort();

        // Moves just the changed profiles into place; the rest must still be sorted. Falls back to Resort when many changed.
        bool Reposition(const QList<int>& changed);

        bool RemoveProfile(int ID);

        bool RemoveProfileBatch(const QList<int>& IDs);

        bool AddProfile(int ID);

        bool AddProfileBatch(const QList<int>& IDs);

        [[nodiscard]] bool HasProfile(int ID) const;

        [[nodiscard]] int PinnedCount() const;

        [[nodiscard]] bool IsPinned(int ID) const;

        // The unpinned ones of IDs join the end of the pinned block in list order; true when any was pinned.
        bool PinProfiles(const QList<int>& IDs);

        // Removes pins without changing the underlying order; returns the unpinned profiles.
        QList<int> UnpinProfiles(const QList<int>& IDs);

        // Drops above/below target, adopting its pinned state; target == row count means the unpinned end.
        // A sorted drag saves the resulting display as the custom order and stops sorting. Returns the new row or -1.
        int MoveProfile(int from, int target, bool below);

        // Drops missing or duplicate pins after a wholesale rewrite; true when any were removed.
        bool NormalizePins();
    };
}// namespace Configs
