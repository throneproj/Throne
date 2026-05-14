#pragma once

#include <memory>
#include <QElapsedTimer>
#include <QProcess>

namespace Configs_sys {
    class CoreProcess : public QProcess
    {
    public:
        QString tag;
        QString program;
        QStringList arguments;

        ~CoreProcess();

        // start & kill is one time

        void Start();

        void Kill();

        CoreProcess(const QString &core_path, const QString &socketName, bool debugMode);

        void Restart();

        void SetUsePkexec(bool enable);

        bool IsUsingPkexec() const;

        void MarkCoreReportedStarted();

        int start_profile_when_core_is_up = -1;

    private:
        QString m_socketName;
        bool m_debugMode = false;
        bool show_stderr = false;
        bool failed_to_start = false;
        bool restarting = false;
        bool use_pkexec = false;
        bool last_start_used_pkexec = false;
        bool core_reported_started = false;
        bool root_start_failed_reported = false;

        QElapsedTimer coreRestartTimer;

    protected:
        bool started = false;
        bool crashed = false;
    };

    inline QAtomicInt logCounter;
} // namespace Configs_sys
