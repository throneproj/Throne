#include "include/engine/vpn/VpnAuthController.h"

#include "include/api/RPC.h"
#include "include/database/DatabaseManager.h"
#include "include/database/OtpProfilesRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EngineControl.h"
#include "include/engine/EnginePolicy.h"
#include "include/engine/EngineTr.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/vpn/VpnEndpoints.h"
#include "include/global/OtpPlaceholder.hpp"
#include "include/global/RunningProfiles.hpp"
#include "include/global/Utils.hpp"
#include "include/global/VpnCredentialOverride.hpp"

#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <utility>

namespace {
    QString vpnAuthControllerFieldHaystack(const VpnAuthField &field) {
        return (field.name + QChar(' ') + field.label).toLower();
    }

    bool vpnAuthControllerLooksLikeToken(const VpnAuthField &field) {
        static const QStringList hints = {QStringLiteral("token"),        QStringLiteral("otp"),
                                          QStringLiteral("passcode"),     QStringLiteral("one-time"),
                                          QStringLiteral("onetime"),      QStringLiteral("second"),
                                          QStringLiteral("challenge"),    QStringLiteral("verification"),
                                          QStringLiteral("authenticator")};
        const auto haystack = vpnAuthControllerFieldHaystack(field);
        return std::any_of(hints.begin(), hints.end(),
                           [&](const QString &hint) { return haystack.contains(hint); });
    }

    bool vpnAuthControllerLooksLikeUsername(const VpnAuthField &field) {
        const auto haystack = vpnAuthControllerFieldHaystack(field);
        return haystack.contains(QStringLiteral("user")) || haystack.contains(QStringLiteral("login")) ||
               haystack.contains(QStringLiteral("account"));
    }

    bool vpnAuthControllerBuildFormAnswer(const VpnAuthChallenge &challenge, const Configs::openconnect *ocon,
                                          const QString &user, const QString &pass, const QString &code,
                                          QMap<QString, QString> *out) {
        if (ocon == nullptr || challenge.fields.isEmpty()) return false;
        bool passwordUsed = false;
        for (const auto &field : challenge.fields) {
            QString value;
            bool resolved = false;
            for (const auto &entry : ocon->form_entries) {
                if (entry == nullptr || entry->promote) continue;
                const bool byKey = !entry->submission_key.isEmpty() &&
                                   entry->submission_key == field.submissionKey;
                // A field reaches a challenge only when the core matched no entry, so a name match can only be the withheld {otp} entry.
                const bool byName = entry->submission_key.isEmpty() && !entry->name.isEmpty() &&
                                    entry->name == field.name &&
                                    entry->value.contains(Configs::kOtpPlaceholder);
                if (!byKey && !byName) continue;
                value = Configs::SubstituteOtp(entry->value, code);
                resolved = true;
            }
            if (!resolved && field.kind == QStringLiteral("password")) {
                if (!passwordUsed && !vpnAuthControllerLooksLikeToken(field) && !pass.isEmpty()) {
                    value = Configs::SubstituteOtp(pass, code);
                    passwordUsed = true;
                } else {
                    value = code;
                }
                resolved = true;
            }
            if (!resolved && vpnAuthControllerLooksLikeUsername(field) && !user.isEmpty()) {
                value = Configs::SubstituteOtp(user, code);
                resolved = true;
            }
            if (!resolved && !field.value.isEmpty()) {
                value = field.value;
                resolved = true;
            }
            if (!resolved) return false;
            out->insert(field.submissionKey, value);
        }
        return !out->isEmpty();
    }
} // namespace

namespace Engine {
    VpnAuthController::VpnAuthController(EnginePolicy *policy, EngineControl *control, QObject *parent)
        : QObject(parent), m_policy(policy), m_control(control) {}

    void VpnAuthController::noteRunStarted() {
        if (m_vpnChallengeTimer == nullptr) {
            m_vpnChallengeTimer = new QTimer(this);
            m_vpnChallengeTimer->setInterval(kPollIntervalMs);
            connect(m_vpnChallengeTimer, &QTimer::timeout, this, [this] { poll(); });
        }
        resetTracking();
        m_vpnChallengeTimer->start();
    }

    void VpnAuthController::noteRunStopped(int profileId, bool manual) {
        // start() stops the old run first, so only a stop outside a restart is the user's.
        const bool restartingForAuth = manual && profileId == m_vpnAuthRestartID;
        m_vpnAuthRestartID = -1;
        if (manual && !restartingForAuth) clearCredentialOverrides();
        stopPoll();
    }

    void VpnAuthController::answer(const QString &endpointTag, const QString &challengeId, const VpnAuthAnswer &reply,
                                   const std::function<void(const QString &error)> &done) {
        ENGINE_CHECK_MAIN_THREAD();
        runOnNewThread([endpointTag, challengeId, reply, done] {
            bool rpcOK = false;
            const auto error = API::defaultClient->SubmitVPNChallenge(&rpcOK, endpointTag, challengeId, reply.username,
                                                                      reply.password, reply.secret, reply.formValues);
            if (done) runOnUiThread([done, error] { done(error); });
        });
    }

    void VpnAuthController::cancel(const QString &endpointTag, const QString &challengeId) {
        ENGINE_CHECK_MAIN_THREAD();
        runOnNewThread([endpointTag, challengeId] {
            bool rpcOK = false;
            API::defaultClient->CancelVPNChallenge(&rpcOK, endpointTag, challengeId);
        });
    }

    QList<VpnAuthChallenge> VpnAuthController::pendingChallenges() const {
        return m_vpnPendingChallenges;
    }

    QString VpnAuthController::exitStateText() const {
        return m_vpnEndpointState;
    }

    void VpnAuthController::cancelAtDeadline(const VpnAuthChallenge &challenge, int capSec) {
        ENGINE_CHECK_MAIN_THREAD();
        if (challenge.id.isEmpty()) return;
        const auto key = challenge.endpointTag + QChar(0x1F) + challenge.id;
        if (!m_cancelTimers.value(key).isNull()) return;

        qint64 delaySec = std::max(capSec, 0);
        if (challenge.deadline > 0) {
            delaySec = std::clamp<qint64>(challenge.deadline - QDateTime::currentSecsSinceEpoch(), 0, delaySec);
        }
        const auto tag = challenge.endpointTag;
        const auto id = challenge.id;
        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(static_cast<int>(std::min<qint64>(delaySec * 1000, std::numeric_limits<int>::max())));
        connect(timer, &QTimer::timeout, this, [this, key, tag, id, timer] {
            if (m_cancelTimers.value(key) == timer) m_cancelTimers.remove(key);
            timer->deleteLater();
            const bool pending = std::any_of(m_vpnPendingChallenges.cbegin(), m_vpnPendingChallenges.cend(),
                                             [&](const VpnAuthChallenge &c) { return c.endpointTag == tag && c.id == id; });
            if (!pending) return;
            MW_show_log(QStringLiteral("[VPN] %1: sign-in prompt cancelled at its deadline").arg(Stats::VpnEndpointDisplayName(tag)));
            cancel(tag, id);
        });
        m_cancelTimers.insert(key, timer);
        timer->start();
    }

    bool VpnAuthController::authFailed() const {
        return m_authFailed;
    }

    void VpnAuthController::stopPoll() {
        if (m_vpnChallengeTimer != nullptr) m_vpnChallengeTimer->stop();
        resetTracking();
        Stats::SetVpnEndpointProfiles({});
        Configs::ClearRunningProfiles();
        policy()->vpnChallengesCleared();
    }

    // Per-run only: the auto-restart budget must not be cleared here, the restart it counts comes through.
    void VpnAuthController::resetTracking() {
        m_vpnChallengeSeen.clear();
        m_vpnEndpointState.clear();
        m_vpnEndpointLastState.clear();
        m_vpnTroubleSummary.clear();
        m_vpnTroubleDetail.clear();
        m_vpnOtpLastCode.clear();
        m_vpnOtpRejects.clear();
        m_vpnChallengeAnswering.clear();
        m_vpnPendingChallenges.clear();
        for (const auto &timer : std::as_const(m_cancelTimers)) {
            if (timer.isNull()) continue;
            timer->stop();
            timer->deleteLater();
        }
        m_cancelTimers.clear();
        m_authFailed = false;
        emit endpointStatusChanged({}, {}, false);
    }

    void VpnAuthController::poll() {
        if (Configs::dataManager->settingsRepo->started_id < 0) return;
        if (m_vpnChallengeBusy.exchange(true)) return;

        runOnNewThread([this] {
            bool rpcOK = false;
            const auto status = API::defaultClient->QueryVPNStatus(&rpcOK, {});

            QList<VpnAuthChallenge> pending;
            QList<QPair<QString, QString>> authFailures;
            QList<VpnEndpointState> endpointStates;
            if (rpcOK) {
                for (const auto &res : status.results) {
                    const auto tag = QString::fromStdString(res.tag.value());

                    VpnEndpointState endpointState;
                    endpointState.tag = tag;
                    endpointState.state = QString::fromStdString(res.state.value());
                    endpointState.error = QString::fromStdString(res.error.value());
                    endpointState.connected = res.connected.value();
                    endpointState.authFailed = res.auth_failed.value();
                    endpointStates.append(endpointState);

                    if (endpointState.authFailed) {
                        authFailures.append({tag, endpointState.error});
                    }
                    if (!res.challenge.has_value()) continue;
                    const auto &raw = res.challenge.value();

                    VpnAuthChallenge challenge;
                    challenge.endpointTag = QString::fromStdString(raw.endpoint_tag.value());
                    if (challenge.endpointTag.isEmpty()) challenge.endpointTag = tag;
                    challenge.id = QString::fromStdString(raw.id.value());
                    challenge.kind = QString::fromStdString(raw.kind.value());
                    challenge.username = QString::fromStdString(raw.username.value());
                    challenge.message = QString::fromStdString(raw.message.value());
                    challenge.banner = QString::fromStdString(raw.banner.value());
                    challenge.error = QString::fromStdString(raw.error.value());
                    challenge.url = QString::fromStdString(raw.url.value());
                    challenge.echo = raw.echo.value();
                    challenge.deadline = raw.deadline.value();
                    for (const auto &rawField : raw.fields) {
                        VpnAuthField field;
                        field.submissionKey = QString::fromStdString(rawField.submission_key.value());
                        field.name = QString::fromStdString(rawField.name.value());
                        field.label = QString::fromStdString(rawField.label.value());
                        if (field.label.isEmpty()) field.label = field.name;
                        field.kind = QString::fromStdString(rawField.kind.value());
                        field.value = QString::fromStdString(rawField.value.value());
                        for (const auto &rawOption : rawField.options) {
                            field.options.append({QString::fromStdString(rawOption.value.value()),
                                                  QString::fromStdString(rawOption.label.value())});
                        }
                        challenge.fields.append(field);
                    }
                    pending.append(challenge);
                }
            }

            runOnUiThread([=, this] {
                m_vpnChallengeBusy.store(false);
                if (m_vpnChallengeTimer == nullptr || !m_vpnChallengeTimer->isActive()) return;
                updateEndpointStates(endpointStates);
                m_vpnPendingChallenges.clear();
                for (const auto &challenge : pending) {
                    if (autoAnswer(challenge)) continue;
                    showChallenge(challenge);
                }
                for (const auto &[tag, error] : authFailures) showAuthFailure(tag, error);
            });
        });
    }

    void VpnAuthController::updateEndpointStates(const QList<VpnEndpointState> &states) {
        QString exitState;
        QStringList summaryParts;
        QStringList detailParts;
        QHash<QString, QString> seenStates;
        bool problem = false;
        m_authFailed = std::any_of(states.cbegin(), states.cend(), [](const VpnEndpointState &state) { return state.authFailed; });

        for (const auto &state : states) {
            const auto name = Stats::VpnEndpointDisplayName(state.tag);
            const auto text = Stats::VpnExitStateText(state.state, state.error);
            if (state.tag == QStringLiteral("proxy")) exitState = text;
            if (state.connected) m_vpnOtpRejects.remove(state.tag);

            const auto previous = m_vpnEndpointLastState.value(state.tag);
            seenStates.insert(state.tag, state.state);
            if (previous != state.state && !previous.isEmpty()) MW_show_log(MainWindowTr::tr("[VPN] %1: %2").arg(name, text));

            if (state.connected) continue;
            if (state.authFailed || state.state == QStringLiteral("error")) problem = true;
            summaryParts << QStringLiteral("%1: %2").arg(name, Stats::VpnStateText(state.state));
            detailParts << (state.error.isEmpty() ? QStringLiteral("%1: %2").arg(name, Stats::VpnStateText(state.state))
                                                  : QStringLiteral("%1: %2").arg(name, state.error));
        }
        m_vpnEndpointLastState = seenStates;

        QString summary;
        if (!summaryParts.isEmpty()) {
            summary = (problem ? MainWindowTr::tr("VPN endpoint problem") : MainWindowTr::tr("VPN endpoint")) + ": " +
                      QStringList(summaryParts.mid(0, 2)).join(QStringLiteral("; "));
            if (summaryParts.size() > 2) summary += QStringLiteral(" (+%1)").arg(summaryParts.size() - 2);
        }
        const auto detail = QStringList(detailParts.mid(0, 3)).join(QStringLiteral(" | "));

        if (summary == m_vpnTroubleSummary && detail == m_vpnTroubleDetail && exitState == m_vpnEndpointState) return;
        m_vpnTroubleSummary = summary;
        m_vpnTroubleDetail = detail;
        m_vpnEndpointState = exitState;
        emit endpointStatusChanged(summary, detail, problem);
        emit exitStateChanged();
    }

    bool VpnAuthController::autoAnswer(const VpnAuthChallenge &challenge) {
        if (challenge.id.isEmpty() || m_control->runningSnapshot() == nullptr) return false;
        if (m_vpnChallengeAnswering.contains(challenge.endpointTag)) return true;

        const int profileID = Stats::VpnEndpointProfileID(challenge.endpointTag);
        if (profileID < 0) return false;
        const auto ent = Configs::dataManager->profilesRepo->GetProfile(profileID);
        if (ent == nullptr) return false;

        const auto *ovpn = ent->OpenVPN();
        const auto *ocon = ent->OpenConnect();
        int otpID = -1;
        QString storedUser, storedPass;
        if (ovpn != nullptr) {
            otpID = ovpn->otp_profile_id;
            storedUser = ovpn->username;
            storedPass = ovpn->password;
        } else if (ocon != nullptr) {
            otpID = ocon->otp_profile_id;
            storedUser = ocon->username;
            storedPass = ocon->password;
        }
        if (otpID < 0) return false;

        const auto otpProfile = Configs::dataManager->otpProfilesRepo->GetOtpProfile(otpID);
        if (otpProfile == nullptr || !otpProfile->Validate().isEmpty()) return false;

        if (!challenge.error.isEmpty()) {
            if (m_vpnOtpRejects.value(challenge.endpointTag) >= kMaxOtpRejects) return false;
            // The same digits were just refused; hold until the TOTP window rolls instead of replaying.
            if (otpProfile->type == OTP::Type::TOTP &&
                otpProfile->CurrentCode() == m_vpnOtpLastCode.value(challenge.endpointTag)) {
                return true;
            }
        }

        const auto creds = Configs::ResolveVpnCredentials(profileID, storedUser, storedPass);
        const bool credentialsKind = ovpn != nullptr && challenge.kind == QStringLiteral("credentials");

        // Settle the shape before minting: resolving a HOTP code spends a counter step.
        if (ovpn != nullptr) {
            if (challenge.kind != QStringLiteral("secret") && !credentialsKind) return false;
            if (credentialsKind && creds.username.isEmpty() && creds.password.isEmpty()) return false;
        } else if (challenge.kind != QStringLiteral("form")) {
            return false;
        }

        const auto code = Configs::ResolveOtpCode(otpID);
        if (code.isEmpty()) return false;

        QString user, pass, secret;
        QMap<QString, QString> formValues;
        if (ovpn != nullptr) {
            // The core packs the answer as SCRV1 whatever it gets, so the code rides the secret slot.
            secret = code;
            if (credentialsKind) {
                user = Configs::SubstituteOtp(creds.username, code);
                pass = Configs::SubstituteOtp(creds.password, code);
            }
        } else if (!vpnAuthControllerBuildFormAnswer(challenge, ocon, creds.username, creds.password, code, &formValues)) {
            return false;
        }

        if (!challenge.error.isEmpty()) {
            m_vpnOtpRejects[challenge.endpointTag] = m_vpnOtpRejects.value(challenge.endpointTag) + 1;
        }
        m_vpnOtpLastCode.insert(challenge.endpointTag, code);
        submitAutoAnswer(challenge, user, pass, secret, formValues);
        return true;
    }

    void VpnAuthController::submitAutoAnswer(const VpnAuthChallenge &challenge, const QString &username,
                                             const QString &password, const QString &secret,
                                             const QMap<QString, QString> &formValues) {
        const auto tag = challenge.endpointTag;
        const auto id = challenge.id;
        const auto name = Stats::VpnEndpointDisplayName(tag);
        m_vpnChallengeAnswering.insert(tag);

        runOnNewThread([=, this] {
            bool rpcOK = false;
            const auto error = API::defaultClient->SubmitVPNChallenge(&rpcOK, tag, id, username, password, secret, formValues);
            runOnUiThread([=, this] {
                m_vpnChallengeAnswering.remove(tag);
                if (!rpcOK) {
                    MW_show_log(MainWindowTr::tr("[VPN] %1: the core did not answer the sign-in prompt.").arg(name));
                } else if (!error.isEmpty()) {
                    MW_show_log(MainWindowTr::tr("[VPN] %1: could not answer the sign-in prompt: %2").arg(name, error));
                } else {
                    MW_show_log(MainWindowTr::tr("[VPN] %1: signed in again with a new one-time code.").arg(name));
                }
            });
        });
    }

    bool VpnAuthController::autoRestartForAuth(const QString &endpointTag, int profileID) {
        const auto ent = Configs::dataManager->profilesRepo->GetProfile(profileID);
        const auto running = m_control->runningSnapshot();
        if (ent == nullptr || running == nullptr) return false;

        int otpID = -1;
        if (const auto *ovpn = ent->OpenVPN(); ovpn != nullptr) otpID = ovpn->otp_profile_id;
        else if (const auto *ocon = ent->OpenConnect(); ocon != nullptr) {
            if (ocon->password_authentication_disabled) return false;
            otpID = ocon->otp_profile_id;
        }
        if (otpID < 0) return false;
        if (m_vpnAutoRestarts.value(profileID) >= kMaxAutoRestarts) return false;

        const auto now = QDateTime::currentSecsSinceEpoch();
        if (now - m_vpnAutoRestartAt < kAutoRestartCooldownSecs) return true;

        m_vpnAutoRestarts[profileID] = m_vpnAutoRestarts.value(profileID) + 1;
        m_vpnAutoRestartAt = now;

        MW_show_log(MainWindowTr::tr("[VPN] %1 rejected the saved credentials; restarting the profile with a new one-time code.")
                        .arg(Stats::VpnEndpointDisplayName(endpointTag)));
        const int runningID = running->id;
        m_vpnAuthRestartID = runningID;
        m_control->start(StartRequest{runningID});
        return true;
    }

    void VpnAuthController::showChallenge(const VpnAuthChallenge &challenge) {
        if (challenge.id.isEmpty()) return;
        m_vpnPendingChallenges.append(challenge);
        if (policy()->vpnPromptOpen()) return;

        const auto key = challenge.endpointTag + QChar(0x1F) + challenge.id;
        if (m_vpnChallengeSeen.contains(key)) return;
        m_vpnChallengeSeen.insert(key);

        policy()->vpnChallenge(challenge);
    }

    void VpnAuthController::showAuthFailure(const QString &endpointTag, const QString &error) {
        const auto running = m_control->runningSnapshot();
        if (policy()->vpnPromptOpen() || running == nullptr) return;
        const int profileID = Stats::VpnEndpointProfileID(endpointTag);
        if (profileID < 0) return;

        const auto key = endpointTag + QChar(0x1F) + "auth-failed";
        if (m_vpnChallengeSeen.contains(key)) return;

        // The core reads credentials once, at build time; rebuilding is the only way to hand it a new code.
        if (autoRestartForAuth(endpointTag, profileID)) return;

        m_vpnChallengeSeen.insert(key);
        if (m_vpnAuthPrompted.value(profileID) >= kMaxCredentialPrompts) return;

        const auto ent = Configs::dataManager->profilesRepo->GetProfile(profileID);
        if (ent == nullptr) return;

        VpnAuthChallenge challenge;
        challenge.endpointTag = endpointTag;
        challenge.kind = "credentials";
        if (const auto *ovpn = ent->OpenVPN(); ovpn != nullptr) challenge.username = ovpn->username;
        else if (const auto *ocon = ent->OpenConnect(); ocon != nullptr) {
            // The core reports a refused password-less login the same way, and credentials cannot fix it.
            if (ocon->password_authentication_disabled) return;
            challenge.username = ocon->username;
        }
        else return;
        challenge.error = error;
        challenge.message = MainWindowTr::tr("The server refused the credentials saved with this profile. Enter the ones "
                                             "to use for this session; the profile itself is left unchanged.");

        m_vpnAuthPrompted[profileID] = m_vpnAuthPrompted.value(profileID) + 1;
        MW_show_log(MainWindowTr::tr("[VPN] %1 rejected the saved credentials.").arg(Stats::VpnEndpointDisplayName(endpointTag)));

        const int runningID = running->id;
        policy()->vpnCredentialsRejected(profileID, challenge, [=, this](const QString &username, const QString &password) {
            retryWithCredentials(runningID, profileID, username, password);
        });
    }

    void VpnAuthController::retryWithCredentials(int runningProfileId, int profileId, const QString &username,
                                                 const QString &password) {
        if (const auto current = m_control->runningSnapshot(); current == nullptr || current->id != runningProfileId) return;
        Configs::SetVpnCredentialOverride(profileId, {username, password});
        m_vpnAuthRestartID = runningProfileId;
        m_control->start(StartRequest{runningProfileId});
    }

    void VpnAuthController::clearCredentialOverrides() {
        for (auto it = m_vpnAuthPrompted.constBegin(); it != m_vpnAuthPrompted.constEnd(); ++it) {
            Configs::ClearVpnCredentialOverride(it.key());
        }
        m_vpnAuthPrompted.clear();
        m_vpnAutoRestarts.clear();
        m_vpnAutoRestartAt = 0;
    }
} // namespace Engine
