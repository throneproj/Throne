#include "include/sys/ServiceManager.hpp"

#include "include/global/Logger.hpp"

#include <QByteArray>
#include <QStringList>

#ifdef Q_OS_LINUX
#include <QSet>
#include <QThread>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace Sys::ServiceManager {
    namespace {
        constexpr const char *kServiceManagerManagerVars[] = {
            "NOTIFY_SOCKET", "WATCHDOG_USEC", "WATCHDOG_PID", "LISTEN_PID", "LISTEN_FDS", "LISTEN_FDNAMES", "CREDENTIALS_DIRECTORY",
        };

        // The only THRONE_* variables the core reads (core/main.go, core/internal/guard).
        constexpr const char *kServiceManagerCoreVars[] = {"THRONE_CORE_SOCKET", "THRONE_CORE_DEBUG", "THRONE_GUARD"};

#ifdef Q_OS_WIN
        constexpr Qt::CaseSensitivity kServiceManagerEnvCase = Qt::CaseInsensitive;
#else
        constexpr Qt::CaseSensitivity kServiceManagerEnvCase = Qt::CaseSensitive;
#endif

        bool serviceManagerCoreVar(const QString &name) {
            for (const char *kept : kServiceManagerCoreVars) {
                if (name.compare(QLatin1String(kept), kServiceManagerEnvCase) == 0) return true;
            }
            return false;
        }
    } // namespace

    QProcessEnvironment ChildEnvironment(QProcessEnvironment env) {
        for (const char *name : kServiceManagerManagerVars) env.remove(QString::fromLatin1(name));
        for (const auto &name : env.keys()) {
            if (name.startsWith(QLatin1String("THRONE_"), kServiceManagerEnvCase) && !serviceManagerCoreVar(name)) env.remove(name);
        }
        return env;
    }

#ifdef Q_OS_LINUX
    namespace {
        constexpr int kServiceManagerRetries = 3;
        constexpr unsigned long kServiceManagerRetryMs = 10;

        struct ServiceManagerSocket {
            bool resolved = false;
            int fd = -1;
            sockaddr_un address{};
            socklen_t length = 0;
        };

        void serviceManagerLogOnce(const QString &kind, const QString &message) {
            static QSet<QString> logged;
            if (logged.contains(kind)) return;
            logged.insert(kind);
            LOG_WARN("sd_notify: " + message);
        }

        QString serviceManagerErrno(int code) {
            return QString::fromLocal8Bit(std::strerror(code));
        }

        ServiceManagerSocket &serviceManagerSocket() {
            static ServiceManagerSocket notify;
            if (notify.resolved) return notify;
            notify.resolved = true;
            const QByteArray target = qgetenv("NOTIFY_SOCKET");
            if (target.isEmpty()) return notify;
            const auto capacity = sizeof(notify.address.sun_path);
            const auto size = static_cast<size_t>(target.size());
            notify.address.sun_family = AF_UNIX;
            if (target.startsWith('/')) {
                if (size >= capacity) {
                    serviceManagerLogOnce(QStringLiteral("address"), QStringLiteral("NOTIFY_SOCKET is too long; readiness is not reported"));
                    return notify;
                }
                std::memcpy(notify.address.sun_path, target.constData(), size);
                notify.length = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + size + 1);
            } else if (target.startsWith('@')) {
                if (size > capacity) {
                    serviceManagerLogOnce(QStringLiteral("address"), QStringLiteral("NOTIFY_SOCKET is too long; readiness is not reported"));
                    return notify;
                }
                notify.address.sun_path[0] = '\0';
                std::memcpy(notify.address.sun_path + 1, target.constData() + 1, size - 1);
                // An abstract name is length-delimited: counting a trailing NUL would address another socket.
                notify.length = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + size);
            } else {
                serviceManagerLogOnce(QStringLiteral("address"),
                                      QStringLiteral("NOTIFY_SOCKET=%1 is not supported (only /path and @abstract); readiness is not reported")
                                          .arg(QString::fromLocal8Bit(target)));
                return notify;
            }
            const int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
            if (fd < 0) {
                serviceManagerLogOnce(QStringLiteral("socket"), QStringLiteral("could not create the notify socket: %1").arg(serviceManagerErrno(errno)));
                notify.length = 0;
                return notify;
            }
            notify.fd = fd;
            return notify;
        }

        // retryBusy: READY, STOPPING and RELOADING must arrive, so a full socket buffer is retried briefly; a dropped WATCHDOG is
        // resent next tick.
        void serviceManagerSend(const QString &message, bool retryBusy) {
            const auto &notify = serviceManagerSocket();
            if (notify.fd < 0) return;
            const QByteArray data = message.toUtf8();
            for (int attempt = 0;; ++attempt) {
                const auto sent = ::sendto(notify.fd, data.constData(), static_cast<size_t>(data.size()), MSG_NOSIGNAL | MSG_DONTWAIT,
                                           reinterpret_cast<const sockaddr *>(&notify.address), notify.length);
                if (sent >= 0) return;
                const int code = errno;
                if (attempt < kServiceManagerRetries) {
                    if (code == EINTR) continue;
                    if (code == EAGAIN && retryBusy) {
                        QThread::msleep(kServiceManagerRetryMs);
                        continue;
                    }
                }
                const auto kind = QString::fromUtf8(data.left(data.indexOf('=')));
                serviceManagerLogOnce(QStringLiteral("send %1 %2").arg(kind).arg(code),
                                      QStringLiteral("could not send %1: %2").arg(kind, serviceManagerErrno(code)));
                return;
            }
        }

        QString serviceManagerOneLine(QString text) {
            text.replace(QLatin1Char('\r'), QLatin1Char(' '));
            text.replace(QLatin1Char('\n'), QLatin1Char(' '));
            return text;
        }

        // systemd compares MONOTONIC_USEC with its own CLOCK_MONOTONIC.
        qint64 serviceManagerMonotonicUsec() {
            timespec now{};
            if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
            return static_cast<qint64>(now.tv_sec) * 1000000 + static_cast<qint64>(now.tv_nsec) / 1000;
        }
    } // namespace

    bool NotifyEnabled() {
        return serviceManagerSocket().fd >= 0;
    }

    void NotifyReady(const QString &status) {
        serviceManagerSend(QStringLiteral("READY=1\nSTATUS=") + serviceManagerOneLine(status), true);
    }

    void NotifyStatus(const QString &status) {
        serviceManagerSend(QStringLiteral("STATUS=") + serviceManagerOneLine(status), false);
    }

    void NotifyStopping(const QString &status) {
        serviceManagerSend(QStringLiteral("STOPPING=1\nSTATUS=") + serviceManagerOneLine(status), true);
    }

    void NotifyReloading(const QString &status) {
        QString message = QStringLiteral("RELOADING=1\n");
        if (const qint64 usec = serviceManagerMonotonicUsec(); usec > 0) message += QStringLiteral("MONOTONIC_USEC=%1\n").arg(usec);
        serviceManagerSend(message + QStringLiteral("STATUS=") + serviceManagerOneLine(status), true);
    }

    void NotifyWatchdog() {
        serviceManagerSend(QStringLiteral("WATCHDOG=1"), false);
    }

    void NotifyExtendTimeout(qint64 usec) {
        if (usec <= 0) return;
        serviceManagerSend(QStringLiteral("EXTEND_TIMEOUT_USEC=%1").arg(usec), false);
    }

    qint64 WatchdogUsec() {
        bool ok = false;
        const qint64 usec = qEnvironmentVariable("WATCHDOG_USEC").trimmed().toLongLong(&ok);
        if (!ok || usec <= 0) return 0;
        const QString owner = qEnvironmentVariable("WATCHDOG_PID").trimmed();
        if (owner.isEmpty()) return usec;
        bool pidOk = false;
        const qint64 pid = owner.toLongLong(&pidOk);
        if (pidOk && pid == static_cast<qint64>(::getpid())) return usec;
        // Leaked from a parent unit: pinging for another process would only confuse its manager.
        serviceManagerLogOnce(QStringLiteral("watchdog pid"),
                              QStringLiteral("WATCHDOG_PID=%1 is not this process; the systemd watchdog is not fed").arg(owner));
        return 0;
    }
#elif !defined(Q_OS_WIN) // Windows: src/sys/windows/ServiceManager_win.cpp
    bool NotifyEnabled() {
        return false;
    }

    void NotifyReady(const QString &status) {
        Q_UNUSED(status);
    }

    void NotifyStatus(const QString &status) {
        Q_UNUSED(status);
    }

    void NotifyStopping(const QString &status) {
        Q_UNUSED(status);
    }

    void NotifyReloading(const QString &status) {
        Q_UNUSED(status);
    }

    void NotifyWatchdog() {}

    void NotifyExtendTimeout(qint64 usec) {
        Q_UNUSED(usec);
    }

    qint64 WatchdogUsec() {
        return 0;
    }
#endif

#ifndef Q_OS_WIN
    void NotifyStartProgress() {}

    void NotifyStopped(int exitCode) {
        Q_UNUSED(exitCode);
    }

    bool RunningAsLocalSystem() {
        return false;
    }

    QString SystemDataDir(bool service) {
        Q_UNUSED(service);
        return {};
    }

    bool SecureDataDir(const QString &path, bool created, QString *error) {
        Q_UNUSED(path);
        Q_UNUSED(created);
        Q_UNUSED(error);
        return true;
    }

    QString UsersCanWriteDataDir(const QString &path) {
        Q_UNUSED(path);
        return {};
    }

    bool LocalClientIsAdmin(qintptr descriptor) {
        Q_UNUSED(descriptor);
        return true;
    }

    bool PipeServerTrusted(qintptr descriptor, QString *why) {
        Q_UNUSED(descriptor);
        Q_UNUSED(why);
        return true;
    }
#endif
} // namespace Sys::ServiceManager
