#pragma once

#include "Database.h"

#include <QString>

namespace Configs {
    // "migration.*" marks a data migration as applied, "notice.*" a warning dismissed for good.
    namespace Markers {
        inline constexpr auto TunPrivateRangesIPv6 = "migration.tun_private_ranges_ipv6";
        inline constexpr auto HijackDeprecated = "notice.hijack_deprecated";
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
