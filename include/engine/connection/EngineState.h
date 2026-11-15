#pragma once

#include <atomic>
#include <memory>

#include <QList>
#include <QMap>
#include <QMutex>

#include "include/engine/EngineTypes.h"
#include "include/global/Const.hpp"

namespace Configs {
    class Profile;
}

namespace Engine {
    // The engine's observable run state, shared by ConnectionManager, StartCoordinator and the Lifecycle resume-target hooks.
    // StartCoordinator writes it, except the resume-target clears that reach it through the Lifecycle accessors.
    class EngineState final {
    public:
        // Any thread; written by the start and stop bodies.
        std::shared_ptr<Configs::Profile> running() const;

        // Any thread. The running config carries a tun inbound.
        bool runningTun() const;

        // tun: the running config carries a tun inbound (generated Tun or a custom config's).
        void setRunning(std::shared_ptr<Configs::Profile> profile, bool tun);

        // Any thread. What a restarted core resumes: set by a successful start, cleared by a manual stop, kept by a crash stop.
        int resumeTarget() const;

        // persist: remember_id is set and saved as well, on the main thread (posted from others); the latest such call wins.
        void setResumeTarget(int profileId, bool persist);

        // Clears only while the target is profileId; true when it did.
        bool clearResumeTargetIf(int profileId, bool persist);

        // Main thread, as are the fields below.
        ConnectionState connectionState() const;

        bool connecting = false;
        bool disconnecting = false;
        ConnectionState published = ConnectionState::Idle;
        int lastTarget = -1;
        QList<RestartReason> restartReasons;
        // Per pending reason, the restartEpoch of its latest note; every note raises the counter.
        QMap<RestartReason, quint64> restartNotedAt;
        quint64 restartEpoch = 0;

    private:
        void persistResumeTarget(int profileId);

        mutable QMutex m_runningMutex;
        std::shared_ptr<Configs::Profile> m_running;
        bool m_runningTun = false;
        std::atomic<int> m_resumeTarget{Configs::NoProfileId};
        // Shared with the posted saves, which may outlive this object.
        std::shared_ptr<std::atomic<quint64>> m_persistSequence = std::make_shared<std::atomic<quint64>>(0);
    };
} // namespace Engine
