#pragma once

#include <array>
#include <span>
#include <string_view>

#include <QString>
#include <QStringList>
#include <QStringView>

namespace Configs {
    class outbound;
}

// Ids and aliases are stored (profiles.type) and exposed (API, type_exclude): never change them.
namespace Configs::Protocols {
    enum Flag : unsigned {
        Serverless = 1u << 0,
        // Chain is Serverless but scans through its first hop.
        NoScan = 1u << 1,
        Advanced = 1u << 2,
        VpnEndpoint = 1u << 3,
        // The id is also the Go scanner's protocol name.
        DnsTunnel = 1u << 4,
        Composite = 1u << 5,
        RunsProgram = 1u << 6,
    };

    struct ProtocolInfo {
        std::string_view id{};
        // Matched by Find, so by Is, Canonical and NewOutboundByType too; NewProfile stores the type as given.
        std::string_view alias{};
        outbound *(*create)() = nullptr;
        unsigned flags = 0;
        // nullptr marks a free slot: GCC before 13 cannot read value-initialized std::string_view slots at compile time.
        std::array<const char *, 4> importSchemes{};
        std::array<const char *, 2> importSingBox{};
        std::array<const char *, 2> importClash{};
        // QT_TRANSLATE_NOOP in the Validation context.
        const char *foreignRefusal = nullptr;
        // 1-based slot in the subscription "Remove types" menu; 0 when not listed.
        int filterRank = 0;
        // QT_TRANSLATE_NOOP in the DialogEditGroupAdvanced context.
        const char *filterLabel = nullptr;
        // Removed by the "Remove types" entry of the row it names.
        std::string_view filterAs{};

        [[nodiscard]] constexpr bool has(Flag flag) const { return (flags & flag) != 0; }

        [[nodiscard]] QLatin1String Id() const { return QLatin1String(id.data(), qsizetype(id.size())); }
    };

    // Editor type-list order.
    std::span<const ProtocolInfo> All();

    // Exact id or alias; nullptr for "", unknown types, "tor" and the custom subtypes.
    const ProtocolInfo *Find(QStringView type);

    bool Is(QStringView type, Flag flag);

    QStringList IdsWith(Flag flag);

    QString Canonical(const QString &type);

    const ProtocolInfo *ForLink(std::string_view line);

    const ProtocolInfo *ForSingBox(QStringView type);

    const ProtocolInfo *ForClash(std::string_view type);
} // namespace Configs::Protocols
