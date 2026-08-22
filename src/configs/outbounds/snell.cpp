#include "include/configs/outbounds/snell.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // The core registers a v4 and a v6 client only; every other Snell version
        // on the wire (v1-v3, v5) has no outbound we could build for it.
        bool supportedVersion(int version) {
            return version == 4 || version == 6;
        }

        // Clash also offers shadow-tls / restls / jls obfuscation, none of which
        // the core's Snell client implements.
        bool supportedObfsMode(const QString& mode) {
            return snellObfsModes.contains(mode);
        }
    }

    bool snell::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid() || url.host().isEmpty()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        psk = url.userName();
        if (psk.isEmpty() && query.hasQueryItem("psk")) psk = query.queryItemValue("psk", QUrl::FullyDecoded);

        if (query.hasQueryItem("version")) version = query.queryItemValue("version").toInt();
        if (!supportedVersion(version)) return false;

        if (query.hasQueryItem("userkey")) userkey = query.queryItemValue("userkey", QUrl::FullyDecoded);
        if (query.hasQueryItem("reuse")) {
            const auto raw = query.queryItemValue("reuse");
            reuse = raw.isEmpty() || (raw != "0" && raw.compare("false", Qt::CaseInsensitive) != 0);
        }
        if (query.hasQueryItem("network")) network = query.queryItemValue("network");
        if (query.hasQueryItem("obfs")) obfs_mode = query.queryItemValue("obfs");
        if (query.hasQueryItem("obfs-host")) obfs_host = query.queryItemValue("obfs-host", QUrl::FullyDecoded);
        if (query.hasQueryItem("mode")) mode = query.queryItemValue("mode");

        return true;
    }

    bool snell::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "snell") return false;
        outbound::ParseFromJson(object);
        if (object.contains("version")) version = object["version"].toInt();
        if (!supportedVersion(version)) return false;
        if (object.contains("psk")) psk = object["psk"].toString();
        if (object.contains("userkey")) userkey = object["userkey"].toString();
        if (object.contains("reuse")) reuse = object["reuse"].toBool();
        if (object.contains("network")) network = object["network"].toString();
        if (object.contains("obfs_mode")) obfs_mode = object["obfs_mode"].toString();
        if (object.contains("obfs_host")) obfs_host = object["obfs_host"].toString();
        if (object.contains("mode")) mode = object["mode"].toString();
        return true;
    }

    bool snell::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "snell") return false;
        outbound::ParseFromClash(object);
        psk = QString::fromStdString(object.psk);
        // Clash omits the key on legacy v1 nodes; assume the oldest version we
        // can still dial rather than dropping every version-less entry.
        if (object.version > 0) version = object.version;
        if (!supportedVersion(version)) return false;

        obfs_mode = QString::fromStdString(object.obfs_opts.mode);
        if (!supportedObfsMode(obfs_mode)) return false;
        obfs_host = QString::fromStdString(object.obfs_opts.host);
        if (!object.udp) network = "tcp";
        return true;
    }

    QString snell::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("snell");
        url.setUserName(psk);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        query.addQueryItem("version", Int2String(version));
        if (!userkey.isEmpty()) query.addQueryItem("userkey", userkey);
        if (reuse) query.addQueryItem("reuse", "1");
        if (!network.isEmpty()) query.addQueryItem("network", network);
        if (version == 6) {
            if (!mode.isEmpty()) query.addQueryItem("mode", mode);
        } else {
            if (!obfs_mode.isEmpty()) query.addQueryItem("obfs", obfs_mode);
            if (!obfs_host.isEmpty()) query.addQueryItem("obfs-host", obfs_host);
        }

        mergeUrlQuery(query, outbound::ExportToLink());

        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject snell::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "snell";
        mergeJsonObjects(object, outbound::ExportToJson());
        object["version"] = version;
        if (!psk.isEmpty()) object["psk"] = psk;
        if (!userkey.isEmpty()) object["userkey"] = userkey;
        if (reuse) object["reuse"] = true;
        if (!network.isEmpty()) object["network"] = network;
        // The core unmarshals the version block strictly, so a key belonging to
        // the other version fails the whole outbound rather than being ignored.
        if (version == 6) {
            if (!mode.isEmpty()) object["mode"] = mode;
        } else {
            if (!obfs_mode.isEmpty()) object["obfs_mode"] = obfs_mode;
            if (!obfs_host.isEmpty()) object["obfs_host"] = obfs_host;
        }
        return object;
    }

    BuildResult snell::Build()
    {
        QJsonObject object;
        object["type"] = "snell";
        mergeJsonObjects(object, outbound::Build().object);
        object["version"] = version;
        if (!psk.isEmpty()) object["psk"] = psk;
        if (!userkey.isEmpty()) object["userkey"] = userkey;
        if (reuse) object["reuse"] = true;
        if (!network.isEmpty()) object["network"] = network;
        if (version == 6) {
            if (!mode.isEmpty()) object["mode"] = mode;
        } else {
            if (!obfs_mode.isEmpty()) object["obfs_mode"] = obfs_mode;
            if (!obfs_host.isEmpty()) object["obfs_host"] = obfs_host;
        }
        return {object, ""};
    }

    QString snell::DisplayType()
    {
        return "Snell";
    }

    SecurityInfo snell::GetSecurity()
    {
        return {QObject::tr("Encrypted"), {}, SecurityLevel::Secure};
    }
}
