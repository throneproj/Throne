#pragma once

#include <functional>

#include <QList>
#include <QString>
#include <QStringList>

namespace Engine {
    class EnginePolicy;
}

namespace Engine::AssetUpdates {
    struct RuleSetResult {
        bool coreReachable = false;
        // The core answered that no profile runs, so no rule-set is loaded.
        bool notRunning = false;
        int updated = 0;
        // "tag: error" lines from the core.
        QStringList failures;
        // The IPC error, or the core's not-running text.
        QString error;
    };

    // Main thread; false while an update runs. Asks the core to refresh every remote rule-set; done runs on the main thread.
    bool UpdateRuleSets(const std::function<void(const RuleSetResult &)> &done);

    bool RuleSetUpdateRunning();

    enum class GeoFile {
        GeoIp,
        GeoSite,
    };

    struct GeoFileStatus {
        GeoFile file = GeoFile::GeoIp;
        QString fileName;
        // The configured download URL.
        QString url;
        bool present = false;
        qint64 size = 0;
        // Unix seconds, 0 when absent.
        qint64 modifiedAt = 0;
    };

    // Main thread.
    QList<GeoFileStatus> GeoStatus();

    struct GeoRequest {
        QList<GeoFile> files{GeoFile::GeoIp, GeoFile::GeoSite};
        bool onlyMissing = false;
        // Empty: the URL from the settings.
        QString geoipUrl;
        QString geositeUrl;
    };

    struct GeoFileResult {
        GeoFile file = GeoFile::GeoIp;
        QString fileName;
        QString url;
        // Empty on success.
        QString error;
        // Present and onlyMissing was set.
        bool skipped = false;
    };

    // Main thread; false while one of the requested files is already downloading. Downloads the files one after another;
    // done runs on the main thread.
    bool UpdateGeoAssets(const GeoRequest &request, const std::function<void(const QList<GeoFileResult> &)> &done);

    bool GeoUpdateRunning();

    struct GeoError {
        // The core error names geoip.dat or geosite.dat.
        bool isGeo = false;
        bool refGeoip = false;
        bool refGeosite = false;
        // The file exists but lacks the category; downloading it again cannot help.
        bool missingCategory = false;
        QString needed;
        QString whichFile;
    };

    // Any thread; classifies a core error the way handleXrayGeoAssetError did.
    GeoError ClassifyGeoError(const QString &coreError);

    // Main thread, once from EngineHost::create().
    void SetUnattendedFetch(bool enabled);

    // Any thread; returns whether coreError is a geo-asset one. With prompt the policy is asked on the main thread whether to
    // fetch the missing files. Without it, the error is only logged, unless unattended fetching is on and the files are missing
    // (not a missing category): then they are downloaded on the main thread and onInstalled runs there after a clean result.
    // policy outlives the prompt.
    bool HandleXrayGeoError(EnginePolicy *policy, const QString &coreError, const QString &contextName, bool prompt,
                            std::function<void()> onInstalled = {});
} // namespace Engine::AssetUpdates
