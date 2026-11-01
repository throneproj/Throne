#include "include/sys/macos/ProxyJournal.hpp"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <cerrno>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace MacSystemProxy {
    namespace {
        constexpr qint64 maxSize = 1024 * 1024;
        QString validateFile(const QString &path, bool &exists) {
            struct stat st{};
            if (::lstat(QFile::encodeName(path).constData(), &st) != 0) {
                exists = false;
                return errno == ENOENT ? QString{} : QStringLiteral("cannot inspect system proxy journal");
            }
            exists = true;
            if (!S_ISREG(st.st_mode) || st.st_uid != ::geteuid() || st.st_nlink != 1 || (st.st_mode & 077) != 0 || st.st_size > maxSize)
                return "unsafe system proxy journal";
            return {};
        }

    }
    QString FileJournal::load(QVector<Entry> &entries) {
        entries.clear();
        bool exists;
        if (auto error = validateFile(path, exists); !error.isEmpty()) return error;
        if (!exists) return {};
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return "cannot read system proxy journal";
        QJsonParseError parseError;
        const auto doc = QJsonDocument::fromJson(file.read(maxSize + 1), &parseError);
        const auto root = doc.object();
        if (parseError.error != QJsonParseError::NoError || root.value("version").toInt() != 1 || !root.value("entries").isArray())
            return "invalid system proxy journal; preserved for recovery";
        QVector<Entry> parsed;
        QSet<QString> seen;
        for (const auto &value : root.value("entries").toArray()) {
            const auto object = value.toObject();
            const int protocol = object.value("protocol").toInt(-1);
            const auto id = object.value("service").toString();
            if (id.isEmpty() || id.size() > 256 || protocol < 0 || protocol > 2 ||
                !object.value("before").isObject() || !object.value("applied").isObject())
                return "invalid system proxy journal entry";
            const auto fields = keys(protocol);
            const auto before = object.value("before").toObject(), applied = object.value("applied").toObject();
            const auto identity = id + '/' + QString::number(protocol);
            if (seen.contains(identity) || !validState(before, fields) || !validState(applied, fields) || applied.size() != fields.size() ||
                applied.value(fields[0]).toInt() != 1 || applied.value(fields[1]).toString().isEmpty() ||
                applied.value(fields[2]).toInt() < 1 || applied.value(fields[2]).toInt() > 65535 || applied.value(fields[3]).toInt() != 0)
                return "invalid system proxy journal values";
            seen.insert(identity);
            parsed << Entry{id, protocol, before, applied};
        }
        entries = parsed;
        return {};
    }
    QString FileJournal::save(const QVector<Entry> &entries) {
        bool exists;
        if (auto error = validateFile(path, exists); !error.isEmpty()) return error;
        if (entries.isEmpty()) {
            if (exists && !QFile::remove(path)) return "cannot remove completed system proxy journal";
            return {};
        }
        QJsonArray array;
        for (const auto &entry : entries) array << QJsonObject{{"service", entry.service}, {"protocol", entry.protocol},
                                                              {"before", entry.before}, {"applied", entry.applied}};
        const auto data = QJsonDocument(QJsonObject{{"version", 1}, {"entries", array}}).toJson(QJsonDocument::Compact);
        if (data.size() > maxSize) return "system proxy journal is too large";
        QSaveFile file(path);
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) ||
            file.write(data) != data.size() || !file.commit()) return "cannot commit system proxy journal";
        // QSaveFile provides atomic replacement; explicitly flush both the record and its directory before touching the network.
        const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        const bool flushed = fd >= 0 && ::fsync(fd) == 0;
        if (fd >= 0) ::close(fd);
        const int dir = ::open(QFile::encodeName(QFileInfo(path).absolutePath()).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        const bool dirFlushed = dir >= 0 && ::fsync(dir) == 0;
        if (dir >= 0) ::close(dir);
        return flushed && dirFlushed ? QString{} : QStringLiteral("cannot flush system proxy journal");
    }
}
