#include "include/sys/Process.hpp"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#include "include/sys/CoreElevation.hpp"
#include "include/sys/KillSwitch.hpp"
#include "include/sys/ServiceManager.hpp"

#include <QElapsedTimer>

#include <algorithm>

#ifdef Q_OS_UNIX
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Configs_sys {
    namespace {
        std::atomic<bool> g_processIsolateChildren{false};

        constexpr int kProcessKillElevatedWaitMs = 10000;
        constexpr int kProcessRestartElevatedWaitMs = 5000;

#ifdef Q_OS_WIN
        // CREATE_NEW_PROCESS_GROUP and CREATE_NO_WINDOW from <winbase.h>, which this portable TU does not include.
        constexpr unsigned long kProcessNewProcessGroup = 0x00000200;
        constexpr unsigned long kProcessNoWindow = 0x08000000;
#endif
    }

    void SetIsolatedChildren(bool enabled) { g_processIsolateChildren.store(enabled); }

    void IsolateChild(QProcess *process) {
#ifdef Q_OS_UNIX
        // Runs in the child between fork and exec: async-signal-safe calls only.
        if (g_processIsolateChildren.load()) process->setChildProcessModifier([] { ::setpgid(0, 0); });
#elif defined(Q_OS_WIN)
        // Qt adds CREATE_NO_WINDOW only while we have no console: after AttachConsole a Ctrl+C or a console close would
        // reach the core and the guard (which disarms) before our ordered teardown.
        if (g_processIsolateChildren.load()) {
            process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
                args->flags |= kProcessNewProcessGroup | kProcessNoWindow;
            });
        }
#else
        Q_UNUSED(process);
#endif
    }

    void KillChildGroup(QProcess *process) {
#ifdef Q_OS_UNIX
        const auto pid = static_cast<pid_t>(process->processId());
        // A group id is not reused while any member lives; ESRCH (setpgid not run yet, or all gone) is harmless.
        if (g_processIsolateChildren.load() && pid > 0) ::kill(-pid, SIGKILL);
#else
        Q_UNUSED(process);
#endif
    }

    CoreProcess::~CoreProcess() {
    }

    void CoreProcess::Kill(int waitMs) {
        QElapsedTimer elapsed;
        elapsed.start();
        KillChildGroup(this);
        kill();
        waitForFinished(waitMs);
        // The stub's job takes the elevated core down with it, but not synchronously.
        waitElevated(waitMs < 0 ? kProcessKillElevatedWaitMs
                                : int(std::clamp<qint64>(waitMs - elapsed.elapsed(), 0, kProcessKillElevatedWaitMs)));
    }

    void CoreProcess::waitElevated(int waitMs) const {
        std::shared_ptr<void> watch;
        qint64 pid = 0;
        {
            std::lock_guard lock(m_elevatedMutex);
            watch = m_elevatedWatch;
            pid = m_elevatedPid;
        }
        if (!watch || Sys::CoreElevation::WaitWatch(watch.get(), waitMs)) return;
        LOG_WARN(QString("the elevated core (PID %1) is still running %2 ms after its stub was killed").arg(pid).arg(waitMs));
    }

    bool CoreProcess::AdoptElevatedPid(qint64 pid, quint64 forGeneration) {
        if (pid <= 0 || pid == processId()) return false;
        std::lock_guard lock(m_elevatedMutex);
        if (generation.load() != forGeneration || pid == m_elevatedPid) return false;
        void *watch = Sys::CoreElevation::OpenWatch(pid);
        if (watch == nullptr) return false;
        m_elevatedWatch = std::shared_ptr<void>(watch, [](void *opened) { Sys::CoreElevation::CloseWatch(opened); });
        m_elevatedPid = pid;
        return true;
    }

    qint64 CoreProcess::EffectivePid() const {
        {
            std::lock_guard lock(m_elevatedMutex);
            if (m_elevatedWatch && !Sys::CoreElevation::WaitWatch(m_elevatedWatch.get(), 0)) return m_elevatedPid;
        }
        return processId();
    }

    CoreProcess::CoreProcess(const QString &core_path, const QString &socketName, bool debugMode)
        : m_socketName(socketName), m_debugMode(debugMode) {
        program = core_path;
    }

    void CoreProcess::Start() {
        if (started) return;
        started = true;
        {
            // With the bump, so an adopt for the previous generation cannot land after the release.
            std::lock_guard lock(m_elevatedMutex);
            ++generation;
            m_elevatedWatch.reset();
            m_elevatedPid = 0;
        }
        failed_to_start = false;

        auto env = Sys::ServiceManager::ChildEnvironment();
        Sys::CoreElevation::ApplyEnvironment(env);
        through_grant = env.contains(QString::fromLatin1(Sys::CoreElevation::kTaskEnv));
        env.insert("THRONE_CORE_SOCKET", m_socketName);
        // Turns an unrecovered Go panic into a real abort, so all goroutine stacks are dumped and WER captures the core too.
        env.insert("GOTRACEBACK", "crash");
        if (m_debugMode) env.insert("THRONE_CORE_DEBUG", "1");
        // Points Xray's asset loader at our writable config dir, so a geoip.dat/geosite.dat downloaded later is found with no core restart.
        env.insert("XRAY_LOCATION_ASSET", Configs::GetBasePath());
        // Never inherited: only guard_identity decides, or an identity exit would repeat with the flag already handled.
        env.remove("THRONE_GUARD");
        guard_identity = Configs::dataManager->settingsRepo->kill_switch && !Sys::KillSwitch::instance()->identityUnsupported();
        if (guard_identity) env.insert("THRONE_GUARD", "1");
        setProcessEnvironment(env);
        IsolateChild(this);
        start(program, {});
    }

    void CoreProcess::Restart() {
        restarting = true;
        KillChildGroup(this);
        kill();
        waitForFinished(500);
        waitElevated(kProcessRestartElevatedWaitMs);
        started = false;
        Start();
        restarting = false;
    }

} // namespace Configs_sys
