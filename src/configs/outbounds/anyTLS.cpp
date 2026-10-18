#include "include/configs/outbounds/anyTLS.h"

#include <QCoreApplication>
#include <QUrlQuery>
#include <include/global/Utils.hpp>
#include <limits>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        void anyTlsImportDuration(const QString& key, const QString& raw, QString& field)
        {
            const QString value = raw.trimmed();
            if (AcceptOptional(key, Validation::Duration(value), value)) field = value;
        }

        void anyTlsDropFastOpen(DialFields& dial)
        {
            if (!dial.tcp_fast_open) return;
            ParseDiag::Warn("tcp_fast_open", QCoreApplication::translate("Validation", "AnyTLS does not support TCP Fast Open, so it was turned off."));
            dial.tcp_fast_open = false;
        }
    }

    bool anyTLS::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        anyTlsDropFastOpen(*dialFields);
        password = url.userName();

        if (server_port == 0) server_port = 443;

        if (query.hasQueryItem("idle_session_check_interval")) anyTlsImportDuration("idle_session_check_interval", query.queryItemValue("idle_session_check_interval"), idle_session_check_interval);
        if (query.hasQueryItem("idle_session_timeout")) anyTlsImportDuration("idle_session_timeout", query.queryItemValue("idle_session_timeout"), idle_session_timeout);
        if (query.hasQueryItem("min_idle_session")) {
            const auto value = query.queryItemValue("min_idle_session").trimmed();
            if (AcceptOptional("min_idle_session", Validation::Int(value, 0, std::numeric_limits<int>::max()), value)) min_idle_session = value.toInt();
        }

        tls->ParseFromLink(link);
        tls->enabled = true; // anyTLS always uses tls

        return true;
    }

    bool anyTLS::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "anytls") return false;
        outbound::ParseFromJson(object);
        if (object.contains("password")) password = object["password"].toString();
        // Stored values load as they are; only an import drops a malformed one.
        const auto readDuration = [&object](const QString& key, QString& field) {
            if (!object.contains(key)) return;
            if (ParseDiag::Active()) anyTlsImportDuration(key, object[key].toString(), field);
            else field = object[key].toString();
        };
        readDuration("idle_session_check_interval", idle_session_check_interval);
        readDuration("idle_session_timeout", idle_session_timeout);
        if (object.contains("min_idle_session")) min_idle_session = object["min_idle_session"].toInt();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        if (ParseDiag::Active()) anyTlsDropFastOpen(*dialFields);
        return true;
    }

    bool anyTLS::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "anytls") return false;
        outbound::ParseFromClash(object);
        password = QString::fromStdString(object.password);
        if (object.idle_session_check_interval > 0) idle_session_check_interval = QString::number(object.idle_session_check_interval) + "s";
        if (object.idle_session_timeout > 0) idle_session_timeout = QString::number(object.idle_session_timeout) + "s";
        if (object.min_idle_session > 0) min_idle_session = object.min_idle_session;

        tls->ParseFromClash(object);
        tls->enabled = true;
        return true;
    }

    QString anyTLS::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("anytls");
        url.setUserName(password);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        if (!idle_session_check_interval.isEmpty()) query.addQueryItem("idle_session_check_interval", idle_session_check_interval);
        if (!idle_session_timeout.isEmpty()) query.addQueryItem("idle_session_timeout", idle_session_timeout);
        if (min_idle_session > 0) query.addQueryItem("min_idle_session", QString::number(min_idle_session));
        
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject anyTLS::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "anytls";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!password.isEmpty()) object["password"] = password;
        if (!idle_session_check_interval.isEmpty()) object["idle_session_check_interval"] = idle_session_check_interval;
        if (!idle_session_timeout.isEmpty()) object["idle_session_timeout"] = idle_session_timeout;
        if (min_idle_session > 0) object["min_idle_session"] = min_idle_session;
        object["tls"] = tls->ExportToJson();
        return object;
    }

    BuildResult anyTLS::Build()
    {
        QJsonObject object;
        object["type"] = "anytls";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        // The core refuses to start an AnyTLS outbound with tcp_fast_open.
        object.remove("tcp_fast_open");
        if (!password.isEmpty()) object["password"] = password;
        if (!idle_session_check_interval.isEmpty()) object["idle_session_check_interval"] = idle_session_check_interval;
        if (!idle_session_timeout.isEmpty()) object["idle_session_timeout"] = idle_session_timeout;
        if (min_idle_session > 0) object["min_idle_session"] = min_idle_session;
        const auto tlsResult = tls->Build();
        if (!tlsResult.error.isEmpty()) return {{}, tlsResult.error};
        object["tls"] = tlsResult.object;
        return {object, ""};
    }

    QString anyTLS::DisplayType()
    {
        return "AnyTLS";
    }

    void anyTLS::Validate(Validation::Issues &issues)
    {
        outbound::Validate(issues);
        using Validation::Severity;
        if (auto message = Validation::Required(password); !message.isEmpty()) issues.append({Severity::Error, "password", message});
        if (auto message = Validation::Duration(idle_session_check_interval); !message.isEmpty())
            issues.append({Severity::Error, "idle_session_check_interval", message, idle_session_check_interval});
        if (auto message = Validation::Duration(idle_session_timeout); !message.isEmpty())
            issues.append({Severity::Error, "idle_session_timeout", message, idle_session_timeout});
    }
}
