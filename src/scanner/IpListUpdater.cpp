#include "include/scanner/IpListUpdater.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QThread>
#include <QUrl>

#include <algorithm>

#include "include/configs/generate.h"
#include "include/database/IpListsRepo.h"
#include "include/global/Configs.hpp"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/scanner/IpListParse.h"

namespace Scanner {
    namespace {
        constexpr qint64 kIpListUpdaterMaxBytes = 64LL * 1024 * 1024;
        constexpr int kIpListUpdaterMinMinutes = 30;

        QByteArray ipListUpdaterHead(const QByteArray &bytes) {
            auto head = bytes.left(4096);
            if (head.startsWith("\xEF\xBB\xBF")) head.remove(0, 3);
            return head.trimmed();
        }

        bool ipListUpdaterIsWebPage(const QByteArray &bytes) {
            const auto head = ipListUpdaterHead(bytes).toLower();
            return head.startsWith("<!doctype html") || head.startsWith("<html");
        }

        std::shared_ptr<Configs::IpList> ipListUpdaterHeader(int listId) {
            for (const auto &header : Configs::dataManager->ipListsRepo->GetAllIpLists(true)) {
                if (header->id == listId) return header;
            }
            return nullptr;
        }

        bool ipListUpdaterRefreshable(const Configs::IpList &list) {
            return list.IsRemote() && list.role == Configs::IpList::Role::User;
        }

        QString ipListUpdaterRefreshOne(int listId) {
            const auto header = ipListUpdaterHeader(listId);
            if (header == nullptr) return IpListUpdater::tr("The IP list no longer exists");
            if (!ipListUpdaterRefreshable(*header)) return IpListUpdater::tr("Only lists with a URL source can be updated");

            const auto name = header->name;
            const auto kind = header->source_kind;
            const auto source = header->source;
            MW_show_log(">>>>>>>> " + IpListUpdater::tr("Updating IP list %1").arg(name));
            const auto outcome = FetchEntries(kind, source);

            // Re-read: the list may have been edited or deleted while it downloaded.
            const auto current = ipListUpdaterHeader(listId);
            if (current == nullptr) return IpListUpdater::tr("The IP list no longer exists");
            if (current->source_kind != kind || current->source != source) {
                MW_show_log("<<<<<<<< " + IpListUpdater::tr("Updating IP list %1 failed: %2")
                                              .arg(name, IpListUpdater::tr("its source changed during the update")));
                return IpListUpdater::tr("The source changed during the update");
            }
            auto &lists = *Configs::dataManager->ipListsRepo;
            QString error = outcome.error;
            if (error.isEmpty() && !lists.ReplaceEntries(listId, outcome.entries))
                error = IpListUpdater::tr("The entries could not be saved");
            if (!error.isEmpty()) {
                current->last_error = error;
                lists.SaveHeader(current);
                MW_show_log("<<<<<<<< " + IpListUpdater::tr("Updating IP list %1 failed: %2").arg(current->name, error));
                return error;
            }
            current->last_update = QDateTime::currentSecsSinceEpoch();
            current->last_error.clear();
            lists.SaveHeader(current);
            MW_show_log("<<<<<<<< " + IpListUpdater::tr("IP list %1 updated (%n entries)", nullptr,
                                                        static_cast<int>(outcome.entries.size()))
                                          .arg(current->name));
            return {};
        }
    } // namespace

    ImportOutcome FetchEntries(Configs::IpList::SourceKind kind, const QString &source) {
        ImportOutcome outcome;
        const auto url = source.trimmed();
        if (kind == Configs::IpList::SourceKind::Manual || url.isEmpty()) {
            outcome.error = IpListUpdater::tr("The list has no source to fetch");
            return outcome;
        }
        const QUrl parsed(url);
        const auto scheme = parsed.scheme().toLower();
        if (!parsed.isValid() || parsed.host().isEmpty() || (scheme != QLatin1String("http") && scheme != QLatin1String("https"))) {
            outcome.error = IpListUpdater::tr("The source must be an http(s) URL");
            return outcome;
        }
        HttpGetOptions options;
        options.maxBytes = kIpListUpdaterMaxBytes;
        const auto response = NetworkRequestHelper::HttpGet(Configs::get_jsdelivr_link(url), options);
        if (!response.error.isEmpty()) {
            outcome.error = response.error;
            return outcome;
        }
        return ParseImportBytes(response.data);
    }

    ImportOutcome ParseImportBytes(const QByteArray &bytes) {
        ImportOutcome outcome;
        if (ipListUpdaterIsWebPage(bytes)) {
            outcome.error = IpListUpdater::tr("The address returned a web page instead of an IP list");
            return outcome;
        }
        auto parsed = ParseIpListText(bytes);
        outcome.entries = std::move(parsed.entries);
        outcome.rejected = parsed.rejected;
        outcome.samples = std::move(parsed.samples);
        if (outcome.entries.isEmpty()) outcome.error = IpListUpdater::tr("No addresses found");
        return outcome;
    }

    IpListUpdater::IpListUpdater(QObject *parent) : QObject(parent) {}

    IpListUpdater *IpListUpdater::instance() {
        // Deliberately leaked; owned by the UI thread wherever it is first asked for.
        static IpListUpdater *const self = [] {
            auto *updater = new IpListUpdater(nullptr);
            if (auto *app = QCoreApplication::instance()) updater->moveToThread(app->thread());
            return updater;
        }();
        return self;
    }

    void IpListUpdater::Refresh(int listId, const Finish &finish) {
        QMutexLocker locker(&mutex);
        if (finish != nullptr) finishers[listId] << finish;
        if (active.contains(listId)) return;
        active.insert(listId);
        queue.append(listId);
        if (running) return;
        running = true;
        runOnNewThread([this] { drain(); });
    }

    void IpListUpdater::RefreshAll(bool onlyAuto) {
        for (const auto &list : Configs::dataManager->ipListsRepo->GetAllIpLists(false)) {
            if (!ipListUpdaterRefreshable(*list) || (onlyAuto && !list->auto_update)) continue;
            Refresh(list->id);
        }
    }

    void IpListUpdater::CheckAutoUpdate() {
        if (Configs::dataManager == nullptr || Configs::dataManager->ipListsRepo == nullptr) return;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        for (const auto &list : Configs::dataManager->ipListsRepo->GetAllIpLists(false)) {
            if (!list->auto_update || !ipListUpdaterRefreshable(*list)) continue;
            const qint64 interval = qint64(std::max(kIpListUpdaterMinMinutes, list->update_interval)) * 60;
            // A failed attempt waits the interval too.
            const qint64 last = std::max(list->last_update, autoAttempts.value(list->id, 0));
            if (now - last < interval) continue;
            autoAttempts[list->id] = now;
            Refresh(list->id);
        }
    }

    bool IpListUpdater::IsRefreshing(int listId) const {
        QMutexLocker locker(&mutex);
        return active.contains(listId);
    }

    void IpListUpdater::NotifyListsChanged() {
        if (QThread::currentThread() == thread()) {
            emit listsChanged();
            return;
        }
        QMetaObject::invokeMethod(this, [this] { emit listsChanged(); }, Qt::QueuedConnection);
    }

    void IpListUpdater::drain() {
        for (;;) {
            int listId = -1;
            {
                QMutexLocker locker(&mutex);
                if (queue.isEmpty()) {
                    running = false;
                    return;
                }
                listId = queue.takeFirst();
            }
            QString error;
            try {
                error = ipListUpdaterRefreshOne(listId);
            } catch (const std::exception &ex) {
                error = QString::fromUtf8(ex.what());
                MW_show_log(tr("IP list update failed: %1").arg(error));
            } catch (...) {
                error = tr("IP list update failed");
                MW_show_log(error);
            }
            QList<Finish> done;
            {
                QMutexLocker locker(&mutex);
                active.remove(listId);
                done = finishers.take(listId);
            }
            emit listUpdated(listId, error);
            for (const auto &finish : done) {
                if (finish != nullptr) finish(error);
            }
        }
    }
} // namespace Scanner
