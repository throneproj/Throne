#include "include/engine/services/AssetUpdates.h"

#include <algorithm>
#include <atomic>
#include <utility>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#ifdef Q_OS_MACOS
#include <QProcess>
#endif

#include "include/api/RPC.h"
#include "include/configs/outbounds/tor.h"
#include "include/engine/EnginePolicy.h"
#include "include/engine/EngineTr.h"
#include "include/engine/ThreadCheck.h"
#include "include/global/Configs.hpp"
#include "include/global/HTTPRequestHelper.hpp"

namespace Engine::AssetUpdates {
    namespace {
        // The core's reply when no box runs (core/internal/rpc/rulesets.go).
        constexpr auto kAssetUpdatesNotRunning = "no instance is running";

        std::atomic<bool> assetRuleSetBusy{false};
        // Bits of the GeoFiles being downloaded; only the main thread changes it.
        std::atomic<unsigned> assetGeoInFlight{0};
        // A geo prompt is pending or open; main thread only.
        bool assetXrayGeoPromptBusy = false;
        std::atomic<bool> assetGeoUnattendedFetch{false};
        // An unattended download runs, and the callers waiting for its clean result; main thread only.
        bool assetGeoUnattendedBusy = false;
        QList<std::function<void()>> assetGeoUnattendedWaiters;

        unsigned assetGeoBit(GeoFile file) {
            return 1u << static_cast<unsigned>(file);
        }

        QString assetGeoFileName(GeoFile file) {
            return file == GeoFile::GeoIp ? QStringLiteral("geoip.dat") : QStringLiteral("geosite.dat");
        }

        QString assetGeoFilePath(GeoFile file) {
            return Configs::GetBasePath() + "/" + assetGeoFileName(file);
        }

        bool assetGeoPresent(GeoFile file) {
            return QFileInfo(assetGeoFilePath(file)).isFile();
        }

        QString assetGeoConfiguredUrl(GeoFile file) {
            const auto &settings = Configs::dataManager->settingsRepo;
            return file == GeoFile::GeoIp ? settings->xray_geoip_url : settings->xray_geosite_url;
        }

        RuleSetResult assetRuleSetResult(bool rpcOK, int updated, const QString &error) {
            RuleSetResult result;
            result.coreReachable = rpcOK;
            if (!rpcOK) {
                result.error = error;
                return result;
            }
            if (error.trimmed() == QLatin1String(kAssetUpdatesNotRunning)) {
                result.notRunning = true;
                result.error = error.trimmed();
                return result;
            }
            result.updated = updated;
            for (const auto &line : error.split('\n', Qt::SkipEmptyParts)) {
                const QString failure = line.trimmed();
                if (!failure.isEmpty()) result.failures << failure;
            }
            return result;
        }
    } // namespace

    bool UpdateRuleSets(const std::function<void(const RuleSetResult &)> &done) {
        ENGINE_CHECK_MAIN_THREAD();
        if (assetRuleSetBusy.exchange(true)) return false;
        runOnNewThread([done] {
            bool rpcOK = false;
            int updated = 0;
            const QString error = API::defaultClient->UpdateRuleSets(&rpcOK, &updated);
            const RuleSetResult result = assetRuleSetResult(rpcOK, updated, error);
            if (result.notRunning) {
                MW_show_log(QStringLiteral("[Rule-sets] Not updated: no profile is running."));
            } else if (result.coreReachable) {
                QString line = QStringLiteral("[Rule-sets] %1 refreshed, %2 failed").arg(result.updated).arg(result.failures.size());
                if (!result.failures.isEmpty()) line += "\n" + result.failures.join("\n");
                MW_show_log(line);
            }
            runOnUiThread([done, result] {
                assetRuleSetBusy.store(false);
                if (done) done(result);
            });
        });
        return true;
    }

    bool RuleSetUpdateRunning() {
        return assetRuleSetBusy.load();
    }

    QList<GeoFileStatus> GeoStatus() {
        ENGINE_CHECK_MAIN_THREAD();
        QList<GeoFileStatus> statuses;
        for (const auto file : {GeoFile::GeoIp, GeoFile::GeoSite}) {
            GeoFileStatus status;
            status.file = file;
            status.fileName = assetGeoFileName(file);
            status.url = assetGeoConfiguredUrl(file);
            const QFileInfo info(assetGeoFilePath(file));
            status.present = info.isFile();
            if (status.present) {
                status.size = info.size();
                status.modifiedAt = info.lastModified().toSecsSinceEpoch();
            }
            statuses << status;
        }
        return statuses;
    }

    bool UpdateGeoAssets(const GeoRequest &request, const std::function<void(const QList<GeoFileResult> &)> &done) {
        ENGINE_CHECK_MAIN_THREAD();
        unsigned files = 0;
        for (const auto file : request.files) files |= assetGeoBit(file);
        // Two downloads of one file would race on its temp path; different files do not share one.
        if (assetGeoInFlight.load() & files) return false;
        assetGeoInFlight.fetch_or(files);
        QList<GeoFileResult> results;
        for (const auto file : request.files) {
            const bool duplicate = std::any_of(results.cbegin(), results.cend(), [file](const GeoFileResult &r) { return r.file == file; });
            if (duplicate) continue;
            const QString &custom = file == GeoFile::GeoIp ? request.geoipUrl : request.geositeUrl;
            GeoFileResult result;
            result.file = file;
            result.fileName = assetGeoFileName(file);
            result.url = custom.isEmpty() ? assetGeoConfiguredUrl(file) : custom;
            results << result;
        }
        const bool onlyMissing = request.onlyMissing;
        runOnNewThread([results, onlyMissing, files, done]() mutable {
            for (auto &result : results) {
                if (onlyMissing && assetGeoPresent(result.file)) {
                    result.skipped = true;
                    continue;
                }
                result.error = NetworkRequestHelper::DownloadAsset(result.url, result.fileName);
                if (result.error.isEmpty()) {
                    MW_show_log(QStringLiteral("[Geo assets] Downloaded %1.").arg(result.fileName));
                } else {
                    MW_show_log(QStringLiteral("[Geo assets] %1 download failed: %2").arg(result.fileName, result.error));
                }
            }
            runOnUiThread([results, files, done] {
                assetGeoInFlight.fetch_and(~files);
                if (done) done(results);
            });
        });
        return true;
    }

    bool GeoUpdateRunning() {
        return assetGeoInFlight.load() != 0;
    }

    GeoError ClassifyGeoError(const QString &coreError) {
        GeoError geo;
        geo.refGeoip = coreError.contains("geoip.dat");
        geo.refGeosite = coreError.contains("geosite.dat");
        geo.isGeo = geo.refGeoip || geo.refGeosite;
        if (!geo.isGeo) return geo;

        // A file that exists yet is still named in the error lacks the category; a fresh copy would not help.
        const bool geoipLacksCategory = geo.refGeoip && assetGeoPresent(GeoFile::GeoIp);
        const bool geositeLacksCategory = geo.refGeosite && assetGeoPresent(GeoFile::GeoSite);
        if (!geoipLacksCategory && !geositeLacksCategory) return geo;

        geo.missingCategory = true;
        geo.whichFile = assetGeoFileName(geositeLacksCategory ? GeoFile::GeoSite : GeoFile::GeoIp);
        const QString ruleType = geositeLacksCategory ? QStringLiteral("geosite") : QStringLiteral("geoip");
        static const QRegularExpression categoryRe(QStringLiteral("code\\s+(\\S+)\\s+from"));
        const auto match = categoryRe.match(coreError);
        if (match.hasMatch()) geo.needed = QStringLiteral("%1:%2").arg(ruleType, match.captured(1));
        return geo;
    }

    namespace {
        constexpr auto kAssetTorManifestUrl = "https://aus1.torproject.org/torbrowser/update_3/release/downloads.json";
        constexpr auto kAssetTorArchive = "tor-expert-bundle.tar.gz";

        std::atomic<bool> assetTorBusy{false};

        QString assetTorPlatform() {
#if defined(Q_OS_WIN) && defined(Q_PROCESSOR_X86_64)
            return QStringLiteral("windows-x86_64");
#elif defined(Q_OS_WIN)
            // There is no arm64 build, and Windows on ARM runs i686 everywhere.
            return QStringLiteral("windows-i686");
#elif defined(Q_OS_MACOS) && defined(Q_PROCESSOR_ARM_64)
            return QStringLiteral("macos-aarch64");
#elif defined(Q_OS_MACOS) && defined(Q_PROCESSOR_X86_64)
            return QStringLiteral("macos-x86_64");
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_ARM_64)
            return QStringLiteral("linux-aarch64");
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_X86_64)
            return QStringLiteral("linux-x86_64");
#else
            return {};
#endif
        }

#ifdef Q_OS_MACOS
        // Expert-bundle binaries are unsigned, and Apple Silicon kills unsigned code.
        QString assetTorAdHocSign() {
            const QDir dir(QStringLiteral("tor"));
            QStringList files{Configs::tor::ExecutablePath()};
            for (const auto &name : dir.entryList({QStringLiteral("*.dylib")}, QDir::Files)) files << dir.absoluteFilePath(name);
            for (const auto &file : files) {
                if (QProcess::execute(QStringLiteral("/usr/bin/codesign"), {QStringLiteral("--verify"), file}) == 0) continue;
                MW_show_log(QStringLiteral("[Tor] Signing %1 ad hoc.").arg(QFileInfo(file).fileName()));
                const int code = QProcess::execute(QStringLiteral("/usr/bin/codesign"), {QStringLiteral("--force"), QStringLiteral("--sign"), QStringLiteral("-"), file});
                if (code != 0) return QObject::tr("Could not sign %1 (codesign exit code %2).").arg(file).arg(code);
            }
            return {};
        }
#endif

        QString assetTorInstall(bool viaProxy) {
            const QString platform = assetTorPlatform();
            if (platform.isEmpty()) return QObject::tr("The Tor Project publishes no Tor build for this platform.");

            MW_show_log(QStringLiteral("[Tor] Looking up the latest version."));
            const auto manifest = NetworkRequestHelper::HttpGet(QString::fromLatin1(kAssetTorManifestUrl), viaProxy);
            if (!manifest.error.isEmpty()) return QObject::tr("Could not look up the Tor version: %1").arg(manifest.error);
            const QString version = QJsonDocument::fromJson(manifest.data).object().value("version").toString().trimmed();
            // It becomes a URL path segment.
            static const QRegularExpression versionRe(QStringLiteral("^[0-9][0-9a-z.]*$"));
            if (!versionRe.match(version).hasMatch()) return QObject::tr("The Tor download list names no usable version.");

            const QString folder = QStringLiteral("https://dist.torproject.org/torbrowser/%1/").arg(version);
            const QString fileName = QStringLiteral("tor-expert-bundle-%1-%2.tar.gz").arg(platform, version);
            const auto sums = NetworkRequestHelper::HttpGet(folder + "sha256sums-signed-build.txt", viaProxy);
            if (!sums.error.isEmpty()) return QObject::tr("Could not get the Tor checksums: %1").arg(sums.error);
            QString sha256;
            for (const auto &line : QString::fromUtf8(sums.data).split('\n')) {
                const auto fields = line.simplified().split(' ');
                if (fields.size() == 2 && fields[1] == fileName) {
                    sha256 = fields[0].toLower();
                    break;
                }
            }
            static const QRegularExpression sha256Re(QStringLiteral("^[0-9a-f]{64}$"));
            if (!sha256Re.match(sha256).hasMatch()) return QObject::tr("%1 is not in the Tor Project's checksum list.").arg(fileName);

            MW_show_log(QStringLiteral("[Tor] Downloading %1.").arg(fileName));
            const QString archivePath = Configs::GetBasePath() + "/" + kAssetTorArchive;
            QString error = NetworkRequestHelper::DownloadAsset(folder + fileName, QString::fromLatin1(kAssetTorArchive), viaProxy);
            if (error.isEmpty()) {
                MW_show_log(QStringLiteral("[Tor] Unpacking."));
                bool ok = false;
                error = API::defaultClient->InstallTor(&ok, archivePath, sha256);
                if (!ok && error.isEmpty()) error = QObject::tr("The core did not answer.");
            }
            QFile::remove(archivePath);
            if (!error.isEmpty()) return error;
#ifdef Q_OS_MACOS
            return assetTorAdHocSign();
#else
            return {};
#endif
        }
    } // namespace

    bool TorInstalled() {
        return Configs::tor::Installed();
    }

    bool InstallTor(std::function<void(const QString &error)> done) {
        ENGINE_CHECK_MAIN_THREAD();
        if (assetTorBusy.exchange(true)) return false;
        // Through the proxy when one runs; a start that fails on missing Tor must not lock out the download.
        const bool viaProxy = Configs::dataManager->settingsRepo->started_id >= 0;
        runOnNewThread([viaProxy, done = std::move(done)] {
            const QString error = assetTorInstall(viaProxy);
            if (error.isEmpty()) MW_show_log(QStringLiteral("[Tor] Installed."));
            else MW_show_log(QStringLiteral("[Tor] Install failed: %1").arg(error));
            runOnUiThread([done, error] {
                assetTorBusy.store(false);
                if (done) done(error);
            });
        });
        return true;
    }

    void SetUnattendedFetch(bool enabled) {
        assetGeoUnattendedFetch.store(enabled);
    }

    namespace {
        void assetGeoFetchUnattended(EnginePolicy *policy, const std::function<void()> &onInstalled) {
            ENGINE_CHECK_MAIN_THREAD();
            if (assetGeoUnattendedBusy) {
                if (onInstalled) assetGeoUnattendedWaiters << onInstalled;
                return;
            }
            GeoRequest request;
            request.files.clear();
            for (const auto &status : GeoStatus()) {
                if (!status.present) request.files << status.file;
            }
            request.onlyMissing = true;
            const bool started = UpdateGeoAssets(request, [policy](const QList<GeoFileResult> &results) {
                QString dlErr;
                for (const auto &result : results) {
                    if (!result.error.isEmpty()) dlErr += result.fileName + ": " + result.error + "\n";
                }
                assetGeoUnattendedBusy = false;
                const auto waiters = std::exchange(assetGeoUnattendedWaiters, {});
                policy->geoAssetDownloadFinished(dlErr);
                if (!dlErr.isEmpty()) return;
                for (const auto &waiter : waiters) waiter();
            });
            if (!started) {
                MW_show_log(QStringLiteral("[Geo assets] The missing files are already being downloaded by another request; nothing is retried."));
                return;
            }
            assetGeoUnattendedBusy = true;
            if (onInstalled) assetGeoUnattendedWaiters << onInstalled;
        }
    } // namespace

    bool HandleXrayGeoError(EnginePolicy *policy, const QString &coreError, const QString &contextName, bool prompt,
                            std::function<void()> onInstalled) {
        const auto classified = ClassifyGeoError(coreError);
        if (!classified.isGeo) return false;
        if (!prompt) {
            if (classified.missingCategory) {
                const QString needed = classified.needed.isEmpty() ? MainWindowTr::tr("a required category") : classified.needed;
                MW_show_log(MainWindowTr::tr("The Xray config \"%1\" needs \"%2\", but the installed %3 does not contain it.")
                                .arg(contextName, needed, classified.whichFile));
            } else {
                QStringList missing;
                if (classified.refGeoip) missing << QStringLiteral("geoip.dat");
                if (classified.refGeosite) missing << QStringLiteral("geosite.dat");
                if (assetGeoUnattendedFetch.load()) {
                    MW_show_log(QStringLiteral("[Geo assets] The Xray config \"%1\" needs geo data files that are not installed: %2. Downloading them.")
                                    .arg(contextName, missing.join(QStringLiteral(", "))));
                    // Queued even from the main thread, so the caller reports its failure before a retry can run.
                    if (auto *app = QCoreApplication::instance()) {
                        QMetaObject::invokeMethod(app, [policy, onInstalled = std::move(onInstalled)] {
                            assetGeoFetchUnattended(policy, onInstalled);
                        }, Qt::QueuedConnection);
                    }
                    return true;
                }
                MW_show_log(MainWindowTr::tr("The Xray config \"%1\" needs geo data files that are not installed: %2. Download them under Basic Settings > Core > Xray Core.")
                                .arg(contextName, missing.join(QStringLiteral(", "))));
            }
            return true;
        }

        runOnUiThread([=] {
            if (assetXrayGeoPromptBusy || GeoUpdateRunning()) return;
            assetXrayGeoPromptBusy = true;
            setTimeout([=] {
                const auto geo = ClassifyGeoError(coreError);
                if (geo.missingCategory) {
                    const QString needed = geo.needed.isEmpty() ? MainWindowTr::tr("a required category") : geo.needed;
                    policy->geoAssetMissingCategory(contextName, needed, geo.whichFile);
                    assetXrayGeoPromptBusy = false;
                    return;
                }

                const bool confirmed = policy->confirmGeoAssetDownload(contextName);
                assetXrayGeoPromptBusy = false;
                if (!confirmed) return;

                GeoRequest request;
                request.files.clear();
                for (const auto &status : GeoStatus()) {
                    if (!status.present) request.files << status.file;
                }
                request.onlyMissing = true;
                const bool started = UpdateGeoAssets(request, [policy](const QList<GeoFileResult> &results) {
                    QString dlErr;
                    for (const auto &result : results) {
                        if (!result.error.isEmpty()) dlErr += result.fileName + ": " + result.error + "\n";
                    }
                    if (dlErr.isEmpty()) MW_show_log(MainWindowTr::tr("Downloaded Xray geo asset files."));
                    policy->geoAssetDownloadFinished(dlErr);
                });
                // An API request may have started a download while the question was open.
                if (!started) MW_show_log(MainWindowTr::tr("Geo asset files are already being downloaded."));
            }, QCoreApplication::instance(), 300);
        });
        return true;
    }
} // namespace Engine::AssetUpdates
