#pragma once

#include <QJsonArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

namespace Scanner::Tune {
    inline constexpr auto kFingerprint = "fingerprint";
    inline constexpr auto kFragment = "fragment";
    inline constexpr auto kFragmentDelay = "fragment_delay";
    inline constexpr auto kRecordFragment = "record_fragment";
    inline constexpr auto kMixedCaseSni = "mixed_case_sni";
    inline constexpr auto kQuicOpener = "quic_opener";
    inline constexpr auto kMtu = "mtu";

    inline constexpr auto kOn = "on";
    inline constexpr auto kOff = "off";

    // Knob -> value a variant changes; empty = the profile as it is.
    using Assignment = QMap<QString, QString>;

    enum class Stage { Baseline = 0, Single = 1, Combine = 2, Refine = 3, Mtu = 4 };

    struct Knob {
        QString key;
        QString current;
        // In the order to try them; never `current`.
        QStringList values;
    };

    struct Space {
        QList<Knob> knobs;
        // Fragment delays, tried only on the best variant that fragments.
        Knob delay;
        bool fragmentOn = false;
        bool mtu = false;
        int mtuCurrent = 0;
        int mtuMin = 0;
        int mtuMax = 0;
        int mtuStep = 10;

        [[nodiscard]] bool IsEmpty() const;
    };

    struct Limits {
        int attempts = 6;
        int maxAttempts = 150;
        // Per MTU probe, which passes on its first success.
        int mtuAttempts = 3;
        int confirmTop = 3;
    };

    struct Trial {
        Assignment assignment;
        Stage stage = Stage::Baseline;
        int planned = 0;
        int round = 1;
        bool stopOnSuccess = false;
        // It could no longer catch its round's leader, or the core refused its config.
        bool dropped = false;
        int successes = 0;
        int failures = 0;
        QList<int> latenciesMs;
        QString lastError;

        [[nodiscard]] int Attempts() const { return successes + failures; }
        [[nodiscard]] bool Pending() const;
        [[nodiscard]] int MedianLatencyMs() const;
    };

    // Wilson lower bound (95 %) of the success rate.
    [[nodiscard]] double Score(const Trial &trial);

    // Score first, then median latency, then fewer changes.
    [[nodiscard]] bool RanksBefore(const Trial &a, const Trial &b);

    [[nodiscard]] QList<int> Rank(const QList<Trial> &trials);

    [[nodiscard]] int UsedAttempts(const QList<Trial> &trials);

    [[nodiscard]] QJsonArray TrialsToJson(const QList<Trial> &trials);
    [[nodiscard]] QList<Trial> TrialsFromJson(const QJsonArray &array);

    // Baseline -> one change at a time -> the winners combined -> fragment delays -> MTU bisection -> a second
    // round for the best few.
    class Planner {
    public:
        Planner(Space space, Limits limits);

        // The trial to attempt next, or -1 when the search is over. It depends only on `trials`, so a resumed search
        // carries on where it stopped.
        int Next(QList<Trial> &trials) const;

        // Skips MTU probes that were never confirmed.
        [[nodiscard]] int Best(const QList<Trial> &trials) const;

        [[nodiscard]] int MtuWinner(const QList<Trial> &trials) const;

        [[nodiscard]] int EstimateTotal(const QList<Trial> &trials) const;

    private:
        void dropHopeless(QList<Trial> &trials) const;
        bool openNextStage(QList<Trial> &trials) const;
        bool openSingles(QList<Trial> &trials) const;
        bool openCombine(QList<Trial> &trials) const;
        bool openRefine(QList<Trial> &trials) const;
        bool openMtuProbe(QList<Trial> &trials) const;
        bool startConfirm(QList<Trial> &trials) const;
        bool addTrials(QList<Trial> &trials, const QList<Assignment> &assignments, Stage stage, int budget) const;
        [[nodiscard]] int budgetFor(const QList<Trial> &trials, Stage stage) const;
        [[nodiscard]] int mtuBudget() const;
        [[nodiscard]] Assignment mtuBase(const QList<Trial> &trials) const;
        [[nodiscard]] int mtuTrialIndex(const QList<Trial> &trials, const Assignment &base, int mtu) const;
        [[nodiscard]] bool fragments(const Trial &trial) const;

        Space space_;
        Limits limits_;
    };
} // namespace Scanner::Tune
