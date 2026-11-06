#include <include/database/entities/Group.h>

#include "include/configs/generate.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"
#include <QRegularExpression>
#include <QSet>

#include <bit>

namespace Configs
{
    QJsonObject SubUserInfo::toJson() const {
        QJsonObject json;
        if (!valid) return json;
        json["valid"] = valid;
        json["has_quota"] = has_quota;
        if (upload > 0) json["upload"] = upload;
        if (download > 0) json["download"] = download;
        if (total > 0) json["total"] = total;
        if (expire > 0) json["expire"] = expire;
        if (!title.isEmpty()) json["title"] = title;
        if (!web_url.isEmpty()) json["web_url"] = web_url;
        if (!support_url.isEmpty()) json["support_url"] = support_url;
        if (!announce.isEmpty()) json["announce"] = announce;
        if (server_interval > 0) json["server_interval"] = server_interval;
        return json;
    }

    SubUserInfo SubUserInfo::fromJson(const QJsonObject &json) {
        SubUserInfo res;
        if (json.isEmpty()) return res;
        res.valid = json["valid"].toBool(false);
        res.has_quota = json["has_quota"].toBool(false);
        res.upload = json["upload"].toVariant().toLongLong();
        res.download = json["download"].toVariant().toLongLong();
        res.total = json["total"].toVariant().toLongLong();
        res.expire = json["expire"].toVariant().toLongLong();
        res.title = json["title"].toString();
        res.web_url = json["web_url"].toString();
        res.support_url = json["support_url"].toString();
        res.announce = json["announce"].toString();
        res.server_interval = json["server_interval"].toInt(0);
        return res;
    }

    SubUserInfo ParseSubUserInfo(const QString &info) {
        SubUserInfo result;
        static const QRegularExpression re(R"(\b(upload|download|total|expire)\s*=\s*(\d+))", QRegularExpression::CaseInsensitiveOption);
        for (auto it = re.globalMatch(info); it.hasNext();) {
            const auto match = it.next();
            const QStringView key = match.capturedView(1);
            const qint64 value = match.capturedView(2).toLongLong();
            const auto is = [key](QStringView name) { return key.compare(name, Qt::CaseInsensitive) == 0; };
            if (is(u"upload")) {
                result.upload = value;
            } else if (is(u"download")) {
                result.download = value;
            } else if (is(u"total")) {
                result.total = value;
                result.has_quota = true;
            } else {
                result.expire = value > 1000000000000LL ? value / 1000 : value;
            }
            result.valid = true;
        }
        return result;
    }

    QJsonObject SubscriptionOptions::ToJson() const {
        QJsonObject json;
        if (!user_agent.isEmpty()) json["user_agent"] = user_agent;
        if (tls_version) json["tls_version"] = static_cast<int>(*tls_version);
        if (http_version) json["http_version"] = static_cast<int>(*http_version);
        if (send_hwid != sendHwid::keepDefault) json["send_hwid"] = static_cast<int>(send_hwid);
        if (!hwid.isEmpty()) json["hwid"] = hwid;
        if (!hwid_os.isEmpty()) json["hwid_os"] = hwid_os;
        if (!hwid_os_version.isEmpty()) json["hwid_os_version"] = hwid_os_version;
        if (!hwid_model.isEmpty()) json["hwid_model"] = hwid_model;
        if (update_interval > 0) json["update_interval"] = update_interval;
        if (respect_server_interval) json["respect_server_interval"] = *respect_server_interval;
        if (keep_working) json["keep_working"] = true;
        if (remove_duplicates) json["remove_duplicates"] = true;
        if (remove_insecure) json["remove_insecure"] = true;
        if (remove_invalid) json["remove_invalid"] = true;
        if (url_test) json["url_test"] = true;
        if (remove_unavailable) json["remove_unavailable"] = true;
        if (sort_by_latency) json["sort_by_latency"] = true;
        return json;
    }

    SubscriptionOptions SubscriptionOptions::FromJson(const QJsonObject &json) {
        SubscriptionOptions options;
        options.user_agent = json["user_agent"].toString();
        if (const int tls = json["tls_version"].toInt(-1); tls >= 0 && tls <= static_cast<int>(subTlsVersion::tls13)) {
            options.tls_version = static_cast<subTlsVersion>(tls);
        }
        if (const int http = json["http_version"].toInt(-1); http >= 0 && http <= static_cast<int>(subHttpVersion::http11)) {
            options.http_version = static_cast<subHttpVersion>(http);
        }
        const int mode = json["send_hwid"].toInt();
        if (mode == static_cast<int>(sendHwid::on) || mode == static_cast<int>(sendHwid::off)) {
            options.send_hwid = static_cast<sendHwid>(mode);
        }
        options.hwid = json["hwid"].toString();
        options.hwid_os = json["hwid_os"].toString();
        options.hwid_os_version = json["hwid_os_version"].toString();
        options.hwid_model = json["hwid_model"].toString();
        options.update_interval = std::max(json["update_interval"].toInt(), 0);
        if (json["respect_server_interval"].isBool()) options.respect_server_interval = json["respect_server_interval"].toBool();
        options.keep_working = json["keep_working"].toBool();
        options.remove_duplicates = json["remove_duplicates"].toBool();
        options.remove_insecure = json["remove_insecure"].toBool();
        options.remove_invalid = json["remove_invalid"].toBool();
        options.url_test = json["url_test"].toBool();
        options.remove_unavailable = json["remove_unavailable"].toBool();
        options.sort_by_latency = json["sort_by_latency"].toBool();
        return options;
    }

    void Group::clearCalculatedColumnWidth() {
        calculated_column_width.clear();
    }

    QList<int> Group::Profiles() const {
        return profiles;
    }

    double bitrateToBps(const QString& str)
    {
        if (str.endsWith("Gbps", Qt::CaseInsensitive)) {
            double val = str.left(str.size() - 4).toDouble();
            return val * 1e9;
        }
        if (str.endsWith("Mbps", Qt::CaseInsensitive)) {
            double val = str.left(str.size() - 4).toDouble();
            return val * 1e6;
        }
        if (str.endsWith("Kbps", Qt::CaseInsensitive)) {
            double val = str.left(str.size() - 4).toDouble();
            return val * 1e3;
        }
        if (str == "N/A") return -1;
        return 0.0;
    }

    namespace {
        bool sortReads(GroupSortMethod::GroupSortMethod method, SortInput input) {
            switch (method) {
                case GroupSortMethod::Raw:
                case GroupSortMethod::ById:
                    return false;
                case GroupSortMethod::ByTestResult:
                case GroupSortMethod::ByLatency:
                    return input == SortInput::Membership || input == SortInput::TestResult;
                case GroupSortMethod::ByTraffic:
                    return input == SortInput::Membership || input == SortInput::Traffic;
                default:
                    return input == SortInput::Membership || input == SortInput::Config;
            }
        }

        // Fields a method leaves unset stay equal, so one comparison serves every method.
        struct SortKey {
            bool missing = false;
            int level = 0;
            double number = 0;
            QString text;
        };

        int latencyForSort(int latency) {
            if (latency == 0) return 100000;
            if (latency < 0) return 99999;
            return latency;
        }

        class SortRule {
        public:
            SortRule(const Group &group, GroupSortAction action)
                : group(group), method(action.method), descending(action.descending) {
                if (method == GroupSortMethod::ByType || method == GroupSortMethod::BySecurity) {
                    method = dataManager->settingsRepo->show_config_security && group.type_sort_by == typeBy::bySecurity
                                 ? GroupSortMethod::BySecurity
                                 : GroupSortMethod::ByType;
                }
                byLatency = method == GroupSortMethod::ByLatency
                            || (method == GroupSortMethod::ByTestResult && group.test_sort_by == testBy::latency);
            }

            [[nodiscard]] SortKey keyOf(const std::shared_ptr<Profile> &profile) const {
                SortKey key;
                if (profile == nullptr || profile->outbound == nullptr) {
                    key.missing = true;
                } else if (method == GroupSortMethod::ByType) {
                    key.text = profile->outbound->DisplayType();
                } else if (method == GroupSortMethod::ByName) {
                    key.text = profile->outbound->name;
                } else if (method == GroupSortMethod::ByAddress) {
                    key.text = DisplayEffectiveAddress(profile);
                } else if (method == GroupSortMethod::BySecurity) {
                    const auto security = profile->outbound->EffectiveSecurity();
                    key.level = static_cast<int>(security.level);
                    key.text = security.transport + security.label;
                } else if (byLatency) {
                    key.number = latencyForSort(profile->latency);
                } else if (method == GroupSortMethod::ByTestResult) {
                    if (group.test_sort_by == testBy::dlSpeed) key.number = bitrateToBps(profile->dl_speed);
                    else if (group.test_sort_by == testBy::ulSpeed) key.number = bitrateToBps(profile->ul_speed);
                    else if (group.test_sort_by == testBy::riskFactor) {
                        key.level = profile->ip_risk_factor < 0 ? 1 : 0;
                        key.number = profile->ip_risk_factor;
                    }
                    else key.text = profile->ip_out;
                } else if (method == GroupSortMethod::ByTraffic) {
                    if (group.traffic_sort_by == trafficBy::dl) key.number = static_cast<double>(profile->traffic_downlink);
                    else if (group.traffic_sort_by == trafficBy::ul) key.number = static_cast<double>(profile->traffic_uplink);
                    else key.number = static_cast<double>(profile->traffic_downlink + profile->traffic_uplink);
                }
                return key;
            }

            // A profile that cannot be loaded goes last in either direction.
            [[nodiscard]] bool less(const SortKey &a, const SortKey &b) const {
                if (a.missing != b.missing) return b.missing;
                return descending ? ascending(b, a) : ascending(a, b);
            }

        private:
            static bool ascending(const SortKey &a, const SortKey &b) {
                if (a.level != b.level) return a.level < b.level;
                if (a.number != b.number) return a.number < b.number;
                return a.text < b.text;
            }

            const Group &group;
            GroupSortMethod::GroupSortMethod method;
            bool descending;
            bool byLatency = false;
        };

        class SortKeys {
        public:
            explicit SortKeys(const SortRule &rule) : rule(rule) {}

            void load(const QList<int> &ids) {
                QList<int> absent;
                for (int id : ids) {
                    if (!keys.contains(id)) absent << id;
                }
                if (absent.isEmpty()) return;
                for (const auto &profile : dataManager->profilesRepo->GetProfileBatch(absent)) {
                    keys.insert(profile->id, rule.keyOf(profile));
                }
                for (int id : absent) {
                    if (!keys.contains(id)) keys.insert(id, rule.keyOf(nullptr));
                }
            }

            // The id must be loaded already.
            [[nodiscard]] const SortKey &at(int id) const { return keys.constFind(id).value(); }

            // A copy: loading one more key can rehash the table under a held reference.
            SortKey get(int id) {
                load({id});
                return at(id);
            }

            [[nodiscard]] bool less(int a, int b) const { return rule.less(at(a), at(b)); }

        private:
            const SortRule &rule;
            QHash<int, SortKey> keys;
        };

        constexpr int kSortAttempts = 3;

        enum class Commit { Moved, Unchanged, Stale };

        // The keys were read unlocked, so a change that landed meanwhile means sorting again.
        Commit commit(Group &group, const QList<int> &snapshot, GroupSortAction action, QList<int> order) {
            QMutexLocker locker(&group.mutex);
            if (group.profiles != snapshot || group.sort != action) return Commit::Stale;
            if (order == group.profiles) return Commit::Unchanged;
            group.profiles = std::move(order);
            return Commit::Moved;
        }
    }

    bool Group::SortReads(SortInput input) const {
        return sortReads(sort.method, input);
    }

    bool Group::KeepsSorted() const {
        return SortReads(SortInput::Membership);
    }

    void Group::SetSort(GroupSortAction action) {
        QMutexLocker locker(&mutex);
        sort = action;
    }

    void Group::RestoreOriginalOrder() {
        QMutexLocker locker(&mutex);
        sort = {};
        if (manual_order.isEmpty()) {
            std::ranges::sort(profiles);
            return;
        }
        QSet<int> remaining(profiles.cbegin(), profiles.cend());
        QList<int> restored;
        restored.reserve(profiles.size());
        for (int id : manual_order) {
            if (remaining.remove(id)) restored.append(id);
        }
        // Deleted members stay deleted; newly imported members follow the baseline in import order.
        const auto baselineSize = restored.size();
        for (int id : profiles) {
            if (remaining.remove(id)) restored.append(id);
        }
        std::sort(restored.begin() + baselineSize, restored.end());
        profiles = std::move(restored);
        manual_order = profiles;
    }

    bool Group::SortProfiles(GroupSortAction action) {
        SetSort(action);
        return Resort();
    }

    bool Group::Resort() {
        for (int attempt = 0; attempt < kSortAttempts; ++attempt) {
            QList<int> snapshot;
            GroupSortAction action;
            {
                QMutexLocker locker(&mutex);
                if (!KeepsSorted()) return false;
                snapshot = profiles;
                action = sort;
            }

            const SortRule rule(*this, action);
            SortKeys keys(rule);
            keys.load(snapshot);
            // Stable, so a re-sort leaves rows with equal keys where they were.
            QList<int> order = snapshot;
            std::ranges::stable_sort(order, [&keys](int a, int b) { return keys.less(a, b); });

            switch (commit(*this, snapshot, action, std::move(order))) {
                case Commit::Moved: return true;
                case Commit::Unchanged: return false;
                case Commit::Stale: break;
            }
        }
        return false;
    }

    bool Group::Reposition(const QList<int> &changed) {
        if (changed.isEmpty()) return false;
        const QSet<int> changedSet(changed.cbegin(), changed.cend());
        for (int attempt = 0; attempt < kSortAttempts; ++attempt) {
            QList<int> snapshot;
            GroupSortAction action;
            {
                QMutexLocker locker(&mutex);
                if (!KeepsSorted()) return false;
                snapshot = profiles;
                action = sort;
            }

            const qsizetype count = snapshot.size();
            QList<qsizetype> positions;
            for (qsizetype i = 0; i < count; ++i) {
                if (changedSet.contains(snapshot[i])) positions << i;
            }
            if (positions.isEmpty()) return false;
            // A full sort loads each row once, a reinsertion about log2(n) rows one by one.
            const auto probes = static_cast<qsizetype>(std::bit_width(static_cast<quint64>(count))) + 4;
            if (positions.size() * probes >= count) return Resort();

            const SortRule rule(*this, action);
            SortKeys keys(rule);
            QList<int> around;
            for (const auto i : positions) {
                around << snapshot[i];
                if (i > 0) around << snapshot[i - 1];
                if (i + 1 < count) around << snapshot[i + 1];
            }
            keys.load(around);
            // The rest was sorted before, so the whole list still is when every changed row fits its neighbours.
            const bool inPlace = std::ranges::all_of(positions, [&](qsizetype i) {
                return (i == 0 || !keys.less(snapshot[i], snapshot[i - 1]))
                       && (i + 1 == count || !keys.less(snapshot[i + 1], snapshot[i]));
            });
            if (inPlace) return false;

            // Lift every changed row, not just the misplaced ones: a fitting row's neighbour may be leaving too.
            QList<int> order;
            order.reserve(count);
            for (int id : snapshot) {
                if (!changedSet.contains(id)) order << id;
            }
            for (const auto i : positions) {
                const int id = snapshot[i];
                const auto key = keys.get(id);
                // After equal keys, where a stable sort leaves a row that came last.
                const auto at = std::upper_bound(order.cbegin(), order.cend(), key, [&](const SortKey &k, int other) {
                    return rule.less(k, keys.get(other));
                });
                order.insert(at, id);
            }

            switch (commit(*this, snapshot, action, std::move(order))) {
                case Commit::Moved: return true;
                case Commit::Unchanged: return false;
                case Commit::Stale: break;
            }
        }
        return false;
    }

    bool Group::AddProfile(int ID)
    {
        QMutexLocker locker(&mutex);
        if (HasProfile(ID))
        {
            return false;
        }
        profiles.append(ID);
        return true;
    }

    bool Group::AddProfileBatch(const QList<int>& IDs) {
        QSet<int> currentProfiles;
        for (const auto& profileID : profiles) {
            currentProfiles.insert(profileID);
        }
        QMutexLocker locker(&mutex);
        for (auto profileID : IDs) {
            if (!currentProfiles.contains(profileID)) {
                profiles.append(profileID);
            }
        }
        return true;
    }

    bool Group::RemoveProfile(int ID)
    {
        QMutexLocker locker(&mutex);
        if (!HasProfile(ID)) return false;
        profiles.removeAll(ID);
        return true;
    }

    bool Group::RemoveProfileBatch(const QList<int>& IDs) {
        QSet<int> toDel;
        for (auto ID : IDs) {
            toDel.insert(ID);
        }
        QList<int> newIDs;
        QMutexLocker locker(&mutex);
        for (auto inID : profiles) {
            if (!toDel.contains(inID)) {
                newIDs.append(inID);
            }
        }
        profiles = newIDs;
        return true;
    }

    bool Group::SwapProfiles(int idx1, int idx2)
    {
        QMutexLocker locker(&mutex);
        if (profiles.size() <= idx1 || profiles.size() <= idx2) return false;
        profiles.swapItemsAt(idx1, idx2);
        sort = {};
        manual_order = profiles;
        return true;
    }

    bool Group::EmplaceProfile(int idx, int newIdx)
    {
        QMutexLocker locker(&mutex);
        if (profiles.size() <= idx || profiles.size() <= newIdx) return false;
        profiles.insert(newIdx+1, profiles[idx]);
        if (idx < newIdx) profiles.remove(idx);
        else profiles.remove(idx+1);
        sort = {};
        manual_order = profiles;
        return true;
    }

    bool Group::HasProfile(int ID) const
    {
        return profiles.contains(ID);
    }
}
