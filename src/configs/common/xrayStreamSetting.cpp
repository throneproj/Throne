#include "include/configs/common/xrayStreamSetting.h"
#include "include/configs/common/TLS.h"
#include <QCoreApplication>
#include <QHash>
#include <QUrlQuery>
#include <QJsonArray>
#include <QJsonDocument>
#include "include/configs/common/utils.h"
#include "include/configs/ParseDiagnostics.h"
#include "include/database/entities/RouteProfile.h"
#include "include/global/Utils.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <utility>

namespace Configs {
    namespace {
        // Xray infra/conf ParseRangeString plus ensureOrder; false where Xray refuses the text.
        bool xrayStreamRange(const QString& text, qint64& from, qint64& to)
        {
            const auto t = text.trimmed();
            from = to = 0;
            if (t.isEmpty()) return true;
            if (!Validation::IntOrRange(t, INT_MIN, INT_MAX, Validation::Empty::Allowed, true).isEmpty()) return false;
            const auto dash = t.indexOf('-', t.startsWith('-') ? 1 : 0);
            from = Validation::ToInt(dash < 0 ? t : t.left(dash)).value_or(0);
            to = Validation::ToInt(dash < 0 ? t : t.mid(dash + 1)).value_or(0);
            if (from > to) std::swap(from, to);
            return true;
        }

        // Xray transport/internet/tls/tls.go: PresetFingerprints, ModernFingerprints and OtherFingerprints, matched lower-cased.
        const QStringList& xrayStreamFingerprints()
        {
            static const QStringList names = {
                "chrome", "firefox", "safari", "ios", "android", "edge", "360", "qq", "random", "randomized", "randomizednoalpn", "unsafe",
                "hellofirefox_120", "hellofirefox_148", "hellochrome_120", "hellochrome_131", "hellochrome_133", "helloios_13",
                "helloios_14", "helloedge_106", "hellosafari_26_3", "hello360_11_0", "helloqq_11_1",
                "hellogolang", "hellorandomized", "hellorandomizedalpn", "hellorandomizednoalpn", "hellofirefox_auto",
                "hellofirefox_55", "hellofirefox_56", "hellofirefox_63", "hellofirefox_65", "hellofirefox_99", "hellofirefox_102",
                "hellofirefox_105", "hellochrome_auto", "hellochrome_58", "hellochrome_62", "hellochrome_70", "hellochrome_72",
                "hellochrome_83", "hellochrome_87", "hellochrome_96", "hellochrome_100", "hellochrome_102", "hellochrome_106_shuffle",
                "helloios_auto", "helloios_11_1", "helloios_12_1", "helloandroid_11_okhttp", "helloedge_85", "helloedge_auto",
                "hellosafari_16_0", "hellosafari_auto", "hello360_auto", "hello360_7_5", "helloqq_auto", "hellochrome_100_psk",
                "hellochrome_112_psk_shuf", "hellochrome_114_padding_psk_shuf", "hellochrome_115_pq", "hellochrome_115_pq_psk",
                "hellochrome_120_pq",
            };
            return names;
        }

        // Unknown names are dropped with a note so Xray's default applies instead of a start failure.
        QString xrayStreamKnownFingerprint(const QString& raw, bool reality, const QString& field)
        {
            const auto fingerprint = raw.trimmed();
            return AcceptOptional(field, Checks::XrayFingerprint(fingerprint, reality), fingerprint) ? fingerprint : QString();
        }

        void xrayStreamAdd(Validation::Issues& issues, Validation::Severity severity, const QString& field, const QString& message,
                           const QString& value = {})
        {
            if (!message.isEmpty()) issues.append({severity, field, message, value});
        }

        // Every key Xray's case-folding decoder reads as name.
        QStringList xrayStreamKeysNamed(const QJsonObject& o, const QString& name)
        {
            QStringList keys;
            for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
                if (it.key().compare(name, Qt::CaseInsensitive) == 0) keys << it.key();
            }
            return keys;
        }

        bool xrayStreamRemoveNamed(QJsonObject& o, const QString& name)
        {
            const auto keys = xrayStreamKeysNamed(o, name);
            for (const auto& key : keys) o.remove(key);
            return !keys.isEmpty();
        }

        // Applies edit to every object spelling of name in o; true when any changed.
        template <typename Edit>
        bool xrayStreamEditNamed(QJsonObject& o, const QString& name, Edit&& edit)
        {
            bool changed = false;
            for (const auto& key : xrayStreamKeysNamed(o, name)) {
                if (!o.value(key).isObject()) continue;
                auto inner = o.value(key).toObject();
                if (!edit(inner)) continue;
                o[key] = inner;
                changed = true;
            }
            return changed;
        }

        // dialerProxy names an outbound of the source config, and domainStrategy would outrank ThroneWiring's resolver; Xray folds case.
        bool xrayStreamStripDialerKeys(QJsonObject& sockopt)
        {
            bool removed = xrayStreamRemoveNamed(sockopt, QStringLiteral("dialerProxy"));
            removed |= xrayStreamRemoveNamed(sockopt, QStringLiteral("domainStrategy"));
            return removed;
        }

        // An object inside a local copy, with its path for messages ("" or ending in '.').
        struct XrayStreamNode {
            QJsonObject* object;
            QString at;
        };

        QList<XrayStreamNode> xrayStreamRoot(QJsonObject& object, const QString& at = {})
        {
            return {XrayStreamNode{&object, at}};
        }

        // Go merges every spelling of a pointer field, holders in turn, into one struct and a null discards the earlier ones: drop those, edit the rest.
        template <typename Edit>
        void xrayStreamEditLive(const QList<XrayStreamNode>& holders, const QString& name, bool create, Edit&& edit)
        {
            QList<std::pair<XrayStreamNode, QString>> spellings;
            for (const auto& holder : holders) {
                for (const auto& key : xrayStreamKeysNamed(*holder.object, name)) spellings.append({holder, key});
            }
            qsizetype discarded = 0;
            for (qsizetype i = 0; i < spellings.size(); ++i) {
                if (spellings[i].first.object->value(spellings[i].second).isNull()) discarded = i + 1;
            }
            for (qsizetype i = 0; i < discarded; ++i) spellings[i].first.object->remove(spellings[i].second);
            spellings.remove(0, discarded);
            // Any other non-object makes Xray refuse the config.
            spellings.removeIf([](const std::pair<XrayStreamNode, QString>& s) { return !s.first.object->value(s.second).isObject(); });
            if (spellings.isEmpty()) {
                if (!create || holders.isEmpty()) return;
                holders.last().object->insert(name, QJsonObject());
                spellings.append({holders.last(), name});
            }
            QList<QJsonObject> objects;
            for (const auto& spelling : spellings) objects << spelling.first.object->value(spelling.second).toObject();
            QList<XrayStreamNode> live;
            for (qsizetype i = 0; i < spellings.size(); ++i) {
                live.append({&objects[i], spellings[i].first.at + spellings[i].second + QLatin1Char('.')});
            }
            edit(std::as_const(live));
            for (qsizetype i = 0; i < spellings.size(); ++i) spellings[i].first.object->insert(spellings[i].second, objects[i]);
        }

        // The value Go keeps for a field across spellings: the last one, a null resetting only a pointer field; Undefined when unset.
        QJsonValue xrayStreamLast(const QList<XrayStreamNode>& nodes, const QString& name, bool pointer, QString* at = nullptr)
        {
            QJsonValue result(QJsonValue::Undefined);
            for (const auto& node : nodes) {
                for (const auto& key : xrayStreamKeysNamed(*node.object, name)) {
                    const auto value = node.object->value(key);
                    if (value.isNull() && !pointer) continue;
                    result = value;
                    if (at != nullptr) *at = node.at + key;
                }
            }
            return result;
        }

        // A set method outranks network, and neither means tcp; only the XHTTP dialer reads downloadSettings.
        bool xrayStreamDialsXHTTP(const QList<XrayStreamNode>& streams)
        {
            auto transport = xrayStreamLast(streams, QStringLiteral("method"), true);
            if (transport.isNull() || transport.isUndefined()) transport = xrayStreamLast(streams, QStringLiteral("network"), true);
            const auto name = XrayLower(transport.toString());
            return name == "xhttp" || name == "splithttp";
        }

        // xhttpSettings outranks splithttpSettings, and extra is a json.RawMessage: its last spelling, even a null one, replaces the rest.
        template <typename Edit>
        void xrayStreamEditDownloadLeg(const QList<XrayStreamNode>& streams, Edit&& edit)
        {
            const auto fromHolders = [&](const QList<XrayStreamNode>& holders) {
                const XrayStreamNode* extraHolder = nullptr;
                QString extraKey;
                for (const auto& holder : holders) {
                    for (const auto& key : xrayStreamKeysNamed(*holder.object, QStringLiteral("extra"))) {
                        extraHolder = &holder;
                        extraKey = key;
                    }
                }
                if (extraHolder == nullptr) {
                    xrayStreamEditLive(holders, QStringLiteral("downloadSettings"), false, edit);
                    return;
                }
                if (!extraHolder->object->value(extraKey).isObject()) return;
                auto extra = extraHolder->object->value(extraKey).toObject();
                xrayStreamEditLive(xrayStreamRoot(extra, extraHolder->at + extraKey + QLatin1Char('.')), QStringLiteral("downloadSettings"), false, edit);
                extraHolder->object->insert(extraKey, extra);
            };
            bool xhttp = false;
            xrayStreamEditLive(streams, QStringLiteral("xhttpSettings"), false, [&](const QList<XrayStreamNode>& holders) {
                xhttp = true;
                fromHolders(holders);
            });
            if (!xhttp) xrayStreamEditLive(streams, QStringLiteral("splithttpSettings"), false, fromHolders);
        }

        // Every spelling loses the key and it is written once, into the last live sockopt, so Go's merge keeps it.
        void xrayStreamSetSockopt(const QList<XrayStreamNode>& streams, const QString& key, const QString& value)
        {
            xrayStreamEditLive(streams, QStringLiteral("sockopt"), true, [&](const QList<XrayStreamNode>& sockopts) {
                for (const auto& sockopt : sockopts) xrayStreamRemoveNamed(*sockopt.object, key);
                sockopts.last().object->insert(key, value);
            });
        }

        // Xray's XHTTP dialer dereferences the download leg's destination and casts its transport settings unchecked: either crashes the core.
        void xrayStreamDownloadEndpoint(Validation::Issues& issues, const QList<XrayStreamNode>& leg)
        {
            const auto at = leg.last().at;
            QString addressAt = at + "address", portAt = at + "port", methodAt, networkAt = at + "network";
            const auto address = xrayStreamLast(leg, QStringLiteral("address"), true, &addressAt);
            const auto port = xrayStreamLast(leg, QStringLiteral("port"), false, &portAt);
            const auto method = xrayStreamLast(leg, QStringLiteral("method"), true, &methodAt);
            const auto network = xrayStreamLast(leg, QStringLiteral("network"), true, &networkAt);
            const bool byMethod = !method.isNull() && !method.isUndefined();
            const auto transport = byMethod ? method : network;
            const auto transportName = transport.isString() ? XrayLower(transport.toString())
                                     : transport.isNull() || transport.isUndefined() ? QStringLiteral("tcp") : QString();
            if (transportName != "xhttp" && transportName != "splithttp") {
                xrayStreamAdd(issues, Validation::Severity::Error, byMethod ? methodAt : networkAt,
                              QCoreApplication::translate("Validation", "The download leg must use XHTTP too: write \"network\": \"xhttp\"."),
                              transport.toString());
            }
            if (!address.isString() || address.toString().trimmed().isEmpty()) {
                xrayStreamAdd(issues, Validation::Severity::Error, addressAt,
                              QCoreApplication::translate("Validation", "Add the download server's address, for example \"address\": \"dl.example.com\"."));
            }
            const auto number = port.toDouble();
            if (!port.isDouble() || number != std::floor(number) || number < 1 || number > 65535) {
                xrayStreamAdd(issues, Validation::Severity::Error, portAt,
                              QCoreApplication::translate("Validation", "Must be a port number from 1 to 65535, written without quotes, for example \"port\": 443."));
            }
        }

        QString qs(const std::string& value) {
            return QString::fromStdString(value);
        }

        bool hasText(const std::string& value) {
            return !value.empty();
        }

        QStringList splitList(const QString& value) {
            QStringList result;
            for (const auto& part : value.split(',', Qt::SkipEmptyParts)) {
                if (const auto trimmed = part.trimmed(); !trimmed.isEmpty()) result << trimmed;
            }
            return result;
        }

        void addXmuxField(QJsonObject& xmux, const QString& key, const std::string& value) {
            if (hasText(value)) xmux[key] = qs(value);
        }

        void addXPaddingFields(QJsonObject& extra, bool obfsMode, const std::string& key,
                               const std::string& header, const std::string& placement,
                               const std::string& method) {
            if (obfsMode) extra["xPaddingObfsMode"] = true;
            if (hasText(key)) extra["xPaddingKey"] = qs(key);
            if (hasText(header)) extra["xPaddingHeader"] = qs(header);
            if (hasText(placement)) extra["xPaddingPlacement"] = qs(placement);
            if (hasText(method)) extra["xPaddingMethod"] = qs(method);
        }

        QJsonObject buildXmuxObject(const clash::xhttpReuseSettings& reuse) {
            QJsonObject xmux;
            addXmuxField(xmux, "maxConcurrency", reuse.max_concurrency);
            addXmuxField(xmux, "maxConnections", reuse.max_connections);
            addXmuxField(xmux, "cMaxReuseTimes", reuse.c_max_reuse_times);
            addXmuxField(xmux, "hMaxRequestTimes", reuse.h_max_request_times);
            addXmuxField(xmux, "hMaxReusableSecs", reuse.h_max_reusable_secs);
            if (hasText(reuse.h_keep_alive_period)) {
                xmux["hKeepAlivePeriod"] = qs(reuse.h_keep_alive_period).toLongLong();
            }
            return xmux;
        }

        // mihomo takes every unset download-settings value from the proxy itself.
        QJsonObject buildDownloadSettingsObject(const clash::Proxies& proxy) {
            const auto& settings = proxy.xhttp_opts.download_settings;
            QJsonObject object;
            if (const auto server = toAceHost(qs(settings.server.value_or(proxy.server)).trimmed()); !server.isEmpty()) object["address"] = server;
            object["port"] = int(settings.port.value_or(proxy.port));
            object["network"] = "xhttp";

            const bool uploadReality = proxy.tls && !proxy.reality_opts.public_key.empty();
            // The upload leg uses its REALITY options only with tls on.
            const auto downloadReality = settings.reality_opts.value_or(proxy.tls ? proxy.reality_opts : clash::realityOpts{});
            const auto serverName = qs(settings.servername.value_or(proxy.servername.empty() ? proxy.sni : proxy.servername)).trimmed();
            const auto clientFingerprint = settings.client_fingerprint.value_or(proxy.client_fingerprint);
            // Unknown names are dropped as on the upload leg, which has already reported an inherited one it refuses too.
            const auto knownFingerprint = [&](bool reality) {
                const auto fingerprint = qs(clientFingerprint).trimmed();
                const auto message = Checks::XrayFingerprint(fingerprint, reality);
                if (message.isEmpty()) return fingerprint;
                if (settings.client_fingerprint || !proxy.tls || Checks::XrayFingerprint(fingerprint, uploadReality).isEmpty()) {
                    AcceptOptional(settings.client_fingerprint ? "xhttp-opts.download-settings.client-fingerprint" : "client-fingerprint", message, fingerprint);
                }
                return QString();
            };

            if (!downloadReality.public_key.empty()) {
                object["security"] = "reality";
                QJsonObject reality;
                reality["show"] = false;
                if (!serverName.isEmpty()) reality["serverName"] = toAceHost(serverName);
                // REALITY needs uTLS, so mihomo's "none" and Xray's "unsafe" leave Xray's default, as xrayReality::Build does.
                if (!IsClashNoFingerprint(clientFingerprint)) {
                    if (const auto fingerprint = knownFingerprint(true); !fingerprint.isEmpty() && fingerprint.toLower() != tlsUnsafeFingerprint) {
                        reality["fingerprint"] = fingerprint;
                    }
                }
                const QString at = QStringLiteral("xhttp-opts.download-settings.reality-opts.");
                QString publicKey;
                if (settings.reality_opts) {
                    publicKey = ImportRealityPublicKey(qs(downloadReality.public_key), at + "public-key");
                    if (const auto message = Checks::RealityPublicKey(publicKey, true); !message.isEmpty()) ParseDiag::Error(at + "public-key", message);
                    if (const auto message = Checks::RealityShortId(qs(downloadReality.short_id).trimmed()); !message.isEmpty()) ParseDiag::Error(at + "short-id", message);
                } else {
                    // The upload leg has already reported and checked the key it lends.
                    ParseDiag::Scope reported;
                    publicKey = ImportRealityPublicKey(qs(downloadReality.public_key), at + "public-key");
                }
                reality["publicKey"] = publicKey;
                if (const auto shortId = qs(downloadReality.short_id).trimmed(); !shortId.isEmpty()) reality["shortId"] = shortId;
                object["realitySettings"] = reality;
            } else if (settings.tls.value_or(proxy.tls)) {
                object["security"] = "tls";
                QJsonObject tls;
                if (!serverName.isEmpty()) tls["serverName"] = toAceHost(serverName);
                QJsonArray alpn;
                for (const auto& item : settings.alpn.value_or(proxy.alpn)) {
                    if (const auto protocol = qs(item).trimmed(); !protocol.isEmpty()) alpn.append(protocol);
                }
                if (!alpn.isEmpty()) tls["alpn"] = alpn;
                if (IsClashNoFingerprint(clientFingerprint)) {
                    tls["fingerprint"] = tlsUnsafeFingerprint;
                } else if (const auto fingerprint = knownFingerprint(false); !fingerprint.isEmpty()) {
                    tls["fingerprint"] = fingerprint;
                }
                if (const auto pin = qs(settings.fingerprint.value_or(proxy.fingerprint)).trimmed(); !pin.isEmpty()) {
                    tls["pinnedPeerCertSha256"] = pin;
                    // An inherited pin on a TLS upload leg is already checked there by Validate.
                    const bool uploadPinned = proxy.tls && !uploadReality;
                    if (const auto message = Checks::XrayPinnedSha256(pin); !message.isEmpty() && (settings.fingerprint || !uploadPinned)) {
                        ParseDiag::Error(settings.fingerprint ? "xhttp-opts.download-settings.fingerprint" : "fingerprint", message);
                    }
                }
                object["tlsSettings"] = tls;
            }

            QJsonObject xhttp;
            if (const auto host = qs(settings.host.value_or(proxy.xhttp_opts.host)); !host.isEmpty()) xhttp["host"] = toAceHost(host);
            if (const auto path = qs(settings.path.value_or(proxy.xhttp_opts.path)); !path.isEmpty()) xhttp["path"] = path;
            xhttp["mode"] = hasText(settings.mode) ? qs(settings.mode) : "auto";

            QJsonObject extra;
            addXPaddingFields(extra, settings.x_padding_obfs_mode, settings.x_padding_key,
                              settings.x_padding_header, settings.x_padding_placement,
                              settings.x_padding_method);
            if (auto xmux = buildXmuxObject(settings.reuse_settings); !xmux.isEmpty()) extra["xmux"] = xmux;
            if (!extra.isEmpty()) xhttp["extra"] = extra;

            object["xhttpSettings"] = xhttp;
            return object;
        }

        const QStringList& knownXHTTPExtraKeys() {
            static const QStringList keys = {
                "headers",
                "xPaddingBytes",
                "xPaddingObfsMode",
                "xPaddingKey",
                "xPaddingHeader",
                "xPaddingPlacement",
                "xPaddingMethod",
                "uplinkHTTPMethod",
                "sessionPlacement",
                "sessionKey",
                "sessionIDPlacement",
                "sessionIDKey",
                "sessionIDTable",
                "sessionIDLength",
                "seqPlacement",
                "seqKey",
                "uplinkDataPlacement",
                "uplinkDataKey",
                "uplinkChunkSize",
                "noGRPCHeader",
                "noSSEHeader",
                "scMaxEachPostBytes",
                "scMinPostsIntervalMs",
                "scMaxBufferedPosts",
                "scStreamUpServerSecs",
                "serverMaxHeaderBytes",
                "xmux",
                "downloadSettings",
            };
            return keys;
        }

        const QStringList& knownXHTTPXmuxKeys() {
            static const QStringList keys = {
                "maxConcurrency",
                "maxConnections",
                "cMaxReuseTimes",
                "hMaxRequestTimes",
                "hMaxReusableSecs",
                "hKeepAlivePeriod",
            };
            return keys;
        }

        void exportString(QJsonObject& object, const QString& key, const QString& value) {
            if (value.isEmpty()) object.remove(key);
            else object[key] = value;
        }

        void exportBool(QJsonObject& object, const QString& key, bool value) {
            if (value) object[key] = true;
            else object.remove(key);
        }

        void exportLongLong(QJsonObject& object, const QString& key, long long value) {
            if (value != 0) object[key] = value;
            else object.remove(key);
        }

        void exportInt(QJsonObject& object, const QString& key, int value) {
            if (value != 0) object[key] = value;
            else object.remove(key);
        }

        void parseString(const QJsonObject& object, const QString& key, QString& target) {
            if (object.contains(key)) target = object[key].toString();
        }

        void parseVariantString(const QJsonObject& object, const QString& key, QString& target) {
            if (object.contains(key)) target = object[key].toVariant().toString();
        }

        void parseBool(const QJsonObject& object, const QString& key, bool& target) {
            if (object.contains(key)) target = object[key].toBool();
        }

        void parseLongLong(const QJsonObject& object, const QString& key, long long& target) {
            if (object.contains(key)) target = object[key].toVariant().toLongLong();
        }

        void parseInt(const QJsonObject& object, const QString& key, int& target) {
            if (object.contains(key)) target = object[key].toVariant().toInt();
        }

        void parseXHTTPXmuxObject(xrayXHTTP* config, const QJsonObject& obj) {
            for (const auto& key : obj.keys()) {
                if (!knownXHTTPXmuxKeys().contains(key)) {
                    config->rawXmux[key] = obj[key];
                }
            }
            parseVariantString(obj, "maxConcurrency", config->maxConcurrency);
            parseVariantString(obj, "maxConnections", config->maxConnections);
            parseVariantString(obj, "cMaxReuseTimes", config->cMaxReuseTimes);
            parseVariantString(obj, "hMaxRequestTimes", config->hMaxRequestTimes);
            parseVariantString(obj, "hMaxReusableSecs", config->hMaxReusableSecs);
            parseLongLong(obj, "hKeepAlivePeriod", config->hKeepAlivePeriod);
        }

        // Links carry headers as name|value|name|value.
        QStringList linkHeaders(const QUrlQuery& query) {
            const auto raw = query.queryItemValue("headers", QUrl::FullyDecoded);
            if (raw.isEmpty()) return {};
            auto headers = raw.split('|');
            if (headers.size() % 2 == 0) return headers;
            ParseDiag::Warn("headers", QCoreApplication::translate("Validation", "The headers must alternate names and values separated by |, for example User-Agent|curl, so they were ignored."));
            return {};
        }

        // Host goes to the host field: Xray httpupgrade refuses it inside headers, ws deprecates it there.
        QString clashWsHeaders(const std::map<std::string, std::string>& source, QStringList& headers) {
            QString host;
            for (const auto& [key, value] : source) {
                const auto name = QString::fromStdString(key);
                if (name.compare(QLatin1String("host"), Qt::CaseInsensitive) == 0) {
                    if (host.isEmpty()) host = QString::fromStdString(value);
                    continue;
                }
                headers << name << QString::fromStdString(value);
            }
            return host;
        }

        void warnUnparsableExtra(const QString& value) {
            const auto trimmed = value.trimmed();
            if (trimmed.isEmpty()) return;
            QJsonParseError err;
            if (const auto doc = QJsonDocument::fromJson(trimmed.toUtf8(), &err);
                err.error == QJsonParseError::NoError && doc.isObject() && doc.object().isEmpty()) return;
            ParseDiag::Warn("extra", QCoreApplication::translate("Validation", "The extra value is not a valid JSON object, so it was ignored."));
        }

        void parseXHTTPExtraObject(xrayXHTTP* config, const QJsonObject& obj) {
            for (const auto& key : obj.keys()) {
                if (!knownXHTTPExtraKeys().contains(key)) {
                    config->rawExtra[key] = obj[key];
                }
            }

            if (obj.contains("headers")) {
                if (obj["headers"].isObject()) {
                    config->headers = jsonObjectToQStringList(obj["headers"].toObject());
                } else if (obj["headers"].isArray()) {
                    config->headers = QJsonArray2QListString(obj["headers"].toArray());
                }
            }
            parseVariantString(obj, "xPaddingBytes", config->xPaddingBytes);
            parseBool(obj, "xPaddingObfsMode", config->xPaddingObfsMode);
            parseString(obj, "xPaddingKey", config->xPaddingKey);
            parseString(obj, "xPaddingHeader", config->xPaddingHeader);
            parseString(obj, "xPaddingPlacement", config->xPaddingPlacement);
            parseString(obj, "xPaddingMethod", config->xPaddingMethod);
            parseString(obj, "uplinkHTTPMethod", config->uplinkHTTPMethod);
            parseString(obj, "sessionPlacement", config->sessionIDPlacement); // legacy alias (pre-v26.6.22 Xray)
            parseString(obj, "sessionIDPlacement", config->sessionIDPlacement);
            parseString(obj, "sessionKey", config->sessionIDKey); // legacy alias (pre-v26.6.22 Xray)
            parseString(obj, "sessionIDKey", config->sessionIDKey);
            parseString(obj, "sessionIDTable", config->sessionIDTable);
            parseVariantString(obj, "sessionIDLength", config->sessionIDLength);
            parseString(obj, "seqPlacement", config->seqPlacement);
            parseString(obj, "seqKey", config->seqKey);
            parseString(obj, "uplinkDataPlacement", config->uplinkDataPlacement);
            parseString(obj, "uplinkDataKey", config->uplinkDataKey);
            parseVariantString(obj, "uplinkChunkSize", config->uplinkChunkSize);
            parseBool(obj, "noGRPCHeader", config->noGRPCHeader);
            parseBool(obj, "noSSEHeader", config->noSSEHeader);
            parseVariantString(obj, "scMaxEachPostBytes", config->scMaxEachPostBytes);
            parseVariantString(obj, "scMinPostsIntervalMs", config->scMinPostsIntervalMs);
            parseLongLong(obj, "scMaxBufferedPosts", config->scMaxBufferedPosts);
            parseVariantString(obj, "scStreamUpServerSecs", config->scStreamUpServerSecs);
            parseInt(obj, "serverMaxHeaderBytes", config->serverMaxHeaderBytes);
            if (obj.contains("downloadSettings")) {
                if (obj["downloadSettings"].isObject()) {
                    config->downloadSettings = QJsonObject2QString(obj["downloadSettings"].toObject(), true);
                } else if (obj["downloadSettings"].isString()) {
                    config->downloadSettings = obj["downloadSettings"].toString();
                }
            }
            if (auto xmuxObj = obj["xmux"].toObject(); !xmuxObj.isEmpty()) {
                parseXHTTPXmuxObject(config, xmuxObj);
            }
        }
    }

    QString NormalizeXrayNetwork(const QString& network) {
        const auto lower = network.toLower();
        if (lower == "tcp") return "raw";
        if (lower == "splithttp") return "xhttp";
        if (lower == "websocket") return "ws";
        return lower;
    }

    QString XrayLower(const QString& text)
    {
        QString lower = text;
        for (auto& c : lower) {
            if (c >= u'A' && c <= u'Z') c = QChar(c.unicode() + 32);
            else if (c == QChar(0x0130)) c = u'i';
            else if (c == QChar(0x212A)) c = u'k';
        }
        return lower;
    }

    QList<QJsonObject> XrayDownloadLeg(const QJsonObject& outbound)
    {
        QList<QJsonObject> leg;
        auto copy = outbound;
        xrayStreamEditLive(xrayStreamRoot(copy), QStringLiteral("streamSettings"), false, [&](const QList<XrayStreamNode>& streams) {
            if (!xrayStreamDialsXHTTP(streams)) return;
            xrayStreamEditDownloadLeg(streams, [&](const QList<XrayStreamNode>& spellings) {
                for (const auto& spelling : spellings) leg << *spelling.object;
            });
        });
        return leg;
    }

    XrayLegProtection XrayDownloadLegProtection(const QList<QJsonObject>& leg, QString& server)
    {
        auto spellings = leg;
        QList<XrayStreamNode> nodes;
        for (auto& spelling : spellings) nodes.append(XrayStreamNode{&spelling, QString()});
        server = xrayStreamLast(nodes, QStringLiteral("address"), true).toString();
        const auto security = XrayLower(xrayStreamLast(nodes, QStringLiteral("security"), false).toString());
        if (security == "reality") return XrayLegProtection::Verified;
        if (security != "tls") return XrayLegProtection::Plain;
        bool insecure = false;
        bool pinned = false;
        xrayStreamEditLive(nodes, QStringLiteral("tlsSettings"), false, [&](const QList<XrayStreamNode>& tls) {
            insecure = xrayStreamLast(tls, QStringLiteral("allowInsecure"), false).toBool();
            // Xray verifies against a pin or the names it is given even with allowInsecure; it splits both on commas and skips blanks.
            for (const auto& name : {QStringLiteral("pinnedPeerCertSha256"), QStringLiteral("verifyPeerCertByName")}) {
                const auto entries = xrayStreamLast(tls, name, false).toString().split(u',');
                pinned = pinned || std::any_of(entries.cbegin(), entries.cend(), [](const QString& entry) { return !entry.trimmed().isEmpty(); });
            }
        });
        return insecure && !pinned ? XrayLegProtection::Unverified : XrayLegProtection::Verified;
    }

    void SetXrayDialerProxy(QJsonObject& outbound, const QString& tag)
    {
        xrayStreamEditLive(xrayStreamRoot(outbound), QStringLiteral("streamSettings"), true, [&](const QList<XrayStreamNode>& streams) {
            xrayStreamSetSockopt(streams, QStringLiteral("dialerProxy"), tag);
            // The download leg dials with its own sockopt unless the main one sets penetrate.
            xrayStreamEditDownloadLeg(streams, [&](const QList<XrayStreamNode>& leg) {
                xrayStreamSetSockopt(leg, QStringLiteral("dialerProxy"), tag);
            });
        });
    }

    void BindXrayDownloadLeg(QJsonObject& outbound, const QString& iface)
    {
        if (iface.isEmpty()) return;
        xrayStreamEditLive(xrayStreamRoot(outbound), QStringLiteral("streamSettings"), false, [&](const QList<XrayStreamNode>& streams) {
            xrayStreamEditDownloadLeg(streams, [&](const QList<XrayStreamNode>& leg) {
                // A leg that names an interface of its own keeps it.
                bool pinned = false;
                xrayStreamEditLive(leg, QStringLiteral("sockopt"), false, [&](const QList<XrayStreamNode>& sockopts) {
                    pinned = !xrayStreamLast(sockopts, QStringLiteral("interface"), false).toString().isEmpty();
                });
                if (!pinned) xrayStreamSetSockopt(leg, QStringLiteral("interface"), iface);
            });
        });
    }

    bool StripXrayDialerState(QJsonObject& stream)
    {
        return xrayStreamEditNamed(stream, QStringLiteral("sockopt"), xrayStreamStripDialerKeys);
    }

    bool xrayTLS::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("sni")) serverName = query.queryItemValue("sni");
        if (query.hasQueryItem("peer")) serverName = query.queryItemValue("peer");
        if (query.hasQueryItem("server_name")) serverName = query.queryItemValue("server_name");
        if (query.hasQueryItem("pcs")) pinnedPeerCertSha256 = query.queryItemValue("pcs", QUrl::FullyDecoded);
        if (query.hasQueryItem("vcn")) verifyPeerCertByName = query.queryItemValue("vcn", QUrl::FullyDecoded);
        if (query.hasQueryItem("alpn")) alpn = SplitAndTrim(query.queryItemValue("alpn", QUrl::FullyDecoded), ",", false);
        if (query.hasQueryItem("fp")) fingerprint = xrayStreamKnownFingerprint(query.queryItemValue("fp"), false, "fp");
        if (query.hasQueryItem("cs")) cipherSuites = splitCipherSuites(query.queryItemValue("cs", QUrl::FullyDecoded)).join(':');
        if (query.hasQueryItem("ech")) echConfigList = query.queryItemValue("ech", QUrl::FullyDecoded).trimmed();
        return true;
    }

    bool xrayTLS::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("serverName")) serverName = object["serverName"].toString();
        if (object.contains("pinnedPeerCertSha256")) pinnedPeerCertSha256 = object["pinnedPeerCertSha256"].toString();
        if (object.contains("verifyPeerCertByName")) verifyPeerCertByName = object["verifyPeerCertByName"].toString();
        // ParseFromJson is also the DB loader: only an import normalises or drops values.
        const bool importing = ParseDiag::Active();
        if (object.contains("alpn")) {
            alpn = JsonStringList(object["alpn"], "tlsSettings.alpn");
            // Xray's StringList also takes one comma-separated string.
            if (importing) alpn = SplitAndTrim(alpn.join(','), ",", false);
        }
        if (object.contains("fingerprint")) {
            const auto value = object["fingerprint"].toString();
            fingerprint = importing ? xrayStreamKnownFingerprint(value, false, "tlsSettings.fingerprint") : value;
        }
        if (object.contains("cipherSuites")) cipherSuites = object["cipherSuites"].toString();
        if (object.contains("echConfigList")) echConfigList = object["echConfigList"].toString();
        return true;
    }

    bool xrayTLS::ParseFromClash(const clash::Proxies& object) {
        if (!object.servername.empty()) {
            serverName = QString::fromStdString(object.servername);
        } else if (!object.sni.empty()) {
            serverName = QString::fromStdString(object.sni);
        } else {
            serverName = QString::fromStdString(object.server);
        }
        for (const auto& s : object.alpn) {
            if (const auto protocol = QString::fromStdString(s).trimmed(); !protocol.isEmpty()) alpn.append(protocol);
        }
        // mihomo's fingerprint is the certificate pin in hex, which Validate checks as for pcs; client-fingerprint is the uTLS hello.
        if (!object.fingerprint.empty()) pinnedPeerCertSha256 = QString::fromStdString(object.fingerprint).trimmed();
        // Xray's "unsafe" is Go's own TLS, which mihomo's "none" asks for; an empty fingerprint would mean Chrome.
        if (IsClashNoFingerprint(object.client_fingerprint)) {
            fingerprint = tlsUnsafeFingerprint;
        } else if (!object.client_fingerprint.empty()) {
            fingerprint = xrayStreamKnownFingerprint(QString::fromStdString(object.client_fingerprint), false, "client-fingerprint");
        }
        return true;
    }

    QString xrayTLS::ExportToLink() {
        QUrlQuery query;
        query.addQueryItem("sni", serverName);
        if (!pinnedPeerCertSha256.isEmpty()) query.addQueryItem("pcs", pinnedPeerCertSha256);
        if (!verifyPeerCertByName.isEmpty()) query.addQueryItem("vcn", verifyPeerCertByName);
        if (!alpn.isEmpty()) query.addQueryItem("alpn", alpn.join(","));
        if (!fingerprint.isEmpty()) query.addQueryItem("fp", fingerprint);
        if (!cipherSuites.isEmpty()) query.addQueryItem("cs", cipherSuites);
        if (!echConfigList.isEmpty()) query.addQueryItem("ech", echConfigList);
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayTLS::ExportToJson() {
        QJsonObject object;
        object["serverName"] = toAceHost(serverName);
        if (!pinnedPeerCertSha256.isEmpty()) object["pinnedPeerCertSha256"] = pinnedPeerCertSha256;
        if (!verifyPeerCertByName.isEmpty()) object["verifyPeerCertByName"] = verifyPeerCertByName;
        if (!alpn.isEmpty()) {
            object["alpn"] = QListStr2QJsonArray(alpn);
        }
        if (!fingerprint.isEmpty()) object["fingerprint"] = fingerprint;
        if (!cipherSuites.isEmpty()) object["cipherSuites"] = cipherSuites;
        if (!echConfigList.isEmpty()) object["echConfigList"] = echConfigList;
        return object;
    }

    BuildResult xrayTLS::Build() {
        auto obj = ExportToJson();
        if (fingerprint.isEmpty() && !Configs::dataManager->settingsRepo->utlsFingerprint.isEmpty()) obj["fingerprint"] = Configs::dataManager->settingsRepo->utlsFingerprint;
        return {obj, ""};
    }

    bool xrayReality::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("sni")) serverName = query.queryItemValue("sni");
        if (query.hasQueryItem("peer")) serverName = query.queryItemValue("peer");
        if (query.hasQueryItem("pbk")) password = ImportRealityPublicKey(query.queryItemValue("pbk"), "pbk");
        if (query.hasQueryItem("fp")) fingerprint = xrayStreamKnownFingerprint(query.queryItemValue("fp"), true, "fp");
        if (query.hasQueryItem("sid")) shortId = query.queryItemValue("sid").trimmed();
        if (query.hasQueryItem("spx")) spiderX = query.queryItemValue("spx", QUrl::FullyDecoded);
        if (query.hasQueryItem("pqv")) mldsa65Verify = query.queryItemValue("pqv", QUrl::FullyDecoded).trimmed();
        return true;

    }

    bool xrayReality::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("serverName")) serverName = object["serverName"].toString();
        if (object.contains("fingerprint")) {
            const auto value = object["fingerprint"].toString();
            fingerprint = ParseDiag::Active() ? xrayStreamKnownFingerprint(value, true, "realitySettings.fingerprint") : value;
        }
        // Xray reads publicKey, the older name, when password is empty.
        if (object.contains("password") || object.contains("publicKey")) {
            const auto field = object["password"].toString().trimmed().isEmpty() ? QStringLiteral("publicKey") : QStringLiteral("password");
            const auto key = object[field].toString();
            password = ParseDiag::Active() ? ImportRealityPublicKey(key, "realitySettings." + field) : key.trimmed();
        }
        if (object.contains("shortId")) shortId = object["shortId"].toString().trimmed();
        if (object.contains("spiderX")) spiderX = object["spiderX"].toString();
        if (object.contains("mldsa65Verify")) mldsa65Verify = object["mldsa65Verify"].toString().trimmed();
        return true;
    }

    bool xrayReality::ParseFromClash(const clash::Proxies& object) {
        if (!object.servername.empty()) {
            serverName = QString::fromStdString(object.servername);
        } else if (!object.sni.empty()) {
            serverName = QString::fromStdString(object.sni);
        } else {
            serverName = QString::fromStdString(object.server);
        }
        // REALITY needs uTLS, so mihomo's "none" leaves Xray's default.
        if (!object.client_fingerprint.empty() && !IsClashNoFingerprint(object.client_fingerprint)) {
            fingerprint = xrayStreamKnownFingerprint(QString::fromStdString(object.client_fingerprint), true, "client-fingerprint");
        }
        if (!object.reality_opts.public_key.empty()) {
            password = ImportRealityPublicKey(QString::fromStdString(object.reality_opts.public_key), "reality-opts.public-key");
        }
        if (!object.reality_opts.short_id.empty()) shortId = QString::fromStdString(object.reality_opts.short_id).trimmed();
        return true;
    }

    QString xrayReality::ExportToLink() {
        QUrlQuery query;
        query.addQueryItem("sni", serverName);
        if (!fingerprint.isEmpty()) query.addQueryItem("fp", fingerprint);
        if (!password.isEmpty()) query.addQueryItem("pbk", password);
        if (!shortId.isEmpty()) query.addQueryItem("sid", shortId);
        if (!spiderX.isEmpty()) query.addQueryItem("spx", spiderX);
        if (!mldsa65Verify.isEmpty()) query.addQueryItem("pqv", mldsa65Verify);
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayReality::ExportToJson() {
        QJsonObject object;
        object["serverName"] = toAceHost(serverName);
        if (!fingerprint.isEmpty()) object["fingerprint"] = fingerprint;
        if (!password.isEmpty()) object["password"] = password;
        if (!shortId.isEmpty()) object["shortId"] = shortId;
        if (!spiderX.isEmpty()) object["spiderX"] = spiderX;
        if (!mldsa65Verify.isEmpty()) object["mldsa65Verify"] = mldsa65Verify;
        return object;
    }

    BuildResult xrayReality::Build() {
        auto obj = ExportToJson();
        if (fingerprint.isEmpty() && !Configs::dataManager->settingsRepo->utlsFingerprint.isEmpty()) obj["fingerprint"] = Configs::dataManager->settingsRepo->utlsFingerprint;
        // REALITY needs a uTLS hello and fails on "unsafe"; Xray's own default (chrome) takes over.
        if (obj.value("fingerprint").toString() == tlsUnsafeFingerprint) obj.remove("fingerprint");
        return {obj, ""};
    }

    bool xrayXHTTP::ParseExtraJson(const QString &str) {
        auto obj = QString2QJsonObject(str);
        // Python dict repr ('key': True), rewritten only when strict JSON fails so valid values stay intact.
        if (obj.isEmpty()) obj = QString2QJsonObject(QString(str).replace('\'', '"').replace("True", "true").replace("False", "false"));
        if (obj.isEmpty()) return false;
        parseXHTTPExtraObject(this, obj);
        return true;
    }


    bool xrayXHTTP::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("host")) host = query.queryItemValue("host");
        if (query.hasQueryItem("path")) path = query.queryItemValue("path", QUrl::FullyDecoded);
        if (query.hasQueryItem("mode")) mode = query.queryItemValue("mode");
        if (query.hasQueryItem("extra")) {
            const auto extra = query.queryItemValue("extra", QUrl::FullyDecoded);
            if (!ParseExtraJson(extra) && !ParseExtraJson(formDecodedQueryValue(query, "extra"))) warnUnparsableExtra(extra);
        }
        if (query.hasQueryItem("headers")) headers = linkHeaders(query);
        if (query.hasQueryItem("x_padding_bytes")) xPaddingBytes = query.queryItemValue("x_padding_bytes");
        if (query.hasQueryItem("no_grpc_header")) noGRPCHeader = query.queryItemValue("no_grpc_header").replace("1", "true") == "true";

        if (query.hasQueryItem("sc_max_each_post_bytes")) scMaxEachPostBytes = query.queryItemValue("sc_max_each_post_bytes");
        if (query.hasQueryItem("sc_min_posts_interval_ms")) scMinPostsIntervalMs = query.queryItemValue("sc_min_posts_interval_ms");

        if (query.hasQueryItem("max_concurrency")) maxConcurrency = query.queryItemValue("max_concurrency");
        if (query.hasQueryItem("max_connections")) maxConnections = query.queryItemValue("max_connections");

        if (query.hasQueryItem("max_reuse_times")) cMaxReuseTimes = query.queryItemValue("max_reuse_times");

        if (query.hasQueryItem("max_request_times")) hMaxRequestTimes = query.queryItemValue("max_request_times");
        if (query.hasQueryItem("max_reusable_secs")) hMaxReusableSecs = query.queryItemValue("max_reusable_secs");
        if (query.hasQueryItem("keep_alive_period")) {
            const auto value = query.queryItemValue("keep_alive_period").trimmed();
            if (AcceptOptional("keep_alive_period", Validation::Int(value, LLONG_MIN, LLONG_MAX), value)) hKeepAlivePeriod = value.toLongLong();
        }
        return true;
    }

    bool xrayXHTTP::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("host")) host = object["host"].toString();
        if (object.contains("path")) path = object["path"].toString();
        if (object.contains("mode")) mode = object["mode"].toString();
        QJsonObject topLevelExtra;
        for (const auto& key : object.keys()) {
            if (key == "host" || key == "path" || key == "mode" || key == "extra") continue;
            topLevelExtra[key] = object[key];
        }
        if (!topLevelExtra.isEmpty()) {
            parseXHTTPExtraObject(this, topLevelExtra);
        }
        if (auto exObj = object["extra"].toObject(); !exObj.isEmpty()) {
            parseXHTTPExtraObject(this, exObj);
        } else if (object["extra"].isString() && !ParseExtraJson(object["extra"].toString())) {
            warnUnparsableExtra(object["extra"].toString());
        }
        return true;
    }

    bool xrayXHTTP::ParseFromClash(const clash::Proxies& object) {
        const auto& opts = object.xhttp_opts;
        host = qs(opts.host);
        path = qs(opts.path);
        mode = hasText(opts.mode) ? qs(opts.mode) : "auto";

        xPaddingObfsMode = opts.x_padding_obfs_mode;
        xPaddingKey = qs(opts.x_padding_key);
        xPaddingHeader = qs(opts.x_padding_header);
        xPaddingPlacement = qs(opts.x_padding_placement);
        xPaddingMethod = qs(opts.x_padding_method);
        scMinPostsIntervalMs = qs(opts.sc_min_posts_interval_ms);

        maxConcurrency = qs(opts.reuse_settings.max_concurrency);
        maxConnections = qs(opts.reuse_settings.max_connections);
        cMaxReuseTimes = qs(opts.reuse_settings.c_max_reuse_times);
        hMaxRequestTimes = qs(opts.reuse_settings.h_max_request_times);
        hMaxReusableSecs = qs(opts.reuse_settings.h_max_reusable_secs);
        if (hasText(opts.reuse_settings.h_keep_alive_period)) {
            hKeepAlivePeriod = qs(opts.reuse_settings.h_keep_alive_period).toLongLong();
        }

        if (opts.has_download_settings) {
            downloadSettings = QJsonObject2QString(buildDownloadSettingsObject(object), true);
        }

        return true;
    }

    QString xrayXHTTP::ExportToLink() {
        QUrlQuery query;
        if (!host.isEmpty()) query.addQueryItem("host", host);
        if (!path.isEmpty()) query.addQueryItem("path", path);
        if (!mode.isEmpty()) query.addQueryItem("mode", mode);
        auto jsonExport = ExportToJson();
        if (jsonExport.contains("extra") && jsonExport["extra"].isObject()) {
            auto exObj = jsonExport["extra"].toObject();
            if (!exObj.isEmpty()) {
                query.addQueryItem("extra", QJsonObject2QString(exObj, true));
            }
        }
        if (!headers.isEmpty()) query.addQueryItem("headers", headers.join("|"));
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayXHTTP::ExportToJson() {
        QJsonObject obj;
        if (!host.isEmpty()) obj["host"] = toAceHost(host);
        if (!path.isEmpty()) obj["path"] = path;
        if (!mode.isEmpty()) obj["mode"] = mode;

        QJsonObject extraObj = rawExtra;
        if (!headers.isEmpty()) extraObj["headers"] = qStringListToJsonObject(headers);
        else extraObj.remove("headers");
        exportString(extraObj, "xPaddingBytes", xPaddingBytes);
        exportBool(extraObj, "xPaddingObfsMode", xPaddingObfsMode);
        exportString(extraObj, "xPaddingKey", xPaddingKey);
        exportString(extraObj, "xPaddingHeader", xPaddingHeader);
        exportString(extraObj, "xPaddingPlacement", xPaddingPlacement);
        exportString(extraObj, "xPaddingMethod", xPaddingMethod);
        exportString(extraObj, "uplinkHTTPMethod", uplinkHTTPMethod);
        exportString(extraObj, "sessionIDPlacement", sessionIDPlacement);
        exportString(extraObj, "sessionIDKey", sessionIDKey);
        exportString(extraObj, "sessionIDTable", sessionIDTable);
        exportString(extraObj, "sessionIDLength", sessionIDLength);
        exportString(extraObj, "seqPlacement", seqPlacement);
        exportString(extraObj, "seqKey", seqKey);
        exportString(extraObj, "uplinkDataPlacement", uplinkDataPlacement);
        exportString(extraObj, "uplinkDataKey", uplinkDataKey);
        exportString(extraObj, "uplinkChunkSize", uplinkChunkSize);
        exportBool(extraObj, "noGRPCHeader", noGRPCHeader);
        exportBool(extraObj, "noSSEHeader", noSSEHeader);
        exportString(extraObj, "scMaxEachPostBytes", scMaxEachPostBytes);
        exportString(extraObj, "scMinPostsIntervalMs", scMinPostsIntervalMs);
        exportLongLong(extraObj, "scMaxBufferedPosts", scMaxBufferedPosts);
        exportString(extraObj, "scStreamUpServerSecs", scStreamUpServerSecs);
        exportInt(extraObj, "serverMaxHeaderBytes", serverMaxHeaderBytes);
        if (mode == "stream-one") {
            // Xray refuses downloadSettings in stream-one mode under any spelling.
            xrayStreamRemoveNamed(extraObj, QStringLiteral("downloadSettings"));
        } else if (!downloadSettings.isEmpty()) {
            if (auto dsObj = QString2QJsonObject(downloadSettings); !dsObj.isEmpty()) {
                extraObj["downloadSettings"] = dsObj;
            }
        } else {
            extraObj.remove("downloadSettings");
        }
        QJsonObject xmuxObj = rawXmux;
        exportString(xmuxObj, "maxConcurrency", maxConcurrency);
        exportString(xmuxObj, "maxConnections", maxConnections);
        exportString(xmuxObj, "cMaxReuseTimes", cMaxReuseTimes);
        exportString(xmuxObj, "hMaxRequestTimes", hMaxRequestTimes);
        exportString(xmuxObj, "hMaxReusableSecs", hMaxReusableSecs);
        exportLongLong(xmuxObj, "hKeepAlivePeriod", hKeepAlivePeriod);
        if (!xmuxObj.isEmpty()) extraObj["xmux"] = xmuxObj;
        else extraObj.remove("xmux");
        if (!extraObj.isEmpty()) obj["extra"] = extraObj;
        return obj;
    }

    BuildResult xrayXHTTP::Build() {
        return {ExportToJson(), ""};
    }

    bool xrayWS::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());
        if (query.hasQueryItem("host")) host = query.queryItemValue("host");
        if (query.hasQueryItem("path")) path = query.queryItemValue("path", QUrl::FullyDecoded);
        ReadQueryInt(query, "ed", ed);
        if (query.hasQueryItem("headers")) headers = linkHeaders(query);
        ReadQueryInt(query, "heartbeat_period", heartbeatPeriod);
        return true;
    }

    bool xrayWS::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("path")) {
            path = object["path"].toString();
            if (path.contains("?ed=")) {
                auto spl = path.split("?ed=");
                path = spl[0];
                ed = spl[1].toInt();
            }
        }
        if (object.contains("host")) host = object["host"].toString();
        if (object.contains("headers") && object["headers"].isObject())
            headers = jsonObjectToQStringList(object["headers"].toObject());
        if (object.contains("heartbeatPeriod")) heartbeatPeriod = object["heartbeatPeriod"].toInt(0);
        return true;
    }

    bool xrayWS::ParseFromClash(const clash::Proxies& object) {
        if (!object.ws_opts.path.empty()) {
            path = QString::fromStdString(object.ws_opts.path);
            if (path.contains("?ed=")) {
                auto spl = path.split("?ed=");
                path = spl[0];
                ed = spl[1].toInt();
            }
        }
        // mihomo sends the ws-opts Host header as the HTTP Host and uses servername only for the SNI.
        if (const auto headerHost = clashWsHeaders(object.ws_opts.headers, headers); !headerHost.isEmpty()) {
            host = headerHost;
        } else if (!object.servername.empty()) {
            host = QString::fromStdString(object.servername);
        }
        return true;
    }

    QString xrayWS::ExportToLink() {
        QUrlQuery query;
        if (!host.isEmpty()) query.addQueryItem("host", host);
        if (!path.isEmpty() && path != "/") query.addQueryItem("path", path);
        if (ed > 0) query.addQueryItem("ed", QString::number(ed));
        if (!headers.isEmpty()) query.addQueryItem("headers", headers.join("|"));
        if (heartbeatPeriod > 0) query.addQueryItem("heartbeat_period", QString::number(heartbeatPeriod));
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayWS::ExportToJson() {
        QJsonObject obj;
        if (!path.isEmpty()) {
            auto fullPath = path;
            if (ed > 0) fullPath += "?ed=" + QString::number(ed);
            obj["path"] = fullPath;
        }
        if (!host.isEmpty()) obj["host"] = toAceHost(host);
        if (!headers.isEmpty()) obj["headers"] = qStringListToJsonObject(headers);
        if (heartbeatPeriod > 0) obj["heartbeatPeriod"] = heartbeatPeriod;
        return obj;
    }

    BuildResult xrayWS::Build() {
        return {ExportToJson(), ""};
    }

    bool xrayHttpUpgrade::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());
        if (query.hasQueryItem("host")) host = query.queryItemValue("host");
        if (query.hasQueryItem("path")) path = query.queryItemValue("path", QUrl::FullyDecoded);
        ReadQueryInt(query, "ed", ed);
        if (query.hasQueryItem("headers")) headers = linkHeaders(query);
        return true;
    }

    bool xrayHttpUpgrade::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("path")) {
            path = object["path"].toString();
            if (path.contains("?ed=")) {
                auto spl = path.split("?ed=");
                path = spl[0];
                ed = spl[1].toInt();
            }
        }
        if (object.contains("host")) host = object["host"].toString();
        if (object.contains("headers") && object["headers"].isObject())
            headers = jsonObjectToQStringList(object["headers"].toObject());
        return true;
    }

    bool xrayHttpUpgrade::ParseFromClash(const clash::Proxies& object) {
        if (!object.ws_opts.path.empty()) {
            path = QString::fromStdString(object.ws_opts.path);
            if (path.contains("?ed=")) {
                auto spl = path.split("?ed=");
                path = spl[0];
                ed = spl[1].toInt();
            }
        }
        // mihomo sends the ws-opts Host header as the HTTP Host and uses servername only for the SNI.
        if (const auto headerHost = clashWsHeaders(object.ws_opts.headers, headers); !headerHost.isEmpty()) {
            host = headerHost;
        } else if (!object.servername.empty()) {
            host = QString::fromStdString(object.servername);
        }
        return true;
    }

    QString xrayHttpUpgrade::ExportToLink() {
        QUrlQuery query;
        if (!host.isEmpty()) query.addQueryItem("host", host);
        if (!path.isEmpty() && path != "/") query.addQueryItem("path", path);
        if (ed > 0) query.addQueryItem("ed", QString::number(ed));
        if (!headers.isEmpty()) query.addQueryItem("headers", headers.join("|"));
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayHttpUpgrade::ExportToJson() {
        QJsonObject obj;
        if (!path.isEmpty()) {
            auto fullPath = path;
            if (ed > 0) fullPath += "?ed=" + QString::number(ed);
            obj["path"] = fullPath;
        }
        if (!host.isEmpty()) obj["host"] = toAceHost(host);
        if (!headers.isEmpty()) obj["headers"] = qStringListToJsonObject(headers);
        return obj;
    }

    BuildResult xrayHttpUpgrade::Build() {
        return {ExportToJson(), ""};
    }

    bool xrayGRPC::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());
        if (query.hasQueryItem("authority")) authority = query.queryItemValue("authority", QUrl::FullyDecoded);
        if (query.hasQueryItem("serviceName")) serviceName = query.queryItemValue("serviceName", QUrl::FullyDecoded);
        if (query.hasQueryItem("mode") && query.queryItemValue("mode") == "multi") multiMode = true;
        return true;
    }

    bool xrayGRPC::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("authority")) authority = object["authority"].toString();
        if (object.contains("serviceName")) serviceName = object["serviceName"].toString();
        if (object.contains("multiMode")) multiMode = object["multiMode"].toBool();
        return true;
    }

    bool xrayGRPC::ParseFromClash(const clash::Proxies& object) {
        if (!object.grpc_opts.grpc_service_name.empty()) serviceName = QString::fromStdString(object.grpc_opts.grpc_service_name);
        return true;
    }

    QString xrayGRPC::ExportToLink() {
        QUrlQuery query;
        if (!authority.isEmpty()) query.addQueryItem("authority", authority);
        if (!serviceName.isEmpty()) query.addQueryItem("serviceName", serviceName);
        if (multiMode) query.addQueryItem("mode", "multi");
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayGRPC::ExportToJson() {
        QJsonObject obj;
        if (!authority.isEmpty()) obj["authority"] = toAceHost(authority);
        if (!serviceName.isEmpty()) obj["serviceName"] = serviceName;
        if (multiMode) obj["multiMode"] = multiMode;
        return obj;
    }

    BuildResult xrayGRPC::Build() {
        return {ExportToJson(), ""};
    }

    bool xrayStreamSetting::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("fm") || query.hasQueryItem("finalmask")) {
            auto key = query.hasQueryItem("fm") ? "fm" : "finalmask";
            auto fmRaw = query.queryItemValue(key, QUrl::FullyDecoded);
            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(fmRaw.toUtf8(), &err);
            if (err.error != QJsonParseError::NoError) doc = QJsonDocument::fromJson(formDecodedQueryValue(query, key).toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                finalmask = doc.object();
            } else if (err.error != QJsonParseError::NoError) {
                ParseDiag::Warn(key, QCoreApplication::translate("Validation", "The finalmask value is not valid JSON (%1), so it was ignored.").arg(err.errorString()));
            } else {
                ParseDiag::Warn(key, QCoreApplication::translate("Validation", "The finalmask value must be a JSON object, so it was ignored."));
            }
        }

        if (const auto type = query.queryItemValue("type"); !type.isEmpty()) network = NormalizeXrayNetwork(type);
        if (!Configs::XrayNetworks.contains(network)) {
            // A share link has no lossless fallback, unlike an Xray JSON outbound, which is then kept verbatim.
            ParseDiag::Error("type", Checks::XrayNetwork(network), network);
            return false;
        }
        if (network == "raw" && query.queryItemValue("headerType") == "http") {
            QJsonObject request;
            if (const auto paths = splitList(query.queryItemValue("path", QUrl::FullyDecoded)); !paths.isEmpty()) {
                request["path"] = QListStr2QJsonArray(paths);
            }
            if (auto hosts = splitList(query.queryItemValue("host", QUrl::FullyDecoded)); !hosts.isEmpty()) {
                for (auto& item : hosts) item = toAceHost(item);
                request["headers"] = QJsonObject{{"Host", QListStr2QJsonArray(hosts)}};
            }
            QJsonObject header{{"type", "http"}};
            if (!request.isEmpty()) header["request"] = request;
            rawSettings = QJsonObject{{"header", header}};
        }
        // Xray matches security case-insensitively.
        if (query.hasQueryItem("security")) security = query.queryItemValue("security").trimmed().toLower();
        if (security == "tls") TLS->ParseFromLink(link);
        else if (security == "reality") reality->ParseFromLink(link);
        if (network == "xhttp") xhttp->ParseFromLink(link);
        else if (network == "ws") ws->ParseFromLink(link);
        else if (network == "httpupgrade") httpupgrade->ParseFromLink(link);
        else if (network == "grpc") grpc->ParseFromLink(link);

        return true;
    }

    bool xrayStreamSetting::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;

        if (object.contains("finalmask") && object["finalmask"].isObject()) finalmask = object["finalmask"].toObject();
        if (object["sockopt"].isObject()) {
            sockopt = object["sockopt"].toObject();
            xrayStreamStripDialerKeys(sockopt);
        }

        if (object.contains("method")) network = object.value("method").toString();
        else if (object.contains("network")) network = object.value("network").toString();
        network = NormalizeXrayNetwork(network);
        if (!Configs::XrayNetworks.contains(network)) return false;
        if (network == "raw") {
            if (object["rawSettings"].isObject()) rawSettings = object["rawSettings"].toObject();
            else if (object["tcpSettings"].isObject()) rawSettings = object["tcpSettings"].toObject();
        }
        if (object.contains("security")) security = object.value("security").toString().trimmed().toLower();
        if (security == "tls" && object["tlsSettings"].isObject()) TLS->ParseFromJson(object["tlsSettings"].toObject());
        else if (security == "reality" && object["realitySettings"].isObject()) reality->ParseFromJson(object["realitySettings"].toObject());
        if (network == "xhttp") {
            // splithttpSettings is Xray's older key; xhttpSettings wins when both are set.
            const auto xhttpSettings = object["xhttpSettings"].isObject() ? object["xhttpSettings"] : object["splithttpSettings"];
            if (xhttpSettings.isObject()) xhttp->ParseFromJson(xhttpSettings.toObject());
        }
        if (network == "ws" && object["wsSettings"].isObject()) ws->ParseFromJson(object["wsSettings"].toObject());
        if (network == "httpupgrade" && object["httpupgradeSettings"].isObject()) httpupgrade->ParseFromJson(object["httpupgradeSettings"].toObject());
        if (network == "grpc" && object["grpcSettings"].isObject()) grpc->ParseFromJson(object["grpcSettings"].toObject());
        return true;
    }

    bool xrayStreamSetting::ParseFromClash(const clash::Proxies& object) {
        if (!object.network.empty()) network = QString::fromStdString(object.network);
        if (network == "tcp") network = "raw";
        if (network != "raw" && network != "ws" && network != "grpc" && network != "xhttp") return false;
        if (object.tls) {
            if (object.reality_opts.public_key.empty()) {
                security = "tls";
                TLS->ParseFromClash(object);
            } else {
                security = "reality";
                reality->ParseFromClash(object);
            }
        }
        if (network == "xhttp") xhttp->ParseFromClash(object);
        if (network == "ws") {
            if (object.ws_opts.v2ray_http_upgrade) {
                network = "httpupgrade";
                httpupgrade->ParseFromClash(object);
            } else {
                ws->ParseFromClash(object);
            }
        }
        if (network == "grpc") grpc->ParseFromClash(object);
        return true;
    }

    QString xrayStreamSetting::ExportToLink() {
        QUrlQuery query;
        if (!network.isEmpty()) query.addQueryItem("type", network == "raw" ? "tcp" : network);
        if (!finalmask.isEmpty()) query.addQueryItem("fm", QJsonObject2QString(finalmask, true));
        // value(), never operator[]: the mutable operator[] inserts a null entry for a missing key.
        if (const auto header = rawSettings.value("header").toObject();
            network == "raw" && header.value("type").toString() == "http") {
            query.addQueryItem("headerType", "http");
            const auto request = header.value("request").toObject();
            if (const auto paths = request.value("path").toArray(); !paths.isEmpty()) {
                query.addQueryItem("path", QJsonArray2QListString(paths).join(','));
            }
            if (const auto hosts = request.value("headers").toObject().value("Host").toArray(); !hosts.isEmpty()) {
                query.addQueryItem("host", QJsonArray2QListString(hosts).join(','));
            }
        }
        if (!security.isEmpty()) query.addQueryItem("security", security);
        if (security == "tls") mergeUrlQuery(query, TLS->ExportToLink());
        if (security == "reality") mergeUrlQuery(query, reality->ExportToLink());
        if (network == "xhttp") mergeUrlQuery(query, xhttp->ExportToLink());
        if (network == "ws") mergeUrlQuery(query, ws->ExportToLink());
        if (network == "httpupgrade") mergeUrlQuery(query, httpupgrade->ExportToLink());
        if (network == "grpc") mergeUrlQuery(query, grpc->ExportToLink());
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayStreamSetting::ExportToJson() {
        QJsonObject object;
        object["network"] = network;
        object["security"] = security;
        if (!finalmask.isEmpty()) object["finalmask"] = finalmask;
        if (!sockopt.isEmpty()) object["sockopt"] = sockopt;
        if (network == "raw" && !rawSettings.isEmpty()) object["rawSettings"] = rawSettings;
        if (security == "tls") object["tlsSettings"] = TLS->ExportToJson();
        else if (security == "reality") object["realitySettings"] = reality->ExportToJson();
        if (network == "xhttp") object["xhttpSettings"] = xhttp->ExportToJson();
        if (network == "ws") object["wsSettings"] = ws->ExportToJson();
        if (network == "httpupgrade") object["httpupgradeSettings"] = httpupgrade->ExportToJson();
        if (network == "grpc") object["grpcSettings"] = grpc->ExportToJson();
        return object;
    }

    QJsonObject xrayStreamSetting::ExportIdentity() {
        QJsonObject object;
        object["network"] = network;
        object["security"] = security;
        // No rawSettings: identity must not carry values a subscription rotates (Host header, paths).
        if (security == "reality") {
            if (!reality->serverName.isEmpty()) object["sni"] = toAceHost(reality->serverName);
            if (!reality->fingerprint.isEmpty()) object["fingerprint"] = reality->fingerprint;
        } else if (security == "tls") {
            if (!TLS->serverName.isEmpty()) object["sni"] = toAceHost(TLS->serverName);
            if (!TLS->fingerprint.isEmpty()) object["fingerprint"] = TLS->fingerprint;
            if (!TLS->echConfigList.isEmpty())
                object["ech"] = TLS->echConfigList.contains("://") ? TLS->echConfigList : QStringLiteral("static");
        }
        return object;
    }


    QString getDirectDomainStrategy() {
        const auto &settings = *Configs::dataManager->settingsRepo;
        bool routeOwnsDns = false;
        const auto dns = Configs::RouteDns::Current(&routeOwnsDns);
        // No DNS rule blanks AAAA for these lookups any more, so the switch caps the strategy instead.
        if (dns.direct_dns_disable_ipv6 && (routeOwnsDns || !settings.use_dns_object)) return "ipv4_only";
        return settings.default_domain_strategy;
    }

    QString getXrayOutboundDomainStrategy() {
        const auto strategy = getDirectDomainStrategy();
        if (strategy == "prefer_ipv4") return "UseIPv4v6";
        if (strategy == "prefer_ipv6") return "UseIPv6v4";
        if (strategy == "ipv6_only") return "ForceIPv6";
        // The cap narrows the family; it must not also make resolution mandatory.
        if (strategy == "ipv4_only") {
            return Configs::dataManager->settingsRepo->default_domain_strategy == "ipv4_only" ? "ForceIPv4" : "UseIPv4";
        }
        return "UseIP";
    }

    BuildResult xrayStreamSetting::Build() {
        // Default-NIC binding and domain resolution are wired on at instance creation (ThroneWiring), not here.
        return {ExportToJson(), ""};
    }

    namespace Checks {
        QString XrayNetwork(const QString& network)
        {
            const auto t = network.trimmed();
            if (t.isEmpty() || XrayNetworks.contains(NormalizeXrayNetwork(t))) return {};
            return QCoreApplication::translate("Validation", "Throne does not support the Xray transport \"%1\". Use raw, xhttp, ws, httpupgrade or grpc.").arg(t);
        }

        QString XraySecurity(const QString& security)
        {
            static const QStringList known = {"", "none", "tls", "reality"};
            const auto t = security.trimmed();
            if (known.contains(t.toLower())) return {};
            return QCoreApplication::translate("Validation", "Xray does not support the security \"%1\". Use none, tls or reality.").arg(t);
        }

        QString XrayRealityNetwork(const QString& security, const QString& network)
        {
            const auto t = NormalizeXrayNetwork(network.trimmed());
            if (security.trimmed().toLower() != "reality" || t.isEmpty() || t == "raw" || t == "xhttp" || t == "grpc") return {};
            return QCoreApplication::translate("Validation", "REALITY works only with the raw, xhttp or grpc transport.");
        }

        QString XrayFingerprint(const QString& fingerprint, bool reality)
        {
            const auto t = fingerprint.trimmed().toLower();
            if (t.isEmpty()) return {};
            // xrayReality::Build drops "unsafe"; Xray refuses hellogolang for REALITY.
            if (reality && t == "hellogolang") return QCoreApplication::translate("Validation", "REALITY cannot use the hellogolang fingerprint; choose another one.");
            if (xrayStreamFingerprints().contains(t)) return {};
            return QCoreApplication::translate("Validation", "Xray does not know the fingerprint \"%1\". Use chrome, firefox, safari, ios, android, edge, 360, qq, random, randomized or unsafe.").arg(fingerprint.trimmed());
        }

        QString XrayCipherSuites(const QString& text)
        {
            for (const auto& name : splitCipherSuites(text)) {
                if (tlsSecureCipherSuites.contains(name) || tlsInsecureCipherSuites.contains(name)) continue;
                return QCoreApplication::translate("Validation", "Xray ignores the unknown cipher suite \"%1\".").arg(name);
            }
            return {};
        }

        QString XrayCipherSuitesNeedUnsafe(const QString& cipherSuites, const QString& fingerprint)
        {
            if (splitCipherSuites(cipherSuites).isEmpty() || fingerprint.trimmed().toLower() == tlsUnsafeFingerprint) return {};
            return QCoreApplication::translate("Validation", "Cipher suites only take effect with the unsafe fingerprint.");
        }

        QString XrayPinnedSha256(const QString& text)
        {
            return Validation::EachItem(text, Validation::Split::Comma, [](const QString& item) -> QString {
                if (Validation::Hex(QString(item).remove(':'), 64, 64, true, Validation::Empty::Required).isEmpty()) return {};
                return QCoreApplication::translate("Validation", "Must be a SHA-256 in hexadecimal (64 characters, colons allowed).");
            });
        }

        QString XrayEchConfigList(const QString& text, const QString& serverName)
        {
            const auto t = text.trimmed();
            if (t.isEmpty()) return {};
            if (!t.contains(QLatin1String("://"))) {
                if (Validation::Base64(t).isEmpty()) return {};
                return QCoreApplication::translate("Validation", "Must be a base64 ECHConfigList, or domain+https://dns/dns-query to fetch it over DNS.");
            }
            // Xray transport/internet/tls/ech.go: "name+server", else the SNI when it is a domain.
            QString name;
            if (const auto plus = t.indexOf('+'); plus >= 0) {
                name = t.left(plus).trimmed();
            } else if (const auto sni = serverName.trimmed(); !sni.isEmpty() && !Validation::Ip(QString(sni).remove('[').remove(']')).isEmpty()) {
                name = sni;
            }
            if (!name.isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Fetching ECH over DNS needs a domain SNI; otherwise write it as domain+https://dns/dns-query.");
        }

        QString XraySpiderX(const QString& spiderX)
        {
            const auto t = spiderX.trimmed();
            if (t.isEmpty() || t.startsWith('/')) return {};
            return QCoreApplication::translate("Validation", "Must start with /, for example /search?q=1.");
        }

        QString XrayMldsa65Verify(const QString& key)
        {
            // Xray infra/conf/transport_security.go: RawURLEncoding of a 1952-byte ML-DSA-65 public key.
            return Validation::Base64Key(key, 1952, true, false, Validation::Empty::Allowed);
        }

        QString XrayHeadersHost(const QStringList& pairs)
        {
            for (qsizetype i = 0; i < pairs.size(); i += 2) {
                if (pairs[i].trimmed().compare(QLatin1String("host"), Qt::CaseInsensitive) == 0) {
                    return QCoreApplication::translate("Validation", "Remove the Host header and put the host in the Host field instead.");
                }
            }
            return {};
        }

        QString XrayInt32Range(const QString& text)
        {
            return Validation::IntOrRange(text, INT_MIN, INT_MAX, Validation::Empty::Allowed, true);
        }

        QString XrayXPaddingBytes(const QString& text)
        {
            qint64 from = 0, to = 0;
            if (!xrayStreamRange(text, from, to)) return XrayInt32Range(text);
            if ((from == 0 && to == 0) || (from > 0 && to > 0)) return {};
            return QCoreApplication::translate("Validation", "Both bounds must be greater than 0, for example 100-1000.");
        }

        QString XrayXHTTPMode(const QString& mode)
        {
            return Validation::OneOf(mode, QStringList{""} + XrayXHTTPModes);
        }

        QString XrayPaddingMethod(const QString& method)
        {
            return Validation::OneOf(method, {"", "repeat-x", "tokenish"});
        }

        QString XrayPaddingPlacement(const QString& placement)
        {
            return Validation::OneOf(placement, {"", "queryInHeader", "cookie", "header", "query"});
        }

        QString XrayMetaPlacement(const QString& placement)
        {
            return Validation::OneOf(placement, XrayXHTTPMetaPlacements);
        }

        QString XrayUplinkDataPlacement(const QString& placement, const QString& mode)
        {
            if (auto message = Validation::OneOf(placement, XrayXHTTPUplinkDataPlacements); !message.isEmpty()) return message;
            const auto t = placement.trimmed();
            // Literally packet-up: Xray checks before it resolves auto.
            if ((t == "cookie" || t == "header") && mode.trimmed() != "packet-up") {
                return QCoreApplication::translate("Validation", "Cookie and header placement need the packet-up mode.");
            }
            return {};
        }

        QString XrayUplinkMethod(const QString& method, const QString& mode)
        {
            if (method.trimmed().toUpper() != "GET" || mode.trimmed() == "packet-up") return {};
            return QCoreApplication::translate("Validation", "GET uploads need the packet-up mode.");
        }

        QString XraySessionId(const QString& table, const QString& length)
        {
            static const QHash<QString, QString> predefined = {
                {"ALPHABET", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
                {"Alphabet", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"},
                {"BASE36", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
                {"Base62", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"},
                {"HEX", "0123456789ABCDEF"},
                {"alphabet", "abcdefghijklmnopqrstuvwxyz"},
                {"base36", "0123456789abcdefghijklmnopqrstuvwxyz"},
                {"hex", "0123456789abcdef"},
                {"number", "0123456789"},
            };
            if (table.isEmpty()) return {};
            const auto bytes = predefined.value(table, table).toUtf8();
            qint64 from = 0, to = 0;
            if (!xrayStreamRange(length, from, to)) return {};
            if (from <= 0) return QCoreApplication::translate("Validation", "Set a session ID length that starts above 0, for example 8-16.");
            // Xray roomSize: the sum of n^k over the length range must reach 2^31.
            constexpr double kRoom = 2147483648.0;
            double room = 0;
            if (bytes.size() == 1) {
                room = double(to - from + 1);
            } else {
                for (qint64 k = from; k <= to && room < kRoom; ++k) room += std::pow(double(bytes.size()), double(k));
            }
            if (room < kRoom) {
                return QCoreApplication::translate("Validation", "The table and length allow too few session IDs; use a longer length (for example 8-16) or a larger table.");
            }
            if (std::any_of(bytes.cbegin(), bytes.cend(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; })) {
                return QCoreApplication::translate("Validation", "The table must contain only ASCII characters.");
            }
            return {};
        }

        QString XrayXmuxExclusive(const QString& maxConnections, const QString& maxConcurrency)
        {
            qint64 fromA = 0, toA = 0, fromB = 0, toB = 0;
            if (!xrayStreamRange(maxConnections, fromA, toA) || !xrayStreamRange(maxConcurrency, fromB, toB)) return {};
            if (toA <= 0 || toB <= 0) return {};
            return QCoreApplication::translate("Validation", "Set either Max Connections or Max Concurrency, not both.");
        }

        void XrayXHTTPDownloadLeg(Validation::Issues& issues, const QString& at, const QJsonObject& outbound)
        {
            auto copy = outbound;
            xrayStreamEditLive(xrayStreamRoot(copy, at), QStringLiteral("streamSettings"), false, [&](const QList<XrayStreamNode>& streams) {
                if (!xrayStreamDialsXHTTP(streams)) return;
                xrayStreamEditDownloadLeg(streams, [&](const QList<XrayStreamNode>& leg) { xrayStreamDownloadEndpoint(issues, leg); });
            });
        }
    }

    void xrayStreamSetting::Validate(Validation::Issues& issues, const QString& serverAddress)
    {
        using Validation::Severity;
        const QString root = QStringLiteral("streamSettings.");
        xrayStreamAdd(issues, Severity::Error, root + "network", Checks::XrayNetwork(network), network);
        xrayStreamAdd(issues, Severity::Error, root + "security", Checks::XraySecurity(security), security);
        xrayStreamAdd(issues, Severity::Error, root + "security", Checks::XrayRealityNetwork(security, network));

        const auto mode = security.trimmed().toLower();
        if (mode == "tls") {
            const QString key = root + "tlsSettings.";
            const auto fingerprint = TLS->fingerprint.isEmpty() ? Configs::dataManager->settingsRepo->utlsFingerprint : TLS->fingerprint;
            xrayStreamAdd(issues, Severity::Warning, key + "serverName", Checks::ServerName(TLS->serverName), TLS->serverName);
            xrayStreamAdd(issues, Severity::Error, key + "fingerprint", Checks::XrayFingerprint(TLS->fingerprint, false), TLS->fingerprint);
            xrayStreamAdd(issues, Severity::Error, key + "pinnedPeerCertSha256", Checks::XrayPinnedSha256(TLS->pinnedPeerCertSha256));
            xrayStreamAdd(issues, Severity::Warning, key + "cipherSuites", Checks::XrayCipherSuites(TLS->cipherSuites));
            xrayStreamAdd(issues, Severity::Warning, key + "cipherSuites", Checks::XrayCipherSuitesNeedUnsafe(TLS->cipherSuites, fingerprint));
            xrayStreamAdd(issues, Severity::Warning, key + "echConfigList",
                          Checks::XrayEchConfigList(TLS->echConfigList, TLS->serverName.isEmpty() ? serverAddress : TLS->serverName));
        } else if (mode == "reality") {
            const QString key = root + "realitySettings.";
            xrayStreamAdd(issues, Severity::Warning, key + "serverName", Checks::ServerName(reality->serverName), reality->serverName);
            xrayStreamAdd(issues, Severity::Error, key + "fingerprint", Checks::XrayFingerprint(reality->fingerprint, true), reality->fingerprint);
            xrayStreamAdd(issues, Severity::Error, key + "password", Checks::RealityPublicKey(reality->password, true));
            xrayStreamAdd(issues, Severity::Error, key + "shortId", Checks::RealityShortId(reality->shortId));
            xrayStreamAdd(issues, Severity::Error, key + "spiderX", Checks::XraySpiderX(reality->spiderX), reality->spiderX);
            xrayStreamAdd(issues, Severity::Error, key + "mldsa65Verify", Checks::XrayMldsa65Verify(reality->mldsa65Verify));
        }

        const auto net = NormalizeXrayNetwork(network);
        if (net == "httpupgrade") {
            xrayStreamAdd(issues, Severity::Error, root + "httpupgradeSettings.headers", Checks::XrayHeadersHost(httpupgrade->headers));
        }
        if (net != "xhttp") return;
        const auto& x = *xhttp;
        const QString extra = root + "xhttpSettings.extra.";
        xrayStreamAdd(issues, Severity::Error, root + "xhttpSettings.mode", Checks::XrayXHTTPMode(x.mode), x.mode);
        xrayStreamAdd(issues, Severity::Error, extra + "headers", Checks::XrayHeadersHost(x.headers));
        xrayStreamAdd(issues, Severity::Error, extra + "xPaddingBytes", Checks::XrayXPaddingBytes(x.xPaddingBytes), x.xPaddingBytes);
        xrayStreamAdd(issues, Severity::Error, extra + "xPaddingMethod", Checks::XrayPaddingMethod(x.xPaddingMethod), x.xPaddingMethod);
        xrayStreamAdd(issues, Severity::Error, extra + "xPaddingPlacement", Checks::XrayPaddingPlacement(x.xPaddingPlacement), x.xPaddingPlacement);
        xrayStreamAdd(issues, Severity::Error, extra + "uplinkHTTPMethod", Checks::XrayUplinkMethod(x.uplinkHTTPMethod, x.mode), x.uplinkHTTPMethod);
        xrayStreamAdd(issues, Severity::Warning, extra + "uplinkHTTPMethod", Validation::HttpToken(x.uplinkHTTPMethod), x.uplinkHTTPMethod);
        xrayStreamAdd(issues, Severity::Error, extra + "uplinkDataPlacement", Checks::XrayUplinkDataPlacement(x.uplinkDataPlacement, x.mode), x.uplinkDataPlacement);
        xrayStreamAdd(issues, Severity::Error, extra + "sessionIDPlacement", Checks::XrayMetaPlacement(x.sessionIDPlacement), x.sessionIDPlacement);
        xrayStreamAdd(issues, Severity::Error, extra + "seqPlacement", Checks::XrayMetaPlacement(x.seqPlacement), x.seqPlacement);
        xrayStreamAdd(issues, Severity::Error, extra + "sessionIDTable", Checks::XraySessionId(x.sessionIDTable, x.sessionIDLength));
        if (x.serverMaxHeaderBytes < 0) {
            xrayStreamAdd(issues, Severity::Error, extra + "serverMaxHeaderBytes", Validation::Int(QString::number(x.serverMaxHeaderBytes), 0, INT_MAX));
        }
        for (const auto& [name, value] : {std::pair{QStringLiteral("sessionIDLength"), x.sessionIDLength},
                                          std::pair{QStringLiteral("uplinkChunkSize"), x.uplinkChunkSize},
                                          std::pair{QStringLiteral("scMaxEachPostBytes"), x.scMaxEachPostBytes},
                                          std::pair{QStringLiteral("scMinPostsIntervalMs"), x.scMinPostsIntervalMs},
                                          std::pair{QStringLiteral("scStreamUpServerSecs"), x.scStreamUpServerSecs},
                                          std::pair{QStringLiteral("xmux.maxConcurrency"), x.maxConcurrency},
                                          std::pair{QStringLiteral("xmux.maxConnections"), x.maxConnections},
                                          std::pair{QStringLiteral("xmux.cMaxReuseTimes"), x.cMaxReuseTimes},
                                          std::pair{QStringLiteral("xmux.hMaxRequestTimes"), x.hMaxRequestTimes},
                                          std::pair{QStringLiteral("xmux.hMaxReusableSecs"), x.hMaxReusableSecs}}) {
            xrayStreamAdd(issues, Severity::Error, extra + name, Checks::XrayInt32Range(value), value);
        }
        xrayStreamAdd(issues, Severity::Error, extra + "xmux.maxConnections", Checks::XrayXmuxExclusive(x.maxConnections, x.maxConcurrency));
        Checks::XrayXHTTPDownloadLeg(issues, {}, QJsonObject{{"streamSettings", QJsonObject{{"network", "xhttp"}, {"xhttpSettings", xhttp->ExportToJson()}}}});
    }
}
