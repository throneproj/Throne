#include "include/configs/outbounds/naive.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

#include <limits>

namespace Configs {
    namespace Checks {
        QString NaiveExtraHeaders(const QString &text)
        {
            if (text.trimmed().isEmpty() || !parseHeaderPairs(text).isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Write each header as Name=\"value\", for example X-Username=\"user\" X-Password=\"pass\".");
        }

        QString NaiveExtraHeaderNames(const QStringList &pairs)
        {
            for (qsizetype i = 0; i + 1 < pairs.size(); i += 2) {
                if (const auto message = Validation::HttpToken(pairs[i], Validation::Empty::Required); !message.isEmpty()) {
                    return QCoreApplication::translate("Validation", "Header name \"%1\": %2").arg(pairs[i], message);
                }
            }
            return {};
        }
    }

    namespace {
        // Without a diagnostics scope (DB loads, clones) a parse keeps every value as it always did.
        bool naiveKeep(const QString& field, const QString& value, const Validation::Rule& rule)
        {
            return !ParseDiag::Active() || AcceptOptional(field, rule(value), value);
        }

        // Go's net/url (Husi, naive's own tooling) splits the userinfo at the last '@', QUrl at the first.
        QString escapeUserInfoAts(const QString& link)
        {
            const auto schemeEnd = link.indexOf(QStringLiteral("://"));
            if (schemeEnd < 0) return link;
            const qsizetype from = schemeEnd + 3;
            qsizetype authEnd = link.size();
            for (qsizetype i = from; i < link.size(); ++i) {
                const auto c = link.at(i);
                if (c == u'/' || c == u'?' || c == u'#') {
                    authEnd = i;
                    break;
                }
            }
            const auto lastAt = link.lastIndexOf(u'@', authEnd - 1);
            if (lastAt < from) return link;
            auto userInfo = link.mid(from, lastAt - from);
            if (!userInfo.contains(u'@')) return link;
            return link.left(from) + userInfo.replace(u'@', QStringLiteral("%40")) + link.mid(lastAt);
        }

        // Form-encoded like Go's url.Values (Husi), so '+' is a space; lines split by CRLF (spec) or LF (Husi).
        QStringList parseExtraHeaders(const QUrlQuery& query)
        {
            auto raw = query.queryItemValue(QStringLiteral("extra-headers"), QUrl::FullyEncoded).replace(u'+', u' ');
            const auto text = QUrl::fromPercentEncoding(raw.toUtf8());
            QStringList pairs;
            for (auto line : text.split(u'\n', Qt::SkipEmptyParts)) {
                if (line.endsWith(u'\r')) line.chop(1);
                const auto colon = line.indexOf(u':');
                const auto name = colon > 0 ? line.left(colon).trimmed() : QString();
                if (name.isEmpty()) {
                    // The line may carry a credential, so it is not echoed.
                    if (!line.trimmed().isEmpty()) {
                        ParseDiag::Warn("extra-headers", QCoreApplication::translate("Validation", "A header line without a name and a colon was ignored; write each line as Name: value."));
                    }
                    continue;
                }
                pairs << name << line.mid(colon + 1).trimmed();
            }
            return pairs;
        }

        QString exportExtraHeaders(const QStringList& pairs)
        {
            QStringList lines;
            for (qsizetype i = 0; i + 1 < pairs.size(); i += 2) lines << pairs[i] + u':' + pairs[i + 1];
            // Pre-encoded: QUrlQuery would keep a literal '+', which every reader decodes as a space.
            return QString::fromLatin1(QUrl::toPercentEncoding(lines.join(QStringLiteral("\r\n"))));
        }

        // The core's naive outbound reads only these and refuses to start on most other TLS options.
        QJsonObject naiveTLS(const QJsonObject& tls, bool keepFragment)
        {
            QJsonObject kept{{"enabled", true}};
            for (const auto key : {"server_name", "certificate", "certificate_path", "ech"}) {
                if (tls.contains(QLatin1String(key))) kept[QLatin1String(key)] = tls[QLatin1String(key)];
            }
            // Stored for the dialer-level ("custom") fragment in outbound::Build(); TLS-level fragment is rejected.
            if (keepFragment && tls.contains("fragment")) kept["fragment"] = tls["fragment"];
            return kept;
        }
    }

    bool naive::ParseFromLink(const QString& rawLink)
    {
        const auto link = escapeUserInfoAts(rawLink);
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        username = url.userName();
        password = url.password();
        if (server_port == 0) server_port = 443;

        if (query.hasQueryItem("uot")) uot = query.queryItemValue("uot") == "true" || query.queryItemValue("uot").toInt() > 0;
        if (query.hasQueryItem("insecure-concurrency")) {
            const auto value = query.queryItemValue("insecure-concurrency").trimmed();
            if (naiveKeep("insecure-concurrency", value, Validation::Rules::Int(0, std::numeric_limits<int>::max()))) {
                insecure_concurrency = qMax(0, value.toInt());
            }
        }
        if (query.hasQueryItem("extra-headers")) extra_headers = parseExtraHeaders(query);

        if (url.scheme() == "naive+quic") {
            quic = true;
            if (query.hasQueryItem("congestion_control")) {
                const auto value = query.queryItemValue("congestion_control").trimmed();
                if (naiveKeep("congestion_control", value, Validation::Rules::OneOf(naiveCongestionControls))) congestion_control = value;
            }
        }

        tls->enabled = true;
        if (query.hasQueryItem("sni")) tls->server_name = query.queryItemValue("sni");
        if (query.hasQueryItem("tls_certificate")) tls->certificate = query.queryItemValue("tls_certificate", QUrl::FullyDecoded).split(",", Qt::SkipEmptyParts);
        if (query.hasQueryItem("tls_certificate_path")) tls->certificate_path = query.queryItemValue("tls_certificate_path", QUrl::FullyDecoded);
        if (query.hasQueryItem("tls_fragment")) {
            tls->fragment = query.queryItemValue("tls_fragment") == "true";
            tls->fragment_unspecified = false;
        }
        tls->ech->ParseFromLink(link);

        return !server.isEmpty();
    }

    bool naive::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "naive") return false;
        outbound::ParseFromJson(object);
        if (object.contains("username")) username = object["username"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("insecure_concurrency")) insecure_concurrency = qMax(0, object["insecure_concurrency"].toInt());
        if (object["extra_headers"].isObject()) {
            extra_headers.clear();
            const auto headers = object["extra_headers"].toObject();
            for (auto it = headers.begin(); it != headers.end(); ++it) {
                QJsonValue value = it.value();
                if (value.isArray()) value = value.toArray().isEmpty() ? QJsonValue() : value.toArray().first();
                extra_headers << it.key() << value.toString();
            }
        }
        if (object.contains("udp_over_tcp"))
        {
            if (object["udp_over_tcp"].isBool()) uot = object["udp_over_tcp"].toBool();
            if (object["udp_over_tcp"].isObject()) uot = object["udp_over_tcp"].toObject()["enabled"].toBool();
        }
        if (object.contains("quic")) quic = object["quic"].toBool();
        if (object.contains("quic_congestion_control")) {
            const auto value = object["quic_congestion_control"].toString();
            if (naiveKeep("quic_congestion_control", value, Validation::Rules::OneOf(naiveCongestionControls))) congestion_control = value;
        }
        if (object.contains("tls")) tls->ParseFromJson(naiveTLS(object["tls"].toObject(), true));
        return true;
    }

    void naive::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        if (quic) {
            if (const auto message = Validation::OneOf(congestion_control, naiveCongestionControls); !message.isEmpty()) {
                issues << Validation::Issue{Validation::Severity::Error, "quic_congestion_control", message, congestion_control};
            }
        }
        if (const auto message = Checks::NaiveExtraHeaderNames(extra_headers); !message.isEmpty()) {
            issues << Validation::Issue{Validation::Severity::Warning, "extra_headers", message, {}};
        }
    }

    QString naive::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setUserName(username);
        url.setPassword(password);
        url.setHost(server);
        if (server_port > 0) url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        if (!tls->server_name.isEmpty()) query.addQueryItem("sni", tls->server_name);
        if (!extra_headers.isEmpty()) query.addQueryItem("extra-headers", exportExtraHeaders(extra_headers));
        if (insecure_concurrency > 0) query.addQueryItem("insecure-concurrency", QString::number(insecure_concurrency));
        if (uot) query.addQueryItem("uot", "1");
        if (quic) {
            url.setScheme("naive+quic");
            if(!congestion_control.isEmpty()) query.addQueryItem("congestion_control", congestion_control);
        } else {
            url.setScheme("naive+https");
        }
        if (!tls->certificate.isEmpty()) query.addQueryItem("tls_certificate", tls->certificate.join(","));
        if (!tls->certificate_path.isEmpty()) query.addQueryItem("tls_certificate_path", tls->certificate_path);
        if (!tls->fragment_unspecified) query.addQueryItem("tls_fragment", tls->fragment ? "true" : "false");

        mergeUrlQuery(query, tls->ech->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());

        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject naive::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "naive";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (insecure_concurrency > 0) object["insecure_concurrency"] = insecure_concurrency;
        if (!extra_headers.isEmpty()) object["extra_headers"] = qStringListToJsonObject(extra_headers);
        if (uot) object["udp_over_tcp"] = uot;
        if (quic) {
            object["quic"] = quic;
            if (!congestion_control.isEmpty()) object["quic_congestion_control"] = congestion_control;
        }
        object["tls"] = naiveTLS(tls->ExportToJson(), true);
        return object;
    }

    BuildResult naive::Build()
    {
        QJsonObject object;
        object["type"] = "naive";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (insecure_concurrency > 0) object["insecure_concurrency"] = insecure_concurrency;
        if (!extra_headers.isEmpty()) object["extra_headers"] = qStringListToJsonObject(extra_headers);
        if (uot) object["udp_over_tcp"] = uot;
        if (quic) {
            object["quic"] = quic;
            if (!congestion_control.isEmpty()) object["quic_congestion_control"] = congestion_control;
        }
        // Not tls->Build(): it injects the global skip_cert / fragment defaults, which the core rejects for naive.
        auto tlsObj = tls->ExportToJson();
        // ExportToJson keeps Throne-only fields (ech.resolver); the core rejects unknown keys, so use ECH's own Build().
        if (tls->ech->enabled) {
            const auto echResult = tls->ech->Build();
            if (!echResult.error.isEmpty()) return {{}, echResult.error};
            tlsObj["ech"] = echResult.object;
        }
        object["tls"] = naiveTLS(tlsObj, false);
        return {object, ""};
    }

    QString naive::DisplayType()
    {
        return "Naive";
    }

    SecurityInfo naive::GetSecurity()
    {
        return SecurityFromTLS(quic ? "QUIC" : QString());
    }
}
