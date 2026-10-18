#include "include/engine/app/DataDirLock.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
// <windows.h> defines SetPort, which under unity builds clobbers Configs::outbound::SetPort.
#undef SetPort
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Engine {
    namespace {
        constexpr qint64 MAX_OWNER_RECORD = 4096;

        QByteArray OwnerRecord(const QString &mode) {
            return QJsonDocument(QJsonObject{
                {"mode", mode},
                {"pid", QCoreApplication::applicationPid()},
                {"started", QDateTime::currentSecsSinceEpoch()},
            }).toJson(QJsonDocument::Compact);
        }

        QJsonObject ParseOwnerRecord(const QByteArray &data) {
            const auto doc = QJsonDocument::fromJson(data.trimmed());
            return doc.isObject() ? doc.object() : QJsonObject{};
        }
    } // namespace

    QString DataDirLock::LockPath(const QString &configDir) {
        return QDir(configDir).absoluteFilePath(".lock");
    }

    DataDirLock::~DataDirLock() {
        release();
    }

    bool DataDirLock::isHeld() const {
#ifdef Q_OS_WIN
        return handle != nullptr;
#else
        return fd >= 0;
#endif
    }

    DataDirLock::Result DataDirLock::tryAcquire(const QString &configDir, const QString &mode) {
        release();
        error.clear();
        const QString path = LockPath(configDir);
        const QByteArray record = OwnerRecord(mode);
#ifdef Q_OS_WIN
        const QString nativePath = QDir::toNativeSeparators(path);
        // Share-read only: a second owner gets ERROR_SHARING_VIOLATION while ReadOwner can still open it.
        const HANDLE h = CreateFileW(reinterpret_cast<LPCWSTR>(nativePath.utf16()), GENERIC_READ | GENERIC_WRITE,
                                     FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            const DWORD code = GetLastError();
            if (code == ERROR_SHARING_VIOLATION) return Result::Busy;
            error = QString("could not open %1: Windows error %2").arg(nativePath).arg(code);
            return Result::Failed;
        }
        handle = h;
        DWORD written = 0;
        if (!SetEndOfFile(h) || !WriteFile(h, record.constData(), static_cast<DWORD>(record.size()), &written, nullptr)) {
            error = QString("could not write the owner record to %1: Windows error %2").arg(nativePath).arg(GetLastError());
        }
#else
        const QByteArray nativePath = QFile::encodeName(path);
        // Whoever owns the data dir could aim the truncate below at another file through a link; O_NONBLOCK keeps a FIFO from hanging.
        const int f = ::open(nativePath.constData(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0600);
        if (f < 0) {
            const int code = errno;
            error = QString("could not open %1: %2")
                        .arg(path, code == ELOOP ? QStringLiteral("it is a symbolic link") : QString::fromLocal8Bit(std::strerror(code)));
            return Result::Failed;
        }
        struct stat st{};
        if (::fstat(f, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1) {
            ::close(f);
            error = QString("could not open %1: it is not a regular file with a single link").arg(path);
            return Result::Failed;
        }
        if (::flock(f, LOCK_EX | LOCK_NB) != 0) {
            const int code = errno;
            ::close(f);
            if (code == EWOULDBLOCK) return Result::Busy;
            error = QString("could not lock %1: %2").arg(path, QString::fromLocal8Bit(std::strerror(code)));
            return Result::Failed;
        }
        fd = f;
        if (::ftruncate(f, 0) != 0) {
            error = QString("could not truncate %1: %2").arg(path, QString::fromLocal8Bit(std::strerror(errno)));
            return Result::Acquired;
        }
        qint64 offset = 0;
        while (offset < record.size()) {
            const auto n = ::pwrite(f, record.constData() + offset, static_cast<size_t>(record.size() - offset), static_cast<off_t>(offset));
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) {
                error = QString("could not write the owner record to %1: %2").arg(path, QString::fromLocal8Bit(std::strerror(errno)));
                break;
            }
            offset += n;
        }
#endif
        return Result::Acquired;
    }

    void DataDirLock::release() {
#ifdef Q_OS_WIN
        if (handle == nullptr) return;
        CloseHandle(static_cast<HANDLE>(handle));
        handle = nullptr;
#else
        if (fd < 0) return;
        ::flock(fd, LOCK_UN);
        ::close(fd);
        fd = -1;
#endif
    }

    QJsonObject DataDirLock::ReadOwner(const QString &configDir) {
        const QString path = LockPath(configDir);
#ifdef Q_OS_WIN
        const QString nativePath = QDir::toNativeSeparators(path);
        // FILE_SHARE_WRITE is required: the owner's handle already holds write access.
        const HANDLE h = CreateFileW(reinterpret_cast<LPCWSTR>(nativePath.utf16()), GENERIC_READ,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return {};
        QByteArray data(MAX_OWNER_RECORD, Qt::Uninitialized);
        DWORD read = 0;
        const bool ok = ReadFile(h, data.data(), static_cast<DWORD>(data.size()), &read, nullptr);
        CloseHandle(h);
        if (!ok) return {};
        data.truncate(static_cast<qsizetype>(read));
        return ParseOwnerRecord(data);
#else
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        return ParseOwnerRecord(file.read(MAX_OWNER_RECORD));
#endif
    }

    QString DataDirLock::DescribeOwner(const QJsonObject &owner) {
        const QString mode = owner.value("mode").toString();
        const qint64 pid = owner.value("pid").toInteger();
        const qint64 started = owner.value("started").toInteger();
        QStringList details;
        if (pid > 0) details << QString("PID %1").arg(pid);
        if (started > 0) details << "since " + QDateTime::fromSecsSinceEpoch(started).toString("yyyy-MM-dd HH:mm:ss");
        QString text = mode.isEmpty() ? QStringLiteral("another Throne") : mode;
        if (!details.isEmpty()) text += " (" + details.join(", ") + ")";
        return text;
    }
} // namespace Engine
