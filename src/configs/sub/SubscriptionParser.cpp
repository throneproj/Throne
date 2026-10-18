#include "include/configs/sub/SubscriptionParser.hpp"

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/OutboundFactory.h"
#include "include/configs/common/utils.h"
#include "include/configs/sub/SubscriptionScan.hpp"
#include "include/configs/sub/clash.hpp"
#include "include/configs/sub/vpnFileImport.hpp"
#include "include/database/ProfilesRepo.h"
#include "include/global/Utils.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QtEndian>

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string_view>

namespace Subscription {
    namespace {
        using ProfilePtr = std::shared_ptr<Configs::Profile>;

        constexpr int kMaxDepth = 16;
        constexpr auto npos = std::string_view::npos;

        enum class SingBoxSubType {
            outboundInJson,
            outboundJsonArray,
            outboundObject,
            invalid,
        };

        enum class XraySubType {
            outboundInJson,
            outboundJsonArray,
            outboundObject,
            configJsonArray,
            invalid,
        };

        struct Protocol {
            const char *type;
            const char *schemes[4];
            const char *singbox[2];
            const char *clash[2];
        };

        constexpr Protocol kProtocols[] = {
            {"socks", {"socks5://", "socks4://", "socks4a://", "socks://"}, {"socks"}, {"socks5"}},
            {"http", {"http://", "https://"}, {"http"}, {"http"}},
            {"shadowsocks", {"ss://"}, {"shadowsocks"}, {"ss"}},
            {"vmess", {"vmess://"}, {"vmess"}, {"vmess"}},
            {"vless", {"vless://"}, {"vless"}, {"vless"}},
            {"trojan", {"trojan://"}, {"trojan"}, {"trojan"}},
            {"anytls", {"anytls://"}, {"anytls"}, {"anytls"}},
            {"mieru", {"mierus://", "mieru://"}, {"mieru"}, {}},
            {"snell", {"snell://"}, {"snell"}, {"snell"}},
            {"hysteria", {"hysteria://", "hysteria2://", "hy2://"}, {"hysteria", "hysteria2"}, {"hysteria", "hysteria2"}},
            {"tuic", {"tuic://"}, {"tuic"}, {"tuic"}},
            {"juicity", {"juicity://"}, {"juicity"}, {}},
            {"trusttunnel", {"tt://"}, {"trusttunnel"}, {}},
            {"shadowtls", {"shadowtls://"}, {"shadowtls"}, {}},
            {"wireguard", {"awg://", "wg://", "wireguard://"}, {"wireguard"}, {}},
            {"masque", {}, {"masque"}, {"masque"}},
            {"ssh", {"ssh://"}, {"ssh"}, {"ssh"}},
            {"naive", {"naive+https://", "naive+quic://"}, {"naive"}, {}},
        };

        const char *typeForScheme(std::string_view link) {
            for (const auto &p : kProtocols) {
                for (const char *scheme : p.schemes) {
                    if (scheme != nullptr && link.starts_with(scheme)) return p.type;
                }
            }
            return nullptr;
        }

        const char *typeForSingBox(const QString &type) {
            for (const auto &p : kProtocols) {
                for (const char *name : p.singbox) {
                    if (name != nullptr && type == QLatin1String(name)) return p.type;
                }
            }
            return nullptr;
        }

        const char *typeForClash(const std::string &type) {
            for (const auto &p : kProtocols) {
                for (const char *name : p.clash) {
                    if (name != nullptr && type == name) return p.type;
                }
            }
            return nullptr;
        }

        // The links' REALITY preference (useXrayVless), limited to the networks Xray runs REALITY over; sing-box also carries ws.
        bool clashRealityOnXray(const clash::Proxies &out) {
            if (!out.tls || out.reality_opts.public_key.empty()) return false;
            if (Configs::dataManager->settingsRepo->xray_vless_preference == Configs::Xray::XhttpOnly) return false;
            return out.network.empty() || out.network == "tcp" || out.network == "grpc";
        }

        // sing-box VLESS rejects the vision-udp443 flow; Xray accepts it.
        bool clashUsesXrayVless(const clash::Proxies &out) {
            const auto flow = QString::fromStdString(out.flow).trimmed().toLower();
            const bool singBoxFlow = flow.isEmpty() || flow == "none" || flow == "xtls-rprx-vision";
            return out.type == "vless" && (out.network == "xhttp" || (!out.encryption.empty() && out.encryption != "none")
                                           || !singBoxFlow || clashRealityOnXray(out));
        }

        // Non-proxy entries every full config carries; skipping them is not worth a report.
        bool isSingBoxInfraType(const QString &type) {
            return type == "direct" || type == "block" || type == "dns" || type == "selector" || type == "urltest";
        }

        bool isClashInfraType(const std::string &type) {
            static const QSet<QString> infra = {"direct", "reject", "reject-drop", "dns", "pass", "compatible",
                                                "relay", "select", "url-test", "fallback", "load-balance"};
            return infra.contains(QString::fromStdString(type).toLower());
        }

        bool isXrayInfraProtocol(const QString &protocol) {
            return protocol == "freedom" || protocol == "direct" || protocol == "blackhole" || protocol == "block"
                   || protocol == "dns" || protocol == "loopback";
        }

        Validation::Issue parserIssue(Validation::Severity severity, const QString &message, const QString &field = {}) {
            return {severity, field, message, {}};
        }

        Validation::Issue parserError(const QString &message, const QString &field = {}) {
            return parserIssue(Validation::Severity::Error, message, field);
        }

        Validation::Issue parserWarning(const QString &message, const QString &field = {}) {
            return parserIssue(Validation::Severity::Warning, message, field);
        }

        QString parserElide(const QString &text) {
            constexpr qsizetype maxLength = 64;
            const auto trimmed = text.trimmed();
            return trimmed.size() > maxLength ? trimmed.left(maxLength) + QChar(0x2026) : trimmed;
        }

        // INI sections ("[Interface]", "[General]") also open with '['; JSON arrays hold values, not bare words.
        bool looksLikeJson(std::string_view text) {
            if (text.front() == '{') return true;
            const auto rest = scan::trim(text.substr(1));
            if (rest.empty()) return false;
            const char c = rest.front();
            return c == '{' || c == '[' || c == '"' || c == ']' || c == '-' || std::isdigit(static_cast<unsigned char>(c));
        }

        // "ssr" for "ssr://...": the RFC 3986 scheme grammar, so plain text and "#key: value" metadata never qualify.
        std::string_view linkScheme(std::string_view line) {
            const auto sep = line.find("://");
            if (sep == npos || sep == 0 || !std::isalpha(static_cast<unsigned char>(line.front()))) return {};
            for (std::size_t i = 1; i < sep; ++i) {
                const auto c = static_cast<unsigned char>(line[i]);
                if (!std::isalnum(c) && c != '+' && c != '.' && c != '-') return {};
            }
            return line.substr(0, sep);
        }

        // Never the raw link: "scheme://host:port #name", the address taken from the parsed outbound when it has one.
        QString linkSource(const QString &link, const Configs::outbound *ob) {
            const auto sep = link.indexOf(QStringLiteral("://"));
            QString label = sep > 0 ? link.left(sep).toLower() + QStringLiteral("://") : QString();
            QString host = ob != nullptr ? ob->server.trimmed() : QString();
            int port = ob != nullptr ? ob->server_port : 0;
            QString name = ob != nullptr ? ob->name : QString();
            if (host.isEmpty() || name.trimmed().isEmpty()) {
                const QUrl url(link);
                // Base64 bodies (vmess://, legacy ss://) parse as a host; only a host:port authority is an address.
                if (host.isEmpty() && url.port() > 0 && !url.host().isEmpty()) {
                    host = url.host();
                    port = url.port();
                }
                if (name.trimmed().isEmpty()) name = url.fragment(QUrl::FullyDecoded);
            }
            if (!host.isEmpty()) {
                label += host.contains(':') ? '[' + host + ']' : host;
                if (port > 0) label += ':' + QString::number(port);
            }
            if (const auto shown = parserElide(name); !shown.isEmpty()) label += QStringLiteral(" #") + shown;
            return label;
        }

        QString singBoxSource(bool endpoint, const QString &tag, int index) {
            const auto name = parserElide(tag);
            if (endpoint) {
                return name.isEmpty() ? QCoreApplication::translate("Validation", "sing-box endpoint #%1").arg(index)
                                      : QCoreApplication::translate("Validation", "sing-box endpoint \"%1\"").arg(name);
            }
            return name.isEmpty() ? QCoreApplication::translate("Validation", "sing-box outbound #%1").arg(index)
                                  : QCoreApplication::translate("Validation", "sing-box outbound \"%1\"").arg(name);
        }

        QString xraySource(const QString &tag, int index) {
            const auto name = parserElide(tag);
            return name.isEmpty() ? QCoreApplication::translate("Validation", "Xray outbound #%1").arg(index)
                                  : QCoreApplication::translate("Validation", "Xray outbound \"%1\"").arg(name);
        }

        QString xrayConfigSource(const QString &remarks, int index) {
            const auto name = parserElide(remarks);
            return name.isEmpty() ? QCoreApplication::translate("Validation", "Xray config #%1").arg(index)
                                  : QCoreApplication::translate("Validation", "Xray config \"%1\"").arg(name);
        }

        QString clashSource(const QString &proxyName, int index) {
            const auto name = parserElide(proxyName);
            return name.isEmpty() ? QCoreApplication::translate("Validation", "Clash proxy #%1").arg(index)
                                  : QCoreApplication::translate("Validation", "Clash proxy \"%1\"").arg(name);
        }

        QString namedFileSource(const QString &kind, const QString &profileName) {
            const auto name = parserElide(profileName);
            return name.isEmpty() ? kind : QStringLiteral("%1 \"%2\"").arg(kind, name);
        }

        // The importers' notes; when the import failed, the last note is the reason.
        void appendImporterNotes(Validation::Issues &issues, const QStringList &notes, bool ok) {
            for (qsizetype i = 0; i < notes.size(); ++i) {
                const bool fatal = !ok && i == notes.size() - 1;
                issues << parserIssue(fatal ? Validation::Severity::Error : Validation::Severity::Warning, notes[i]);
            }
        }

        QString toQString(std::string_view s) {
            return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size())).trimmed();
        }

        // Shared by document() and vpnLink() so both agree on what counts as a WireGuard conf.
        bool hasWireGuardSections(std::string_view text) {
            return text.find("[Interface]") != npos && text.find("[Peer]") != npos;
        }

        // Filters dividers and boilerplate headers ("# WireGuard configuration", "# Peer 1") so only a real node name can become a profile name.
        bool looksLikeName(const QString &comment) {
            static const QRegularExpression separatorRe(R"([\s_\-.:]+)", QRegularExpression::UseUnicodePropertiesOption);
            static const QSet<QString> generic = {"peer", "server", "wireguard", "configuration",
                                                  "config", "interface", "client", "wg"};
            if (!std::any_of(comment.begin(), comment.end(), [](QChar c) { return c.isLetterOrNumber(); }))
                return false;
            for (auto w : comment.toLower().split(separatorRe, Qt::SkipEmptyParts)) {
                while (!w.isEmpty() && w.back().isDigit()) w.chop(1);
                if (w.isEmpty()) continue;
                if (!generic.contains(w)) return true;
            }
            return false;
        }

        struct WireGuardNames {
            QString explicitName;
            QString peerComment;
        };

        // A comment names the peer only when it sits directly above [Peer]; any blank or config line in between drops it, so [Interface] comments cannot leak in.
        WireGuardNames extractWireGuardNames(std::string_view text) {
            static const QRegularExpression explicitRe(R"(^(?:name|remarks?)\s*[:=]\s*(.+)$)",
                                                       QRegularExpression::CaseInsensitiveOption);
            static const QRegularExpression markersRe(R"(^[#;\s]+)", QRegularExpression::UseUnicodePropertiesOption);
            static const QRegularExpression directiveRe(R"(^\w+\s*=)");
            WireGuardNames out;
            QString lastComment;

            scan::forEachLine(text, [&](std::string_view raw) {
                const auto line = scan::trim(raw);
                const bool isComment = !line.empty() && (line.front() == '#' || line.front() == ';');
                if (!isComment) {
                    // Assigned on every [Peer]: the name comes from the last peer, the one ParseFromLink keeps for the endpoint.
                    if (!line.empty() && scan::startsWithNoCase(line, "[peer]")) out.peerComment = lastComment;
                    lastComment.clear();
                    return true;
                }

                QString comment = QString::fromUtf8(line.data(), static_cast<qsizetype>(line.size()));
                comment.remove(markersRe);
                comment = comment.trimmed();

                if (out.explicitName.isEmpty()) {
                    if (const auto m = explicitRe.match(comment); m.hasMatch()) {
                        out.explicitName = m.captured(1).trimmed();
                    }
                }

                // Commented-out options such as "# DNS = 1.1.1.1" are not names.
                lastComment = (!directiveRe.match(comment).hasMatch() && looksLikeName(comment)) ? comment : QString();
                return true;
            });

            return out;
        }

        // Candidates are in priority order; a name ParseFromLink already read from a #fragment is never overwritten.
        void setNameIfEmpty(Configs::Profile &ent, std::initializer_list<QString> candidates) {
            if (!ent.outbound->name.trimmed().isEmpty()) return;
            for (const auto &c : candidates) {
                const auto t = c.trimmed();
                if (!t.isEmpty()) {
                    ent.outbound->name = t;
                    return;
                }
            }
        }

        SingBoxSubType getSingBoxSubType(const QJsonDocument &doc) {
            if (doc.isObject()) {
                const auto obj = doc.object();
                if (obj.contains("outbounds") || obj.contains("endpoints")) return SingBoxSubType::outboundInJson;
                if (obj.contains("type")) return SingBoxSubType::outboundObject;
                return SingBoxSubType::invalid;
            }
            if (doc.isArray() && !doc.array().empty()) {
                const auto firstRaw = doc.array().first();
                if (firstRaw.isObject() && firstRaw.toObject().contains("type")) return SingBoxSubType::outboundJsonArray;
            }
            return SingBoxSubType::invalid;
        }

        // Xray tags outbounds with "protocol" where sing-box uses "type".
        XraySubType getXraySubType(const QJsonDocument &doc) {
            if (doc.isObject()) {
                const auto obj = doc.object();
                if (obj.contains("outbounds")) {
                    for (const auto &item : obj["outbounds"].toArray()) {
                        if (item.isObject() && item.toObject().contains("protocol")) return XraySubType::outboundInJson;
                    }
                }
                if (obj.contains("protocol")) return XraySubType::outboundObject;
                return XraySubType::invalid;
            }
            if (doc.isArray() && !doc.array().empty()) {
                const auto first = doc.array().first();
                if (first.isObject()) {
                    const auto obj = first.toObject();
                    if (obj.contains("protocol")) return XraySubType::outboundJsonArray;
                    if (obj.contains("outbounds")) {
                        for (const auto &item : obj["outbounds"].toArray()) {
                            if (item.isObject() && item.toObject().contains("protocol")) return XraySubType::configJsonArray;
                        }
                    }
                }
            }
            return XraySubType::invalid;
        }

        // Real Xray VLESS nests the server under settings.vnext[0]; ParseFromJson wants it flat.
        QJsonObject normalizeXrayVlessForParse(const QJsonObject &out, Validation::Issues &notes) {
            if (out["protocol"].toString() != "vless") return {};
            const auto settings = out["settings"].toObject();
            if (settings.contains("address") && !settings.contains("vnext")) return out;
            const auto vnext = settings["vnext"].toArray();
            if (vnext.isEmpty()) return {};
            const auto first = vnext.first().toObject();
            if (first.isEmpty()) return {};
            const auto users = first["users"].toArray();
            if (users.isEmpty()) return {};
            if (vnext.size() > 1) {
                notes << parserWarning(QCoreApplication::translate("Validation", "Only the first server of %n was imported.", nullptr,
                                                                   static_cast<int>(vnext.size())), "vnext");
            }
            if (users.size() > 1) {
                notes << parserWarning(QCoreApplication::translate("Validation", "Only the first user of %n was imported.", nullptr,
                                                                   static_cast<int>(users.size())), "users");
            }
            const auto user = users.first().toObject();
            QJsonObject simpleSettings;
            simpleSettings["address"] = first["address"];
            simpleSettings["port"] = first["port"];
            simpleSettings["id"] = user["id"];
            simpleSettings["encryption"] = user.contains("encryption") ? user["encryption"] : QJsonValue("none");
            simpleSettings["flow"] = user["flow"];
            QJsonObject normalized = out;
            normalized["settings"] = simpleSettings;
            return normalized;
        }

        enum class XrayValue { Flag, Text, TextOrList, Object };

        struct XrayKey {
            const char *name;
            XrayValue value;
            // The VLESS model reads this exact spelling; any other spelling is still read by Xray (Go's encoding/json folds case).
            bool modelled;
            // Losing the value weakens the connection's protection.
            bool protects;
        };

        struct XrayUnmodelledKey {
            QString path;
            // The spelling the VLESS model reads; empty when it cannot store the key at all.
            QString spelling;
            bool protects;
        };

        // A value of the wrong type makes Xray's decoder refuse the whole outbound, so it does not count.
        bool xrayValueSet(const QJsonValue &value, XrayValue kind) {
            const bool text = value.isString() && !value.toString().trimmed().isEmpty();
            switch (kind) {
            case XrayValue::Flag: return value.isBool() && value.toBool();
            case XrayValue::Text: return text;
            case XrayValue::TextOrList: return text || (value.isArray() && !value.toArray().isEmpty());
            case XrayValue::Object: return value.isObject() && !value.toObject().isEmpty();
            }
            return false;
        }

        void xrayCollectUnmodelled(const QJsonObject &object, const QString &at, std::initializer_list<XrayKey> keys,
                                   QList<XrayUnmodelledKey> &found) {
            for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
                for (const auto &key : keys) {
                    const QLatin1String name(key.name);
                    if (it.key().compare(name, Qt::CaseInsensitive) != 0) continue;
                    if ((!key.modelled || it.key() != name) && xrayValueSet(it.value(), key.value)) {
                        found.append(XrayUnmodelledKey{at + it.key(), key.modelled ? QString(name) : QString(), key.protects});
                    }
                    break;
                }
            }
        }

        // Set security keys Xray reads that the typed VLESS model would drop.
        QList<XrayUnmodelledKey> xrayUnmodelledKeys(const QJsonObject &out) {
            using V = XrayValue;
            QList<XrayUnmodelledKey> found;
            xrayCollectUnmodelled(out, {}, {{"streamSettings", V::Object, true, true}}, found);
            const auto stream = out["streamSettings"].toObject();
            const QString at = QStringLiteral("streamSettings.");
            xrayCollectUnmodelled(stream, at, {{"security", V::Text, true, true}}, found);
            const auto security = stream["security"].toString().trimmed().toLower();
            // Xray reads only the block its security selects, which a security spelled in another case makes unknown.
            bool securityCased = false;
            for (auto it = stream.constBegin(); it != stream.constEnd(); ++it) {
                if (it.key() == QLatin1String("security") || it.key().compare(QLatin1String("security"), Qt::CaseInsensitive) != 0) continue;
                securityCased = securityCased || xrayValueSet(it.value(), V::Text);
            }
            if (securityCased || security == "tls") xrayCollectUnmodelled(stream, at, {{"tlsSettings", V::Object, true, true}}, found);
            if (securityCased || security == "reality") xrayCollectUnmodelled(stream, at, {{"realitySettings", V::Object, true, true}}, found);
            if (security == "tls") {
                xrayCollectUnmodelled(stream["tlsSettings"].toObject(), at + QStringLiteral("tlsSettings."), {
                    {"serverName", V::Text, true, false}, {"pinnedPeerCertSha256", V::Text, true, true},
                    {"verifyPeerCertByName", V::Text, true, true}, {"alpn", V::TextOrList, true, false},
                    {"fingerprint", V::Text, true, false}, {"cipherSuites", V::Text, true, false},
                    {"echConfigList", V::Text, true, true}, {"allowInsecure", V::Flag, false, false},
                    {"minVersion", V::Text, false, false}, {"maxVersion", V::Text, false, false},
                    {"curvePreferences", V::TextOrList, false, false}}, found);
            } else if (security == "reality") {
                xrayCollectUnmodelled(stream["realitySettings"].toObject(), at + QStringLiteral("realitySettings."), {
                    {"serverName", V::Text, true, false}, {"fingerprint", V::Text, true, false},
                    {"password", V::Text, true, true}, {"publicKey", V::Text, true, true}, {"shortId", V::Text, true, true},
                    {"spiderX", V::Text, true, false}, {"mldsa65Verify", V::Text, true, true}}, found);
            }
            return found;
        }

        Validation::Issue xrayUnmodelledIssue(const XrayUnmodelledKey &key) {
            if (key.spelling.isEmpty()) {
                return parserWarning(QCoreApplication::translate("Validation", "A VLESS profile cannot store this setting, so it was ignored. Such an outbound is kept as written only when nothing else in it needs fixing."), key.path);
            }
            if (key.protects) {
                return parserError(QCoreApplication::translate("Validation", "Rename this key to \"%1\": Throne reads only that spelling, and dropping its value would weaken the connection's security.").arg(key.spelling), key.path);
            }
            return parserWarning(QCoreApplication::translate("Validation", "Rename this key to \"%1\": Throne reads only that spelling, so its value was ignored.").arg(key.spelling), key.path);
        }

        // Values the typed parse fixes without a note; a verbatim copy would hand Xray the originals, which it refuses.
        bool xrayTypedRepaired(const QJsonObject &out, const Configs::xrayVless &typed) {
            const auto differs = [](const QJsonObject &object, const QString &key, const QString &kept) {
                return object.value(key).toString() != kept;
            };
            const auto settings = out["settings"].toObject();
            const auto user = settings.contains("vnext")
                ? settings["vnext"].toArray().first().toObject()["users"].toArray().first().toObject() : settings;
            if (differs(user, "flow", typed.flow) || differs(user, "encryption", typed.encryption)) return true;
            const auto stream = out["streamSettings"].toObject();
            const auto &s = *typed.streamSetting;
            if (stream.contains("security") && stream["security"].toString().toLower() != s.security) return true;
            // The typed profile keeps neither key in any spelling, while Xray reads every spelling of sockopt.
            for (auto s = stream.constBegin(); s != stream.constEnd(); ++s) {
                if (s.key().compare(QLatin1String("sockopt"), Qt::CaseInsensitive) != 0) continue;
                const auto sockopt = s.value().toObject();
                for (auto it = sockopt.constBegin(); it != sockopt.constEnd(); ++it) {
                    if (it.key().compare(QLatin1String("dialerProxy"), Qt::CaseInsensitive) == 0
                        || it.key().compare(QLatin1String("domainStrategy"), Qt::CaseInsensitive) == 0) {
                        return true;
                    }
                }
            }
            // StripForeignState drops them from the download leg too.
            for (auto leg : Configs::XrayDownloadLeg(out)) {
                if (Configs::StripXrayDialerState(leg)) return true;
            }
            if (s.security == "tls") return differs(stream["tlsSettings"].toObject(), "fingerprint", s.TLS->fingerprint);
            if (s.security != "reality") return false;
            const auto reality = stream["realitySettings"].toObject();
            const auto password = reality["password"].toString();
            // Xray takes any non-empty password over publicKey, so a blank one fails there.
            if (!password.isEmpty() && password.trimmed().isEmpty()) return true;
            const auto keyField = password.trimmed().isEmpty() ? QStringLiteral("publicKey") : QStringLiteral("password");
            // xrayReality::Build drops "unsafe", which Xray refuses for REALITY.
            return s.reality->fingerprint.toLower() == Configs::tlsUnsafeFingerprint || differs(reality, "fingerprint", s.reality->fingerprint)
                   || differs(reality, keyField, s.reality->password) || differs(reality, "shortId", s.reality->shortId)
                   || differs(reality, "mldsa65Verify", s.reality->mldsa65Verify);
        }

        bool looksLikeOvpnConfig(std::string_view text) {
            bool hasRemote = false;
            bool hasClientMarker = false;
            bool result = false;
            scan::forEachLine(text, [&](std::string_view raw) {
                const auto line = scan::trim(raw);
                if (line.empty() || line.front() == '#' || line.front() == ';') return true;
                if (line.starts_with("remote ") || line == "<ca>" || line == "<tls-auth>" || line == "<tls-crypt>" ||
                    line == "<tls-crypt-v2>" || line == "<secret>") {
                    hasRemote = true;
                }
                if (line == "client" || line == "tls-client" || line.starts_with("dev ") || line.starts_with("dev-type ") ||
                    line.starts_with("proto ")) {
                    hasClientMarker = true;
                }
                result = hasRemote && hasClientMarker;
                return !result;
            });
            return result;
        }

        bool looksLikeOpenConnectProfile(std::string_view text) {
            if (text.find("<AnyConnectProfile") != npos || text.find("<ServerList") != npos) return true;
            static const QRegularExpression cliRe(R"((?:^|\s)--protocol[= ](?:anyconnect|nc|gp|pulse|f5|fortinet)\b)");
            static const QRegularExpression fileRe(R"(^[ \t]*protocol[ \t]*=[ \t]*(?:anyconnect|nc|gp|pulse|f5|fortinet)[ \t]*$)",
                                                   QRegularExpression::MultilineOption);
            bool matched = false;
            if (text.find("protocol") != npos) {
                scan::forEachLine(text, [&](std::string_view raw) {
                    if (raw.find("protocol") == npos) return true;
                    const auto line = QString::fromUtf8(raw.data(), static_cast<qsizetype>(raw.size()));
                    matched = cliRe.match(line).hasMatch() || fileRe.match(line).hasMatch();
                    return !matched;
                });
            }
            if (matched) return true;
            bool result = false;
            scan::forEachLine(text, [&](std::string_view raw) {
                const auto line = scan::trim(raw);
                if (line.empty() || line.front() == '#') return true;
                result = line.starts_with("openconnect ");
                return false;
            });
            return result;
        }

        // qUncompress allocates whatever size the 4-byte header claims before inflating, so validate the zlib header and cap it first.
        QByteArray uncompressVpnPayload(const QByteArray &data) {
            constexpr quint32 maxSize = 16 * 1024 * 1024;
            if (data.size() < 6) return {};
            const auto bytes = reinterpret_cast<const uchar *>(data.constData());
            const quint32 expected = qFromBigEndian<quint32>(bytes);
            const uint cmf = bytes[4], flg = bytes[5];
            if (expected == 0 || expected > maxSize || (cmf & 0x0F) != 8 || ((cmf << 8) | flg) % 31 != 0) return {};
            return qUncompress(data);
        }

        // A NUL in the first four bytes makes fkYAML decode as UTF-16/32 and read out of bounds (#1746).
        std::string sanitizeClashYaml(std::string_view text) {
            QString normalized = QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
            normalized.remove(QChar(QChar::Null));
            if (normalized.startsWith(QChar(QChar::ByteOrderMark))) normalized.remove(0, 1);
            return normalized.toStdString();
        }

        // Diagnostics only: a hostile entry can make even the name lookup throw.
        QString clashEntryName(const fkyaml::node &node) {
            try {
                if (!node.is_mapping() || !node.contains("name") || !node["name"].is_string()) return {};
                return parserElide(QString::fromStdString(node["name"].get_value<std::string>()));
            } catch (...) {
                return {};
            }
        }

        class Parser {
        public:
            explicit Parser(const ParseSink &sink) : sink(sink) {}

            void document(std::string_view raw, bool allowBase64, bool needParse, int depth, const QString &overrideName = QString());

        private:
            const ParseSink &sink;
            int produced = 0;
            int reported = 0;

            void produce(ProfilePtr ent) {
                if (ent == nullptr) return;
                ++produced;
                if (sink.profile) sink.profile(std::move(ent));
            }

            // `source` is a callable: labels are only built for items that have something to report.
            template <typename Source>
            void report(Source &&source, bool imported, Validation::Issues issues) {
                if (!sink.report) return;
                ++reported;
                sink.report(ParseReport{source(), imported, std::move(issues)});
            }

            // About the whole document rather than one profile: its errors become the import's reasons.
            void reportDocument(const QString &source, Validation::Issues issues) {
                if (!sink.report) return;
                ++reported;
                sink.report(ParseReport{source, false, std::move(issues), true});
            }

            // The scope covers the parse only: it must be closed before any sink callback runs.
            template <typename Parse>
            static bool scoped(Parse &&parse, Validation::Issues &issues) {
                Configs::ParseDiag::Scope scope;
                const bool ok = parse();
                issues << scope.take();
                return ok;
            }

            // Import checks after a parse; an Error refuses the profile, warnings travel with it.
            template <typename Source>
            void accept(const ProfilePtr &ent, bool parsed, Validation::Issues issues, Source &&source) {
                bool imported = false;
                if (!parsed) {
                    if (!Validation::HasError(issues)) issues << parserError(QCoreApplication::translate("Validation", "Could not be parsed."));
                } else {
                    ent->outbound->StripForeignState(issues);
                    ent->outbound->Validate(issues);
                    imported = !Validation::HasError(issues);
                }
                if (!imported || !issues.isEmpty()) report(source, imported, std::move(issues));
                if (imported) produce(ent);
            }

            void documentBody(std::string_view text, bool needParse, int depth, const QString &overrideName);
            void reportJsonSyntax(std::string_view text, const QJsonParseError &error);
            void json(const QJsonDocument &doc, std::string_view text);
            void singBox(const QJsonDocument &doc, SingBoxSubType type);
            void xray(const QJsonDocument &doc, XraySubType type);
            void xrayOutbound(const QJsonObject &out, int index);
            void sip008(const QJsonDocument &doc);
            void clash(std::string_view text);
            void clashProxy(const fkyaml::node &node, int index);
            void wireguardFile(std::string_view text, const QString &overrideName);
            void openVpnFile(std::string_view text);
            void openConnectProfile(std::string_view text);
            void link(std::string_view line, int depth, const QString &overrideName);
            void jsonLink(const QString &str, bool throneAdd);
            void vpnLink(const QString &str, int depth);
        };

        void Parser::document(std::string_view raw, bool allowBase64, bool needParse, int depth, const QString &overrideName) {
            if (depth > kMaxDepth) return;
            const auto text = scan::trim(raw);
            if (text.empty()) return;

            if (allowBase64 && scan::looksLikeBase64(text)) {
                if (const auto decoded = scan::decodeBase64(text); !decoded.isEmpty()) {
                    document(scan::view(decoded), false, true, depth + 1, overrideName);
                    return;
                }
            }

            if (text.front() == '{' || text.front() == '[') {
                QJsonParseError error{};
                const auto doc = QJsonDocument::fromJson(QByteArray::fromRawData(text.data(), static_cast<qsizetype>(text.size())), &error);
                if (error.error == QJsonParseError::NoError) {
                    json(doc, text);
                    return;
                }
                // Whole documents only: the per-line split of a broken one would repeat the error for every brace line.
                if (needParse && looksLikeJson(text)) {
                    const int before = produced;
                    const int reportedBefore = reported;
                    documentBody(text, needParse, depth, overrideName);
                    if (produced == before && reported == reportedBefore) reportJsonSyntax(text, error);
                    return;
                }
            }

            documentBody(text, needParse, depth, overrideName);
        }

        void Parser::documentBody(std::string_view text, bool needParse, int depth, const QString &overrideName) {
            if (text.find("proxies:") != npos) {
                clash(text);
                return;
            }

            if (hasWireGuardSections(text)) {
                wireguardFile(text, overrideName);
                return;
            }

            if (looksLikeOvpnConfig(text)) {
                openVpnFile(text);
                return;
            }

            if (looksLikeOpenConnectProfile(text)) {
                openConnectProfile(text);
                return;
            }

            if (needParse && text.find('\n') != npos) {
                scan::forEachItem(text, [&](std::string_view item) { document(item, true, false, depth + 1, overrideName); });
                return;
            }

            link(text, depth, overrideName);
        }

        void Parser::reportJsonSyntax(std::string_view text, const QJsonParseError &error) {
            const auto offset = static_cast<std::size_t>(std::clamp<qsizetype>(error.offset, 0, static_cast<qsizetype>(text.size())));
            int line = 1;
            std::size_t lineStart = 0;
            for (std::size_t i = 0; i < offset; ++i) {
                if (text[i] != '\n') continue;
                ++line;
                lineStart = i + 1;
            }
            const auto message = QCoreApplication::translate("Validation", "Not valid JSON: %1 at line %2, column %3.")
                                     .arg(error.errorString(), QString::number(line), QString::number(offset - lineStart + 1));
            reportDocument({}, {parserError(message)});
        }

        void Parser::json(const QJsonDocument &doc, std::string_view text) {
            // Xray first: its configs share the "outbounds" wrapper with sing-box.
            const auto xrayType = getXraySubType(doc);
            if (xrayType == XraySubType::outboundObject) {
                xrayOutbound(doc.object(), 1);
                return;
            }
            if (xrayType != XraySubType::invalid) {
                xray(doc, xrayType);
                return;
            }

            const auto subType = getSingBoxSubType(doc);
            if (subType == SingBoxSubType::outboundObject) {
                auto ent = Configs::ProfilesRepo::NewProfile("custom");
                ent->Custom()->type = Configs::Custom::CustomOutbound;
                ent->Custom()->config = toQString(text);
                const auto tag = doc.object()["tag"].toString();
                accept(ent, true, {}, [&] { return singBoxSource(false, tag, 1); });
                return;
            }
            if (subType != SingBoxSubType::invalid) {
                singBox(doc, subType);
                return;
            }

            if (text.find("version") != npos && text.find("servers") != npos) sip008(doc);
        }

        void Parser::singBox(const QJsonDocument &doc, SingBoxSubType type) {
            QJsonArray outbounds, endpoints;
            if (type == SingBoxSubType::outboundInJson) {
                const auto json = doc.object();
                outbounds = json["outbounds"].toArray();
                endpoints = json["endpoints"].toArray();
            } else if (type == SingBoxSubType::outboundJsonArray) {
                outbounds = doc.array();
            } else {
                return;
            }

            const auto handle = [&](const QJsonValue &value, bool endpoint, int index) {
                if (!value.isObject()) return;
                const auto out = value.toObject();
                const auto source = [&] { return singBoxSource(endpoint, out["tag"].toString(), index); };
                if (out.isEmpty()) {
                    report(source, false, {parserWarning(QCoreApplication::translate("Validation", "Empty object."))});
                    return;
                }
                const auto type = out["type"].toString();
                const char *profileType = typeForSingBox(type);
                if (profileType == nullptr) {
                    if (isSingBoxInfraType(type)) return;
                    report(source, false, {parserWarning(type.isEmpty()
                        ? QCoreApplication::translate("Validation", "Missing \"type\".")
                        : QCoreApplication::translate("Validation", "Unsupported sing-box type \"%1\".").arg(type))});
                    return;
                }
                auto ent = Configs::ProfilesRepo::NewProfile(profileType);
                Validation::Issues issues;
                const bool parsed = scoped([&] { return ent->outbound->ParseFromJson(out); }, issues);
                accept(ent, parsed, std::move(issues), source);
            };
            int index = 0;
            for (const auto &outbound : outbounds) handle(outbound, false, ++index);
            index = 0;
            for (const auto &endpoint : endpoints) handle(endpoint, true, ++index);
        }

        void Parser::xray(const QJsonDocument &doc, XraySubType type) {
            // Each element is a self-contained config (balancers, dialerProxy chains) that must run verbatim.
            if (type == XraySubType::configJsonArray) {
                int index = 0;
                for (const auto &c : doc.array()) {
                    ++index;
                    if (!c.isObject()) continue;
                    auto cfg = c.toObject();
                    if (!cfg.contains("outbounds")) continue;
                    // Throne injects its own bridge inbound; the bundled ones only risk port-bind conflicts.
                    cfg.remove("inbounds");
                    auto ent = Configs::ProfilesRepo::NewProfile("custom");
                    ent->Custom()->type = Configs::Custom::CustomXrayFullConfig;
                    ent->Custom()->config = QJsonObject2QString(cfg, false);
                    const auto remarks = cfg["remarks"].toString();
                    if (!remarks.isEmpty()) ent->Custom()->name = remarks;
                    accept(ent, true, {}, [&] { return xrayConfigSource(remarks, index); });
                }
                return;
            }

            QJsonArray outbounds;
            if (type == XraySubType::outboundInJson) {
                outbounds = doc.object()["outbounds"].toArray();
            } else if (type == XraySubType::outboundJsonArray) {
                outbounds = doc.array();
            } else {
                return;
            }
            int index = 0;
            for (const auto &o : outbounds) {
                ++index;
                if (!o.isObject()) continue;
                xrayOutbound(o.toObject(), index);
            }
        }

        void Parser::xrayOutbound(const QJsonObject &out, int index) {
            if (out.isEmpty() || isXrayInfraProtocol(out["protocol"].toString())) return;
            const auto source = [&] { return xraySource(out["tag"].toString(), index); };
            Validation::Issues verbatimIssues;
            if (out["protocol"].toString() == "vless") {
                Validation::Issues issues;
                if (const auto normalized = normalizeXrayVlessForParse(out, issues); !normalized.isEmpty()) {
                    auto ent = Configs::ProfilesRepo::NewProfile("xrayvless");
                    if (scoped([&] { return ent->XrayVLESS()->ParseFromJson(normalized); }, issues)) {
                        const auto unmodelled = xrayUnmodelledKeys(out);
                        Validation::Issues checked;
                        if (!unmodelled.isEmpty()) ent->outbound->Validate(checked);
                        // A verbatim copy keeps the original values, so it fits only when Xray accepts them and the typed parse fixed nothing.
                        if (unmodelled.isEmpty() || Validation::HasError(checked) || !issues.isEmpty()
                            || xrayTypedRepaired(out, *ent->XrayVLESS())) {
                            for (const auto &key : unmodelled) issues << xrayUnmodelledIssue(key);
                            accept(ent, true, std::move(issues), source);
                            return;
                        }
                        QStringList paths;
                        for (const auto &key : unmodelled) paths << key.path;
                        verbatimIssues << parserWarning(QCoreApplication::translate("Validation", "Imported as a custom Xray outbound to keep %1, which a VLESS profile would drop.")
                                                            .arg(paths.join(QStringLiteral(", "))));
                    }
                }
            }
            // Kept verbatim, so nothing was lost; the notes of a failed typed parse no longer apply.
            auto ent = Configs::ProfilesRepo::NewProfile("custom");
            ent->Custom()->type = Configs::Custom::CustomXrayOutbound;
            ent->Custom()->config = QJsonObject2QString(out, false);
            if (const auto tag = out["tag"].toString(); !tag.isEmpty()) ent->Custom()->name = tag;
            accept(ent, true, std::move(verbatimIssues), source);
        }

        void Parser::sip008(const QJsonDocument &doc) {
            int index = 0;
            for (const auto &o : doc.object()["servers"].toArray()) {
                ++index;
                const auto source = [&] { return QCoreApplication::translate("Validation", "SIP008 server #%1").arg(index); };
                const auto out = o.toObject();
                if (out.isEmpty()) {
                    report(source, false, {parserWarning(QCoreApplication::translate("Validation", "Empty server object."))});
                    continue;
                }
                auto ent = Configs::ProfilesRepo::NewProfile("shadowsocks");
                Validation::Issues issues;
                const bool parsed = scoped([&] { return ent->ShadowSocks()->ParseFromSIP008(out); }, issues);
                accept(ent, parsed, std::move(issues), source);
            }
        }

        void Parser::clash(std::string_view text) {
            try {
                const fkyaml::node root = fkyaml::node::deserialize(sanitizeClashYaml(text));
                const bool mapping = root.is_mapping();
                if (!mapping || !root.contains("proxies") || !root["proxies"].is_sequence() || root["proxies"].empty()) {
                    const auto message = mapping && root.contains("proxy-providers")
                        ? QCoreApplication::translate("Validation", "This Clash config only lists proxy-providers, which are not downloaded. Add each provider's URL as a subscription instead.")
                        : QCoreApplication::translate("Validation", "This Clash config has no proxies list.");
                    reportDocument({}, {parserError(message)});
                    return;
                }

                int index = 0;
                for (const auto &node : root["proxies"]) {
                    ++index;
                    QString reason;
                    try {
                        clashProxy(node, index);
                        continue;
                    } catch (const std::exception &ex) {
                        reason = QCoreApplication::translate("Validation", "Could not be read: %1").arg(QString::fromUtf8(ex.what()));
                    } catch (...) {
                        reason = QCoreApplication::translate("Validation", "Could not be read.");
                    }
                    report([&] { return clashSource(clashEntryName(node), index); }, false, {parserError(reason)});
                }
            // fkYAML can throw beyond fkyaml::exception on hostile input (bad_alloc, length_error).
            } catch (const std::exception &ex) {
                reportDocument({}, {parserError(QCoreApplication::translate("Validation", "Could not read the Clash config: %1.")
                                                    .arg(QString::fromUtf8(ex.what())))});
            } catch (...) {
                reportDocument({}, {parserError(QCoreApplication::translate("Validation", "Could not read the Clash config."))});
            }
        }

        // One entry at a time: clash::Proxies is several KB even when empty.
        void Parser::clashProxy(const fkyaml::node &node, int index) {
            ProfilePtr ent;
            QString name;
            QString unsupported;
            bool infra = false;
            bool parsed = false;
            Validation::Issues issues;
            {
                Configs::ParseDiag::Scope scope;
                const auto out = node.get_value<clash::Proxies>();
                name = QString::fromStdString(out.name);
                if (const char *profileType = typeForClash(out.type); profileType == nullptr) {
                    infra = isClashInfraType(out.type);
                    unsupported = QString::fromStdString(out.type);
                } else {
                    ent = Configs::ProfilesRepo::NewProfile(clashUsesXrayVless(out) ? "xrayvless" : profileType);
                    parsed = ent->outbound->ParseFromClash(out);
                }
                issues = scope.take();
            }
            const auto source = [&] { return clashSource(name, index); };
            if (ent == nullptr) {
                if (infra) return;
                issues << parserWarning(unsupported.isEmpty()
                    ? QCoreApplication::translate("Validation", "Missing \"type\".")
                    : QCoreApplication::translate("Validation", "Unsupported Clash type \"%1\".").arg(unsupported));
                report(source, false, std::move(issues));
                return;
            }
            accept(ent, parsed, std::move(issues), source);
        }

        void Parser::wireguardFile(std::string_view text, const QString &overrideName) {
            auto ent = Configs::ProfilesRepo::NewProfile("wireguard");
            Validation::Issues issues;
            const bool parsed = scoped([&] { return ent->Wireguard()->ParseFromLink(toQString(text)); }, issues);
            if (parsed) {
                const auto names = extractWireGuardNames(text);
                setNameIfEmpty(*ent, {overrideName, names.explicitName, names.peerComment});
            } else if (!Validation::HasError(issues)) {
                if (ent->Wireguard()->private_key.trimmed().isEmpty()) {
                    issues << parserError(QCoreApplication::translate("Validation", "The [Interface] section has no PrivateKey."), "PrivateKey");
                }
                if (ent->Wireguard()->peer->public_key.trimmed().isEmpty()) {
                    issues << parserError(QCoreApplication::translate("Validation", "The [Peer] section has no PublicKey."), "PublicKey");
                }
            }
            accept(ent, parsed, std::move(issues), [&] {
                return namedFileSource(QCoreApplication::translate("Validation", "WireGuard config"),
                                       parsed ? ent->outbound->name : overrideName);
            });
        }

        void Parser::openVpnFile(std::string_view text) {
            QStringList problems;
            auto ent = Configs::ProfilesRepo::NewProfile("openvpn");
            Validation::Issues issues;
            const bool ok = scoped([&] { return Configs::ParseOvpnConfig(toQString(text), *ent->OpenVPN(), &problems); }, issues);
            appendImporterNotes(issues, problems, ok);
            accept(ent, ok, std::move(issues), [&] {
                return namedFileSource(QCoreApplication::translate("Validation", "OpenVPN config"), ent->outbound->name);
            });
        }

        void Parser::openConnectProfile(std::string_view text) {
            QStringList problems;
            const auto str = toQString(text);
            const auto kind = [] { return QCoreApplication::translate("Validation", "OpenConnect profile"); };
            if (text.find("<AnyConnectProfile") != npos || text.find("<ServerList") != npos) {
                QList<std::shared_ptr<Configs::openconnect>> hosts;
                Validation::Issues issues;
                const bool ok = scoped([&] { return Configs::ParseAnyConnectXml(str, hosts, &problems); }, issues);
                appendImporterNotes(issues, problems, ok);
                if (!ok) {
                    if (!Validation::HasError(issues)) issues << parserError(QCoreApplication::translate("Validation", "Could not be parsed."));
                    reportDocument(kind(), std::move(issues));
                    return;
                }
                if (!issues.isEmpty()) reportDocument(kind(), std::move(issues));
                for (const auto &host : hosts) {
                    auto ent = Configs::ProfilesRepo::NewProfile("openconnect");
                    ent->outbound = host;
                    accept(ent, true, {}, [&] { return namedFileSource(kind(), host->name); });
                }
                return;
            }

            auto ent = Configs::ProfilesRepo::NewProfile("openconnect");
            Validation::Issues issues;
            const bool ok = scoped([&] { return Configs::ParseOpenConnectProfile(str, *ent->OpenConnect(), &problems); }, issues);
            appendImporterNotes(issues, problems, ok);
            accept(ent, ok, std::move(issues), [&] { return namedFileSource(kind(), ent->outbound->name); });
        }

        void Parser::link(std::string_view line, int depth, const QString &overrideName) {
            if (line.starts_with("//") || line.starts_with("#") || line.size() < 2) return;

            if (line.starts_with("json://")) {
                jsonLink(toQString(line), false);
                return;
            }
            if (scan::startsWithNoCase(line, "throne://add/")) {
                jsonLink(toQString(line), true);
                return;
            }
            if (scan::startsWithNoCase(line, "vpn://")) {
                vpnLink(toQString(line), depth);
                return;
            }

            const char *profileType = typeForScheme(line);
            if (profileType == nullptr) {
                if (const auto scheme = linkScheme(line); !scheme.empty() && sink.report) {
                    const auto written = toQString(scheme);
                    const auto lower = written.toLower();
                    const bool known = lower == "json" || typeForScheme((lower + "://").toStdString()) != nullptr;
                    const auto message = known
                        ? QCoreApplication::translate("Validation", "Write the link type in lower case: \"%1://\", not \"%2://\".").arg(lower, written)
                        : QCoreApplication::translate("Validation", "Unsupported link type \"%1://\".").arg(written);
                    report([&] { return linkSource(toQString(line), nullptr); }, false, {parserWarning(message)});
                }
                return;
            }
            const auto str = toQString(line);
            ProfilePtr ent;
            if (line.starts_with("vless://") && Configs::useXrayVless(str)) {
                ent = Configs::ProfilesRepo::NewProfile("xrayvless");
            } else {
                ent = Configs::ProfilesRepo::NewProfile(profileType);
            }
            Validation::Issues issues;
            const bool parsed = scoped([&] { return ent->outbound->ParseFromLink(str); }, issues);

            if (parsed && std::string_view(profileType) == "wireguard") setNameIfEmpty(*ent, {overrideName});

            accept(ent, parsed, std::move(issues), [&] { return linkSource(str, ent->outbound.get()); });
        }

        void Parser::jsonLink(const QString &str, bool throneAdd) {
            QString type;
            QString name;
            const auto source = [&] {
                QString label = throneAdd ? QStringLiteral("throne://add/") : QStringLiteral("json://");
                label += type;
                if (const auto shown = parserElide(name); !shown.isEmpty()) label += QStringLiteral(" #") + shown;
                return label;
            };
            const auto damaged = [&](const QString &message) { report(source, false, {parserError(message)}); };

            const QUrl link(str);
            if (!link.isValid()) {
                damaged(QCoreApplication::translate("Validation", "The link is damaged: it is not a valid URL."));
                return;
            }
            auto dataBytes = throneAdd ? DecodeB64IfValid(link.path().mid(1))
                                       : DecodeB64IfValid(link.fragment().toUtf8(), QByteArray::Base64UrlEncoding);
            // ExportJsonLink emits the url-safe alphabet, which the standard decoder rejects whenever '-' or '_' occurs.
            if (dataBytes.isEmpty() && throneAdd) dataBytes = DecodeB64IfValid(link.path().mid(1), QByteArray::Base64UrlEncoding);
            if (dataBytes.isEmpty()) {
                damaged(QCoreApplication::translate("Validation", "The link is damaged: its payload is not valid base64."));
                return;
            }
            const auto data = QJsonDocument::fromJson(dataBytes).object();
            if (data.isEmpty()) {
                damaged(QCoreApplication::translate("Validation", "The link is damaged: its payload is not a JSON object."));
                return;
            }
            name = data["tag"].toString();
            ProfilePtr ent;
            if (data.contains("protocol")) {
                type = "xray" + data["protocol"].toString();
                ent = Configs::ProfilesRepo::NewProfile(type);
            } else {
                type = data["type"].toString();
                if (const auto refusal = Configs::RefusedForeignType(type); !refusal.isEmpty()) {
                    damaged(refusal);
                    return;
                }
                ent = Configs::ProfilesRepo::NewProfile(type == "hysteria2" ? QStringLiteral("hysteria") : type);
            }
            if (ent->outbound->invalid) {
                damaged(type.isEmpty() ? QCoreApplication::translate("Validation", "Missing \"type\".")
                                       : QCoreApplication::translate("Validation", "Unsupported profile type \"%1\".").arg(type));
                return;
            }
            Validation::Issues issues;
            const bool parsed = scoped([&] { return ent->outbound->ParseFromJson(data); }, issues);
            accept(ent, parsed, std::move(issues), source);
        }

        void Parser::vpnLink(const QString &str, int depth) {
            auto raw = str.mid(6);
            QString fragmentName;
            if (const auto frag = raw.indexOf('#'); frag != -1) {
                fragmentName = QUrl::fromPercentEncoding(raw.mid(frag + 1).toUtf8()).trimmed();
                raw = raw.left(frag);
            }
            const auto source = [&] { return namedFileSource(QStringLiteral("vpn://"), fragmentName); };
            raw = QUrl::fromPercentEncoding(raw.toUtf8());
            auto dataBytes = DecodeB64IfValid(raw);
            if (dataBytes.isEmpty()) dataBytes = DecodeB64IfValid(raw, QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
            if (dataBytes.isEmpty()) {
                report(source, false, {parserError(QCoreApplication::translate("Validation", "The vpn:// link could not be decoded."))});
                return;
            }
            if (const auto uncompressed = uncompressVpnPayload(dataBytes); !uncompressed.isEmpty()) dataBytes = uncompressed;

            const int before = produced;
            const int reportedBefore = reported;
            const auto doc = QJsonDocument::fromJson(dataBytes);
            if (doc.isObject() && doc.object().contains("containers")) {
                const auto root = doc.object();
                QString jsonName = root["description"].toString().trimmed();
                if (jsonName.isEmpty()) jsonName = root["name"].toString().trimmed();
                const QString targetName = !fragmentName.isEmpty() ? fragmentName : jsonName;

                struct ConfigEntry {
                    QByteArray bytes;
                    QString containerType;
                    bool isWireGuard;
                };
                QList<ConfigEntry> entries;
                int wgCount = 0;

                for (const auto &cVal : root["containers"].toArray()) {
                    if (!cVal.isObject()) continue;
                    const auto cObj = cVal.toObject();
                    const QString containerType = cObj["container"].toString().trimmed();

                    for (const auto &key : cObj.keys()) {
                        if (!cObj[key].isObject()) continue;
                        const auto protoObj = cObj[key].toObject();
                        QString conf;
                        if (protoObj["last_config"].isString()) {
                            const auto lc = protoObj["last_config"].toString();
                            const auto innerDoc = QJsonDocument::fromJson(lc.toUtf8());
                            if (innerDoc.isObject() && innerDoc.object().contains("config")) {
                                conf = innerDoc.object()["config"].toString();
                            } else {
                                conf = lc;
                            }
                        } else if (protoObj["last_config"].isObject()) {
                            conf = protoObj["last_config"].toObject()["config"].toString();
                        }
                        if (conf.isEmpty()) continue;

                        auto bytes = conf.toUtf8();
                        const bool isWg = hasWireGuardSections(scan::view(bytes));
                        wgCount += isWg;
                        entries.push_back(ConfigEntry{std::move(bytes), containerType, isWg});
                    }
                }

                for (const auto &e : entries) {
                    // Several WireGuard confs in one export would share a single name, so tag each with its container type.
                    QString name = targetName;
                    if (e.isWireGuard && wgCount > 1 && !name.isEmpty() && !e.containerType.isEmpty()) {
                        name += " (" + e.containerType + ")";
                    }
                    document(scan::view(e.bytes), false, true, depth + 1, name);
                }
            } else {
                document(scan::view(dataBytes), false, true, depth + 1, fragmentName);
            }
            // Inner items that failed already said why.
            if (produced == before && reported == reportedBefore) {
                report(source, false, {parserError(QCoreApplication::translate("Validation", "No importable profile was found in the vpn:// link."))});
            }
        }
    }

    void ParseDocument(QByteArray body, const ParseSink &sink) {
        scan::trimInPlace(body);
        if (body.isEmpty()) return;

        if (scan::looksLikeBase64(scan::view(body))) {
            if (auto decoded = scan::decodeBase64(scan::view(body)); !decoded.isEmpty()) body = std::move(decoded);
        } else if (scan::looksLikeWrappedBase64(scan::view(body))) {
            if (auto decoded = QByteArray::fromBase64(body); !decoded.isEmpty()) body = std::move(decoded);
        }

        Parser parser(sink);
        parser.document(scan::view(body), false, true, 0);
    }

    void ParseText(const QString &text, const ParseSink &sink) {
        ParseDocument(text.toUtf8(), sink);
    }
}
