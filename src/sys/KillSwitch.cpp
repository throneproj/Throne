#include "include/sys/KillSwitch.hpp"

#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#include "include/sys/Process.hpp"
#include "include/sys/ServiceManager.hpp"

#include <QDeadlineTimer>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

#include <chrono>
#include <future>
#include <memory>
#include <utility>

namespace Sys {
    namespace {
        constexpr int kReadyTimeoutMs = 15000;
        constexpr int kCloseGraceMs = 5000;
        constexpr int kRespawnIntervalMs = 30000;
        constexpr int kShutdownWaitMs = 3000;

        const QString kPrivilege = QStringLiteral("privilege");
        const QString kUnsupported = QStringLiteral("unsupported");
        const QString kConfig = QStringLiteral("config");
        const QString kTampered = QStringLiteral("tampered");
        const QString kInternal = QStringLiteral("internal");
        const QString kCrash = QStringLiteral("crash");
        const QString kTimeout = QStringLiteral("timeout");
        const QString kOverridden = QStringLiteral("overridden");

        std::atomic<quint16> g_killSwitchApiPort{0};
        // Set once instance() has built the switch on the UI thread; SetApiPort never creates it from a worker.
        std::atomic<KillSwitch *> g_killSwitchInstance{nullptr};

        QString killSwitchIdentityMessage(const QString &detail) {
            auto message = QStringLiteral("the core cannot adopt the kill-switch group 1950896719 here (it needs CAP_SETGID and that GID "
                                          "mapped in this user namespace); the kill switch keeps blocking traffic, and profiles stay "
                                          "refused until kill_switch is turned off");
            if (!detail.isEmpty()) message += QStringLiteral("; the core reported: %1").arg(detail);
            return message;
        }

        // Must match genTunName() and the l3-direct outbound's bridge_name in generate.cpp.
        QString tunName() {
#ifdef Q_OS_MACOS
            return {};
#else
            return QStringLiteral("throne-tun");
#endif
        }

        QString bridgeName() {
#ifdef Q_OS_LINUX
            if (Configs::dataManager->settingsRepo->vpn_l3_bridge) return QStringLiteral("throne-br");
#endif
            return {};
        }

#ifdef Q_OS_WIN
        constexpr int kPermitTimeoutMs = 15000;

        // The guard drops relative entries; resolve them against the working directory the core inherits.
        QString nativePath(const QString &path) {
            const auto trimmed = path.trimmed();
            if (trimmed.isEmpty()) return {};
            return QDir::toNativeSeparators(QDir::cleanPath(QDir::current().absoluteFilePath(trimmed)));
        }

        bool listsPath(const QStringList &paths, const QString &path) {
            return paths.contains(nativePath(path), Qt::CaseInsensitive);
        }
#endif

        void stopHard(QProcess *process) {
#ifdef Q_OS_MACOS
            // Never SIGKILL on macOS: pf keeps the rules until the guard removes them itself.
            process->terminate();
#else
            process->kill();
#endif
        }

        const char *stateName(KillSwitch::State state) {
            switch (state) {
                case KillSwitch::State::Disabled: return "disabled";
                case KillSwitch::State::Arming: return "arming";
                case KillSwitch::State::Armed: return "armed";
                case KillSwitch::State::Failed: return "failed";
            }
            return "unknown";
        }
    } // namespace

    struct GuardConfig {
        QByteArray json;
        QStringList extraPaths;
    };

    namespace {
        GuardConfig buildConfig() {
            const auto *settings = Configs::dataManager->settingsRepo.get();
            GuardConfig config;
#ifdef Q_OS_WIN
            for (const auto &path : settings->GetExtraCorePaths()) {
                const auto native = nativePath(path);
                if (!native.isEmpty() && !config.extraPaths.contains(native, Qt::CaseInsensitive)) config.extraPaths << native;
            }
#endif
            const QJsonObject object{
                {"allow_lan", !settings->disable_private_range_bypass},
                {"tun_name", tunName()},
                {"tun_cidrs", QJsonArray{settings->vpn_tun_ipv4_cidr, settings->vpn_tun_ipv6_cidr}},
                {"extra_paths", QJsonArray::fromStringList(config.extraPaths)},
                {"bridge_name", bridgeName()},
                {"api_port", int(g_killSwitchApiPort.load())},
            };
            config.json = QJsonDocument(object).toJson(QJsonDocument::Compact);
            return config;
        }
    } // namespace

    class GuardProcess : public QProcess {
    public:
        explicit GuardProcess(QObject *parent) : QProcess(parent), readyTimer(new QTimer(this)) {
            readyTimer->setSingleShot(true);
        }

        QByteArray config;
        // What the guard reads on stdin: config plus launch-only keys, never compared.
        QByteArray launch;
        QStringList extraPaths;
        QByteArray stderrTail;
        QTimer *readyTimer;
        bool ready = false;
        // Once set, the guard's exit is expected and never read as a failure.
        bool closing = false;
    };

    // Never destroyed: ~QProcess would SIGKILL a guard that still has to remove its own rules.
    KillSwitch *KillSwitch::instance() {
        static auto *self = [] {
            auto *created = new KillSwitch;
            g_killSwitchInstance.store(created);
            return created;
        }();
        return self;
    }

    bool KillSwitch::allowsStart() const {
        const auto current = state();
        return current == State::Disabled || current == State::Armed;
    }

    bool KillSwitch::failedForPrivileges() const {
        return state() == State::Failed && failureCode_ == kPrivilege;
    }

    bool KillSwitch::failedPersistently() const {
        if (state() != State::Failed) return false;
        return failureCode_ == kPrivilege || failureCode_ == kUnsupported || failureCode_ == kConfig || failureCode_ == kOverridden;
    }

    bool KillSwitch::failedForIdentity() const {
        return state() == State::Failed && failureCode_ == kUnsupported && identityUnsupported_ && (active_ != nullptr || pending_ != nullptr);
    }

    QString KillSwitch::failureText() const {
        if (failureCode_ == kOverridden) return failureMessage_;
        QString text;
        if (failureCode_ == kPrivilege) {
#ifdef Q_OS_WIN
            text = tr("The kill switch needs Throne to run as administrator.");
#else
            text = tr("The kill switch needs the core to have root privileges.");
#endif
        } else if (failureCode_ == kUnsupported) {
            return failureMessage_.isEmpty() ? tr("The kill switch is not supported on this system.") : failureMessage_;
        } else if (failureCode_ == kTampered) {
            text = tr("Another program removed the kill switch rules or disabled the system firewall.");
        } else if (failureCode_ == kConfig) {
            text = tr("The kill switch rejected its settings.");
        } else {
            text = tr("The kill switch process stopped unexpectedly.");
        }
        if (!failureMessage_.isEmpty()) text += "\n\n" + tr("Details: %1").arg(failureMessage_);
        return text;
    }

    void KillSwitch::noteIdentityUnsupported(const QString &detail) {
        identityDetail_ = detail;
        identityUnsupported_ = true;
        apply();
    }

    void KillSwitch::setOverride(bool overridden) {
        overridden_ = overridden;
    }

    void KillSwitch::SetApiPort(quint16 port) {
        if (g_killSwitchApiPort.exchange(port) == port) return;
        // Not built yet: its first apply() reads the new port.
        auto *self = g_killSwitchInstance.load();
        if (self == nullptr) return;
        // The state is read on the UI thread, so an apply() racing this change cannot keep the old port.
        QMetaObject::invokeMethod(self, [self] {
            const auto current = self->state();
            // A guard kept under the identity failure reports Failed but still needs the new port.
            if (current != State::Arming && current != State::Armed && self->active_ == nullptr && self->pending_ == nullptr) return;
            // A teardown stops the listener first; re-arming then would only respawn a guard that shutdown() closes next.
            if (Configs::dataManager->settingsRepo->prepare_exit) return;
            self->apply();
        }, Qt::QueuedConnection);
    }

    void KillSwitch::apply() {
        if (shutdown_) return;
        if (!Configs::dataManager->settingsRepo->kill_switch) {
            identityUnsupported_ = false;
            identityDetail_.clear();
            const bool wasOn = state() != State::Disabled;
            closeGuard(std::exchange(pending_, nullptr));
            closeGuard(std::exchange(active_, nullptr));
            if (wasOn) MW_show_log(tr("[Kill switch] Off."));
            setState(State::Disabled);
            settle();
            return;
        }
        if (overridden_) {
            closeGuard(std::exchange(pending_, nullptr));
            closeGuard(std::exchange(active_, nullptr));
            const auto message = QStringLiteral("turned off for this run by --no-kill-switch (THRONE_KILL_SWITCH); profiles stay refused "
                                                "until kill_switch is turned off");
            if (state() == State::Failed && failureCode_ == kOverridden && failureMessage_ == message) {
                settle();
                return;
            }
            fail(kOverridden, message);
            return;
        }
        if (identityUnsupported_ && state() == State::Armed) failIdentity();
        const auto config = buildConfig();
        if (pending_ != nullptr && pending_->config == config.json) return;
        if (active_ != nullptr && active_->config == config.json) {
            if (pending_ != nullptr) {
                closeGuard(std::exchange(pending_, nullptr));
                settle();
            }
            return;
        }
        spawn(config);
    }

    void KillSwitch::whenSettled(QObject *context, std::function<void(bool armed)> callback) {
        if (state() != State::Arming) {
            callback(state() == State::Armed);
            return;
        }
        waiters_.append({context, context != nullptr, false, std::move(callback)});
    }

    void KillSwitch::whenIdle(std::function<void()> callback) {
        if (pending_ == nullptr) {
            callback();
            return;
        }
        waiters_.append({this, true, true, [callback = std::move(callback)](bool) { callback(); }});
    }

    bool KillSwitch::permitExtraCore(const QString &path) {
#ifndef Q_OS_WIN
        Q_UNUSED(path)
        return true;
#else
        if (state() == State::Disabled) return true;
        auto promise = std::make_shared<std::promise<bool>>();
        auto future = promise->get_future();
        runOnUiThread([this, path, promise] {
            if (state() == State::Disabled || (active_ != nullptr && listsPath(active_->extraPaths, path))) {
                promise->set_value(true);
                return;
            }
            auto *settings = Configs::dataManager->settingsRepo.get();
            if (settings->AddExtraCorePath(path)) settings->Save();
            MW_show_log(tr("[Kill switch] Allowing the extra core %1.").arg(nativePath(path)));
            apply();
            whenIdle([this, path, promise] {
                promise->set_value(state() == State::Armed && active_ != nullptr && listsPath(active_->extraPaths, path));
            });
        });
        if (future.wait_for(std::chrono::milliseconds(kPermitTimeoutMs)) != std::future_status::ready) return false;
        try {
            return future.get();
        } catch (const std::future_error &) {
            return false;
        }
#endif
    }

    void KillSwitch::shutdown() {
        if (shutdown_) return;
        shutdown_ = true;
        waiters_.clear();
        active_ = nullptr;
        pending_ = nullptr;

        QList<GuardProcess *> guards;
        // GuardProcess has no metaobject of its own, and every QProcess child is one.
        for (auto *child : findChildren<QProcess *>(QString(), Qt::FindDirectChildrenOnly)) guards << static_cast<GuardProcess *>(child);
        for (auto *guard : guards) {
            guard->closing = true;
            guard->readyTimer->stop();
            if (guard->state() != QProcess::NotRunning) guard->closeWriteChannel();
        }
        const QDeadlineTimer deadline(kShutdownWaitMs);
        for (auto *guard : guards) {
            if (guard->state() != QProcess::NotRunning) guard->waitForFinished(int(qMax<qint64>(0, deadline.remainingTime())));
        }
        for (auto *guard : guards) {
            if (guard->state() == QProcess::NotRunning) continue;
            LOG_WARN("kill switch: a guard did not exit after its input closed, stopping it");
            stopHard(guard);
            guard->waitForFinished(1000);
        }
    }

    void KillSwitch::spawn(const GuardConfig &config) {
        closeGuard(std::exchange(pending_, nullptr));
        auto *guard = new GuardProcess(this);
        guard->config = config.json;
        guard->launch = config.json;
        if (active_ != nullptr) {
            // The READY guard's anchor stays loaded until this one is ready, so no state formed outside the block (macOS flush).
            auto object = QJsonDocument::fromJson(config.json).object();
            object.insert(QStringLiteral("keep_states"), true);
            guard->launch = QJsonDocument(object).toJson(QJsonDocument::Compact);
        }
        guard->extraPaths = config.extraPaths;
        pending_ = guard;

        connect(guard->readyTimer, &QTimer::timeout, this, [this, guard] { onReadyTimeout(guard); });
        // The write channel stays open for the guard's whole life: EOF is its signal to remove the rules.
        connect(guard, &QProcess::started, this, [guard] {
            if (!guard->closing) guard->write(guard->launch + '\n');
        });
        connect(guard, &QProcess::readyReadStandardOutput, this, [this, guard] { readOutput(guard); });
        connect(guard, &QProcess::readyReadStandardError, this, [this, guard] { readErrors(guard, false); });
        connect(guard, &QProcess::errorOccurred, this, [this, guard](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                onStartFailed(guard);
                return;
            }
            LOG_WARN(QString("kill switch: guard process error %1: %2").arg(int(error)).arg(guard->errorString()));
        });
        connect(guard, &QProcess::finished, this, [this, guard](int exitCode, QProcess::ExitStatus status) {
            onFinished(guard, exitCode, status == QProcess::CrashExit);
        });

        LOG_INFO("kill switch: starting a guard with " + QString::fromUtf8(guard->launch));
        // Before start(): Windows reports a failed start synchronously, and that must move Arming on to Failed.
        // Under an identity failure the state stays Failed (unsupported) whatever the guard does.
        if (active_ == nullptr && !identityUnsupported()) setState(State::Arming);
        guard->readyTimer->start(kReadyTimeoutMs);
        guard->setProcessEnvironment(Sys::ServiceManager::ChildEnvironment());
        Configs_sys::IsolateChild(guard);
        guard->start(Configs::FindCoreRealPath(), {QStringLiteral("--guard")});
    }

    void KillSwitch::closeGuard(GuardProcess *guard) {
        if (guard == nullptr || guard->closing) return;
        guard->closing = true;
        guard->readyTimer->stop();
        if (guard->state() == QProcess::NotRunning) {
            guard->deleteLater();
            return;
        }
        guard->closeWriteChannel();
        QTimer::singleShot(kCloseGraceMs, guard, [guard] {
            if (guard->state() == QProcess::NotRunning) return;
            LOG_WARN("kill switch: a closed guard is still running, stopping it");
            stopHard(guard);
        });
    }

    void KillSwitch::readOutput(GuardProcess *guard) {
        while (guard->canReadLine()) {
            const auto line = QString::fromUtf8(guard->readLine()).trimmed();
            if (!line.isEmpty()) handleLine(guard, line);
        }
    }

    void KillSwitch::readErrors(GuardProcess *guard, bool flush) {
        guard->stderrTail += guard->readAllStandardError();
        for (auto end = guard->stderrTail.indexOf('\n'); end >= 0; end = guard->stderrTail.indexOf('\n')) {
            const auto line = QString::fromUtf8(guard->stderrTail.left(end)).trimmed();
            guard->stderrTail.remove(0, end + 1);
            if (!line.isEmpty()) MW_show_log("[Kill switch] " + line);
        }
        if (flush && !guard->stderrTail.trimmed().isEmpty()) {
            MW_show_log("[Kill switch] " + QString::fromUtf8(guard->stderrTail).trimmed());
            guard->stderrTail.clear();
        }
    }

    void KillSwitch::handleLine(GuardProcess *guard, const QString &line) {
        const auto verb = line.section(' ', 0, 0);
        const auto rest = line.section(' ', 1);
        if (verb == QLatin1String("WARN")) {
            LOG_WARN("kill switch: " + rest);
            MW_show_log(tr("[Kill switch] Warning: %1").arg(rest));
            return;
        }
        if (verb == QLatin1String("READY")) {
            onReady(guard);
            return;
        }
        if (verb != QLatin1String("ERROR") && verb != QLatin1String("FAIL")) {
            MW_show_log("[Kill switch] " + line);
            return;
        }
        if (guard->closing) return;
        const auto code = rest.section(' ', 0, 0);
        const auto message = rest.section(' ', 1);
        const bool wasActive = guard == active_;
        const bool wasPending = guard == pending_;
        if (wasActive) active_ = nullptr;
        if (wasPending) pending_ = nullptr;
        closeGuard(guard);
        if (wasActive) protectionLost(code, message);
        else if (wasPending) armFailed(code, message);
    }

    void KillSwitch::onReady(GuardProcess *guard) {
        if (guard != pending_ || guard->ready) return;
        guard->ready = true;
        guard->readyTimer->stop();
        auto *previous = std::exchange(active_, guard);
        pending_ = nullptr;
        closeGuard(previous);
        LOG_INFO("kill switch: armed with " + QString::fromUtf8(guard->config));
        MW_show_log(previous == nullptr ? tr("[Kill switch] On: only traffic through Throne can leave this device.")
                                        : tr("[Kill switch] New settings applied."));
        if (identityUnsupported_) {
            failIdentity();
            return;
        }
        setState(State::Armed);
        settle();
    }

    void KillSwitch::onReadyTimeout(GuardProcess *guard) {
        if (guard != pending_ || guard->ready) return;
        pending_ = nullptr;
        closeGuard(guard);
        stopHard(guard);
        armFailed(kTimeout, tr("the kill switch did not respond within %1 seconds").arg(kReadyTimeoutMs / 1000));
    }

    void KillSwitch::onStartFailed(GuardProcess *guard) {
        guard->readyTimer->stop();
        const auto message = tr("could not start %1: %2").arg(guard->program(), guard->errorString());
        LOG_ERROR("kill switch: " + message);
        const bool wasPending = !guard->closing && guard == pending_;
        if (wasPending) pending_ = nullptr;
        guard->closing = true;
        guard->deleteLater();
        if (wasPending) armFailed(kInternal, message);
    }

    void KillSwitch::onFinished(GuardProcess *guard, int exitCode, bool crashed) {
        readOutput(guard);
        if (const auto last = QString::fromUtf8(guard->readAllStandardOutput()).trimmed(); !last.isEmpty()) {
            handleLine(guard, last);
        }
        readErrors(guard, true);
        guard->readyTimer->stop();

        Logging::Write(guard->closing ? Logging::Level::Info : Logging::Level::Error,
                       QString("kill switch: guard %1 (code %2)").arg(crashed ? "crashed" : "exited").arg(exitCode));
        const bool wasActive = !guard->closing && guard == active_;
        const bool wasPending = !guard->closing && guard == pending_;
        if (wasActive) active_ = nullptr;
        if (wasPending) pending_ = nullptr;
        guard->closing = true;
        guard->deleteLater();
        if (wasActive) {
            protectionLost(kCrash, crashed ? tr("the kill switch process crashed")
                                           : tr("the kill switch process exited with code %1").arg(exitCode));
        } else if (wasPending) {
            armFailed(kCrash, crashed ? tr("the kill switch process crashed before it was ready")
                                      : tr("the kill switch process exited with code %1 before it was ready").arg(exitCode));
        }
    }

    void KillSwitch::armFailed(const QString &code, const QString &message) {
        LOG_ERROR(QString("kill switch: arming failed (%1): %2").arg(code, message));
        if (active_ != nullptr) {
            MW_show_log(tr("[Kill switch] The new settings could not be applied, the previous ones stay in force: %1").arg(message));
            if (code != kPrivilege && code != kUnsupported && code != kConfig) {
                // apply() is a no-op once the kept guard's config matches again (e.g. the API port moved back).
                QTimer::singleShot(kRespawnIntervalMs, this, [this] {
                    if (shutdown_ || pending_ != nullptr || active_ == nullptr) return;
                    if (Configs::dataManager->settingsRepo->prepare_exit) return;
                    apply();
                });
            }
            settle();
            return;
        }
        fail(code, message);
    }

    void KillSwitch::protectionLost(const QString &code, const QString &message) {
        LOG_ERROR(QString("kill switch: protection lost (%1): %2").arg(code, message));
        MW_show_log(tr("[Kill switch] Protection lost: %1").arg(message));
        if (shutdown_) return;
        if (!Configs::dataManager->settingsRepo->kill_switch) {
            apply();
            return;
        }
        if (pending_ != nullptr) {
            if (!identityUnsupported()) setState(State::Arming);
            settle();
            return;
        }
        const bool recoverable = code != kPrivilege && code != kUnsupported && code != kConfig;
        if (recoverable && (!lastRespawn_.isValid() || lastRespawn_.hasExpired(kRespawnIntervalMs))) {
            lastRespawn_.start();
            MW_show_log(tr("[Kill switch] Starting it again..."));
            spawn(buildConfig());
            return;
        }
        fail(code, message);
    }

    void KillSwitch::fail(const QString &code, const QString &message) {
        failureCode_ = code;
        failureMessage_ = message;
        MW_show_log(tr("[Kill switch] Not active: %1").arg(message.isEmpty() ? code : message));
        setState(State::Failed);
        settle();
    }

    void KillSwitch::failIdentity() {
        const auto message = killSwitchIdentityMessage(identityDetail_);
        if (state() != State::Failed || failureCode_ != kUnsupported || failureMessage_ != message) {
            failureCode_ = kUnsupported;
            failureMessage_ = message;
            // Not fail(): its "Not active" would be wrong while a guard still blocks.
            MW_show_log(QStringLiteral("[Kill switch] Profiles refused (%1): %2").arg(kUnsupported, message));
            setState(State::Failed);
        }
        settle();
    }

    void KillSwitch::setState(State state) {
        if (state != State::Failed) {
            failureCode_.clear();
            failureMessage_.clear();
        }
        if (state_.exchange(state) == state) return;
        LOG_INFO(QString("kill switch: %1").arg(stateName(state)));
        emit stateChanged();
    }

    void KillSwitch::settle() {
        const auto waiting = std::exchange(waiters_, {});
        for (const auto &waiter : waiting) {
            if (waiter.idle ? pending_ != nullptr : state() == State::Arming) {
                waiters_.append(waiter);
                continue;
            }
            if (waiter.bound && waiter.context.isNull()) continue;
            waiter.callback(state() == State::Armed);
        }
    }
} // namespace Sys
