#include "include/global/Validation.hpp"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QStringView>
#include <QUrl>

#include <algorithm>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace Validation {
    namespace {
        enum class ValidationIpKind { Invalid, V4, V6 };

        struct ValidationIp {
            ValidationIpKind kind = ValidationIpKind::Invalid;
            bool zoned = false;
        };

        bool validationDigit(QChar c) {
            return c.unicode() >= '0' && c.unicode() <= '9';
        }

        bool validationHexDigit(QChar c) {
            const char16_t u = c.unicode();
            return (u >= '0' && u <= '9') || (u >= 'a' && u <= 'f') || (u >= 'A' && u <= 'F');
        }

        bool validationAsciiAlnum(char16_t u) {
            return (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z');
        }

        bool validationAllDigits(QStringView s) {
            return !s.isEmpty() && std::all_of(s.begin(), s.end(), validationDigit);
        }

        bool validationSignedDigits(QStringView s) {
            return validationAllDigits(s.startsWith(u'-') ? s.mid(1) : s);
        }

        QString validationEmpty(Empty empty) {
            return empty == Empty::Required ? Required(QString()) : QString();
        }

        QString validationShort(const QString &text) {
            return text.size() > 40 ? text.left(40) + QChar(0x2026) : text;
        }

        std::optional<qint64> validationParseInt(QStringView s) {
            if (s.isEmpty()) return std::nullopt;
            const bool neg = s.front() == u'-';
            qsizetype i = neg ? 1 : 0;
            if (i == s.size()) return std::nullopt;
            const quint64 limit = quint64(std::numeric_limits<qint64>::max()) + (neg ? 1 : 0);
            quint64 v = 0;
            for (; i < s.size(); ++i) {
                if (!validationDigit(s[i])) return std::nullopt;
                const quint64 d = s[i].unicode() - '0';
                if (v > (limit - d) / 10) return std::nullopt;
                v = v * 10 + d;
            }
            if (!neg) return qint64(v);
            return v == limit ? std::numeric_limits<qint64>::min() : -qint64(v);
        }

        // strconv.ParseUint(s, 10, 16): decimal digits only, leading zeros allowed.
        std::optional<quint16> validationParseUint16(QStringView s) {
            if (!validationAllDigits(s)) return std::nullopt;
            qsizetype i = 0;
            while (i < s.size() - 1 && s[i] == u'0') ++i;
            s = s.mid(i);
            if (s.size() > 5) return std::nullopt;
            const int v = s.toInt();
            if (v > 65535) return std::nullopt;
            return quint16(v);
        }

        // Go netip parseIPv4Fields.
        bool validationIpv4Fields(QStringView s) {
            int val = 0, pos = 0, digLen = 0;
            for (qsizetype i = 0; i < s.size(); ++i) {
                if (validationDigit(s[i])) {
                    if (digLen == 1 && val == 0) return false;
                    val = val * 10 + (s[i].unicode() - '0');
                    digLen++;
                    if (val > 255) return false;
                } else if (s[i] == u'.') {
                    if (i == 0 || i == s.size() - 1 || s[i - 1] == u'.') return false;
                    if (pos == 3) return false;
                    pos++;
                    val = 0;
                    digLen = 0;
                } else {
                    return false;
                }
            }
            return pos == 3;
        }

        // Go netip parseIPv6.
        bool validationIpv6(QStringView in, bool &zoned) {
            QStringView s = in;
            zoned = false;
            if (const auto pct = s.indexOf(u'%'); pct >= 0) {
                if (pct == s.size() - 1) return false;
                zoned = true;
                s = s.left(pct);
            }
            const qsizetype n = s.size();
            qsizetype p = 0;
            int ellipsis = -1;
            if (n >= 2 && s[0] == u':' && s[1] == u':') {
                ellipsis = 0;
                p = 2;
                if (p == n) return true;
            }
            int i = 0;
            while (i < 16) {
                qsizetype off = 0;
                quint32 acc = 0;
                for (; p + off < n; ++off) {
                    const QChar c = s[p + off];
                    if (!validationHexDigit(c)) break;
                    const char16_t u = c.unicode();
                    acc = (acc << 4) + (u <= '9' ? u - '0' : (u | 0x20) - 'a' + 10);
                    if (off > 3 || acc > 0xFFFF) return false;
                }
                if (off == 0) return false;
                if (p + off < n && s[p + off] == u'.') {
                    if (ellipsis < 0 && i != 12) return false;
                    if (i + 4 > 16) return false;
                    if (!validationIpv4Fields(s.mid(p))) return false;
                    p = n;
                    i += 4;
                    break;
                }
                i += 2;
                p += off;
                if (p == n) break;
                if (s[p] != u':' || p + 1 == n) return false;
                p++;
                if (s[p] == u':') {
                    if (ellipsis >= 0) return false;
                    ellipsis = i;
                    p++;
                    if (p == n) break;
                }
            }
            if (p != n) return false;
            if (i < 16) return ellipsis >= 0;
            return ellipsis < 0;
        }

        // Go netip.ParseAddr dispatches on the first '.' or ':'.
        ValidationIp validationParseIp(QStringView s) {
            for (const QChar c : s) {
                if (c == u'.') return {validationIpv4Fields(s) ? ValidationIpKind::V4 : ValidationIpKind::Invalid, false};
                if (c == u':') {
                    ValidationIp ip;
                    if (validationIpv6(s, ip.zoned)) ip.kind = ValidationIpKind::V6;
                    return ip;
                }
                if (c == u'%') break;
            }
            return {};
        }

        QString validationIpMessage(Family family) {
            switch (family) {
                case Family::V4:
                    return QCoreApplication::translate("Validation", "Not a valid IPv4 address (four numbers from 0 to 255 separated by dots).");
                case Family::V6:
                    return QCoreApplication::translate("Validation", "Not a valid IPv6 address.");
                default:
                    return QCoreApplication::translate("Validation", "Not a valid IP address.");
            }
        }

        QString validationCheckIp(const ValidationIp &ip, Family family) {
            if (ip.kind == ValidationIpKind::Invalid) return validationIpMessage(family);
            if (family == Family::V4 && ip.kind != ValidationIpKind::V4) return validationIpMessage(family);
            if (family == Family::V6 && ip.kind != ValidationIpKind::V6) return validationIpMessage(family);
            if (ip.zoned) return QCoreApplication::translate("Validation", "An IPv6 zone (the part starting with %) is not allowed here.");
            return {};
        }

        QString validationPortMessage() {
            return QCoreApplication::translate("Validation", "Must be a port number from 1 to 65535.");
        }

        QString validationHostMessage() {
            return QCoreApplication::translate("Validation", "Not a valid host name or IP address.");
        }

        QString validationSchemeMessage() {
            return QCoreApplication::translate("Validation", "Enter only the host name, without a scheme such as https://.");
        }

        // Interface names never contain ':', so a colon in the zone is a port typed after the address.
        bool validationZoneHasColon(QStringView s) {
            const qsizetype pct = s.indexOf(u'%');
            return pct >= 0 && s.mid(pct + 1).contains(u':');
        }

        // Meant as IPv6: netip reads IPv4 when a '.' comes before the first ':', so "1.2.3.4:443:user:pass" is not.
        bool validationIpv6Like(QStringView s) {
            const qsizetype pct = s.indexOf(u'%');
            const QStringView address = pct < 0 ? s : s.left(pct);
            const qsizetype colon = address.indexOf(u':'), dot = address.indexOf(u'.');
            if (colon < 0 || (dot >= 0 && dot < colon)) return false;
            return std::all_of(address.begin(), address.end(), [](QChar c) { return validationHexDigit(c) || c == u':' || c == u'.'; });
        }

        // The port text of a URL's authority, or an empty view when "://" does not directly follow the scheme.
        QStringView validationUrlPort(QStringView t) {
            const qsizetype sep = t.indexOf(u"://");
            const auto schemeChar = [](QChar c) { return validationAsciiAlnum(c.unicode()) || c == u'+' || c == u'-' || c == u'.'; };
            const bool letterFirst = sep > 0 && validationAsciiAlnum(t[0].unicode()) && !validationDigit(t[0]);
            if (!letterFirst || !std::all_of(t.begin(), t.begin() + sep, schemeChar)) return {};
            qsizetype end = sep + 3;
            while (end < t.size() && t[end] != u'/' && t[end] != u'?' && t[end] != u'#') ++end;
            QStringView host = t.mid(sep + 3, end - sep - 3);
            host = host.mid(host.lastIndexOf(u'@') + 1);
            if (host.startsWith(u'[')) {
                const qsizetype close = host.indexOf(u']');
                if (close < 0 || close + 1 >= host.size() || host[close + 1] != u':') return {};
                return host.mid(close + 2);
            }
            const qsizetype colon = host.lastIndexOf(u':');
            return colon < 0 ? QStringView() : host.mid(colon + 1);
        }

        QString validationRangeMessage(qint64 min, qint64 max) {
            return QCoreApplication::translate("Validation", "Must be from %1 to %2.").arg(QString::number(min), QString::number(max));
        }

        QString validationFormatDuration(qint64 ns) {
            if (ns == 0) return QStringLiteral("0s");
            QString out;
            quint64 v = quint64(ns);
            if (ns < 0) {
                out = QStringLiteral("-");
                v = 0 - v;
            }
            static constexpr struct {
                quint64 unit;
                const char *name;
            } units[] = {{3600000000000ULL, "h"}, {60000000000ULL, "m"}, {1000000000ULL, "s"}, {1000000ULL, "ms"}, {1000ULL, "us"}, {1ULL, "ns"}};
            for (const auto &u : units) {
                if (const quint64 q = v / u.unit) {
                    out += QString::number(q) + QLatin1String(u.name);
                    v %= u.unit;
                }
            }
            return out;
        }

        // Go's regexp/syntax escapes (parse.go parseEscape + Perl/Unicode classes): advances i past the escape, returns the offending token.
        QString validationRe2Escape(const QString &p, qsizetype &i, bool inClass) {
            const QChar d = p[i + 1];
            const char16_t c = d.unicode();
            const QString escape = QStringLiteral("\\") + d;
            const qsizetype at = i + 2;
            i = at;
            if (c < 0x80 && !validationAsciiAlnum(c)) return {};
            switch (c) {
                case 'p':
                case 'P': {
                    // Go knows only Unicode categories and scripts; PCRE adds Xan-style, L& and name=value properties.
                    QStringView name;
                    if (at < p.size() && p[at] == u'{') {
                        const qsizetype close = p.indexOf(u'}', at);
                        if (close < 0) return {};
                        name = QStringView(p).mid(at + 1, close - at - 1);
                        i = close + 1;
                    } else if (at < p.size()) {
                        name = QStringView(p).mid(at, 1);
                        i = at + 1;
                    }
                    if (name.startsWith(u'^')) name = name.mid(1);
                    // Name lookup ignores case, '_', '-' and ' ' in both engines; no Go name starts with X.
                    while (!name.isEmpty() && (name.front() == u'_' || name.front() == u'-' || name.front() == u' ')) name = name.mid(1);
                    if (name.startsWith(u'X', Qt::CaseInsensitive) || name.contains(u'&') || name.contains(u':') || name.contains(u'='))
                        return escape + p.mid(at, i - at);
                    return {};
                }
                case 'x': {
                    // Go: exactly two hex digits, or one or more in braces up to 10FFFF.
                    if (at < p.size() && p[at] == u'{') {
                        const qsizetype close = p.indexOf(u'}', at);
                        if (close < 0) {
                            i = p.size();
                            return escape + p.mid(at);
                        }
                        i = close + 1;
                        const QStringView digits = QStringView(p).mid(at + 1, close - at - 1);
                        bool ok = !digits.isEmpty() && std::all_of(digits.begin(), digits.end(), validationHexDigit);
                        if (ok) {
                            bool parsed = false;
                            ok = digits.toUInt(&parsed, 16) <= 0x10FFFF && parsed;
                        }
                        return ok ? QString() : escape + p.mid(at, i - at);
                    }
                    if (at + 1 < p.size() && validationHexDigit(p[at]) && validationHexDigit(p[at + 1])) {
                        i = at + 2;
                        return {};
                    }
                    return escape + p.mid(at, 2);
                }
                case 'd': case 'D': case 's': case 'S': case 'w': case 'W':
                case 'a': case 'f': case 'n': case 'r': case 't': case 'v': case '0':
                    return {};
                case 'A': case 'b': case 'B': case 'z':
                    if (!inClass) return {};
                    break;
                case '1': case '2': case '3': case '4': case '5': case '6': case '7':
                    if (at < p.size() && p[at].unicode() >= '0' && p[at].unicode() <= '7') return {};
                    break;
                default:
                    break;
            }
            return escape;
        }

        bool validationGoCaptureName(QStringView name) {
            return !name.isEmpty() && std::all_of(name.begin(), name.end(), [](QChar c) { return validationAsciiAlnum(c.unicode()) || c == u'_'; });
        }

        struct ValidationRe2Group {
            QString bad;
            qsizetype length = 0;
            bool opens = true;
        };

        // After "(?": Go accepts only (?P<name>, (?<name> and the flags i, m, s, U with at most one '-' followed by a flag, ending in ':' or ')'.
        ValidationRe2Group validationRe2Group(QStringView rest) {
            const auto at = [&](qsizetype k) { return k < rest.size() ? rest[k] : QChar(); };
            const auto token = [&](qsizetype length) { return ValidationRe2Group{QStringLiteral("(?") + rest.left(length).toString()}; };
            const QChar c0 = at(0), c1 = at(1);
            if ((c0 == u'P' && c1 == u'<') || (c0 == u'<' && c1 != u'=' && c1 != u'!')) {
                const qsizetype nameStart = c0 == u'P' ? 2 : 1;
                const qsizetype close = rest.indexOf(u'>');
                if (close >= 0 && !validationGoCaptureName(rest.mid(nameStart, close - nameStart))) return token(close + 1);
                return {{}, close + 1, true};
            }
            if (c0 == u'<' || (c0 == u'P' && (c1 == u'=' || c1 == u'>')) || (c0 == u'-' && validationDigit(c1))) return token(2);
            if (c0 == u'=' || c0 == u'!' || c0 == u'>' || c0 == u'(' || c0 == u'#' || c0 == u'|' || c0 == u'\'' || c0 == u'R' || c0 == u'&' ||
                c0 == u'+' || validationDigit(c0))
                return token(1);
            bool negated = false, sawFlag = false;
            for (qsizetype k = 0; k < rest.size(); ++k) {
                const QChar f = rest[k];
                if (f == u':' || f == u')') {
                    if (negated && !sawFlag) return token(k + 1);
                    return {{}, k + 1, f == u':'};
                }
                if (f == u'-') {
                    if (negated) return token(k + 1);
                    negated = true;
                    sawFlag = false;
                } else if (f == u'i' || f == u'm' || f == u's' || f == u'U') {
                    sawFlag = true;
                } else {
                    return token(k + 1);
                }
            }
            return {};
        }

        // Exclusive end of a {n}, {n,} or {n,m} repeat at i, or -1 when the brace is a literal; max is -1 when unbounded.
        qsizetype validationRepeatEnd(const QString &p, qsizetype i, int &min, int &max) {
            qsizetype j = i + 1;
            // Go parseInt: a missing number or a leading zero ({,3}, {01}) makes the brace a literal.
            const auto number = [&](int &out) {
                const qsizetype start = j;
                while (j < p.size() && validationDigit(p[j])) ++j;
                if (j == start || (j - start > 1 && p[start] == u'0')) return false;
                out = j - start > 7 ? 100000000 : QStringView(p).mid(start, j - start).toInt();
                return true;
            };
            if (!number(min)) return -1;
            max = min;
            if (j < p.size() && p[j] == u',') {
                ++j;
                if (j < p.size() && p[j] == u'}') max = -1;
                else if (!number(max)) return -1;
            }
            if (j >= p.size() || p[j] != u'}') return -1;
            return j + 1;
        }

        // unicode.CategoryAliases and Assigned, which only Go knows; compared like Go's canonicalName.
        bool validationGoOnlyProperty(QStringView name) {
            static constexpr const char *names[] = {
                "assigned", "casedletter", "closepunctuation", "cntrl", "combiningmark", "connectorpunctuation", "control",
                "currencysymbol", "dashpunctuation", "decimalnumber", "digit", "enclosingmark", "finalpunctuation", "format",
                "initialpunctuation", "letter", "letternumber", "lineseparator", "lowercaseletter", "mark", "mathsymbol",
                "modifierletter", "modifiersymbol", "nonspacingmark", "number", "openpunctuation", "other", "otherletter",
                "othernumber", "otherpunctuation", "othersymbol", "paragraphseparator", "privateuse", "punct", "punctuation",
                "separator", "spaceseparator", "spacingmark", "surrogate", "symbol", "titlecaseletter", "unassigned",
                "uppercaseletter"};
            QByteArray canonical;
            for (const QChar c : name) {
                const char16_t u = c.unicode();
                if (u == '_' || u == '-' || u == ' ') continue;
                if (!validationAsciiAlnum(u)) return false;
                canonical += char(u >= 'A' && u <= 'Z' ? u + ('a' - 'A') : u);
            }
            return std::any_of(std::begin(names), std::end(names), [&canonical](const char *n) { return canonical == n; });
        }

        // A copy PCRE judges like Go where Go is laxer: class '-', repeated assertions and flag groups, group names, category aliases, literal '{'.
        QString validationPcreCopy(const QString &p) {
            const qsizetype n = p.size();
            const auto flagGroupsEnd = [&p, n](qsizetype i) {
                while (i + 1 < n && p[i] == u'(' && p[i + 1] == u'?') {
                    const auto group = validationRe2Group(QStringView(p).mid(i + 2));
                    if (!group.bad.isEmpty() || group.opens) break;
                    i += 2 + group.length;
                }
                return i;
            };
            const auto quantifierEnd = [&p, n](qsizetype i) -> qsizetype {
                if (i >= n) return -1;
                qsizetype end = -1;
                if (p[i] == u'*' || p[i] == u'+' || p[i] == u'?') {
                    end = i + 1;
                } else if (p[i] == u'{') {
                    int min = 0, max = 0;
                    end = validationRepeatEnd(p, i, min, max);
                }
                if (end > 0 && end < n && (p[end] == u'?' || p[end] == u'+')) ++end;
                return end;
            };
            QString out;
            out.reserve(n + 16);
            QSet<QString> names;
            int renamed = 0;
            bool inClass = false, afterClass = false;
            qsizetype classStart = 0;
            qsizetype i = 0;
            const auto property = [&] {
                qsizetype end = std::min(n, i + 3);
                QStringView name = QStringView(p).mid(i + 2, end - i - 2);
                if (i + 2 < n && p[i + 2] == u'{') {
                    const qsizetype close = p.indexOf(u'}', i + 2);
                    if (close < 0) {
                        out += p.mid(i);
                        i = n;
                        return;
                    }
                    end = close + 1;
                    name = QStringView(p).mid(i + 3, close - i - 3);
                }
                if (name.startsWith(u'^')) name = name.mid(1);
                out += validationGoOnlyProperty(name) ? QStringLiteral("\\pL") : p.mid(i, end - i);
                i = end;
            };
            const auto assertion = [&](qsizetype length) {
                const QString token = p.mid(i, length);
                i += length;
                out += quantifierEnd(flagGroupsEnd(i)) > 0 ? QStringLiteral("(?:") + token + QLatin1Char(')') : token;
            };
            while (i < n) {
                const QChar c = p[i];
                if (inClass) {
                    if (c == u'\\' && i + 1 < n) {
                        const QChar d = p[i + 1];
                        if (d == u'p' || d == u'P') {
                            property();
                            afterClass = true;
                            continue;
                        }
                        out += p.mid(i, 2);
                        i += 2;
                        afterClass = QStringView(u"dDsSwW").contains(d);
                        continue;
                    }
                    if (c == u'[' && i + 1 < n && p[i + 1] == u':') {
                        if (const auto end = p.indexOf(QStringLiteral(":]"), i + 2); end >= 0) {
                            out += p.mid(i, end + 2 - i);
                            i = end + 2;
                            afterClass = true;
                            continue;
                        }
                    }
                    if (c == u'-' && afterClass) {
                        out += QStringLiteral("\\-");
                    } else if (c == u'[' && i + 1 < n && (p[i + 1] == u'.' || p[i + 1] == u'=')) {
                        out += QStringLiteral("\\[");
                    } else {
                        if (c == u']' && i > classStart) inClass = false;
                        out += c;
                    }
                    afterClass = false;
                    ++i;
                    continue;
                }
                if (c == u'\\' && i + 1 < n) {
                    const QChar d = p[i + 1];
                    if (d == u'Q') {
                        const auto end = p.indexOf(QStringLiteral("\\E"), i + 2);
                        const qsizetype stop = end < 0 ? n : end + 2;
                        out += p.mid(i, stop - i);
                        i = stop;
                    } else if (d == u'p' || d == u'P') {
                        property();
                    } else if (d == u'b' || d == u'B' || d == u'A' || d == u'z') {
                        assertion(2);
                    } else if (QStringView(u"xogkN").contains(d) && i + 2 < n && p[i + 2] == u'{') {
                        // The braces belong to the escape, so the literal-brace rewrite below must not reach them.
                        const qsizetype close = p.indexOf(u'}', i + 3);
                        const qsizetype stop = close < 0 ? n : close + 1;
                        out += p.mid(i, stop - i);
                        i = stop;
                    } else {
                        out += p.mid(i, 2);
                        i += 2;
                    }
                    continue;
                }
                if (c == u'^' || c == u'$') {
                    assertion(1);
                    continue;
                }
                if (c == u'[') {
                    out += c;
                    ++i;
                    if (i < n && p[i] == u'^') out += p[i++];
                    if (i < n && (p[i] == u':' || p[i] == u'.' || p[i] == u'=')) out += u'\\';
                    inClass = true;
                    afterClass = false;
                    classStart = i;
                    continue;
                }
                if (c == u'(' && i + 1 < n && p[i + 1] == u'?') {
                    // Go's flag groups add no atom, so a repeat after them applies to what came before.
                    if (const qsizetype flags = flagGroupsEnd(i); flags > i) {
                        const qsizetype repeat = quantifierEnd(flags);
                        if (repeat > 0) {
                            out += p.mid(flags, repeat - flags) + p.mid(i, flags - i);
                            i = repeat;
                        } else {
                            out += p.mid(i, flags - i);
                            i = flags;
                        }
                        continue;
                    }
                    const QStringView rest = QStringView(p).mid(i + 2);
                    const bool angle = rest.startsWith(u'<') && !rest.startsWith(u"<=") && !rest.startsWith(u"<!");
                    const qsizetype nameStart = rest.startsWith(u"P<") ? 2 : angle ? 1 : 0;
                    if (const qsizetype close = nameStart > 0 ? rest.indexOf(u'>') : -1; close > nameStart) {
                        const QString name = rest.mid(nameStart, close - nameStart).toString();
                        if (validationGoCaptureName(name) && (validationDigit(name[0]) || name.size() > 32 || names.contains(name))) {
                            QString fresh;
                            do {
                                fresh = QStringLiteral("_") + QString::number(++renamed);
                            } while (names.contains(fresh));
                            names.insert(fresh);
                            out += p.mid(i, 2 + nameStart) + fresh + QLatin1Char('>');
                        } else {
                            names.insert(name);
                            out += p.mid(i, 2 + close + 1);
                        }
                        i += 2 + close + 1;
                        continue;
                    }
                    out += p.mid(i, 2);
                    i += 2;
                    continue;
                }
                if (c == u'{') {
                    int min = 0, max = 0;
                    if (validationRepeatEnd(p, i, min, max) < 0) {
                        out += QStringLiteral("\\{");
                        ++i;
                        continue;
                    }
                }
                out += c;
                ++i;
            }
            return out;
        }

        QString validationRe2Unsupported(const QString &p) {
            const auto unsupported = [](const QString &token) {
                return QCoreApplication::translate("Validation", "The core's regex engine (RE2) does not support \"%1\".").arg(token);
            };
            const auto tooManyRepeats = [] {
                return QCoreApplication::translate("Validation", "The core's regex engine (RE2) allows at most 1000 repetitions, counting nested repeats multiplied together.");
            };
            const qsizetype n = p.size();
            // Go repeatIsValid: an atom's weight is the largest product of the {n,m} counts nested inside it.
            std::vector<int> groups{1};
            int last = 0;
            const auto commit = [&] {
                groups.back() = std::max(groups.back(), last);
                last = 0;
            };
            bool inClass = false;
            qsizetype classStart = 0;
            qsizetype i = 0;
            while (i < n) {
                const QChar c = p[i];
                if (c == u'\\') {
                    if (i + 1 >= n) return {};
                    if (!inClass) {
                        commit();
                        last = 1;
                    }
                    if (!inClass && p[i + 1] == u'Q') {
                        const auto end = p.indexOf(QStringLiteral("\\E"), i + 2);
                        if (end < 0) return {};
                        i = end + 2;
                        continue;
                    }
                    if (auto bad = validationRe2Escape(p, i, inClass); !bad.isEmpty()) return unsupported(bad);
                    continue;
                }
                if (inClass) {
                    if (c == u'[' && i + 1 < n && p[i + 1] == u':') {
                        if (const auto end = p.indexOf(QStringLiteral(":]"), i + 2); end >= 0) {
                            i = end + 2;
                            continue;
                        }
                    }
                    if (c == u']' && i > classStart) inClass = false;
                    ++i;
                    continue;
                }
                switch (c.unicode()) {
                    case '[':
                        commit();
                        last = 1;
                        inClass = true;
                        ++i;
                        if (i < n && p[i] == u'^') ++i;
                        classStart = i;
                        continue;
                    case '(':
                        if (i + 1 < n && p[i + 1] == u'*') {
                            const qsizetype close = p.indexOf(u')', i);
                            return unsupported(close < 0 ? p.mid(i, 2) : p.mid(i, close - i + 1));
                        }
                        commit();
                        if (i + 1 < n && p[i + 1] == u'?') {
                            const auto group = validationRe2Group(QStringView(p).mid(i + 2));
                            if (!group.bad.isEmpty()) return unsupported(group.bad);
                            if (group.opens) groups.push_back(1);
                            i += 2 + group.length;
                            continue;
                        }
                        groups.push_back(1);
                        ++i;
                        continue;
                    case ')':
                        commit();
                        if (groups.size() > 1) {
                            last = groups.back();
                            groups.pop_back();
                        }
                        ++i;
                        continue;
                    case '|':
                        commit();
                        ++i;
                        continue;
                    case '*':
                    case '+':
                    case '?':
                        if (i + 1 < n && p[i + 1] == u'+') return unsupported(p.mid(i, 2));
                        ++i;
                        if (i < n && p[i] == u'?') ++i;
                        continue;
                    case '{': {
                        int min = 0, max = 0;
                        const auto end = validationRepeatEnd(p, i, min, max);
                        if (end < 0) break;
                        if (min > 1000 || max > 1000) return tooManyRepeats();
                        if (end < n && p[end] == u'+') return unsupported(p.mid(i, end - i + 1));
                        if (max == 0) {
                            last = 1;
                        } else if (const int m = max < 0 ? min : max; m > 1) {
                            last *= m;
                            if (last > 1000) return tooManyRepeats();
                        }
                        i = end;
                        if (i < n && p[i] == u'?') ++i;
                        continue;
                    }
                    default:
                        break;
                }
                commit();
                last = 1;
                ++i;
            }
            return {};
        }

        quint64 validationMemoryUnit(const QString &unit) {
            static constexpr struct {
                const char *name;
                quint64 value;
            } table[] = {{"b", 1ULL}, {"k", 1ULL << 10}, {"kb", 1ULL << 10}, {"m", 1ULL << 20}, {"mb", 1ULL << 20},
                         {"g", 1ULL << 30}, {"gb", 1ULL << 30}, {"t", 1ULL << 40}, {"tb", 1ULL << 40}, {"p", 1ULL << 50},
                         {"pb", 1ULL << 50}, {"e", 1ULL << 60}, {"eb", 1ULL << 60}};
            for (const auto &u : table)
                if (unit == QLatin1String(u.name)) return u.value;
            return 0;
        }

        quint64 validationDurationUnit(std::string_view u) {
            static constexpr struct {
                const char *name;
                quint64 value;
            } table[] = {{"ns", 1ULL}, {"us", 1000ULL}, {"\xC2\xB5s", 1000ULL}, {"\xCE\xBCs", 1000ULL}, {"ms", 1000000ULL},
                         {"s", 1000000000ULL}, {"m", 60000000000ULL}, {"h", 3600000000000ULL}, {"d", 86400000000000ULL}};
            for (const auto &e : table)
                if (u == e.name) return e.value;
            return 0;
        }

        bool validationBase64Char(QChar c, bool url) {
            const char16_t u = c.unicode();
            if (validationAsciiAlnum(u)) return true;
            return url ? (u == '-' || u == '_') : (u == '+' || u == '/');
        }

        std::optional<QByteArray> validationDecodeBase64(const QString &text, bool url) {
            QByteArray data = text.toLatin1();
            while (data.size() % 4) data.append('=');
            const auto result = QByteArray::fromBase64Encoding(
                data, (url ? QByteArray::Base64UrlEncoding : QByteArray::Base64Encoding) | QByteArray::AbortOnBase64DecodingErrors);
            if (!result) return std::nullopt;
            return result.decoded;
        }

        // gofrs/uuid v5 codec.go parseBytes.
        bool validationGofrsUuid(QByteArray b) {
            switch (b.size()) {
                case 32:
                case 36:
                    break;
                case 34:
                case 38:
                    if (b.front() != '{' || b.back() != '}') return false;
                    b = b.mid(1, b.size() - 2);
                    break;
                case 41:
                case 45:
                    if (!b.startsWith("urn:uuid:")) return false;
                    b = b.mid(9);
                    break;
                default:
                    return false;
            }
            for (qsizetype i = 0; i < b.size(); ++i) {
                const bool dash = b.size() == 36 && (i == 8 || i == 13 || i == 18 || i == 23);
                if (dash ? b[i] != '-' : !validationHexDigit(QChar(uchar(b[i])))) return false;
            }
            return true;
        }

        // Xray-core common/uuid/uuid.go ParseString: trailing bytes after the five groups are ignored.
        bool validationXrayUuid(const QByteArray &text) {
            const qsizetype l = text.size();
            if (l < 32 || l > 36) return l != 0 && l <= 30;
            qsizetype p = 0;
            for (const int group : {8, 4, 4, 4, 12}) {
                if (p < l && text[p] == '-') ++p;
                if (l - p < group) return false;
                for (int k = 0; k < group; ++k)
                    if (!validationHexDigit(QChar(uchar(text[p + k])))) return false;
                p += group;
            }
            return true;
        }

        bool validationHttpTokenChar(QChar c) {
            const char16_t u = c.unicode();
            return validationAsciiAlnum(u) || (u != 0 && u < 0x80 && std::strchr("!#$%&'*+-.^_`|~", char(u)));
        }
    }

    bool HasError(const Issues &issues) {
        return std::any_of(issues.begin(), issues.end(), [](const Issue &issue) { return issue.severity == Severity::Error; });
    }

    std::optional<qint64> ToInt(const QString &text) {
        return validationParseInt(QStringView(text).trimmed());
    }

    QString Required(const QString &text) {
        if (!text.trimmed().isEmpty()) return {};
        return QCoreApplication::translate("Validation", "This field is required.");
    }

    QString Int(const QString &text, qint64 min, qint64 max, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        const auto v = validationParseInt(t);
        if (!v) {
            if (validationSignedDigits(t)) return validationRangeMessage(min, max);
            return QCoreApplication::translate("Validation", "Must be a whole number.");
        }
        if (*v < min || *v > max) return validationRangeMessage(min, max);
        return {};
    }

    QString Port(const QString &text, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        const auto v = validationParseInt(t);
        if (!v || *v < 1 || *v > 65535) return validationPortMessage();
        return {};
    }

    QString PortRange(const QString &text, const PortRangeStyle &style) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return {};
        const bool colon = style.sep == RangeSep::Colon;
        const auto format = [colon] {
            return colon ? QCoreApplication::translate("Validation", "Use a range like 20000:30000.")
                         : QCoreApplication::translate("Validation", "Use a range like 20000-30000.");
        };
        const auto tooLarge = [] { return QCoreApplication::translate("Validation", "Ports must be at most 65535."); };
        const qsizetype at = t.indexOf(colon ? u':' : u'-');
        if (at < 0) {
            if (validationParseUint16(t)) {
                if (style.allowSingle) return {};
                return colon ? QCoreApplication::translate("Validation", "\"%1\" is a single port; write it as %1:%1.").arg(t)
                             : QCoreApplication::translate("Validation", "\"%1\" is a single port; write it as %1-%1.").arg(t);
            }
            return validationAllDigits(t) ? tooLarge() : format();
        }
        // sing-box route/rule/rule_item_port_range.go and sing-quic hysteria ParsePorts: an empty side is 0 or 65535.
        const QStringView left = QStringView(t).left(at), right = QStringView(t).mid(at + 1);
        quint32 start = 0, end = 65535;
        for (const auto &[side, value] : {std::pair{left, &start}, std::pair{right, &end}}) {
            if (side.isEmpty()) {
                if (!style.allowOpenEnds) return format();
                continue;
            }
            const auto v = validationParseUint16(side);
            if (!v) return validationAllDigits(side) ? tooLarge() : format();
            *value = *v;
        }
        if (style.checkOrder && start > end) return QCoreApplication::translate("Validation", "The range starts after it ends, so it matches nothing.");
        return {};
    }

    QString PortRangeOrder(const QString &text, const PortRangeStyle &style) {
        PortRangeStyle syntax = style;
        syntax.checkOrder = false;
        if (!PortRange(text, syntax).isEmpty()) return {};
        syntax.checkOrder = true;
        return PortRange(text, syntax);
    }

    QString IntOrRange(const QString &text, qint64 min, qint64 max, Empty empty, bool allowReversed) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        const bool negative = min < 0;
        const qsizetype dash = t.indexOf(u'-', negative && t.startsWith(u'-') ? 1 : 0);
        const QStringView view(t);
        const QStringView a = dash < 0 ? view : view.left(dash);
        const QStringView b = dash < 0 ? view : view.mid(dash + 1);
        const auto format = [] { return QCoreApplication::translate("Validation", "Must be a whole number or a range like 10-20."); };
        qint64 values[2] = {0, 0};
        int k = 0;
        for (const QStringView part : {a, b}) {
            if (!validationSignedDigits(part) || (!negative && part.startsWith(u'-'))) return format();
            const auto v = validationParseInt(part);
            if (!v || *v < min || *v > max) return validationRangeMessage(min, max);
            values[k++] = *v;
        }
        if (!allowReversed && values[0] > values[1]) return QCoreApplication::translate("Validation", "The range starts after it ends; put the smaller number first.");
        return {};
    }

    // sing common/json/badoption/internal/my_time/format.go, on UTF-8 bytes like the Go original.
    std::optional<qint64> ParseDurationNs(const QString &text) {
        const QByteArray bytes = text.trimmed().toUtf8();
        const char *s = bytes.constData();
        const qsizetype n = bytes.size();
        constexpr quint64 kLimit = 1ULL << 63;
        const auto digit = [](char c) { return c >= '0' && c <= '9'; };
        qsizetype p = 0;
        bool neg = false;
        if (p < n && (s[p] == '-' || s[p] == '+')) {
            neg = s[p] == '-';
            ++p;
        }
        if (n - p == 1 && s[p] == '0') return 0;
        if (p == n) return std::nullopt;
        quint64 d = 0;
        while (p < n) {
            quint64 v = 0, f = 0;
            double scale = 1;
            if (!(s[p] == '.' || digit(s[p]))) return std::nullopt;
            const qsizetype intStart = p;
            for (; p < n && digit(s[p]); ++p) {
                if (v > kLimit / 10) return std::nullopt;
                v = v * 10 + quint64(s[p] - '0');
                if (v > kLimit) return std::nullopt;
            }
            const bool pre = p != intStart;
            bool post = false;
            if (p < n && s[p] == '.') {
                ++p;
                const qsizetype fracStart = p;
                bool overflow = false;
                for (; p < n && digit(s[p]); ++p) {
                    if (overflow) continue;
                    if (f > (kLimit - 1) / 10) {
                        overflow = true;
                        continue;
                    }
                    const quint64 y = f * 10 + quint64(s[p] - '0');
                    if (y > kLimit) {
                        overflow = true;
                        continue;
                    }
                    f = y;
                    scale *= 10;
                }
                post = p != fracStart;
            }
            if (!pre && !post) return std::nullopt;
            const qsizetype unitStart = p;
            while (p < n && s[p] != '.' && !digit(s[p])) ++p;
            if (p == unitStart) return std::nullopt;
            const quint64 unit = validationDurationUnit(std::string_view(s + unitStart, size_t(p - unitStart)));
            if (unit == 0) return std::nullopt;
            if (v > kLimit / unit) return std::nullopt;
            v *= unit;
            if (f > 0) {
                v += quint64(double(f) * (double(unit) / scale));
                if (v > kLimit) return std::nullopt;
            }
            d += v;
            if (d > kLimit) return std::nullopt;
        }
        if (neg) return d == kLimit ? std::numeric_limits<qint64>::min() : -qint64(d);
        if (d > kLimit - 1) return std::nullopt;
        return qint64(d);
    }

    QString Duration(const QString &text, const DurationRule &rule) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(rule.empty);
        const auto ns = ParseDurationNs(t);
        if (!ns) {
            const qsizetype dot = t.indexOf(u'.');
            const bool bare = dot < 0 ? validationAllDigits(t)
                                      : validationAllDigits(QStringView(t).left(dot)) && validationAllDigits(QStringView(t).mid(dot + 1));
            if (bare) return QCoreApplication::translate("Validation", "Add a unit: write %1ms or %1s, not %1.").arg(t);
            return QCoreApplication::translate("Validation", "Not a duration. Use a number followed by a unit (ns, us, ms, s, m, h or d), for example 500ms or 1m30s.");
        }
        if (!rule.allowNegative && *ns < 0) return QCoreApplication::translate("Validation", "Must not be negative.");
        if (rule.wholeSeconds && *ns % 1000000000 != 0) return QCoreApplication::translate("Validation", "Use whole seconds, for example 30s.");
        if (rule.minNs && *ns < *rule.minNs) return QCoreApplication::translate("Validation", "Must be at least %1.").arg(validationFormatDuration(*rule.minNs));
        if (rule.maxNs && *ns > *rule.maxNs) return QCoreApplication::translate("Validation", "Must be at most %1.").arg(validationFormatDuration(*rule.maxNs));
        return {};
    }

    // sing common/byteformats/json.go parseUnit: digits, then a unit that is trimmed and lower-cased.
    QString MemoryBytes(const QString &text, bool allowBareInteger, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        const auto wholeNumber = [] { return QCoreApplication::translate("Validation", "Use a whole number before the unit."); };
        const auto tooLarge = [] { return QCoreApplication::translate("Validation", "The number is too large."); };
        qsizetype i = 0;
        while (i < t.size() && validationDigit(t[i])) ++i;
        if (i == t.size()) {
            if (!allowBareInteger) return QCoreApplication::translate("Validation", "Add a unit, for example 8m or 512kb.");
            return validationParseInt(t) ? QString() : tooLarge();
        }
        const bool digitNext = i + 1 < t.size() && validationDigit(t[i + 1]);
        if (i == 0) {
            if (t[0] == u'-' && digitNext) return QCoreApplication::translate("Validation", "Must not be negative.");
            if (t[0] == u'.' && digitNext) return wholeNumber();
            return QCoreApplication::translate("Validation", "Not a size. Use a whole number followed by a unit, for example 8m or 512kb.");
        }
        if ((t[i] == u'.' || t[i] == u',') && digitNext) return wholeNumber();
        bool ok = false;
        const quint64 value = t.left(i).toULongLong(&ok, 10);
        if (!ok) return tooLarge();
        const QString unit = t.mid(i).trimmed();
        const quint64 unitValue = validationMemoryUnit(unit.toLower());
        if (unitValue == 0)
            return QCoreApplication::translate("Validation", "Unknown unit \"%1\". Use b, k, kb, m, mb, g or gb.").arg(validationShort(unit));
        if (value > std::numeric_limits<quint64>::max() / unitValue) return tooLarge();
        return {};
    }

    QString Ip(const QString &text, Family family, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        return validationCheckIp(validationParseIp(t), family);
    }

    // Go netip.ParsePrefix: split at the last '/', no zone, decimal bits without a sign or leading zeros.
    QString Cidr(const QString &text, Family family, bool allowBareIp, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        const auto needPrefix = [] {
            return QCoreApplication::translate("Validation", "Use an address with a prefix length, for example 10.0.0.0/8 or fd00::/8.");
        };
        const qsizetype slash = t.lastIndexOf(u'/');
        if (slash < 0) {
            if (allowBareIp) return validationCheckIp(validationParseIp(t), family);
            return needPrefix();
        }
        const auto ip = validationParseIp(QStringView(t).left(slash));
        if (auto message = validationCheckIp(ip, family); !message.isEmpty()) return message;
        const QStringView bits = QStringView(t).mid(slash + 1);
        if (bits.contains(u'.')) return QCoreApplication::translate("Validation", "Use a prefix length instead of a netmask, for example 10.0.0.0/8.");
        if (!validationAllDigits(bits)) return needPrefix();
        if (bits.size() > 1 && bits[0] == u'0') return QCoreApplication::translate("Validation", "Write the prefix length without leading zeros.");
        const int maxBits = ip.kind == ValidationIpKind::V4 ? 32 : 128;
        if (bits.size() > 3 || bits.toInt() > maxBits)
            return QCoreApplication::translate("Validation", "The prefix length must be from 0 to %1.").arg(maxBits);
        return {};
    }

    QString Host(const QString &text, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        if (t.contains(QStringLiteral("://"))) return validationSchemeMessage();
        if (std::any_of(t.begin(), t.end(), [](QChar c) { return c.isSpace(); }))
            return QCoreApplication::translate("Validation", "Must not contain spaces.");
        if (std::any_of(t.begin(), t.end(), [](QChar c) { return c == u'/' || c == u'?' || c == u'#' || c == u'@'; }))
            return QCoreApplication::translate("Validation", "Enter only the host name, without a path or user name.");
        const auto portMessage = [] { return QCoreApplication::translate("Validation", "Enter the port in its own field."); };
        if (t.startsWith(u'[')) {
            const qsizetype close = t.indexOf(u']');
            if (close < 0 || validationParseIp(QStringView(t).mid(1, close - 1)).kind != ValidationIpKind::V6) return validationHostMessage();
            const QStringView rest = QStringView(t).mid(close + 1);
            if (rest.isEmpty()) return {};
            if (rest.startsWith(u':') && validationAllDigits(rest.mid(1))) return portMessage();
            return validationHostMessage();
        }
        if (validationParseIp(t).kind != ValidationIpKind::Invalid) {
            if (!validationZoneHasColon(t)) return {};
            return validationAllDigits(QStringView(t).mid(t.lastIndexOf(u':') + 1)) ? portMessage() : validationIpMessage(Family::V6);
        }
        if (const qsizetype colon = t.lastIndexOf(u':'); colon >= 0) {
            if (t.indexOf(u':') != colon) return validationIpv6Like(t) ? validationIpMessage(Family::V6) : validationHostMessage();
            if (validationAllDigits(QStringView(t).mid(colon + 1))) return portMessage();
            return validationHostMessage();
        }
        // Digits and dots only: meant as an IPv4 address, which Go would otherwise resolve as a name and fail.
        if (std::all_of(t.begin(), t.end(), [](QChar c) { return validationDigit(c) || c == u'.'; }))
            return validationIpMessage(Family::V4);
        QByteArray name = QUrl::toAce(t);
        if (name.endsWith('.')) name.chop(1);
        if (name.isEmpty() || name.size() > 253) return validationHostMessage();
        for (const auto &label : name.split('.')) {
            if (label.isEmpty() || label.size() > 63 || label.startsWith('-') || label.endsWith('-')) return validationHostMessage();
            for (const char c : label)
                if (!validationAsciiAlnum(uchar(c)) && c != '-' && c != '_') return validationHostMessage();
        }
        return {};
    }

    QString HostPort(const QString &text, bool portRequired, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        if (t.contains(QStringLiteral("://"))) return validationSchemeMessage();
        const auto bracketMessage = [] {
            return QCoreApplication::translate("Validation", "Put an IPv6 address in brackets before the port, for example [2001:db8::1]:443.");
        };
        QString port;
        bool hasPort = false;
        if (t.startsWith(u'[')) {
            const qsizetype close = t.indexOf(u']');
            if (close < 0 || validationParseIp(QStringView(t).mid(1, close - 1)).kind != ValidationIpKind::V6)
                return validationIpMessage(Family::V6);
            const QString rest = t.mid(close + 1);
            if (!rest.isEmpty()) {
                if (!rest.startsWith(u':')) return validationHostMessage();
                port = rest.mid(1);
                hasPort = true;
            }
        } else {
            if (validationParseIp(t).kind == ValidationIpKind::V6) return portRequired || validationZoneHasColon(t) ? bracketMessage() : QString();
            QString host = t;
            if (const qsizetype colon = t.lastIndexOf(u':'); colon >= 0) {
                host = t.left(colon);
                port = t.mid(colon + 1);
                hasPort = true;
                if (host.contains(u':')) return validationIpv6Like(host) ? bracketMessage() : validationHostMessage();
            }
            if (host.isEmpty()) return validationHostMessage();
            if (auto message = Host(host, Empty::Required); !message.isEmpty()) return message;
        }
        if (!hasPort) return portRequired ? QCoreApplication::translate("Validation", "Add the port after a colon, for example example.com:443.") : QString();
        const auto v = validationParseInt(port);
        if (!v || *v < 1 || *v > 65535) return validationPortMessage();
        return {};
    }

    QString Base64Key(const QString &text, int bytes, bool urlAlphabet, bool padded, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        const int remainder = bytes % 3;
        const int dataChars = bytes / 3 * 4 + (remainder ? remainder + 1 : 0);
        const int total = dataChars + (padded && remainder ? 3 - remainder : 0);
        bool ok = t.size() == total;
        for (int i = 0; ok && i < total; ++i)
            ok = i < dataChars ? validationBase64Char(t[i], urlAlphabet) : t[i] == u'=';
        if (ok) {
            const auto decoded = validationDecodeBase64(t, urlAlphabet);
            ok = decoded && decoded->size() == bytes;
        }
        if (ok) return {};
        const QString count = QString::number(bytes), length = QString::number(total);
        if (urlAlphabet && !padded)
            return QCoreApplication::translate("Validation", "Must be a %1-byte key in URL-safe base64 without padding (%2 characters).").arg(count, length);
        if (urlAlphabet)
            return QCoreApplication::translate("Validation", "Must be a %1-byte key in URL-safe base64 (%2 characters).").arg(count, length);
        if (!padded)
            return QCoreApplication::translate("Validation", "Must be a %1-byte key in base64 without padding (%2 characters).").arg(count, length);
        return QCoreApplication::translate("Validation", "Must be a %1-byte key in base64 (%2 characters).").arg(count, length);
    }

    QString Base64(const QString &text, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        qsizetype equals = 0;
        while (equals < t.size() && t[t.size() - 1 - equals] == u'=') ++equals;
        const qsizetype data = t.size() - equals;
        bool ok = data > 0 && equals <= 2 && (equals ? t.size() % 4 == 0 : t.size() % 4 != 1);
        for (qsizetype i = 0; ok && i < data; ++i) ok = validationBase64Char(t[i], false);
        if (ok) {
            const auto decoded = validationDecodeBase64(t, false);
            ok = decoded && !decoded->isEmpty();
        }
        return ok ? QString() : QCoreApplication::translate("Validation", "Not valid base64.");
    }

    QString Hex(const QString &text, int minChars, int maxChars, bool evenLength, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        if (!std::all_of(t.begin(), t.end(), validationHexDigit)) return QCoreApplication::translate("Validation", "Must be hexadecimal (0-9, a-f).");
        if (evenLength && t.size() % 2) return QCoreApplication::translate("Validation", "Must have an even number of characters.");
        if (maxChars > 0 && t.size() > maxChars) return QCoreApplication::translate("Validation", "Must be at most %1 characters long.").arg(maxChars);
        if (minChars > 0 && t.size() < minChars) return QCoreApplication::translate("Validation", "Must be at least %1 characters long.").arg(minChars);
        return {};
    }

    QString Uuid(const QString &text, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        if (validationGofrsUuid(t.toUtf8())) return {};
        return QCoreApplication::translate("Validation", "Must be a UUID, for example 123e4567-e89b-12d3-a456-426614174000.");
    }

    QString XrayId(const QString &text, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        if (validationXrayUuid(t.toUtf8())) return {};
        return QCoreApplication::translate("Validation", "Must be a UUID, or a custom ID of at most 30 bytes.");
    }

    QString Url(const QString &text, const QStringList &schemes, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        if (std::any_of(t.begin(), t.end(), [](QChar c) { return c.isSpace(); }))
            return QCoreApplication::translate("Validation", "Must not contain spaces.");
        const QUrl url(t, QUrl::TolerantMode);
        if (!schemes.isEmpty() && !schemes.contains(url.scheme(), Qt::CaseInsensitive)) {
            QStringList shown;
            for (const auto &scheme : schemes) shown << scheme.toLower() + QStringLiteral("://");
            const QString list = shown.size() == 1 ? shown.first()
                                                   : QCoreApplication::translate("Validation", "%1 or %2").arg(shown.mid(0, shown.size() - 1).join(QStringLiteral(", ")), shown.last());
            return QCoreApplication::translate("Validation", "Must be a URL starting with %1.").arg(list);
        }
        if (const QStringView port = validationUrlPort(t); !port.isEmpty()) {
            const auto v = validationParseInt(port);
            if (!v || *v < 1 || *v > 65535) return validationPortMessage();
        }
        if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) return QCoreApplication::translate("Validation", "Not a valid URL.");
        return {};
    }

    QString OneOf(const QString &text, const QStringList &allowed, Empty empty, Qt::CaseSensitivity cs) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        if (allowed.contains(t, cs)) return {};
        QStringList shown;
        for (const auto &value : allowed) {
            if (value.isEmpty()) continue;
            if (shown.size() == 12) {
                shown << QString(QChar(0x2026));
                break;
            }
            shown << value;
        }
        return QCoreApplication::translate("Validation", "Unknown value \"%1\". Use one of: %2.").arg(validationShort(t), shown.join(QStringLiteral(", ")));
    }

    QString HttpToken(const QString &text, Empty empty) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return validationEmpty(empty);
        if (std::all_of(t.begin(), t.end(), validationHttpTokenChar)) return {};
        return QCoreApplication::translate("Validation", "Must not contain spaces or special characters.");
    }

    QString Regex(const QString &pattern) {
        const QString t = pattern.trimmed();
        if (t.isEmpty()) return {};
        const QRegularExpression re(t);
        if (re.isValid()) return {};
        return QCoreApplication::translate("Validation", "Invalid regular expression: %1.").arg(re.errorString());
    }

    QString GoRegex(const QString &pattern) {
        const QString t = pattern.trimmed();
        if (auto message = Regex(validationPcreCopy(t)); !message.isEmpty()) return message;
        return validationRe2Unsupported(t);
    }

    QString JsonObject(const QString &text, Empty empty) {
        if (text.trimmed().isEmpty()) return validationEmpty(empty);
        const QByteArray utf8 = text.toUtf8();
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(utf8, &error);
        if (error.error != QJsonParseError::NoError) {
            const qsizetype offset = std::clamp<qsizetype>(error.offset, 0, utf8.size());
            qsizetype line = 1, lineStart = 0;
            for (qsizetype i = 0; i < offset; ++i) {
                if (utf8[i] == '\n') {
                    ++line;
                    lineStart = i + 1;
                }
            }
            const qsizetype column = QString::fromUtf8(utf8.constData() + lineStart, offset - lineStart).size() + 1;
            return QCoreApplication::translate("Validation", "Not valid JSON: %1 (line %2, column %3).")
                .arg(error.errorString(), QString::number(line), QString::number(column));
        }
        if (!doc.isObject()) return QCoreApplication::translate("Validation", "Must be a JSON object.");
        return {};
    }

    QStringList Items(const QString &text, Split split) {
        QStringList parts;
        switch (split) {
            case Split::Comma:
                parts = text.split(u',');
                break;
            case Split::Newline:
                parts = text.split(u'\n');
                break;
            case Split::CommaOrNewline:
                parts = QString(text).replace(u'\n', u',').split(u',');
                break;
            case Split::Whitespace:
                parts = text.simplified().split(u' ');
                break;
        }
        QStringList items;
        for (const auto &part : parts) {
            if (const QString item = part.trimmed(); !item.isEmpty()) items << item;
        }
        return items;
    }

    QString EachItem(const QString &text, Split split, const Rule &item, Empty empty, bool echoItem) {
        const QStringList items = Items(text, split);
        if (items.isEmpty()) return validationEmpty(empty);
        if (!item) return {};
        for (qsizetype i = 0; i < items.size(); ++i) {
            const QString message = item(items[i]);
            if (message.isEmpty()) continue;
            if (!echoItem) return QCoreApplication::translate("Validation", "Item %1: %2").arg(QString::number(i + 1), message);
            return QCoreApplication::translate("Validation", "Item %1 (\"%2\"): %3").arg(QString::number(i + 1), validationShort(items[i]), message);
        }
        return {};
    }

    namespace Rules {
        Rule Required() {
            return [](const QString &text) { return Validation::Required(text); };
        }

        Rule Int(qint64 min, qint64 max, Empty empty) {
            return [=](const QString &text) { return Validation::Int(text, min, max, empty); };
        }

        Rule Port(Empty empty) {
            return [=](const QString &text) { return Validation::Port(text, empty); };
        }

        Rule PortRange(PortRangeStyle style) {
            return [=](const QString &text) { return Validation::PortRange(text, style); };
        }

        Rule PortRangeOrder(PortRangeStyle style) {
            return [=](const QString &text) { return Validation::PortRangeOrder(text, style); };
        }

        Rule IntOrRange(qint64 min, qint64 max, Empty empty, bool allowReversed) {
            return [=](const QString &text) { return Validation::IntOrRange(text, min, max, empty, allowReversed); };
        }

        Rule Duration(DurationRule rule) {
            return [=](const QString &text) { return Validation::Duration(text, rule); };
        }

        Rule MemoryBytes(bool allowBareInteger, Empty empty) {
            return [=](const QString &text) { return Validation::MemoryBytes(text, allowBareInteger, empty); };
        }

        Rule Ip(Family family, Empty empty) {
            return [=](const QString &text) { return Validation::Ip(text, family, empty); };
        }

        Rule Cidr(Family family, bool allowBareIp, Empty empty) {
            return [=](const QString &text) { return Validation::Cidr(text, family, allowBareIp, empty); };
        }

        Rule Host(Empty empty) {
            return [=](const QString &text) { return Validation::Host(text, empty); };
        }

        Rule HostPort(bool portRequired, Empty empty) {
            return [=](const QString &text) { return Validation::HostPort(text, portRequired, empty); };
        }

        Rule Base64Key(int bytes, bool urlAlphabet, bool padded, Empty empty) {
            return [=](const QString &text) { return Validation::Base64Key(text, bytes, urlAlphabet, padded, empty); };
        }

        Rule Base64(Empty empty) {
            return [=](const QString &text) { return Validation::Base64(text, empty); };
        }

        Rule Hex(int minChars, int maxChars, bool evenLength, Empty empty) {
            return [=](const QString &text) { return Validation::Hex(text, minChars, maxChars, evenLength, empty); };
        }

        Rule Uuid(Empty empty) {
            return [=](const QString &text) { return Validation::Uuid(text, empty); };
        }

        Rule XrayId(Empty empty) {
            return [=](const QString &text) { return Validation::XrayId(text, empty); };
        }

        Rule Url(QStringList schemes, Empty empty) {
            return [schemes = std::move(schemes), empty](const QString &text) { return Validation::Url(text, schemes, empty); };
        }

        Rule OneOf(QStringList allowed, Empty empty, Qt::CaseSensitivity cs) {
            return [allowed = std::move(allowed), empty, cs](const QString &text) { return Validation::OneOf(text, allowed, empty, cs); };
        }

        Rule HttpToken(Empty empty) {
            return [=](const QString &text) { return Validation::HttpToken(text, empty); };
        }

        Rule Regex() {
            return [](const QString &text) { return Validation::Regex(text); };
        }

        Rule GoRegex() {
            return [](const QString &text) { return Validation::GoRegex(text); };
        }

        Rule JsonObject(Empty empty) {
            return [=](const QString &text) { return Validation::JsonObject(text, empty); };
        }

        Rule List(Split split, Rule item, Empty empty, bool echoItem) {
            return [split, item = std::move(item), empty, echoItem](const QString &text) { return Validation::EachItem(text, split, item, empty, echoItem); };
        }

        Rule AllOf(QList<Rule> rules) {
            return [rules = std::move(rules)](const QString &text) {
                for (const auto &rule : rules) {
                    if (!rule) continue;
                    if (QString message = rule(text); !message.isEmpty()) return message;
                }
                return QString();
            };
        }
    }
}
