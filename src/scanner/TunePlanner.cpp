#include "include/scanner/TunePlanner.h"

#include <QJsonObject>

#include <algorithm>
#include <climits>
#include <cmath>
#include <map>
#include <utility>

namespace Scanner::Tune {
    namespace {
        constexpr double kTunePlannerZ = 1.96;
        // Each pass returns or opens a stage, so this only guards against a bug.
        constexpr int kTunePlannerMaxPasses = 256;

        int tunePlannerFind(const QList<Trial> &trials, const Assignment &assignment) {
            for (int i = 0; i < trials.size(); ++i) {
                if (trials[i].assignment == assignment) return i;
            }
            return -1;
        }

        bool tunePlannerHasStage(const QList<Trial> &trials, Stage stage) {
            return std::any_of(trials.cbegin(), trials.cend(), [stage](const Trial &trial) { return trial.stage == stage; });
        }

        Stage tunePlannerLatestStage(const QList<Trial> &trials) {
            Stage latest = Stage::Baseline;
            for (const auto &trial : trials) latest = std::max(latest, trial.stage);
            return latest;
        }

        bool tunePlannerConfirmStarted(const QList<Trial> &trials) {
            return std::any_of(trials.cbegin(), trials.cend(), [](const Trial &trial) { return trial.round > 1; });
        }

        Assignment tunePlannerUnion(const QList<Trial> &trials, const QList<int> &indexes) {
            Assignment merged;
            for (const int index : indexes) {
                const auto &assignment = trials[index].assignment;
                for (auto it = assignment.cbegin(); it != assignment.cend(); ++it) merged.insert(it.key(), it.value());
            }
            return merged;
        }

        int tunePlannerJsonInt(const QJsonObject &object, const char *key, int fallback, int min, int max) {
            const auto value = object.value(QLatin1String(key));
            if (!value.isDouble()) return fallback;
            return static_cast<int>(std::clamp(value.toDouble(), double(min), double(max)));
        }
    } // namespace

    bool Space::IsEmpty() const {
        const bool anyKnob = std::any_of(knobs.cbegin(), knobs.cend(), [](const Knob &knob) { return !knob.values.isEmpty(); });
        return !anyKnob && delay.values.isEmpty() && !(mtu && mtuMax > mtuMin);
    }

    bool Trial::Pending() const {
        if (dropped || planned <= 0) return false;
        if (stopOnSuccess && successes > 0) return false;
        return Attempts() < planned;
    }

    int Trial::MedianLatencyMs() const {
        if (latenciesMs.isEmpty()) return -1;
        auto sorted = latenciesMs;
        std::sort(sorted.begin(), sorted.end());
        const auto n = sorted.size();
        return n % 2 == 1 ? sorted[n / 2] : (sorted[n / 2 - 1] + sorted[n / 2]) / 2;
    }

    double Score(const Trial &trial) {
        const double n = trial.Attempts();
        if (n <= 0) return 0;
        const double p = trial.successes / n;
        const double z2 = kTunePlannerZ * kTunePlannerZ;
        const double centre = p + z2 / (2 * n);
        const double margin = kTunePlannerZ * std::sqrt(p * (1 - p) / n + z2 / (4 * n * n));
        return std::max(0.0, (centre - margin) / (1 + z2 / n));
    }

    bool RanksBefore(const Trial &a, const Trial &b) {
        const double scoreA = Score(a);
        const double scoreB = Score(b);
        if (std::abs(scoreA - scoreB) > 1e-9) return scoreA > scoreB;
        const int latencyA = a.MedianLatencyMs();
        const int latencyB = b.MedianLatencyMs();
        if (latencyA != latencyB) {
            if (latencyA < 0) return false;
            if (latencyB < 0) return true;
            return latencyA < latencyB;
        }
        return a.assignment.size() < b.assignment.size();
    }

    QList<int> Rank(const QList<Trial> &trials) {
        QList<int> order;
        order.reserve(trials.size());
        for (int i = 0; i < trials.size(); ++i) order << i;
        std::stable_sort(order.begin(), order.end(), [&trials](int a, int b) { return RanksBefore(trials[a], trials[b]); });
        return order;
    }

    int UsedAttempts(const QList<Trial> &trials) {
        int used = 0;
        for (const auto &trial : trials) used += trial.Attempts();
        return used;
    }

    QJsonArray TrialsToJson(const QList<Trial> &trials) {
        QJsonArray array;
        for (const auto &trial : trials) {
            QJsonObject assignment;
            for (auto it = trial.assignment.cbegin(); it != trial.assignment.cend(); ++it) assignment[it.key()] = it.value();
            QJsonArray latencies;
            for (const int ms : trial.latenciesMs) latencies.append(ms);
            QJsonObject object;
            object["assignment"] = assignment;
            object["stage"] = static_cast<int>(trial.stage);
            object["planned"] = trial.planned;
            object["round"] = trial.round;
            object["stopOnSuccess"] = trial.stopOnSuccess;
            object["dropped"] = trial.dropped;
            object["successes"] = trial.successes;
            object["failures"] = trial.failures;
            object["latencies"] = latencies;
            if (!trial.lastError.isEmpty()) object["error"] = trial.lastError;
            array.append(object);
        }
        return array;
    }

    QList<Trial> TrialsFromJson(const QJsonArray &array) {
        QList<Trial> trials;
        for (const auto &item : array) {
            if (!item.isObject()) continue;
            const auto object = item.toObject();
            Trial trial;
            const auto assignment = object.value(QLatin1String("assignment")).toObject();
            for (auto it = assignment.constBegin(); it != assignment.constEnd(); ++it) {
                if (it.value().isString()) trial.assignment.insert(it.key(), it.value().toString());
            }
            trial.stage = static_cast<Stage>(tunePlannerJsonInt(object, "stage", 0, 0, static_cast<int>(Stage::Mtu)));
            trial.planned = tunePlannerJsonInt(object, "planned", 0, 0, 100000);
            trial.round = tunePlannerJsonInt(object, "round", 1, 1, 2);
            trial.stopOnSuccess = object.value(QLatin1String("stopOnSuccess")).toBool();
            trial.dropped = object.value(QLatin1String("dropped")).toBool();
            trial.successes = tunePlannerJsonInt(object, "successes", 0, 0, 100000);
            trial.failures = tunePlannerJsonInt(object, "failures", 0, 0, 100000);
            for (const auto &latency : object.value(QLatin1String("latencies")).toArray()) {
                if (latency.isDouble()) trial.latenciesMs << std::max(0, latency.toInt());
            }
            trial.lastError = object.value(QLatin1String("error")).toString();
            trials << trial;
        }
        return trials;
    }

    Planner::Planner(Space space, Limits limits) : space_(std::move(space)), limits_(limits) {
        limits_.attempts = std::max(1, limits_.attempts);
        limits_.maxAttempts = std::max(1, limits_.maxAttempts);
        limits_.mtuAttempts = std::max(1, limits_.mtuAttempts);
        limits_.confirmTop = std::max(0, limits_.confirmTop);
        space_.mtuStep = std::max(1, space_.mtuStep);
    }

    int Planner::Next(QList<Trial> &trials) const {
        for (int pass = 0; pass < kTunePlannerMaxPasses; ++pass) {
            if (UsedAttempts(trials) >= limits_.maxAttempts) return -1;
            dropHopeless(trials);
            // Round-robin: the pending trial with the fewest attempts, the earliest on a tie.
            int pick = -1;
            for (int i = 0; i < trials.size(); ++i) {
                if (trials[i].Pending() && (pick < 0 || trials[i].Attempts() < trials[pick].Attempts())) pick = i;
            }
            if (pick >= 0) return pick;
            if (!openNextStage(trials)) return -1;
        }
        return -1;
    }

    int Planner::Best(const QList<Trial> &trials) const {
        int best = -1;
        for (int i = 0; i < trials.size(); ++i) {
            if (trials[i].stopOnSuccess) continue;
            if (best < 0 || RanksBefore(trials[i], trials[best])) best = i;
        }
        return best;
    }

    int Planner::MtuWinner(const QList<Trial> &trials) const {
        if (!space_.mtu || space_.mtuMax <= space_.mtuMin) return 0;
        const auto base = mtuBase(trials);
        int winner = 0;
        for (const auto &trial : trials) {
            auto stripped = trial.assignment;
            const auto value = stripped.take(kMtu);
            if (stripped != base || trial.successes <= 0) continue;
            const int mtu = value.isEmpty() ? space_.mtuCurrent : value.toInt();
            if (mtu >= space_.mtuMin && mtu <= space_.mtuMax) winner = std::max(winner, mtu);
        }
        return winner;
    }

    int Planner::EstimateTotal(const QList<Trial> &trials) const {
        const int used = UsedAttempts(trials);
        int estimate = used;
        for (const auto &trial : trials) {
            if (trial.Pending()) estimate += trial.planned - trial.Attempts();
        }
        if (!tunePlannerConfirmStarted(trials)) {
            const auto latest = trials.isEmpty() ? Stage::Baseline : tunePlannerLatestStage(trials);
            if (trials.isEmpty()) estimate += limits_.attempts;
            if (!tunePlannerHasStage(trials, Stage::Single)) {
                for (const auto &knob : space_.knobs) estimate += static_cast<int>(knob.values.size()) * limits_.attempts;
            }
            if (latest < Stage::Combine) estimate += limits_.attempts;
            if (latest < Stage::Refine) estimate += static_cast<int>(space_.delay.values.size()) * limits_.attempts;
            if (latest < Stage::Mtu) estimate += mtuBudget() / 2;
            estimate += limits_.confirmTop * limits_.attempts;
        }
        return std::max(used, std::min(estimate, limits_.maxAttempts));
    }

    void Planner::dropHopeless(QList<Trial> &trials) const {
        // A round's trials share `planned`, so one that cannot reach the leader's successes cannot outrank it.
        std::map<std::pair<int, int>, int> leaders;
        for (const auto &trial : trials) {
            if (trial.stopOnSuccess || trial.planned <= 0) continue;
            auto &leader = leaders[{trial.round, trial.planned}];
            leader = std::max(leader, trial.successes);
        }
        for (auto &trial : trials) {
            if (trial.stopOnSuccess || !trial.Pending()) continue;
            const auto it = leaders.find({trial.round, trial.planned});
            if (it != leaders.end() && trial.successes + (trial.planned - trial.Attempts()) < it->second) trial.dropped = true;
        }
    }

    bool Planner::openNextStage(QList<Trial> &trials) const {
        if (trials.isEmpty()) return addTrials(trials, {Assignment{}}, Stage::Baseline, INT_MAX);
        if (tunePlannerConfirmStarted(trials)) return false;
        for (int s = static_cast<int>(tunePlannerLatestStage(trials)); s <= static_cast<int>(Stage::Mtu); ++s) {
            const auto stage = static_cast<Stage>(s);
            // Every stage but the MTU bisection opens once.
            if (stage != Stage::Mtu && tunePlannerHasStage(trials, stage)) continue;
            bool opened = false;
            switch (stage) {
                case Stage::Baseline: break;
                case Stage::Single: opened = openSingles(trials); break;
                case Stage::Combine: opened = openCombine(trials); break;
                case Stage::Refine: opened = openRefine(trials); break;
                case Stage::Mtu: opened = openMtuProbe(trials); break;
            }
            if (opened) return true;
        }
        return startConfirm(trials);
    }

    bool Planner::openSingles(QList<Trial> &trials) const {
        // Breadth first, so a tight budget still tries every knob's first alternative.
        qsizetype depth = 0;
        for (const auto &knob : space_.knobs) depth = std::max(depth, knob.values.size());
        QList<Assignment> singles;
        for (qsizetype i = 0; i < depth; ++i) {
            for (const auto &knob : space_.knobs) {
                if (i < knob.values.size()) singles << Assignment{{knob.key, knob.values[i]}};
            }
        }
        return addTrials(trials, singles, Stage::Single, budgetFor(trials, Stage::Single));
    }

    bool Planner::openCombine(QList<Trial> &trials) const {
        int baseline = -1;
        for (int i = 0; i < trials.size() && baseline < 0; ++i) {
            if (trials[i].stage == Stage::Baseline) baseline = i;
        }
        // Per knob, its best single change, if that beats the profile as it is.
        QList<int> winners;
        for (const auto &knob : space_.knobs) {
            int best = -1;
            for (int i = 0; i < trials.size(); ++i) {
                const auto &trial = trials[i];
                if (trial.stage != Stage::Single || !trial.assignment.contains(knob.key)) continue;
                if (best < 0 || RanksBefore(trial, trials[best])) best = i;
            }
            if (best < 0) continue;
            if (baseline >= 0 ? RanksBefore(trials[best], trials[baseline]) : trials[best].successes > 0) winners << best;
        }
        std::stable_sort(winners.begin(), winners.end(), [&trials](int a, int b) { return RanksBefore(trials[a], trials[b]); });

        QList<Assignment> combos;
        if (winners.size() >= 2) {
            combos << tunePlannerUnion(trials, winners);
            if (winners.size() >= 3) combos << tunePlannerUnion(trials, winners.mid(0, 2));
        } else {
            const int best = Best(trials);
            const bool perfect = best >= 0 && trials[best].successes > 0 && trials[best].failures == 0;
            if (!perfect) {
                // Nothing helped alone: every evasion that is off now, together.
                auto all = tunePlannerUnion(trials, winners);
                for (const auto &knob : space_.knobs) {
                    if (all.contains(knob.key)) continue;
                    if (knob.values.contains(QLatin1String(kOn))) {
                        all.insert(knob.key, kOn);
                    } else if (knob.key == QLatin1String(kFingerprint)) {
                        int bestFingerprint = -1;
                        for (int i = 0; i < trials.size(); ++i) {
                            const auto &trial = trials[i];
                            if (trial.stage != Stage::Single || !trial.assignment.contains(knob.key) || trial.successes <= 0) continue;
                            if (bestFingerprint < 0 || RanksBefore(trial, trials[bestFingerprint])) bestFingerprint = i;
                        }
                        if (bestFingerprint >= 0) all.insert(knob.key, trials[bestFingerprint].assignment.value(knob.key));
                    }
                }
                if (all.size() >= 2) combos << all;
            }
        }
        return addTrials(trials, combos, Stage::Combine, budgetFor(trials, Stage::Combine));
    }

    bool Planner::openRefine(QList<Trial> &trials) const {
        if (space_.delay.values.isEmpty()) return false;
        int base = Best(trials);
        if (base >= 0 && !fragments(trials[base])) {
            // A delay only matters while fragmenting; with nothing working, refine the best variant that fragments.
            const bool nothingWorks = trials[base].successes <= 0;
            base = -1;
            if (nothingWorks) {
                for (const int i : Rank(trials)) {
                    if (!trials[i].stopOnSuccess && fragments(trials[i])) {
                        base = i;
                        break;
                    }
                }
            }
        }
        if (base < 0) return false;
        QList<Assignment> refined;
        for (const auto &delay : space_.delay.values) {
            auto assignment = trials[base].assignment;
            assignment.insert(kFragmentDelay, delay);
            refined << assignment;
        }
        return addTrials(trials, refined, Stage::Refine, budgetFor(trials, Stage::Refine));
    }

    bool Planner::openMtuProbe(QList<Trial> &trials) const {
        if (!space_.mtu || space_.mtuMax <= space_.mtuMin) return false;
        const auto base = mtuBase(trials);
        // 1 = passed, 0 = failed, -1 = not measured yet.
        const auto state = [&](int mtu) {
            const int index = mtuTrialIndex(trials, base, mtu);
            if (index < 0) return -1;
            if (trials[index].successes > 0) return 1;
            return trials[index].Pending() ? -1 : 0;
        };
        const auto probe = [&](int mtu) {
            if (limits_.maxAttempts - UsedAttempts(trials) - limits_.confirmTop * limits_.attempts < limits_.mtuAttempts) return false;
            Trial trial;
            trial.assignment = base;
            if (mtu != space_.mtuCurrent) trial.assignment.insert(kMtu, QString::number(mtu));
            trial.stage = Stage::Mtu;
            trial.planned = limits_.mtuAttempts;
            trial.stopOnSuccess = true;
            trials << trial;
            return true;
        };

        const int top = state(space_.mtuMax);
        if (top < 0) return probe(space_.mtuMax);
        if (top > 0) return false;
        const int bottom = state(space_.mtuMin);
        if (bottom < 0) return probe(space_.mtuMin);
        // Not even the smallest MTU passes: the MTU is not what fails.
        if (bottom == 0) return false;

        int low = space_.mtuMin;
        QList<std::pair<int, bool>> measured;
        for (const auto &trial : trials) {
            auto stripped = trial.assignment;
            const auto value = stripped.take(kMtu);
            if (stripped != base || trial.Pending()) continue;
            const int mtu = value.isEmpty() ? space_.mtuCurrent : value.toInt();
            if (mtu < space_.mtuMin || mtu > space_.mtuMax) continue;
            measured.append({mtu, trial.successes > 0});
            if (trial.successes > 0) low = std::max(low, mtu);
        }
        int high = space_.mtuMax;
        for (const auto &[mtu, passed] : measured) {
            if (!passed && mtu > low) high = std::min(high, mtu);
        }
        if (high - low <= space_.mtuStep) return false;
        int mid = space_.mtuMin + ((low + (high - low) / 2 - space_.mtuMin) / space_.mtuStep) * space_.mtuStep;
        if (mid <= low) mid = low + space_.mtuStep;
        if (mid >= high) return false;
        return probe(mid);
    }

    bool Planner::startConfirm(QList<Trial> &trials) const {
        const int target = limits_.attempts * 2;
        int budget = limits_.maxAttempts - UsedAttempts(trials);
        QList<int> picks;
        // The MTU winner first: when the profile's own MTU fails, it is the fix.
        if (const int mtu = MtuWinner(trials); mtu > 0) {
            if (const int index = mtuTrialIndex(trials, mtuBase(trials), mtu); index >= 0) picks << index;
        }
        int taken = 0;
        for (const int index : Rank(trials)) {
            if (taken >= limits_.confirmTop) break;
            const auto &trial = trials[index];
            if (trial.dropped || trial.stopOnSuccess || trial.successes <= 0) continue;
            ++taken;
            if (!picks.contains(index)) picks << index;
        }
        bool started = false;
        for (const int index : picks) {
            auto &trial = trials[index];
            const int cost = target - trial.Attempts();
            if (cost <= 0 || cost > budget) continue;
            budget -= cost;
            trial.planned = target;
            trial.round = 2;
            trial.stopOnSuccess = false;
            trial.dropped = false;
            started = true;
        }
        return started;
    }

    bool Planner::addTrials(QList<Trial> &trials, const QList<Assignment> &assignments, Stage stage, int budget) const {
        bool added = false;
        for (const auto &assignment : assignments) {
            if (budget < limits_.attempts) break;
            if (tunePlannerFind(trials, assignment) >= 0) continue;
            Trial trial;
            trial.assignment = assignment;
            trial.stage = stage;
            trial.planned = limits_.attempts;
            trials << trial;
            budget -= limits_.attempts;
            added = true;
        }
        return added;
    }

    int Planner::budgetFor(const QList<Trial> &trials, Stage stage) const {
        int reserve = limits_.confirmTop * limits_.attempts;
        if (stage < Stage::Mtu) reserve += mtuBudget();
        return limits_.maxAttempts - UsedAttempts(trials) - reserve;
    }

    int Planner::mtuBudget() const {
        if (!space_.mtu || space_.mtuMax <= space_.mtuMin) return 0;
        int steps = (space_.mtuMax - space_.mtuMin) / space_.mtuStep;
        int probes = 2;
        while (steps > 1) {
            steps = (steps + 1) / 2;
            ++probes;
        }
        return probes * limits_.mtuAttempts;
    }

    Assignment Planner::mtuBase(const QList<Trial> &trials) const {
        for (const auto &trial : trials) {
            if (trial.stage == Stage::Mtu) {
                auto base = trial.assignment;
                base.remove(kMtu);
                return base;
            }
        }
        const int best = Best(trials);
        return best >= 0 ? trials[best].assignment : Assignment{};
    }

    int Planner::mtuTrialIndex(const QList<Trial> &trials, const Assignment &base, int mtu) const {
        auto assignment = base;
        if (mtu != space_.mtuCurrent) assignment.insert(kMtu, QString::number(mtu));
        return tunePlannerFind(trials, assignment);
    }

    bool Planner::fragments(const Trial &trial) const {
        return trial.assignment.value(kFragment, space_.fragmentOn ? kOn : kOff) == QLatin1String(kOn);
    }
} // namespace Scanner::Tune
