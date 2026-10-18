#pragma once

#include <QJsonObject>
#include <QString>

namespace Engine {
    // Exclusive OS lock on <configDir>/.lock, released by the OS when the process dies; the file itself is never removed.
    class DataDirLock {
    public:
        enum class Result {
            Acquired,
            Busy,
            // The lock file could not be opened or locked for another reason; see errorString().
            Failed,
        };

        DataDirLock() = default;
        ~DataDirLock();

        DataDirLock(const DataDirLock &) = delete;
        DataDirLock &operator=(const DataDirLock &) = delete;

        // On success the file holds {"mode", "pid", "started"}.
        Result tryAcquire(const QString &configDir, const QString &mode);

        void release();

        [[nodiscard]] bool isHeld() const;

        [[nodiscard]] QString errorString() const { return error; }

        // Readable while another process holds the lock; empty when there is no owner record.
        static QJsonObject ReadOwner(const QString &configDir);

        // "<mode> (PID n, since <local time>)".
        static QString DescribeOwner(const QJsonObject &owner);

        static QString LockPath(const QString &configDir);

    private:
#ifdef Q_OS_WIN
        void *handle = nullptr;
#else
        int fd = -1;
#endif
        QString error;
    };
} // namespace Engine
