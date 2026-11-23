#include "include/configs/outbounds/psiphon.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QLocale>

#include <algorithm>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/RuleSetMirror.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // Signed by Psiphon Inc. and verified on download, so a mirror cannot alter it.
        constexpr auto kPsiphonListGithub = "https://raw.githubusercontent.com/throneproj/routeprofiles/psiphon/server_list_compressed";
        // The double slash is part of Psiphon's real URL.
        constexpr auto kPsiphonListS3 = "https://s3.amazonaws.com//psiphon/web/mjr4-p23r-puwl/server_list_compressed";
    }

    bool psiphon::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty() || object["type"].toString() != "psiphon") return false;
        outbound::ParseFromJson(object);
        server.clear();
        server_port = 0;
        egress_region = object["egress_region"].toString().trimmed().toUpper();
        // The core pins the data folder and the fork brings Psiphon's public network values, so a foreign outbound's own are dropped.
        QStringList ignored;
        for (const auto *key : {"data_directory", "propagation_channel_id", "sponsor_id", "remote_server_list_url", "remote_server_list_urls",
                                "remote_server_list_signature_public_key", "obfuscated_server_list_root_urls", "network_id", "client_platform",
                                "client_version", "upstream_proxy_url", "establish_tunnel_timeout", "limit_tunnel_protocols", "tunnel_pool_size"}) {
            if (object.contains(QLatin1String(key))) ignored << QString::fromLatin1(key);
        }
        if (!ignored.isEmpty()) {
            ParseDiag::Warn(ignored.first(),
                            QCoreApplication::translate("Validation", "Ignored %1: Psiphon profiles take only a region.")
                                .arg(ignored.join(QStringLiteral(", "))));
        }
        return true;
    }

    QString psiphon::ExportToLink() {
        return {};
    }

    QJsonObject psiphon::ExportToJson() {
        QJsonObject object;
        object["type"] = "psiphon";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!egress_region.isEmpty()) object["egress_region"] = egress_region;
        return object;
    }

    QJsonObject psiphon::ExportIdentity() {
        QJsonObject object{{"type", "psiphon"}};
        if (!egress_region.isEmpty()) object["egress_region"] = egress_region;
        return object;
    }

    BuildResult psiphon::Build() {
        QJsonObject object{{"type", "psiphon"}};
        if (!egress_region.isEmpty()) {
            if (const auto message = Checks::PsiphonRegion(egress_region); !message.isEmpty()) return {{}, message};
            object["egress_region"] = egress_region;
        }
        // Tried in order: the chosen jsDelivr mirror gets through where GitHub is blocked, and Psiphon's own S3 copy comes last.
        const QString github = QString::fromLatin1(kPsiphonListGithub);
        QJsonArray lists;
        if (const auto mirrored = get_jsdelivr_link(github); mirrored != github) lists << mirrored;
        lists << github << QString::fromLatin1(kPsiphonListS3);
        object["remote_server_list_urls"] = lists;
        return {object, ""};
    }

    QString psiphon::DisplayAddress() {
        return PsiphonRegionName(egress_region);
    }

    QString psiphon::DisplayType() {
        return QStringLiteral("Psiphon");
    }

    SecurityInfo psiphon::GetSecurity() {
        return {QObject::tr("Encrypted"), {}, SecurityLevel::Secure};
    }

    void psiphon::Validate(Validation::Issues &issues) {
        outbound::Validate(issues);
        if (const auto message = Checks::PsiphonRegion(egress_region); !message.isEmpty())
            issues << Validation::Issue{Validation::Severity::Error, "egress_region", message, egress_region};
    }

    QString PsiphonRegionName(const QString &code) {
        return code.isEmpty() ? QObject::tr("Best performance") : TerritoryName(code);
    }

    namespace Checks {
        QString PsiphonRegion(const QString &code) {
            if (code.isEmpty()) return {};
            const bool letters = code.size() == 2 && std::ranges::all_of(code, [](QChar c) { return c >= u'A' && c <= u'Z'; });
            if (!letters || QLocale::codeToTerritory(code) == QLocale::AnyTerritory)
                return QCoreApplication::translate("Validation", "Use a two-letter country code, like US.");
            return {};
        }
    }
}
