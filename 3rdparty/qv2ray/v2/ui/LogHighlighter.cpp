#include "LogHighlighter.hpp"

#include <QHostAddress>
#include <QSet>

#include <algorithm>

namespace Qv2ray::ui {
    namespace {
        struct Span {
            int start = -1;
            int length = 0;
        };

        struct Prefix {
            LogLevel level = LogLevel::Info;
            Span time, levelWord, counter, connection, id;
            quint64 connectionId = 0;
            int bodyStart = 0;
        };

        // sing-box: optional "-0700 date time", LEVEL, optional "[secs]" counter, optional "[id duration]", optional "tag: ".
        const QRegularExpression &singBoxPrefix() {
            static const QRegularExpression re(QStringLiteral(
                R"re(^(?:([+-]\d{4} \d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d+)?) )?(TRACE|DEBUG|INFO|WARN|ERROR|FATAL|PANIC)(?=[\[ ]|$)(\[\d+\])? ?(?:(\[(\d+) \d[\d.m]*s\]) )?(?:[a-z][\w-]*(?:/[\w-]+)?(?:\[[^\]]*\])?: )?)re"));
            return re;
        }

        // Xray and Go's standard logger: "yyyy/mm/dd hh:mm:ss[.us]", optional "[Level]", "[id]" and "component/path: ".
        const QRegularExpression &xrayPrefix() {
            static const QRegularExpression re(QStringLiteral(
                R"re(^(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}(?:\.\d+)?) (?:\[(Debug|Info|Warning|Error)\] )?(?:(\[(\d+)\]) )?(?:(?:[\w-]+/)+[\w-]+: )?)re"));
            return re;
        }

        const QRegularExpression &throneParts() {
            static const QRegularExpression re(QStringLiteral(R"re(^\[([A-Za-z][A-Za-z ]{0,20})\] )re"));
            return re;
        }

        const QRegularExpression &bodyTokens() {
            static const QRegularExpression re(QStringLiteral(
                R"re((\b(?:NXDOMAIN|SERVFAIL|REFUSED|FORMERR|NOTIMP)\b)|(=> (?:reject|block)\b|\brejected\b))re"));
            return re;
        }

        // [IPv6], IPv4, bare IPv6 or a domain (group 4 = its TLD), then an optional :port or /prefix.
        // The TLD must be lowercase, so Go identifiers like net.OpError never match.
        const QRegularExpression &addressTokens() {
            static const QRegularExpression re(QStringLiteral(
                R"re((?<![\w.-])(?:\[([0-9A-Fa-f.]*:[0-9A-Fa-f:.]*(?:%[\w.-]+)?)\]|((?:\d{1,3}\.){3}\d{1,3})|(?<!:)((?:[0-9A-Fa-f]{0,4}:){2,7}(?:(?:\d{1,3}\.){3}\d{1,3}|[0-9A-Fa-f]{1,4})?(?:%[\w.-]+)?)|(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?\.)+([a-z][a-z0-9-]{0,61}[a-z0-9]))(?::\d{1,5}|/\d{1,3})?(?![\w(\[-]|[.:]\w))re"));
            return re;
        }

        bool isAddress(const QRegularExpressionMatch &match) {
            if (match.capturedStart(1) >= 0) return !QHostAddress(match.captured(1)).isNull();
            if (match.capturedStart(2) >= 0) return !QHostAddress(match.captured(2)).isNull();
            if (match.capturedStart(3) >= 0) {
                // A digit rules out hex words around "::"; parsing rules out clock times and MACs.
                const auto v6 = match.capturedView(3);
                return std::any_of(v6.begin(), v6.end(), [](QChar c) { return c.isDigit(); }) && !QHostAddress(v6.toString()).isNull();
            }
            static const QSet<QString> fileExtensions{
                QStringLiteral("bat"),  QStringLiteral("bin"),  QStringLiteral("cfg"),  QStringLiteral("conf"), QStringLiteral("cpp"),
                QStringLiteral("crt"),  QStringLiteral("css"),  QStringLiteral("dat"),  QStringLiteral("db"),   QStringLiteral("dll"),
                QStringLiteral("dylib"), QStringLiteral("exe"), QStringLiteral("go"),   QStringLiteral("gz"),   QStringLiteral("hpp"),
                QStringLiteral("html"), QStringLiteral("ini"),  QStringLiteral("js"),   QStringLiteral("json"), QStringLiteral("jsonc"),
                QStringLiteral("key"),  QStringLiteral("lock"), QStringLiteral("log"),  QStringLiteral("mmdb"), QStringLiteral("pem"),
                QStringLiteral("png"),  QStringLiteral("ps1"),  QStringLiteral("qm"),   QStringLiteral("qss"),  QStringLiteral("so"),
                QStringLiteral("srs"),  QStringLiteral("svg"),  QStringLiteral("sys"),  QStringLiteral("tar"),  QStringLiteral("tmp"),
                QStringLiteral("toml"), QStringLiteral("txt"),  QStringLiteral("ui"),   QStringLiteral("xml"),  QStringLiteral("xz"),
                QStringLiteral("yaml"), QStringLiteral("yml"),  QStringLiteral("zip"),
            };
            return !fileExtensions.contains(match.captured(4));
        }

        Span span(const QRegularExpressionMatch &match, int group) {
            if (match.capturedStart(group) < 0) return {};
            return {static_cast<int>(match.capturedStart(group)), static_cast<int>(match.capturedLength(group))};
        }

        LogLevel levelFromWord(QStringView word) {
            if (word.compare(u"DEBUG", Qt::CaseInsensitive) == 0 || word.compare(u"TRACE", Qt::CaseInsensitive) == 0) return LogLevel::Debug;
            if (word.startsWith(u"WARN", Qt::CaseInsensitive)) return LogLevel::Warn;
            if (word.endsWith(u"ERROR", Qt::CaseInsensitive) || word.compare(u"FATAL", Qt::CaseInsensitive) == 0
                || word.compare(u"PANIC", Qt::CaseInsensitive) == 0) {
                return LogLevel::Error;
            }
            return LogLevel::Info;
        }

        void readConnection(Prefix &p, const QRegularExpressionMatch &match, int bracketGroup, int idGroup) {
            if (match.capturedStart(idGroup) < 0) return;
            p.connection = span(match, bracketGroup);
            p.id = span(match, idGroup);
            bool ok = false;
            p.connectionId = match.capturedView(idGroup).toULongLong(&ok);
            if (!ok) p.connectionId = qHash(match.capturedView(idGroup));
        }

        Prefix parsePrefix(const QString &text) {
            Prefix p;
            if (text.isEmpty()) return p;
            const QChar first = text.front();
            if (first == u'[') {
                const auto match = throneParts().match(text);
                if (match.hasMatch()) {
                    p.level = levelFromWord(match.capturedView(1));
                    p.levelWord = {0, static_cast<int>(match.capturedLength(0)) - 1};
                    p.bodyStart = static_cast<int>(match.capturedLength(0));
                }
                return p;
            }
            if (first.isDigit()) {
                const auto match = xrayPrefix().match(text);
                if (match.hasMatch()) {
                    p.time = span(match, 1);
                    if (match.capturedStart(2) >= 0) {
                        p.level = levelFromWord(match.capturedView(2));
                        p.levelWord = span(match, 2);
                    }
                    readConnection(p, match, 3, 4);
                    p.bodyStart = static_cast<int>(match.capturedLength(0));
                }
                return p;
            }
            if (first.isUpper() || first == u'+' || first == u'-') {
                const auto match = singBoxPrefix().match(text);
                if (match.hasMatch()) {
                    p.time = span(match, 1);
                    p.level = levelFromWord(match.capturedView(2));
                    p.levelWord = span(match, 2);
                    p.counter = span(match, 3);
                    readConnection(p, match, 4, 5);
                    p.bodyStart = static_cast<int>(match.capturedLength(0));
                }
            }
            return p;
        }

        QTextCharFormat makeFormat(const QColor &color, int weight = QFont::Normal) {
            QTextCharFormat format;
            if (color.isValid()) format.setForeground(color);
            if (weight != QFont::Normal) format.setFontWeight(weight);
            return format;
        }
    } // namespace

    LogLevel ClassifyLogLine(const QString &line) {
        return parsePrefix(line).level;
    }

    SyntaxHighlighter::SyntaxHighlighter(const LogColors &colors, QTextDocument *parent) : QSyntaxHighlighter(parent) {
        mutedFormat = makeFormat(colors.muted);
        levelWordFormats = {makeFormat(colors.muted), makeFormat(colors.info), makeFormat(colors.warn, QFont::Bold),
                            makeFormat(colors.error, QFont::Bold)};
        levelTextFormats = {QTextCharFormat(), QTextCharFormat(), makeFormat(colors.warn), makeFormat(colors.error)};
        for (const auto &color : colors.connections) connectionFormats.append(makeFormat(color));
        warnTokenFormat = makeFormat(colors.warn);
        errorTokenFormat = makeFormat(colors.error);
        addressFormat = makeFormat(colors.address);
        searchFormat.setBackground(colors.searchBackground);
        searchFormat.setForeground(colors.searchForeground);
    }

    void SyntaxHighlighter::setSearchPattern(const QRegularExpression &pattern) {
        searchPattern = pattern;
    }

    void SyntaxHighlighter::highlightBlock(const QString &text) {
        const Prefix p = parsePrefix(text);
        const auto paint = [this](const Span &s, const QTextCharFormat &format) {
            if (s.start >= 0 && s.length > 0) setFormat(s.start, s.length, format);
        };
        paint(p.time, mutedFormat);
        paint(p.levelWord, levelWordFormats[static_cast<int>(p.level)]);
        paint(p.counter, mutedFormat);
        paint(p.connection, mutedFormat);
        if (!connectionFormats.isEmpty()) paint(p.id, connectionFormats[static_cast<qsizetype>(p.connectionId % connectionFormats.size())]);

        const int bodyStart = p.bodyStart;
        if (p.level == LogLevel::Warn || p.level == LogLevel::Error) {
            // The cause trails the last ": "; a message without one is the cause itself.
            const auto colon = text.lastIndexOf(QLatin1String(": "));
            const int from = colon >= bodyStart ? static_cast<int>(colon) + 2 : bodyStart;
            if (from < text.size()) setFormat(from, static_cast<int>(text.size()) - from, levelTextFormats[static_cast<int>(p.level)]);
        }

        auto addresses = addressTokens().globalMatch(text, bodyStart);
        while (addresses.hasNext()) {
            const auto match = addresses.next();
            if (isAddress(match)) setFormat(static_cast<int>(match.capturedStart(0)), static_cast<int>(match.capturedLength(0)), addressFormat);
        }

        auto tokens = bodyTokens().globalMatch(text, bodyStart);
        while (tokens.hasNext()) {
            const auto match = tokens.next();
            const bool nxdomain = match.capturedView(1) == u"NXDOMAIN";
            setFormat(static_cast<int>(match.capturedStart(0)), static_cast<int>(match.capturedLength(0)), nxdomain ? warnTokenFormat : errorTokenFormat);
        }

        // Last, so search hits paint over the syntax colors.
        if (!searchPattern.pattern().isEmpty()) {
            auto hits = searchPattern.globalMatch(text);
            while (hits.hasNext()) {
                const auto match = hits.next();
                if (match.capturedLength() > 0) setFormat(static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength()), searchFormat);
            }
        }

        setCurrentBlockState(0);
    }
} // namespace Qv2ray::ui
