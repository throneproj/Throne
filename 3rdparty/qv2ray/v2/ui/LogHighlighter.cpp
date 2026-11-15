#include "LogHighlighter.hpp"

namespace Qv2ray::ui {
    namespace {
        enum class TagKind : quint8 { None, Inbound, Outbound, Dns, Router, App, Other };

        struct Span {
            int start = -1;
            int length = 0;
        };

        struct Prefix {
            LogLevel level = LogLevel::Info;
            TagKind kind = TagKind::None;
            Span time, levelWord, counter, connection, tag;
            int bodyStart = 0;
        };

        // sing-box: optional "-0700 date time", LEVEL, optional "[secs]" counter, optional "[id duration]", optional "tag: ".
        const QRegularExpression &singBoxPrefix() {
            static const QRegularExpression re(QStringLiteral(
                R"re(^(?:([+-]\d{4} \d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d+)?) )?(TRACE|DEBUG|INFO|WARN|ERROR|FATAL|PANIC)(?=[\[ ]|$)(\[\d+\])? ?(?:(\[\d+ \d[\d.m]*s\]) )?(?:([a-z][\w-]*(?:/[\w-]+)?(?:\[[^\]]*\])?): )?)re"));
            return re;
        }

        // Xray and Go's standard logger: "yyyy/mm/dd hh:mm:ss[.us]", optional "[Level]", "[id]" and "component/path: ".
        const QRegularExpression &xrayPrefix() {
            static const QRegularExpression re(QStringLiteral(
                R"re(^(\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}(?:\.\d+)?) (?:\[(Debug|Info|Warning|Error)\] )?(?:(\[\d+\]) )?(?:((?:[\w-]+/)+[\w-]+): )?)re"));
            return re;
        }

        const QRegularExpression &throneParts() {
            static const QRegularExpression re(QStringLiteral(R"re(^\[([A-Za-z][A-Za-z ]{0,20})\] )re"));
            return re;
        }

        const QRegularExpression &bodyTokens() {
            static const QRegularExpression re(QStringLiteral(
                R"re((\b(?:NOERROR|NXDOMAIN|SERVFAIL|REFUSED|FORMERR|NOTIMP)\b))re"
                R"re(|(=> (?:route|route-options|resolve|sniff)(?:\([^)]*\))?|=> (?:reject|direct|block|bypass|hijack-dns)\b|\b(?:accepted|rejected)\b))re"
                R"re(|(\b(?:tcp|udp):(?=[\w\[])))re"
                R"re(|((?:\[[0-9A-Fa-f:.]+(?:%\w+)?\]|\b(?:\d{1,3}\.){3}\d{1,3}|\b(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?\.)+[A-Za-z][A-Za-z0-9-]{1,62}))re"
                R"re(:(?:6553[0-5]|655[0-2]\d|65[0-4]\d{2}|6[0-4]\d{3}|[1-5]\d{4}|[1-9]\d{0,3})(?![\w.])))re"
                R"re(|(\b(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?\.)+[A-Za-z][A-Za-z0-9-]{1,62}\b))re"));
            return re;
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

        TagKind kindOf(QStringView tag) {
            if (tag.startsWith(u"inbound")) return TagKind::Inbound;
            if (tag.contains(u"outbound") || tag.startsWith(u"proxy/")) return TagKind::Outbound;
            if (tag.startsWith(u"dns") || tag.contains(u"/dns")) return TagKind::Dns;
            if (tag.startsWith(u"router") || tag.contains(u"/router") || tag.contains(u"dispatcher")) return TagKind::Router;
            return TagKind::Other;
        }

        Prefix parsePrefix(const QString &text) {
            Prefix p;
            if (text.isEmpty()) return p;
            const QChar first = text.front();
            if (first == u'[') {
                const auto match = throneParts().match(text);
                if (match.hasMatch()) {
                    p.level = levelFromWord(match.capturedView(1));
                    p.kind = TagKind::App;
                    p.tag = {0, static_cast<int>(match.capturedLength(0)) - 1};
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
                    p.connection = span(match, 3);
                    if (match.capturedStart(4) >= 0) {
                        p.tag = span(match, 4);
                        p.kind = kindOf(match.capturedView(4));
                    }
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
                    p.connection = span(match, 4);
                    if (match.capturedStart(5) >= 0) {
                        p.tag = span(match, 5);
                        p.kind = kindOf(match.capturedView(5));
                    }
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
        levelWordFormats = {makeFormat(colors.muted), makeFormat(colors.info, QFont::DemiBold),
                            makeFormat(colors.warn, QFont::Bold), makeFormat(colors.error, QFont::Bold)};
        levelTextFormats = {QTextCharFormat(), QTextCharFormat(), makeFormat(colors.warn), makeFormat(colors.error)};
        inboundFormat = makeFormat(colors.inbound, QFont::DemiBold);
        outboundFormat = makeFormat(colors.outbound, QFont::DemiBold);
        dnsFormat = makeFormat(colors.dns, QFont::DemiBold);
        routerFormat = makeFormat(colors.router, QFont::DemiBold);
        appFormat = makeFormat(colors.text, QFont::Bold);
        otherFormat = makeFormat(colors.other, QFont::DemiBold);
        hostPortFormat = makeFormat(colors.host, QFont::DemiBold);
        hostFormat = makeFormat(colors.host);
        protocolFormat = makeFormat(colors.muted, QFont::DemiBold);
        okFormat = makeFormat(colors.ok, QFont::DemiBold);
        warnTokenFormat = makeFormat(colors.warn, QFont::DemiBold);
        errorTokenFormat = makeFormat(colors.error, QFont::DemiBold);
        infoTokenFormat = makeFormat(colors.info, QFont::DemiBold);
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
        switch (p.kind) {
            case TagKind::Inbound: paint(p.tag, inboundFormat); break;
            case TagKind::Outbound: paint(p.tag, outboundFormat); break;
            case TagKind::Dns: paint(p.tag, dnsFormat); break;
            case TagKind::Router: paint(p.tag, routerFormat); break;
            case TagKind::App: paint(p.tag, appFormat); break;
            case TagKind::Other: paint(p.tag, otherFormat); break;
            case TagKind::None: break;
        }

        const int bodyStart = p.bodyStart;
        if (p.level == LogLevel::Warn || p.level == LogLevel::Error) {
            // The cause trails the last ": "; a message without one is the cause itself.
            const auto colon = text.lastIndexOf(QLatin1String(": "));
            const int from = colon >= bodyStart ? static_cast<int>(colon) + 2 : bodyStart;
            if (from < text.size()) setFormat(from, static_cast<int>(text.size()) - from, levelTextFormats[static_cast<int>(p.level)]);
        }

        const bool bareHosts = p.kind == TagKind::Dns || text.indexOf(QLatin1String("domain"), bodyStart) >= 0;
        auto tokens = bodyTokens().globalMatch(text, bodyStart);
        while (tokens.hasNext()) {
            const auto match = tokens.next();
            const int start = static_cast<int>(match.capturedStart(0));
            const int length = static_cast<int>(match.capturedLength(0));
            if (match.capturedStart(1) >= 0) {
                const auto rcode = match.capturedView(1);
                setFormat(start, length, rcode == u"NOERROR" ? okFormat : rcode == u"NXDOMAIN" ? warnTokenFormat : errorTokenFormat);
            } else if (match.capturedStart(2) >= 0) {
                const auto action = match.capturedView(2);
                if (action.contains(u"reject") || action.contains(u"block")) setFormat(start, length, errorTokenFormat);
                else if (action.contains(u"direct") || action.contains(u"accepted") || action.contains(u"bypass")) setFormat(start, length, okFormat);
                else if (action.startsWith(u"=> route(")) setFormat(start, length, infoTokenFormat);
                else setFormat(start, length, protocolFormat);
            } else if (match.capturedStart(3) >= 0) {
                setFormat(start, length, protocolFormat);
            } else if (match.capturedStart(4) >= 0) {
                setFormat(start, length, hostPortFormat);
            } else if (bareHosts) {
                setFormat(start, length, hostFormat);
            }
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
