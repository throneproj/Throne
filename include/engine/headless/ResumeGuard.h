#pragma once

#include <QJsonObject>
#include <QString>

namespace Engine {
    // <config>/headless-state.json: the boot-resume crash-loop guard and the last exit. Main thread; QSaveFile writes.
    // {"version":1,"resume":{"profile":X,"strikes":n,"armed":b,"armed_at":t},"boot":{"open":b,"unstable":n},
    //  "last_exit":{"code":c,"reason":"..","at":t}}
    class ResumeGuard {
    public:
        static constexpr int kStrikesToSuppress = 3;
        // CoreSupervisor::kStableMs.
        static constexpr qint64 kSurviveMs = 60000;
        // Boots in a row that never had a core connected for kSurviveMs; from this many on, a boot stays up instead of exiting
        // 70/71, so a cause only the API can fix stays fixable.
        static constexpr int kUnstableBootsToStayUp = 3;

        // Loads the file; a missing or corrupt one is a fresh record (a corrupt one is logged once).
        explicit ResumeGuard(const QString &configDir);

        // First thing every boot, resume or not: a previous boot that ended armed strikes its profile (logged). Afterwards only
        // an arm made by this process is ever struck or cleared. Also settles the boot streak: a previous boot still open
        // (never markStable(), whatever its exit) adds one unstable boot; this boot opens (saved).
        void settlePrevious();

        // After settlePrevious(): the earlier boots in a row that never became stable.
        int unstableBoots() const;

        bool stayUp() const { return unstableBoots() >= kUnstableBootsToStayUp; }

        // A core stayed connected for kSurviveMs in this boot: the streak resets and this boot closes. Idempotent.
        void markStable();

        // After settlePrevious(). Another profile resets the record. -1 when candidate < 0 (record untouched) or when it is
        // suppressed (logged).
        int decide(int candidateId);

        bool suppressed() const;

        // Another profile started, so remember_id names it now and the next boot will not suppress: the suppression is lifted
        // in memory, the record stays on disk. A no-op for the record's own profile.
        void liftSuppression(int startedProfileId);

        // The last_exit the previous boot wrote; {} when none.
        QJsonObject previousExit() const;

        // The boot start was dispatched.
        void arm(int profileId);

        // No strike.
        void disarm();

        // An unexpected core exit while armed; disarms.
        void strike();

        // Running for kSurviveMs: strikes 0, disarmed, suppression lifted.
        void survived(int profileId);

        // Writes last_exit. Only 70 and 71, the exits a profile can cause, stay armed (the next boot strikes); others disarm.
        void noteExit(int code, const QString &reason);

        bool armed() const;

    private:
        void save();

        QString m_path;
        int m_profile = -1;
        int m_strikes = 0;
        bool m_armed = false;
        qint64 m_armedAt = 0;
        bool m_suppressed = false;
        bool m_bootOpen = false;
        int m_unstableBoots = 0;
        QJsonObject m_previousExit;
        QJsonObject m_lastExit;
    };
} // namespace Engine
