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

        CoreProcess(const QString &core_path, const QStringList &args);

        void Restart();

        int start_profile_when_core_is_up = -1;

    private:
        bool show_stderr = false;
        bool failed_to_start = false;
        bool restarting = false;

        QElapsedTimer coreRestartTimer;

    protected:
        bool started = false;
        bool killed = false;
        bool crashed = false;
    };

    inline QAtomicInt logCounter;
} // namespace Configs_sys
