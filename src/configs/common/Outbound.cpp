#include "include/configs/common/Outbound.h"
#include "include/configs/common/utils.h"

namespace Configs {
    bool outbound::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid())
        {
            if(!url.errorString().startsWith("Invalid port"))
                return false;
            server_port = 0;
        } else {
            server_port = url.port();
        }

        if (url.hasFragment()) name = url.fragment(QUrl::FullyDecoded);
        server = url.host();
        dialFields->ParseFromLink(link);
        return true;
    }
    bool outbound::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("tag")) name = object["tag"].toString();
        if (object.contains("import_source")) import_source = object["import_source"].toString();
        if (object.contains("server")) server = object["server"].toString();
        if (object.contains("server_port")) server_port = object["server_port"].toInt();
        dialFields->ParseFromJson(object);
        return true;
    }
    bool outbound::ParseFromClash(const clash::Proxies& object)
    {
        name = QString::fromStdString(object.name);
        server = QString::fromStdString(object.server);
        server_port = object.port;
        return true;
    }
    QString outbound::ExportToLink()
    {
        QUrlQuery query;
        mergeUrlQuery(query, dialFields->ExportToLink());
        return query.toString();
    }
    QJsonObject outbound::ExportToJson()
    {
        QJsonObject object;
        if (!name.isEmpty()) object["tag"] = name;
        if (!import_source.isEmpty()) object["import_source"] = import_source;
        if (!server.isEmpty()) object["server"] = server;
        if (server_port > 0) object["server_port"] = server_port;
        auto dialFieldsObj = dialFields->ExportToJson();
        mergeJsonObjects(object, dialFieldsObj);
        return object;
    }
    BuildResult outbound::Build()
    {
        QJsonObject object;
        if (!server.isEmpty()) object["server"] = server;
        if (server_port > 0) object["server_port"] = server_port;
        mergeJsonObjects(object, dialFields->Build().object);
        // hiddify: the custom TLS-fragment implementation lives at the dialer level
        // (a sibling of "tls", not inside it), so emit it here when it is selected,
        // TLS is enabled, and fragment is effectively on. The built-in implementation
        // is emitted inside TLS::Build() as tls.fragment instead.
        if (HasTLS()) {
            auto t = GetTLS();
            if (t->enabled && t->FragmentEffectivelyOn() &&
                Configs::dataManager->settingsRepo->fragment_implementation == "custom") {
                // the core rejects tls_fragment combined with tcp_fast_open
                object.remove("tcp_fast_open");
                object["tls_fragment"] = QJsonObject{
                    {"enabled", true},
                    {"size", Configs::dataManager->settingsRepo->fragment_size.isEmpty() ? QString("10-100") : Configs::dataManager->settingsRepo->fragment_size},
                    {"sleep", Configs::dataManager->settingsRepo->fragment_sleep.isEmpty() ? QString("2-5") : Configs::dataManager->settingsRepo->fragment_sleep},
                };
            }
        }
        return {object, ""};
    }
}

