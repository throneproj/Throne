#pragma once

#include "Database.h"

#include <QString>

namespace Configs {
    // "migration.*" marks a data migration as applied, "notice.*" a warning dismissed for good.
    namespace Markers {
        inline constexpr auto TunPrivateRangesIPv6 = "migration.tun_private_ranges_ipv6";
        inline constexpr auto DefaultIpLists = "migration.default_ip_lists";
        inline constexpr auto DefaultResolverLists = "migration.default_resolver_lists";
        inline constexpr auto WarpMasqueRanges = "migration.warp_masque_ranges";
    }

    class MarkersRepo {
    private:
        Database &db;

        void createTables() const;

    public:
        explicit MarkersRepo(Database &database);

        [[nodiscard]] bool IsMarked(const QString &key) const;

        void Mark(const QString &key);
    };
}
