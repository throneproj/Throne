#include "include/engine/connection/EngineState.h"

#include "include/database/DatabaseManager.h"
#include "include/global/Utils.hpp"

#include <QMutexLocker>

namespace Engine {
    std::shared_ptr<Configs::Profile> EngineState::running() const {
        QMutexLocker locker(&m_runningMutex);
        return m_running;
    }

    bool EngineState::runningTun() const {
        QMutexLocker locker(&m_runningMutex);
        return m_runningTun;
    }

    void EngineState::setRunning(std::shared_ptr<Configs::Profile> profile, bool tun) {
        QMutexLocker locker(&m_runningMutex);
        m_running.swap(profile);
        m_runningTun = m_running != nullptr && tun;
    }

    int EngineState::resumeTarget() const {
        return m_resumeTarget.load();
    }

    void EngineState::setResumeTarget(int profileId, bool persist) {
        m_resumeTarget.store(profileId);
        if (persist) persistResumeTarget(profileId);
    }

    bool EngineState::clearResumeTargetIf(int profileId, bool persist) {
        int expected = profileId;
        if (!m_resumeTarget.compare_exchange_strong(expected, Configs::NoProfileId)) return false;
        if (persist) persistResumeTarget(Configs::NoProfileId);
        return true;
    }

    void EngineState::persistResumeTarget(int profileId) {
        const quint64 sequence = ++*m_persistSequence;
        runOnUiThread([latest = m_persistSequence, sequence, profileId] {
            // A later persisting call has run already or is queued behind this one.
            if (latest->load() != sequence) return;
            auto *settings = Configs::dataManager->settingsRepo.get();
            if (settings->noSave) return;
            settings->remember_id = profileId;
            settings->Save();
        });
    }

    ConnectionState EngineState::connectionState() const {
        if (connecting) return ConnectionState::Connecting;
        if (disconnecting) return ConnectionState::Stopping;
        return running() != nullptr ? ConnectionState::Running : ConnectionState::Idle;
    }
} // namespace Engine
