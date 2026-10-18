#include "include/stats/traffic/TrafficLooper.hpp"

#include "include/api/RPC.h"
#include "include/global/Utils.hpp"
#include "include/stats/StatsSink.hpp"

#include <QObject>
#include <QThread>
#include <QJsonDocument>
#include <QHash>
#include <QSet>

#include <cmath>
#include <limits>

#include "include/configs/generate.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/GroupsRepo.h"
#include "include/database/DatabaseManager.h"
#include "include/stats/traffic/TrafficStatsManager.hpp"


namespace Stats {

    TrafficLooper *trafficLooper = new TrafficLooper;

    namespace {
        constexpr qint64 kTrafficLooperProbeIntervalMs = 500;
        constexpr qint64 kTrafficLooperEmitIntervalMs = 1000;
        constexpr qint64 kTrafficLooperSaveIntervalMs = 10000;
        constexpr qint64 kTrafficLooperGapMs = 2000;
        constexpr size_t kTrafficLooperRateWindowProbes = 3;
        constexpr qsizetype kTrafficLooperMaxExtraLines = 4;
    }

    void TrafficLooper::UpdateAll() {
        if (Configs::dataManager->settingsRepo->disable_traffic_stats) {
            return;
        }

        auto resp = API::defaultClient->QueryStats();
        const auto now = MonotonicMs();
        const auto durationMs = lastProbeMs > 0 ? now - lastProbeMs : 0;
        lastProbeMs = now;
        if (durationMs > kTrafficLooperGapMs) rateWindow.clear();
        RateProbe probe{durationMs, QVector<qint64>(lines.size() * 2, 0)};

        for (auto& group : groups) {
            const auto& tagKey = group.watchTagKey;
            if (!resp.ups.contains(tagKey)) continue;
            const auto up = resp.ups.at(tagKey);
            const auto down = resp.downs.at(tagKey);
            // An auto-selector contributes one group per pool member, nearly all idle, so skip the zero deltas.
            if (up != 0 || down != 0) {
                for (auto& profile : group.profiles) {
                    profile->traffic_uplink += up;
                    profile->traffic_downlink += down;
                    trafficStatsManager->AddConfigDelta(profile->id, up, down);
                }
                group.dirty = true;
            }
            probe.bytes[group.line * 2] += down;
            probe.bytes[group.line * 2 + 1] += up;
        }

        const std::string directTag = "direct";
        if (resp.ups.contains(directTag)) {
            const auto up = resp.ups.at(directTag);
            const auto down = resp.downs.at(directTag);
            trafficStatsManager->AddConfigDelta(DIRECT_STAT_PROFILE_ID, up, down);
            if (!probe.bytes.isEmpty()) {
                probe.bytes[probe.bytes.size() - 2] += down;
                probe.bytes.last() += up;
            }
        }

        if (durationMs <= 0 || durationMs > kTrafficLooperGapMs) return;
        rateWindow.push_back(std::move(probe));
        while (rateWindow.size() > kTrafficLooperRateWindowProbes) rateWindow.pop_front();
    }

    QVector<double> TrafficLooper::WindowRates() const {
        QVector<double> rates(lines.size() * 2, 0.0);
        qint64 windowMs = 0;
        for (const auto& probe : rateWindow) {
            windowMs += probe.durationMs;
            for (qsizetype i = 0; i < rates.size(); ++i) rates[i] += static_cast<double>(probe.bytes[i]);
        }
        if (windowMs <= 0) {
            rates.fill(std::numeric_limits<double>::quiet_NaN());
            return rates;
        }
        for (auto& rate : rates) rate = rate * 1000.0 / static_cast<double>(windowMs);
        return rates;
    }

    void TrafficLooper::Loop() {
        qint64 lastEmitMs = 0;
        qint64 lastPersistMs = MonotonicMs();
        // Half a probe of slack, so sleep jitter can't push an emission to the third probe.
        const auto emitDue = [&lastEmitMs](qint64 now) {
            if (now - lastEmitMs < kTrafficLooperEmitIntervalMs - kTrafficLooperProbeIntervalMs / 2) return false;
            lastEmitMs = now;
            return true;
        };
        while (true) {
            QThread::msleep(kTrafficLooperProbeIntervalMs);

            if (Configs::dataManager->settingsRepo->disable_traffic_stats) {
                continue;
            }

            if (!loop_enabled) {
                {
                    QMutexLocker lk(&loop_mutex);
                    rateWindow.clear();
                    for (const auto& entry : {proxy, direct}) {
                        if (!entry) continue;
                        entry->downlink_rate = 0;
                        entry->uplink_rate = 0;
                    }
                }
                if (looping) {
                    looping = false;
                    runOnUiThread([=] {
                        if (auto *sink = CurrentStatsSink()) sink->onTrafficStopped();
                    });
                }
                if (const auto now = MonotonicMs(); emitDue(now)) {
                    runOnUiThread([=] {
                        if (auto *sink = CurrentStatsSink()) sink->onTrafficSample(TrafficSample{.monoMs = now});
                    });
                }
                continue;
            } else {
                if (!looping) {
                    looping = true;
                }
            }

            qint64 now = 0;
            bool due = false;
            QVector<double> values;
            double proxyDown = 0, proxyUp = 0, directDown = 0, directUp = 0;
            QString status;
            QList<int> ids;
            {
                QMutexLocker lk(&loop_mutex);
                UpdateAll();
                now = MonotonicMs();
                due = !lines.isEmpty() && emitDue(now);
                if (due) {
                    values = WindowRates();
                    const auto directLine = lines.size() - 1;
                    for (qsizetype line = 0; line < lines.size(); ++line) {
                        const auto down = values[line * 2];
                        const auto up = values[line * 2 + 1];
                        if (std::isnan(down) || std::isnan(up)) continue;
                        if (line == directLine) {
                            directDown = down;
                            directUp = up;
                        } else {
                            proxyDown += down;
                            proxyUp += up;
                        }
                    }
                    proxy->downlink_rate = proxyDown;
                    proxy->uplink_rate = proxyUp;
                    direct->downlink_rate = directDown;
                    direct->uplink_rate = directUp;
                    status = QObject::tr("Proxy: %1\nDirect: %2").arg(DisplaySpeed(proxy), DisplaySpeed(direct));
                    ids = creditedIds;
                }
            }

            if (now - lastPersistMs >= kTrafficLooperSaveIntervalMs) {
                lastPersistMs = now;
                PersistTraffic();
            }

            if (!due) continue;
            runOnUiThread([=] {
                if (auto *sink = CurrentStatsSink()) {
                    sink->onTrafficSample(TrafficSample{now, true, values, proxyDown, proxyUp, directDown, directUp, status, ids});
                }
            });
        }
    }

    void TrafficLooper::PersistTraffic() {
        QList<std::shared_ptr<Configs::Profile>> all;
        {
            QMutexLocker lk(&loop_mutex);
            // A profile can appear in several groups (an auto selector is credited by every member), so dedup first.
            QSet<int> seen;
            for (auto& group : groups) {
                if (!group.dirty) continue;
                group.dirty = false;
                for (const auto& profile : group.profiles) {
                    if (!profile || profile->id < 0) continue;
                    if (seen.contains(profile->id)) continue;
                    seen.insert(profile->id);
                    all.append(profile);
                }
            }
        }
        if (all.isEmpty()) return;
        if (Configs::dataManager && Configs::dataManager->profilesRepo) {
            Configs::dataManager->profilesRepo->SaveTrafficBatch(all);
        }
        // At the save cadence rather than every tick, so a traffic-sorted list moves at most every few seconds.
        Configs::RepositionLater(all, Configs::SortInput::Traffic);
    }

    void TrafficLooper::SetChainGroups(const QList<Configs::TrafficChainGroup>& configGroups) {
        const auto lineKey = [](const Configs::TrafficChainGroup& group) -> QString {
            if (group.profileId < 0) return {};
            switch (group.kind) {
                case Configs::TrafficChainKind::Route: return QStringLiteral("route:%1").arg(group.profileId);
                case Configs::TrafficChainKind::Aux: return QStringLiteral("aux:%1").arg(group.profileId);
                default: return {};
            }
        };

        QList<TrafficLooperGroup> newGroups;
        QStringList groupKeys;
        QList<TrafficLine> extras;
        QSet<QString> extraKeys;
        for (const auto& configGroup : configGroups) {
            if (configGroup.watchTag.isEmpty() || configGroup.profiles.isEmpty()) continue;
            TrafficLooperGroup g;
            g.watchTag = configGroup.watchTag;
            g.watchTagKey = configGroup.watchTag.toStdString();
            g.profiles = configGroup.profiles;
            newGroups.append(g);

            const auto key = lineKey(configGroup);
            groupKeys << key;
            if (key.isEmpty() || extraKeys.contains(key)) continue;
            extraKeys.insert(key);
            extras << TrafficLine{key, configGroup.name,
                                  configGroup.kind == Configs::TrafficChainKind::Route ? TrafficLineKind::Route : TrafficLineKind::Aux};
        }

        QList<TrafficLine> newLines{{QStringLiteral("proxy"), QObject::tr("Proxy"), TrafficLineKind::Proxy}};
        QHash<QString, int> lineOf;
        const auto ownLines = extras.size() <= kTrafficLooperMaxExtraLines ? extras.size() : kTrafficLooperMaxExtraLines - 1;
        for (qsizetype i = 0; i < extras.size(); ++i) {
            if (i < ownLines) {
                lineOf.insert(extras[i].key, static_cast<int>(newLines.size()));
                newLines << extras[i];
            } else {
                if (i == ownLines) newLines << TrafficLine{QStringLiteral("other"), QObject::tr("Other outbounds"), TrafficLineKind::Other};
                lineOf.insert(extras[i].key, static_cast<int>(newLines.size() - 1));
            }
        }
        newLines << TrafficLine{QStringLiteral("direct"), QObject::tr("Direct"), TrafficLineKind::Direct};
        for (qsizetype i = 0; i < newGroups.size(); ++i) newGroups[i].line = lineOf.value(groupKeys[i], 0);

        QList<int> ids;
        QSet<int> seen;
        for (const auto& group : newGroups) {
            for (const auto& profile : group.profiles) {
                if (!profile || profile->id < 0) continue;
                if (seen.contains(profile->id)) continue;
                seen.insert(profile->id);
                ids << profile->id;
            }
        }

        {
            QMutexLocker lk(&loop_mutex);
            if (!proxy) proxy = std::make_shared<TrafficLooperEntry>();
            if (!direct) direct = std::make_shared<TrafficLooperEntry>();
            *proxy = TrafficLooperEntry{QStringLiteral("proxy")};
            *direct = TrafficLooperEntry{QStringLiteral("direct")};
            groups = newGroups;
            lines = newLines;
            creditedIds = ids;
            rateWindow.clear();
            lastProbeMs = MonotonicMs();
        }
        runOnUiThread([newLines] {
            if (auto *sink = CurrentStatsSink()) sink->onTrafficLines(newLines);
        });

        trafficStatsManager->EnsureDirectMeta();
        QSet<int> snapshotted;
        for (const auto& g : newGroups) {
            for (const auto& p : g.profiles) {
                if (!p || p->id < 0) continue;
                if (snapshotted.contains(p->id)) continue;
                snapshotted.insert(p->id);
                QString groupName;
                if (const auto grp = Configs::dataManager->groupsRepo->GetGroup(p->gid)) groupName = grp->name;
                trafficStatsManager->SnapshotConfigMeta(
                    p->id,
                    p->outbound ? p->outbound->DisplayName() : p->name,
                    groupName,
                    p->type,
                    Configs::DisplayEffectiveAddress(p));
            }
        }
    }

} // namespace Stats
