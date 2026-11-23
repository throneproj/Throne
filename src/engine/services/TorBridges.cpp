#include "include/engine/services/TorBridges.h"

#include "include/api/RPC.h"
#include "include/configs/RuleSetMirror.h"
#include "include/configs/outbounds/tor.h"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/global/Logger.hpp"
#include "include/global/Utils.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QSaveFile>

#include <atomic>

namespace Engine::TorBridges {
    namespace {
        const QString kTorBridgesUrl = QStringLiteral("https://raw.githubusercontent.com/throneproj/routeprofiles/tor/bridges-v1.json");
        constexpr qint64 kTorBridgesAttemptIntervalSecs = 24 * 60 * 60;
        constexpr qint64 kTorBridgesMaxBytes = 256 * 1024;
        constexpr int kTorBridgesVersion = 1;

        // Guards the loaded state below; held across the IPC of a first load.
        QMutex torBridgesStateMutex;
        bool torBridgesLoaded = false;
        QHash<QString, QStringList> torBridgesBuiltin;
        QByteArray torBridgesDocument;

        // Two refreshes at once would race on the cache and its attempt stamp.
        QMutex torBridgesRefreshMutex;
        std::atomic<bool> torBridgesRefreshQueued{false};

        QString torBridgesCachePath() {
            return QDir::current().absoluteFilePath(QStringLiteral("tor_bridges.json"));
        }

        QString torBridgesMetaPath() {
            return QDir::current().absoluteFilePath(QStringLiteral("tor_bridges.meta.json"));
        }

        struct TorBridgesMeta {
            // Names the cached copy.
            QString etag;
            // Unix seconds of the last download attempt, failed or not.
            qint64 attemptedAt = 0;
        };

        TorBridgesMeta torBridgesReadMeta() {
            QFile file(torBridgesMetaPath());
            if (!file.open(QIODevice::ReadOnly)) return {};
            const auto json = QJsonDocument::fromJson(file.read(4096)).object();
            return {json.value(QStringLiteral("etag")).toString(), json.value(QStringLiteral("attempted_at")).toInteger()};
        }

        bool torBridgesWriteFile(const QString &path, const QByteArray &data) {
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit()) return true;
            LOG_WARN(QStringLiteral("[Tor] Cannot write %1: %2").arg(QDir::toNativeSeparators(path), file.errorString()));
            return false;
        }

        void torBridgesWriteMeta(const TorBridgesMeta &meta) {
            const QJsonObject json{{"etag", meta.etag}, {"attempted_at", meta.attemptedAt}};
            torBridgesWriteFile(torBridgesMetaPath(), QJsonDocument(json).toJson(QJsonDocument::Compact));
        }

        QByteArray torBridgesReadCache() {
            QFile file(torBridgesCachePath());
            if (!file.open(QIODevice::ReadOnly)) return {};
            const auto data = file.read(kTorBridgesMaxBytes + 1);
            return data.size() > kTorBridgesMaxBytes ? QByteArray() : data;
        }

        bool torBridgesUsable(bool rpcOK, const libcore::ParseTorBridgesResponse &parsed) {
            return rpcOK && parsed.error.value().empty() && parsed.version.value() == kTorBridgesVersion;
        }

        QHash<QString, QStringList> torBridgesLines(const libcore::ParseTorBridgesResponse &parsed) {
            QHash<QString, QStringList> lines;
            for (const auto &setting : parsed.builtin) {
                QStringList bridges;
                for (const auto &line : setting.bridges) bridges << QString::fromStdString(line);
                lines.insert(QString::fromStdString(setting.type.value()), bridges);
            }
            return lines;
        }

        bool torBridgesEmbedded(libcore::ParseTorBridgesResponse *embedded) {
            // Checked first: a call without the core would log an IPC failure on every Build() of a built-in profile.
            if (API::defaultClient == nullptr || !API::defaultClient->IsConnected()) return false;
            bool ok = false;
            *embedded = API::defaultClient->ParseTorBridges(&ok, {});
            return ok && embedded->error.value().empty();
        }

        // Caller holds torBridgesStateMutex; false leaves the next call to try again.
        bool torBridgesLoadLocked() {
            if (torBridgesLoaded) return true;
            libcore::ParseTorBridgesResponse embedded;
            if (!torBridgesEmbedded(&embedded)) return false;
            torBridgesBuiltin = torBridgesLines(embedded);
            torBridgesDocument.clear();
            if (const auto cached = torBridgesReadCache(); !cached.isEmpty()) {
                bool ok = false;
                const auto parsed = API::defaultClient->ParseTorBridges(&ok, cached);
                if (torBridgesUsable(ok, parsed) && parsed.revision.value() > embedded.revision.value()) {
                    torBridgesBuiltin = torBridgesLines(parsed);
                    torBridgesDocument = cached;
                }
            }
            torBridgesLoaded = true;
            return true;
        }

        void torBridgesRefresh() {
            QMutexLocker refreshLocker(&torBridgesRefreshMutex);
            auto meta = torBridgesReadMeta();
            const qint64 now = QDateTime::currentSecsSinceEpoch();
            // A stamp in the future means the clock went back, and waiting it out could take years.
            if (meta.attemptedAt > 0 && meta.attemptedAt <= now && now - meta.attemptedAt < kTorBridgesAttemptIntervalSecs) return;
            libcore::ParseTorBridgesResponse embedded;
            // Without the core nothing could be validated; the attempt is left for a later call.
            if (!torBridgesEmbedded(&embedded)) return;
            meta.attemptedAt = now;
            torBridgesWriteMeta(meta);

            bool cacheUsable = false;
            if (const auto cached = torBridgesReadCache(); !cached.isEmpty()) {
                bool ok = false;
                const auto parsed = API::defaultClient->ParseTorBridges(&ok, cached);
                cacheUsable = torBridgesUsable(ok, parsed);
            }
            HttpGetOptions options;
            options.maxBytes = kTorBridgesMaxBytes;
            // The ETag is only worth sending while the copy it names is still there.
            if (cacheUsable && !meta.etag.isEmpty()) options.headers.emplaceBack("If-None-Match", meta.etag.toLatin1());
            const auto response = NetworkRequestHelper::HttpGet(Configs::get_jsdelivr_link(kTorBridgesUrl), options);
            if (!response.error.isEmpty()) {
                LOG_WARN(QStringLiteral("[Tor] Bridge list download failed: %1").arg(response.error));
                return;
            }
            if (response.status == 304) return;

            bool ok = false;
            QString coreError;
            auto fresh = API::defaultClient->ParseTorBridges(&ok, response.data, &coreError);
            if (!torBridgesUsable(ok, fresh)) {
                QString why = coreError;
                if (ok) {
                    why = fresh.error.value().empty() ? QStringLiteral("unsupported version %1").arg(fresh.version.value())
                                                      : QString::fromStdString(fresh.error.value());
                }
                LOG_WARN(QStringLiteral("[Tor] Downloaded bridge list rejected: %1").arg(why));
                return;
            }
            if (fresh.revision.value() <= embedded.revision.value()) return;
            if (!torBridgesWriteFile(torBridgesCachePath(), response.data)) return;
            meta.etag = NetworkRequestHelper::GetHeader(response.header, QStringLiteral("ETag"));
            torBridgesWriteMeta(meta);

            QMutexLocker stateLocker(&torBridgesStateMutex);
            torBridgesBuiltin = torBridgesLines(fresh);
            torBridgesDocument = response.data;
            torBridgesLoaded = true;
        }
    } // namespace

    void Install() {
        Configs::tor::BuiltinBridges = [](const QString &type) { return BuiltinLines(type); };
    }

    QStringList BuiltinLines(const QString &type) {
        QMutexLocker locker(&torBridgesStateMutex);
        if (!torBridgesLoadLocked()) return {};
        return torBridgesBuiltin.value(type);
    }

    QByteArray NewerDocument() {
        QMutexLocker locker(&torBridgesStateMutex);
        if (!torBridgesLoadLocked()) return {};
        return torBridgesDocument;
    }

    void RefreshInBackground() {
        if (torBridgesRefreshQueued.exchange(true)) return;
        runOnNewThread([] {
            torBridgesRefresh();
            torBridgesRefreshQueued.store(false);
        });
    }
} // namespace Engine::TorBridges
