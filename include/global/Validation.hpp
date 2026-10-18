#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

// Every check returns an empty string for acceptable text, otherwise one translated sentence without a field label.
// Grammars mirror the core's own parsers on trimmed text; whitespace-only text counts as empty. Safe on any thread.
// Callers store trimmed text; the core does not trim.
namespace Validation {
    enum class Severity { Error, Warning };

    enum class Empty { Allowed, Required };

    enum class Family { Any, V4, V6 };

    // `value` is shown to the user next to the message: never put a password, key or UUID in it.
    struct Issue {
        Severity severity = Severity::Error;
        QString field;
        QString message;
        QString value;
    };

    using Issues = QList<Issue>;

    using Rule = std::function<QString(const QString &text)>;

    bool HasError(const Issues &issues);

    // Optional '-', decimal digits only, overflow-checked.
    std::optional<qint64> ToInt(const QString &text);

    QString Required(const QString &text);

    QString Int(const QString &text, qint64 min, qint64 max, Empty empty = Empty::Allowed);

    QString Port(const QString &text, Empty empty = Empty::Required);

    enum class RangeSep { Colon, Dash };

    // sing-box colon ranges: checkOrder = false plus PortRangeOrder as a warning; Xray and mieru dash ranges keep checkOrder = true.
    struct PortRangeStyle {
        RangeSep sep = RangeSep::Colon;
        bool allowSingle = false;
        bool allowOpenEnds = false;
        bool checkOrder = true;
    };

    QString PortRange(const QString &text, const PortRangeStyle &style = {});

    // Only the start > end message (the core accepts such a range, which matches nothing); empty unless the text is a well-formed range.
    QString PortRangeOrder(const QString &text, const PortRangeStyle &style = {});

    // "n" or "a-b" with a <= b (either order with allowReversed), both within [min, max].
    QString IntOrRange(const QString &text, qint64 min, qint64 max, Empty empty = Empty::Allowed, bool allowReversed = false);

    struct DurationRule {
        Empty empty = Empty::Allowed;
        bool allowNegative = false;
        bool wholeSeconds = false;
        std::optional<qint64> minNs;
        std::optional<qint64> maxNs;
    };

    // sing my_time.ParseDuration: optional sign, "0", or <number><unit> segments with ns/us/µs/μs/ms/s/m/h/d.
    std::optional<qint64> ParseDurationNs(const QString &text);

    QString Duration(const QString &text, const DurationRule &rule = {});

    // sing byteformats MemoryBytes; a bare integer is valid only where the builder emits it as a JSON number.
    QString MemoryBytes(const QString &text, bool allowBareInteger, Empty empty = Empty::Allowed);

    // Go netip rules: four decimal octets without leading zeros, IPv6 without a zone.
    QString Ip(const QString &text, Family family = Family::Any, Empty empty = Empty::Required);

    QString Cidr(const QString &text, Family family = Family::Any, bool allowBareIp = false, Empty empty = Empty::Required);

    // IP literal (bracketed IPv6 allowed) or DNS name; no scheme, path, port or whitespace.
    QString Host(const QString &text, Empty empty = Empty::Required);

    QString HostPort(const QString &text, bool portRequired, Empty empty = Empty::Required);

    QString Base64Key(const QString &text, int bytes, bool urlAlphabet = false, bool padded = true, Empty empty = Empty::Required);

    QString Base64(const QString &text, Empty empty = Empty::Allowed);

    QString Hex(const QString &text, int minChars, int maxChars, bool evenLength = true, Empty empty = Empty::Allowed);

    // gofrs/uuid FromString: canonical, 32 hex digits, {braced} or urn:uuid: forms.
    QString Uuid(const QString &text, Empty empty = Empty::Required);

    // Xray uuid.ParseString: a 1-30 byte custom id, or a 32-36 character UUID with optional dashes.
    QString XrayId(const QString &text, Empty empty = Empty::Required);

    // Tolerant parsing like Qt's network stack; needs a host and a scheme from the list (any scheme when it is empty).
    QString Url(const QString &text, const QStringList &schemes, Empty empty = Empty::Allowed);

    QString OneOf(const QString &text, const QStringList &allowed, Empty empty = Empty::Allowed,
                  Qt::CaseSensitivity cs = Qt::CaseSensitive);

    QString HttpToken(const QString &text, Empty empty = Empty::Allowed);

    QString Regex(const QString &pattern);

    // PCRE validity of a copy rewritten where Go is laxer, plus what RE2 refuses; known misses: \p{Alphabetic}, \p{Grek}, [[:a]b:]] pass; a*(?s)*, \x{D800} fail.
    QString GoRegex(const QString &pattern);

    QString JsonObject(const QString &text, Empty empty = Empty::Allowed);

    enum class Split { Comma, Newline, CommaOrNewline, Whitespace };

    // Trimmed, empty items dropped.
    QStringList Items(const QString &text, Split split);

    // The first bad item as "Item 2 ("x"): <message>", or "Item 2: <message>" without echoItem (for keys and other secrets).
    QString EachItem(const QString &text, Split split, const Rule &item, Empty empty = Empty::Allowed, bool echoItem = true);

    namespace Rules {
        Rule Required();
        Rule Int(qint64 min, qint64 max, Empty empty = Empty::Allowed);
        Rule Port(Empty empty = Empty::Required);
        Rule PortRange(PortRangeStyle style = {});
        Rule PortRangeOrder(PortRangeStyle style = {});
        Rule IntOrRange(qint64 min, qint64 max, Empty empty = Empty::Allowed, bool allowReversed = false);
        Rule Duration(DurationRule rule = {});
        Rule MemoryBytes(bool allowBareInteger, Empty empty = Empty::Allowed);
        Rule Ip(Family family = Family::Any, Empty empty = Empty::Required);
        Rule Cidr(Family family = Family::Any, bool allowBareIp = false, Empty empty = Empty::Required);
        Rule Host(Empty empty = Empty::Required);
        Rule HostPort(bool portRequired, Empty empty = Empty::Required);
        Rule Base64Key(int bytes, bool urlAlphabet = false, bool padded = true, Empty empty = Empty::Required);
        Rule Base64(Empty empty = Empty::Allowed);
        Rule Hex(int minChars, int maxChars, bool evenLength = true, Empty empty = Empty::Allowed);
        Rule Uuid(Empty empty = Empty::Required);
        Rule XrayId(Empty empty = Empty::Required);
        Rule Url(QStringList schemes, Empty empty = Empty::Allowed);
        Rule OneOf(QStringList allowed, Empty empty = Empty::Allowed, Qt::CaseSensitivity cs = Qt::CaseSensitive);
        Rule HttpToken(Empty empty = Empty::Allowed);
        Rule Regex();
        Rule GoRegex();
        Rule JsonObject(Empty empty = Empty::Allowed);
        Rule List(Split split, Rule item, Empty empty = Empty::Allowed, bool echoItem = true);
        Rule AllOf(QList<Rule> rules);
    }
}
