#include "include/configs/outbounds/dnstt.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <algorithm>
#include <cmath>
#include <optional>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/DnsTunnel.h"
#include "include/configs/common/utils.h"
#include "include/configs/outbounds/shadowsocks.h"
#include "include/configs/outbounds/ssh.h"
#include "include/global/Utils.hpp"

namespace Configs {
    namespace {
        constexpr int kDnsttMinMtu = 25;
        constexpr qsizetype kDnsttMaxLinkSize = 64 * 1024;
        // The core's defaults: dnstt and VayDNS's compat mode, then native VayDNS.
        constexpr qint64 kDnsttIdleNs = 120'000'000'000;
        constexpr qint64 kDnsttKeepaliveNs = 10'000'000'000;
        constexpr qint64 kVayDnsIdleNs = 10'000'000'000;
        constexpr qint64 kVayDnsKeepaliveNs = 2'000'000'000;

        bool dnsttVayNative(const QString& mode, bool compat)
        {
            return mode == QLatin1String("vaydns") && !compat;
        }

        Validation::DurationRule dnsttPositiveDuration()
        {
            Validation::DurationRule rule;
            rule.minNs = 1'000'000;
            return rule;
        }

        QStringList dnsttNonBlankItems(const QJsonValue& value)
        {
            QStringList items;
            for (const auto& item : value.isString() ? QStringList{value.toString()} : QJsonArray2QListString(value.toArray())) {
                if (const auto trimmed = item.trimmed(); !trimmed.isEmpty()) items << trimmed;
            }
            return items;
        }

        // sing-box takes a bool or {enabled, version}.
        bool dnsttUdpOverTcp(const QJsonValue& value)
        {
            return value.isObject() ? value.toObject().value("enabled").toBool() : value.toBool();
        }

        QJsonObject dnsttBackendObject(const dnstt& bean)
        {
            QJsonObject object;
            if (bean.backend == QLatin1String("socks")) {
                if (!bean.socks_username.isEmpty()) object["username"] = bean.socks_username;
                if (!bean.socks_password.isEmpty()) object["password"] = bean.socks_password;
                if (bean.socks_udp_over_tcp) object["udp_over_tcp"] = true;
            } else if (bean.backend == QLatin1String("shadowsocks")) {
                if (!bean.ss_method.isEmpty()) object["method"] = bean.ss_method;
                if (!bean.ss_password.isEmpty()) object["password"] = bean.ss_password;
                if (bean.ss_udp_over_tcp) object["udp_over_tcp"] = true;
            } else if (bean.backend == QLatin1String("ssh")) {
                if (!bean.ssh_user.isEmpty()) object["user"] = bean.ssh_user;
                if (!bean.ssh_password.isEmpty()) object["password"] = bean.ssh_password;
                if (!bean.ssh_private_key.isEmpty()) object["private_key"] = bean.ssh_private_key;
                if (!bean.ssh_private_key_path.isEmpty()) object["private_key_path"] = bean.ssh_private_key_path;
                if (!bean.ssh_private_key_passphrase.isEmpty()) object["private_key_passphrase"] = bean.ssh_private_key_passphrase;
                if (!bean.ssh_host_key.isEmpty()) object["host_key"] = QListStr2QJsonArray(bean.ssh_host_key);
                if (!bean.ssh_host_key_algorithms.isEmpty()) object["host_key_algorithms"] = QListStr2QJsonArray(bean.ssh_host_key_algorithms);
                if (!bean.ssh_client_version.isEmpty()) object["client_version"] = bean.ssh_client_version;
            }
            return object;
        }

        void dnsttWriteCommon(const dnstt& bean, QJsonObject& object)
        {
            object["mode"] = bean.mode.isEmpty() ? QStringLiteral("dnstt") : bean.mode;
            if (bean.mode == QLatin1String("vaydns") && bean.dnstt_compat) object["dnstt_compat"] = true;
            if (!bean.domain.isEmpty()) object["domain"] = bean.domain;
            if (!bean.public_key.isEmpty()) object["public_key"] = bean.public_key.trimmed().toLower();
            if (!bean.record_type.isEmpty()) object["record_type"] = bean.record_type;
            if (bean.client_id_size > 0) object["client_id_size"] = bean.client_id_size;
            if (bean.max_qname_len > 0) object["max_qname_len"] = bean.max_qname_len;
            if (bean.rps > 0) object["rps"] = bean.rps;
            if (const auto idle = bean.idle_timeout.trimmed(); !idle.isEmpty()) object["idle_timeout"] = idle;
            if (const auto keepalive = bean.keepalive_interval.trimmed(); !keepalive.isEmpty()) object["keepalive_interval"] = keepalive;
            if (!bean.resolver_strategy.isEmpty()) object["resolver_strategy"] = bean.resolver_strategy;
            if (!bean.backend.isEmpty() && dnsttBackends.contains(bean.backend)) object[bean.backend] = dnsttBackendObject(bean);
        }

        bool dnsttReadBool(const QString& field, const QString& text, bool& target)
        {
            const auto value = text.trimmed().toLower();
            if (value == QLatin1String("true") || value == QLatin1String("1") || value == QLatin1String("yes")) target = true;
            else if (value.isEmpty() || value == QLatin1String("false") || value == QLatin1String("0") || value == QLatin1String("no")) target = false;
            else return AcceptOptional(field, QCoreApplication::translate("Validation", "Use true or false."), value);
            return true;
        }

        void dnsttReadVayKnobs(dnstt& bean, const QHash<QString, QString>& params)
        {
            if (params.contains("dnstt-compat")) dnsttReadBool("dnstt_compat", params.value("dnstt-compat"), bean.dnstt_compat);
            if (const auto text = params.value("record-type").trimmed().toLower(); !text.isEmpty()) {
                if (AcceptOptional("record_type", Checks::DnsttRecordType(bean.mode, bean.dnstt_compat, text), text)) bean.record_type = text;
            }
            if (const auto text = params.value("clientid-size").trimmed(); !text.isEmpty()) {
                auto message = Validation::Int(text, 0, 253);
                if (message.isEmpty()) message = Checks::DnsttClientIdSize(bean.mode, bean.dnstt_compat, text.toInt());
                if (AcceptOptional("client_id_size", message, text)) bean.client_id_size = text.toInt();
            }
            if (const auto text = params.value("idle-timeout").trimmed(); !text.isEmpty()) {
                if (AcceptOptional("idle_timeout", Validation::Duration(text, dnsttPositiveDuration()), text)) bean.idle_timeout = text;
            }
            if (const auto text = params.value("keepalive").trimmed(); !text.isEmpty()) {
                if (AcceptOptional("keepalive_interval", Validation::Duration(text, dnsttPositiveDuration()), text)) bean.keepalive_interval = text;
            }
        }

        bool dnsttParseDnst(dnstt& bean, const QString& link)
        {
            DnstLink parsed;
            QString error;
            if (!DecodeDnstLink(link, parsed, &error)) {
                ParseDiag::Error("link", error);
                return false;
            }
            if (!dnsttModes.contains(parsed.transport)) {
                ParseDiag::Error("mode", QCoreApplication::translate("Validation", "This DNS tunnel type is not supported."), parsed.transport);
                return false;
            }
            const auto& params = parsed.params;
            bean.mode = parsed.transport;
            bean.name = parsed.tag;
            bean.domain = toAceHost(parsed.domain);
            bean.public_key = params.value("pubkey").trimmed();
            // The spec gives these knobs to VayDNS only.
            if (bean.mode == QLatin1String("vaydns")) dnsttReadVayKnobs(bean, params);

            const auto user = params.value("user");
            const auto password = params.value("password");
            if (parsed.backend == QLatin1String("socks")) {
                bean.backend = "socks";
                bean.socks_username = user;
                bean.socks_password = password;
            } else if (parsed.backend == QLatin1String("ssh")) {
                bean.backend = "ssh";
                bean.ssh_user = user;
                bean.ssh_password = password;
                bean.ssh_private_key = params.value("pk");
            } else if (parsed.backend == QLatin1String("shadowsocks")) {
                bean.backend = "shadowsocks";
                bean.ss_method = params.value("method").trimmed().toLower();
                bean.ss_password = password;
            } else if (parsed.backend == QLatin1String("custom")) {
                bean.backend.clear();
            } else if (parsed.backend.isEmpty()) {
                ParseDiag::Error("backend", QCoreApplication::translate("Validation", "The link does not name the service behind the tunnel."));
                return false;
            } else {
                ParseDiag::Error("backend", QCoreApplication::translate("Validation", "This service behind the tunnel is not supported."), parsed.backend);
                return false;
            }
            if (parsed.hasResolvers) bean.resolvers.lines = ImportResolverLines(parsed.resolvers, ResolverFlavor::Dnstt, "resolvers");
            return true;
        }

        // "ip:port:flag" in every published sample; the transport field names how the resolver is reached.
        QString dnsttSlipnetResolver(const QString& text, const QString& transport)
        {
            auto parts = text.split(u':');
            if (parts.size() == 3) parts.removeLast();
            const auto address = parts.join(u':');
            if (address.isEmpty()) return {};
            const auto kind = transport.trimmed().toLower();
            if (kind.isEmpty() || kind == QLatin1String("udp")) return address;
            if (kind == QLatin1String("tcp")) return QStringLiteral("tcp://") + address;
            if (kind == QLatin1String("dot") || kind == QLatin1String("tls")) return QStringLiteral("tls://") + address;
            ParseDiag::Warn("resolvers", QCoreApplication::translate("Validation", "The resolver's transport is unknown, so it is used over UDP."), kind);
            return address;
        }

        // SlipNet publishes no spec: positional '|' fields inferred from public links of versions 16 to 28.
        bool dnsttParseSlipnet(dnstt& bean, const QString& link)
        {
            auto payload = link.mid(10);
            if (const auto hash = payload.indexOf(u'#'); hash >= 0) payload = payload.left(hash);
            const auto bytes = DnsTunnelBase64(payload.trimmed());
            if (bytes.isEmpty()) {
                ParseDiag::Error("link", QCoreApplication::translate("Validation", "The link is damaged: its payload is not valid base64."));
                return false;
            }
            const auto fields = QString::fromUtf8(bytes).split(u'|');
            const auto field = [&fields](int index) { return fields.value(index).trimmed(); };
            bool versionOk = false;
            const int version = field(0).toInt(&versionOk);
            if (!versionOk || version < 16) {
                ParseDiag::Error("link", QCoreApplication::translate("Validation", "This SlipNet link version is not supported."), field(0).left(16));
                return false;
            }
            const int needed = version >= 28 ? 79 : version >= 22 ? 60 : version >= 20 ? 50 : 36;
            if (fields.size() < needed) {
                ParseDiag::Error("link", QCoreApplication::translate("Validation", "The link is damaged: it is shorter than its version allows."));
                return false;
            }
            if (version > 28) {
                ParseDiag::Warn("link", QCoreApplication::translate("Validation", "This link comes from a newer SlipNet version; check the imported settings."), field(0));
            }

            auto type = field(1).toLower();
            const bool sshType = type.endsWith(QLatin1String("_ssh"));
            if (sshType) type.chop(4);
            if (type == QLatin1String("dnstt") || type == QLatin1String("vaydns")) {
                bean.mode = type;
            } else if (type == QLatin1String("sayedns")) {
                bean.mode = "dnstt";
                ParseDiag::Warn("mode", QCoreApplication::translate("Validation", "NoizDNS was imported as plain DNSTT: its server accepts that, but without NoizDNS's DPI evasion."));
            } else {
                ParseDiag::Error("mode", QCoreApplication::translate("Validation", "This SlipNet profile type is not supported."), field(1).left(32));
                return false;
            }

            bean.name = field(2);
            bean.domain = toAceHost(field(3));
            bean.public_key = field(11);
            if (const auto resolver = dnsttSlipnetResolver(field(4), field(22)); !resolver.isEmpty()) {
                bean.resolvers.lines = ImportResolverLines({resolver}, ResolverFlavor::Dnstt, "resolvers");
            }
            if (sshType || field(14) == QLatin1String("1")) {
                bean.backend = "ssh";
                bean.ssh_user = field(15);
                bean.ssh_password = fields.value(16);
                if (const auto key = field(24); !key.isEmpty()) {
                    const auto pem = key.contains(QLatin1String("-----BEGIN ")) ? key : DnstPrivateKeyPem(key);
                    if (pem.isEmpty()) ParseDiag::Warn("ssh_private_key", QCoreApplication::translate("Validation", "The SSH private key in the link could not be read; it was ignored."));
                    bean.ssh_private_key = pem;
                }
            } else {
                bean.backend = "socks";
                bean.socks_username = field(12);
                bean.socks_password = fields.value(13);
            }

            if (bean.mode == QLatin1String("vaydns")) {
                if (const auto text = field(42).toLower(); !text.isEmpty()) {
                    if (AcceptOptional("record_type", Checks::DnsttRecordType(bean.mode, false, text), text)) bean.record_type = text;
                }
                if (const auto text = field(43); !text.isEmpty() && text != QLatin1String("0")) {
                    if (AcceptOptional("max_qname_len", Validation::Int(text, 1, 253), text)) bean.max_qname_len = text.toInt();
                }
                if (const auto text = field(44); !text.isEmpty() && text != QLatin1String("0")) {
                    bool ok = false;
                    const double rps = text.toDouble(&ok);
                    if (AcceptOptional("rps", Checks::DnsttRps(ok ? rps : qQNaN()), text)) bean.rps = rps;
                }
                if (const auto text = field(49); !text.isEmpty() && text != QLatin1String("0")) {
                    if (AcceptOptional("client_id_size", Validation::Int(text, 1, 253), text)) bean.client_id_size = text.toInt();
                }
            }
            return true;
        }
    }

    namespace Checks {
        QString DnsttPublicKey(const QString& text)
        {
            if (Validation::Hex(text.trimmed(), 64, 64, true, Validation::Empty::Required).isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Use the server's public key: 64 hexadecimal digits, as the server prints it.");
        }

        QString DnsttRecordType(const QString& mode, bool compat, const QString& recordType)
        {
            if (const auto message = Validation::OneOf(recordType, dnsttRecordTypes); !message.isEmpty()) return message;
            if (!dnsttVayNative(mode, compat) && !recordType.isEmpty() && recordType != QLatin1String("txt"))
                return QCoreApplication::translate("Validation", "The classic dnstt wire only uses TXT records.");
            return {};
        }

        QString DnsttClientIdSize(const QString& mode, bool compat, int size)
        {
            if (dnsttVayNative(mode, compat)) return Validation::Int(QString::number(size), 0, 253);
            if (size == 0 || size == 8) return {};
            return QCoreApplication::translate("Validation", "The classic dnstt wire always uses 8-byte client IDs.");
        }

        // dnstt-client nameCapacity and VayDNS's QNAME budget: labels cost a length byte each, base32 carries 5 bits per character.
        int DnsttMtu(const QString& mode, bool compat, const QString& domain, int maxQnameLen, int clientIdSize)
        {
            auto name = toAceHost(domain.trimmed());
            while (name.endsWith(u'.')) name.chop(1);
            int used = 0;
            for (const auto& label : name.split(u'.', Qt::SkipEmptyParts)) used += int(label.size()) + 1;
            int capacity = 254 - used;
            int overhead = 13;
            if (mode == QLatin1String("vaydns")) {
                int budget = compat ? 253 : 101;
                if (maxQnameLen > 0) budget = std::min(maxQnameLen, 253);
                capacity = std::max(budget - used, 0);
                if (!compat) overhead = (clientIdSize > 0 ? clientIdSize : 2) + 1;
            }
            return capacity * 63 / 64 * 5 / 8 - overhead;
        }

        QString DnsttMtuError(const QString& mode, bool compat, const QString& domain, int maxQnameLen, int clientIdSize)
        {
            const int mtu = DnsttMtu(mode, compat, domain, maxQnameLen, clientIdSize);
            if (mtu >= kDnsttMinMtu) return {};
            return QCoreApplication::translate("Validation", "Each query would carry %1 bytes, fewer than the %2 the tunnel needs: use a shorter domain or a longer maximum QNAME length.")
                .arg(std::max(mtu, 0))
                .arg(kDnsttMinMtu);
        }

        QString DnsttRps(double rps)
        {
            if (!std::isfinite(rps)) return QCoreApplication::translate("Validation", "Must be a number.");
            if (rps < 0) return QCoreApplication::translate("Validation", "Must not be negative.");
            return {};
        }

        QString DnsttKeepalive(const QString& mode, bool compat, const QString& idleTimeout, const QString& keepalive)
        {
            const bool vay = dnsttVayNative(mode, compat);
            const auto idle = idleTimeout.trimmed().isEmpty() ? std::optional<qint64>(vay ? kVayDnsIdleNs : kDnsttIdleNs) : Validation::ParseDurationNs(idleTimeout.trimmed());
            const auto alive = keepalive.trimmed().isEmpty() ? std::optional<qint64>(vay ? kVayDnsKeepaliveNs : kDnsttKeepaliveNs) : Validation::ParseDurationNs(keepalive.trimmed());
            if (!idle || !alive || *idle <= 0 || *alive <= 0 || *alive < *idle) return {};
            return QCoreApplication::translate("Validation", "The keepalive interval must be shorter than the idle timeout.");
        }

        QString DnsttChainOnly(const QString& backend)
        {
            if (!backend.isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Without a backend, only a profile chained over this one can use the tunnel.");
        }
    }

    bool dnstt::ParseFromLink(const QString& link)
    {
        if (link.size() > kDnsttMaxLinkSize) {
            ParseDiag::Error("link", QCoreApplication::translate("Validation", "The link is too long."));
            return false;
        }
        if (link.startsWith(QLatin1String("slipnet://"))) return dnsttParseSlipnet(*this, link);
        if (link.startsWith(QLatin1String("slipnet-"))) {
            ParseDiag::Error("link", QCoreApplication::translate("Validation", "Encrypted SlipNet links cannot be read: export the profile from SlipNet without encryption."));
            return false;
        }
        return dnsttParseDnst(*this, link);
    }

    bool dnstt::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "dnstt") return false;
        outbound::ParseFromJson(object);
        // The tunnel only dials resolvers; a server here would be refused by the core.
        server.clear();
        server_port = 0;
        if (object.contains("mode")) mode = object["mode"].toString().trimmed().toLower();
        if (mode.isEmpty()) mode = "dnstt";
        dnstt_compat = object["dnstt_compat"].toBool();
        if (object.contains("domain")) domain = toAceHost(object["domain"].toString().trimmed());
        if (object.contains("public_key")) public_key = object["public_key"].toString().trimmed();
        if (object.contains("record_type")) record_type = object["record_type"].toString().trimmed().toLower();
        client_id_size = object["client_id_size"].toInt();
        max_qname_len = object["max_qname_len"].toInt();
        rps = object["rps"].toDouble();
        if (object.contains("idle_timeout")) idle_timeout = object["idle_timeout"].toString().trimmed();
        if (object.contains("keepalive_interval")) keepalive_interval = object["keepalive_interval"].toString().trimmed();
        if (object.contains("resolver_strategy")) resolver_strategy = object["resolver_strategy"].toString().trimmed().toLower();
        tcp_fallback = object["tcp_fallback"].toBool();

        if (object.contains("resolvers")) {
            const auto value = object["resolvers"];
            QStringList lines;
            for (const auto& item : value.isArray() ? value.toArray() : QJsonArray{value}) {
                if (item.isString()) {
                    lines << item.toString();
                } else if (item.isObject()) {
                    QString why;
                    const auto line = DnsttResolverObjectLine(item.toObject(), &tcp_fallback, &why);
                    if (!why.isEmpty()) ParseDiag::Warn("resolvers", why, line);
                    if (!line.isEmpty()) lines << line;
                } else if (!item.isNull()) {
                    ParseDiag::Warn("resolvers", QCoreApplication::translate("Validation", "List items that are not text were ignored."));
                }
            }
            resolvers.lines = ImportResolverLines(lines, ResolverFlavor::Dnstt, "resolvers");
        }
        if (object.contains("resolver_list")) {
            if (ParseDiag::Active()) {
                ParseDiag::Warn("resolver_list", QCoreApplication::translate("Validation", "The resolver IP list belongs to another installation, so it was not imported."));
            } else {
                resolvers.ipListId = object["resolver_list"].toInt(-1);
            }
        }

        QStringList backends;
        for (const auto& name : {QStringLiteral("socks"), QStringLiteral("shadowsocks"), QStringLiteral("ssh")}) {
            if (object[name].isObject()) backends << name;
        }
        if (backends.size() > 1) {
            ParseDiag::Error("backend", QCoreApplication::translate("Validation", "Use only one of socks, shadowsocks and ssh."), backends.join(", "));
        }
        backend = backends.value(0);
        const auto options = object[backend].toObject();
        if (backend == QLatin1String("socks")) {
            socks_username = options["username"].toString();
            socks_password = options["password"].toString();
            socks_udp_over_tcp = dnsttUdpOverTcp(options["udp_over_tcp"]);
        } else if (backend == QLatin1String("shadowsocks")) {
            ss_method = options["method"].toString().trimmed().toLower();
            ss_password = options["password"].toString();
            ss_udp_over_tcp = dnsttUdpOverTcp(options["udp_over_tcp"]);
        } else if (backend == QLatin1String("ssh")) {
            ssh_user = options["user"].toString();
            ssh_password = options["password"].toString();
            // The core joins private_key lines with '\n'.
            if (options.contains("private_key")) {
                const auto key = options["private_key"];
                ssh_private_key = key.isString() ? key.toString() : QJsonArray2QListString(key.toArray()).join('\n');
            }
            ssh_private_key_path = options["private_key_path"].toString();
            ssh_private_key_passphrase = options["private_key_passphrase"].toString();
            ssh_host_key = dnsttNonBlankItems(options["host_key"]);
            ssh_host_key_algorithms = dnsttNonBlankItems(options["host_key_algorithms"]);
            ssh_client_version = options["client_version"].toString();
        }
        return true;
    }

    QString dnstt::ExportToLink()
    {
        DnstLink link;
        link.tag = name;
        link.domain = domain;
        link.transport = mode == QLatin1String("vaydns") ? QStringLiteral("vaydns") : QStringLiteral("dnstt");
        link.backend = backend.isEmpty() ? QStringLiteral("custom") : backend;
        auto& params = link.params;
        params.insert("pubkey", public_key.trimmed().toLower());
        if (mode == QLatin1String("vaydns")) {
            params.insert("record-type", record_type);
            if (dnstt_compat) params.insert("dnstt-compat", "true");
            if (client_id_size > 0) params.insert("clientid-size", QString::number(client_id_size));
            params.insert("idle-timeout", idle_timeout.trimmed());
            params.insert("keepalive", keepalive_interval.trimmed());
        }
        bool needsJson = false;
        if (backend == QLatin1String("socks")) {
            params.insert("user", socks_username);
            params.insert("password", socks_password);
        } else if (backend == QLatin1String("ssh")) {
            params.insert("user", ssh_user);
            params.insert("password", ssh_password);
            if (const auto key = ssh_private_key.trimmed(); !key.isEmpty()) {
                params.insert("pk", key);
                needsJson = DnstPrivateKeyParam(key).isEmpty();
            }
        } else if (backend == QLatin1String("shadowsocks")) {
            params.insert("method", ss_method);
            params.insert("password", ss_password);
        }
        return EncodeDnstLink(link, needsJson);
    }

    QJsonObject dnstt::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "dnstt";
        mergeJsonObjects(object, outbound::ExportToJson());
        dnsttWriteCommon(*this, object);
        if (!resolvers.lines.isEmpty()) object["resolvers"] = QListStr2QJsonArray(resolvers.lines);
        if (resolvers.ipListId >= 0) object["resolver_list"] = resolvers.ipListId;
        if (tcp_fallback) object["tcp_fallback"] = true;
        return object;
    }

    QJsonObject dnstt::ExportIdentity()
    {
        QJsonObject object{{"type", "dnstt"}, {"mode", mode}, {"domain", domain.toLower()}, {"public_key", public_key.trimmed().toLower()}};
        if (mode == QLatin1String("vaydns") && dnstt_compat) object["dnstt_compat"] = true;
        if (!backend.isEmpty()) {
            QJsonObject identity{{"type", backend}};
            if (backend == QLatin1String("socks")) identity["username"] = socks_username;
            else if (backend == QLatin1String("ssh")) identity["user"] = ssh_user;
            else if (backend == QLatin1String("shadowsocks")) identity["method"] = ss_method;
            object["backend"] = identity;
        }
        return object;
    }

    BuildResult dnstt::Build()
    {
        QJsonObject object;
        object["type"] = "dnstt";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        dnsttWriteCommon(*this, object);
        auto built = BuildResolvers(resolvers, ResolverFlavor::Dnstt, DisplayTypeAndName());
        if (!built.error.isEmpty()) return {{}, built.error};
        if (tcp_fallback) {
            for (qsizetype i = 0; i < built.resolvers.size(); ++i) {
                auto resolver = built.resolvers[i].toObject();
                if (resolver.value("type").toString() != QLatin1String("udp")) continue;
                resolver["tcp_fallback"] = true;
                built.resolvers[i] = resolver;
            }
        }
        object["resolvers"] = built.resolvers;
        return {object, ""};
    }

    QString dnstt::DisplayAddress()
    {
        return domain;
    }

    QString dnstt::DisplayType()
    {
        return DnsttDisplayType(mode);
    }

    SecurityInfo dnstt::GetSecurity()
    {
        return {QObject::tr("Encrypted"), {}, SecurityLevel::Secure};
    }

    void dnstt::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        using Validation::Severity;
        const auto report = [&issues](const QString& field, const QString& message, const QString& value = {},
                                      Severity severity = Severity::Error) {
            if (!message.isEmpty()) issues << Validation::Issue{severity, field, message, value};
        };
        report("mode", Validation::OneOf(mode, dnsttModes, Validation::Empty::Required), mode);
        const auto domainMessage = Checks::DnsTunnelDomain(domain);
        report("domain", domainMessage, domain);
        report("public_key", Checks::DnsttPublicKey(public_key));
        report("record_type", Checks::DnsttRecordType(mode, dnstt_compat, record_type), record_type);
        report("client_id_size", Checks::DnsttClientIdSize(mode, dnstt_compat, client_id_size), QString::number(client_id_size));
        report("max_qname_len", Validation::Int(QString::number(max_qname_len), 0, 253), QString::number(max_qname_len));
        if (domainMessage.isEmpty()) report("domain", Checks::DnsttMtuError(mode, dnstt_compat, domain, max_qname_len, client_id_size), domain);
        report("rps", Checks::DnsttRps(rps), QString::number(rps));
        report("idle_timeout", Validation::Duration(idle_timeout, dnsttPositiveDuration()), idle_timeout);
        report("keepalive_interval", Validation::Duration(keepalive_interval, dnsttPositiveDuration()), keepalive_interval);
        report("keepalive_interval", Checks::DnsttKeepalive(mode, dnstt_compat, idle_timeout, keepalive_interval), keepalive_interval);
        for (const auto& line : resolvers.lines) {
            if (const auto message = Checks::DnsttResolverLine(line); !message.isEmpty()) {
                report("resolvers", message, line.trimmed());
                break;
            }
        }
        report("resolver_strategy", Validation::OneOf(resolver_strategy, dnsttResolverStrategies), resolver_strategy);
        report("backend", Validation::OneOf(backend, dnsttBackends), backend);
        report("backend", Checks::DnsttChainOnly(backend), {}, Severity::Warning);
        if (backend == QLatin1String("shadowsocks")) {
            report("ss_method", Validation::OneOf(ss_method, shadowsocksMethods, Validation::Empty::Required), ss_method);
            report("ss_password", Checks::ShadowsocksPassword(ss_method, ss_password));
        } else if (backend == QLatin1String("ssh")) {
            report("ssh_auth", Checks::SshAuthentication(ssh_password, ssh_private_key, ssh_private_key_path), {}, Severity::Warning);
            report("ssh_private_key", Checks::SshPrivateKey(ssh_private_key));
            report("ssh_private_key", Checks::SshKeySources(ssh_private_key, ssh_private_key_path), {}, Severity::Warning);
            for (const auto& key : ssh_host_key) {
                if (const auto message = Checks::SshHostKey(key); !message.isEmpty()) {
                    report("ssh_host_key", message, key);
                    break;
                }
            }
            report("ssh_client_version", Checks::SshClientVersion(ssh_client_version), ssh_client_version, Severity::Warning);
        }
    }
}
