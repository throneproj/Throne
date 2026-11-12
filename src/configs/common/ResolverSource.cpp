#include "include/configs/common/ResolverSource.h"

#include <QCoreApplication>
#include <QHash>
#include <QHostAddress>
#include <QSet>

#include <algorithm>
#include <limits>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/TLS.h"
#include "include/configs/common/utils.h"
#include "include/database/DatabaseManager.h"
#include "include/database/IpListsRepo.h"
#include "include/global/Utils.hpp"
#include "include/global/Validation.hpp"

namespace Configs {
    struct ResolverSourceList {
        bool missing = true;
        bool truncated = false;
        QString name;
        QList<IpListEntry> entries;
    };

    struct ResolverSourceCache {
        QHash<int, ResolverSourceList> lists;
        QSet<int> logged;
    };

    namespace {
        constexpr qint64 kResolverSourceDnsttRangeHosts = 256;
        constexpr qint64 kResolverSourceMasterDnsRangeHosts = 65536;
        constexpr int kResolverSourceMaxExpanded = 4096;
        // Xray's xdns client opens a socket and a reader goroutine per resolver when it starts.
        constexpr int kResolverSourceMaxXdns = 256;
        constexpr int kResolverSourceMaxListEntries = 65536;
        constexpr int kResolverSourceMaxImportLines = 4096;

        thread_local ResolverSourceCache *resolverSourceCache = nullptr;

        struct ResolverSourceLine {
            enum class Kind { Skip, System, Host, Range, Url };
            Kind kind = Kind::Skip;
            QString scheme;
            QString host;
            int port = 0;
            QString path;
            int bits = 0;
            bool v6 = false;
            QString error;
        };

        QString resolverSourceShapeMessage(ResolverFlavor flavor) {
            switch (flavor) {
                case ResolverFlavor::Dnstt:
                    return QCoreApplication::translate("Validation", "Use system, an IP address with an optional port, a range such as 192.0.2.0/24, or a udp://, tcp://, tls:// or https:// address.");
                case ResolverFlavor::MasterDns:
                    return QCoreApplication::translate("Validation", "Use system, an IP address or a range such as 192.0.2.0/24, each with an optional :port.");
                case ResolverFlavor::Xdns:
                    break;
            }
            return QCoreApplication::translate("Validation", "Use an IP address with an optional port, or a udp://, tcp:// or https:// address.");
        }

        QString resolverSourceCanonicalIp(const QString &ip) {
            const QHostAddress address(ip);
            return address.isNull() ? ip : address.toString();
        }

        bool resolverSourceTarget(const QString &text, ResolverSourceLine &line) {
            if (text.contains(u'/')) {
                if (!Validation::Cidr(text).isEmpty()) return false;
                const auto slash = text.lastIndexOf(u'/');
                line.kind = ResolverSourceLine::Kind::Range;
                line.host = resolverSourceCanonicalIp(text.left(slash));
                line.bits = text.mid(slash + 1).toInt();
            } else {
                if (!Validation::Ip(text).isEmpty()) return false;
                line.kind = ResolverSourceLine::Kind::Host;
                line.host = resolverSourceCanonicalIp(text);
            }
            line.v6 = line.host.contains(u':');
            return true;
        }

        ResolverSourceLine resolverSourceParseUrl(const QString &text, ResolverFlavor flavor) {
            ResolverSourceLine line;
            const auto invalid = [&line, flavor] {
                line.error = resolverSourceShapeMessage(flavor);
                return line;
            };
            const auto sep = text.indexOf(QLatin1String("://"));
            const auto scheme = text.left(sep).toLower();
            static const QStringList schemes = {QStringLiteral("udp"), QStringLiteral("tcp"), QStringLiteral("tls"), QStringLiteral("https")};
            if (!schemes.contains(scheme) || (flavor == ResolverFlavor::Xdns && scheme == QLatin1String("tls"))) return invalid();
            auto authority = text.mid(sep + 3);
            QString path;
            if (const auto slash = authority.indexOf(u'/'); slash >= 0) {
                path = authority.mid(slash);
                authority = authority.left(slash);
            }
            const auto bad = [](QChar c) { return c.isSpace() || c == u'?' || c == u'#' || c == u'@'; };
            if (authority.isEmpty() || std::any_of(authority.begin(), authority.end(), bad) || std::any_of(path.begin(), path.end(), bad))
                return invalid();
            if (!path.isEmpty() && path != QLatin1String("/") && scheme != QLatin1String("https")) {
                line.error = QCoreApplication::translate("Validation", "Only https:// addresses take a path.");
                return line;
            }
            QString host = authority;
            QString port;
            if (authority.startsWith(u'[')) {
                const auto close = authority.indexOf(u']');
                if (close < 0) return invalid();
                host = authority.mid(1, close - 1);
                const auto after = authority.mid(close + 1);
                if (!after.isEmpty()) {
                    if (!after.startsWith(u':')) return invalid();
                    port = after.mid(1);
                }
                if (!Validation::Ip(host, Validation::Family::V6).isEmpty()) return invalid();
            } else if (const auto colon = authority.lastIndexOf(u':'); colon >= 0) {
                if (authority.indexOf(u':') != colon) return invalid();
                host = authority.left(colon);
                port = authority.mid(colon + 1);
            }
            if (const auto message = Validation::Host(host); !message.isEmpty()) {
                line.error = message;
                return line;
            }
            if (!port.isNull()) {
                if (port.isEmpty()) return invalid();
                if (const auto message = Validation::Port(port); !message.isEmpty()) {
                    line.error = message;
                    return line;
                }
                line.port = port.toInt();
            }
            line.kind = ResolverSourceLine::Kind::Url;
            line.scheme = scheme;
            line.host = Validation::Ip(host).isEmpty() ? resolverSourceCanonicalIp(host) : toAceHost(host);
            line.v6 = host.contains(u':');
            line.path = path == QLatin1String("/") ? QString() : path;
            return line;
        }

        // MasterDnsVPN config/client_resolvers.go parseResolverEntry: the bare form first, then host and port split at "]:" or the last ':'.
        ResolverSourceLine resolverSourceParse(const QString &raw, ResolverFlavor flavor) {
            ResolverSourceLine line;
            const auto text = raw.trimmed();
            if (text.isEmpty() || text.startsWith(u'#')) return line;
            if (text.compare(QLatin1String("system"), Qt::CaseInsensitive) == 0) {
                line.kind = ResolverSourceLine::Kind::System;
                return line;
            }
            if (text.contains(QLatin1String("://"))) {
                if (flavor == ResolverFlavor::Dnstt) return resolverSourceParseUrl(text, flavor);
                line.error = resolverSourceShapeMessage(flavor);
                return line;
            }
            if (resolverSourceTarget(text, line)) return line;
            QString hostPart;
            QString portPart;
            if (text.startsWith(u'[')) {
                if (const auto close = text.indexOf(u']'); close > 0 && text.mid(close + 1, 1) == QLatin1String(":")) {
                    hostPart = text.mid(1, close - 1).trimmed();
                    portPart = text.mid(close + 2).trimmed();
                }
            } else if (const auto colon = text.lastIndexOf(u':'); colon > 0) {
                hostPart = text.left(colon).trimmed();
                portPart = text.mid(colon + 1).trimmed();
            }
            ResolverSourceLine target;
            if (hostPart.isEmpty() || portPart.isEmpty() || !resolverSourceTarget(hostPart, target)) {
                line = {};
                line.error = resolverSourceShapeMessage(flavor);
                return line;
            }
            if (const auto message = Validation::Port(portPart); !message.isEmpty()) {
                line = {};
                line.error = message;
                return line;
            }
            target.port = portPart.toInt();
            return target;
        }

        // Xray's xdns mask reads an address without a scheme as udp://, and Go's URL parser splits a bare IPv6 at its last colon.
        ResolverSourceLine resolverSourceParseXdns(const QString &raw) {
            ResolverSourceLine line;
            const auto text = raw.trimmed();
            if (text.isEmpty() || text.startsWith(u'#')) return line;
            if (text.contains(QLatin1String("://"))) return resolverSourceParseUrl(text, ResolverFlavor::Xdns);
            if (text.compare(QLatin1String("system"), Qt::CaseInsensitive) == 0) {
                line.error = QCoreApplication::translate("Validation", "XDNS has no system resolver: enter the address of a DNS resolver.");
                return line;
            }
            if (const auto slash = text.indexOf(u'/'); slash >= 0) {
                auto head = text.left(slash);
                head.remove(u'[');
                line.error = Validation::Ip(head).isEmpty()
                                 ? QCoreApplication::translate("Validation", "Enter single addresses here; a range works only from an IP list.")
                                 : resolverSourceShapeMessage(ResolverFlavor::Xdns);
                return line;
            }
            if (Validation::Ip(text, Validation::Family::V6).isEmpty()) {
                line.error = QCoreApplication::translate("Validation", "Put an IPv6 address in brackets, for example [2001:db8::1]:53.");
                return line;
            }
            line = resolverSourceParseUrl(QStringLiteral("udp://") + text, ResolverFlavor::Xdns);
            if (line.error.isEmpty() && !Validation::Ip(line.host).isEmpty()) {
                line = {};
                line.error = resolverSourceShapeMessage(ResolverFlavor::Xdns);
            }
            return line;
        }

        // Usable addresses as MasterDnsVPN counts them: an IPv4 range below /31 loses its network and broadcast addresses, IPv6 below /127 the first.
        qint64 resolverSourceRangeHosts(const ResolverSourceLine &line) {
            const int hostBits = (line.v6 ? 128 : 32) - line.bits;
            if (hostBits > 40) return std::numeric_limits<qint64>::max();
            const qint64 total = qint64(1) << hostBits;
            if (!line.v6) return line.bits < 31 ? total - 2 : total;
            return line.bits < 127 ? total - 1 : total;
        }

        QString resolverSourceRangeLimit(const ResolverSourceLine &line, qint64 limit) {
            if (line.kind != ResolverSourceLine::Kind::Range || resolverSourceRangeHosts(line) <= limit) return {};
            const bool small = limit <= kResolverSourceDnsttRangeHosts;
            return QCoreApplication::translate("Validation", "A range may hold at most %1 addresses: use /%2 or longer for IPv4 and /%3 or longer for IPv6.")
                .arg(QString::number(limit), small ? QStringLiteral("24") : QStringLiteral("16"), small ? QStringLiteral("120") : QStringLiteral("112"));
        }

        // Only for ranges within kResolverSourceDnsttRangeHosts, whose host bits all sit in the last byte.
        QStringList resolverSourceRangeAddresses(const ResolverSourceLine &line) {
            const QHostAddress base(line.host);
            const int hostBits = (line.v6 ? 128 : 32) - line.bits;
            if (base.isNull() || hostBits < 0 || hostBits > 8) return {};
            const int count = 1 << hostBits;
            const bool dropFirst = line.v6 ? line.bits < 127 : line.bits < 31;
            const bool dropLast = !line.v6 && line.bits < 31;
            QStringList addresses;
            if (line.v6) {
                auto bytes = base.toIPv6Address();
                const auto network = quint8(bytes[15] & quint8(0xFF << hostBits));
                for (int i = dropFirst ? 1 : 0; i < count; ++i) {
                    bytes[15] = quint8(network | i);
                    addresses << QHostAddress(bytes).toString();
                }
            } else {
                const quint32 network = base.toIPv4Address() & ~quint32(count - 1);
                for (int i = dropFirst ? 1 : 0; i < (dropLast ? count - 1 : count); ++i) addresses << QHostAddress(network + quint32(i)).toString();
            }
            return addresses;
        }

        QString resolverSourceHostPort(const QString &host, bool v6, int port) {
            if (port <= 0) return host;
            return (v6 ? QLatin1Char('[') + host + QLatin1Char(']') : host) + QLatin1Char(':') + QString::number(port);
        }

        QString resolverSourceXdnsAddress(const ResolverSourceLine &line) {
            const auto host = line.host.toLower();
            if (line.scheme != QLatin1String("https")) return line.scheme + QStringLiteral("://") + resolverSourceHostPort(host, line.v6, line.port > 0 ? line.port : 53);
            const auto authority = line.v6 ? QLatin1Char('[') + host + QLatin1Char(']') : host;
            return QStringLiteral("https://") + (line.port > 0 ? authority + QLatin1Char(':') + QString::number(line.port) : authority) + line.path;
        }

        QJsonObject resolverSourceUdp(const QString &host, int port) {
            QJsonObject object{{"type", "udp"}, {"server", host}};
            if (port > 0) object["server_port"] = port;
            return object;
        }

        QJsonObject resolverSourceUrlObject(const ResolverSourceLine &line) {
            QJsonObject object{{"type", line.scheme}, {"server", line.host}};
            if (line.port > 0) object["server_port"] = line.port;
            if (line.scheme == QLatin1String("https") && !line.path.isEmpty()) object["path"] = line.path;
            if (line.scheme == QLatin1String("tls") || line.scheme == QLatin1String("https")) {
                QJsonObject tls{{"enabled", true}};
                if (!Validation::Ip(line.host).isEmpty()) tls["server_name"] = line.host;
                // uTLS::Build's rule: the global fingerprint, never "unsafe".
                const auto fingerprint = dataManager != nullptr ? dataManager->settingsRepo->utlsFingerprint.trimmed() : QString();
                if (!fingerprint.isEmpty() && fingerprint != tlsUnsafeFingerprint) {
                    tls["utls"] = QJsonObject{{"enabled", true}, {"fingerprint", fingerprint}};
                }
                object["tls"] = tls;
            }
            return object;
        }

        QString resolverSourceKey(const QJsonObject &object) {
            return QStringLiteral("%1|%2|%3|%4").arg(object.value("type").toString(), object.value("server").toString(),
                                                    QString::number(object.value("server_port").toInt()), object.value("path").toString());
        }

        ResolverSourceList resolverSourceReadList(int id) {
            auto *cache = resolverSourceCache;
            if (cache != nullptr) {
                if (const auto it = cache->lists.constFind(id); it != cache->lists.constEnd()) return it.value();
            }
            ResolverSourceList list;
            if (dataManager != nullptr && dataManager->ipListsRepo != nullptr) {
                if (const auto header = dataManager->ipListsRepo->GetIpListHeader(id); header != nullptr && !header->IsHidden()) {
                    list.missing = false;
                    list.name = header->name;
                    list.entries = dataManager->ipListsRepo->GetEntries(id, 0, kResolverSourceMaxListEntries + 1);
                    if (list.entries.size() > kResolverSourceMaxListEntries) {
                        list.truncated = true;
                        list.entries.resize(kResolverSourceMaxListEntries);
                    }
                }
            }
            if (cache != nullptr) cache->lists.insert(id, list);
            return list;
        }

        void resolverSourceLog(int listId, const QString &line) {
            auto *cache = resolverSourceCache;
            if (cache == nullptr || cache->logged.contains(listId)) return;
            cache->logged.insert(listId);
            MW_show_log(line);
        }
    }

    bool ResolverSource::IsEmpty() const {
        return ipListId < 0 && std::all_of(lines.cbegin(), lines.cend(), [](const QString &raw) {
            const auto text = raw.trimmed();
            return text.isEmpty() || text.startsWith(u'#');
        });
    }

    ResolverBuild BuildResolvers(const ResolverSource &source, ResolverFlavor flavor, const QString &owner) {
        ResolverBuild out;
        QSet<QString> seen;
        const int cap = flavor == ResolverFlavor::MasterDns ? 0 : flavor == ResolverFlavor::Xdns ? kResolverSourceMaxXdns : kResolverSourceMaxExpanded;
        const auto add = [&out, &seen, cap](const QJsonValue &value, const QString &key) {
            if (seen.contains(key)) return true;
            if (cap > 0 && out.resolvers.size() >= cap) return false;
            seen.insert(key);
            out.resolvers.append(value);
            return true;
        };
        const auto masterDnsText = [](const ResolverSourceLine &line) {
            const auto target = line.kind == ResolverSourceLine::Kind::Range ? line.host + QLatin1Char('/') + QString::number(line.bits) : line.host;
            return resolverSourceHostPort(target, line.v6, line.port);
        };

        for (const auto &raw : source.lines) {
            if (flavor == ResolverFlavor::Xdns) {
                const auto line = resolverSourceParseXdns(raw);
                if (!line.error.isEmpty()) {
                    out.error = QObject::tr("%1: the resolver \"%2\" is invalid: %3").arg(owner, raw.trimmed(), line.error);
                    return out;
                }
                if (line.kind == ResolverSourceLine::Kind::Url) {
                    const auto text = resolverSourceXdnsAddress(line);
                    add(text, text);
                }
                continue;
            }
            const bool dnstt = flavor == ResolverFlavor::Dnstt;
            const auto line = resolverSourceParse(raw, flavor);
            auto error = line.error;
            if (error.isEmpty()) error = resolverSourceRangeLimit(line, dnstt ? kResolverSourceDnsttRangeHosts : kResolverSourceMasterDnsRangeHosts);
            if (!error.isEmpty()) {
                out.error = QObject::tr("%1: the resolver \"%2\" is invalid: %3").arg(owner, raw.trimmed(), error);
                return out;
            }
            switch (line.kind) {
                case ResolverSourceLine::Kind::Skip:
                    break;
                case ResolverSourceLine::Kind::System:
                    if (dnstt) add(QJsonObject{{"type", "system"}}, QStringLiteral("system"));
                    else add(QStringLiteral("system"), QStringLiteral("system"));
                    break;
                case ResolverSourceLine::Kind::Url: {
                    const auto object = resolverSourceUrlObject(line);
                    add(object, resolverSourceKey(object));
                    break;
                }
                case ResolverSourceLine::Kind::Host:
                case ResolverSourceLine::Kind::Range:
                    if (!dnstt) {
                        const auto text = masterDnsText(line);
                        add(text, text);
                    } else if (line.kind == ResolverSourceLine::Kind::Host) {
                        const auto object = resolverSourceUdp(line.host, line.port);
                        add(object, resolverSourceKey(object));
                    } else {
                        for (const auto &address : resolverSourceRangeAddresses(line)) {
                            const auto object = resolverSourceUdp(address, line.port);
                            add(object, resolverSourceKey(object));
                        }
                    }
                    break;
            }
        }

        if (source.ipListId >= 0) {
            const auto list = resolverSourceReadList(source.ipListId);
            qsizetype skipped = 0;
            for (const auto &entry : list.entries) {
                ResolverSourceLine line;
                if (!resolverSourceTarget(entry.cidr.trimmed(), line)) {
                    ++skipped;
                    continue;
                }
                line.port = entry.port > 0 && entry.port <= 65535 ? entry.port : 0;
                if (flavor == ResolverFlavor::MasterDns) {
                    if (!resolverSourceRangeLimit(line, kResolverSourceMasterDnsRangeHosts).isEmpty()) {
                        ++skipped;
                        continue;
                    }
                    const auto text = masterDnsText(line);
                    add(text, text);
                    continue;
                }
                if (!resolverSourceRangeLimit(line, kResolverSourceDnsttRangeHosts).isEmpty()) {
                    ++skipped;
                    continue;
                }
                const auto addresses = line.kind == ResolverSourceLine::Kind::Range ? resolverSourceRangeAddresses(line) : QStringList{line.host};
                for (const auto &address : addresses) {
                    bool kept;
                    if (flavor == ResolverFlavor::Dnstt) {
                        const auto object = resolverSourceUdp(address, line.port);
                        kept = add(object, resolverSourceKey(object));
                    } else {
                        const auto text = QStringLiteral("udp://") + resolverSourceHostPort(address, line.v6, line.port > 0 ? line.port : 53);
                        kept = add(text, text);
                    }
                    if (!kept) ++skipped;
                }
            }
            if (list.missing) {
                resolverSourceLog(source.ipListId, QStringLiteral("%1: %2").arg(owner, QObject::tr("the resolver IP list no longer exists")));
            } else if (list.entries.isEmpty()) {
                resolverSourceLog(source.ipListId, QStringLiteral("%1: %2").arg(owner, QObject::tr("the resolver IP list \"%1\" is empty").arg(list.name)));
            } else if (list.truncated) {
                resolverSourceLog(source.ipListId, QStringLiteral("%1: %2").arg(owner, QObject::tr("the resolver IP list \"%1\" holds more than %2 entries; the rest were skipped")
                                                                                         .arg(list.name, QString::number(kResolverSourceMaxListEntries))));
            } else if (skipped > 0) {
                resolverSourceLog(source.ipListId, QStringLiteral("%1: %2").arg(owner, QObject::tr("%n entries of the resolver IP list \"%1\" were skipped: ranges too large or past the resolver limit", nullptr, int(skipped))
                                                                                         .arg(list.name)));
            }
        }

        if (out.resolvers.isEmpty() && flavor != ResolverFlavor::Xdns) {
            if (flavor == ResolverFlavor::Dnstt) out.resolvers.append(QJsonObject{{"type", "system"}});
            else out.resolvers.append(QStringLiteral("system"));
        }
        return out;
    }

    QString DnsttResolverObjectLine(const QJsonObject &object, bool *tcpFallback, QString *why) {
        const auto type = object.value("type").toString().trimmed().toLower();
        if (type == QLatin1String("system")) return QStringLiteral("system");
        const auto server = toAceHost(object.value("server").toString().trimmed());
        static const QStringList types = {QStringLiteral("udp"), QStringLiteral("tcp"), QStringLiteral("tls"), QStringLiteral("https")};
        if (!types.contains(type) || server.isEmpty()) {
            if (why != nullptr) *why = QCoreApplication::translate("Validation", "Only system, udp, tcp, tls and https resolvers with a server are supported.");
            return {};
        }
        if (type == QLatin1String("udp") && tcpFallback != nullptr && object.value("tcp_fallback").toBool()) *tcpFallback = true;
        const int port = object.value("server_port").toInt(0);
        const bool v6 = server.contains(u':');
        QString line;
        if (type == QLatin1String("udp") && Validation::Ip(server).isEmpty()) {
            line = resolverSourceHostPort(server, v6, port);
        } else {
            line = type + QStringLiteral("://") + resolverSourceHostPort(v6 ? QLatin1Char('[') + server + QLatin1Char(']') : server, false, port);
            if (const auto path = object.value("path").toString().trimmed(); type == QLatin1String("https") && !path.isEmpty())
                line += path.startsWith(u'/') ? path : QLatin1Char('/') + path;
        }
        const auto serverName = object.value("tls").toObject().value("server_name").toString().trimmed();
        if (why != nullptr && !serverName.isEmpty() && toAceHost(serverName) != server)
            *why = QCoreApplication::translate("Validation", "A TLS server name other than the resolver's own host is not kept.");
        return line;
    }

    QStringList ImportResolverLines(const QStringList &lines, ResolverFlavor flavor, const QString &field) {
        if (!ParseDiag::Active()) return lines;
        QStringList kept;
        bool truncated = false;
        for (const auto &raw : lines) {
            const auto line = raw.trimmed();
            if (line.isEmpty()) continue;
            if (kept.size() >= kResolverSourceMaxImportLines) {
                truncated = true;
                break;
            }
            QString message;
            if (flavor == ResolverFlavor::Dnstt) message = Checks::DnsttResolverLine(line);
            else if (flavor == ResolverFlavor::MasterDns) message = Checks::MasterDnsResolverLine(line);
            else message = Checks::XdnsResolverLine(line);
            if (AcceptOptional(field, message, line)) kept << line;
        }
        if (truncated) {
            ParseDiag::Warn(field, QCoreApplication::translate("Validation", "Only the first %1 resolvers were kept.").arg(kResolverSourceMaxImportLines));
        }
        return kept;
    }

    ResolverSourceScope::ResolverSourceScope() : previous_(resolverSourceCache) {
        if (previous_ != nullptr) return;
        cache_ = std::make_unique<ResolverSourceCache>();
        resolverSourceCache = cache_.get();
    }

    ResolverSourceScope::~ResolverSourceScope() {
        resolverSourceCache = previous_;
    }

    QString XdnsResolverAddress(const QString &line) {
        const auto parsed = resolverSourceParseXdns(line);
        if (!parsed.error.isEmpty() || parsed.kind != ResolverSourceLine::Kind::Url) return {};
        return resolverSourceXdnsAddress(parsed);
    }

    namespace Checks {
        QString DnsttResolverLine(const QString &line) {
            const auto parsed = resolverSourceParse(line, ResolverFlavor::Dnstt);
            if (!parsed.error.isEmpty()) return parsed.error;
            return resolverSourceRangeLimit(parsed, kResolverSourceDnsttRangeHosts);
        }

        QString MasterDnsResolverLine(const QString &line) {
            const auto parsed = resolverSourceParse(line, ResolverFlavor::MasterDns);
            if (!parsed.error.isEmpty()) return parsed.error;
            return resolverSourceRangeLimit(parsed, kResolverSourceMasterDnsRangeHosts);
        }

        QString XdnsResolverLine(const QString &line) {
            return resolverSourceParseXdns(line).error;
        }
    }
}
