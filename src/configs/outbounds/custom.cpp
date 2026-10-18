#include "include/configs/outbounds/custom.h"

#include "include/configs/common/OutboundFactory.h"
#include "include/configs/common/utils.h"
#include "include/configs/outbounds/xrayVless.h"
#include "include/global/Utils.hpp"

#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <memory>
#include <optional>

namespace Configs
{
    namespace
    {
        bool isSingBoxInfra(const QString& t)
        {
            return t == "direct" || t == "block" || t == "dns";
        }

        SecurityInfo analyzeSingBoxOutbound(const QJsonObject& o)
        {
            const auto type = o["type"].toString();
            if (type.isEmpty() || type == "custom" || type == "selector" || type == "urltest"
                || isSingBoxInfra(type))
                return {};
            std::shared_ptr<outbound> ob(NewOutboundByType(type));
            if (!ob || ob->invalid) return {};
            ob->ParseFromJson(o);
            return WithPrivateServer(ob->GetSecurity(), ob->GetAddress());
        }

        QJsonObject singBoxOutboundIdentity(const QJsonObject& o)
        {
            const auto type = o["type"].toString();
            if (type.isEmpty() || type == "custom" || type == "selector" || type == "urltest"
                || isSingBoxInfra(type))
                return {};
            std::shared_ptr<outbound> ob(NewOutboundByType(type));
            if (!ob || ob->invalid) return {};
            ob->ParseFromJson(o);
            return ob->ExportIdentity();
        }

        QJsonObject resolveSingBoxEgress(const QJsonArray& outbounds, const QString& tag, int depth)
        {
            if (depth <= 0 || outbounds.isEmpty()) return {};
            // The named tag, or the first outbound (sing-box's default egress).
            QJsonObject target;
            if (tag.isEmpty()) {
                target = outbounds.first().toObject();
            } else {
                for (const auto& v : outbounds) {
                    if (auto obj = v.toObject(); obj["tag"].toString() == tag) {
                        target = obj;
                        break;
                    }
                }
            }
            if (target.isEmpty()) return {};
            const auto type = target["type"].toString();
            if (type == "selector" || type == "urltest") {
                auto subs = target["outbounds"].toArray();
                QString next = target["default"].toString();
                if (next.isEmpty() && !subs.isEmpty()) next = subs.first().toString();
                return resolveSingBoxEgress(outbounds, next, depth - 1);
            }
            if (type.isEmpty() || isSingBoxInfra(type)) return {};
            return target;
        }

        // The core matches object keys case-insensitively, so every spelling of a key sets the same option.
        QStringList keysNamed(const QJsonObject& o, const QString& name)
        {
            QStringList keys;
            for (const auto& key : o.keys()) {
                if (key.compare(name, Qt::CaseInsensitive) == 0) keys << key;
            }
            return keys;
        }

        QJsonValue customValueNamed(const QJsonObject& o, const QString& name)
        {
            const auto keys = keysNamed(o, name);
            return keys.isEmpty() ? QJsonValue() : o.value(keys.first());
        }

        bool isXrayInfra(const QString& p)
        {
            // direct and block are Xray's aliases of freedom and blackhole.
            return p == "freedom" || p == "direct" || p == "blackhole" || p == "block" || p == "dns" || p == "loopback";
        }

        // Xray keeps one spelling of a key, so a label trusts only a value every spelling agrees on: undefined when they differ.
        QJsonValue xrayAgreed(const QJsonObject& o, const QString& name)
        {
            const auto keys = keysNamed(o, name);
            for (const auto& key : keys) {
                if (o.value(key) != o.value(keys.first())) return QJsonValue(QJsonValue::Undefined);
            }
            return keys.isEmpty() ? QJsonValue(QJsonValue::Undefined) : o.value(keys.first());
        }

        // For names Xray lower-cases; empty when there are none, nullopt when they differ.
        std::optional<QString> xrayAgreedName(const QStringList& lowerCased)
        {
            if (lowerCased.isEmpty()) return QString();
            if (lowerCased.count(lowerCased.first()) != lowerCased.size()) return std::nullopt;
            return lowerCased.first();
        }

        std::optional<QString> xrayAgreedProtocol(const QJsonObject& o)
        {
            QStringList protocols;
            for (const auto& key : keysNamed(o, "protocol")) protocols << o.value(key).toString().toLower();
            return xrayAgreedName(protocols);
        }

        // Xray lets a non-null top-level address replace vnext/servers, taking the inline user with it; nullopt when the spellings disagree.
        std::optional<bool> xrayInlinePeer(const QJsonObject& settings)
        {
            bool set = false;
            bool unset = false;
            for (const auto& key : keysNamed(settings, "address")) (settings.value(key).isNull() ? unset : set) = true;
            if (set && unset) return std::nullopt;
            return set;
        }

        // Only VMess and VLESS read vnext; the other protocols read servers and ignore a vnext list.
        QJsonObject xrayPeer(const QJsonObject& settings, const QString& protocol)
        {
            const auto inlinePeer = xrayInlinePeer(settings);
            if (!inlinePeer) return {};
            if (*inlinePeer) return settings;
            const auto list = protocol == "vmess" || protocol == "vless" ? QStringLiteral("vnext") : QStringLiteral("servers");
            return xrayAgreed(settings, list).toArray().at(0).toObject();
        }

        // Empty when the protocol is missing or its spellings disagree, which leaves the list Xray reads unknown.
        QJsonObject xrayOutboundPeer(const QJsonObject& o)
        {
            const auto protocol = xrayAgreedProtocol(o);
            if (!protocol || protocol->isEmpty()) return {};
            return xrayPeer(xrayAgreed(o, "settings").toObject(), *protocol);
        }

        QString xrayVlessEncryption(const QJsonObject& settings)
        {
            const auto inlinePeer = xrayInlinePeer(settings);
            if (!inlinePeer) return {};
            const auto user = *inlinePeer ? settings : xrayAgreed(xrayPeer(settings, "vless"), "users").toArray().at(0).toObject();
            return xrayAgreed(user, "encryption").toString();
        }

        QString xrayOutboundAddress(const QJsonObject& o)
        {
            return xrayAgreed(xrayOutboundPeer(o), "address").toString();
        }

        SecurityInfo xrayOutboundSecurity(const QJsonObject& o)
        {
            const auto agreedProtocol = xrayAgreedProtocol(o);
            if (agreedProtocol && (agreedProtocol->isEmpty() || isXrayInfra(*agreedProtocol))) return {};
            // Disagreeing spellings leave the protocol unknown, so only TLS or REALITY lift the label above Raw.
            const auto protocol = agreedProtocol.value_or(QString());

            // Go's decoder fills one stream from every spelling of streamSettings; a null one discards those decoded before it.
            QList<QJsonObject> streams;
            bool streamsKnown = true;
            for (const auto& key : keysNamed(o, "streamSettings")) {
                streamsKnown = streamsKnown && o.value(key).isObject();
                streams << o.value(key).toObject();
            }
            SecurityInfo info;
            info.transport = DisplayTransportName(customValueNamed(streams.value(0), "network").toString().toLower());

            QStringList securities;
            for (const auto& stream : streams) {
                for (const auto& key : keysNamed(stream, "security")) securities << stream.value(key).toString().toLower();
            }
            const auto security = streamsKnown ? xrayAgreedName(securities).value_or(QString()) : QString();
            if (security == "reality") {
                info.label = QObject::tr("Reality");
                info.level = SecurityLevel::Secure;
            } else if (security == "tls") {
                bool insecure = false;
                for (const auto& stream : streams) {
                    for (const auto& tlsKey : keysNamed(stream, "tlsSettings")) {
                        const auto tls = stream.value(tlsKey).toObject();
                        for (const auto& key : keysNamed(tls, "allowInsecure")) insecure = insecure || tls.value(key).toBool();
                    }
                }
                info.label = insecure ? QObject::tr("Insecure TLS") : QObject::tr("TLS");
                info.level = insecure ? SecurityLevel::Weak : SecurityLevel::Secure;
            } else {
                info.label = QObject::tr("Raw");
                info.level = SecurityLevel::None;
            }
            // Shadowsocks, WireGuard and VLESS Encryption wrap the whole connection, both XHTTP legs included; Xray's none and plain ciphers do not encrypt.
            const auto ssMethod = XrayLower(xrayAgreed(xrayOutboundPeer(o), "method").toString());
            const bool encrypts = (protocol == "shadowsocks" && !ssMethod.isEmpty() && ssMethod != "none" && ssMethod != "plain")
                                  || protocol == "wireguard"
                                  || (protocol == "vless" && IsVlessEncrypted(xrayVlessEncryption(xrayAgreed(o, "settings").toObject())));
            if (!encrypts) info = WithXrayDownloadLeg(info, xrayOutboundAddress(o), XrayDownloadLeg(o));
            if (info.level != SecurityLevel::None) return info;
            // Without transport security those still encrypt; VMess counts as insecure.
            if (encrypts) {
                info.label = QObject::tr("Encrypted");
                info.level = SecurityLevel::Secure;
                return info;
            }
            if (protocol == "vmess") {
                info.label = QObject::tr("Insecure");
                info.level = SecurityLevel::Weak;
            }
            return info;
        }

        SecurityInfo analyzeXrayOutbound(const QJsonObject& o)
        {
            return WithPrivateServer(xrayOutboundSecurity(o), xrayOutboundAddress(o));
        }

        QJsonObject xrayOutboundIdentity(const QJsonObject& o)
        {
            const auto protocol = o["protocol"].toString();
            if (protocol.isEmpty() || isXrayInfra(protocol)) return {};
            QJsonObject id;
            id["protocol"] = protocol;
            const auto settings = o["settings"].toObject();
            QJsonObject peer;
            if (settings.contains("vnext")) peer = settings["vnext"].toArray().first().toObject();
            else if (settings.contains("servers")) peer = settings["servers"].toArray().first().toObject();
            if (!peer.isEmpty()) {
                id["server"] = peer["address"];
                id["server_port"] = peer["port"];
            }
            const auto stream = o["streamSettings"].toObject();
            QJsonObject sid;
            if (const auto network = stream["network"].toString(); !network.isEmpty()) sid["network"] = network;
            const auto security = stream["security"].toString();
            if (!security.isEmpty()) sid["security"] = security;
            if (security == "reality") {
                const auto r = stream["realitySettings"].toObject();
                if (r.contains("serverName")) sid["sni"] = r["serverName"];
                if (r.contains("fingerprint")) sid["fingerprint"] = r["fingerprint"];
            } else if (security == "tls") {
                const auto t = stream["tlsSettings"].toObject();
                if (t.contains("serverName")) sid["sni"] = t["serverName"];
                if (t.contains("fingerprint")) sid["fingerprint"] = t["fingerprint"];
            }
            if (!sid.isEmpty()) id["stream"] = sid;
            return id;
        }

        // Xray routes to the first outbound by default; pick the first real proxy.
        QJsonObject firstXrayEgress(const QJsonArray& outbounds)
        {
            for (const auto& v : outbounds) {
                if (auto obj = v.toObject(); !isXrayInfra(obj["protocol"].toString())) return obj;
            }
            return {};
        }

        bool isBlank(const QJsonValue& value)
        {
            return value.isNull() || value.isUndefined() || (value.isString() && value.toString().isEmpty()) ||
                   (value.isArray() && value.toArray().isEmpty()) || (value.isObject() && value.toObject().isEmpty());
        }

        // torrc keys reach Tor's SETCONF unescaped.
        bool isTorOptionName(const QString& key)
        {
            if (key.isEmpty()) return false;
            for (const auto c : key) {
                if (!(c >= u'A' && c <= u'Z') && !(c >= u'a' && c <= u'z') && !(c >= u'0' && c <= u'9') && c != u'_') return false;
            }
            return true;
        }

        // A pluggable-transport line ("obfs4 exec /path/to/program") makes Tor spawn that program; Tor also takes abbreviated option names.
        bool isTorPluginOption(const QString& key)
        {
            return key.size() >= 4 && (QStringLiteral("ClientTransportPlugin").startsWith(key, Qt::CaseInsensitive) ||
                                       QStringLiteral("ServerTransportPlugin").startsWith(key, Qt::CaseInsensitive));
        }

        // Removes what would make the core start a program or let tailnet peers reach this computer; true when anything went.
        bool stripLaunchers(QJsonObject& o, const QString& at, Validation::Issues& issues)
        {
            bool removed = false;
            const auto report = [&](const QString& key, const QString& message, const QString& value = {}) {
                issues << Validation::Issue{Validation::Severity::Warning, at + key, message, value};
                removed = true;
            };
            const auto strip = [&](const QString& name, const QString& message, bool showValue) {
                for (const auto& key : keysNamed(o, name)) {
                    if (isBlank(o.value(key))) continue;
                    report(key, message, showValue ? o.value(key).toString() : QString());
                    o.remove(key);
                }
            };
            const auto type = o.value("type").toString();
            if (type == "tor") {
                strip("executable_path",
                      QCoreApplication::translate("Validation", "Removed the Tor executable path from the imported configuration; add it back only if you trust that program."),
                      true);
                strip("extra_args",
                      QCoreApplication::translate("Validation", "Removed the Tor command-line arguments from the imported configuration: they can make Tor start other programs."),
                      false);
                strip("data_directory",
                      QCoreApplication::translate("Validation", "Removed the Tor data directory from the imported configuration: Tor reads its torrc file from there, which can make it start other programs."),
                      true);
                for (const auto& torrcKey : keysNamed(o, "torrc")) {
                    auto torrc = o.value(torrcKey).toObject();
                    bool torrcChanged = false;
                    for (const auto& key : torrc.keys()) {
                        if (!isTorOptionName(key)) {
                            report(torrcKey,
                                   QCoreApplication::translate("Validation", "Removed a Tor option with an invalid name from the imported configuration: option names use only letters, digits and underscores, like UseBridges."),
                                   key);
                        } else if (isTorPluginOption(key)) {
                            report(torrcKey + "." + key,
                                   QCoreApplication::translate("Validation", "Removed the Tor option %1 from the imported configuration: it makes Tor start a program on this computer.").arg(key),
                                   torrc.value(key).toString());
                        } else {
                            continue;
                        }
                        torrc.remove(key);
                        torrcChanged = true;
                    }
                    if (torrcChanged) o[torrcKey] = torrc;
                }
            } else if (type == "openconnect") {
                for (const auto& section : {QStringLiteral("csd"), QStringLiteral("hip"), QStringLiteral("tncc")}) {
                    for (const auto& sectionKey : keysNamed(o, section)) {
                        auto wrapper = o.value(sectionKey).toObject();
                        bool wrapperChanged = false;
                        for (const auto& key : keysNamed(wrapper, "wrapper_path")) {
                            if (isBlank(wrapper.value(key))) continue;
                            report(sectionKey + "." + key,
                                   QCoreApplication::translate("Validation", "Removed the %1 wrapper path from the imported configuration; add it back only if you trust that program.").arg(section.toUpper()),
                                   wrapper.value(key).toString());
                            wrapper.remove(key);
                            wrapperChanged = true;
                        }
                        if (wrapperChanged) o[sectionKey] = wrapper;
                    }
                }
            } else if (type == "tailscale") {
                for (const auto& key : keysNamed(o, "ssh_server")) {
                    const auto ssh = o.value(key);
                    const auto sshObject = ssh.toObject();
                    bool enabled = ssh.toBool();
                    for (const auto& enabledKey : keysNamed(sshObject, "enabled")) enabled = enabled || sshObject.value(enabledKey).toBool();
                    if (!enabled) continue;
                    report(key,
                           QCoreApplication::translate("Validation", "Removed the Tailscale SSH server from the imported configuration: it would let devices on that tailnet open a shell on this computer."));
                    o.remove(key);
                }
                strip("taildrop_directory",
                      QCoreApplication::translate("Validation", "Removed the Taildrop folder from the imported configuration: devices on that tailnet could place files there. Received files now go to the default folder."),
                      true);
            }
            return removed;
        }

        bool stripNamed(QJsonObject& o, const QString& name, const QString& at, const QString& message, bool showValue,
                        Validation::Issues& issues)
        {
            bool removed = false;
            for (const auto& key : keysNamed(o, name)) {
                if (isBlank(o.value(key))) continue;
                issues << Validation::Issue{Validation::Severity::Warning, at + key, message, showValue ? o.value(key).toString() : QString()};
                o.remove(key);
                removed = true;
            }
            return removed;
        }

        template <typename Edit>
        bool editObjectNamed(QJsonObject& o, const QString& name, const QString& at, Edit&& edit)
        {
            bool changed = false;
            for (const auto& key : keysNamed(o, name)) {
                auto inner = o.value(key).toObject();
                if (!edit(inner, at + key + QLatin1Char('.'))) continue;
                o[key] = inner;
                changed = true;
            }
            return changed;
        }

        template <typename Edit>
        bool editListNamed(QJsonObject& o, const QString& name, const QString& at, Edit&& edit)
        {
            bool changed = false;
            for (const auto& key : keysNamed(o, name)) {
                auto items = o.value(key).toArray();
                bool listChanged = false;
                for (qsizetype i = 0; i < items.size(); ++i) {
                    auto item = items.at(i).toObject();
                    if (!edit(item, QStringLiteral("%1%2[%3].").arg(at, key).arg(i))) continue;
                    items.replace(i, item);
                    listChanged = true;
                }
                if (!listChanged) continue;
                o[key] = items;
                changed = true;
            }
            return changed;
        }

        QString opensPortMessage()
        {
            return QCoreApplication::translate("Validation", "Removed from the imported configuration: it would open a port on this computer that other devices could reach.");
        }

        QString writesFileMessage()
        {
            return QCoreApplication::translate("Validation", "Removed from the imported configuration: it would write a file on this computer at a path the sender chose.");
        }

        QString xrayReverseMessage()
        {
            return QCoreApplication::translate("Validation", "Removed the reverse proxy from the imported configuration: it would let the server reach this computer and its network.");
        }

        // Every spelling must agree: the core may read any of them.
        bool isLocalSingBoxInbound(const QJsonObject& inbound)
        {
            static const QStringList localTypes{"mixed", "socks", "http", "direct", "redirect", "tproxy"};
            const auto typeKeys = keysNamed(inbound, "type");
            if (typeKeys.isEmpty()) return false;
            bool tun = true;
            bool local = true;
            for (const auto& key : typeKeys) {
                const auto t = inbound.value(key).toString();
                tun = tun && t == "tun";
                local = local && localTypes.contains(t);
            }
            if (tun) return true;
            if (!local) return false;
            // TLS can carry ACME, which answers challenges on public ports.
            for (const auto& key : keysNamed(inbound, "tls")) {
                if (!isBlank(inbound.value(key))) return false;
            }
            // The core listens on 127.0.0.1 when no address is given.
            for (const auto& key : keysNamed(inbound, "listen")) {
                const auto listen = inbound.value(key);
                if (!isBlank(listen) && !QHostAddress(listen.toString()).isLoopback()) return false;
            }
            return true;
        }

        // Removes what would open a port on this computer or write a file; Throne runs a full config as it is.
        bool stripSingBoxExposure(QJsonObject& root, Validation::Issues& issues)
        {
            bool removed = false;
            for (const auto& key : keysNamed(root, "inbounds")) {
                const auto items = root.value(key).toArray();
                QJsonArray kept;
                for (qsizetype i = 0; i < items.size(); ++i) {
                    const auto inbound = items.at(i).toObject();
                    if (isLocalSingBoxInbound(inbound)) {
                        kept << items.at(i);
                        continue;
                    }
                    issues << Validation::Issue{Validation::Severity::Warning, QStringLiteral("%1[%2]").arg(key).arg(i),
                                                QCoreApplication::translate("Validation", "Removed this inbound from the imported configuration: it could let other devices connect through this computer. Only tun inbounds and local proxies that listen on 127.0.0.1 or ::1 are kept."),
                                                customValueNamed(inbound, "type").toString()};
                }
                if (kept.size() == items.size()) continue;
                root[key] = kept;
                removed = true;
            }
            removed |= stripNamed(root, "services", {}, opensPortMessage(), false, issues);
            removed |= stripNamed(root, "certificate_providers", {}, opensPortMessage(), false, issues);
            removed |= editObjectNamed(root, "log", {}, [&](QJsonObject& log, const QString& at) {
                return stripNamed(log, "output", at, writesFileMessage(), true, issues);
            });
            removed |= editObjectNamed(root, "experimental", {}, [&](QJsonObject& experimental, const QString& at) {
                bool changed = stripNamed(experimental, "clash_api", at,
                                          QCoreApplication::translate("Validation", "Removed the Clash API from the imported configuration: web pages and programs on this computer could use it to control the core, and its external_ui setting downloads files."),
                                          false, issues);
                // The core builds the V2Ray API only when it listens.
                for (const auto& key : keysNamed(experimental, "v2ray_api")) {
                    const auto api = experimental.value(key).toObject();
                    bool listens = false;
                    for (const auto& listenKey : keysNamed(api, "listen")) listens = listens || !isBlank(api.value(listenKey));
                    if (!listens) continue;
                    issues << Validation::Issue{Validation::Severity::Warning, at + key,
                                                QCoreApplication::translate("Validation", "Removed the V2Ray API from the imported configuration: Throne's core is built without it and would not start, and it would open a statistics port on this computer."),
                                                {}};
                    experimental.remove(key);
                    changed = true;
                }
                changed |= editObjectNamed(experimental, "debug", at, [&](QJsonObject& debug, const QString& debugAt) {
                    return stripNamed(debug, "listen", debugAt, opensPortMessage(), true, issues);
                });
                changed |= editObjectNamed(experimental, "cache_file", at, [&](QJsonObject& cache, const QString& cacheAt) {
                    return stripNamed(cache, "path", cacheAt, writesFileMessage(), true, issues);
                });
                return changed;
            });
            removed |= editListNamed(root, "network_namespaces", {}, [&](QJsonObject& ns, const QString& at) {
                return stripNamed(ns, "pid_file", at, writesFileMessage(), true, issues);
            });
            return removed;
        }

        // Xray reads "none" as no file.
        bool stripXrayFilePath(QJsonObject& o, const QString& name, const QString& at, Validation::Issues& issues)
        {
            bool removed = false;
            for (const auto& key : keysNamed(o, name)) {
                const auto value = o.value(key);
                if (isBlank(value) || value.toString() == "none") continue;
                issues << Validation::Issue{Validation::Severity::Warning, at + key, writesFileMessage(), value.toString()};
                o.remove(key);
                removed = true;
            }
            return removed;
        }

        // XHTTP's download leg is a stream of its own, also reachable through "extra".
        bool stripXrayKeyLog(QJsonObject& stream, const QString& at, Validation::Issues& issues)
        {
            bool removed = false;
            for (const auto& name : {QStringLiteral("tlsSettings"), QStringLiteral("realitySettings")}) {
                removed |= editObjectNamed(stream, name, at, [&](QJsonObject& security, const QString& securityAt) {
                    return stripXrayFilePath(security, "masterKeyLog", securityAt, issues);
                });
            }
            const auto stripDownload = [&](QJsonObject& o, const QString& oAt) {
                return editObjectNamed(o, "downloadSettings", oAt, [&](QJsonObject& download, const QString& downloadAt) {
                    return stripXrayKeyLog(download, downloadAt, issues);
                });
            };
            for (const auto& name : {QStringLiteral("xhttpSettings"), QStringLiteral("splithttpSettings")}) {
                removed |= editObjectNamed(stream, name, at, [&](QJsonObject& xhttp, const QString& xhttpAt) {
                    bool changed = stripDownload(xhttp, xhttpAt);
                    changed |= editObjectNamed(xhttp, "extra", xhttpAt, stripDownload);
                    return changed;
                });
            }
            return removed;
        }

        bool stripXrayOutbound(QJsonObject& outbound, const QString& at, Validation::Issues& issues)
        {
            bool removed = editObjectNamed(outbound, "settings", at, [&](QJsonObject& settings, const QString& settingsAt) {
                return stripNamed(settings, "reverse", settingsAt, xrayReverseMessage(), false, issues);
            });
            removed |= editObjectNamed(outbound, "streamSettings", at, [&](QJsonObject& stream, const QString& streamAt) {
                return stripXrayKeyLog(stream, streamAt, issues);
            });
            return removed;
        }

        bool stripXrayExposure(QJsonObject& root, Validation::Issues& issues)
        {
            bool removed = stripNamed(root, "inbounds", {},
                                      QCoreApplication::translate("Validation", "Removed the inbounds from the imported configuration: Throne adds its own local inbound, and these could let other devices connect through this computer."),
                                      false, issues);
            removed |= stripNamed(root, "api", {}, opensPortMessage(), false, issues);
            removed |= stripNamed(root, "metrics", {}, opensPortMessage(), false, issues);
            removed |= stripNamed(root, "reverse", {}, xrayReverseMessage(), false, issues);
            // Xray runs inside the core process and applies "env" with os.Setenv.
            removed |= stripNamed(root, "env", {},
                                  QCoreApplication::translate("Validation", "Removed the environment variables from the imported configuration: they would apply to Throne's core and to the programs it starts."),
                                  false, issues);
            // The geodata updater swaps files in the asset folder, which is Throne's own folder.
            removed |= stripNamed(root, "geodata", {},
                                  QCoreApplication::translate("Validation", "Removed the geodata updater from the imported configuration: it would download files from addresses the sender chose and replace files in Throne's folder."),
                                  false, issues);
            removed |= editObjectNamed(root, "routing", {}, [&](QJsonObject& routing, const QString& at) {
                return editListNamed(routing, "rules", at, [&](QJsonObject& rule, const QString& ruleAt) {
                    return stripNamed(rule, "webhook", ruleAt,
                                      QCoreApplication::translate("Validation", "Removed the webhook from this routing rule: it would send the details of every matching connection to an address the sender chose."),
                                      false, issues);
                });
            });
            removed |= editObjectNamed(root, "log", {}, [&](QJsonObject& log, const QString& at) {
                bool changed = stripXrayFilePath(log, "access", at, issues);
                changed |= stripXrayFilePath(log, "error", at, issues);
                return changed;
            });
            removed |= editListNamed(root, "outbounds", {}, [&](QJsonObject& outbound, const QString& at) {
                return stripXrayOutbound(outbound, at, issues);
            });
            return removed;
        }

        // A // or /* outside a string; one inside (an https:// URL) is no comment.
        bool hasJsonComment(const QString& text)
        {
            bool inString = false;
            for (qsizetype i = 0; i < text.size(); ++i) {
                const auto c = text.at(i);
                if (inString) {
                    if (c == u'\\') ++i;
                    else if (c == u'"') inString = false;
                } else if (c == u'"') {
                    inString = true;
                } else if (c == u'/' && i + 1 < text.size() && (text.at(i + 1) == u'/' || text.at(i + 1) == u'*')) {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    bool StripXrayStreamKeyLog(QJsonObject& stream, const QString& at, Validation::Issues& issues)
    {
        return stripXrayKeyLog(stream, at, issues);
    }

    namespace Checks {
        QString CustomConfig(const QString& subtype, const QString& text)
        {
            if (auto message = Validation::JsonObject(text, Validation::Empty::Required); !message.isEmpty()) {
                if (!hasJsonComment(text)) return message;
                return message + QLatin1Char(' ') + QCoreApplication::translate("Validation", "Comments are not allowed in the JSON.");
            }
            const auto root = QJsonDocument::fromJson(text.toUtf8()).object();
            if (subtype == Custom::CustomOutbound && customValueNamed(root, "type").toString().trimmed().isEmpty()) {
                return QCoreApplication::translate("Validation", "Add the outbound type, for example \"type\": \"socks\".");
            }
            if (subtype == Custom::CustomXrayOutbound && customValueNamed(root, "protocol").toString().trimmed().isEmpty()) {
                return QCoreApplication::translate("Validation", "Add the protocol, for example \"protocol\": \"vless\".");
            }
            if (subtype == Custom::CustomFullConfig || subtype == Custom::CustomXrayFullConfig) {
                if (const auto outbounds = customValueNamed(root, "outbounds"); !outbounds.isUndefined() && !outbounds.isArray()) {
                    return QCoreApplication::translate("Validation", "\"outbounds\" must be a list, for example \"outbounds\": [].");
                }
            }
            return {};
        }
    }

    void Custom::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        const QStringList subtypes{CustomOutbound, CustomFullConfig, CustomXrayOutbound, CustomXrayFullConfig};
        if (!subtypes.contains(type)) {
            // OneOf trims, but Build() and the strip compare exactly.
            auto subtypeMessage = Validation::OneOf(type, subtypes, Validation::Empty::Required);
            if (subtypeMessage.isEmpty()) subtypeMessage = QCoreApplication::translate("Validation", "Must not contain spaces.");
            issues << Validation::Issue{Validation::Severity::Error, "subtype", subtypeMessage, type};
            return;
        }
        if (const auto message = Checks::CustomConfig(type, config); !message.isEmpty()) {
            issues << Validation::Issue{Validation::Severity::Error, "config", message, {}};
        }
        const auto root = QString2QJsonObject(config);
        if (type == CustomXrayOutbound) {
            Checks::XrayXHTTPDownloadLeg(issues, {}, root);
        } else if (type == CustomXrayFullConfig) {
            for (const auto& key : keysNamed(root, "outbounds")) {
                const auto items = root.value(key).toArray();
                for (qsizetype i = 0; i < items.size(); ++i) {
                    Checks::XrayXHTTPDownloadLeg(issues, QStringLiteral("%1[%2].").arg(key).arg(i), items.at(i).toObject());
                }
            }
        }
    }

    QString Custom::GetAddress()
    {
        if (type == CustomOutbound) return QString2QJsonObject(config)["server"].toString();
        // EffectiveSecurity's private-server check reads this, so it must be the destination Xray dials.
        if (type == CustomXrayOutbound) return xrayOutboundAddress(QString2QJsonObject(config));
        return {};
    }

    QString Custom::DisplayAddress()
    {
        if (type == CustomOutbound) {
            const auto obj = QString2QJsonObject(config);
            return ::DisplayAddress(obj["server"].toString(), obj["server_port"].toInt());
        }
        if (type == CustomXrayOutbound) {
            if (const auto peer = xrayOutboundPeer(QString2QJsonObject(config)); !peer.isEmpty()) {
                return ::DisplayAddress(xrayAgreed(peer, "address").toString(), xrayAgreed(peer, "port").toInt());
            }
        }
        return {};
    }

    SecurityInfo Custom::GetSecurity()
    {
        if (type == CustomOutbound) {
            return analyzeSingBoxOutbound(QString2QJsonObject(config));
        }
        if (type == CustomFullConfig) {
            const auto obj = QString2QJsonObject(config);
            const auto egress = resolveSingBoxEgress(obj["outbounds"].toArray(),
                                                     obj["route"].toObject()["final"].toString(), 5);
            return egress.isEmpty() ? SecurityInfo{} : analyzeSingBoxOutbound(egress);
        }
        if (type == CustomXrayOutbound) {
            return analyzeXrayOutbound(QString2QJsonObject(config));
        }
        if (type == CustomXrayFullConfig) {
            const auto obj = QString2QJsonObject(config);
            const auto egress = firstXrayEgress(obj["outbounds"].toArray());
            return egress.isEmpty() ? SecurityInfo{} : analyzeXrayOutbound(egress);
        }
        return {};
    }

    QJsonObject Custom::ExportIdentity()
    {
        QJsonObject ob;
        if (type == CustomOutbound) {
            ob = singBoxOutboundIdentity(QString2QJsonObject(config));
        } else if (type == CustomFullConfig) {
            const auto obj = QString2QJsonObject(config);
            const auto egress = resolveSingBoxEgress(obj["outbounds"].toArray(),
                                                     obj["route"].toObject()["final"].toString(), 5);
            if (!egress.isEmpty()) ob = singBoxOutboundIdentity(egress);
        } else if (type == CustomXrayOutbound) {
            ob = xrayOutboundIdentity(QString2QJsonObject(config));
        } else if (type == CustomXrayFullConfig) {
            const auto obj = QString2QJsonObject(config);
            const auto egress = firstXrayEgress(obj["outbounds"].toArray());
            if (!egress.isEmpty()) ob = xrayOutboundIdentity(egress);
        }
        if (ob.isEmpty()) return ExportToJson();
        return QJsonObject{{"subtype", type}, {"ob", ob}};
    }

    void Custom::StripForeignState(Validation::Issues& issues)
    {
        auto root = QString2QJsonObject(config);
        bool removed = false;
        if (type == CustomXrayOutbound) {
            removed = stripXrayOutbound(root, {}, issues);
        } else if (type == CustomXrayFullConfig) {
            removed = stripXrayExposure(root, issues);
        } else if (type == CustomFullConfig) {
            removed = stripSingBoxExposure(root, issues);
            for (const auto& list : {QStringLiteral("outbounds"), QStringLiteral("endpoints")}) {
                removed |= editListNamed(root, list, {}, [&](QJsonObject& item, const QString& at) { return stripLaunchers(item, at, issues); });
            }
        } else {
            // Build() hands every other subtype to the core as a sing-box outbound.
            removed = stripLaunchers(root, {}, issues);
        }
        if (removed) config = QJsonObject2QString(root, false);
    }
}
