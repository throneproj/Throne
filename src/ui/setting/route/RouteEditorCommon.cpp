#include "include/ui/setting/route/RouteEditorCommon.h"

#include "include/configs/ConfigBuild.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"
#include "include/ui/setting/ThemeManager.hpp"

#include <QCoreApplication>
#include <QFontMetrics>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRect>
#include <QRegularExpression>
#include <QSet>
#include <QSize>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <tuple>

namespace RouteEditor {
    namespace {
        QString tr(const char* text) {
            return QCoreApplication::translate("RouteEditor", text);
        }

        constexpr int kPillPadding = 9;

        double luminance(const QColor& color) {
            const auto channel = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
            const QColor rgb = color.toRgb();
            return 0.2126 * channel(rgb.redF()) + 0.7152 * channel(rgb.greenF()) + 0.0722 * channel(rgb.blueF());
        }

        double contrast(const QColor& a, const QColor& b) {
            const double la = luminance(a) + 0.05;
            const double lb = luminance(b) + 0.05;
            return la > lb ? la / lb : lb / la;
        }

        // Steps HSL lightness away from background until the ratio is met, as ThemeManager does for its tokens.
        QColor readable(QColor color, const QColor& background, double target) {
            const int direction = luminance(background) > 0.5 ? -1 : 1;
            for (int i = 0; i < 24 && contrast(color, background) < target; ++i) {
                int h, s, l, a;
                color.getHsl(&h, &s, &l, &a);
                const int next = qBound(0, l + direction * 10, 255);
                if (next == l) break;
                color.setHsl(h < 0 ? 0 : h, s, next, a);
            }
            return color;
        }

        struct ProfileCache {
            QList<QPair<QString, int>> choices;
            QHash<int, QString> labels;
            QHash<int, QString> names;
            quint64 profilesRevision = 0;
            quint64 groupsRevision = 0;
            bool built = false;
        };

        QString displayName(int profileId, const QString& stored) {
            if (!stored.trimmed().isEmpty()) return stored;
            const auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId);
            return profile != nullptr && profile->outbound != nullptr ? profile->outbound->DisplayName() : stored;
        }

        // Rebuilt only after profiles or group membership or order changed.
        const ProfileCache& profileCache() {
            static ProfileCache cache;
            const quint64 profilesRevision = Configs::dataManager->profilesRepo->Revision();
            const quint64 groupsRevision = Configs::dataManager->groupsRepo->Revision();
            if (cache.built && cache.profilesRevision == profilesRevision && cache.groupsRevision == groupsRevision) return cache;

            ProfileCache next;
            for (const auto& [id, name] : Configs::dataManager->profilesRepo->GetAllProfileIDNameMapped()) next.names.insert(id, displayName(id, name));
            for (const int groupId : Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
                const auto group = Configs::dataManager->groupsRepo->GetGroup(groupId);
                if (group == nullptr) continue;
                for (const int profileId : group->DisplayedProfiles()) {
                    const auto it = next.names.constFind(profileId);
                    if (it == next.names.cend()) continue;
                    const QString label = QStringLiteral("[%1] %2").arg(group->name, it.value());
                    next.choices.append({label, profileId});
                    if (!next.labels.contains(profileId)) next.labels.insert(profileId, label);
                }
            }
            next.profilesRevision = profilesRevision;
            next.groupsRevision = groupsRevision;
            next.built = true;
            cache = std::move(next);
            return cache;
        }

        struct OrGroup {
            const char* title;
            QStringList attrs;
        };

        // The rule items sing-box ORs together; every other item must match on its own.
        const QList<OrGroup>& orGroups() {
            static const QList<OrGroup> groups = {
                {QT_TRANSLATE_NOOP("RouteEditor", "Destination"),
                 {"domain", "domain_suffix", "domain_keyword", "domain_regex", "ip_cidr", "ip_is_private", "rule_set"}},
                {QT_TRANSLATE_NOOP("RouteEditor", "Destination port"), {"port", "port_range"}},
                {QT_TRANSLATE_NOOP("RouteEditor", "Source"), {"source_ip_cidr", "source_ip_is_private"}},
                {QT_TRANSLATE_NOOP("RouteEditor", "Source port"), {"source_port", "source_port_range"}},
            };
            return groups;
        }

        const QStringList& standaloneOrder() {
            static const QStringList order = {"process_name", "process_path", "process_path_regex", "package_name", "network",
                                              "ip_version",   "protocol",     "inbound",            "wifi_ssid",    "wifi_bssid"};
            return order;
        }

        bool inOrGroup(const QString& attr) {
            const auto& groups = orGroups();
            return std::any_of(groups.cbegin(), groups.cend(), [&attr](const OrGroup& group) { return group.attrs.contains(attr); });
        }
    } // namespace

    QList<Row> BuildRows(const Configs::RouteProfile& profile, bool includeEmptyLists) {
        QList<Row> rows;
        const auto& rules = profile.Rules;
        for (int i = 0; i < rules.size(); ++i) {
            const int type = rules[i]->type;
            if (!IsQuickListType(type)) {
                rows.append({type == Configs::endpointPreferredBy ? RowKind::Endpoint : RowKind::Rule, i, 1, Configs::proxy});
                continue;
            }
            const auto list = ListOfType(type);
            if (!rows.isEmpty() && rows.last().kind == RowKind::QuickList && rows.last().list == list) {
                ++rows.last().count;
                continue;
            }
            rows.append({RowKind::QuickList, i, 1, list});
        }
        if (!includeEmptyLists) {
            rows.removeIf([&rules](const Row& row) {
                const auto first = rules.cbegin() + row.first;
                return row.kind == RowKind::QuickList &&
                       std::all_of(first, first + row.count, [](const auto& rule) { return rule->isEmpty(); });
            });
        }
        return rows;
    }

    int RowOfRule(const QList<Row>& rows, int ruleIndex) {
        const auto it = std::upper_bound(rows.cbegin(), rows.cend(), ruleIndex, [](int index, const Row& row) { return index < row.first; });
        if (it == rows.cbegin()) return -1;
        const auto row = std::prev(it);
        return ruleIndex < row->first + row->count ? static_cast<int>(row - rows.cbegin()) : -1;
    }

    int RowNumber(const QList<Row>& rows, const Configs::RouteProfile& profile, const std::shared_ptr<Configs::RouteRule>& rule) {
        const auto index = profile.Rules.indexOf(rule);
        if (index < 0) return 0;
        const int row = RowOfRule(rows, static_cast<int>(index));
        return row < 0 ? 0 : row + 1;
    }

    void MoveRow(Configs::RouteProfile& profile, const QList<Row>& rows, int from, int to) {
        if (from < 0 || from >= rows.size()) return;
        to = std::clamp(to, 0, static_cast<int>(rows.size()) - 1);
        if (from == to) return;
        const Row& source = rows.at(from);
        const Row& target = rows.at(to);
        if (source.first + source.count > profile.Rules.size() || target.first + target.count > profile.Rules.size()) return;
        auto begin = profile.Rules.begin();
        if (from < to)
            std::rotate(begin + source.first, begin + source.first + source.count, begin + target.first + target.count);
        else
            std::rotate(begin + target.first, begin + source.first, begin + source.first + source.count);
    }

    const QList<Configs::simpleAction>& QuickListOrder() {
        static const QList<Configs::simpleAction> order = {Configs::bypass, Configs::proxy, Configs::block, Configs::warpBypass};
        return order;
    }

    QString ListName(Configs::simpleAction list) {
        return ThenLabel(ThenOfList(list));
    }

    bool IsQuickListType(int type) {
        return type >= Configs::simpleAddressProxy && type <= Configs::simpleProcessPathWarpBypass;
    }

    Configs::simpleAction ListOfType(int type) {
        switch (type) {
            case Configs::simpleAddressBypass:
            case Configs::simpleProcessNameBypass:
            case Configs::simpleProcessPathBypass: return Configs::bypass;
            case Configs::simpleAddressBlock:
            case Configs::simpleProcessNameBlock:
            case Configs::simpleProcessPathBlock: return Configs::block;
            case Configs::simpleAddressWarpBypass:
            case Configs::simpleProcessNameWarpBypass:
            case Configs::simpleProcessPathWarpBypass: return Configs::warpBypass;
            default: return Configs::proxy;
        }
    }

    EntryKind KindOfType(int type) {
        switch (type) {
            case Configs::simpleProcessNameProxy:
            case Configs::simpleProcessNameBypass:
            case Configs::simpleProcessNameBlock:
            case Configs::simpleProcessNameWarpBypass: return EntryKind::ProcessName;
            case Configs::simpleProcessPathProxy:
            case Configs::simpleProcessPathBypass:
            case Configs::simpleProcessPathBlock:
            case Configs::simpleProcessPathWarpBypass: return EntryKind::ProcessPath;
            default: return EntryKind::Address;
        }
    }

    Configs::ruleType QuickListType(Configs::simpleAction list, EntryKind kind) {
        switch (list) {
            case Configs::bypass:
                return kind == EntryKind::Address ? Configs::simpleAddressBypass
                     : kind == EntryKind::ProcessName ? Configs::simpleProcessNameBypass : Configs::simpleProcessPathBypass;
            case Configs::block:
                return kind == EntryKind::Address ? Configs::simpleAddressBlock
                     : kind == EntryKind::ProcessName ? Configs::simpleProcessNameBlock : Configs::simpleProcessPathBlock;
            case Configs::warpBypass:
                return kind == EntryKind::Address ? Configs::simpleAddressWarpBypass
                     : kind == EntryKind::ProcessName ? Configs::simpleProcessNameWarpBypass : Configs::simpleProcessPathWarpBypass;
            case Configs::proxy:
            default:
                return kind == EntryKind::Address ? Configs::simpleAddressProxy
                     : kind == EntryKind::ProcessName ? Configs::simpleProcessNameProxy : Configs::simpleProcessPathProxy;
        }
    }

    Then ThenOf(const Configs::RouteRule& rule) {
        if (rule.type == Configs::endpointPreferredBy) return Then::Profile;
        const QString& action = rule.action;
        if (action == QLatin1String("route") || action.isEmpty()) return ThenOfOutbound(rule.outboundID);
        if (action == QLatin1String("reject")) return Then::Block;
        if (action == QLatin1String("hijack-dns")) return Then::HijackDns;
        if (action == QLatin1String("sniff")) return Then::Sniff;
        if (action == QLatin1String("resolve")) return Then::Resolve;
        if (action == QLatin1String("route-options")) return Then::RouteOptions;
        if (action == QLatin1String("bypass")) return Then::Bypass;
        return Then::Other;
    }

    Then ThenOfList(Configs::simpleAction list) {
        switch (list) {
            case Configs::bypass: return Then::Direct;
            case Configs::block: return Then::Block;
            case Configs::warpBypass: return Then::NoWarp;
            case Configs::proxy:
            default: return Then::Proxy;
        }
    }

    Then ThenOfOutbound(int outboundID) {
        switch (outboundID) {
            case Configs::proxyID: return Then::Proxy;
            case Configs::directID: return Then::Direct;
            case Configs::blockID: return Then::Block;
            case Configs::warpBypassID: return Then::NoWarp;
            case -4: return Then::HijackDns;
            default: return outboundID >= 0 ? Then::Profile : Then::Other;
        }
    }

    void SetThen(Configs::RouteRule& rule, Then then, int profileId) {
        const auto route = [&rule](int outboundID) {
            rule.action = QStringLiteral("route");
            rule.outboundID = outboundID;
        };
        switch (then) {
            case Then::Proxy: route(Configs::proxyID); return;
            case Then::Direct: route(Configs::directID); return;
            case Then::NoWarp: route(Configs::warpBypassID); return;
            case Then::Profile:
                if (profileId >= 0) route(profileId);
                return;
            case Then::Bypass: {
                const int outbound = rule.outboundID;
                rule.action = QStringLiteral("bypass");
                if (outbound < 0 && outbound != Configs::proxyID && outbound != Configs::warpBypassID) rule.outboundID = Configs::directID;
                return;
            }
            case Then::Other: return;
            default:
                rule.action = ActionOf(then);
                // validate() warns about an outbound left set on an action that ignores it.
                rule.outboundID = Configs::directID;
                return;
        }
    }

    QString ActionOf(Then then) {
        switch (then) {
            case Then::Proxy:
            case Then::Direct:
            case Then::NoWarp:
            case Then::Profile: return QStringLiteral("route");
            case Then::Block: return QStringLiteral("reject");
            case Then::HijackDns: return QStringLiteral("hijack-dns");
            case Then::Sniff: return QStringLiteral("sniff");
            case Then::Resolve: return QStringLiteral("resolve");
            case Then::RouteOptions: return QStringLiteral("route-options");
            case Then::Bypass: return QStringLiteral("bypass");
            case Then::Other: break;
        }
        return {};
    }

    bool IsFinal(Then then) {
        return then != Then::Sniff && then != Then::Resolve && then != Then::RouteOptions;
    }

    bool ThenUses(Then then, const QString& attr) {
        const QStringList scope = Configs::RouteRule::action_scope(attr);
        if (scope.isEmpty()) return true;
        const QString action = ActionOf(then);
        return !action.isEmpty() && scope.contains(action);
    }

    QString ThenLabel(Then then) {
        switch (then) {
            case Then::Proxy: return tr("Proxy");
            case Then::Direct: return tr("Direct");
            case Then::NoWarp: return tr("Proxy without exit");
            case Then::Block: return tr("Block");
            case Then::HijackDns: return tr("Hijack DNS");
            case Then::Sniff: return tr("Sniff");
            case Then::Resolve: return tr("Resolve");
            case Then::RouteOptions: return tr("Set route options");
            case Then::Bypass: return tr("Bypass");
            case Then::Profile: return tr("Profile");
            case Then::Other: break;
        }
        return tr("Other action");
    }

    QString ThenLabel(const Configs::RouteRule& rule) {
        const Then then = ThenOf(rule);
        if (then == Then::Profile) return ProfileName(rule.outboundID);
        if (then == Then::Other) return rule.action;
        return ThenLabel(then);
    }

    const QList<QPair<QString, int>>& ProfileChoices() {
        return profileCache().choices;
    }

    bool ProfileExists(int profileId) {
        return profileCache().names.contains(profileId);
    }

    QString ProfileLabel(int profileId) {
        const auto& cache = profileCache();
        if (const auto it = cache.labels.constFind(profileId); it != cache.labels.cend()) return it.value();
        if (const auto it = cache.names.constFind(profileId); it != cache.names.cend()) return it.value();
        return tr("Missing profile (id %1)").arg(profileId);
    }

    int InnerHopOwner(const Configs::RouteProfile& profile, int endpointId) {
        for (const int owner : profile.innerHopEndpointIDs) {
            if (owner != endpointId && profile.endpointProfileIDs.contains(owner) && Configs::AuxEndpointInnerHops(owner).contains(endpointId)) return owner;
        }
        return -1;
    }

    QString ProfileName(int profileId) {
        const auto& names = profileCache().names;
        if (const auto it = names.constFind(profileId); it != names.cend()) return it.value();
        return tr("Missing profile (id %1)").arg(profileId);
    }

    Tone ToneOf(Then then) {
        switch (then) {
            case Then::Proxy: return Tone::Proxy;
            case Then::Direct: return Tone::Direct;
            case Then::NoWarp: return Tone::NoWarp;
            case Then::Block: return Tone::Block;
            case Then::Profile: return Tone::Profile;
            default: return Tone::Neutral;
        }
    }

    Tone ToneOf(const Configs::RouteRule& rule) {
        const Then then = ThenOf(rule);
        if (then == Then::Profile && !ProfileExists(rule.outboundID)) return Tone::Missing;
        return ToneOf(then);
    }

    ToneColors ColorsOf(Tone tone) {
        const auto& tokens = themeManager()->tokens;
        QColor base;
        bool outlined = false;
        switch (tone) {
            case Tone::Proxy: base = tokens.info; break;
            case Tone::Direct: base = tokens.success; break;
            case Tone::NoWarp: base = tokens.chartSeries[4]; break;
            case Tone::Block: base = tokens.danger; break;
            case Tone::Neutral: base = tokens.onSurface; break;
            case Tone::Profile:
                base = tokens.info;
                outlined = true;
                break;
            case Tone::Missing:
                base = tokens.danger;
                outlined = true;
                break;
        }
        ToneColors colors;
        if (outlined) {
            colors.fill = Qt::transparent;
            colors.border = Mix(base, tokens.surface, 0.5);
            colors.text = readable(base, tokens.surface, 4.5);
            return colors;
        }
        colors.fill = Mix(base, tokens.surface, tone == Tone::Neutral ? 0.09 : 0.15);
        colors.border = colors.fill;
        colors.text = readable(tone == Tone::Neutral ? Mix(base, tokens.surface, 0.85) : base, colors.fill, 4.5);
        return colors;
    }

    int TextWidth(const QFontMetrics& metrics, const QString& text) {
        return static_cast<int>(std::ceil(QFontMetricsF(metrics).horizontalAdvance(text)));
    }

    QSize PillSize(const QFontMetrics& metrics, const QString& text) {
        return {TextWidth(metrics, text) + 2 * kPillPadding + 2, metrics.height() + 4};
    }

    QColor Mix(const QColor& color, const QColor& base, double amount) {
        const QColor c = color.toRgb();
        const QColor b = base.toRgb();
        return QColor::fromRgbF(static_cast<float>(c.redF() * amount + b.redF() * (1 - amount)),
                                static_cast<float>(c.greenF() * amount + b.greenF() * (1 - amount)),
                                static_cast<float>(c.blueF() * amount + b.blueF() * (1 - amount)));
    }

    void PaintPill(QPainter* painter, const QRect& rect, const QString& text, Tone tone) {
        const ToneColors colors = ColorsOf(tone);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QRectF frame = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal radius = frame.height() / 2;
        painter->setPen(colors.border.alpha() > 0 ? QPen(colors.border, 1) : QPen(Qt::NoPen));
        painter->setBrush(colors.fill);
        painter->drawRoundedRect(frame, radius, radius);
        painter->setPen(colors.text);
        const QRect textRect = rect.adjusted(kPillPadding + 1, 0, -kPillPadding - 1, 0);
        painter->drawText(textRect, Qt::AlignCenter, painter->fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));
        painter->restore();
    }

    QString ConditionLabel(const QString& attr) {
        static const QHash<QString, const char*> labels = {
            {"ip_version", QT_TRANSLATE_NOOP("RouteEditor", "IP version")},
            {"network", QT_TRANSLATE_NOOP("RouteEditor", "Network")},
            {"protocol", QT_TRANSLATE_NOOP("RouteEditor", "Protocol (sniffed)")},
            {"inbound", QT_TRANSLATE_NOOP("RouteEditor", "Inbound")},
            {"domain", QT_TRANSLATE_NOOP("RouteEditor", "Domain")},
            {"domain_suffix", QT_TRANSLATE_NOOP("RouteEditor", "Domain suffix")},
            {"domain_keyword", QT_TRANSLATE_NOOP("RouteEditor", "Domain keyword")},
            {"domain_regex", QT_TRANSLATE_NOOP("RouteEditor", "Domain regex")},
            {"source_ip_cidr", QT_TRANSLATE_NOOP("RouteEditor", "Source IP range")},
            {"source_ip_is_private", QT_TRANSLATE_NOOP("RouteEditor", "Source private IP")},
            {"ip_cidr", QT_TRANSLATE_NOOP("RouteEditor", "IP range")},
            {"ip_is_private", QT_TRANSLATE_NOOP("RouteEditor", "Private IP")},
            {"source_port", QT_TRANSLATE_NOOP("RouteEditor", "Source port")},
            {"source_port_range", QT_TRANSLATE_NOOP("RouteEditor", "Source port range")},
            {"port", QT_TRANSLATE_NOOP("RouteEditor", "Port")},
            {"port_range", QT_TRANSLATE_NOOP("RouteEditor", "Port range")},
            {"process_name", QT_TRANSLATE_NOOP("RouteEditor", "Process name")},
            {"process_path", QT_TRANSLATE_NOOP("RouteEditor", "Process path")},
            {"process_path_regex", QT_TRANSLATE_NOOP("RouteEditor", "Process path regex")},
            {"package_name", QT_TRANSLATE_NOOP("RouteEditor", "Package name")},
            {"wifi_ssid", QT_TRANSLATE_NOOP("RouteEditor", "Wi-Fi name (SSID)")},
            {"wifi_bssid", QT_TRANSLATE_NOOP("RouteEditor", "Wi-Fi BSSID")},
            {"rule_set", QT_TRANSLATE_NOOP("RouteEditor", "Rule-set")},
            {"invert", QT_TRANSLATE_NOOP("RouteEditor", "Invert")},
            {"action", QT_TRANSLATE_NOOP("RouteEditor", "Action")},
            {"outbound", QT_TRANSLATE_NOOP("RouteEditor", "Outbound")},
            {"override_address", QT_TRANSLATE_NOOP("RouteEditor", "Override address")},
            {"override_port", QT_TRANSLATE_NOOP("RouteEditor", "Override port")},
            {"tls_fragment", QT_TRANSLATE_NOOP("RouteEditor", "Fragment TLS")},
            {"tls_fragment_fallback_delay", QT_TRANSLATE_NOOP("RouteEditor", "Fallback delay")},
            {"tls_record_fragment", QT_TRANSLATE_NOOP("RouteEditor", "Fragment TLS")},
            {"tls_spoof", QT_TRANSLATE_NOOP("RouteEditor", "Spoof SNI")},
            {"tls_spoof_method", QT_TRANSLATE_NOOP("RouteEditor", "Spoof method")},
            {"method", QT_TRANSLATE_NOOP("RouteEditor", "Reject with")},
            {"no_drop", QT_TRANSLATE_NOOP("RouteEditor", "Never switch to drop")},
            {"override_destination", QT_TRANSLATE_NOOP("RouteEditor", "Use the sniffed domain as the destination")},
            {"strategy", QT_TRANSLATE_NOOP("RouteEditor", "Strategy")},
        };
        const char* label = labels.value(attr, nullptr);
        return label != nullptr ? tr(label) : attr;
    }

    QString ShortConditionLabel(const QString& attr) {
        if (attr == QLatin1String("protocol")) return tr("Protocol");
        return ConditionLabel(attr);
    }

    QList<ConditionGroup> ConditionMenu() {
        QList<ConditionGroup> groups = {
            {tr("Destination"), {"domain", "domain_suffix", "domain_keyword", "domain_regex", "ip_cidr", "ip_is_private", "rule_set"}},
            {tr("Ports"), {"port", "port_range", "source_port", "source_port_range"}},
            {tr("Source"), {"source_ip_cidr", "source_ip_is_private"}},
            {tr("App"), {"process_name", "process_path", "process_path_regex"}},
            {tr("Connection"), {"network", "ip_version", "protocol", "inbound"}},
        };
        if (getOS() != Darwin) groups.append({tr("Wi-Fi"), {"wifi_ssid", "wifi_bssid"}});
        return groups;
    }

    QList<Box> BoxesFor(const QStringList& attrs) {
        QList<Box> boxes;
        for (const auto& group : orGroups()) {
            Box box{tr(group.title), {}, true};
            for (const QString& attr : group.attrs) {
                if (attrs.contains(attr)) box.attrs << attr;
            }
            if (!box.attrs.isEmpty()) boxes << box;
        }
        for (const QString& attr : standaloneOrder()) {
            if (attrs.contains(attr)) boxes << Box{ConditionLabel(attr), {attr}, false};
        }
        for (const QString& attr : attrs) {
            if (attr == QLatin1String("invert") || inOrGroup(attr) || standaloneOrder().contains(attr)) continue;
            boxes << Box{ConditionLabel(attr), {attr}, false};
        }
        return boxes;
    }

    QStringList SetConditions(const Configs::RouteRule& rule) {
        QStringList out;
        for (const QString& attr : Configs::RouteRule::get_attributes()) {
            if (attr == QLatin1String("invert") || !Configs::RouteRule::is_match_attribute(attr)) continue;
            if (!Configs::RouteRule::is_attribute_at_default(rule, attr)) out << attr;
        }
        return out;
    }

    QString ElideValue(const QString& text) {
        return text.size() > 40 ? text.left(39) + QChar(0x2026) : text;
    }

    QString ValueText(const QString& attr, const QString& value) {
        if (attr == QLatin1String("ip_is_private") || attr == QLatin1String("source_ip_is_private")) return tr("any private address");
        if (attr == QLatin1String("network")) return value.toUpper();
        if (attr == QLatin1String("ip_version")) {
            if (value == QLatin1String("4")) return QStringLiteral("IPv4");
            if (value == QLatin1String("6")) return QStringLiteral("IPv6");
            return value;
        }
        if (attr == QLatin1String("protocol")) {
            static const QHash<QString, QString> names = {
                {"tls", "TLS"}, {"http", "HTTP"}, {"quic", "QUIC"}, {"dns", "DNS"}, {"stun", "STUN"},
                {"bittorrent", "BitTorrent"}, {"dtls", "DTLS"}, {"ssh", "SSH"}, {"rdp", "RDP"}, {"ntp", "NTP"},
            };
            return names.value(value, value);
        }
        return value;
    }

    QString RuleSummary(const Configs::RouteRule& rule) {
        constexpr int shown = 3;
        QStringList parts;
        for (const Box& box : BoxesFor(SetConditions(rule))) {
            for (const QString& attr : box.attrs) {
                const QString label = ConditionLabel(attr);
                if (Configs::RouteRule::get_input_type(attr) == Configs::trufalse) {
                    parts << label;
                    continue;
                }
                QStringList values;
                int count = 0;
                for (const QString& raw : rule.get_current_value_string(attr)) {
                    const QString value = raw.trimmed();
                    if (value.isEmpty()) continue;
                    if (count++ < shown) values << ValueText(attr, value);
                }
                QString text = label + QLatin1Char(' ') + values.join(QStringLiteral(", "));
                if (count > shown) text = tr("%1 and %2 more").arg(text).arg(count - shown);
                parts << text;
            }
        }
        if (parts.isEmpty()) return tr("All traffic");
        const QString summary = parts.join(QStringLiteral(" · "));
        return rule.invert ? tr("Not: %1").arg(summary) : summary;
    }

    QList<Configs::RuleIssue> RuleProblems(const Configs::RouteRule& rule) {
        QList<Configs::RuleIssue> out;
        const bool catchAll = rule.type == Configs::custom && IsFinal(ThenOf(rule)) && !rule.has_match_conditions();
        for (auto issue : rule.validate()) {
            // validate()'s own catch-all warning; the error below replaces it.
            if (catchAll && issue.attr == QLatin1String("action") && issue.severity == Validation::Severity::Warning && issue.related.isEmpty()) continue;
            if (rule.disabled) issue.severity = Validation::Severity::Warning;
            out << issue;
        }
        if (catchAll && !rule.disabled) {
            out.prepend({Validation::Severity::Error, QStringLiteral("action"),
                         tr("This rule has no conditions, so it would catch all traffic. Add a condition, or turn the rule off."), {}, {}});
        }
        return out;
    }

    namespace {
        enum class SplitMode { Words, Commas, Lines };

        SplitMode splitModeOf(const QString& attr) {
            static const QSet<QString> lines = {"domain_regex", "process_path", "process_path_regex", "wifi_ssid"};
            if (lines.contains(attr)) return SplitMode::Lines;
            if (attr == QLatin1String("process_name")) return SplitMode::Commas;
            return SplitMode::Words;
        }

        bool isDomainAttr(const QString& attr) {
            return attr == QLatin1String("domain") || attr == QLatin1String("domain_suffix") || attr == QLatin1String("domain_keyword");
        }

        bool isRangeAttr(const QString& attr) {
            return attr == QLatin1String("port_range") || attr == QLatin1String("source_port_range");
        }

        QString aceDomain(const QString& domain) {
            if (std::all_of(domain.cbegin(), domain.cend(), [](QChar c) { return c.unicode() < 0x80; })) return domain;
            const qsizetype dot = domain.startsWith(u'.') ? 1 : 0;
            const QByteArray ace = QUrl::toAce(domain.mid(dot));
            return ace.isEmpty() ? domain : domain.left(dot) + QString::fromLatin1(ace);
        }

        // Only \\, \", \/ and \uXXXX count: \n, \t or \b are as likely to be a Windows path such as C:\new\bin.
        bool jsonEscaped(const QString& inner) {
            bool escaped = false;
            for (qsizetype i = 0; i < inner.size(); ++i) {
                if (inner.at(i) != u'\\') continue;
                const QChar next = i + 1 < inner.size() ? inner.at(i + 1) : QChar();
                bool hex = false;
                if (next == u'u' && i + 5 < inner.size()) std::ignore = QStringView(inner).mid(i + 2, 4).toUInt(&hex, 16);
                if (next == u'\\' || next == u'"' || next == u'/') {
                    ++i;
                } else if (hex) {
                    i += 5;
                } else {
                    return false;
                }
                escaped = true;
            }
            return escaped;
        }

        QStringList splitCommas(const QString& line) {
            static const QRegularExpression separators(QStringLiteral("[,;\\t]+"));
            QStringList words;
            for (const QString& piece : line.split(separators, Qt::SkipEmptyParts)) {
                const QString word = Unquote(piece, true);
                if (!word.isEmpty()) words << word;
            }
            return words;
        }
    } // namespace

    QStringList ValueLines(const QString& text) {
        static const QRegularExpression breaks(QStringLiteral("[\\r\\n\\x{2028}\\x{2029}]"));
        static const QRegularExpression bullet(QStringLiteral("^[-*+\\x{2022}]\\s+"));
        QStringList lines;
        for (const QString& raw : text.split(breaks, Qt::SkipEmptyParts)) {
            QString line = raw.trimmed();
            if (line.startsWith(u'#') || line.startsWith(u'!') || line.startsWith(QLatin1String("//"))) continue;
            line = line.remove(bullet).trimmed();
            if (!line.isEmpty()) lines << line;
        }
        return lines;
    }

    QStringList SplitWords(const QString& line) {
        static const QRegularExpression separators(QStringLiteral("[,;\\s]+"));
        static const QRegularExpression jsonKey(QStringLiteral("^\"[^\"]*\"\\s*:$"));
        QStringList words;
        for (const QString& piece : line.section(u'#', 0, 0).split(separators, Qt::SkipEmptyParts)) {
            if (piece.contains(u'{') || piece.contains(u'}') || jsonKey.match(piece).hasMatch()) continue;
            const QString word = Unquote(piece, true);
            if (!word.isEmpty()) words << word;
        }
        return words;
    }

    void DropHostsAddress(QStringList& words) {
        if (words.size() < 2 || HostSuffix(words.at(1)).isEmpty()) return;
        const QString& first = words.first();
        if (first == QLatin1String("0.0.0.0") || first == QLatin1String("127.0.0.1") || first == QLatin1String("::") || first == QLatin1String("::1"))
            words.removeFirst();
    }

    QString Unquote(const QString& text, bool brackets) {
        QString t = text.trimmed();
        if (brackets) {
            qsizetype from = 0;
            qsizetype to = t.size();
            while (from < to && t.at(from) == u'[') ++from;
            while (to > from && t.at(to - 1) == u']') --to;
            t = t.mid(from, to - from).trimmed();
        }
        const auto quote = [](QChar c) { return c == u'"' || c == u'\'' || c == u'`'; };
        if (t.size() >= 3 && t.endsWith(u',') && quote(t.front()) && t.at(t.size() - 2) == t.front()) t.chop(1);
        if (t.size() < 2 || !quote(t.front()) || t.back() != t.front()) return t;
        if (t.front() == u'"' && jsonEscaped(t.mid(1, t.size() - 2))) {
            const QJsonDocument doc = QJsonDocument::fromJson("[" + t.toUtf8() + "]");
            if (doc.isArray() && doc.array().size() == 1 && doc.array().first().isString()) return doc.array().first().toString().trimmed();
        }
        return t.mid(1, t.size() - 2).trimmed();
    }

    QString ClashRuleValue(const QString& line, QString* kind) {
        static const QRegularExpression rule(
            QStringLiteral("^(DOMAIN(?:-SUFFIX|-KEYWORD|-REGEX)?|IP-CIDR6?|SRC-IP-CIDR|DST-PORT|SRC-PORT|PROCESS-NAME|PROCESS-PATH)\\s*,\\s*([^,]+)"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = rule.match(line);
        if (!match.hasMatch()) match = rule.match(Unquote(line, false));
        if (!match.hasMatch()) return {};
        if (kind != nullptr) *kind = match.captured(1).toUpper();
        return Unquote(match.captured(2), false);
    }

    QStringList SplitValues(const QString& attr, const QString& text) {
        QStringList out;
        QSet<QString> seen;
        const auto add = [&](const QString& raw) {
            const QString value = TidyValue(attr, raw);
            if (value.isEmpty() || seen.contains(value)) return;
            seen.insert(value);
            out << value;
        };
        const QString whole = text.trimmed();
        if (whole.startsWith(u'[') || whole.startsWith(u'{')) {
            const QJsonDocument doc = QJsonDocument::fromJson(whole.toUtf8());
            QJsonArray items = doc.array();
            if (doc.isObject()) {
                const QJsonValue field = doc.object().value(attr);
                items = field.isArray() ? field.toArray() : field.isString() || field.isDouble() ? QJsonArray{field} : QJsonArray();
            }
            for (const QJsonValue& item : std::as_const(items)) {
                if (item.isString()) add(item.toString());
                else if (item.isDouble()) add(QString::number(item.toInteger()));
            }
            if (!out.isEmpty()) return out;
        }
        const SplitMode mode = splitModeOf(attr);
        for (const QString& line : ValueLines(text)) {
            if (const QString clash = ClashRuleValue(line); !clash.isEmpty()) {
                add(clash);
                continue;
            }
            if (mode == SplitMode::Lines) {
                add(Unquote(line, false));
                continue;
            }
            QStringList words = mode == SplitMode::Words ? SplitWords(line) : splitCommas(line);
            DropHostsAddress(words);
            for (const QString& word : std::as_const(words)) add(word);
        }
        return out;
    }

    QString TidyValue(const QString& attr, const QString& value) {
        QString v = value.trimmed();
        if (isRangeAttr(attr)) return v.replace(u'-', u':');
        if (!isDomainAttr(attr)) return v;
        if (v.startsWith(QLatin1String("||"))) v = v.mid(2);
        if (v.endsWith(u'^')) v.chop(1);
        if (v.contains(QLatin1String("://"))) {
            const QUrl url(v);
            if (url.isValid() && !url.host().isEmpty()) v = url.host(QUrl::FullyEncoded);
        }
        // Left whole for ShapeAttr, which sends it to ip_cidr.
        if (IpShape(v)) return v;
        static const QRegularExpression hostPort(QStringLiteral("^([^:/]+):\\d+(?:/.*)?$"));
        if (const QRegularExpressionMatch match = hostPort.match(v); match.hasMatch()) v = match.captured(1);
        else if (v.contains(u'/')) v = v.section(u'/', 0, 0);
        if (attr == QLatin1String("domain_suffix")) {
            if (v.startsWith(QLatin1String("+."))) v = v.mid(2);
            else if (v.startsWith(QLatin1String("*."))) v = v.mid(1);
        }
        if (v.size() > 1 && v.endsWith(u'.')) v.chop(1);
        if (attr != QLatin1String("domain_keyword")) v = aceDomain(v);
        return v.toLower();
    }

    QString ShapeAttr(const QString& attr, const QString& value) {
        const bool domains = attr == QLatin1String("domain") || attr == QLatin1String("domain_suffix");
        const bool ruleSet = attr == QLatin1String("rule_set");
        const bool ips = attr == QLatin1String("ip_cidr");
        if ((domains || ruleSet) && IpShape(value)) return QStringLiteral("ip_cidr");
        if ((domains || ips) && RuleSetShape(value)) return QStringLiteral("rule_set");
        if (attr == QLatin1String("domain") && (value.startsWith(QLatin1String("*.")) || value.startsWith(QLatin1String("+."))))
            return QStringLiteral("domain_suffix");
        if ((ips || ruleSet) && !RuleSetShape(value)) {
            const QString host = TidyValue(QStringLiteral("domain_suffix"), value);
            if (!IpShape(host) && !HostSuffix(host).isEmpty()) return QStringLiteral("domain_suffix");
        }
        static const QRegularExpression single(QStringLiteral("^\\d+$"));
        static const QRegularExpression range(QStringLiteral("^(?=.*\\d)\\d*[-:]\\d*$"));
        if (attr == QLatin1String("port") && range.match(value).hasMatch()) return QStringLiteral("port_range");
        if (attr == QLatin1String("source_port") && range.match(value).hasMatch()) return QStringLiteral("source_port_range");
        if (attr == QLatin1String("port_range") && single.match(value).hasMatch()) return QStringLiteral("port");
        if (attr == QLatin1String("source_port_range") && single.match(value).hasMatch()) return QStringLiteral("source_port");
        return attr;
    }

    bool IpShape(const QString& text) {
        const QString address = text.section(u'/', 0, 0);
        // QHostAddress also reads shorthands such as 10.1, which the core does not.
        if (!address.contains(u':') && address.count(u'.') != 3) return false;
        QHostAddress host;
        return host.setAddress(address);
    }

    bool RuleSetShape(const QString& text) {
        return !text.trimmed().isEmpty() && Configs::RouteRule::check_value(QStringLiteral("rule_set"), text).message.isEmpty();
    }

    QString HostSuffix(const QString& text) {
        const QString host = text.startsWith(QLatin1String("*.")) ? text.mid(1) : text;
        if (!host.contains(u'.')) return {};
        bool named = false;
        for (const QChar c : host) {
            if (c.isLetterOrNumber()) named = true;
            else if (c != u'-' && c != u'_' && c != u'.') return {};
        }
        return named ? host.toLower() : QString();
    }
} // namespace RouteEditor
