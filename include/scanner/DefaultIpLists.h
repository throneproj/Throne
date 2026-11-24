#pragma once

#include <QString>

namespace Scanner::DefaultIpLists {
    // Untranslated on purpose: views find the default lists by name, whatever the UI language.
    inline const QString kWarpWireGuardName = QStringLiteral("Cloudflare WARP (WireGuard)");
    inline const QString kWarpMasqueName = QStringLiteral("Cloudflare WARP (MASQUE)");
    inline const QString kIranResolversName = QStringLiteral("Iran DNS resolvers");
    inline const QString kRussiaResolversName = QStringLiteral("Russia DNS resolvers");

    int Ensure(const QString &name);

    int EnsureAll();

    // Puts the 162.159.198.0/24 and 199.0/24 seeds in the MASQUE list in place of the .1/.2 hosts they cover.
    // True when done or there is nothing to do (the list is gone); false when the rewrite failed.
    bool MigrateWarpMasqueRanges();

    // Gives the WARP WireGuard, Iran and Russia lists their 2026-11 seeds where they still hold the old seed exactly.
    // True when done or there is nothing to do; false when a rewrite failed.
    bool MigrateListSeedsNov2026();
} // namespace Scanner::DefaultIpLists
