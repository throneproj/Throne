#include "include/configs/sub/GroupUpdater.hpp"

#include "include/configs/generate.h"
#include "include/configs/sub/ImportReport.hpp"
#include "include/configs/sub/SubscriptionParser.hpp"
#include "include/configs/sub/SubscriptionReconcile.hpp"
#include "include/configs/sub/SubscriptionScan.hpp"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EngineEvents.h"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/global/Notice.hpp"
#include "include/global/Utils.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThreadPool>
#include <QUrl>

#include <algorithm>
#include <atomic>

namespace Subscription {
    namespace {
        constexpr int kInsertChunk = 500;
        constexpr qint64 kMaxSubscriptionBytes = 64LL * 1024 * 1024;
        constexpr qsizetype kMaxHeaderValue = 1000;
        constexpr int kValidityCheckThreads = 10;

        QByteArray digest(const QByteArray &data) {
            return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
        }

        QByteArray contentKeyOf(const Configs::Profile &ent) {
            return digest(QJsonDocument(ent.outbound->ExportToJson()).toJson(QJsonDocument::Compact));
        }

        QByteArray identityKeyOf(const Configs::Profile &ent) {
            return digest(ent.type.toUtf8() + '|' + QJsonDocument(ent.outbound->ExportIdentity()).toJson(QJsonDocument::Compact));
        }

        bool usableHeaderValue(const QString &value) {
            return !value.isEmpty() && value.size() < kMaxHeaderValue && !value.contains('\n') && !value.contains('\r');
        }

        void applyCustomHwidParams(DeviceDetails &device, const QString &params) {
            for (const auto &pair : params.split(',')) {
                const auto trimmed = pair.trimmed();
                const auto eqPos = trimmed.indexOf('=');
                if (eqPos <= 0) continue;
                const auto key = trimmed.left(eqPos).trimmed().toLower();
                const auto value = trimmed.mid(eqPos + 1).trimmed();
                if (!usableHeaderValue(value)) continue;
                if (key == "hwid") device.hwid = value;
                else if (key == "os") device.os = value;
                else if (key == "osversion") device.osVersion = value;
                else if (key == "model") device.model = value;
            }
        }

        QList<QPair<QByteArray, QByteArray>> identityHeaders(const RequestIdentity &identity) {
            QList<QPair<QByteArray, QByteArray>> headers;
            if (!identity.sendHwid) return headers;
            const auto add = [&headers](const char *name, const QString &value) {
                if (usableHeaderValue(value)) headers.append({name, value.toUtf8()});
            };
            add("x-hwid", identity.device.hwid);
            add("x-device-os", identity.device.os);
            add("x-ver-os", identity.device.osVersion);
            add("x-device-model", identity.device.model);
            return headers;
        }

        QSsl::SslProtocol tlsProtocolOf(Configs::subTlsVersion version) {
            switch (version) {
                case Configs::subTlsVersion::tls12: return QSsl::TlsV1_2;
                case Configs::subTlsVersion::tls13: return QSsl::TlsV1_3;
                default: return QSsl::SecureProtocols;
            }
        }

        bool looksLikeWebPage(const QByteArray &body) {
            const auto text = scan::trim(scan::view(body));
            return scan::startsWithNoCase(text, "<!doctype html") || scan::startsWithNoCase(text, "<html");
        }

        template <typename Visit>
        void forEachProfile(const QList<int> &ids, Visit &&visit) {
            for (qsizetype off = 0; off < ids.size(); off += Configs::BATCH_LIMIT_READ) {
                for (const auto &ent : Configs::dataManager->profilesRepo->GetProfileBatch(ids.mid(off, Configs::BATCH_LIMIT_READ))) {
                    if (ent != nullptr) visit(ent);
                }
            }
        }

        // Auto selectors are local state, not servers the remote sent.
        QList<int> withoutSelectors(const QList<int> &ids) {
            const auto selectorIds = Configs::dataManager->profilesRepo->GetProfileIdsByType("autoselector");
            const QSet<int> selectors(selectorIds.begin(), selectorIds.end());
            QList<int> result;
            for (int id : ids) {
                if (!selectors.contains(id)) result << id;
            }
            return result;
        }

        // BatchDeleteProfiles silently drops the running profile from the list it was handed (#1753).
        struct DeleteOutcome {
            bool ok = false;
            QList<int> deleted;
            QList<int> kept;
        };

        DeleteOutcome deleteProfiles(QList<int> ids) {
            DeleteOutcome outcome;
            const QSet<int> requested(ids.begin(), ids.end());
            outcome.ok = Configs::dataManager->profilesRepo->BatchDeleteProfiles(
                ids, Configs::dataManager->settingsRepo->allow_stopping_active_profile, false);
            const QSet<int> deleted(ids.begin(), ids.end());
            outcome.deleted = std::move(ids);
            for (int id : requested) {
                if (!deleted.contains(id)) outcome.kept << id;
            }
            return outcome;
        }

        // Inserts in chunks; only per-profile digests survive a flush.
        class ImportSink {
        public:
            ImportSink(int gid, ContentIndex *index) : gid(gid), index(index) {}

            void add(std::shared_ptr<Configs::Profile> ent) {
                NewEntry entry;
                entry.display = ent->outbound->DisplayTypeAndName();
                if (index != nullptr) {
                    entry.keys.content = contentKeyOf(*ent);
                    index->noteArrival(entry.keys.content);
                    entry.contentKnown = index->knows(entry.keys.content);
                    if (const auto oldId = index->claim(entry.keys.content)) {
                        entry.id = *oldId;
                        entry.reused = true;
                        entries.append(std::move(entry));
                        return;
                    }
                    if (!entry.contentKnown) entry.keys.identity = identityKeyOf(*ent);
                }
                entryIndex.append(entries.size());
                entries.append(std::move(entry));
                chunk.append(std::move(ent));
                if (chunk.size() >= kInsertChunk) flush();
            }

            void flush() {
                if (chunk.isEmpty()) return;
                if (Configs::dataManager->profilesRepo->AddProfileBatch(chunk, gid)) {
                    for (qsizetype i = 0; i < chunk.size(); ++i) {
                        entries[entryIndex[i]].id = chunk[i]->id;
                        added << chunk[i]->id;
                    }
                    destination = chunk.first()->gid;
                }
                chunk.clear();
                entryIndex.clear();
            }

            // Sorted here rather than queued, so the SubscriptionFinished redraw already shows the order.
            void finish() {
                flush();
                const auto group = Configs::dataManager->groupsRepo->GetGroup(destination);
                if (group != nullptr && group->Reposition(added)) Configs::dataManager->groupsRepo->Save(group);
            }

            // Rows that really reached the database: added, or reused from the group.
            [[nodiscard]] int stored() const {
                return static_cast<int>(std::count_if(entries.cbegin(), entries.cend(), [](const NewEntry &e) { return e.id >= 0; }));
            }

            [[nodiscard]] QList<int> storedIds() const {
                QList<int> ids;
                for (const auto &entry : entries) {
                    if (entry.id >= 0) ids << entry.id;
                }
                return ids;
            }

            QList<NewEntry> entries;

        private:
            int gid;
            int destination = -1;
            QList<int> added;
            ContentIndex *index;
            QList<std::shared_ptr<Configs::Profile>> chunk;
            QList<qsizetype> entryIndex;
        };

        ParseSink sinkFor(ImportSink &sink, ImportReport &report) {
            ParseSink parseSink;
            parseSink.profile = [&sink](std::shared_ptr<Configs::Profile> ent) { sink.add(std::move(ent)); };
            parseSink.report = [&report](const ParseReport &item) { report.add(item); };
            return parseSink;
        }

        // A dry run, as the real parse inserts as it goes. Its diagnostics only matter when it finds nothing: otherwise the real parse repeats them.
        bool yieldsProfile(const QByteArray &body, ImportReport &diagnostics) {
            bool found = false;
            ParseSink probe;
            probe.profile = [&found](const std::shared_ptr<Configs::Profile> &) { found = true; };
            probe.report = [&diagnostics](const ParseReport &item) { diagnostics.add(item); };
            ParseDocument(body, probe);
            return found;
        }

        void logImportReport(const ImportReport &report) {
            QString text = report.summary();
            if (const auto details = report.details(); !details.isEmpty()) text += '\n' + details;
            MW_show_log(text);
        }

        // Qt's HTTP error text embeds the request (or redirect) URL, and with it the subscription token.
        QString withoutUrls(const QString &text, const QString &url) {
            const auto hostOf = [](const QUrl &u) { return u.host().isEmpty() ? QString(QChar(0x2026)) : u.host(); };
            const QUrl parsed(url);
            QString redacted = text;
            for (const auto &form : {url, parsed.toString(), parsed.toString(QUrl::FullyEncoded), parsed.toDisplayString()}) {
                if (!form.isEmpty()) redacted.replace(form, hostOf(parsed));
            }
            static const QRegularExpression urlRe(R"([A-Za-z][A-Za-z0-9+.\-]*://[^\s"'<>]+)");
            QString result;
            qsizetype last = 0;
            for (auto it = urlRe.globalMatch(redacted); it.hasNext();) {
                const auto match = it.next();
                result += QStringView(redacted).mid(last, match.capturedStart() - last);
                result += hostOf(QUrl(match.captured()));
                last = match.capturedEnd();
            }
            result += QStringView(redacted).mid(last);
            return result;
        }

        QString fetchFailure(const QString &error) {
            return QCoreApplication::translate("Validation", "Could not download the subscription: %1").arg(error);
        }

        QString groupUpdaterWebPageReason(const QByteArray &body) {
            return looksLikeWebPage(body)
                ? QCoreApplication::translate("Validation", "The server returned a web page instead of a subscription.")
                : QString();
        }

        // Outcome errors reach the API, so they stay English.
        UpdateOutcome groupUpdaterFailure(int gid, UpdateOutcome::Failure failure, const QString &error) {
            UpdateOutcome outcome;
            outcome.gid = gid;
            outcome.failure = failure;
            outcome.error = error;
            return outcome;
        }

        UpdateOutcome groupUpdaterMissingGroup(int gid) {
            return groupUpdaterFailure(gid, UpdateOutcome::Failure::NoGroup, QStringLiteral("The group no longer exists."));
        }

        QString groupUpdaterFetchError(const QString &reason) {
            return QStringLiteral("Could not download the subscription: %1").arg(reason);
        }

        // Stays Internal when the job throws before replacing it.
        std::shared_ptr<UpdateOutcome> groupUpdaterUnfinished(int gid, const QString &error) {
            return std::make_shared<UpdateOutcome>(groupUpdaterFailure(gid, UpdateOutcome::Failure::Internal, error));
        }

        GroupUpdater::Finish groupUpdaterDeliver(const std::shared_ptr<UpdateOutcome> &outcome, const GroupUpdater::OutcomeFinish &done) {
            if (done == nullptr) return nullptr;
            return [outcome, done] { done(*outcome); };
        }

        QString reportTitle(GroupUpdater::Surface surface, const QString &groupName) {
            return surface == GroupUpdater::Surface::Report
                ? QCoreApplication::translate("Validation", "New subscription: %1").arg(groupName)
                : QCoreApplication::translate("Validation", "Update of %1").arg(groupName);
        }

        void reportDeleteFailure() {
            PostNotice({NoticeLevel::Error, "Internal error", "DB Error when deleting profiles, data may be corrupted"});
        }

        QString notice(const QStringList &names, const QString &prefix, const QString &action) {
            if (names.size() >= 1000) return QStringLiteral("%1 %2 %3\n").arg(prefix, action).arg(names.size());
            QString result;
            for (const auto &name : names) {
                result += prefix;
                result += ' ';
                result += name;
                result += '\n';
            }
            return result;
        }

        QString groupLabel(int gid) {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
            return group == nullptr ? Int2String(gid) : group->name;
        }

        QString decodeHeaderValue(const QString &raw) {
            QString str = raw.trimmed();
            if (str.startsWith("base64:", Qt::CaseInsensitive)) {
                QString payload = str.mid(7).trimmed();
                if (payload.isEmpty()) return QString();
                QByteArray decoded = QByteArray::fromBase64(payload.toUtf8(), 
                    QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
                if (decoded.isEmpty()) {
                    decoded = QByteArray::fromBase64(payload.toUtf8());
                }
                if (!decoded.isEmpty()) return QString::fromUtf8(decoded).trimmed();
                return QString();
            }
            return str;
        }

        // profile-update-interval is in hours; h/d/s suffixes are tolerated.
        int parseUpdateInterval(const QString &raw) {
            QString s = raw.trimmed().remove('"').remove('\'').toLower();
            qint64 unit = 3600;
            if (s.endsWith('h')) {
                s.chop(1);
            } else if (s.endsWith('d')) {
                s.chop(1);
                unit = 86400;
            } else if (s.endsWith('s')) {
                s.chop(1);
                unit = 1;
            }
            bool ok = false;
            const qint64 value = s.trimmed().toLongLong(&ok);
            if (!ok || value <= 0) return 0;
            constexpr qint64 kMaxHours = 24 * 366;
            return static_cast<int>(std::clamp<qint64>(std::min(value, kMaxHours * 3600) * unit / 3600, 1, kMaxHours));
        }

        // The body's head for the leading `#key: value` scan, decoded first when the body is one base64 blob.
        QByteArray metadataHead(const QByteArray &body) {
            constexpr std::size_t kHeadChars = 16384;
            const auto head = scan::trim(scan::view(body)).substr(0, kHeadChars);
            QByteArray compact;
            compact.reserve(static_cast<qsizetype>(head.size()));
            for (const char c : head) {
                if (!scan::isSpace(c)) compact.append(c);
            }
            if (compact.isEmpty() || !scan::looksLikeBase64(scan::view(compact))) {
                return QByteArray(head.data(), static_cast<qsizetype>(head.size()));
            }
            compact.truncate(compact.size() & ~qsizetype(3));
            return QByteArray::fromBase64(compact);
        }

        QSet<int> invalidProfiles(const QList<std::shared_ptr<Configs::Profile>> &profiles, bool &coreUnreachable) {
            QSet<int> invalid;
            QMutex invalidMutex;
            std::atomic<bool> unreachable = false;
            QThreadPool pool;
            pool.setMaxThreadCount(kValidityCheckThreads);
            for (const auto &ent : profiles) {
                pool.start([&, ent] {
                    if (unreachable.load()) return;
                    bool failed = false;
                    const bool valid = Configs::IsValid(ent, &failed);
                    if (failed) {
                        unreachable.store(true);
                        return;
                    }
                    if (valid) return;
                    QMutexLocker locker(&invalidMutex);
                    invalid.insert(ent->id);
                });
            }
            pool.waitForDone();
            coreUnreachable = unreachable.load();
            return invalid;
        }

        // Each profile is listed under the first enabled action that claims it.
        DeleteOutcome removeFlagged(const QList<int> &ids, const Configs::SubscriptionOptions &options, QString &report) {
            if (!options.remove_duplicates && !options.remove_insecure && !options.remove_invalid) return {true, {}, {}};
            QList<std::shared_ptr<Configs::Profile>> profiles;
            forEachProfile(ids, [&profiles](const std::shared_ptr<Configs::Profile> &ent) { profiles << ent; });

            QSet<int> flagged;
            QList<int> doomed;
            QString sections;
            const auto collect = [&](const QString &heading, const auto &matches) {
                QStringList names;
                for (const auto &ent : profiles) {
                    if (flagged.contains(ent->id) || !matches(ent)) continue;
                    flagged.insert(ent->id);
                    doomed << ent->id;
                    names << ent->outbound->DisplayTypeAndName();
                }
                if (!names.isEmpty()) sections += "\n" + heading.arg(names.size()) + "\n" + notice(names, "[-]", "removed");
            };

            if (options.remove_duplicates) {
                QList<std::shared_ptr<Configs::Profile>> uniq;
                Configs::ProfileFilter::Uniq(profiles, uniq, false);
                QSet<int> keep;
                for (const auto &ent : uniq) keep.insert(ent->id);
                collect(QObject::tr("Removed %1 duplicate profiles:"),
                        [&keep](const std::shared_ptr<Configs::Profile> &ent) { return !keep.contains(ent->id); });
            }
            if (options.remove_insecure) {
                collect(QObject::tr("Removed %1 insecure profiles:"),
                        [](const std::shared_ptr<Configs::Profile> &ent) { return ent->outbound->EffectiveSecurity().isInsecure(); });
            }
            if (options.remove_invalid) {
                QList<std::shared_ptr<Configs::Profile>> unchecked;
                for (const auto &ent : profiles) {
                    if (!flagged.contains(ent->id)) unchecked << ent;
                }
                bool coreUnreachable = false;
                const auto invalid = invalidProfiles(unchecked, coreUnreachable);
                // Every check fails while the core is down: deleting on that verdict would empty the group.
                if (coreUnreachable) {
                    MW_show_log(QObject::tr("Skipped removing invalid profiles: the core is unreachable."));
                } else {
                    collect(QObject::tr("Removed %1 invalid profiles:"),
                            [&invalid](const std::shared_ptr<Configs::Profile> &ent) { return invalid.contains(ent->id); });
                }
            }

            if (doomed.isEmpty()) return {true, {}, {}};
            const auto outcome = deleteProfiles(doomed);
            if (!outcome.ok) {
                reportDeleteFailure();
            }
            report += sections;
            if (!outcome.kept.isEmpty()) report += "\n" + QObject::tr("The running profile was kept.");
            return outcome;
        }

        QList<int> removeUnavailableAndSort(const std::shared_ptr<Configs::Group> &group, const Configs::SubscriptionOptions &options, QString &report) {
            QList<int> deleted;
            if (options.remove_unavailable) {
                QList<int> doomed;
                QStringList names;
                forEachProfile(withoutSelectors(group->Profiles()), [&](const std::shared_ptr<Configs::Profile> &ent) {
                    if (!ent->IsUnavailable()) return;
                    doomed << ent->id;
                    names << ent->outbound->DisplayTypeAndName();
                });
                if (!doomed.isEmpty()) {
                    const auto outcome = deleteProfiles(doomed);
                    if (!outcome.ok) {
                        reportDeleteFailure();
                    }
                    deleted = outcome.deleted;
                    report += "\n" + QObject::tr("Removed %1 unavailable profiles:").arg(names.size()) + "\n" + notice(names, "[-]", "removed");
                    if (!outcome.kept.isEmpty()) report += "\n" + QObject::tr("The running profile was kept.");
                }
            }
            if (options.sort_by_latency) {
                group->SortProfiles({GroupSortMethod::ByLatency, false});
                Configs::dataManager->groupsRepo->Save(group);
            }
            return deleted;
        }
    } // namespace

    AutoUpdatePlan ResolveAutoUpdate(const Configs::Group &group) {
        const auto &settings = Configs::dataManager->settingsRepo;
        if (settings->sub_auto_update < 30 || group.url.isEmpty() || group.archive || group.skip_auto_update) return {};
        const auto &options = group.sub_options;
        if (options.respect_server_interval.value_or(settings->sub_respect_server_interval) && group.sub_info.server_interval > 0) {
            return {static_cast<qint64>(group.sub_info.server_interval) * 3600, AutoUpdatePlan::Source::server};
        }
        if (options.update_interval > 0) {
            return {static_cast<qint64>(std::max(options.update_interval, 30)) * 60, AutoUpdatePlan::Source::group};
        }
        return {static_cast<qint64>(settings->sub_auto_update) * 60, AutoUpdatePlan::Source::global};
    }

    RequestIdentity ResolveIdentity(const Configs::Group *group) {
        const auto &settings = Configs::dataManager->settingsRepo;
        RequestIdentity identity;
        identity.userAgent = settings->GetUserAgent();
        identity.sendHwid = settings->sub_send_hwid;
        identity.device = GetDeviceDetails();
        applyCustomHwidParams(identity.device, settings->sub_custom_hwid_params);
        identity.tlsVersion = static_cast<Configs::subTlsVersion>(
            std::clamp(settings->sub_tls_version, 0, static_cast<int>(Configs::subTlsVersion::tls13)));
        identity.httpVersion = static_cast<Configs::subHttpVersion>(
            std::clamp(settings->sub_http_version, 0, static_cast<int>(Configs::subHttpVersion::http11)));
        if (group == nullptr) return identity;

        const auto &options = group->sub_options;
        if (usableHeaderValue(options.user_agent)) identity.userAgent = options.user_agent;
        identity.tlsVersion = options.tls_version.value_or(identity.tlsVersion);
        identity.httpVersion = options.http_version.value_or(identity.httpVersion);
        if (options.send_hwid != Configs::sendHwid::keepDefault) identity.sendHwid = options.send_hwid == Configs::sendHwid::on;
        const auto take = [](QString &field, const QString &value) {
            if (usableHeaderValue(value)) field = value;
        };
        take(identity.device.hwid, options.hwid);
        take(identity.device.os, options.hwid_os);
        take(identity.device.osVersion, options.hwid_os_version);
        take(identity.device.model, options.hwid_model);
        return identity;
    }

    GroupUpdater *updater() {
        static auto *instance = new GroupUpdater;
        return instance;
    }

    void GroupUpdater::SetUrlTester(UrlTester tester) {
        QMutexLocker locker(&mutex);
        urlTester = std::move(tester);
    }

    void GroupUpdater::RefreshGroup(int gid, const Finish &finish, bool showDiff) {
        RefreshGroup(gid, finish, showDiff ? Surface::Diff : Surface::Log);
    }

    void GroupUpdater::RefreshGroup(int gid, const Finish &finish, Surface surface) {
        QMutexLocker locker(&mutex);
        if (pending.contains(gid)) {
            locker.unlock();
            MW_show_log(QObject::tr("Subscription update already queued: %1").arg(groupLabel(gid)));
            if (finish != nullptr) finish();
            return;
        }
        pending.insert(gid);
        enqueueLocked(refreshJob(gid, false, surface, finish));
    }

    void GroupUpdater::RefreshWithOutcome(int gid, Surface surface, const OutcomeFinish &done) {
        // Refused here: drain only clears pending for a gid >= 0, so such a job would never end.
        if (gid < 0) {
            if (done != nullptr) done(groupUpdaterMissingGroup(gid));
            return;
        }
        QMutexLocker locker(&mutex);
        if (done != nullptr) waiters[gid].append(done);
        if (pending.contains(gid)) return;
        pending.insert(gid);
        enqueueLocked(refreshJob(gid, false, surface, nullptr));
    }

    bool GroupUpdater::IsBusy(int gid) {
        QMutexLocker locker(&mutex);
        return pending.contains(gid);
    }

    bool GroupUpdater::IsIdle() {
        QMutexLocker locker(&mutex);
        return !running && queue.isEmpty();
    }

    void GroupUpdater::RefreshAll(bool onlyAllowed) {
        QMutexLocker locker(&mutex);
        if (pendingBatch > 0) {
            locker.unlock();
            MW_show_log("The last subscription update has not exited.");
            return;
        }
        for (const int gid : Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
            if (group == nullptr || group->url.isEmpty() || group->archive || (onlyAllowed && group->skip_auto_update)) continue;
            if (pending.contains(gid)) continue;
            pending.insert(gid);
            ++pendingBatch;
            enqueueLocked(refreshJob(gid, true, Surface::Log, nullptr));
        }
    }

    qint64 GroupUpdater::autoUpdateDue(const Configs::Group &group, qint64 interval) const {
        // A failed attempt waits the interval too, so a dead URL is not re-fetched on every poll.
        const qint64 last = std::max(group.sub_last_update, autoAttempts.value(group.id, 0));
        return last <= 0 ? 0 : last + interval;
    }

    void GroupUpdater::CheckAutoUpdate() {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        for (const int gid : Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
            if (group == nullptr) continue;
            const auto plan = ResolveAutoUpdate(*group);
            if (plan.interval <= 0 || autoUpdateDue(*group, plan.interval) > now) continue;
            autoAttempts[gid] = now;
            RefreshGroup(gid, nullptr, false);
        }
    }

    qint64 GroupUpdater::NextAutoUpdate() const {
        qint64 next = -1;
        for (const int gid : Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
            if (group == nullptr) continue;
            const auto plan = ResolveAutoUpdate(*group);
            if (plan.interval <= 0) continue;
            const qint64 due = autoUpdateDue(*group, plan.interval);
            if (next < 0 || due < next) next = due;
        }
        return next;
    }

    void GroupUpdater::SubscribeUrl(const QString &url, const Finish &finish) {
        const auto content = url.trimmed();
        enqueue({-1, false, [=, this] {
            auto group = Configs::GroupsRepo::NewGroup();
            group->name = QUrl(content).host();
            group->url = content;
            // Stored before the download: a failed one is fixed through the group's own TLS, HTTP and User Agent settings.
            Configs::dataManager->groupsRepo->AddGroup(group);
            Engine::EngineEvents::SubscriptionNewGroup(group->id);
            refresh(group->id, Surface::Report);
            emit asyncUpdateCallback(group->id);
        }, finish});
    }

    void GroupUpdater::ImportUrl(const QString &url, const Finish &finish) {
        const auto content = url.trimmed();
        enqueue({-1, false, [=, this] {
            QByteArray body;
            Configs::SubUserInfo subInfo;
            QString error;
            if (fetch(content, QUrl(content).host(), ResolveIdentity(nullptr), body, subInfo, &error)) {
                const auto emptyReason = groupUpdaterWebPageReason(body);
                importDocuments(-1, {std::move(body)}, emptyReason);
            } else {
                ImportReport report;
                report.addFatal(fetchFailure(error));
                ShowImportReport(QCoreApplication::translate("Validation", "Import"), report);
            }
            emit asyncUpdateCallback(-1);
        }, finish});
    }

    void GroupUpdater::ImportText(const QString &text, int gid, const Finish &finish) {
        QByteArray body = text.toUtf8();
        enqueue({-1, false, [=, this]() mutable {
            importDocuments(gid, {std::move(body)});
            emit asyncUpdateCallback(gid);
        }, finish});
    }

    void GroupUpdater::ImportBatch(const QStringList &payloads, const Finish &finish) {
        if (payloads.isEmpty()) return;
        QList<QByteArray> documents;
        for (const auto &payload : payloads) documents << payload.trimmed().toUtf8();
        enqueue({-1, false, [=, this]() mutable {
            importDocuments(-1, std::move(documents));
            emit asyncUpdateCallback(-1);
        }, finish});
    }

    void GroupUpdater::ImportTextTo(int gid, const QString &text, const OutcomeFinish &done) {
        QByteArray body = text.toUtf8();
        const auto outcome = groupUpdaterUnfinished(gid, QStringLiteral("The import failed unexpectedly."));
        enqueue({-1, false, [=, this]() mutable {
            *outcome = Configs::dataManager->groupsRepo->GetGroup(gid) == nullptr
                ? groupUpdaterMissingGroup(gid)
                : importDocuments(gid, {std::move(body)}, {}, true);
            emit asyncUpdateCallback(gid);
        }, groupUpdaterDeliver(outcome, done)});
    }

    void GroupUpdater::ImportUrlTo(int gid, const QString &url, const OutcomeFinish &done) {
        const auto content = url.trimmed();
        const auto outcome = groupUpdaterUnfinished(gid, QStringLiteral("The import failed unexpectedly."));
        enqueue({-1, false, [=, this] {
            if (Configs::dataManager->groupsRepo->GetGroup(gid) == nullptr) {
                *outcome = groupUpdaterMissingGroup(gid);
                return;
            }
            QByteArray body;
            Configs::SubUserInfo subInfo;
            QString error;
            if (fetch(content, QUrl(content).host(), ResolveIdentity(nullptr), body, subInfo, &error)) {
                const auto emptyReason = groupUpdaterWebPageReason(body);
                *outcome = importDocuments(gid, {std::move(body)}, emptyReason, true);
            } else {
                *outcome = groupUpdaterFailure(gid, UpdateOutcome::Failure::Fetch, groupUpdaterFetchError(error));
                outcome->report.addFatal(fetchFailure(error));
            }
            emit asyncUpdateCallback(gid);
        }, groupUpdaterDeliver(outcome, done)});
    }

    void GroupUpdater::CloneProfiles(const QList<int> &ids, const Finish &finish) {
        // A direct copy, not a link round trip: links refuse the types whose state only means something here.
        struct CloneSource {
            QString type;
            QString name;
            QJsonObject outbound;
            Configs::EndpointSource endpoint;
            QString display;
        };
        QList<CloneSource> sources;
        for (const auto &ent : Configs::dataManager->profilesRepo->GetProfileBatch(ids)) {
            if (ent == nullptr || ent->outbound == nullptr) continue;
            sources.append({ent->type, ent->name, ent->outbound->ExportToJson(), ent->endpoint, ent->outbound->DisplayTypeAndName()});
        }
        enqueue({-1, false, [=, this] {
            auto &settings = Configs::dataManager->settingsRepo;
            settings->imported_count = 0;
            ImportSink sink(-1, nullptr);
            for (const auto &source : sources) {
                auto clone = Configs::ProfilesRepo::NewProfile(source.type);
                if (clone->outbound == nullptr || clone->outbound->invalid || !clone->outbound->ParseFromJson(source.outbound)) {
                    MW_show_log(QCoreApplication::translate("Validation", "Could not copy %1.").arg(source.display));
                    continue;
                }
                clone->name = source.name;
                clone->endpoint = source.endpoint;
                sink.add(std::move(clone));
            }
            sink.finish();
            const int count = sink.stored();
            settings->imported_count = count;
            Engine::EngineEvents::SubscriptionFinished(count, false);
            emit asyncUpdateCallback(-1);
        }, finish});
    }

    GroupUpdater::Job GroupUpdater::refreshJob(int gid, bool batch, Surface surface, const Finish &finish) {
        const auto outcome = groupUpdaterUnfinished(gid, QStringLiteral("The update failed unexpectedly."));
        return {gid, batch, [=, this] {
            *outcome = refresh(gid, surface);
            emit asyncUpdateCallback(gid);
        }, finish, outcome};
    }

    void GroupUpdater::enqueue(Job job) {
        QMutexLocker locker(&mutex);
        enqueueLocked(std::move(job));
    }

    void GroupUpdater::enqueueLocked(Job job) {
        queue.append(std::move(job));
        if (running) return;
        running = true;
        runOnNewThread([this] { drain(); });
    }

    void GroupUpdater::drain() {
        for (;;) {
            Job job;
            {
                QMutexLocker locker(&mutex);
                if (queue.isEmpty()) {
                    running = false;
                    return;
                }
                job = queue.takeFirst();
            }
            const auto guarded = [](const std::function<void()> &step) {
                try {
                    step();
                } catch (const std::exception &ex) {
                    MW_show_log(QString("Subscription task failed: %1").arg(ex.what()));
                } catch (...) {
                    MW_show_log("Subscription task failed.");
                }
            };
            guarded(job.run);
            // The UI clears its "updating" state in finish, so it must run even after a throw.
            if (job.finish != nullptr) guarded(job.finish);
            QList<OutcomeFinish> waiting;
            {
                QMutexLocker locker(&mutex);
                if (job.gid >= 0) {
                    pending.remove(job.gid);
                    // Taken with the pending mark: a later RefreshWithOutcome queues a new job instead of joining this one.
                    waiting = waiters.take(job.gid);
                }
                if (job.batch) --pendingBatch;
            }
            for (const auto &done : waiting) guarded([&] { done(*job.outcome); });
        }
    }

    bool GroupUpdater::fetch(const QString &url, const QString &name, const RequestIdentity &identity, QByteArray &body,
                             Configs::SubUserInfo &subInfo, QString *error) {
        MW_show_log(">>>>>>>> " + QObject::tr("Requesting subscription: %1").arg(name));
        HttpGetOptions options;
        options.maxBytes = kMaxSubscriptionBytes;
        options.userAgent = identity.userAgent;
        options.headers = identityHeaders(identity);
        options.tlsProtocol = tlsProtocolOf(identity.tlsVersion);
        options.http2 = identity.httpVersion != Configs::subHttpVersion::http11;
        auto resp = NetworkRequestHelper::HttpGet(url, options);
        if (!resp.error.isEmpty()) {
            const auto reason = withoutUrls(resp.error, url);
            MW_show_log("<<<<<<<< " + QObject::tr("Requesting subscription %1 error: %2").arg(name, reason + "\n" + withoutUrls(QString::fromUtf8(resp.data), url)));
            if (error != nullptr) *error = reason;
            return false;
        }
        body = std::move(resp.data);

        const QString userInfoHeader = NetworkRequestHelper::GetHeader(resp.header, "Subscription-UserInfo");
        if (!userInfoHeader.isEmpty()) subInfo = Configs::ParseSubUserInfo(userInfoHeader);
        bool userInfoSeen = subInfo.valid;

        QString intervalHeader = NetworkRequestHelper::GetHeader(resp.header, "profile-update-interval");
        if (intervalHeader.isEmpty()) intervalHeader = NetworkRequestHelper::GetHeader(resp.header, "x-profile-update-interval");
        if (const int hours = parseUpdateInterval(intervalHeader); hours > 0) {
            subInfo.server_interval = hours;
            subInfo.valid = true;
        }

        QString profileTitle = decodeHeaderValue(NetworkRequestHelper::GetHeader(resp.header, "Profile-Title"));
        if (!profileTitle.isEmpty()) {
            subInfo.title = profileTitle;
            subInfo.valid = true;
        }

        QString webPageUrl = NetworkRequestHelper::GetHeader(resp.header, "Profile-Web-Page-Url");
        if (!webPageUrl.isEmpty()) {
            subInfo.web_url = webPageUrl;
            subInfo.valid = true;
        }

        const QString supportUrl = NetworkRequestHelper::GetHeader(resp.header, "Support-Url");
        if (!supportUrl.isEmpty()) {
            subInfo.support_url = supportUrl;
            subInfo.valid = true;
        }

        QString announceMsg = decodeHeaderValue(NetworkRequestHelper::GetHeader(resp.header, "Announce"));
        if (!announceMsg.isEmpty()) {
            subInfo.announce = announceMsg;
            subInfo.valid = true;
        }

        const QByteArray head = metadataHead(body);
        scan::forEachLine(scan::view(head), [&](std::string_view rawLine) -> bool {
            auto line = scan::trim(rawLine);
            if (line.empty()) return true;
            if (!line.starts_with('#') && !line.starts_with("//")) return false;

            std::string_view comment = line.starts_with("//") ? line.substr(2) : line.substr(1);
            comment = scan::trim(comment);

            const auto colon = comment.find(':');
            if (colon == std::string_view::npos) return true;

            const auto key = scan::trim(comment.substr(0, colon));
            const auto val = scan::trim(comment.substr(colon + 1));
            const QString valStr = QString::fromUtf8(val.data(), static_cast<qsizetype>(val.size()));

            if (!userInfoSeen && scan::startsWithNoCase(key, "subscription-userinfo")) {
                if (const auto parsed = Configs::ParseSubUserInfo(valStr); parsed.valid) {
                    subInfo.upload = parsed.upload;
                    subInfo.download = parsed.download;
                    subInfo.total = parsed.total;
                    subInfo.expire = parsed.expire;
                    subInfo.has_quota = parsed.has_quota;
                    subInfo.valid = true;
                    userInfoSeen = true;
                }
            } else if (subInfo.server_interval == 0 && scan::startsWithNoCase(key, "profile-update-interval")) {
                int hours = parseUpdateInterval(valStr);
                if (hours > 0) {
                    subInfo.server_interval = hours;
                    subInfo.valid = true;
                }
            } else if (subInfo.title.isEmpty() && scan::startsWithNoCase(key, "profile-title")) {
                subInfo.title = decodeHeaderValue(valStr);
                if (!subInfo.title.isEmpty()) subInfo.valid = true;
            } else if (subInfo.web_url.isEmpty() && scan::startsWithNoCase(key, "profile-web-page-url")) {
                subInfo.web_url = valStr;
                if (!subInfo.web_url.isEmpty()) subInfo.valid = true;
            } else if (subInfo.support_url.isEmpty() && scan::startsWithNoCase(key, "support-url")) {
                subInfo.support_url = valStr;
                if (!subInfo.support_url.isEmpty()) subInfo.valid = true;
            } else if (subInfo.announce.isEmpty() && (scan::startsWithNoCase(key, "announce") || scan::startsWithNoCase(key, "notice"))) {
                subInfo.announce = decodeHeaderValue(valStr);
                if (!subInfo.announce.isEmpty()) subInfo.valid = true;
            }

            return true;
        });

        MW_show_log("<<<<<<<< " + QObject::tr("Subscription request fininshed: %1").arg(name));
        if (subInfo.server_interval > 0) {
            MW_show_log(QObject::tr("Subscription server update interval: %1 hour(s)").arg(subInfo.server_interval));
        }
        return true;
    }

    UpdateOutcome GroupUpdater::importDocuments(int gid, QList<QByteArray> documents, const QString &emptyReason, bool quiet) {
        auto &settings = Configs::dataManager->settingsRepo;
        settings->imported_count = 0;
        ImportSink sink(gid, nullptr);
        ImportReport report;

        MW_show_log(">>>>>>>> " + QObject::tr("Processing subscription data..."));
        for (auto &document : documents) ParseDocument(std::move(document), sinkFor(sink, report));
        sink.finish();
        MW_show_log(">>>>>>>> " + QObject::tr("Process complete, applying..."));

        const int count = sink.stored();
        report.setImported(count);
        if (const auto lost = static_cast<int>(sink.entries.size()) - count; lost > 0) {
            report.addFatal(QCoreApplication::translate("Validation", "%n profile(s) could not be saved to the database.", nullptr, lost));
        }
        if (count == 0) {
            if (!emptyReason.isEmpty()) report.addFatal(emptyReason);
            else if (!report.hasProblems()) report.addFatal(QCoreApplication::translate("Validation", "No profiles were found."));
        }
        settings->imported_count = count;
        // The logged summary already carries the count, so the handler's own count line is skipped.
        if (report.hasProblems()) logImportReport(report);
        if (quiet) {
            Engine::EngineEvents::SubscriptionFinished(count, true);
        } else if (report.hasProblems()) {
            ShowImportReport(QCoreApplication::translate("Validation", "Import"), report);
            Engine::EngineEvents::SubscriptionFinished(-1, true);
        } else {
            Engine::EngineEvents::SubscriptionFinished(count, false);
        }

        UpdateOutcome outcome;
        outcome.gid = gid;
        if (gid >= 0 && Configs::dataManager->groupsRepo->GetGroup(gid) == nullptr) {
            outcome = groupUpdaterMissingGroup(gid);
        } else if (count == 0) {
            outcome.failure = UpdateOutcome::Failure::Empty;
            outcome.error = !emptyReason.isEmpty() ? emptyReason : report.fatalReasons().value(0, QStringLiteral("No profiles were found."));
        } else {
            outcome.addedIds = sink.storedIds();
        }
        outcome.report = std::move(report);
        return outcome;
    }

    UpdateOutcome GroupUpdater::refresh(int gid, Surface surface) {
        Configs::dataManager->settingsRepo->imported_count = 0;
        auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (group == nullptr) return groupUpdaterMissingGroup(gid);
        if (group->archive) return groupUpdaterFailure(gid, UpdateOutcome::Failure::NoGroup, QStringLiteral("The group is archived."));

        const QString url = group->url.trimmed();
        QByteArray body;
        Configs::SubUserInfo subInfo;
        QString error;
        if (!fetch(url, group->name, ResolveIdentity(group.get()), body, subInfo, &error)) {
            auto outcome = groupUpdaterFailure(gid, UpdateOutcome::Failure::Fetch, groupUpdaterFetchError(error));
            outcome.report.addFatal(fetchFailure(error));
            if (surface == Surface::Report) {
                outcome.report.addFatal(QCoreApplication::translate("Validation", "The group %1 was created, but its first download failed. Update it to try again; if the download keeps failing, try another TLS version, HTTP version or User Agent in the group's Advanced settings.").arg(group->name));
            }
            if (surface != Surface::Log) ShowImportReport(reportTitle(surface, group->name), outcome.report);
            return outcome;
        }
        UpdateOutcome outcome;
        outcome.gid = gid;
        applyRefresh(group, url, std::move(body), subInfo, surface, outcome);
        return outcome;
    }

    void GroupUpdater::applyRefresh(const std::shared_ptr<Configs::Group> &group, const QString &url, QByteArray body,
                                    const Configs::SubUserInfo &subInfo, Surface surface, UpdateOutcome &result) {
        auto &settings = Configs::dataManager->settingsRepo;
        auto &profilesRepo = Configs::dataManager->profilesRepo;
        auto &groupsRepo = Configs::dataManager->groupsRepo;
        const int gid = group->id;
        const auto options = group->sub_options;

        if (group->url.trimmed() != url) {
            MW_show_log(QObject::tr("The URL of %1 changed during the update, so the result was discarded.").arg(group->name));
            result.failure = UpdateOutcome::Failure::Fetch;
            result.error = QStringLiteral("The subscription URL changed during the update.");
            return;
        }

        // Not a single profile is far likelier a broken or blocked response than an emptied subscription: touch nothing.
        ImportReport probeReport;
        if (!yieldsProfile(body, probeReport)) {
            if (looksLikeWebPage(body)) {
                probeReport.addFatal(QObject::tr("The server returned a web page instead of a subscription. If it is a CAPTCHA or a bot check, try another TLS version, HTTP version or User Agent in the group's Advanced settings."));
            }
            // Panels answer a device or quota limit with an empty list and an announcement saying why.
            if (subInfo.valid) {
                if (!subInfo.announce.isEmpty()) probeReport.addFatal(QObject::tr("Announcement from %1: %2").arg(group->name, subInfo.announce));
                group->sub_info = subInfo;
                group->info.clear();
                groupsRepo->Save(group);
            }
            probeReport.addFatal(QObject::tr("No profiles found in the subscription: %1 was left unchanged.").arg(group->name));
            logImportReport(probeReport);
            if (surface != Surface::Log) ShowImportReport(reportTitle(surface, group->name), probeReport);
            result.failure = UpdateOutcome::Failure::Empty;
            result.error = probeReport.fatalReasons().join('\n');
            result.report = std::move(probeReport);
            return;
        }

        group->sub_info = subInfo;
        group->info.clear();
        groupsRepo->Save(group);

        // Auto selectors are local state, not servers the remote sent: keep them out of the diff.
        const auto selectorIds = profilesRepo->GetProfileIdsByType("autoselector");
        const QSet<int> selectors(selectorIds.begin(), selectorIds.end());
        QList<QPair<int, int>> sticky;
        QSet<int> stickyIDs;
        for (int i = 0; i < group->profiles.size(); i++) {
            if (!selectors.contains(group->profiles[i])) continue;
            sticky << qMakePair(i, group->profiles[i]);
            stickyIDs.insert(group->profiles[i]);
        }
        const auto members = [&] {
            QList<int> ids;
            for (int id : group->profiles) {
                if (!stickyIDs.contains(id)) ids << id;
            }
            return ids;
        };

        // Ids a running auto selector can no longer trust: deleted, or same id with new settings.
        QList<int> disturbed;
        QSet<int> keptIds;
        bool deleteFailed = false;
        bool cleared = false;
        if (settings->sub_clear) {
            MW_show_log(QObject::tr("Clearing servers..."));
            auto doomed = members();
            if (options.keep_working) {
                QSet<int> working;
                forEachProfile(doomed, [&working](const std::shared_ptr<Configs::Profile> &ent) {
                    if (ent->IsWorking()) working.insert(ent->id);
                });
                doomed.removeIf([&working](int id) { return working.contains(id); });
            }
            const auto outcome = deleteProfiles(doomed);
            if (!outcome.ok) {
                PostNotice({NoticeLevel::Error, "Internal Error", "DB Error when deleting profiles, Please try again."});
                result.failure = UpdateOutcome::Failure::Database;
                result.error = QStringLiteral("The old profiles could not be deleted from the database.");
                return;
            }
            disturbed = outcome.deleted;
            result.removed = static_cast<int>(outcome.deleted.size());
            // A survivor still belongs to the subscription: fall through to the diff.
            cleared = members().isEmpty();
        }

        QList<OldEntry> old;
        QSet<int> working;
        if (!cleared) {
            forEachProfile(members(), [&](const std::shared_ptr<Configs::Profile> &ent) {
                old.append({ent->id, {contentKeyOf(*ent), identityKeyOf(*ent)}, ent->outbound->DisplayTypeAndName()});
                if (options.keep_working && ent->IsWorking()) working.insert(ent->id);
            });
        }
        ContentIndex index(old);
        ImportSink sink(gid, cleared ? nullptr : &index);
        ImportReport report;

        MW_show_log(">>>>>>>> " + QObject::tr("Processing subscription data..."));
        ParseDocument(std::move(body), sinkFor(sink, report));
        sink.flush();
        MW_show_log(">>>>>>>> " + QObject::tr("Process complete, applying..."));
        report.setImported(sink.stored());

        QString change_text;
        if (cleared) {
            result.added = sink.stored();
            if (sink.entries.size() >= 1000) {
                change_text += "[+] " + Int2String(sink.entries.size()) + " profiles\n";
            } else {
                for (const auto &entry : sink.entries) change_text += "[+] " + entry.display + "\n";
            }
        } else {
            const auto plan = Reconcile(old, sink.entries, index);
            result.added = static_cast<int>(plan.added.size());
            result.updated = static_cast<int>(plan.updates.size());
            for (const auto &[oldId, newId] : plan.updates) {
                auto oldEnt = profilesRepo->GetProfile(oldId);
                const auto newEnt = profilesRepo->GetProfile(newId);
                if (oldEnt != nullptr && newEnt != nullptr) {
                    oldEnt->outbound = newEnt->outbound;
                    // Still stamped with the temporary row's id; VPN credential overrides are keyed by it.
                    oldEnt->outbound->profile_id = oldId;
                    oldEnt->name = oldEnt->outbound->name;
                    profilesRepo->Save(oldEnt);
                }
                disturbed << oldId;
            }

            QList<int> stale;
            QList<int> keptWorking;
            for (int id : plan.stale) (working.contains(id) ? keptWorking : stale) << id;

            const auto previousOrder = group->profiles;
            group->profiles = plan.order;
            for (const auto &[position, id] : sticky) {
                group->profiles.insert(std::min<qsizetype>(position, group->profiles.size()), id);
            }
            groupsRepo->Save(group);

            const auto outcome = deleteProfiles(stale);
            if (!outcome.ok) {
                reportDeleteFailure();
                deleteFailed = true;
            }
            disturbed << outcome.deleted;
            // stale also lists the fresh rows that only carried an update into an old id.
            const QSet<int> deletedIds(outcome.deleted.begin(), outcome.deleted.end());
            for (const auto &entry : old) {
                if (deletedIds.contains(entry.id)) ++result.removed;
            }
            for (int id : outcome.kept) keptIds << id;
            for (int id : keptWorking) keptIds << id;

            // Nothing rebuilds group->profiles from the rows: a survivor left out here is orphaned.
            const auto restore = [&](int id) {
                if (group->HasProfile(id)) return false;
                const auto position = previousOrder.indexOf(id);
                group->profiles.insert(position < 0 ? group->profiles.size()
                                                    : std::min<qsizetype>(position, group->profiles.size()), id);
                return true;
            };
            QString notice_kept;
            for (int id : outcome.kept) {
                if (!restore(id)) continue;
                if (const auto ent = profilesRepo->GetProfile(id); ent != nullptr) {
                    notice_kept += "[=] " + ent->outbound->DisplayTypeAndName() + "\n";
                }
            }
            QStringList workingNames;
            for (int id : keptWorking) {
                if (!restore(id)) continue;
                if (const auto ent = profilesRepo->GetProfile(id); ent != nullptr) workingNames << ent->outbound->DisplayTypeAndName();
            }
            if (!outcome.kept.isEmpty() || !keptWorking.isEmpty()) groupsRepo->Save(group);

            auto deletedNames = plan.deleted;
            if (!keptWorking.isEmpty()) {
                const QSet<int> doomed(stale.begin(), stale.end());
                deletedNames.clear();
                for (const auto &entry : old) {
                    if (doomed.contains(entry.id)) deletedNames << entry.display;
                }
            }

            change_text = "\n" + QObject::tr("Added %1 profiles:\n%2\nUpdated %3 profiles:\n%4\nDeleted %5 Profiles:\n%6")
                                     .arg(plan.added.size())
                                     .arg(notice(plan.added, "[+]", "added"))
                                     .arg(plan.updates.size())
                                     .arg(notice(plan.updated, "[~]", "updated"))
                                     .arg(deletedNames.size())
                                     .arg(notice(deletedNames, "[-]", "deleted"));
            if (!notice_kept.isEmpty()) {
                change_text += "\n" + QObject::tr("Still in use, so kept instead of deleted:\n%1").arg(notice_kept);
            }
            if (!workingNames.isEmpty()) {
                change_text += "\n" + QObject::tr("Working, so kept instead of deleted:\n%1").arg(notice(workingNames, "[=]", "kept"));
            }
            if (plan.added.isEmpty() && plan.updates.isEmpty() && deletedNames.isEmpty() && keptWorking.isEmpty()) {
                change_text = QObject::tr("Nothing");
            }
        }

        const auto flagged = removeFlagged(members(), options, change_text);
        disturbed << flagged.deleted;
        result.removed += static_cast<int>(flagged.deleted.size());
        for (int id : flagged.kept) keptIds << id;
        result.kept = static_cast<int>(keptIds.size());
        if (deleteFailed || !flagged.ok) {
            result.failure = UpdateOutcome::Failure::Database;
            result.error = QStringLiteral("Some profiles could not be deleted from the database.");
        }

        // The subscription's own order replaced the group's above.
        group->Resort();
        // Stamped only once the update went through, so an interrupted one is retried instead of counting as fresh.
        group->sub_last_update = QDateTime::currentSecsSinceEpoch();
        groupsRepo->Save(group);

        MW_show_log("<<<<<<<< " + QObject::tr("Change of %1:").arg(group->name) + "\n" + change_text);
        if (report.hasProblems()) logImportReport(report);
        if (surface == Surface::Diff && settings->sub_show_change_popup) {
            const auto diffTitle = QObject::tr("Change of %1").arg(group->name);
            auto diffBody = change_text.trimmed();
            if (diffBody.isEmpty()) diffBody = QObject::tr("Nothing");
            if (report.hasProblems()) {
                diffBody += "\n\n" + QCoreApplication::translate("Validation", "Problems:") + "\n" + report.summary();
                if (const auto details = report.details(); !details.isEmpty()) diffBody += "\n" + details;
            }
            Engine::EngineEvents::SubscriptionDiff(diffTitle, diffBody);
        } else if (surface != Surface::Log && report.hasProblems()) {
            ShowImportReport(reportTitle(surface, group->name), report);
        }
        // Auto selectors resolve members from the group at build time, so a refresh can invalidate an untouched one.
        Engine::EngineEvents::SubscriptionGroupChanged(group->id, disturbed);
        Engine::EngineEvents::SubscriptionFinished(-1, true);
        result.report = std::move(report);

        if (!options.url_test) return;
        UrlTester tester;
        {
            QMutexLocker locker(&mutex);
            tester = urlTester;
        }
        if (tester == nullptr) return;
        // Back through the queue, so the follow-up never races another job over the group.
        tester(members(), surface != Surface::Log, [=, this] { enqueue({-1, false, [=, this] { afterUrlTest(gid); }, nullptr}); });
    }

    void GroupUpdater::afterUrlTest(int gid) {
        const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (group == nullptr || group->archive) return;
        const auto options = group->sub_options;
        if (!options.url_test || (!options.remove_unavailable && !options.sort_by_latency)) return;

        QString report;
        const auto deleted = removeUnavailableAndSort(group, options, report);
        if (!report.isEmpty()) MW_show_log("<<<<<<<< " + QObject::tr("After the URL test of %1:").arg(group->name) + report);

        Engine::EngineEvents::SubscriptionGroupChanged(gid, deleted);
        Engine::EngineEvents::SubscriptionFinished(-1, true);
    }
} // namespace Subscription
