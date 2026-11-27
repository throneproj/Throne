#pragma once

#include <QColor>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

#include <memory>

#include "include/database/entities/RouteProfile.h"

class QFontMetrics;
class QPainter;
class QRect;
class QSize;

// The routing profile editor's shared vocabulary: rows, quick lists, Then, tones and conditions.
namespace RouteEditor {
    enum class RowKind { Rule, QuickList, Endpoint };

    // One custom or endpoint rule, or one run of adjacent rules of the same quick list.
    struct Row {
        RowKind kind = RowKind::Rule;
        // The rules it stands for: profile.Rules[first, first + count).
        int first = 0;
        int count = 1;
        // QuickList rows only.
        Configs::simpleAction list = Configs::proxy;
    };

    // The "Everything else" row is not among them: it always follows the last one.
    QList<Row> BuildRows(const Configs::RouteProfile& profile);

    // The row holding profile.Rules[ruleIndex], -1 when none.
    int RowOfRule(const QList<Row>& rows, int ruleIndex);

    // 1-based, as the # column shows it; 0 when the rule is not in the profile.
    int RowNumber(const QList<Row>& rows, const Configs::RouteProfile& profile, const std::shared_ptr<Configs::RouteRule>& rule);

    // Moves rows[from] whole so that it becomes row to of the new order.
    void MoveRow(Configs::RouteProfile& profile, const QList<Row>& rows, int from, int to);

    // Direct, Proxy, Block, Proxy without exit.
    const QList<Configs::simpleAction>& QuickListOrder();
    QString ListName(Configs::simpleAction list);

    enum class EntryKind { Address, ProcessName, ProcessPath };
    bool IsQuickListType(int type);
    Configs::simpleAction ListOfType(int type);
    EntryKind KindOfType(int type);
    Configs::ruleType QuickListType(Configs::simpleAction list, EntryKind kind);

    // What a rule does with what it matches, as the Then picker offers it.
    enum class Then { Proxy, Direct, NoWarp, Block, HijackDns, Sniff, Resolve, RouteOptions, Bypass, Profile, Other };

    Then ThenOf(const Configs::RouteRule& rule);
    Then ThenOfList(Configs::simpleAction list);
    // The default route: proxy, direct, block or warp-bypass.
    Then ThenOfOutbound(int outboundID);
    // Writes action and outbound only; profileId is read for Then::Profile.
    void SetThen(Configs::RouteRule& rule, Then then, int profileId = -1);
    // The sing-box action a Then writes.
    QString ActionOf(Then then);
    // False for the actions after which matching goes on with the next rule.
    bool IsFinal(Then then);
    // Whether a scoped attribute (Connection options, reject, sniff and resolve settings) applies to that Then.
    bool ThenUses(Then then, const QString& attr);
    QString ThenLabel(Then then);
    // A Profile rule names its profile, Other its raw action.
    QString ThenLabel(const Configs::RouteRule& rule);

    // Every profile as "[Group] Name" with its id, in group tab order; rebuilt only after profiles or groups changed.
    const QList<QPair<QString, int>>& ProfileChoices();
    bool ProfileExists(int profileId);
    // "[Group] Name", or "Missing profile (id N)".
    QString ProfileLabel(int profileId);
    // The name alone, for pills.
    QString ProfileName(int profileId);
    // The listed endpoint whose inner hops include endpointId, or -1.
    int InnerHopOwner(const Configs::RouteProfile& profile, int endpointId);

    enum class Tone { Proxy, Direct, NoWarp, Block, Neutral, Profile, Missing };
    Tone ToneOf(Then then);
    // A rule's tone, Missing when its profile is gone.
    Tone ToneOf(const Configs::RouteRule& rule);

    struct ToneColors {
        QColor text;
        QColor fill;
        QColor border;
    };
    // From the current theme tokens, so read them when painting.
    ToneColors ColorsOf(Tone tone);
    // horizontalAdvance() rounded up: elidedText() elides a text even a fraction of a pixel wider than its width.
    int TextWidth(const QFontMetrics& metrics, const QString& text);
    QSize PillSize(const QFontMetrics& metrics, const QString& text);
    // Draws with the painter's font; pass a rect of PillSize().
    void PaintPill(QPainter* painter, const QRect& rect, const QString& text, Tone tone);
    // amount of color laid over base.
    QColor Mix(const QColor& color, const QColor& base, double amount);

    QString ConditionLabel(const QString& attr);
    // ConditionLabel where the condition tree's leaf shows the rest as a tooltip: "Protocol" for "Protocol (sniffed)".
    QString ShortConditionLabel(const QString& attr);

    struct ConditionGroup {
        QString title;
        QStringList attrs;
    };
    // The "+ Add condition" menu: no package_name, and no Wi-Fi on macOS.
    QList<ConditionGroup> ConditionMenu();

    // A box ORs its attributes and every box must match, as sing-box ANDs its rule items.
    struct Box {
        // The group's title, or the attribute's label for a box of its own.
        QString title;
        QStringList attrs;
        // Several attributes in one box ("Any of these"), not one attribute's values ("Any of these values").
        bool grouped = false;
    };
    QList<Box> BoxesFor(const QStringList& attrs);

    // The match attributes rule has a value in, invert excluded, in attribute-table order.
    QStringList SetConditions(const Configs::RouteRule& rule);
    // A value as a chip shows it: TCP, IPv6, any private address.
    QString ValueText(const QString& attr, const QString& value);
    // At most 40 characters, for a value quoted in a message.
    QString ElideValue(const QString& text);
    // "Rule set geoip-ir, geosite-ir · Port 443", the first few values of each condition only.
    QString RuleSummary(const Configs::RouteRule& rule);

    // validate(), an Error for an enabled final rule without conditions, and a disabled rule's errors as warnings.
    QList<Configs::RuleIssue> RuleProblems(const Configs::RouteRule& rule, bool profilePinsXray);

    // Text lines, trimmed, without blank lines, comment lines (#, !, //) and list bullets.
    QStringList ValueLines(const QString& text);
    // A line cut at commas, semicolons and whitespace, each piece without quotes or brackets; a '#' ends the line.
    QStringList SplitWords(const QString& line);
    // Drops the address before the name of a hosts file line, such as 0.0.0.0 in "0.0.0.0 ads.example".
    void DropHostsAddress(QStringList& words);
    // Drops wrapping quotes, decoding a JSON string; brackets also drops leading '[' and trailing ']'.
    QString Unquote(const QString& text, bool brackets);
    // The value of a Clash rule line such as DOMAIN-SUFFIX,example.com,PROXY, its type uppercased in kind; empty when line is not one.
    QString ClashRuleValue(const QString& line, QString* kind = nullptr);
    // The values a typed or pasted text holds for attr, tidied as attr stores them, without duplicates.
    QStringList SplitValues(const QString& attr, const QString& text);
    // The attribute of attr's OR box that value belongs in by its shape, such as ip_cidr for an address among domains; attr when none.
    QString ShapeAttr(const QString& attr, const QString& value);
    // value as attr stores it: a domain lowercased in ACE form without scheme or wildcard, a port range with ':'.
    QString TidyValue(const QString& attr, const QString& value);
    bool IpShape(const QString& text);
    // A built-in rule-set name or an http(s) URL of a .srs file.
    bool RuleSetShape(const QString& text);
    // The domain suffix a host name stands for, lowercased, "*.x" as ".x"; empty when text is not one.
    QString HostSuffix(const QString& text);
} // namespace RouteEditor
