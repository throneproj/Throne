#include "include/engine/headless/HeadlessInstance.h"

#include <cstdio>
#include <functional>
#include <utility>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#ifndef Q_OS_WIN
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "NkrVersion.h"
#include "include/engine/app/ExitCodes.h"
#include "include/engine/app/InstanceServer.h"
#include "include/global/Logger.hpp"
#include "include/sys/ServiceManager.hpp"

namespace Engine::HeadlessInstance {
    namespace {
        constexpr int kHeadlessInstanceIdleMs = 5000;
        constexpr int kHeadlessInstanceClientMs = 3000;
        constexpr qsizetype kHeadlessInstanceMaxReply = 1 << 20;
        constexpr char kHeadlessInstanceAnswered[] = "throneHeadlessAnswered";
        constexpr char kHeadlessInstanceForbidden[] = "ERR forbidden";

        QString headlessInstanceHash(const QDir &wd) {
            return QString::fromLatin1(QCryptographicHash::hash(wd.absolutePath().toUtf8(), QCryptographicHash::Md5).toHex());
        }

#ifndef Q_OS_WIN
        // Only we can write to it, so another user can neither squat the socket name nor answer on it (the socket itself is 0700).
        // Root's dir may be 0755: it is shared with HostPresence's marker and a systemd RuntimeDirectory=. *why is set when it is
        // not ours alone; without create nothing is made, and a missing dir is fine (nothing listens in it).
        QString headlessInstancePrivateDir(bool create, QString *why) {
            const auto euid = ::geteuid();
            QByteArray dir = QByteArrayLiteral("/tmp/throne-") + QByteArray::number(qulonglong(euid));
            if (euid == 0) {
                // Under a root-only parent: no user can pre-create it.
#ifdef Q_OS_MACOS
                dir = QByteArrayLiteral("/var/run/throne");
#else
                struct stat run{};
                dir = ::stat("/run", &run) == 0 && S_ISDIR(run.st_mode) ? QByteArrayLiteral("/run/throne") : QByteArrayLiteral("/var/run/throne");
#endif
            }
            struct stat st{};
            if (create && ::mkdir(dir.constData(), 0700) != 0 && errno != EEXIST) {
                *why = QString::fromLocal8Bit(std::strerror(errno));
            } else if (::lstat(dir.constData(), &st) != 0) {
                if (create || errno != ENOENT) *why = QString::fromLocal8Bit(std::strerror(errno));
            } else if (S_ISLNK(st.st_mode)) {
                *why = QStringLiteral("it is a symbolic link");
            } else if (!S_ISDIR(st.st_mode)) {
                *why = QStringLiteral("it is not a directory");
            } else if (st.st_uid != euid) {
                *why = QStringLiteral("it belongs to uid %1").arg(qulonglong(st.st_uid));
            } else if ((st.st_mode & (euid == 0 ? 022 : 077)) != 0) {
                *why = QStringLiteral("its mode is %1, not %2")
                           .arg(uint(st.st_mode & 0777), 4, 8, QLatin1Char('0'))
                           .arg(euid == 0 ? QStringLiteral("0755 or stricter") : QStringLiteral("0700"));
            }
            return QFile::decodeName(dir);
        }

        QString headlessInstanceFallbackName(const QDir &wd, bool create, const QString &remedy) {
            QString why;
            const auto dir = headlessInstancePrivateDir(create, &why);
            if (why.isEmpty()) return dir + QStringLiteral("/headless-") + headlessInstanceHash(wd);
            if (create) LOG_ERROR(QStringLiteral("[Instance] %1 cannot hold the instance socket: %2. Have it removed%3.").arg(dir, why, remedy));
            return {};
        }

        // A socket of this user, root or the data dir's owner; anyone else could answer HEALTH with their own API and key.
        bool headlessInstanceTrusted(const QString &socketName) {
            struct stat st{};
            if (::lstat(QFile::encodeName(socketName).constData(), &st) != 0 || !S_ISSOCK(st.st_mode)) return false;
            if (st.st_uid == ::geteuid() || st.st_uid == 0) return true;
            const QFileInfo socket(socketName);
            if (socket.fileName() == QLatin1String(".instance")) {
                // <wd>/config/.instance
                struct stat owner{};
                const auto wd = QFile::encodeName(QFileInfo(socket.absolutePath()).absolutePath());
                if (::stat(wd.constData(), &owner) == 0 && owner.st_uid == st.st_uid) return true;
            }
            LOG_WARN(QStringLiteral("[Instance] Not talking to %1: it belongs to uid %2, a process of another user.")
                         .arg(socketName).arg(qulonglong(st.st_uid)));
            return false;
        }
#endif

        // Client names come from another process; keep log lines single and short.
        QString headlessInstancePrintable(const QString &text) {
            QString out = text.left(64);
            for (auto &ch : out) {
                if (ch.unicode() < 0x20 || ch.unicode() == 0x7f) ch = QLatin1Char('?');
            }
            return out;
        }

        // Scheme and the throne:// action only: the query can carry subscription tokens.
        QString headlessInstanceLinkLabel(const QString &line) {
            const QUrl url(line);
            const auto scheme = url.scheme().toLower();
            if (scheme == QLatin1String("throne") && !url.host().isEmpty()) return QStringLiteral("throne://") + headlessInstancePrintable(url.host());
            return scheme + QStringLiteral("://");
        }

        QByteArray headlessInstanceReply(const QString &line, const std::function<QJsonObject()> &health) {
            if (line == QLatin1String("HEALTH")) return QJsonDocument(health()).toJson(QJsonDocument::Compact);
            if (line == QLatin1String("HELLO") || line.startsWith(QLatin1String("HELLO "))) {
                LOG_DEBUG(QStringLiteral("[Instance] HELLO from %1").arg(headlessInstancePrintable(line.mid(6).trimmed())));
                return QStringLiteral("HEADLESS pid=%1 version=%2").arg(QCoreApplication::applicationPid()).arg(QStringLiteral(NKR_VERSION)).toUtf8();
            }
            if (line.startsWith(QLatin1String("throne://"), Qt::CaseInsensitive) || line.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
                LOG_WARN(QStringLiteral("[Instance] Refused a %1 link from another launch: headless Throne takes links and files through thronectl.")
                             .arg(headlessInstanceLinkLabel(line)));
                return QByteArrayLiteral("REJECT headless");
            }
            return QByteArrayLiteral("ERR unknown");
        }

        void headlessInstanceHandleLine(QLocalSocket *socket, const QString &line, const std::function<QJsonObject()> &health,
                                        bool adminOnly) {
            if (line.isEmpty() || socket->property(kHeadlessInstanceAnswered).toBool()) return;
            if (socket->state() != QLocalSocket::ConnectedState) return;
            socket->setProperty(kHeadlessInstanceAnswered, true);
            if (adminOnly && !Sys::ServiceManager::LocalClientIsAdmin(socket->socketDescriptor())) {
                LOG_DEBUG(QStringLiteral("[Instance] Refused a client that is neither SYSTEM nor an administrator."));
                socket->write(QByteArray(kHeadlessInstanceForbidden) + '\n');
            } else {
                socket->write(headlessInstanceReply(line, health) + '\n');
            }
            socket->flush();
            // A disconnect from inside the line handler would re-enter InstanceServer's read loop.
            QMetaObject::invokeMethod(socket, [socket] { socket->disconnectFromServer(); }, Qt::QueuedConnection);
        }

        void headlessInstancePrint(FILE *stream, const QByteArray &bytes) {
            std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stream);
            std::fputc('\n', stream);
            std::fflush(stream);
        }

        void headlessInstanceNote(const QString &text) {
            headlessInstancePrint(stderr, text.toLocal8Bit());
        }

        // Single-quoted unless plainly safe; brackets too, since older shells glob the arguments of export.
        QString headlessInstanceShellValue(const QString &value) {
            static const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9_@%+=:,./-]+$"));
            if (safe.match(value).hasMatch()) return value;
            auto quoted = value;
            quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
            return QLatin1Char('\'') + quoted + QLatin1Char('\'');
        }

        // Where a client on this host connects to reach a listener bound to address.
        QString headlessInstanceConnectAddress(const QHostAddress &address, quint16 port) {
            if (address.isNull() || address == QHostAddress::Any || address == QHostAddress::AnyIPv4) return QStringLiteral("127.0.0.1:%1").arg(port);
            if (address == QHostAddress::AnyIPv6) return QStringLiteral("[::1]:%1").arg(port);
            if (address.protocol() == QAbstractSocket::IPv6Protocol) return QStringLiteral("[%1]:%2").arg(address.toString()).arg(port);
            return QStringLiteral("%1:%2").arg(address.toString()).arg(port);
        }

        // Ask(); *denied is set when the pipe refused this account (logged here).
        Reply headlessInstanceAskName(const QString &socketName, const QByteArray &request, int timeoutMs, bool *denied) {
            Reply reply;
            if (socketName.isEmpty()) return reply;
#ifndef Q_OS_WIN
            if (socketName.startsWith(QLatin1Char('/')) && !headlessInstanceTrusted(socketName)) return reply;
#endif
            QElapsedTimer clock;
            clock.start();
            const auto remaining = [&clock, timeoutMs] { return int(qMax<qint64>(0, timeoutMs - clock.elapsed())); };

            QLocalSocket socket;
            socket.connectToServer(socketName);
            if (!socket.waitForConnected(timeoutMs)) {
#ifdef Q_OS_WIN
                // A service's pipe admits SYSTEM and Administrators; any other headless admits only the account it runs as.
                if (socket.error() == QLocalSocket::SocketAccessError) {
                    if (Sys::ServiceManager::RunningAsLocalSystem() || Sys::ServiceManager::SelfElevated()) {
                        LOG_WARN(QStringLiteral("[Instance] Not talking to %1: access denied. That headless Throne admits only the "
                                                "account it runs as.").arg(socketName));
                    } else {
                        LOG_WARN(QStringLiteral("[Instance] Not talking to %1: access denied. A headless Throne running as a service or "
                                                "as LocalSystem admits only Administrators and SYSTEM (run this from an elevated prompt); "
                                                "any other admits only the account it runs as.").arg(socketName));
                        if (denied != nullptr) *denied = true;
                    }
                }
#else
                Q_UNUSED(denied)
#endif
                return reply;
            }
            // Windows: any user can create a pipe of this name first and answer HEALTH with an API and key of their own.
            if (QString why; !Sys::ServiceManager::PipeServerTrusted(socket.socketDescriptor(), &why)) {
                LOG_WARN(QStringLiteral("[Instance] Not talking to %1: %2.").arg(socketName, why));
                socket.abort();
                return reply;
            }
            socket.write(request + '\n');
            socket.flush();
            socket.waitForBytesWritten(remaining());

            QByteArray buffer;
            while (!buffer.contains('\n') && buffer.size() <= kHeadlessInstanceMaxReply) {
                if (socket.bytesAvailable() <= 0) {
                    const int left = remaining();
                    if (left <= 0 || socket.state() != QLocalSocket::ConnectedState || !socket.waitForReadyRead(left)) {
                        buffer += socket.readAll();
                        break;
                    }
                }
                buffer += socket.readAll();
            }
            const bool closed = socket.state() != QLocalSocket::ConnectedState;
            socket.abort();

            const auto newline = buffer.indexOf('\n');
            if (newline >= 0) {
                reply.answered = true;
                reply.line = buffer.left(newline);
            } else if (closed && !buffer.isEmpty()) {
                reply.answered = true;
                reply.line = buffer;
            }
            return reply;
        }

        // One budget for both names; *answeredOn gets the one that answered.
        Reply headlessInstanceAsk(const QDir &wd, const QByteArray &request, int timeoutMs, QString *answeredOn, bool *denied = nullptr) {
            QElapsedTimer clock;
            clock.start();
            QStringList names;
            if (const auto primary = SocketName(wd); !primary.isEmpty()) names << primary;
            if (const auto fallback = FallbackSocketName(wd); !fallback.isEmpty() && !names.contains(fallback)) names << fallback;
            Reply reply;
            for (const auto &name : std::as_const(names)) {
                const int left = int(qMax<qint64>(0, timeoutMs - clock.elapsed()));
                if (left <= 0) break;
                reply = headlessInstanceAskName(name, request, left, denied);
                if (!reply.answered) continue;
                if (answeredOn) *answeredOn = name;
                break;
            }
            return reply;
        }

        // For the no-answer notes: SocketName, plus the private-dir name when something is there; says why that dir was skipped.
        QString headlessInstanceWhere(const QDir &wd) {
            QStringList names;
            if (const auto primary = SocketName(wd); !primary.isEmpty()) names << primary;
#ifndef Q_OS_WIN
            QString why;
            const auto dir = headlessInstancePrivateDir(false, &why);
            if (!why.isEmpty()) headlessInstanceNote(QStringLiteral("Not looking in %1: %2.").arg(dir, why));
            struct stat st{};
            if (const auto fallback = FallbackSocketName(wd);
                !fallback.isEmpty() && !names.contains(fallback) && ::lstat(QFile::encodeName(fallback).constData(), &st) == 0) {
                names << fallback;
            }
#endif
            return names.isEmpty() ? QStringLiteral("its instance socket") : names.join(QStringLiteral(" or "));
        }

        QString headlessInstanceNoAnswerHint(const QString &where) {
            return QStringLiteral("No headless Throne answered on %1. Pass the service's --data-dir and run as the user it runs as.")
                .arg(where);
        }

        bool headlessInstanceRefused(const Reply &reply, const QString &where) {
            if (!reply.answered || reply.line.trimmed() != kHeadlessInstanceForbidden) return false;
            headlessInstanceNote(QStringLiteral("The headless Throne on %1 answers only Administrators and SYSTEM: run this from an "
                                                "elevated prompt.")
                                     .arg(where));
            return true;
        }
    } // namespace

    QString SocketName(const QDir &wd, bool create) {
#ifdef Q_OS_WIN
        Q_UNUSED(create)
        return QStringLiteral("throne-headless-") + headlessInstanceHash(wd);
#else
        // sun_path is 104 bytes on macOS and 108 on Linux.
        const auto path = QDir(wd.absoluteFilePath(QStringLiteral("config"))).absoluteFilePath(QStringLiteral(".instance"));
        if (QFile::encodeName(path).size() < 100) return path;
        return headlessInstanceFallbackName(wd, create, QStringLiteral(", or use a shorter data dir path"));
#endif
    }

    QString FallbackSocketName(const QDir &wd, bool create) {
#ifdef Q_OS_WIN
        Q_UNUSED(wd)
        Q_UNUSED(create)
        return {};
#else
        return headlessInstanceFallbackName(wd, create, {});
#endif
    }

    void Install(InstanceServer &server, std::function<QJsonObject()> health) {
#ifdef Q_OS_WIN
        // HeadlessMain keeps the default named-pipe ACL then, which already lets only them connect; this backs it up.
        const bool adminOnly = Sys::ServiceManager::IsService() || Sys::ServiceManager::RunningAsLocalSystem();
#else
        const bool adminOnly = false;
#endif
        server.setHandlers(
            [health = std::move(health), adminOnly](QLocalSocket *socket, const QString &line) {
                headlessInstanceHandleLine(socket, line, health, adminOnly);
            },
            [](QLocalSocket *socket) {
                QTimer::singleShot(kHeadlessInstanceIdleMs, socket, [socket] {
                    socket->abort();
                    socket->deleteLater();
                });
            });
    }

    Reply Ask(const QString &socketName, const QByteArray &request, int timeoutMs) {
        return headlessInstanceAskName(socketName, request, timeoutMs, nullptr);
    }

    Reply AskInstance(const QDir &wd, const QByteArray &request, int timeoutMs) {
        return headlessInstanceAsk(wd, request, timeoutMs, nullptr);
    }

    qint64 Hello(const QDir &wd, const QString &client, int timeoutMs) {
        const auto reply = AskInstance(wd, QByteArrayLiteral("HELLO ") + client.toUtf8(), timeoutMs);
        if (!reply.answered) return -1;
        const auto tokens = reply.line.trimmed().split(' ');
        if (tokens.isEmpty() || tokens.first() != "HEADLESS") return 0;
        for (const auto &token : tokens) {
            if (!token.startsWith("pid=")) continue;
            bool ok = false;
            const auto pid = token.mid(4).toLongLong(&ok);
            return ok && pid > 0 ? pid : 0;
        }
        return 0;
    }

    int RunHealth(const QDir &wd) {
        QString socketName;
        bool denied = false;
        const auto reply = headlessInstanceAsk(wd, QByteArrayLiteral("HEALTH"), kHeadlessInstanceClientMs, &socketName, &denied);
        // The warning already says to elevate.
        if (denied) return ExitCode::NoInstance;
        if (!reply.answered) {
            headlessInstanceNote(headlessInstanceNoAnswerHint(headlessInstanceWhere(wd)));
            return ExitCode::NoInstance;
        }
        if (headlessInstanceRefused(reply, socketName)) return ExitCode::NoInstance;
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(reply.line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            headlessInstanceNote(QStringLiteral("The answer on %1 is not a HEALTH reply.").arg(socketName));
            return ExitCode::NoInstance;
        }
        const auto health = document.object();
        // Docker keeps this output; the full reply carries the profile's server address.
        QJsonObject subset;
        for (const char *key : {"ok", "ready", "state", "degraded", "stuck_s", "core_restarts", "headless_dialogs", "previous_crash",
                                "previous_exit"}) {
            const auto name = QString::fromLatin1(key);
            if (health.contains(name)) subset.insert(name, health.value(name));
        }
        if (const auto api = health.value(QStringLiteral("api")).toObject(); api.contains(QStringLiteral("state"))) {
            subset.insert(QStringLiteral("api"), api.value(QStringLiteral("state")));
        }
        headlessInstancePrint(stdout, QJsonDocument(subset).toJson(QJsonDocument::Compact));
        return health.value(QStringLiteral("ok")).toBool() ? ExitCode::Ok : ExitCode::Fatal;
    }

    int RunPrintConnection(const QDir &wd, const RemoteApi::Overlay &overlay) {
        QString socketName;
        bool denied = false;
        const auto reply = headlessInstanceAsk(wd, QByteArrayLiteral("HEALTH"), kHeadlessInstanceClientMs, &socketName, &denied);
        // Guessing the address and key file would only send an unelevated caller to a key it cannot read.
        if (denied || headlessInstanceRefused(reply, socketName)) return ExitCode::NoInstance;
        const auto document = reply.answered ? QJsonDocument::fromJson(reply.line) : QJsonDocument();
        const auto api = document.object().value(QStringLiteral("api")).toObject();

        QString address;
        QString keyFile;
        if (!api.isEmpty()) {
            const auto state = api.value(QStringLiteral("state")).toString();
            if (state == QLatin1String("off")) {
                headlessInstanceNote(QStringLiteral("The Throne API of the running headless Throne is off."));
                return ExitCode::NoInstance;
            }
            const auto error = api.value(QStringLiteral("error")).toString(state);
            // The port may belong to another process (retried quietly), or the key file holds no usable key.
            if (state == QLatin1String("failed") || state == QLatin1String("key_invalid")) {
                headlessInstanceNote(QStringLiteral("The Throne API of the running headless Throne is not listening (%1).").arg(error));
                return ExitCode::NoInstance;
            }
            RemoteApi::Overlay listen;
            QString why;
            if (!RemoteApi::ParseListen(api.value(QStringLiteral("listen")).toString(), &listen, &why) || !listen.port.has_value()) {
                headlessInstanceNote(QStringLiteral("The running headless Throne reported no API address."));
                return ExitCode::NoInstance;
            }
            address = headlessInstanceConnectAddress(listen.address, *listen.port);
            keyFile = api.value(QStringLiteral("key_file")).toString();
            if (state == QLatin1String("address_missing")) {
                headlessInstanceNote(QStringLiteral("Note: the API is unreachable (%1).").arg(error));
            } else if (state != QLatin1String("listening")) {
                headlessInstanceNote(QStringLiteral("Note: the API is not listening (%1).").arg(error));
            }
        } else {
            const auto defaultKeyFile = QDir(wd.absoluteFilePath(QStringLiteral("config"))).absoluteFilePath(QStringLiteral("api.key"));
            headlessInstanceNote(reply.answered ? QStringLiteral("The headless Throne is still starting; using the flags, the environment and %1.").arg(defaultKeyFile)
                                                : QStringLiteral("No headless Throne answered on %1; using the flags, the environment and %2.")
                                                      .arg(headlessInstanceWhere(wd), defaultKeyFile));
            if (overlay.enabled.has_value() && !*overlay.enabled) {
                headlessInstanceNote(QStringLiteral("The Throne API is turned off (--api-listen or THRONE_API_LISTEN)."));
                return ExitCode::NoInstance;
            }
            address = overlay.port.has_value() ? headlessInstanceConnectAddress(overlay.address.isNull() ? QHostAddress(QHostAddress::LocalHost) : overlay.address, *overlay.port)
                                               : headlessInstanceConnectAddress(QHostAddress(QHostAddress::LocalHost), RemoteApi::kDefaultPort);
            if (!overlay.keyFile.isEmpty()) keyFile = overlay.keyFile;
            else if (QFileInfo::exists(defaultKeyFile)) keyFile = defaultKeyFile;
        }

        // Exported: eval of a bare assignment leaves it out of thronectl's environment.
        headlessInstancePrint(stdout, (QStringLiteral("export THRONE_ADDR=") + headlessInstanceShellValue(address)).toLocal8Bit());
        if (keyFile.isEmpty()) {
            headlessInstanceNote(QStringLiteral("The API key is not in a file (it comes from the settings, or none was created yet); "
                                                "give thronectl THRONE_KEY or --key-file."));
            return ExitCode::NoInstance;
        }
        headlessInstancePrint(stdout, (QStringLiteral("export THRONE_KEY_FILE=") + headlessInstanceShellValue(keyFile)).toLocal8Bit());
        return ExitCode::Ok;
    }
} // namespace Engine::HeadlessInstance
