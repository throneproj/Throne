#pragma once

#include <atomic>
#include <functional>

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>

#include "include/engine/vpn/VpnAuth.h"

class QTimer;

namespace Engine {
    class EngineControl;
    class EnginePolicy;

    // Sign-in side of the running profile's OpenVPN/OpenConnect endpoints: polls them, answers OTP challenges, asks the policy
    // for the rest.
    class VpnAuthController final : public QObject {
        Q_OBJECT

    public:
        static constexpr int kPollIntervalMs = 2000;
        static constexpr int kMaxOtpRejects = 3;
        static constexpr int kMaxAutoRestarts = 3;
        static constexpr qint64 kAutoRestartCooldownSecs = 20;
        static constexpr qint64 kStableConnectedSecs = 60;
        static constexpr int kMaxCredentialPrompts = 3;

        // policy and control outlive the controller.
        VpnAuthController(EnginePolicy *policy, EngineControl *control, QObject *parent = nullptr);

        // Main thread, from the start success hop.
        void noteRunStarted();

        // Main thread, from the stop teardown, before another start or stop can be accepted. switching: a start's stop of the
        // previous profile.
        void noteRunStopped(int profileId, bool manual, bool switching);

        // Main thread; returns at once. done runs on the main thread with the core's error, empty when accepted.
        void answer(const QString &endpointTag, const QString &challengeId, const VpnAuthAnswer &reply,
                    const std::function<void(const QString &error)> &done = {});

        void cancel(const QString &endpointTag, const QString &challengeId);

        // Challenges of the current run left to a person, as of the last poll; nothing reads it yet.
        QList<VpnAuthChallenge> pendingChallenges() const;

        QString exitStateText() const;

        // Main thread. Cancels the challenge at the earlier of its own deadline (epoch seconds) and now + capSec, unless it is
        // no longer pending or the run ended.
        void cancelAtDeadline(const VpnAuthChallenge &challenge, int capSec);

        // An endpoint reported an authentication failure on the last poll.
        bool authFailed() const;

    signals:
        void endpointStatusChanged(const QString &summary, const QString &detail, bool problem);

        // After a poll changed the status line; follows endpointStatusChanged.
        void exitStateChanged();

    private:
        EnginePolicy *policy() const { return m_policy; }

        void stopPoll();

        void resetTracking();

        void poll();

        void updateEndpointStates(const QList<VpnEndpointState> &states);

        bool autoAnswer(const VpnAuthChallenge &challenge);

        void submitAutoAnswer(const VpnAuthChallenge &challenge, const QString &username, const QString &password,
                              const QString &secret, const QMap<QString, QString> &formValues);

        void autoRestartForAuth(const QString &endpointTag, int profileID);

        void offerRestartForAuth(const QString &endpointTag, int profileID, const QString &error);

        void showChallenge(const VpnAuthChallenge &challenge);

        void showAuthFailure(const QString &endpointTag, const QString &error);

        void retryWithCredentials(int runningProfileId, int profileId, const QString &username, const QString &password);

        void restartForAuth(int runningProfileId, bool interactive);

        void clearCredentialOverrides();

        EnginePolicy *m_policy;
        EngineControl *m_control;
        QList<VpnAuthChallenge> m_vpnPendingChallenges;

        QTimer *m_vpnChallengeTimer = nullptr;
        std::atomic<bool> m_vpnChallengeBusy{false};
        QSet<QString> m_vpnChallengeSeen;
        QString m_vpnEndpointState;
        QString m_vpnTroubleSummary;
        QString m_vpnTroubleDetail;
        QHash<QString, QString> m_vpnEndpointLastState;
        QHash<QString, QString> m_vpnOtpLastCode;
        QHash<QString, int> m_vpnOtpRejects;
        QHash<QString, qint64> m_vpnOtpRejectAt;
        QSet<QString> m_vpnChallengeAnswering;
        QHash<int, int> m_vpnAutoRestarts;
        qint64 m_vpnAutoRestartAt = 0;
        // This run already restarted for a sign-in, or asked whether to; one question per run.
        bool m_vpnRestartOffered = false;
        quint64 m_vpnRunSerial = 0;
        // Survives the restart the recovery itself triggers, so a rejected retry cannot loop.
        QHash<int, int> m_vpnAuthPrompted;
        int m_vpnAuthRestartID = -1;
        quint64 m_vpnAuthRestartSerial = 0;
        // Keyed by endpoint tag + U+001F + challenge id; cleared in resetTracking().
        QHash<QString, QPointer<QTimer>> m_cancelTimers;
        bool m_authFailed = false;
    };
} // namespace Engine
