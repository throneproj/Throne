#include "include/engine/tests/ServiceChecks.h"

#include "include/api/RPC.h"
#include "include/configs/generate.h"
#include "include/global/Configs.hpp"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/global/Logger.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QSaveFile>
#include <QStringList>

namespace Engine::ServiceChecks {
    namespace {
        const QString kServiceChecksUrl = QStringLiteral("https://raw.githubusercontent.com/throneproj/routeprofiles/servicecheck/checks-v1.json");
        constexpr qint64 kServiceChecksAttemptIntervalSecs = 24 * 60 * 60;
        constexpr qint64 kServiceChecksMaxBytes = 256 * 1024;
        constexpr int kServiceChecksVersion = 1;

        // Two refreshes at once would race on the cache and its attempt stamp.
        QMutex serviceChecksMutex;

        QString serviceChecksCachePath() {
            return QDir::current().absoluteFilePath(QStringLiteral("service_checks.json"));
        }

        QString serviceChecksMetaPath() {
            return QDir::current().absoluteFilePath(QStringLiteral("service_checks.meta.json"));
        }

        struct ServiceChecksMeta {
            // Names the cached copy.
            QString etag;
            // Unix seconds of the last download attempt, failed or not.
            qint64 attemptedAt = 0;
        };

        ServiceChecksMeta serviceChecksReadMeta() {
            QFile file(serviceChecksMetaPath());
            if (!file.open(QIODevice::ReadOnly)) return {};
            const auto json = QJsonDocument::fromJson(file.read(4096)).object();
            return {json.value(QStringLiteral("etag")).toString(), json.value(QStringLiteral("attempted_at")).toInteger()};
        }

        bool serviceChecksWriteFile(const QString &path, const QByteArray &data) {
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit()) return true;
            LOG_WARN(QStringLiteral("[Service check] Cannot write %1: %2").arg(QDir::toNativeSeparators(path), file.errorString()));
            return false;
        }

        void serviceChecksWriteMeta(const ServiceChecksMeta &meta) {
            const QJsonObject json{{"etag", meta.etag}, {"attempted_at", meta.attemptedAt}};
            serviceChecksWriteFile(serviceChecksMetaPath(), QJsonDocument(json).toJson(QJsonDocument::Compact));
        }

        QByteArray serviceChecksReadCache() {
            QFile file(serviceChecksCachePath());
            if (!file.open(QIODevice::ReadOnly)) return {};
            const auto data = file.read(kServiceChecksMaxBytes + 1);
            return data.size() > kServiceChecksMaxBytes ? QByteArray() : data;
        }

        bool serviceChecksUsable(bool rpcOK, const libcore::ParseServiceChecksResponse &parsed) {
            return rpcOK && parsed.error.value().empty() && parsed.version.value() == kServiceChecksVersion;
        }

        QList<Category> serviceChecksCategories(const libcore::ParseServiceChecksResponse &parsed) {
            QList<Category> categories;
            for (const auto &category : parsed.categories) {
                categories << Category{QString::fromStdString(category.id.value()), QString::fromStdString(category.name.value()),
                                       QString::fromStdString(category.icon.value())};
            }
            return categories;
        }

        // On a newer valid download, stores it and hands it back through cached, parsed and usable.
        void serviceChecksRefresh(QByteArray *cached, libcore::ParseServiceChecksResponse *parsed, bool *usable) {
            auto meta = serviceChecksReadMeta();
            const qint64 now = QDateTime::currentSecsSinceEpoch();
            // A stamp in the future means the clock went back, and waiting it out could take years.
            if (meta.attemptedAt > 0 && meta.attemptedAt <= now && now - meta.attemptedAt < kServiceChecksAttemptIntervalSecs) return;
            meta.attemptedAt = now;
            serviceChecksWriteMeta(meta);

            HttpGetOptions options;
            options.maxBytes = kServiceChecksMaxBytes;
            // The ETag is only worth sending while the copy it names is still there.
            if (*usable && !meta.etag.isEmpty()) options.headers.emplaceBack("If-None-Match", meta.etag.toLatin1());
            const auto response = NetworkRequestHelper::HttpGet(Configs::get_jsdelivr_link(kServiceChecksUrl), options);
            if (!response.error.isEmpty()) {
                LOG_WARN(QStringLiteral("[Service check] Definitions download failed: %1").arg(response.error));
                return;
            }
            if (response.status == 304) return;

            bool ok = false;
            QString coreError;
            auto fresh = API::defaultClient->ParseServiceChecks(&ok, response.data, &coreError);
            if (!serviceChecksUsable(ok, fresh)) {
                QString why = coreError;
                if (ok) {
                    why = fresh.error.value().empty() ? QStringLiteral("unsupported version %1").arg(fresh.version.value())
                                                      : QString::fromStdString(fresh.error.value());
                }
                LOG_WARN(QStringLiteral("[Service check] Downloaded definitions rejected: %1").arg(why));
                return;
            }
            if (!serviceChecksWriteFile(serviceChecksCachePath(), response.data)) return;
            meta.etag = NetworkRequestHelper::GetHeader(response.header, QStringLiteral("ETag"));
            serviceChecksWriteMeta(meta);
            *cached = response.data;
            *parsed = std::move(fresh);
            *usable = true;
        }
    } // namespace

    Definitions Prepare() {
        QMutexLocker locker(&serviceChecksMutex);
        Definitions defs;
        bool ok = false;
        QString coreError;
        const auto embedded = API::defaultClient->ParseServiceChecks(&ok, {}, &coreError);
        if (!ok) {
            defs.error = coreError.isEmpty() ? QStringLiteral("IPC error") : coreError;
            return defs;
        }
        if (!embedded.error.value().empty()) {
            defs.error = QString::fromStdString(embedded.error.value());
            return defs;
        }

        auto cached = serviceChecksReadCache();
        libcore::ParseServiceChecksResponse parsed;
        bool usable = false;
        if (!cached.isEmpty()) {
            bool cacheOK = false;
            parsed = API::defaultClient->ParseServiceChecks(&cacheOK, cached);
            usable = serviceChecksUsable(cacheOK, parsed);
        }
        serviceChecksRefresh(&cached, &parsed, &usable);

        if (usable && parsed.revision.value() > embedded.revision.value()) {
            defs.text = cached;
            defs.categories = serviceChecksCategories(parsed);
        } else {
            defs.categories = serviceChecksCategories(embedded);
        }
        return defs;
    }

    QString ResultJson(const libcore::ServiceTestResult &result, const QList<Category> &categories, qint64 checkedAt) {
        QJsonObject root{{"at", checkedAt}};
        const auto error = QString::fromStdString(result.error.value());
        root.insert(QStringLiteral("error"), error);
        if (!error.isEmpty()) return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));

        QJsonArray checks;
        QStringList used;
        for (const auto &check : result.checks) {
            const auto category = QString::fromStdString(check.category.value());
            if (!used.contains(category)) used << category;
            checks.append(QJsonObject{
                {"id", QString::fromStdString(check.id.value())},
                {"name", QString::fromStdString(check.name.value())},
                {"cat", category},
                {"status", QString::fromStdString(check.status.value())},
                {"region", QString::fromStdString(check.region.value())},
                {"note", QString::fromStdString(check.note.value())},
            });
        }
        QJsonArray cats;
        for (const auto &category : categories) {
            if (!used.removeOne(category.id)) continue;
            cats.append(QJsonObject{{"id", category.id}, {"name", category.name}, {"icon", category.icon}});
        }
        // A check naming a category the definitions do not list still gets a section.
        for (const auto &id : used) cats.append(QJsonObject{{"id", id}, {"name", id}, {"icon", QString()}});

        QJsonArray ip;
        for (const auto &info : result.ip_info) {
            QJsonArray flags;
            for (const auto &flag : info.flags) flags.append(QString::fromStdString(flag));
            ip.append(QJsonObject{
                {"provider", QString::fromStdString(info.provider.value())},
                {"ip", QString::fromStdString(info.ip.value())},
                {"cc", QString::fromStdString(info.country_code.value())},
                {"asn", QString::fromStdString(info.asn.value())},
                {"org", QString::fromStdString(info.org.value())},
                {"type", QString::fromStdString(info.type.value())},
                {"risk", info.risk_score.value()},
                {"flags", flags},
                {"error", QString::fromStdString(info.error.value())},
            });
        }
        root.insert(QStringLiteral("cats"), cats);
        root.insert(QStringLiteral("checks"), checks);
        root.insert(QStringLiteral("ip"), ip);
        return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }
} // namespace Engine::ServiceChecks
