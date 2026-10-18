#include "include/sys/HostPresence.hpp"

#include "include/global/Logger.hpp"

#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_UNIX
#include "NkrVersion.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Sys::HostPresence {
#ifdef Q_OS_UNIX
    namespace {
        constexpr qint64 kHostPresenceMaxRecord = 4096;
        // The first try plus 3 retries: a GUI probe holds LOCK_SH only for a moment.
        constexpr int kHostPresenceLockAttempts = 4;
        constexpr unsigned long kHostPresenceRetryMs = 50;

        // Never closed: the flock is the marker, and CLOEXEC releases it on an exec restart.
        int g_hostPresenceFd = -1;

        QString hostPresenceRootDir() {
#ifdef Q_OS_LINUX
            return QStringLiteral("/run/throne");
#else
            return QStringLiteral("/var/run/throne");
#endif
        }

        QString hostPresenceUserDir() {
            const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
            if (runtime.isEmpty() || !QDir::isAbsolutePath(runtime)) return {};
            return QDir(runtime).absoluteFilePath(QStringLiteral("throne"));
        }

        QString hostPresenceFile(const QString &dir) {
            return QDir(dir).absoluteFilePath(QStringLiteral("headless.lock"));
        }

        QString hostPresenceErrno(int code) {
            return QString::fromLocal8Bit(std::strerror(code));
        }

        QByteArray hostPresenceRead(int fd) {
            QByteArray data(kHostPresenceMaxRecord, Qt::Uninitialized);
            qint64 total = 0;
            while (total < data.size()) {
                const auto n = ::pread(fd, data.data() + total, static_cast<size_t>(data.size() - total), static_cast<off_t>(total));
                if (n < 0 && errno == EINTR) continue;
                if (n <= 0) break;
                total += n;
            }
            data.truncate(static_cast<qsizetype>(total));
            return data;
        }

        // A record still being written parses as a holder with pid 0: the lock alone proves a live headless.
        Holder hostPresenceParse(const QByteArray &data) {
            Holder holder;
            const auto doc = QJsonDocument::fromJson(data.trimmed());
            if (!doc.isObject()) return holder;
            const auto object = doc.object();
            holder.pid = object.value(QStringLiteral("pid")).toInteger();
            holder.installDir = object.value(QStringLiteral("install_dir")).toString();
            holder.dataDir = object.value(QStringLiteral("data_dir")).toString();
            holder.version = object.value(QStringLiteral("version")).toString();
            holder.started = object.value(QStringLiteral("started")).toInteger();
            return holder;
        }

        std::optional<Holder> hostPresenceProbe(const QString &dir) {
            const QByteArray path = QFile::encodeName(hostPresenceFile(dir));
            const int fd = ::open(path.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
            if (fd < 0) return std::nullopt;
            struct stat st{};
            if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
                ::close(fd);
                return std::nullopt;
            }
            int rc = 0;
            do {
                rc = ::flock(fd, LOCK_SH | LOCK_NB);
            } while (rc != 0 && errno == EINTR);
            if (rc == 0) {
                // Nobody holds it: the marker of a headless that has exited.
                ::flock(fd, LOCK_UN);
                ::close(fd);
                return std::nullopt;
            }
            if (errno != EWOULDBLOCK) {
                ::close(fd);
                return std::nullopt;
            }
            const auto holder = hostPresenceParse(hostPresenceRead(fd));
            ::close(fd);
            // Our own published marker conflicts with a second open file description too.
            if (holder.pid == QCoreApplication::applicationPid()) return std::nullopt;
            return holder;
        }

        // Root only: an older build made the root dir 0700, which hides the marker from a GUI that runs as another user.
        void hostPresenceWidenRootDir(const QString &dir, const QByteArray &native) {
            const int fd = ::open(native.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (fd < 0) return;
            struct stat st{};
            if (::fstat(fd, &st) == 0 && S_ISDIR(st.st_mode) && st.st_uid == 0 && (st.st_mode & 07777) != 0755) {
                const QString old = QStringLiteral("%1").arg(uint(st.st_mode & 07777), 4, 8, QLatin1Char('0'));
                if (::fchmod(fd, 0755) == 0) {
                    LOG_INFO(QStringLiteral("host presence: changed the mode of %1 from %2 to 0755").arg(dir, old));
                } else {
                    const int code = errno;
                    LOG_WARN(QStringLiteral("host presence: could not change the mode of %1 from %2 to 0755: %3; a GUI running as "
                                            "another user may not see this instance")
                                 .arg(dir, old, hostPresenceErrno(code)));
                }
            }
            ::close(fd);
        }

        bool hostPresenceEnsureDir(const QString &dir) {
            const QByteArray native = QFile::encodeName(dir);
            if (::mkdir(native.constData(), 0755) == 0) {
                // The umask must not hide the marker from a GUI that runs as another user.
                ::chmod(native.constData(), 0755);
                return true;
            }
            const int code = errno;
            if (code == EEXIST && QFileInfo(dir).isDir()) {
                if (::geteuid() == 0) hostPresenceWidenRootDir(dir, native);
                return true;
            }
            LOG_WARN(QStringLiteral("host presence: could not create %1: %2; a GUI on this computer cannot see this instance")
                         .arg(dir, code == EEXIST ? QStringLiteral("it is not a directory") : hostPresenceErrno(code)));
            return false;
        }

        bool hostPresenceWrite(int fd, const QByteArray &record) {
            if (::ftruncate(fd, 0) != 0) return false;
            qint64 offset = 0;
            while (offset < record.size()) {
                const auto n = ::pwrite(fd, record.constData() + offset, static_cast<size_t>(record.size() - offset), static_cast<off_t>(offset));
                if (n < 0 && errno == EINTR) continue;
                if (n <= 0) return false;
                offset += n;
            }
            return true;
        }
    } // namespace

    bool Publish(const QString &dataDir) {
        if (g_hostPresenceFd >= 0) return true;
        const QString dir = ::geteuid() == 0 ? hostPresenceRootDir() : hostPresenceUserDir();
        if (dir.isEmpty()) {
            LOG_WARN("host presence: not published (not root and XDG_RUNTIME_DIR is not set); a GUI on this computer cannot see this instance");
            return false;
        }
        if (!hostPresenceEnsureDir(dir)) return false;

        const QString path = hostPresenceFile(dir);
        const QByteArray native = QFile::encodeName(path);
        const int fd = ::open(native.constData(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0644);
        if (fd < 0) {
            const int code = errno;
            LOG_WARN(QStringLiteral("host presence: could not open %1: %2")
                         .arg(path, code == ELOOP ? QStringLiteral("it is a symbolic link") : hostPresenceErrno(code)));
            return false;
        }
        struct stat st{};
        if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1) {
            ::close(fd);
            LOG_WARN(QStringLiteral("host presence: could not open %1: it is not a regular file with a single link").arg(path));
            return false;
        }

        int code = 0;
        for (int attempt = 0; attempt < kHostPresenceLockAttempts; ++attempt) {
            if (attempt > 0) QThread::msleep(kHostPresenceRetryMs);
            if (::flock(fd, LOCK_EX | LOCK_NB) == 0) {
                code = 0;
                break;
            }
            code = errno;
            if (code != EWOULDBLOCK && code != EINTR) break;
        }
        if (code != 0) {
            if (code == EWOULDBLOCK) {
                const auto holder = hostPresenceParse(hostPresenceRead(fd));
                const QString who = holder.pid > 0 ? QStringLiteral("PID %1, data dir %2").arg(holder.pid).arg(holder.dataDir)
                                                   : QStringLiteral("its PID is unknown");
                LOG_WARN(QStringLiteral("host presence: another headless Throne (%1) already publishes %2; a GUI on this computer sees "
                                        "that one, not this one")
                             .arg(who, path));
            } else {
                LOG_WARN(QStringLiteral("host presence: could not lock %1: %2").arg(path, hostPresenceErrno(code)));
            }
            ::close(fd);
            return false;
        }

        if (st.st_uid == ::geteuid() && (st.st_mode & 07777) != 0644) ::fchmod(fd, 0644);
        const QByteArray record = QJsonDocument(QJsonObject{
                                                    {"pid", QCoreApplication::applicationPid()},
                                                    {"install_dir", QCoreApplication::applicationDirPath()},
                                                    {"data_dir", dataDir},
                                                    {"version", QStringLiteral(NKR_VERSION)},
                                                    {"started", QDateTime::currentSecsSinceEpoch()},
                                                })
                                      .toJson(QJsonDocument::Compact);
        if (!hostPresenceWrite(fd, record)) {
            // The lock alone still marks this instance; readers then report an unknown PID.
            LOG_WARN(QStringLiteral("host presence: could not write %1: %2").arg(path, hostPresenceErrno(errno)));
        }
        g_hostPresenceFd = fd;
        LOG_INFO(QStringLiteral("host presence: published in %1").arg(path));
        return true;
    }

    std::optional<Holder> OtherHeadless() {
        const QString root = hostPresenceRootDir();
        if (auto holder = hostPresenceProbe(root)) return holder;
        const QString user = hostPresenceUserDir();
        if (user.isEmpty() || QDir::cleanPath(user) == QDir::cleanPath(root)) return std::nullopt;
        return hostPresenceProbe(user);
    }
#elif !defined(Q_OS_WIN) // Windows: src/sys/windows/HostPresence_win.cpp
    bool Publish(const QString &dataDir) {
        Q_UNUSED(dataDir);
        LOG_INFO("host presence: not published (not supported on this OS yet)");
        return false;
    }

    std::optional<Holder> OtherHeadless() {
        return std::nullopt;
    }
#endif

    bool SameInstall(const Holder &holder, const QString &installDir) {
        if (holder.installDir.isEmpty() || installDir.isEmpty()) return false;
#ifdef Q_OS_WIN
        constexpr auto sensitivity = Qt::CaseInsensitive;
#else
        constexpr auto sensitivity = Qt::CaseSensitive;
#endif
        if (QDir::cleanPath(holder.installDir).compare(QDir::cleanPath(installDir), sensitivity) == 0) return true;
        const QString held = QFileInfo(holder.installDir).canonicalFilePath();
        return !held.isEmpty() && held.compare(QFileInfo(installDir).canonicalFilePath(), sensitivity) == 0;
    }
} // namespace Sys::HostPresence
