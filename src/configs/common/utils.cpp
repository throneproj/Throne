#include "include/configs/common/utils.h"

#include <algorithm>
#include <QCoreApplication>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QRegularExpression>

#include "include/configs/common/xrayStreamSetting.h"
#include "include/configs/ParseDiagnostics.h"
#include "include/global/Configs.hpp"
#include "include/global/Validation.hpp"

namespace Configs
{
    bool AcceptOptional(const QString& field, const QString& message, const QString& value)
    {
        if (message.isEmpty()) return true;
        ParseDiag::Warn(field, QCoreApplication::translate("Validation", "%1 The value was ignored.").arg(message), value);
        return false;
    }

    void ReadQueryInt(const QUrlQuery& query, const QString& key, int& target, int min, int max)
    {
        if (!query.hasQueryItem(key)) return;
        const auto text = query.queryItemValue(key).trimmed();
        if (!AcceptOptional(key, Validation::Int(text, min, max), text)) return;
        if (const auto value = Validation::ToInt(text)) target = static_cast<int>(*value);
    }

    QString LegacyComboValue(const QString& value, const QStringList& identifiers)
    {
        // Labels that case and space folding alone cannot map back.
        static const QHash<QString, QString> translated = {
            {QStringLiteral("нативный"), QStringLiteral("native")},
            {QStringLiteral("bbr v2"), QStringLiteral("bbr2")},
            {QStringLiteral("вариант bbr"), QStringLiteral("bbr_variant")},
            {QStringLiteral("bbr变体"), QStringLiteral("bbr_variant")},
            {QStringLiteral("标准bbr"), QStringLiteral("bbr_standard")},
        };
        const auto folded = value.trimmed().toLower();
        for (const auto& candidate : {translated.value(folded, folded), QString(folded).replace(' ', '_'), QString(folded).remove(' ')}) {
            if (identifiers.contains(candidate)) return candidate;
        }
        return value.trimmed();
    }

    QStringList JsonStringList(const QJsonValue& value, const QString& field)
    {
        if (value.isString()) return {value.toString()};
        if (value.isNull() || value.isUndefined()) return {};
        if (!value.isArray()) {
            ParseDiag::Warn(field, QCoreApplication::translate("Validation", "Expected text or a list of text, so the value was ignored."));
            return {};
        }
        QStringList items;
        bool skipped = false;
        for (const auto& item : value.toArray()) {
            if (item.isString()) items << item.toString();
            else skipped = true;
        }
        if (skipped) ParseDiag::Warn(field, QCoreApplication::translate("Validation", "List items that are not text were ignored."));
        return items;
    }

    namespace Checks {
        QString HeaderPairs(const QString& text)
        {
            const auto t = text.trimmed();
            if (t.isEmpty()) return {};
            if (t.contains('|')) return QCoreApplication::translate("Validation", "Must not contain \"|\", which separates headers in share links.");
            const auto pairs = parseHeaderPairs(t);
            bool ok = t.count('"') % 2 == 0 && !pairs.isEmpty();
            for (qsizetype i = 0; ok && i < pairs.size(); i += 2) ok = !pairs[i].trimmed().isEmpty();
            if (ok) return {};
            return QCoreApplication::translate("Validation", "Write Name=value pairs separated by spaces, quoting values that contain spaces, for example User-Agent=\"Mozilla/5.0\" X-Token=abc.");
        }

        QString HostHeader(const QString& text, bool allowList)
        {
            const auto rule = [](const QString& item) { return Validation::HostPort(item, false); };
            if (allowList) return Validation::EachItem(text, Validation::Split::Comma, rule);
            return text.trimmed().isEmpty() ? QString() : rule(text);
        }

        QString ServerName(const QString& text)
        {
            const auto message = Validation::Host(text, Validation::Empty::Allowed);
            if (message.isEmpty()) return {};
            if (Validation::HostPort(text, true).isEmpty()) return QCoreApplication::translate("Validation", "Enter only the domain name, without a port.");
            return message;
        }
    }

    void mergeUrlQuery(QUrlQuery& baseQuery, const QString& strQuery)
    {
        QUrlQuery query = QUrlQuery(strQuery);
        for (const auto& item : query.queryItems())
        {
            baseQuery.addQueryItem(item.first, item.second);
        }
    }

    QString formDecodedQueryValue(const QUrlQuery& query, const QString& key)
    {
        // urlencode()-style panels send '+' for a space, which QUrlQuery never decodes but keeps apart from %2B.
        auto raw = query.queryItemValue(key, QUrl::FullyEncoded).toUtf8();
        raw.replace('+', "%20");
        return QUrl::fromPercentEncoding(raw);
    }

    void mergeJsonObjects(QJsonObject& baseObject, const QJsonObject& obj)
    {
        for (const auto& key : obj.keys())
        {
            baseObject[key] = obj[key];
        }
    }

    QStringList jsonObjectToQStringList(const QJsonObject& obj)
    {
        auto result = QStringList();
        for (const auto& key : obj.keys())
        {
            result << key << obj[key].toString();
        }
        return result;
    }

    QJsonObject qStringListToJsonObject(const QStringList& list)
    {
        auto result = QJsonObject();
        if (list.count() %2 != 0)
        {
            qDebug() << "QStringList of odd length in qStringListToJsonObject:" << list;
            return result;
        }
        for (int i=0;i<list.size();i+=2)
        {
            result[list[i]] = list[i+1];
        }
        return result;
    }

    namespace {
        // The TLS::ParseFromLink rule: a known security value, else an sni/peer unless security is explicitly off.
        bool linkEnablesTls(const QUrlQuery& query) {
            const auto security = query.queryItemValue("security").trimmed().toLower();
            if (security == "tls" || security == "true" || security == "1" || security == "reality") return true;
            if (security == "none" || security == "false" || security == "0") return false;
            // ParseFromLink reads sni first and lets peer overwrite it.
            const auto serverName = query.hasQueryItem("peer") ? query.queryItemValue("peer") : query.queryItemValue("sni");
            return !serverName.isEmpty();
        }
    }

    // TODO add setting items and use them here
    bool useXrayVless(const QString& link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());
        const auto transport = NormalizeXrayNetwork(query.queryItemValue("type"));
        const auto encryption = query.queryItemValue("encryption");
        const auto security = query.queryItemValue("security").trimmed().toLower();
        const bool xrayOnly = query.hasQueryItem("fm")
                              || query.hasQueryItem("finalmask")
                              // sing-box has no counterpart to verifyPeerCertByName
                              || query.hasQueryItem("vcn")
                              // sing-box rejects the CBC suites `cs` is used for; Xray accepts them with fp=unsafe
                              || query.hasQueryItem("cs")
                              // sing-box VLESS takes only "" and xtls-rprx-vision
                              || query.queryItemValue("flow").trimmed().toLower() == "xtls-rprx-vision-udp443"
                              || (!encryption.isEmpty() && encryption != "none")
                              || !query.queryItemValue("extra").isEmpty()
                              // sing-box REALITY cannot verify the ML-DSA-65 signature
                              || (security == "reality" && !query.queryItemValue("pqv").trimmed().isEmpty());
        // Throne's Xray stream refuses any other network, which sing-box VLESS may still carry (http, quic), but it would drop the Xray-only values.
        if (!transport.isEmpty() && !XrayNetworks.contains(transport)) return xrayOnly;
        // Xray refuses REALITY over ws and httpupgrade, which sing-box carries.
        if (!xrayOnly && !Checks::XrayRealityNetwork(security, transport).isEmpty()) return false;
        // sing-box's http transport speaks the raw HTTP header only in plaintext; TLS (which a bare sni also enables) turns it into h2
        const bool rawHttpOverTls = (transport.isEmpty() || transport == "raw")
                                    && query.queryItemValue("headerType") == "http"
                                    && linkEnablesTls(query);

        return xrayOnly
               || dataManager->settingsRepo->xray_vless_preference == Xray::AllVLESS
               || rawHttpOverTls
               || transport == "xhttp"
               || (security == "reality" && dataManager->settingsRepo->xray_vless_preference == Xray::XhttpAndReality);
    }

    QString toAceHost(const QString& host)
    {
        // the http transport and Xray's raw header carry a comma list of hosts
        if (host.contains(',')) {
            auto parts = host.split(',');
            for (auto& part : parts) part = toAceHost(part.trimmed());
            return parts.join(',');
        }
        bool ascii = true;
        for (const auto ch : host) {
            if (ch.unicode() > 0x7F) {
                ascii = false;
                break;
            }
        }
        if (ascii) return host;
        // toAce is empty for IP literals and for names it rejects
        const auto ace = QString::fromLatin1(QUrl::toAce(host));
        return ace.isEmpty() ? host : ace;
    }

    bool IsPrivateHost(const QString& host)
    {
        auto bare = host.trimmed().toLower();
        bare.remove('[').remove(']');
        if (bare.endsWith('.')) bare.chop(1);
        if (bare.isEmpty()) return false;

        if (QHostAddress ip; ip.setAddress(bare)) {
            // Folds a v4-mapped IPv6 address onto the IPv4 ranges.
            bool isV4 = false;
            if (const auto v4 = ip.toIPv4Address(&isV4); isV4) ip = QHostAddress(v4);
            static const auto ranges = [] {
                QList<QPair<QHostAddress, int>> list;
                for (const auto cidr : {"0.0.0.0/8", "10.0.0.0/8", "100.64.0.0/10", "127.0.0.0/8", "169.254.0.0/16",
                                        "172.16.0.0/12", "192.0.0.0/24", "192.0.2.0/24", "192.88.99.0/24", "192.168.0.0/16",
                                        "198.18.0.0/15", "198.51.100.0/24", "203.0.113.0/24", "224.0.0.0/3",
                                        "::/127", "fc00::/7", "fe80::/10", "ff00::/8"}) {
                    list << QHostAddress::parseSubnet(QLatin1String(cidr));
                }
                return list;
            }();
            return std::any_of(ranges.cbegin(), ranges.cend(), [&ip](const auto& range) { return ip.isInSubnet(range); });
        }

        static const QStringList suffixes = {"lan", "localdomain", "example", "invalid", "localhost", "test", "local",
                                             "home.arpa", "internal"};
        for (const auto& suffix : suffixes) {
            if (bare == suffix || bare.endsWith('.' + suffix)) return true;
        }
        // A dotless name only resolves through the local search domain.
        static const QRegularExpression dotless(QStringLiteral("^[a-z]([a-z0-9-]{0,61}[a-z0-9])?$"));
        return dotless.match(bare).hasMatch();
    }

    QString getHeadersString(const QStringList& headers) {
        QString result;
        if (headers.length()%2 != 0) {
            return "";
        }
        QStringList formatted;
        formatted.reserve(headers.length()/2);

        for (int i=0;i<headers.length();i+=2) {
            formatted.append(QStringLiteral("%1=\"%2\"").arg(headers.at(i), headers.at(i + 1)));
        }
        return formatted.join(' ');
    }

    QStringList parseHeaderPairs(const QString& rawHeader) {
        bool inQuote = false;
        QString curr;
        QStringList list;
        for (const auto &ch: rawHeader) {
            if (inQuote) {
                if (ch == '"') {
                    inQuote = false;
                    list << curr;
                    curr = "";
                    continue;
                } else {
                    curr += ch;
                    continue;
                }
            }
            if (ch == '"') {
                inQuote = true;
                continue;
            }
            if (ch == ' ') {
                if (!curr.isEmpty()) {
                    list << curr;
                    curr = "";
                }
                continue;
            }
            if (ch == '=') {
                if (!curr.isEmpty()) {
                    list << curr;
                    curr = "";
                }
                continue;
            }
            curr+=ch;
        }
        if (!curr.isEmpty()) list<<curr;

        if (list.size()%2 != 0) {
            return {};
        }

        return list;
    }
}
