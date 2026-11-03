#pragma once

#include <QString>

namespace Scanner::DefaultIpLists {
    // Untranslated on purpose: views find the default lists by name, whatever the UI language.
    inline const QString kCloudflareCdnName = QStringLiteral("Cloudflare CDN");
    inline const QString kFastlyCdnName = QStringLiteral("Fastly CDN");
    inline const QString kGcoreCdnName = QStringLiteral("Gcore CDN");
    inline const QString kWarpWireGuardName = QStringLiteral("Cloudflare WARP (WireGuard)");
    inline const QString kWarpMasqueName = QStringLiteral("Cloudflare WARP (MASQUE)");
    inline const QString kIranResolversName = QStringLiteral("Iran DNS resolvers");
    inline const QString kRussiaResolversName = QStringLiteral("Russia DNS resolvers");

    int Ensure(const QString &name);

    int EnsureAll();
} // namespace Scanner::DefaultIpLists
