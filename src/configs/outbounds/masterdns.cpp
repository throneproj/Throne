#include "include/configs/outbounds/masterdns.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include <climits>
#include <cmath>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/DnsTunnel.h"
#include "include/configs/common/utils.h"
#include "include/global/Utils.hpp"

namespace Configs {
    namespace {
        constexpr qsizetype kMasterDnsMaxLinkSize = 64 * 1024;
        constexpr int kMasterDnsMaxSetupDuplication = 12;
        // RESOLVER_BALANCING_STRATEGY ids: 0 and 2 are both round robin.
        const QStringList kMasterDnsBalancingIds = {"round_robin", "random", "round_robin", "least_loss", "lowest_latency", "hybrid",
                                                    "loss_then_latency", "least_loss_top_random", "least_loss_top_round_robin"};

        Validation::DurationRule masterDnsPositiveDuration()
        {
            Validation::DurationRule rule;
            rule.minNs = 1'000'000;
            return rule;
        }

        const QRegularExpression &masterDnsSeparators()
        {
            static const QRegularExpression separators(QStringLiteral("[,;\\s]+"));
            return separators;
        }

        QStringList masterDnsDomains(const QStringList& items)
        {
            QStringList domains;
            for (const auto& item : items) {
                const auto domain = toAceHost(item.trimmed());
                if (!domain.isEmpty() && !domains.contains(domain)) domains << domain;
            }
            return domains;
        }

        void masterDnsWriteCommon(const masterdns& bean, QJsonObject& object)
        {
            object["domain"] = QListStr2QJsonArray(bean.domains);
            object["mode"] = bean.mode.isEmpty() ? QStringLiteral("socks5") : bean.mode;
            object["encryption_method"] = bean.encryption_method.isEmpty() ? QStringLiteral("xor") : bean.encryption_method;
            if (!bean.encryption_key.isEmpty()) object["encryption_key"] = bean.encryption_key;
            if (bean.base64_response) object["base64_response"] = true;
            if (!bean.upload_compression.isEmpty()) object["upload_compression"] = bean.upload_compression;
            if (!bean.download_compression.isEmpty()) object["download_compression"] = bean.download_compression;
            if (!bean.balancing_strategy.isEmpty()) object["balancing_strategy"] = bean.balancing_strategy;
            if (bean.packet_duplication > 0) object["packet_duplication"] = bean.packet_duplication;
            if (bean.setup_packet_duplication > 0) object["setup_packet_duplication"] = bean.setup_packet_duplication;
            QJsonObject mtu;
            if (bean.mtu_min_upload > 0) mtu["min_upload"] = bean.mtu_min_upload;
            if (bean.mtu_max_upload > 0) mtu["max_upload"] = bean.mtu_max_upload;
            if (bean.mtu_min_download > 0) mtu["min_download"] = bean.mtu_min_download;
            if (bean.mtu_max_download > 0) mtu["max_download"] = bean.mtu_max_download;
            if (bean.mtu_test_retries > 0) mtu["test_retries"] = bean.mtu_test_retries;
            if (const auto timeout = bean.mtu_test_timeout.trimmed(); !timeout.isEmpty()) mtu["test_timeout"] = timeout;
            if (bean.mtu_test_parallelism > 0) mtu["test_parallelism"] = bean.mtu_test_parallelism;
            if (!bean.mtu_auto_remove_low) mtu["auto_remove_low"] = false;
            if (!mtu.isEmpty()) object["mtu"] = mtu;
        }

        // DATA_ENCRYPTION_METHOD as links carry it; MasterDnsVPN's default is 1, XOR.
        bool masterDnsReadMethod(masterdns& bean, const QString& text)
        {
            const auto value = text.trimmed();
            if (value.isEmpty()) {
                bean.encryption_method = "xor";
                return true;
            }
            const auto id = Validation::ToInt(value);
            if (!id || *id < 0 || *id >= masterDnsEncryptionMethods.size()) {
                ParseDiag::Error("encryption_method", QCoreApplication::translate("Validation", "Unknown encryption method: use a number from 0 to 5."), value.left(16));
                return false;
            }
            bean.encryption_method = masterDnsEncryptionMethods[int(*id)];
            return true;
        }

        void masterDnsReadId(const QString& field, const QString& text, const QStringList& names, bool zeroIsDefault, QString& target)
        {
            const auto value = text.trimmed();
            if (value.isEmpty() || !AcceptOptional(field, Validation::Int(value, 0, names.size() - 1), value)) return;
            const int id = value.toInt();
            target = zeroIsDefault && id == 0 ? QString() : names[id];
        }

        void masterDnsReadCount(const QString& field, const QString& text, int min, int max, int& target)
        {
            const auto value = text.trimmed();
            if (!value.isEmpty() && AcceptOptional(field, Validation::Int(value, min, max), value)) target = value.toInt();
        }

        void masterDnsReadBool(const QString& field, const QJsonValue& value, bool& target)
        {
            if (value.isBool()) target = value.toBool();
            else AcceptOptional(field, QCoreApplication::translate("Validation", "Use true or false."), value.toVariant().toString().left(32));
        }

        bool masterDnsParseDnst(masterdns& bean, const QString& link)
        {
            DnstLink parsed;
            QString error;
            if (!DecodeDnstLink(link, parsed, &error)) {
                ParseDiag::Error("link", error);
                return false;
            }
            if (parsed.transport != QLatin1String("masterdns")) {
                ParseDiag::Error("mode", QCoreApplication::translate("Validation", "This DNS tunnel type is not supported."), parsed.transport);
                return false;
            }
            bean.name = parsed.tag;
            bean.domains = masterDnsDomains({parsed.domain});
            bean.encryption_key = parsed.params.value("mkey");
            if (!masterDnsReadMethod(bean, parsed.params.value("encryption"))) return false;
            if (parsed.backend == QLatin1String("socks")) {
                bean.mode = "socks5";
                if (!parsed.params.value("user").isEmpty() || !parsed.params.value("password").isEmpty()) {
                    ParseDiag::Warn("mode", QCoreApplication::translate("Validation", "The SOCKS user name and password only guard a local listener, which this profile does not open; they were ignored."));
                }
            } else if (parsed.backend == QLatin1String("custom")) {
                bean.mode = "tcp";
            } else if (parsed.backend.isEmpty()) {
                ParseDiag::Error("mode", QCoreApplication::translate("Validation", "The link does not name the service behind the tunnel."));
                return false;
            } else {
                ParseDiag::Error("mode", QCoreApplication::translate("Validation", "MasterDNS only carries a SOCKS or a custom service."), parsed.backend);
                return false;
            }
            if (parsed.hasResolvers) bean.resolvers.lines = ImportResolverLines(parsed.resolvers, ResolverFlavor::MasterDns, "resolvers");
            return true;
        }

        // yptun: v1?domains=&key=&enc=&resolvers=[&bal=][&dup=][&proxy=]#name, lists split on commas, semicolons or spaces.
        bool masterDnsParseYptun(masterdns& bean, QString rest)
        {
            if (const auto hash = rest.indexOf(u'#'); hash >= 0) {
                bean.name = QUrl::fromPercentEncoding(rest.mid(hash + 1).toUtf8()).trimmed();
                rest = rest.left(hash);
            }
            if (rest.startsWith(u'?')) rest.remove(0, 1);
            QHash<QString, QString> query;
            for (const auto& pair : rest.split(u'&', Qt::SkipEmptyParts)) {
                const auto eq = pair.indexOf(u'=');
                const auto key = QUrl::fromPercentEncoding((eq < 0 ? pair : pair.left(eq)).toUtf8()).trimmed().toLower();
                if (!query.contains(key)) query.insert(key, eq < 0 ? QString() : QUrl::fromPercentEncoding(pair.mid(eq + 1).toUtf8()));
            }
            const auto& separators = masterDnsSeparators();
            bean.domains = masterDnsDomains(query.value("domains").split(separators, Qt::SkipEmptyParts));
            bean.encryption_key = query.value("key");
            if (!masterDnsReadMethod(bean, query.value("enc"))) return false;
            if (query.contains("resolvers")) {
                bean.resolvers.lines = ImportResolverLines(query.value("resolvers").split(separators, Qt::SkipEmptyParts), ResolverFlavor::MasterDns, "resolvers");
            }
            // 0 leaves both to the client's default.
            masterDnsReadId("balancing_strategy", query.value("bal"), kMasterDnsBalancingIds, true, bean.balancing_strategy);
            masterDnsReadCount("packet_duplication", query.value("dup"), 0, 10, bean.packet_duplication);
            if (!query.value("proxy").trimmed().isEmpty()) {
                ParseDiag::Warn("link", QCoreApplication::translate("Validation", "The link also carries a proxy to run over the tunnel; import that proxy separately and chain it over this profile."));
            }
            return true;
        }

        // WhiteDNS (whitedns.profile) and Hidden-Node (masterdns.profile, stormdns.profile) share this JSON, schema version 1.
        bool masterDnsParseProfile(masterdns& bean, QString payload)
        {
            for (const QChar mark : {u'#', u'?'}) {
                if (const auto at = payload.indexOf(mark); at >= 0) payload = payload.left(at);
            }
            const auto bytes = DnsTunnelBase64(payload.trimmed());
            if (bytes.isEmpty()) {
                ParseDiag::Error("link", QCoreApplication::translate("Validation", "The link is damaged: its payload is not valid base64."));
                return false;
            }
            const auto document = QJsonDocument::fromJson(bytes);
            if (!document.isObject()) {
                ParseDiag::Error("link", QCoreApplication::translate("Validation", "The link is damaged: its payload is not a JSON object."));
                return false;
            }
            const auto root = document.object();
            const auto schema = root.value("schema").toString().trimmed().toLower();
            static const QStringList schemas = {"whitedns.profile", "masterdns.profile", "stormdns.profile"};
            if (!schemas.contains(schema)) {
                ParseDiag::Error("link", QCoreApplication::translate("Validation", "This MasterDNS link format is not supported."), schema.left(64));
                return false;
            }
            if (const auto version = root.value("version").toVariant().toString().trimmed(); version != QLatin1String("1")) {
                ParseDiag::Error("link", QCoreApplication::translate("Validation", "This MasterDNS link version is not supported."), version.left(16));
                return false;
            }
            const auto profile = root.value("profile").toObject();
            const auto server = profile.value("server").toObject();
            const auto pick = [&server](const QString& snake, const QString& camel) {
                return server.contains(snake) ? server.value(snake) : server.value(camel);
            };
            bean.name = profile.value("name").toString().trimmed();
            QStringList domains;
            if (server.contains("domain")) domains << server.value("domain").toString();
            if (server.contains("domains")) domains << JsonStringList(server.value("domains"), "domain");
            bean.domains = masterDnsDomains(domains);
            bean.encryption_key = pick("encryption_key", "encryptionKey").toString();
            const auto method = pick("encryption_method", "encryptionMethod");
            return masterDnsReadMethod(bean, method.isDouble() ? QString::number(method.toDouble()) : method.toString());
        }

        // The TOML subset MasterDnsVPN client configs use: KEY = string, number, boolean or a flat array of them, and # comments.
        class MasterDnsToml {
        public:
            explicit MasterDnsToml(const QString& text) : text_(text) {}

            bool read(QHash<QString, QJsonValue>& values, QStringList& order)
            {
                for (;;) {
                    skipSpace(true);
                    if (atEnd()) return true;
                    if (text_[pos_] == u'[') {
                        while (!atEnd() && text_[pos_] != u'\n') ++pos_;
                        continue;
                    }
                    const auto start = pos_;
                    while (!atEnd() && (text_[pos_].isLetterOrNumber() || text_[pos_] == u'_' || text_[pos_] == u'-')) ++pos_;
                    const auto key = text_.mid(start, pos_ - start);
                    skipSpace(false);
                    if (key.isEmpty() || atEnd() || text_[pos_] != u'=') return false;
                    ++pos_;
                    QJsonValue value;
                    if (!readValue(value, false)) return false;
                    skipSpace(false);
                    if (!atEnd() && text_[pos_] != u'\n') return false;
                    if (!values.contains(key)) {
                        if (order.size() >= 512) return false;
                        order << key;
                    }
                    values.insert(key, value);
                }
            }

        private:
            const QString& text_;
            qsizetype pos_ = 0;

            [[nodiscard]] bool atEnd() const { return pos_ >= text_.size(); }

            static bool isDelimiter(QChar c)
            {
                return c == u',' || c == u']' || c == u'#' || c == u'\n' || c == u'\r' || c == u'\t' || c == u' ';
            }

            void skipSpace(bool newlines)
            {
                while (!atEnd()) {
                    const auto c = text_[pos_];
                    if (c == u'#') {
                        while (!atEnd() && text_[pos_] != u'\n') ++pos_;
                    } else if (c == u' ' || c == u'\t' || c == u'\r' || (newlines && c == u'\n')) {
                        ++pos_;
                    } else {
                        return;
                    }
                }
            }

            bool readString(QString& out)
            {
                const auto quote = text_[pos_++];
                out.clear();
                while (!atEnd()) {
                    const auto c = text_[pos_++];
                    if (c == quote) return true;
                    if (c == u'\n') return false;
                    if (c != u'\\' || quote == u'\'') {
                        out += c;
                        continue;
                    }
                    if (atEnd()) return false;
                    const char16_t escape = text_[pos_++].unicode();
                    switch (escape) {
                        case u'n': out += u'\n'; break;
                        case u't': out += u'\t'; break;
                        case u'r': out += u'\r'; break;
                        case u'b': out += u'\b'; break;
                        case u'f': out += u'\f'; break;
                        case u'"': out += u'"'; break;
                        case u'\\': out += u'\\'; break;
                        case u'u':
                        case u'U': {
                            const int digits = escape == u'u' ? 4 : 8;
                            bool ok = false;
                            const uint code = pos_ + digits <= text_.size() ? text_.mid(pos_, digits).toUInt(&ok, 16) : 0;
                            if (!ok || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
                            pos_ += digits;
                            const char32_t ucs4 = code;
                            out += QString::fromUcs4(&ucs4, 1);
                            break;
                        }
                        default:
                            return false;
                    }
                }
                return false;
            }

            bool readValue(QJsonValue& out, bool inArray)
            {
                skipSpace(inArray);
                if (atEnd()) return false;
                const auto c = text_[pos_];
                if (c == u'"' || c == u'\'') {
                    QString text;
                    if (!readString(text)) return false;
                    out = text;
                    return true;
                }
                if (c == u'[') {
                    if (inArray) return false;
                    ++pos_;
                    QJsonArray array;
                    for (;;) {
                        skipSpace(true);
                        if (atEnd()) return false;
                        if (text_[pos_] == u']') break;
                        QJsonValue item;
                        if (array.size() >= 4096 || !readValue(item, true)) return false;
                        array.append(item);
                        skipSpace(true);
                        if (atEnd()) return false;
                        if (text_[pos_] == u',') ++pos_;
                        else if (text_[pos_] != u']') return false;
                    }
                    ++pos_;
                    out = array;
                    return true;
                }
                const auto start = pos_;
                while (!atEnd() && !isDelimiter(text_[pos_])) ++pos_;
                const auto word = text_.mid(start, pos_ - start);
                if (word == QLatin1String("true") || word == QLatin1String("false")) {
                    out = word == QLatin1String("true");
                    return true;
                }
                bool ok = false;
                const double number = QString(word).remove(u'_').toDouble(&ok);
                if (!ok || !std::isfinite(number)) return false;
                out = number;
                return true;
            }
        };

        bool masterDnsLocalKey(const QString& key)
        {
            static const QSet<QString> keys = {"LOG_LEVEL", "MTU_USING_SECTION_SEPARATOR_TEXT", "MTU_SERVERS_FILE_NAME", "FD_CONTROL_UNIX_SOCKET",
                                              "RESOLVERS_FILE_PATH"};
            return keys.contains(key) || key.startsWith(QLatin1String("LISTEN_")) || key.startsWith(QLatin1String("SOCKS5_")) ||
                   key.startsWith(QLatin1String("LOCAL_DNS_")) || key.startsWith(QLatin1String("SAVE_MTU_")) ||
                   (key.startsWith(QLatin1String("MTU_")) && key.endsWith(QLatin1String("_FORMAT")));
        }

        // lucx-ui: v1/<base64url of the client TOML>, which never carries resolvers.
        bool masterDnsParseToml(masterdns& bean, QString payload)
        {
            if (const auto hash = payload.indexOf(u'#'); hash >= 0) {
                bean.name = QUrl::fromPercentEncoding(payload.mid(hash + 1).toUtf8()).trimmed();
                payload = payload.left(hash);
            }
            const auto bytes = DnsTunnelBase64(payload.trimmed());
            if (bytes.isEmpty()) {
                ParseDiag::Error("link", QCoreApplication::translate("Validation", "The link is damaged: its payload is not valid base64."));
                return false;
            }
            const auto text = QString::fromUtf8(bytes);
            QHash<QString, QJsonValue> values;
            QStringList order;
            if (!MasterDnsToml(text).read(values, order)) {
                ParseDiag::Error("link", QCoreApplication::translate("Validation", "The link is damaged: its client configuration cannot be read."));
                return false;
            }
            const auto textOf = [](const QJsonValue& value) {
                if (value.isString()) return value.toString();
                if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
                if (value.isDouble()) return QString::number(value.toDouble(), 'g', 15);
                return QString();
            };
            QStringList unsupported;
            for (const auto& key : order) {
                const auto upper = key.toUpper();
                const auto value = values.value(key);
                const auto text = textOf(value).trimmed();
                if (upper == QLatin1String("DOMAINS")) {
                    QStringList items;
                    if (value.isArray()) {
                        for (const auto& item : value.toArray()) {
                            if (item.isString()) items << item.toString();
                        }
                    } else {
                        items = text.split(masterDnsSeparators(), Qt::SkipEmptyParts);
                    }
                    bean.domains = masterDnsDomains(items);
                } else if (upper == QLatin1String("DATA_ENCRYPTION_METHOD")) {
                    if (!masterDnsReadMethod(bean, text)) return false;
                } else if (upper == QLatin1String("ENCRYPTION_KEY")) {
                    bean.encryption_key = textOf(value);
                } else if (upper == QLatin1String("PROTOCOL_TYPE")) {
                    if (text.compare(QLatin1String("SOCKS5"), Qt::CaseInsensitive) == 0) {
                        bean.mode = "socks5";
                    } else if (text.compare(QLatin1String("TCP"), Qt::CaseInsensitive) == 0) {
                        bean.mode = "tcp";
                    } else {
                        ParseDiag::Error("mode", QCoreApplication::translate("Validation", "Unknown protocol type: use SOCKS5 or TCP."), text.left(32));
                        return false;
                    }
                } else if (upper == QLatin1String("BASE_ENCODE_DATA")) {
                    masterDnsReadBool("base64_response", value, bean.base64_response);
                } else if (upper == QLatin1String("UPLOAD_COMPRESSION_TYPE")) {
                    masterDnsReadId("upload_compression", text, masterDnsCompressions, false, bean.upload_compression);
                } else if (upper == QLatin1String("DOWNLOAD_COMPRESSION_TYPE")) {
                    masterDnsReadId("download_compression", text, masterDnsCompressions, false, bean.download_compression);
                } else if (upper == QLatin1String("RESOLVER_BALANCING_STRATEGY")) {
                    masterDnsReadId("balancing_strategy", text, kMasterDnsBalancingIds, false, bean.balancing_strategy);
                } else if (upper == QLatin1String("PACKET_DUPLICATION_COUNT")) {
                    masterDnsReadCount("packet_duplication", text, 1, 10, bean.packet_duplication);
                } else if (upper == QLatin1String("SETUP_PACKET_DUPLICATION_COUNT")) {
                    masterDnsReadCount("setup_packet_duplication", text, 1, kMasterDnsMaxSetupDuplication, bean.setup_packet_duplication);
                } else if (upper == QLatin1String("MIN_UPLOAD_MTU")) {
                    masterDnsReadCount("mtu_min_upload", text, 0, 65535, bean.mtu_min_upload);
                } else if (upper == QLatin1String("MAX_UPLOAD_MTU")) {
                    masterDnsReadCount("mtu_max_upload", text, 0, 65535, bean.mtu_max_upload);
                } else if (upper == QLatin1String("MIN_DOWNLOAD_MTU")) {
                    masterDnsReadCount("mtu_min_download", text, 0, 65535, bean.mtu_min_download);
                } else if (upper == QLatin1String("MAX_DOWNLOAD_MTU")) {
                    masterDnsReadCount("mtu_max_download", text, 0, 65535, bean.mtu_max_download);
                } else if (upper == QLatin1String("AUTO_REMOVE_LOW_MTU_SERVERS")) {
                    masterDnsReadBool("mtu_auto_remove_low", value, bean.mtu_auto_remove_low);
                } else if (upper == QLatin1String("MTU_TEST_RETRIES")) {
                    masterDnsReadCount("mtu_test_retries", text, 0, 1000, bean.mtu_test_retries);
                } else if (upper == QLatin1String("MTU_TEST_TIMEOUT")) {
                    bool ok = false;
                    const double seconds = text.toDouble(&ok);
                    const bool valid = ok && seconds > 0 && seconds < 86400;
                    if (AcceptOptional("mtu_test_timeout", valid ? QString() : QCoreApplication::translate("Validation", "Use a number of seconds above 0."), text.left(32)))
                        bean.mtu_test_timeout = QString::number(seconds, 'g', 15) + QLatin1Char('s');
                } else if (upper == QLatin1String("MTU_TEST_PARALLELISM")) {
                    masterDnsReadCount("mtu_test_parallelism", text, 1, 4096, bean.mtu_test_parallelism);
                } else if (!masterDnsLocalKey(upper)) {
                    unsupported << key.left(64);
                }
            }
            if (!unsupported.isEmpty()) {
                ParseDiag::Warn("link", QCoreApplication::translate("Validation", "Settings this profile does not support were ignored."), unsupported.join(", ").left(256));
            }
            return true;
        }
    }

    namespace Checks {
        QString MasterDnsEncryptionKey(const QString& method, const QString& key)
        {
            if (method == QLatin1String("none") || !key.isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Enter the encryption key the server uses.");
        }

        QString MasterDnsDuplication(int duplication)
        {
            return Validation::Int(QString::number(duplication), 0, 10);
        }

        QString MasterDnsMtuRange(int min, int max, int defaultMin, int defaultMax)
        {
            if (min < 0 || max < 0) return QCoreApplication::translate("Validation", "Must not be negative.");
            const int low = min > 0 ? min : defaultMin;
            const int high = max > 0 ? max : defaultMax;
            if (low <= high) return {};
            return QCoreApplication::translate("Validation", "The minimum MTU %1 is above the maximum %2.").arg(low).arg(high);
        }
    }

    bool masterdns::ParseFromLink(const QString& link)
    {
        if (link.size() > kMasterDnsMaxLinkSize) {
            ParseDiag::Error("link", QCoreApplication::translate("Validation", "The link is too long."));
            return false;
        }
        if (link.startsWith(QLatin1String("dnst://"), Qt::CaseInsensitive)) return masterDnsParseDnst(*this, link);
        const auto separator = link.indexOf(QLatin1String("://"));
        if (separator < 0) return false;
        const auto rest = link.mid(separator + 3).trimmed();
        if (rest.startsWith(QLatin1String("v1/"), Qt::CaseInsensitive)) return masterDnsParseToml(*this, rest.mid(3));
        if (rest.compare(QLatin1String("v1"), Qt::CaseInsensitive) == 0 || rest.startsWith(QLatin1String("v1?"), Qt::CaseInsensitive) ||
            rest.startsWith(QLatin1String("v1#"), Qt::CaseInsensitive))
            return masterDnsParseYptun(*this, rest.mid(2));
        return masterDnsParseProfile(*this, rest);
    }

    bool masterdns::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "masterdns") return false;
        outbound::ParseFromJson(object);
        // The tunnel only dials resolvers; a server here would be refused by the core.
        server.clear();
        server_port = 0;
        if (object.contains("domain")) domains = masterDnsDomains(JsonStringList(object["domain"], "domain"));
        if (object.contains("resolvers")) {
            resolvers.lines = ImportResolverLines(JsonStringList(object["resolvers"], "resolvers"), ResolverFlavor::MasterDns, "resolvers");
        }
        if (object.contains("resolver_list")) {
            if (ParseDiag::Active()) {
                ParseDiag::Warn("resolver_list", QCoreApplication::translate("Validation", "The resolver IP list belongs to another installation, so it was not imported."));
            } else {
                resolvers.ipListId = object["resolver_list"].toInt(-1);
            }
        }
        if (object.contains("mode")) mode = object["mode"].toString().trimmed().toLower();
        if (object.contains("encryption_method")) encryption_method = object["encryption_method"].toString().trimmed().toLower();
        encryption_key = object["encryption_key"].toString();
        base64_response = object["base64_response"].toBool();
        upload_compression = object["upload_compression"].toString().trimmed().toLower();
        download_compression = object["download_compression"].toString().trimmed().toLower();
        balancing_strategy = object["balancing_strategy"].toString().trimmed().toLower();
        packet_duplication = object["packet_duplication"].toInt();
        setup_packet_duplication = object["setup_packet_duplication"].toInt();
        const auto mtu = object["mtu"].toObject();
        mtu_min_upload = mtu["min_upload"].toInt();
        mtu_max_upload = mtu["max_upload"].toInt();
        mtu_min_download = mtu["min_download"].toInt();
        mtu_max_download = mtu["max_download"].toInt();
        mtu_test_retries = mtu["test_retries"].toInt();
        mtu_test_timeout = mtu["test_timeout"].toString().trimmed();
        mtu_test_parallelism = mtu["test_parallelism"].toInt();
        mtu_auto_remove_low = mtu["auto_remove_low"].toBool(true);
        return true;
    }

    QString masterdns::ExportToLink()
    {
        DnstLink link;
        link.tag = name;
        link.domain = domains.value(0);
        link.transport = "masterdns";
        link.backend = mode == QLatin1String("tcp") ? QStringLiteral("custom") : QStringLiteral("socks");
        link.params.insert("mkey", encryption_key);
        const auto method = masterDnsEncryptionMethods.indexOf(encryption_method);
        link.params.insert("encryption", QString::number(method >= 0 ? method : 1));
        return EncodeDnstLink(link, false);
    }

    QJsonObject masterdns::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "masterdns";
        mergeJsonObjects(object, outbound::ExportToJson());
        masterDnsWriteCommon(*this, object);
        if (!resolvers.lines.isEmpty()) object["resolvers"] = QListStr2QJsonArray(resolvers.lines);
        if (resolvers.ipListId >= 0) object["resolver_list"] = resolvers.ipListId;
        return object;
    }

    QJsonObject masterdns::ExportIdentity()
    {
        QStringList lowered;
        for (const auto& item : domains) lowered << item.toLower();
        return {{"type", "masterdns"},
                {"domain", QListStr2QJsonArray(lowered)},
                {"mode", mode},
                {"encryption_method", encryption_method},
                {"encryption_key", encryption_key}};
    }

    BuildResult masterdns::Build()
    {
        QJsonObject object;
        object["type"] = "masterdns";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        masterDnsWriteCommon(*this, object);
        const auto built = BuildResolvers(resolvers, ResolverFlavor::MasterDns, DisplayTypeAndName());
        if (!built.error.isEmpty()) return {{}, built.error};
        object["resolvers"] = built.resolvers;
        return {object, ""};
    }

    QString masterdns::DisplayAddress()
    {
        if (domains.size() <= 1) return domains.value(0);
        return QStringLiteral("%1 +%2").arg(domains.first()).arg(domains.size() - 1);
    }

    QString masterdns::DisplayType()
    {
        return "MasterDNS";
    }

    SecurityInfo masterdns::GetSecurity()
    {
        if (encryption_method == QLatin1String("none")) return {QObject::tr("Raw"), {}, SecurityLevel::None};
        // MasterDnsVPN's AES-GCM modes leave what the server sends back unencrypted.
        if (encryption_method.startsWith(QLatin1String("aes-"))) return {QObject::tr("Plaintext Replies"), {}, SecurityLevel::Weak};
        return {QObject::tr("Weak Cipher"), {}, SecurityLevel::Weak};
    }

    void masterdns::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        using Validation::Severity;
        const auto report = [&issues](const QString& field, const QString& message, const QString& value = {},
                                      Severity severity = Severity::Error) {
            if (!message.isEmpty()) issues << Validation::Issue{severity, field, message, value};
        };
        if (domains.isEmpty()) report("domain", Checks::DnsTunnelDomain({}));
        for (const auto& item : domains) {
            if (const auto message = Checks::DnsTunnelDomain(item); !message.isEmpty()) {
                report("domain", message, item);
                break;
            }
        }
        report("mode", Validation::OneOf(mode, masterDnsModes, Validation::Empty::Required), mode);
        report("encryption_method", Validation::OneOf(encryption_method, masterDnsEncryptionMethods, Validation::Empty::Required), encryption_method);
        report("encryption_key", Checks::MasterDnsEncryptionKey(encryption_method, encryption_key));
        report("upload_compression", Validation::OneOf(upload_compression, masterDnsCompressions), upload_compression);
        report("download_compression", Validation::OneOf(download_compression, masterDnsCompressions), download_compression);
        report("balancing_strategy", Validation::OneOf(balancing_strategy, masterDnsBalancingStrategies), balancing_strategy);
        report("packet_duplication", Checks::MasterDnsDuplication(packet_duplication), QString::number(packet_duplication));
        // The core raises a setup duplication below the packet duplication to it, as MasterDnsVPN does.
        report("setup_packet_duplication", Validation::Int(QString::number(setup_packet_duplication), 0, kMasterDnsMaxSetupDuplication),
               QString::number(setup_packet_duplication));
        report("mtu_min_upload", Checks::MasterDnsMtuRange(mtu_min_upload, mtu_max_upload, 38, 150));
        report("mtu_min_download", Checks::MasterDnsMtuRange(mtu_min_download, mtu_max_download, 200, 4000));
        report("mtu_test_retries", Validation::Int(QString::number(mtu_test_retries), 0, INT_MAX), QString::number(mtu_test_retries));
        report("mtu_test_timeout", Validation::Duration(mtu_test_timeout, masterDnsPositiveDuration()), mtu_test_timeout);
        report("mtu_test_parallelism", Validation::Int(QString::number(mtu_test_parallelism), 0, INT_MAX), QString::number(mtu_test_parallelism));
        for (const auto& line : resolvers.lines) {
            if (const auto message = Checks::MasterDnsResolverLine(line); !message.isEmpty()) {
                report("resolvers", message, line.trimmed());
                break;
            }
        }
    }
}
