#include "include/api/remote/Router.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <utility>

#include "include/configs/sub/GroupUpdater.hpp"
#include "include/engine/EditLocks.h"
#include "include/engine/EngineControl.h"
#include "include/engine/services/BackupArchive.h"
#include "include/engine/services/SettingsRegistry.h"
#include "include/global/Configs.hpp"

namespace RemoteApi::RouterBackupImpl {
    constexpr qsizetype kKeptBackups = 2;
    constexpr qsizetype kOpenUploads = 2;
    constexpr qint64 kIdleMs = 10 * 60 * 1000;
    constexpr int kSweepMs = 60 * 1000;
    constexpr qint64 kDefaultSlice = 4 * 1024 * 1024;
    constexpr qint64 kMaxSlice = 16 * 1024 * 1024;
    constexpr qint64 kChunkLimit = 512 * 1024;
    constexpr qint64 kMaxUpload = qint64(1) << 30;
    constexpr int kBuildWaitSec = 120;
    constexpr int kInspectWaitSec = 120;
    constexpr int kApplyWaitSec = 60;

    Response busy(const QString &message) {
        return Response::Error(409, QStringLiteral("busy"), message);
    }

    Response internal(const QString &message) {
        return Response::Error(500, QStringLiteral("internal"), message);
    }

    Response inProgress(int id, const QString &state) {
        return Response::Ok(QJsonObject{{"id", id}, {"state", state}}, 202);
    }

    Configs::BackupParts everyPart() {
        return Configs::BackupParts{.profiles = true, .routes = true, .settings = true, .otp = true, .ipLists = true, .icons = true};
    }

    // Leaves *names and *parts alone without "parts"; false with *error for a malformed list or an unknown name.
    bool readParts(const QJsonObject &body, std::optional<QStringList> *names, Configs::BackupParts *parts, QString *error) {
        const auto value = body.value(QStringLiteral("parts"));
        if (value.isUndefined() || value.isNull()) return true;
        const auto malformed = QStringLiteral("\"parts\" must be an array of part names");
        if (!value.isArray()) {
            *error = malformed;
            return false;
        }
        QStringList list;
        for (const auto &item : value.toArray()) {
            if (!item.isString()) {
                *error = malformed;
                return false;
            }
            list.append(item.toString());
        }
        list.removeDuplicates();
        if (list.isEmpty()) {
            *error = QStringLiteral("\"parts\" names no part");
            return false;
        }
        Configs::BackupParts chosen;
        QString unknown;
        if (!Engine::BackupArchive::PartsFromNames(list, &chosen, &unknown)) {
            *error = QStringLiteral("Unknown part \"%1\"; the parts are profiles, routes, settings, otp, ip_lists and icons").arg(unknown);
            return false;
        }
        *names = list;
        *parts = chosen;
        return true;
    }

    // Any thread. Returns an error text, empty on success.
    QString digestFile(const QString &path, qint64 *size, QString *sha256) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("Could not read the backup: %1").arg(file.errorString());
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file)) return QStringLiteral("Could not read the backup: %1").arg(file.errorString());
        *size = file.size();
        *sha256 = QString::fromLatin1(hash.result().toHex());
        return {};
    }

    bool isSha256Hex(const QString &text) {
        return text.size() == 64 && std::all_of(text.begin(), text.end(), [](QChar c) {
                   return (c >= QLatin1Char('0') && c <= QLatin1Char('9')) || (c >= QLatin1Char('a') && c <= QLatin1Char('f')) ||
                          (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
               });
    }

    // False when the query parameter is present but not an integer.
    bool queryInteger(const Request &request, const QString &key, qint64 fallback, qint64 *out) {
        if (!request.query.contains(key)) {
            *out = fallback;
            return true;
        }
        bool ok = false;
        *out = request.query.value(key).toLongLong(&ok);
        return ok;
    }

    void makeParentDir(const QString &path) {
        QDir().mkpath(QFileInfo(path).absolutePath());
    }

    // Retries while a window restart waits on its UAC prompt; the restored database must not keep running.
    void requestRestart(Engine::EngineControl *engine, QObject *context) {
        if (engine->requestExit(Engine::ExitReason::Restart) || Configs::dataManager->settingsRepo->prepare_exit) return;
        QTimer::singleShot(1000, context, [engine, context] { requestRestart(engine, context); });
    }
}

namespace RemoteApi {
    struct Router::Transfer {
        int id = 0;
        bool upload = false;
        QString path;
        qint64 size = 0;
        qint64 received = 0;
        // Lowercase hex; for an upload, the digest the client announced.
        QString sha256;
        // Uploads only: the digest of the bytes received so far.
        std::shared_ptr<QCryptographicHash> hash;
        // A backup being built, or an upload whose archive is being inspected.
        bool working = false;
        bool complete = false;
        // Why the uploaded archive was refused after its waiter had already been answered; kept for GET.
        QString failure;
        QStringList parts;
        QJsonObject archive;
        qint64 touchedAt = 0;
        // The request waiting for the build or the inspection.
        WaiterPtr waiter;

        void touch() { touchedAt = QDateTime::currentMSecsSinceEpoch(); }

        static std::shared_ptr<Transfer> find(const QList<std::shared_ptr<Transfer>> &list, int id);

        // Forgets the transfer and deletes its file.
        static void drop(QList<std::shared_ptr<Transfer>> &list, const std::shared_ptr<Transfer> &transfer);

        // Adds the transfer and runs the expiry sweep, which stops once no transfer is left.
        static void keep(Router *router, QList<std::shared_ptr<Transfer>> &list, const std::shared_ptr<Transfer> &transfer);
    };

    std::shared_ptr<Router::Transfer> Router::Transfer::find(const QList<std::shared_ptr<Transfer>> &list, int id) {
        for (const auto &transfer : list) {
            if (transfer->id == id) return transfer;
        }
        return nullptr;
    }

    void Router::Transfer::drop(QList<std::shared_ptr<Transfer>> &list, const std::shared_ptr<Transfer> &transfer) {
        list.removeAll(transfer);
        // Fails while a worker still has the file open; the worker's report removes it then.
        QFile::remove(transfer->path);
    }

    void Router::Transfer::keep(Router *router, QList<std::shared_ptr<Transfer>> &list, const std::shared_ptr<Transfer> &transfer) {
        transfer->touch();
        list.append(transfer);
        if (router->transferSweep_ == nullptr) {
            router->transferSweep_ = new QTimer(router);
            router->transferSweep_->setInterval(RouterBackupImpl::kSweepMs);
            QObject::connect(router->transferSweep_, &QTimer::timeout, router, [router] {
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                for (auto *transfers : {&router->backups_, &router->restores_}) {
                    const auto snapshot = *transfers;
                    for (const auto &entry : snapshot) {
                        if (!entry->working && now - entry->touchedAt >= RouterBackupImpl::kIdleMs) drop(*transfers, entry);
                    }
                }
                if (router->backups_.isEmpty() && router->restores_.isEmpty()) router->transferSweep_->stop();
            });
        }
        if (!router->transferSweep_->isActive()) router->transferSweep_->start();
    }

    void Router::postBackup(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        WaitOptions options;
        if (!bodyObject(request, false, &body, &error) ||
            !readWait(body, true, RouterBackupImpl::kBuildWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        std::optional<QStringList> names;
        Configs::BackupParts parts = RouterBackupImpl::everyPart();
        QString problem;
        if (!RouterBackupImpl::readParts(body, &names, &parts, &problem)) {
            respond(badRequest(problem));
            return;
        }
        if (backups_.size() >= RouterBackupImpl::kKeptBackups) {
            respond(RouterBackupImpl::busy(
                QStringLiteral("Throne keeps %1 backups at most; delete one first").arg(RouterBackupImpl::kKeptBackups)));
            return;
        }

        Engine::BackupArchive::Prepare(parts);
        auto transfer = std::make_shared<Transfer>();
        transfer->id = ++lastTransferId_;
        transfer->path = Engine::BackupArchive::TempPath(QStringLiteral("api-backup"));
        transfer->working = true;
        RouterBackupImpl::makeParentDir(transfer->path);
        Transfer::keep(this, backups_, transfer);
        const int id = transfer->id;
        if (options.wait) {
            transfer->waiter = makeWaiter(respond, options.timeoutMs, [id] {
                return RouterBackupImpl::inProgress(id, QStringLiteral("building"));
            });
        } else {
            respond(RouterBackupImpl::inProgress(id, QStringLiteral("building")));
        }

        const QPointer<Router> self(this);
        runOnNewThread([self, id, parts, path = transfer->path] {
            qint64 size = 0;
            QString sha256;
            Configs::BackupParts written;
            QString failure = Engine::BackupArchive::Build(parts, path, &written);
            if (failure.isEmpty()) failure = RouterBackupImpl::digestFile(path, &size, &sha256);
            runOnUiThread([self, id, path, failure, size, sha256, written] {
                const auto entry = self != nullptr ? Transfer::find(self->backups_, id) : nullptr;
                if (entry == nullptr) {
                    QFile::remove(path);
                    return;
                }
                const auto waiter = std::exchange(entry->waiter, nullptr);
                entry->working = false;
                entry->touch();
                if (!failure.isEmpty()) {
                    Transfer::drop(self->backups_, entry);
                    self->fail(waiter, RouterBackupImpl::internal(failure));
                    return;
                }
                entry->size = size;
                entry->sha256 = sha256;
                entry->parts = Engine::BackupArchive::PartNames(written);
                entry->complete = true;
                if (waiter == nullptr) return;
                self->answer(waiter, Response::Ok(QJsonObject{
                    {"id", id},
                    {"size", size},
                    {"sha256", sha256},
                    {"parts", QJsonArray::fromStringList(entry->parts)},
                }));
            });
        });
    }

    void Router::getBackup(const Request &request, const Responder &respond, int id) {
        qint64 offset = 0;
        qint64 length = 0;
        if (!RouterBackupImpl::queryInteger(request, QStringLiteral("offset"), 0, &offset) ||
            !RouterBackupImpl::queryInteger(request, QStringLiteral("length"), RouterBackupImpl::kDefaultSlice, &length)) {
            respond(badRequest(QStringLiteral("\"offset\" and \"length\" must be integers")));
            return;
        }
        if (offset < 0 || length < 1 || length > RouterBackupImpl::kMaxSlice) {
            respond(badRequest(QStringLiteral("\"offset\" must not be negative and \"length\" must be from 1 to %1")
                                   .arg(RouterBackupImpl::kMaxSlice)));
            return;
        }
        const auto transfer = Transfer::find(backups_, id);
        if (transfer == nullptr) {
            respond(notFound(QStringLiteral("Unknown backup %1").arg(id)));
            return;
        }
        transfer->touch();
        if (transfer->working) {
            respond(RouterBackupImpl::busy(QStringLiteral("The backup is still being built")));
            return;
        }
        if (offset > transfer->size) {
            respond(badRequest(QStringLiteral("\"offset\" is past the end of the %1-byte backup").arg(transfer->size)));
            return;
        }

        const qint64 count = std::min(length, transfer->size - offset);
        QFile file(transfer->path);
        QByteArray slice;
        if (file.open(QIODevice::ReadOnly) && file.seek(offset)) slice = file.read(count);
        if (slice.size() != count) {
            respond(RouterBackupImpl::internal(QStringLiteral("Could not read the backup: %1").arg(file.errorString())));
            return;
        }
        respond(Response::Ok(QJsonObject{
            {"id", id},
            {"size", transfer->size},
            {"sha256", transfer->sha256},
            {"parts", QJsonArray::fromStringList(transfer->parts)},
            {"offset", offset},
            {"data", QString::fromLatin1(slice.toBase64())},
            {"eof", offset + count >= transfer->size},
        }));
    }

    void Router::deleteBackup(const Request &, const Responder &respond, int id) {
        const auto transfer = Transfer::find(backups_, id);
        if (transfer == nullptr) {
            respond(notFound(QStringLiteral("Unknown backup %1").arg(id)));
            return;
        }
        if (transfer->waiter != nullptr) {
            answer(transfer->waiter, notFound(QStringLiteral("Backup %1 was deleted before it was built").arg(id)));
        }
        Transfer::drop(backups_, transfer);
        respond(Response::Ok(QJsonObject{{"deleted", id}}));
    }

    void Router::postRestore(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        std::optional<qint64> size;
        std::optional<QString> sha256;
        if (!bodyObject(request, true, &body, &error) || !readInteger(body, QStringLiteral("size"), &size, &error) ||
            !readString(body, QStringLiteral("sha256"), &sha256, &error)) {
            respond(error);
            return;
        }
        if (!size || !sha256) {
            respond(badRequest(QStringLiteral("\"size\" and \"sha256\" are required")));
            return;
        }
        if (*size > RouterBackupImpl::kMaxUpload) {
            respond(Response::Error(413, QStringLiteral("too_large"), QStringLiteral("An archive may be 1 GiB at most")));
            return;
        }
        if (*size < 1) {
            respond(badRequest(QStringLiteral("\"size\" must be at least 1")));
            return;
        }
        if (!RouterBackupImpl::isSha256Hex(*sha256)) {
            respond(badRequest(QStringLiteral("\"sha256\" must be 64 hexadecimal digits")));
            return;
        }
        if (restores_.size() >= RouterBackupImpl::kOpenUploads) {
            respond(RouterBackupImpl::busy(
                QStringLiteral("%1 uploads are already open; finish or delete one first").arg(RouterBackupImpl::kOpenUploads)));
            return;
        }

        const QString path = Engine::BackupArchive::TempPath(QStringLiteral("api-restore"));
        RouterBackupImpl::makeParentDir(path);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            respond(RouterBackupImpl::internal(QStringLiteral("Could not create the upload: %1").arg(file.errorString())));
            return;
        }
        file.close();
        auto transfer = std::make_shared<Transfer>();
        transfer->id = ++lastTransferId_;
        transfer->upload = true;
        transfer->path = path;
        transfer->size = *size;
        transfer->sha256 = sha256->toLower();
        transfer->hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);
        Transfer::keep(this, restores_, transfer);
        respond(Response::Ok(QJsonObject{{"id", transfer->id}, {"chunk_limit", RouterBackupImpl::kChunkLimit}}));
    }

    void Router::putRestore(const Request &request, const Responder &respond, int id) {
        QJsonObject body;
        Response error;
        std::optional<qint64> offset;
        std::optional<QString> data;
        WaitOptions options;
        if (!bodyObject(request, true, &body, &error) || !readInteger(body, QStringLiteral("offset"), &offset, &error) ||
            !readString(body, QStringLiteral("data"), &data, &error) ||
            !readWait(body, true, RouterBackupImpl::kInspectWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        if (!offset || !data) {
            respond(badRequest(QStringLiteral("\"offset\" and \"data\" are required")));
            return;
        }
        const auto transfer = Transfer::find(restores_, id);
        if (transfer == nullptr) {
            respond(notFound(QStringLiteral("Unknown restore %1").arg(id)));
            return;
        }
        transfer->touch();
        if (!transfer->failure.isEmpty()) {
            respond(Response::Error(422, QStringLiteral("invalid_archive"), transfer->failure));
            return;
        }
        if (transfer->working) {
            respond(RouterBackupImpl::busy(QStringLiteral("The archive is being checked")));
            return;
        }
        if (transfer->complete) {
            respond(badRequest(QStringLiteral("The upload is already complete")));
            return;
        }
        if (*offset != transfer->received) {
            respond(Response::Error(409, QStringLiteral("bad_offset"), QStringLiteral("Expected offset %1").arg(transfer->received)));
            return;
        }
        const auto decoded =
            QByteArray::fromBase64Encoding(data->toLatin1(), QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
        if (decoded.decodingStatus != QByteArray::Base64DecodingStatus::Ok) {
            respond(badRequest(QStringLiteral("\"data\" is not valid base64")));
            return;
        }
        const QByteArray &chunk = decoded.decoded;
        if (chunk.isEmpty() || chunk.size() > RouterBackupImpl::kChunkLimit) {
            respond(badRequest(QStringLiteral("\"data\" must decode to 1 to %1 bytes").arg(RouterBackupImpl::kChunkLimit)));
            return;
        }
        if (transfer->received + chunk.size() > transfer->size) {
            respond(badRequest(QStringLiteral("The chunk ends past the announced size of %1 bytes").arg(transfer->size)));
            return;
        }
        QFile file(transfer->path);
        if (!file.open(QIODevice::Append) || file.write(chunk) != chunk.size() || !file.flush()) {
            const auto reason = file.errorString();
            file.close();
            // A short write must not shift the next chunk.
            file.resize(transfer->received);
            respond(RouterBackupImpl::internal(QStringLiteral("Could not write the upload: %1").arg(reason)));
            return;
        }
        file.close();
        transfer->hash->addData(chunk);
        transfer->received += chunk.size();
        if (transfer->received < transfer->size) {
            respond(Response::Ok(QJsonObject{{"id", id}, {"size", transfer->size}, {"received", transfer->received}}));
            return;
        }

        const auto digest = QString::fromLatin1(transfer->hash->result().toHex());
        transfer->hash.reset();
        if (digest != transfer->sha256) {
            Transfer::drop(restores_, transfer);
            respond(Response::Error(422, QStringLiteral("checksum_mismatch"),
                                    QStringLiteral("The upload's SHA-256 is %1, not the announced %2; it was discarded")
                                        .arg(digest, transfer->sha256)));
            return;
        }
        transfer->working = true;
        if (options.wait) {
            transfer->waiter = makeWaiter(respond, options.timeoutMs, [id] {
                return RouterBackupImpl::inProgress(id, QStringLiteral("inspecting"));
            });
        } else {
            respond(RouterBackupImpl::inProgress(id, QStringLiteral("inspecting")));
        }

        const QPointer<Router> self(this);
        runOnNewThread([self, id, path = transfer->path] {
            Engine::BackupArchive::Info info;
            QString failure = Engine::BackupArchive::Inspect(path, &info);
            if (failure.isEmpty() && !info.available.any()) failure = QStringLiteral("The archive holds nothing Throne can restore");
            runOnUiThread([self, id, path, failure, info] {
                const auto entry = self != nullptr ? Transfer::find(self->restores_, id) : nullptr;
                if (entry == nullptr) {
                    QFile::remove(path);
                    return;
                }
                const auto waiter = std::exchange(entry->waiter, nullptr);
                entry->working = false;
                entry->touch();
                if (!failure.isEmpty()) {
                    if (waiter != nullptr && !waiter->done) {
                        Transfer::drop(self->restores_, entry);
                        self->answer(waiter, Response::Error(422, QStringLiteral("invalid_archive"), failure));
                    } else {
                        entry->failure = failure;
                        MW_show_log(QStringLiteral("[API] Restore %1 refused: %2").arg(id).arg(failure));
                    }
                    return;
                }
                entry->complete = true;
                entry->parts = Engine::BackupArchive::PartNames(info.available);
                entry->archive = QJsonObject{
                    {"created_at", info.createdAt},
                    {"platform", info.platform},
                    {"parts", QJsonArray::fromStringList(entry->parts)},
                };
                if (waiter == nullptr) return;
                self->answer(waiter, Response::Ok(QJsonObject{
                    {"id", id},
                    {"size", entry->size},
                    {"received", entry->received},
                    {"archive", entry->archive},
                }));
            });
        });
    }

    void Router::postRestoreApply(const Request &request, const Responder &respond, int id) {
        QJsonObject body;
        Response error;
        WaitOptions options;
        if (!bodyObject(request, false, &body, &error) ||
            !readWait(body, true, RouterBackupImpl::kApplyWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        const auto transfer = Transfer::find(restores_, id);
        if (transfer == nullptr) {
            respond(notFound(QStringLiteral("Unknown restore %1").arg(id)));
            return;
        }
        transfer->touch();
        if (!transfer->failure.isEmpty()) {
            respond(Response::Error(422, QStringLiteral("invalid_archive"), transfer->failure));
            return;
        }
        if (transfer->working) {
            respond(RouterBackupImpl::busy(QStringLiteral("The archive is still being checked")));
            return;
        }
        if (!transfer->complete) {
            respond(Response::Error(409, QStringLiteral("incomplete"), QStringLiteral("The upload is not complete")));
            return;
        }

        std::optional<QStringList> names;
        Configs::BackupParts parts;
        QString problem;
        if (!RouterBackupImpl::readParts(body, &names, &parts, &problem)) {
            respond(badRequest(problem));
            return;
        }
        if (names) {
            for (const auto &name : *names) {
                if (transfer->parts.contains(name)) continue;
                respond(badRequest(QStringLiteral("The archive has no \"%1\"").arg(name)));
                return;
            }
        } else {
            auto chosen = transfer->parts;
            chosen.removeAll(QStringLiteral("settings"));
            chosen.removeAll(QStringLiteral("icons"));
            QString unknown;
            Engine::BackupArchive::PartsFromNames(chosen, &parts, &unknown);
        }
        if (!parts.any()) {
            respond(badRequest(QStringLiteral("The archive holds only settings or icons; name them in \"parts\" to restore them")));
            return;
        }

        // Rechecked once the running profile has stopped: the wait lets updaters, tests and editors start.
        const auto quiet = [this]() -> std::optional<Response> {
            if (engine_->testsRunning()) return RouterBackupImpl::busy(QStringLiteral("A test is running"));
            if (!Subscription::updater()->IsIdle()) return RouterBackupImpl::busy(QStringLiteral("A subscription update is running"));
            if (Engine::AnyEditorOpen()) {
                return Response::Error(409, QStringLiteral("editing"), QStringLiteral("A settings or editor window is open in Throne"));
            }
            return std::nullopt;
        };
        if (connectionBusy()) {
            respond(RouterBackupImpl::busy(QStringLiteral("A profile is starting or stopping")));
            return;
        }
        if (const auto refused = quiet()) {
            respond(*refused);
            return;
        }

        WaiterPtr waiter;
        if (options.wait) {
            waiter = makeWaiter(respond, options.timeoutMs, [id] { return RouterBackupImpl::inProgress(id, QStringLiteral("applying")); });
        } else {
            respond(RouterBackupImpl::inProgress(id, QStringLiteral("applying")));
        }

        const auto apply = [this, id, parts, waiter, quiet] {
            const auto entry = Transfer::find(restores_, id);
            if (entry == nullptr) {
                fail(waiter, notFound(QStringLiteral("Restore %1 was deleted before it was applied").arg(id)));
                return;
            }
            if (Configs::dataManager->settingsRepo->prepare_exit) {
                fail(waiter, Response::Error(503, QStringLiteral("exiting"), QStringLiteral("Throne is shutting down")));
                return;
            }
            if (const auto busyNow = quiet()) {
                fail(waiter, *busyNow);
                return;
            }
            Engine::BackupArchive::ApplyResult result;
            const auto failure =
                Engine::BackupArchive::Apply(entry->path, parts, Engine::SettingsRegistry::HostKeys(), &result, true);
            if (result.refused) {
                fail(waiter, Response::Error(422, QStringLiteral("invalid_archive"), failure));
                return;
            }
            if (!failure.isEmpty()) {
                fail(waiter, RouterBackupImpl::internal(failure));
                return;
            }
            Transfer::drop(restores_, entry);
            restarting_ = true;
            if (waiter != nullptr) {
                answer(waiter, Response::Ok(QJsonObject{
                    {"restored", QJsonArray::fromStringList(Engine::BackupArchive::PartNames(result.restored))},
                    {"skipped_rules", result.skippedRules},
                    {"restarting", true},
                }));
            }
            // Queued, so the answer above leaves first.
            RouterBackupImpl::requestRestart(engine_, this);
        };

        // The content check comes before the stop, so a refused archive leaves the running profile alone.
        transfer->working = true;
        transfer->waiter = waiter;
        const QPointer<Router> self(this);
        runOnNewThread([self, id, parts, waiter, apply, path = transfer->path] {
            bool refused = false;
            const auto problem = Engine::BackupArchive::CheckUntrusted(path, parts, &refused);
            runOnUiThread([self, id, waiter, apply, problem, refused] {
                if (self == nullptr) return;
                const auto entry = Transfer::find(self->restores_, id);
                if (entry == nullptr) return;
                entry->working = false;
                entry->waiter = nullptr;
                entry->touch();
                if (!problem.isEmpty()) {
                    self->fail(waiter, refused ? Response::Error(422, QStringLiteral("invalid_archive"), problem)
                                               : RouterBackupImpl::internal(problem));
                    return;
                }
                if (const auto busyNow = self->afterStop(self->engine_->runningSnapshot() != nullptr, apply)) self->fail(waiter, *busyNow);
            });
        });
    }

    void Router::getRestore(const Request &, const Responder &respond, int id) {
        const auto transfer = Transfer::find(restores_, id);
        if (transfer == nullptr) {
            respond(notFound(QStringLiteral("Unknown restore %1").arg(id)));
            return;
        }
        transfer->touch();
        QString state = QStringLiteral("uploading");
        if (!transfer->failure.isEmpty()) state = QStringLiteral("failed");
        else if (transfer->working) state = QStringLiteral("inspecting");
        else if (transfer->complete) state = QStringLiteral("complete");
        QJsonObject out{{"id", id}, {"size", transfer->size}, {"received", transfer->received}, {"state", state}};
        if (transfer->complete) out.insert(QStringLiteral("archive"), transfer->archive);
        if (!transfer->failure.isEmpty()) out.insert(QStringLiteral("error"), transfer->failure);
        respond(Response::Ok(out));
    }

    void Router::deleteRestore(const Request &, const Responder &respond, int id) {
        const auto transfer = Transfer::find(restores_, id);
        if (transfer == nullptr) {
            respond(notFound(QStringLiteral("Unknown restore %1").arg(id)));
            return;
        }
        if (transfer->waiter != nullptr) {
            answer(transfer->waiter, notFound(QStringLiteral("Restore %1 was deleted before its archive was checked").arg(id)));
        }
        Transfer::drop(restores_, transfer);
        respond(Response::Ok(QJsonObject{{"deleted", id}}));
    }
}
