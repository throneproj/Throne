#include "include/configs/outbounds/snell.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QUrlQuery>
#include <algorithm>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // The core registers a v4 and a v6 client only; no outbound exists for v1-v3 / v5.
        bool supportedVersion(int version) {
            if (version == 4 || version == 6) return true;
            ParseDiag::Error("version", QCoreApplication::translate("Validation", "Only Snell versions 4 and 6 are supported."), QString::number(version));
            return false;
        }

        // Clash also offers shadow-tls / restls / jls obfs, none of which the core's Snell client implements.
        bool supportedObfsMode(const QString& mode) {
            if (snellObfsModes.contains(mode)) return true;
            ParseDiag::Error("obfs", QCoreApplication::translate("Validation", "Unsupported obfuscation; Snell can use only none, http or tls."), mode);
            return false;
        }

        // The core takes tcp, udp or a list of them; both means the empty default.
        QString snellImportNetwork(const QString& raw) {
            const QStringList items = Validation::Items(raw.toLower(), Validation::Split::Comma);
            if (std::all_of(items.begin(), items.end(), [](const QString& item) { return item == "tcp" || item == "udp"; }))
                return items.contains("tcp") && items.contains("udp") ? QString() : items.value(0);
            ParseDiag::Warn("network", QCoreApplication::translate("Validation", "Unknown network, so both TCP and UDP are used. Use tcp or udp."), raw.trimmed());
            return {};
        }

        // The core reads obfuscation modes case-insensitively; the editor lists them in lower case.
        QString snellImportObfsMode(const QString& raw) {
            return raw.trimmed().toLower();
        }
    }

    namespace Checks {
        QString SnellUserKey(const QString &userkey) {
            const auto bytes = userkey.toUtf8().size();
            if (bytes <= 255) return {};
            return QCoreApplication::translate("Validation", "Must be at most 255 bytes; this one is %1 bytes.").arg(bytes);
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
        if (query.hasQueryItem("network")) network = snellImportNetwork(query.queryItemValue("network"));
        if (query.hasQueryItem("obfs")) obfs_mode = snellImportObfsMode(query.queryItemValue("obfs"));
        if (query.hasQueryItem("obfs-host")) obfs_host = query.queryItemValue("obfs-host", QUrl::FullyDecoded).trimmed();
        if (query.hasQueryItem("mode")) mode = query.queryItemValue("mode").trimmed();

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
        if (object.contains("network")) {
            if (const auto value = object["network"]; value.isArray()) {
                QStringList networks;
                for (const auto& item : value.toArray()) networks << item.toString();
                network = snellImportNetwork(networks.join(','));
            } else {
                network = value.toString();
            }
        }
        if (object.contains("obfs_mode")) obfs_mode = object["obfs_mode"].toString();
        if (object.contains("obfs_host")) obfs_host = object["obfs_host"].toString();
        if (object.contains("mode")) mode = object["mode"].toString();
        if (ParseDiag::Active()) {
            network = snellImportNetwork(network);
            obfs_mode = snellImportObfsMode(obfs_mode);
            mode = mode.trimmed();
        }
        return true;
    }

    bool snell::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "snell") return false;
        outbound::ParseFromClash(object);
        psk = QString::fromStdString(object.psk);
        // Clash omits the key on legacy v1 nodes; keep the default rather than dropping the entry.
        if (object.version > 0) version = object.version;
        if (!supportedVersion(version)) return false;

        obfs_mode = snellImportObfsMode(QString::fromStdString(object.obfs_opts.mode));
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
        // The core unmarshals strictly: a key from the other version fails the whole outbound.
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
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
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

    void snell::Validate(Validation::Issues &issues)
    {
        outbound::Validate(issues);
        using Validation::Severity;
        if (auto message = Validation::Required(psk); !message.isEmpty()) issues.append({Severity::Error, "psk", message});
        if (auto message = Checks::SnellUserKey(userkey); !message.isEmpty()) issues.append({Severity::Error, "userkey", message});
        if (auto message = Validation::OneOf(network, snellNetworks); !message.isEmpty()) issues.append({Severity::Error, "network", message, network});
        // Build() emits only the chosen version's options.
        if (version == 6) {
            if (auto message = Validation::OneOf(mode, snellV6Modes); !message.isEmpty()) issues.append({Severity::Error, "mode", message, mode});
            if ((!obfs_mode.isEmpty() && obfs_mode.compare("none", Qt::CaseInsensitive) != 0) || !obfs_host.isEmpty())
                issues.append({Severity::Warning, "obfs_mode", QCoreApplication::translate("Validation", "Snell v6 has no obfuscation, so these settings are ignored.")});
        } else {
            if (auto message = Validation::OneOf(obfs_mode, snellObfsModes, Validation::Empty::Allowed, Qt::CaseInsensitive); !message.isEmpty())
                issues.append({Severity::Error, "obfs_mode", message, obfs_mode});
            else if (const QString obfs = obfs_mode.trimmed().toLower(); !obfs_host.trimmed().isEmpty() && obfs != "http" && obfs != "tls")
                issues.append({Severity::Warning, "obfs_host", QCoreApplication::translate("Validation", "The obfuscation host is used only with http or tls obfuscation, so it is ignored."), obfs_host});
            if (!mode.isEmpty())
                issues.append({Severity::Warning, "mode", QCoreApplication::translate("Validation", "The mode is used only by Snell v6, so it is ignored."), mode});
        }
    }

    SecurityInfo snell::GetSecurity()
    {
        return {QObject::tr("Encrypted"), {}, SecurityLevel::Secure};
    }
}
