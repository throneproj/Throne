#include "include/api/remote/Overlay.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringConverter>

#include <algorithm>
#include <cerrno>
#include <cstring>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#endif

#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"

namespace RemoteApi {
    namespace {
        constexpr qint64 kConfigKeyFileMaxBytes = 4096;

        Overlay &configOverlay() {
            static Overlay overlay;
            return overlay;
        }

        // KeyFromText keeps a BOM, and PowerShell writes UTF-16 with one.
        QByteArray configDecodeKey(const QByteArray &bytes) {
            QString text;
            if (const auto encoding = QStringConverter::encodingForData(bytes)) {
                QStringDecoder decoder(*encoding);
                text = decoder.decode(bytes);
                if (decoder.hasError()) text.clear();
            } else {
                text = QString::fromUtf8(bytes);
            }
            return KeyFromText(text);
        }

        // 0 or errno. QFileDevice has no sync; on Windows handle() is a CRT descriptor over the file's HANDLE, which _commit flushes.
        int configSyncFile(QFile &file) {
#ifdef Q_OS_UNIX
            if (::fsync(file.handle()) != 0) return errno;
#elif defined(Q_OS_WIN)
            if (::_commit(file.handle()) != 0) return errno;
#endif
            return 0;
        }

        // What an interrupted first start leaves: nothing, or NULs where delayed allocation never wrote the data back.
        bool configHoldsNoKey(const QByteArray &bytes) {
            return std::all_of(bytes.cbegin(), bytes.cend(), [](char c) { return c == '\0' || c == ' ' || (c >= '\t' && c <= '\r'); });
        }

        // Never replaces an existing file.
        bool configPlaceKeyFile(const QString &temp, const QString &path, QString *error) {
#ifdef Q_OS_UNIX
            const auto from = QFile::encodeName(temp);
            if (::link(from.constData(), QFile::encodeName(path).constData()) == 0) {
                ::unlink(from.constData());
                // The name itself survives a power cut too; without it the worst case is a missing file, created again next start.
                const int dir = ::open(QFile::encodeName(QFileInfo(path).absolutePath()).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                if (dir >= 0) {
                    ::fsync(dir);
                    ::close(dir);
                }
                return true;
            }
            const int linkError = errno;
            // vfat and some FUSE or network mounts have no hard links; QFile::rename still refuses an existing target.
            if (linkError != EPERM && linkError != EOPNOTSUPP && linkError != ENOTSUP && linkError != ENOSYS) {
                *error = QString::fromLocal8Bit(std::strerror(linkError));
                return false;
            }
#endif
            QFile file(temp);
            if (file.rename(path)) return true;
            *error = file.errorString();
            return false;
        }

        // Written under api.key.tmp-<pid> and synced before it takes the real name, so a crash never leaves a partial key file.
        KeyFile configCreateKeyFile(const QString &path) {
            KeyFile result;
            const auto temp = path + QStringLiteral(".tmp-") + QString::number(QCoreApplication::applicationPid());
            QFile file(temp);
#ifdef Q_OS_UNIX
            const auto nativeTemp = QFile::encodeName(temp);
            constexpr int createFlags = O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC;
            // Owner-only from the create on: a reader that opened it while lax would keep its descriptor past a chmod.
            int fd = ::open(nativeTemp.constData(), createFlags, 0600);
            // A crash can leave the temp behind, and a container restarts with the same pid; the data dir lock rules out a live owner.
            if (fd < 0 && errno == EEXIST && ::unlink(nativeTemp.constData()) == 0) fd = ::open(nativeTemp.constData(), createFlags, 0600);
            if (fd < 0) {
                result.error = QStringLiteral("it cannot be created: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
                return result;
            }
            const bool ready = file.open(fd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle);
            if (!ready) ::close(fd);
#else
            bool ready = file.open(QIODevice::WriteOnly | QIODevice::NewOnly);
            if (!ready && QFile::exists(temp) && QFile::remove(temp)) ready = file.open(QIODevice::WriteOnly | QIODevice::NewOnly);
            if (!ready) {
                result.error = QStringLiteral("it cannot be created: %1").arg(file.errorString());
                return result;
            }
            // Owner-only before the key is in it; QSaveFile would copy a lax mode instead.
            ready = file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
            const auto key = GenerateKey();
            const auto text = KeyToText(key).toLatin1() + '\n';
            if (!ready || file.write(text) != text.size() || !file.flush()) {
                result.error = QStringLiteral("it cannot be written: %1").arg(file.errorString());
                file.close();
                QFile::remove(temp);
                return result;
            }
            if (const int syncError = configSyncFile(file); syncError != 0) {
                result.error = QStringLiteral("it cannot be written to disk: %1").arg(QString::fromLocal8Bit(std::strerror(syncError)));
                file.close();
                QFile::remove(temp);
                return result;
            }
            file.close();
            QString placeError;
            if (!configPlaceKeyFile(temp, path, &placeError)) {
                QFile::remove(temp);
                result.error = QStringLiteral("it cannot be created: %1").arg(placeError);
                return result;
            }
            result.key = key;
            result.created = true;
            return result;
        }

#ifdef Q_OS_UNIX
        // Unlinks the file fstat described only while the name still leads to it, so a file swapped in meanwhile is left alone.
        bool configRemoveKeyFile(const QString &path, const struct stat &opened) {
            const auto native = QFile::encodeName(path);
            struct stat current{};
            if (::lstat(native.constData(), &current) != 0 || !S_ISREG(current.st_mode)) return false;
            if (current.st_dev != opened.st_dev || current.st_ino != opened.st_ino) return false;
            return ::unlink(native.constData()) == 0;
        }

        // Only our own default key file, opened without a link, is narrowed; an operator's file is never chmod'ed.
        void configCheckKeyFileMode(int fd, const struct stat &st, const QString &path, bool mayNarrow) {
            if ((st.st_mode & 077) == 0) return;
            const auto mode = QStringLiteral("%1").arg(uint(st.st_mode & 0777), 4, 8, QLatin1Char('0'));
            if (!mayNarrow || st.st_uid != ::geteuid()) {
                LOG_WARN(QStringLiteral("[API] The key file %1 is open to other users (mode %2); make it owner-only (chmod 600).").arg(path, mode));
            } else if (::fchmod(fd, 0600) == 0) {
                LOG_WARN(QStringLiteral("[API] The key file %1 was open to other users (mode %2); it is owner-only (0600) now.").arg(path, mode));
            } else {
                LOG_WARN(QStringLiteral("[API] The key file %1 is open to other users (mode %2) and could not be made owner-only: %3")
                             .arg(path, mode, QString::fromLocal8Bit(std::strerror(errno))));
            }
        }
#endif
    } // namespace

    Config ConfigFromSettings() {
        const auto &settings = Configs::dataManager->settingsRepo;
        Config config;
        config.enabled = settings->remote_api_enable;
        config.lan = settings->remote_api_lan;
        config.adminLan = settings->remote_api_admin_lan;
        if (settings->remote_api_port >= 1 && settings->remote_api_port <= 65535) config.port = quint16(settings->remote_api_port);
        config.key = KeyFromText(settings->remote_api_key);
        config.allow = ParseAllowList(settings->remote_api_allow);
        return config;
    }

    QList<QPair<QHostAddress, int>> ParseAllowList(const QString &text) {
        QList<QPair<QHostAddress, int>> allow;
        static const QRegularExpression separators(QStringLiteral("[,\\s]+"));
        for (auto entry : text.split(separators, Qt::SkipEmptyParts)) {
            const auto host = entry.section(QLatin1Char('/'), 0, 0);
            // parseSubnet alone takes abbreviated IPv4 such as "10" for 10.0.0.0/8.
            if (!host.contains(QLatin1Char(':')) && host.count(QLatin1Char('.')) != 3) continue;
            if (!entry.contains(QLatin1Char('/'))) entry += host.contains(QLatin1Char(':')) ? QStringLiteral("/128") : QStringLiteral("/32");
            const auto subnet = QHostAddress::parseSubnet(entry);
            if (subnet.second >= 0) allow << subnet;
        }
        return allow;
    }

    bool ParseListen(const QString &text, Overlay *overlay, QString *error) {
        const auto value = text.trimmed();
        if (value.compare(QLatin1String("off"), Qt::CaseInsensitive) == 0 || value.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0) {
            overlay->enabled = false;
            return true;
        }

        const auto colon = value.lastIndexOf(QLatin1Char(':'));
        const auto portText = colon >= 0 ? value.mid(colon + 1) : value;
        static const QRegularExpression digits(QStringLiteral("^[0-9]{1,5}$"));
        const uint port = portText.toUInt();
        if (!digits.match(portText).hasMatch() || port < 1 || port > 65535) {
            *error = QStringLiteral("\"%1\" needs a port from 1 to 65535: PORT, IPv4:PORT, [IPv6]:PORT, *:PORT or off").arg(value);
            return false;
        }

        QHostAddress address;
        if (colon >= 0) {
            const auto host = value.left(colon);
            if (host.isEmpty() || host == QLatin1String("*")) {
                address = QHostAddress(QHostAddress::Any);
            } else if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']'))) {
                if (!address.setAddress(host.mid(1, host.size() - 2)) || address.protocol() != QAbstractSocket::IPv6Protocol) {
                    *error = QStringLiteral("\"%1\" is not an IPv6 address").arg(host);
                    return false;
                }
            } else if (host.contains(QLatin1Char(':'))) {
                *error = QStringLiteral("\"%1\": write an IPv6 address in brackets, as in [::1]:9095").arg(value);
                return false;
            } else if (host.count(QLatin1Char('.')) != 3 || !address.setAddress(host) || address.protocol() != QAbstractSocket::IPv4Protocol) {
                *error = QStringLiteral("\"%1\" is not an IP address; host names are not accepted").arg(host);
                return false;
            }
        }
        overlay->enabled = true;
        overlay->address = address;
        overlay->port = quint16(port);
        return true;
    }

    KeyFile LoadKeyFile(const QString &path, bool createIfMissing) {
        KeyFile result;
        QFile file(path);
#ifdef Q_OS_UNIX
        // O_NONBLOCK: a FIFO opens at once and is refused below instead of blocking; O_NOFOLLOW tells a link apart.
        const auto native = QFile::encodeName(path);
        constexpr int flags = O_RDONLY | O_NONBLOCK | O_NOCTTY | O_CLOEXEC;
        bool linked = false;
        int fd = ::open(native.constData(), flags | O_NOFOLLOW);
        if (fd < 0 && (errno == ELOOP || errno == EMLINK)) {
            linked = true;
            fd = ::open(native.constData(), flags);
        }
        if (fd < 0) {
            const int error = errno;
            if (error == ENOENT && createIfMissing) return configCreateKeyFile(path);
            result.error = error == ENOENT ? QStringLiteral("it does not exist")
                                           : QStringLiteral("it cannot be read: %1").arg(QString::fromLocal8Bit(std::strerror(error)));
            return result;
        }
        struct stat st{};
        if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
            ::close(fd);
            result.error = QStringLiteral("it is not a regular file");
            return result;
        }
        ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) & ~O_NONBLOCK);
        if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
            ::close(fd);
            result.error = QStringLiteral("it cannot be read: %1").arg(file.errorString());
            return result;
        }
#else
        const QFileInfo info(path);
        if (!info.exists()) {
            if (createIfMissing) return configCreateKeyFile(path);
            result.error = QStringLiteral("it does not exist");
            return result;
        }
        if (!info.isFile()) {
            result.error = QStringLiteral("it is not a regular file");
            return result;
        }
        if (!file.open(QIODevice::ReadOnly)) {
            result.error = QStringLiteral("it cannot be read: %1").arg(file.errorString());
            return result;
        }
#endif
        const auto bytes = file.read(kConfigKeyFileMaxBytes + 1);
        if (bytes.size() > kConfigKeyFileMaxBytes) {
            result.error = QStringLiteral("it is larger than 4 KiB, so it is not a key file");
            return result;
        }
        result.key = configDecodeKey(bytes);
        if (result.key.isEmpty()) {
#ifdef Q_OS_UNIX
            const bool ours = !linked && st.st_uid == ::geteuid();
#else
            const bool ours = !info.isSymLink();
#endif
            if (createIfMissing && ours && file.error() == QFileDevice::NoError && configHoldsNoKey(bytes)) {
                file.close();
#ifdef Q_OS_UNIX
                const bool removed = configRemoveKeyFile(path, st);
#else
                const bool removed = QFile::remove(path);
#endif
                if (removed) {
                    auto created = configCreateKeyFile(path);
                    created.replaced = created.created;
                    return created;
                }
            }
            result.error = QStringLiteral("it does not hold a Throne API key");
            return result;
        }
#ifdef Q_OS_UNIX
        configCheckKeyFileMode(fd, st, path, createIfMissing && !linked);
#endif
        return result;
    }

    void SetOverlay(const Overlay &overlay) {
        configOverlay() = overlay;
    }

    const Overlay &CurrentOverlay() {
        return configOverlay();
    }

    Effective EffectiveConfig() {
        Effective effective;
        effective.config = ConfigFromSettings();
        const auto &overlay = configOverlay();
        if (!overlay.headless) return effective;

        if (!effective.config.enabled) {
            effective.config = Config{};
            effective.config.enabled = true;
            effective.config.key = overlay.key;
            effective.keyFile = overlay.defaultKeyFile;
            effective.source = ConfigSource::Default;
        }
        if (overlay.enabled.has_value() && !*overlay.enabled) {
            effective.config.enabled = false;
            effective.source = overlay.listenSource;
            return effective;
        }
        if (overlay.port.has_value()) {
            effective.config.address = overlay.address.isNull() ? QHostAddress(QHostAddress::LocalHost) : overlay.address;
            effective.config.port = *overlay.port;
            effective.config.lan = !effective.config.address.isLoopback();
            effective.explicitListen = true;
            effective.source = overlay.listenSource;
        }
        if (!overlay.keyFile.isEmpty()) {
            effective.config.key = overlay.key;
            effective.keyFile = overlay.keyFile;
        }
        if (overlay.adminLan.has_value()) effective.config.adminLan = *overlay.adminLan;
        if (overlay.allow.has_value()) effective.config.allow = ParseAllowList(*overlay.allow);
        return effective;
    }
} // namespace RemoteApi
