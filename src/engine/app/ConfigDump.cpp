#include "include/engine/app/ConfigDump.h"

#include <algorithm>
#include <cstdio>
#include <utility>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QScopeGuard>

#include "include/api/RPC.h"
#include "include/configs/common/xrayStreamSetting.h"
#include "include/configs/generate.h"
#include "include/database/OtpProfilesRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/engine/app/Bootstrap.h"
#include "include/engine/app/DataDirLock.h"
#include "include/engine/app/ExitCodes.h"
#include "include/engine/app/InstanceServer.h"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#ifdef Q_OS_LINUX
#include "include/sys/linux/systemChecks.h"
#endif
#ifdef Q_OS_WIN
#include <windows.h>
// <windows.h> defines SetPort, which under unity builds clobbers Configs::outbound::SetPort.
#undef SetPort
#endif

namespace Engine {
    namespace {
        const QString kPort = QStringLiteral("<port>");
        const QString kVolatile = QStringLiteral("<volatile>");

        void PrintError(const QString &text) {
#ifdef Q_OS_WIN
            if (_fileno(stderr) < 0) return;
#endif
            std::fputs(text.toLocal8Bit().append('\n').constData(), stderr);
            std::fflush(stderr);
        }

        // Bridge credentials come from GetRandomString(32) and are always used as both user and password.
        bool IsBridgeSecret(const QJsonValue &user, const QJsonValue &pass) {
            static const QRegularExpression secretRe(QStringLiteral("^[A-Za-z0-9]{32}$"));
            return user.isString() && user == pass && secretRe.match(user.toString()).hasMatch();
        }

        bool IsSingleBridgeAccount(const QJsonValue &accounts, const QString &userKey, const QString &passKey) {
            const auto list = accounts.toArray();
            if (list.size() != 1) return false;
            const auto account = list.first().toObject();
            return IsBridgeSecret(account.value(userKey), account.value(passKey));
        }

        QJsonValue MaskVolatile(const QJsonValue &value);

        // The bridge ports come from MkManyPorts and the credentials from GetRandomString, both fresh on every build.
        QJsonObject MaskVolatileObject(QJsonObject object) {
            static const QRegularExpression portTagRe(QStringLiteral("^bridge-\\d+$"));
            // sing-box socks outbound into a bridge, the custom Xray full config one included.
            if (IsBridgeSecret(object.value("username"), object.value("password"))) {
                object["username"] = kVolatile;
                object["password"] = kVolatile;
                if (object.contains("server_port")) object["server_port"] = kPort;
            }
            // Settings of an Xray socks outbound into a sing-box bridge.
            if (IsBridgeSecret(object.value("user"), object.value("pass"))) {
                object["user"] = kVolatile;
                object["pass"] = kVolatile;
                if (object.contains("port")) object["port"] = kPort;
            }
            // sing-box socks bridge inbound; the test box names it after its port.
            if (IsSingleBridgeAccount(object.value("users"), "username", "password")) {
                object["users"] = QJsonArray{QJsonObject{{"username", kVolatile}, {"password", kVolatile}}};
                if (object.contains("listen_port")) object["listen_port"] = kPort;
                if (portTagRe.match(object.value("tag").toString()).hasMatch()) object["tag"] = "bridge-" + kPort;
            }
            // Xray socks bridge inbound.
            if (auto settings = object.value("settings").toObject(); IsSingleBridgeAccount(settings.value("accounts"), "user", "pass")) {
                settings["accounts"] = QJsonArray{QJsonObject{{"user", kVolatile}, {"pass", kVolatile}}};
                object["settings"] = settings;
                if (object.contains("port")) object["port"] = kPort;
            }
            // Warm entries are aged against the build time.
            if (object.value("type").toString() == "auto-selector" && object.contains("warm")) object["warm"] = kVolatile;
            for (auto it = object.begin(); it != object.end(); ++it) it.value() = MaskVolatile(it.value());
            return object;
        }

        QJsonValue MaskVolatile(const QJsonValue &value) {
            if (value.isObject()) return MaskVolatileObject(value.toObject());
            if (!value.isArray()) return value;
            auto list = value.toArray();
            for (auto it = list.begin(); it != list.end(); ++it) *it = MaskVolatile(*it);
            return list;
        }

        QJsonObject MaskSingBoxConfig(const QJsonObject &config) {
            auto masked = MaskVolatileObject(config);
            // The api secret is a random UUID minted per data dir.
            if (masked.contains("services")) {
                auto services = masked["services"].toArray();
                for (auto it = services.begin(); it != services.end(); ++it) {
                    auto service = it->toObject();
                    if (service.value("type").toString() == "api" && service.contains("secret")) service["secret"] = kVolatile;
                    *it = service;
                }
                masked["services"] = services;
            }
            // The GUI re-rolls the mixed port on every start when random_inbound_port is set.
            if (Configs::dataManager->settingsRepo->random_inbound_port && masked.contains("inbounds")) {
                auto inbounds = masked["inbounds"].toArray();
                for (auto it = inbounds.begin(); it != inbounds.end(); ++it) {
                    auto inbound = it->toObject();
                    if (inbound.value("tag").toString() == "mixed-in" && inbound.contains("listen_port")) inbound["listen_port"] = kPort;
                    *it = inbound;
                }
                masked["inbounds"] = inbounds;
            }
            return masked;
        }

        QJsonValue ParseEmbeddedConfig(const QString &text) {
            const auto doc = QJsonDocument::fromJson(text.toUtf8());
            if (!doc.isObject()) return text;
            return MaskVolatile(doc.object());
        }

        QJsonObject FullConfigDocument(const Configs::BuildConfigResult &result) {
            if (!result.error.isEmpty()) return {{"error", result.error}};
            QJsonObject doc{{"singbox", MaskSingBoxConfig(result.coreConfig)}};
            if (!result.tunIPv4CIDR.isEmpty()) doc["tun_ipv4_cidr"] = result.tunIPv4CIDR;
            if (!result.xrayConfig.isEmpty()) doc["xray"] = MaskVolatile(result.xrayConfig);
            QJsonArray fullConfigs;
            for (const auto &text : result.xrayFullConfigs) fullConfigs << ParseEmbeddedConfig(text);
            if (!fullConfigs.isEmpty()) doc["xray_full_configs"] = fullConfigs;
            if (!result.xrayConfig.isEmpty() || !fullConfigs.isEmpty()) {
                doc["xray_outbound_dns_strategy"] = Configs::getXrayOutboundDomainStrategy();
            }
            if (const auto &extra = result.extraCoreData; extra != nullptr && !extra->path.isEmpty()) {
                doc["extra_core"] = QJsonObject{
                    {"path", extra->path},
                    {"args", extra->args},
                    {"config", extra->config},
                    {"no_log", extra->noLog},
                };
            }
            return doc;
        }

        QJsonObject TestConfigDocument(const Configs::BuildTestConfigResult &result) {
            if (!result.error.isEmpty()) return {{"error", result.error}};
            QJsonObject doc{{"singbox", MaskVolatile(result.coreConfig)}};
            if (!result.xrayConfig.isEmpty()) doc["xray"] = MaskVolatile(result.xrayConfig);
            QJsonArray xrayFullConfigs;
            for (const auto &text : result.xrayFullConfigs) xrayFullConfigs << ParseEmbeddedConfig(text);
            if (!xrayFullConfigs.isEmpty()) doc["xray_full_configs"] = xrayFullConfigs;
            QJsonObject customFullConfigs;
            for (auto it = result.fullConfigs.cbegin(); it != result.fullConfigs.cend(); ++it) {
                customFullConfigs[QString::number(it.key())] = ParseEmbeddedConfig(it.value());
            }
            if (!customFullConfigs.isEmpty()) doc["custom_full_configs"] = customFullConfigs;
            doc["outbound_tags"] = QJsonArray::fromStringList(result.outboundTags);
            QJsonObject tagProfiles;
            for (auto it = result.tag2entID.cbegin(); it != result.tag2entID.cend(); ++it) tagProfiles[it.key()] = it.value();
            doc["tag_profiles"] = tagProfiles;
            if (!result.xrayDnsStrategy.isEmpty()) doc["xray_dns_strategy"] = result.xrayDnsStrategy;
            return doc;
        }

        QString PlatformName() {
#if defined(Q_OS_WIN)
            return "windows";
#elif defined(Q_OS_MACOS)
            return "macos";
#elif defined(Q_OS_LINUX)
            return "linux";
#else
            return "unix";
#endif
        }

        class Dumper {
        public:
            explicit Dumper(const QDir &outDir) : out(outDir) {}

            // A build error is data: it is written to the file and listed in the manifest.
            QJsonObject write(const QString &name, const QJsonObject &doc) {
                QJsonObject entry{{"file", name}};
                if (doc.contains("error")) entry["error"] = doc.value("error");
                QFile file(out.absoluteFilePath(name));
                const auto data = QJsonDocument(doc).toJson(QJsonDocument::Indented);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(data) != data.size()) {
                    PrintError(QString("-dump-configs: could not write %1: %2").arg(file.fileName(), file.errorString()));
                    writeFailures++;
                }
                return entry;
            }

            // The builders dereference the outbound unchecked.
            QJsonObject full(const std::shared_ptr<Configs::Profile> &ent, const QString &name) {
                if (ent->outbound == nullptr) return write(name, {{"error", "the profile has no outbound"}});
                return write(name, FullConfigDocument(*Configs::BuildSingBoxConfig(ent)));
            }

            QJsonObject test(const std::shared_ptr<Configs::Profile> &ent, const QString &name) {
                if (ent->outbound == nullptr) return write(name, {{"error", "the profile has no outbound"}});
                return write(name, TestConfigDocument(*Configs::BuildTestConfig({ent})));
            }

            int writeFailures = 0;

        private:
            QDir out;
        };

        QString ProfileName(const std::shared_ptr<Configs::Profile> &ent) {
            return ent->outbound != nullptr ? ent->outbound->DisplayName() : ent->name;
        }

        void CollectDumpFiles(const QJsonValue &value, QStringList &files) {
            static const QRegularExpression dumpFile(R"(^(profile|route)-[^/\\]+\.json$)");
            if (value.isObject()) {
                const auto object = value.toObject();
                for (auto it = object.begin(); it != object.end(); ++it) {
                    if (it.key() == "file" && dumpFile.match(it.value().toString()).hasMatch()) files << it.value().toString();
                    else CollectDumpFiles(it.value(), files);
                }
            } else if (value.isArray()) {
                for (const auto &item : value.toArray()) CollectDumpFiles(item, files);
            }
        }

        // The output dir may be any directory, '.' included: only files a previous dump's manifest lists are removed.
        bool RemovePreviousDump(QDir outDir) {
            const auto clashing = outDir.entryList({"profile-*.json", "route-*.json", "manifest.json"}, QDir::Files | QDir::Hidden | QDir::System);
            if (clashing.isEmpty()) return true;
            const auto manifest = QJsonDocument::fromJson(ReadFile(outDir.absoluteFilePath("manifest.json"))).object();
            if (manifest.value("format").toInt() != 1 || !manifest.value("profiles").isArray()) {
                PrintError(QString("-dump-configs: %1 holds %2, which no previous dump wrote; use an empty directory")
                               .arg(QDir::toNativeSeparators(outDir.absolutePath()), clashing.first()));
                return false;
            }
            QStringList files;
            CollectDumpFiles(manifest, files);
            files << "manifest.json";
            files.removeDuplicates();
            for (const auto &name : files) {
                if (!outDir.exists(name) || outDir.remove(name)) continue;
                PrintError(QString("-dump-configs: could not remove the previous %1").arg(outDir.absoluteFilePath(name)));
                return false;
            }
            return true;
        }

        bool CanBuildRoutes(const QJsonObject &profileEntry) {
            return !profileEntry.value("config").toObject().contains("error") && profileEntry.value("type").toString() != "autoselector";
        }

        QJsonObject Manifest(const QJsonArray &profiles, const QJsonArray &routes, int routeProfileId, int currentRouteId) {
            QJsonObject manifest{
                {"format", 1},
                {"platform", PlatformName()},
                {"current_route_id", currentRouteId},
                {"route_dump_profile_id", routeProfileId},
                {"profiles", profiles},
                {"routes", routes},
                {"placeholders", QJsonObject{
                    {kPort, "a port picked at build time (sing-box <-> Xray bridges; the mixed inbound when random_inbound_port is set)"},
                    {kVolatile, "a per-build or per-data-dir random value (bridge credentials, the api service secret, auto-selector warm entries)"},
                }},
                {"notes", QJsonArray{
                    "No core runs: IsValid() makes only its local checks and counts the core's config check as passed.",
                    "OTP codes are not resolved; {otp} placeholders stay in the credentials.",
                }},
            };
#ifdef Q_OS_LINUX
            manifest["systemd_resolved"] = isSystemdResolvedDefaultResolver();
#endif
            return manifest;
        }
    } // namespace

    int RunConfigDump(int argc, char **argv) {
        const QDir launchCwd = QDir::current();
#ifdef Q_OS_WIN
        // A GUI-subsystem exe gets no stderr unless redirected; borrow the launching console instead.
        if (_fileno(stderr) < 0 && AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE *console = nullptr;
            freopen_s(&console, "CONOUT$", "w", stderr);
        }
#endif
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName("Throne");
        Logging::EnableStdSink(Logging::Level::Info);

        QStringList arguments = QCoreApplication::arguments();
        const auto flagIndex = arguments.indexOf("-dump-configs");
        const auto appdataIndex = arguments.indexOf("-appdata");
        const auto hasValue = [&arguments](qsizetype index) {
            return index >= 0 && index + 1 < arguments.size() && !arguments.at(index + 1).startsWith('-');
        };
        if (!hasValue(flagIndex) || !hasValue(appdataIndex)) {
            PrintError("usage: Throne -dump-configs <outdir> -appdata <dir>   (<dir> should be a copy of a data directory)");
            return ExitCode::Usage;
        }
        const QDir outDir(QDir::cleanPath(launchCwd.absoluteFilePath(arguments.at(flagIndex + 1))));
        const QString appdataPath = QDir::cleanPath(launchCwd.absoluteFilePath(arguments.at(appdataIndex + 1)));
        if (const auto dbPath = QDir(appdataPath).absoluteFilePath("config/throne.db"); !QFileInfo(dbPath).isFile()) {
            PrintError(QString("-dump-configs: no database at %1").arg(QDir::toNativeSeparators(dbPath)));
            return ExitCode::NoInput;
        }
        arguments[appdataIndex + 1] = appdataPath;
        PrintError("-dump-configs: run this on a COPY of the data directory, opening the database may migrate it.");

        const auto dataDir = Bootstrap::ResolveDataDir(arguments, launchCwd);
        Configs::SetBasePath(dataDir.wd.absolutePath());

        DataDirLock lock;
        switch (lock.tryAcquire(dataDir.configDir, "dump-configs")) {
            case DataDirLock::Result::Busy:
                PrintError(QString("-dump-configs: %1 is in use by %2")
                               .arg(dataDir.wd.absolutePath(), DataDirLock::DescribeOwner(DataDirLock::ReadOwner(dataDir.configDir))));
                return ExitCode::DataDirOwned;
            case DataDirLock::Result::Failed:
                LOG_WARN("-dump-configs: continuing without the data dir lock: " + lock.errorString());
                break;
            case DataDirLock::Result::Acquired:
                if (!lock.errorString().isEmpty()) LOG_WARN(lock.errorString());
                break;
        }
        // A Throne older than the lock, or a failed lock, leaves the instance socket as the only sign of a live owner.
        if (InstanceServer::HandOff(Bootstrap::InstanceServerName(dataDir.wd), {})) {
            PrintError(QString("-dump-configs: a running Throne owns %1").arg(dataDir.wd.absolutePath()));
            return ExitCode::DataDirOwned;
        }

        if (!QDir().mkpath(outDir.absolutePath())) {
            PrintError(QString("-dump-configs: could not create %1").arg(outDir.absolutePath()));
            return ExitCode::CantCreate;
        }
        if (!RemovePreviousDump(outDir)) return ExitCode::CantCreate;

        QDir::setCurrent(dataDir.configDir);
        Configs::initDB(QString(QDir::currentPath() + QDir::separator() + "throne.db").toStdString());
        auto &settings = *Configs::dataManager->settingsRepo;
        settings.noSave = true;
        settings.flag_use_appdata = dataDir.useAppdata;
        if (dataDir.useAppdata && !dataDir.appdataDir.isEmpty()) settings.appdataDir = dataDir.appdataDir;
        LOG_INFO(QString("-dump-configs: data dir %1, output %2").arg(dataDir.wd.absolutePath(), outDir.absolutePath()));

        // Never connected: a core check that slips past the offline scope fails fast instead of dereferencing a null client.
        API::defaultClient = new API::Client();
        const Configs::ScopedOfflineValidation offlineValidation;
        // ResolveOtpCode answers empty without the repo: HOTP would bump and save its counter, TOTP follows the clock.
        auto otpRepo = std::exchange(Configs::dataManager->otpProfilesRepo, nullptr);
        const auto restoreRuntime = qScopeGuard([&otpRepo] {
            Configs::dataManager->otpProfilesRepo = std::move(otpRepo);
            delete std::exchange(API::defaultClient, nullptr);
        });

        auto ids = Configs::dataManager->profilesRepo->GetAllProfileIds();
        std::sort(ids.begin(), ids.end());
        Dumper dumper(outDir);

        QList<QJsonObject> profileEntries;
        QList<int> dumpedIds;
        for (const int id : ids) {
            const auto ent = Configs::dataManager->profilesRepo->GetProfile(id);
            if (ent == nullptr) continue;
            const auto base = QString("profile-%1").arg(id);
            QJsonObject entry{
                {"id", id},
                {"name", ProfileName(ent)},
                {"type", ent->type},
                {"group", ent->gid},
            };
            entry["config"] = dumper.full(ent, base + ".json");
            {
                const bool tun = std::exchange(settings.spmode_vpn, true);
                const auto restoreTun = qScopeGuard([&settings, tun] { settings.spmode_vpn = tun; });
                entry["tun"] = dumper.full(ent, base + ".tun.json");
            }
            profileEntries << entry;
            dumpedIds << id;
        }

        const int currentRouteId = settings.current_route_id;
        int routeProfileId = -1;
        if (const auto at = dumpedIds.indexOf(settings.remember_id); at >= 0 && CanBuildRoutes(profileEntries[at])) {
            routeProfileId = settings.remember_id;
        } else {
            for (qsizetype i = 0; i < dumpedIds.size(); i++) {
                if (!CanBuildRoutes(profileEntries[i])) continue;
                routeProfileId = dumpedIds[i];
                break;
            }
        }
        QJsonArray routeEntries;
        auto routeIds = Configs::dataManager->routesRepo->GetAllRouteProfileIds();
        std::sort(routeIds.begin(), routeIds.end());
        for (const int routeId : routeIds) {
            const auto route = Configs::dataManager->routesRepo->GetRouteProfile(routeId);
            if (route == nullptr) continue;
            QJsonObject entry{{"id", routeId}, {"name", route->name}};
            const auto ent = Configs::dataManager->profilesRepo->GetProfile(routeProfileId);
            if (ent == nullptr) {
                entry["error"] = "no profile to build the route with";
                routeEntries << entry;
                continue;
            }
            settings.current_route_id = routeId;
            const auto restoreRoute = qScopeGuard([&settings, currentRouteId] { settings.current_route_id = currentRouteId; });
            const auto written = dumper.full(ent, QString("route-%1.json").arg(routeId));
            for (auto it = written.begin(); it != written.end(); ++it) entry[it.key()] = it.value();
            routeEntries << entry;
        }

        // Last: a failed check marks the profile's in-memory latency, which an auto selector build would read.
        QJsonArray profilesOut;
        for (qsizetype i = 0; i < dumpedIds.size(); i++) {
            auto entry = profileEntries[i];
            if (const auto ent = Configs::dataManager->profilesRepo->GetProfile(dumpedIds[i]); ent != nullptr) {
                entry["test"] = dumper.test(ent, QString("profile-%1.test.json").arg(dumpedIds[i]));
            }
            profilesOut << entry;
        }

        dumper.write("manifest.json", Manifest(profilesOut, routeEntries, routeProfileId, currentRouteId));
        LOG_INFO(QString("-dump-configs: %1 profiles, %2 routes written to %3")
                     .arg(profilesOut.size()).arg(routeEntries.size()).arg(outDir.absolutePath()));
        return dumper.writeFailures > 0 ? ExitCode::IoError : ExitCode::Ok;
    }
} // namespace Engine
