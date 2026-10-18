#include "include/engine/headless/ResumeGuard.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSaveFile>

#include "include/engine/app/ExitCodes.h"
#include "include/global/Logger.hpp"

namespace Engine {
    namespace {
        constexpr int kResumeGuardVersion = 1;
        constexpr qint64 kResumeGuardMaxBytes = 64 * 1024;
        constexpr int kResumeGuardMaxUnstableBoots = 1000000;
    } // namespace

    ResumeGuard::ResumeGuard(const QString &configDir) : m_path(QDir(configDir).filePath(QStringLiteral("headless-state.json"))) {
        QFile file(m_path);
        if (!file.exists()) return;
        const auto fresh = [this](const QString &why) {
            LOG_WARN(QStringLiteral("[Resume] %1 %2; starting with a fresh record.").arg(QDir::toNativeSeparators(m_path), why));
        };
        if (!file.open(QIODevice::ReadOnly)) {
            fresh(QStringLiteral("cannot be read (%1)").arg(file.errorString()));
            return;
        }
        if (file.size() > kResumeGuardMaxBytes) {
            fresh(QStringLiteral("is too large"));
            return;
        }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            fresh(QStringLiteral("is not valid JSON"));
            return;
        }
        const auto root = document.object();
        if (root.value(QStringLiteral("version")).toInt() != kResumeGuardVersion) {
            fresh(QStringLiteral("has an unknown version"));
            return;
        }
        const auto resume = root.value(QStringLiteral("resume")).toObject();
        m_profile = resume.value(QStringLiteral("profile")).toInt(-1);
        if (m_profile < 0) m_profile = -1;
        m_strikes = qBound(0, resume.value(QStringLiteral("strikes")).toInt(0), kStrikesToSuppress);
        m_armed = m_profile >= 0 && resume.value(QStringLiteral("armed")).toBool(false);
        m_armedAt = m_armed ? resume.value(QStringLiteral("armed_at")).toInteger(0) : 0;
        const auto boot = root.value(QStringLiteral("boot")).toObject();
        m_bootOpen = boot.value(QStringLiteral("open")).toBool(false);
        m_unstableBoots = qBound(0, boot.value(QStringLiteral("unstable")).toInt(0), kResumeGuardMaxUnstableBoots);
        m_previousExit = root.value(QStringLiteral("last_exit")).toObject();
        // Consumed: a crash of this run must not report the previous run's exit again.
        if (!m_previousExit.isEmpty()) save();
    }

    void ResumeGuard::settlePrevious() {
        if (m_bootOpen) m_unstableBoots = qMin(m_unstableBoots + 1, kResumeGuardMaxUnstableBoots);
        m_bootOpen = true;
        if (!m_armed) {
            save();
            return;
        }
        m_strikes = qMin(m_strikes + 1, kStrikesToSuppress);
        m_armed = false;
        m_armedAt = 0;
        save();
        LOG_WARN(QStringLiteral("[Resume] The previous run ended within %1 s of resuming profile %2 (%3 of %4 in a row).")
                     .arg(kSurviveMs / 1000)
                     .arg(m_profile)
                     .arg(m_strikes)
                     .arg(kStrikesToSuppress));
    }

    int ResumeGuard::unstableBoots() const {
        return m_unstableBoots;
    }

    void ResumeGuard::markStable() {
        if (!m_bootOpen && m_unstableBoots == 0) return;
        m_bootOpen = false;
        m_unstableBoots = 0;
        save();
    }

    int ResumeGuard::decide(int candidateId) {
        m_suppressed = false;
        if (candidateId < 0) return -1;
        if (candidateId != m_profile) {
            const bool hadRecord = m_profile >= 0;
            m_profile = candidateId;
            m_strikes = 0;
            m_armed = false;
            m_armedAt = 0;
            if (hadRecord) save();
            return candidateId;
        }
        if (m_strikes >= kStrikesToSuppress) {
            m_suppressed = true;
            LOG_WARN(QStringLiteral("[Resume] not resuming profile %1: %2 boots in a row ended within %3 s of starting it; "
                                    "start it once (thronectl) to clear")
                         .arg(m_profile)
                         .arg(m_strikes)
                         .arg(kSurviveMs / 1000));
            return -1;
        }
        return candidateId;
    }

    bool ResumeGuard::suppressed() const {
        return m_suppressed;
    }

    void ResumeGuard::liftSuppression(int startedProfileId) {
        if (startedProfileId >= 0 && startedProfileId != m_profile) m_suppressed = false;
    }

    QJsonObject ResumeGuard::previousExit() const {
        return m_previousExit;
    }

    void ResumeGuard::arm(int profileId) {
        if (profileId < 0) return;
        if (profileId != m_profile) {
            m_profile = profileId;
            m_strikes = 0;
        }
        m_armed = true;
        m_armedAt = QDateTime::currentSecsSinceEpoch();
        save();
    }

    void ResumeGuard::disarm() {
        if (!m_armed) return;
        m_armed = false;
        m_armedAt = 0;
        save();
    }

    void ResumeGuard::strike() {
        if (!m_armed) return;
        m_strikes = qMin(m_strikes + 1, kStrikesToSuppress);
        m_armed = false;
        m_armedAt = 0;
        save();
        LOG_WARN(QStringLiteral("[Resume] The core exited within %1 s of resuming profile %2 (%3 of %4 in a row).")
                     .arg(kSurviveMs / 1000)
                     .arg(m_profile)
                     .arg(m_strikes)
                     .arg(kStrikesToSuppress));
    }

    void ResumeGuard::survived(int profileId) {
        if (profileId < 0 || profileId != m_profile) return;
        if (m_strikes == 0 && !m_armed && !m_suppressed) return;
        const bool wasCounting = m_strikes > 0 || m_suppressed;
        m_strikes = 0;
        m_armed = false;
        m_armedAt = 0;
        m_suppressed = false;
        save();
        if (wasCounting) {
            LOG_INFO(QStringLiteral("[Resume] Profile %1 ran for %2 s; its crash count is cleared.").arg(profileId).arg(kSurviveMs / 1000));
        }
    }

    void ResumeGuard::noteExit(int code, const QString &reason) {
        m_lastExit = QJsonObject{
            {QStringLiteral("code"), code},
            {QStringLiteral("reason"), reason},
            {QStringLiteral("at"), QDateTime::currentSecsSinceEpoch()},
        };
        // In a runtime 1/78 come only from the API bind retry, never from the profile.
        if (code != ExitCode::CoreGaveUp && code != ExitCode::Stuck) {
            m_armed = false;
            m_armedAt = 0;
        }
        save();
    }

    bool ResumeGuard::armed() const {
        return m_armed;
    }

    void ResumeGuard::save() {
        QJsonObject root{
            {QStringLiteral("version"), kResumeGuardVersion},
            {QStringLiteral("resume"), QJsonObject{
                {QStringLiteral("profile"), m_profile},
                {QStringLiteral("strikes"), m_strikes},
                {QStringLiteral("armed"), m_armed},
                {QStringLiteral("armed_at"), m_armedAt},
            }},
            {QStringLiteral("boot"), QJsonObject{
                {QStringLiteral("open"), m_bootOpen},
                {QStringLiteral("unstable"), m_unstableBoots},
            }},
        };
        if (!m_lastExit.isEmpty()) root.insert(QStringLiteral("last_exit"), m_lastExit);
        QSaveFile file(m_path);
        if (!file.open(QIODevice::WriteOnly)) {
            LOG_WARN(QStringLiteral("[Resume] Cannot write %1: %2").arg(QDir::toNativeSeparators(m_path), file.errorString()));
            return;
        }
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact) + '\n');
        if (!file.commit()) {
            LOG_WARN(QStringLiteral("[Resume] Cannot write %1: %2").arg(QDir::toNativeSeparators(m_path), file.errorString()));
        }
    }
} // namespace Engine
