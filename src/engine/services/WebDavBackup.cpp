#include "include/engine/services/WebDavBackup.h"

#include <algorithm>
#include <utility>

#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QSysInfo>
#include <QTimeZone>
#include <QUrl>
#include <QXmlStreamReader>

#include "NkrVersion.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/services/BackupArchive.h"
#include "include/global/Configs.hpp"
#include "include/global/HTTPRequestHelper.hpp"

namespace Engine::WebDavBackup {
    namespace {
        constexpr int kWebDavTimeoutMs = 30000;
        // Idle timeout of the archive transfers: a server may go quiet while it stores a large upload.
        constexpr int kWebDavTransferTimeoutMs = 120000;
        constexpr qint64 kWebDavIndexMaxBytes = 4LL * 1024 * 1024;
        constexpr qint64 kWebDavListingMaxBytes = 32LL * 1024 * 1024;
        constexpr qint64 kWebDavArchiveMaxBytes = 1024LL * 1024 * 1024;
        constexpr qsizetype kWebDavIndexMaxEntries = 1000;
        constexpr int kWebDavIndexRounds = 4;
        constexpr int kWebDavMaxRedirects = 5;
        constexpr int kWebDavIndexFormat = 1;
        constexpr char kWebDavIndexName[] = "throne_backups.json";
        constexpr char kWebDavDefaultFolder[] = "Throne";
        constexpr char kWebDavPropfindBody[] =
            R"(<?xml version="1.0" encoding="utf-8"?><d:propfind xmlns:d="DAV:"><d:prop><d:resourcetype/><d:getcontentlength/><d:getlastmodified/><d:getetag/></d:prop></d:propfind>)";

        struct WebDavRequest {
            QByteArray verb = "GET";
            QUrl url;
            QList<QPair<QByteArray, QByteArray>> headers;
            QByteArray body;
            // Sent instead of body when set.
            QIODevice *upload = nullptr;
            // A 200 answer is written here (and hashed) instead of collected into the response body.
            QIODevice *sink = nullptr;
            QCryptographicHash *hash = nullptr;
            qint64 maxBytes = kWebDavIndexMaxBytes;
            int timeoutMs = kWebDavTimeoutMs;
            bool withAuth = true;
        };

        struct WebDavResponse {
            int status = 0;
            // Set when no usable HTTP answer arrived.
            QString error;
            QByteArray body;
            QByteArray etag;
            QByteArray location;
            qint64 sunk = 0;
        };

        struct WebDavListedFile {
            QString name;
            qint64 size = -1;
            QDateTime modified;
        };

        struct WebDavIndex {
            QJsonObject root;
            bool existed = false;
            // Strong ETag only.
            QByteArray etag;
        };

        bool WebDavSameOrigin(const QUrl &a, const QUrl &b) {
            const auto port = [](const QUrl &url) {
                return url.port(url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) == 0 ? 443 : 80);
            };
            return a.scheme().compare(b.scheme(), Qt::CaseInsensitive) == 0 && a.host().compare(b.host(), Qt::CaseInsensitive) == 0 &&
                   port(a) == port(b);
        }

        // One operation's connection to the server, used from the worker thread that created it.
        class WebDavSession {
        public:
            explicit WebDavSession(const Config &config) {
                QString proxyError;
                if (Configs_network::RequestUsesMixedInbound(config.useProxy, &proxyError)) {
                    manager_.setProxy(Configs_network::MixedInboundProxy());
                } else if (!proxyError.isEmpty()) {
                    error_ = proxyError;
                    return;
                }
                const QString server = config.server.trimmed();
                if (server.isEmpty()) {
                    error_ = QObject::tr("Enter the WebDAV server URL in the WebDAV settings.");
                    return;
                }
                QUrl base(server, QUrl::TolerantMode);
                const QString scheme = base.scheme().toLower();
                if (!base.isValid() || (scheme != QLatin1String("http") && scheme != QLatin1String("https")) || base.host().isEmpty()) {
                    error_ = QObject::tr("The WebDAV server URL must start with http:// or https://.");
                    return;
                }
                base.setUserInfo(QString());
                base.setQuery(QString());
                base.setFragment(QString());
                basePath_ = base.path(QUrl::FullyEncoded);
                if (!basePath_.endsWith(u'/')) basePath_ += u'/';
                base_ = base;
                segments_ = config.path.split(u'/', Qt::SkipEmptyParts);
                if (segments_.isEmpty()) segments_ << QString::fromLatin1(kWebDavDefaultFolder);
                folder_ = collection(segments_.size());
                if (!config.user.isEmpty() || !config.pass.isEmpty())
                    auth_ = "Basic " + (config.user.toUtf8() + ':' + config.pass.toUtf8()).toBase64();
                insecure_ = Configs::dataManager->settingsRepo->net_insecure;
            }

            const QString &error() const { return error_; }
            const QUrl &folder() const { return folder_; }
            qsizetype segmentCount() const { return segments_.size(); }

            // The folder made of the first count segments, with its trailing slash.
            QUrl collection(qsizetype count) const {
                QString path = basePath_;
                for (qsizetype i = 0; i < count; ++i) path += QString::fromLatin1(QUrl::toPercentEncoding(segments_[i])) + u'/';
                QUrl url = base_;
                url.setPath(path, QUrl::StrictMode);
                return url;
            }

            QUrl file(const QString &name) const {
                QUrl url = folder_;
                url.setPath(folder_.path(QUrl::FullyEncoded) + QString::fromLatin1(QUrl::toPercentEncoding(name)), QUrl::StrictMode);
                return url;
            }

            WebDavResponse send(const WebDavRequest &r) {
                QNetworkRequest request(r.url);
                request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
                request.setTransferTimeout(r.timeoutMs);
                request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Throne/") + QStringLiteral(NKR_VERSION));
                if (r.withAuth && !auth_.isEmpty()) request.setRawHeader("Authorization", auth_);
                if (insecure_) {
                    auto ssl = request.sslConfiguration();
                    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
                    request.setSslConfiguration(ssl);
                }
                for (const auto &[name, value] : r.headers) request.setRawHeader(name, value);

                QNetworkReply *reply = nullptr;
                if (r.verb == "GET") {
                    reply = manager_.get(request);
                } else if (r.upload != nullptr) {
                    request.setHeader(QNetworkRequest::ContentLengthHeader, r.upload->size());
                    reply = manager_.sendCustomRequest(request, r.verb, r.upload);
                } else if (r.body.isEmpty()) {
                    reply = manager_.sendCustomRequest(request, r.verb);
                } else {
                    reply = manager_.sendCustomRequest(request, r.verb, r.body);
                }

                WebDavResponse out;
                qint64 received = 0;
                bool tooLarge = false;
                bool writeFailed = false;
                const auto consume = [&] {
                    const QByteArray chunk = reply->readAll();
                    if (chunk.isEmpty() || tooLarge || writeFailed) return;
                    received += chunk.size();
                    if (received > r.maxBytes) {
                        tooLarge = true;
                        reply->abort();
                        return;
                    }
                    if (r.sink != nullptr && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
                        if (r.sink->write(chunk) != chunk.size()) {
                            writeFailed = true;
                            reply->abort();
                            return;
                        }
                        if (r.hash != nullptr) r.hash->addData(chunk);
                        out.sunk += chunk.size();
                    } else {
                        out.body += chunk;
                    }
                };
                QObject::connect(reply, &QNetworkReply::readyRead, reply, consume);
                QEventLoop loop;
                QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                if (!reply->isFinished()) loop.exec();
                consume();

                out.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                out.etag = reply->rawHeader("ETag");
                out.location = reply->rawHeader("Location");
                const auto netError = reply->error();
                if (tooLarge) {
                    out.error = QObject::tr("The server sent more than %1 MB.").arg(r.maxBytes / (1024 * 1024));
                } else if (writeFailed) {
                    out.error = QObject::tr("Could not write the downloaded backup: %1").arg(r.sink->errorString());
                } else if (out.status == 0 || (out.status >= 200 && out.status < 300 && netError != QNetworkReply::NoError)) {
                    // A 2xx that still failed was cut off mid-transfer.
                    out.error = netError == QNetworkReply::OperationCanceledError ? QObject::tr("The server did not answer in time.")
                                                                                  : reply->errorString();
                    if (out.error.isEmpty()) out.error = QObject::tr("The server could not be reached.");
                }
                reply->deleteLater();
                return out;
            }

            // Follows redirects, never from https down to http; credentials go to the original origin only.
            WebDavResponse get(WebDavRequest r) {
                r.verb = "GET";
                const QUrl origin = r.url;
                for (int hop = 0;; ++hop) {
                    auto response = send(r);
                    const int s = response.status;
                    const bool redirect = response.error.isEmpty() && (s == 301 || s == 302 || s == 303 || s == 307 || s == 308);
                    if (!redirect || hop >= kWebDavMaxRedirects || response.location.trimmed().isEmpty()) return response;
                    const QUrl next = r.url.resolved(QUrl::fromEncoded(response.location.trimmed(), QUrl::TolerantMode));
                    const QString scheme = next.scheme().toLower();
                    const bool fromHttps = r.url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) == 0;
                    if (!next.isValid() || next.host().isEmpty() ||
                        (scheme != QLatin1String("https") && (scheme != QLatin1String("http") || fromHttps)))
                        return response;
                    if (!WebDavSameOrigin(next, origin)) r.withAuth = false;
                    r.url = next;
                }
            }

        private:
            QNetworkAccessManager manager_;
            QString error_;
            QUrl base_;
            QString basePath_;
            QStringList segments_;
            QUrl folder_;
            QByteArray auth_;
            bool insecure_ = false;
        };

        QString WebDavFailure(const WebDavResponse &r) {
            if (!r.error.isEmpty()) return r.error;
            if (r.status >= 300 && r.status < 400) {
                const QString location = QString::fromUtf8(r.location).simplified().left(200);
                if (location.isEmpty()) return QObject::tr("The server answered with a redirect (HTTP %1); check the server URL.").arg(r.status);
                return QObject::tr("The server redirected to %1; check the server URL.").arg(location);
            }
            if (r.status == 401) return QObject::tr("Wrong username or password.");
            if (r.status == 403) return QObject::tr("Permission denied.");
            if (r.status == 404) return QObject::tr("Not found.");
            if (r.status >= 500 && r.status < 600) return QObject::tr("Server error (HTTP %1).").arg(r.status);
            return QObject::tr("HTTP %1").arg(r.status);
        }

        bool WebDavMultiStatus(int status) {
            return status == 207 || status == 200;
        }

        bool WebDavSafeFileName(const QString &name) {
            return !name.isEmpty() && !name.contains(u'/') && !name.contains(u'\\') && name != QLatin1String(".") &&
                   name != QLatin1String("..") && name.endsWith(QLatin1String(".thrbackup"), Qt::CaseInsensitive);
        }

        // Newest first; unknown dates sort last.
        bool WebDavNewer(const QDateTime &a, const QDateTime &b) {
            if (!a.isValid()) return false;
            return !b.isValid() || a > b;
        }

        // RFC 1123, as getlastmodified carries it.
        QDateTime WebDavHttpDate(const QString &text) {
            const QString trimmed = text.trimmed();
            if (trimmed.isEmpty()) return {};
            QDateTime parsed = QDateTime::fromString(trimmed, Qt::RFC2822Date);
            // Qt's RFC 2822 parser takes only numeric zones, so "... GMT" lands here.
            if (!parsed.isValid()) {
                const QDateTime naive = QLocale::c().toDateTime(trimmed, QStringLiteral("ddd, d MMM yyyy HH:mm:ss 'GMT'"));
                if (naive.isValid()) parsed = QDateTime(naive.date(), naive.time(), QTimeZone::utc());
            }
            return parsed.isValid() ? parsed.toUTC() : QDateTime();
        }

        QDateTime WebDavIsoDate(const QJsonValue &value) {
            const QDateTime parsed = QDateTime::fromString(value.toString(), Qt::ISODate);
            return parsed.isValid() ? parsed.toUTC() : QDateTime();
        }

        // The UTC yyyyMMdd_HHmmss stamp Throne puts in its file names.
        QDateTime WebDavNameStamp(const QString &name) {
            static const QRegularExpression stamp(QStringLiteral(R"((\d{8})_(\d{6}))"));
            const auto match = stamp.match(name);
            if (!match.hasMatch()) return {};
            const QDate date = QDate::fromString(match.captured(1), QStringLiteral("yyyyMMdd"));
            const QTime time = QTime::fromString(match.captured(2), QStringLiteral("HHmmss"));
            if (!date.isValid() || !time.isValid()) return {};
            return QDateTime(date, time, QTimeZone::utc());
        }

        QString WebDavNewFileName(const QDateTime &utc) {
            QString version = QStringLiteral(NKR_VERSION);
            for (auto &c : version) {
                const char16_t u = c.unicode();
                const bool kept = (u >= u'a' && u <= u'z') || (u >= u'A' && u <= u'Z') || (u >= u'0' && u <= u'9') || u == u'.' || u == u'_' ||
                                  u == u'-';
                if (!kept) c = u'-';
            }
            const QString random = QStringLiteral("%1").arg(QRandomGenerator::global()->bounded(0x10000), 4, 16, QLatin1Char('0'));
            return QStringLiteral("throne_backup_%1_%2_%3.thrbackup").arg(version, utc.toString(QStringLiteral("yyyyMMdd_HHmmss")), random);
        }

        QString WebDavTrimSlashes(QString path) {
            while (path.endsWith(u'/')) path.chop(1);
            return path;
        }

        // The folder's .thrbackup children; false when the body is not a readable multistatus.
        bool WebDavParseListing(const QByteArray &xml, const QUrl &folder, QList<WebDavListedFile> *files) {
            const QString dav = QStringLiteral("DAV:");
            const QString folderPath = WebDavTrimSlashes(folder.path(QUrl::FullyDecoded));
            QSet<QString> seen;
            QXmlStreamReader reader(xml);
            bool multistatus = false;
            bool inResponse = false;
            bool collection = false;
            QString href;
            QString length;
            QString modified;
            while (!reader.atEnd()) {
                const auto token = reader.readNext();
                if (token == QXmlStreamReader::StartElement && reader.namespaceUri() == dav) {
                    const auto name = reader.name();
                    if (name == QLatin1String("multistatus")) {
                        multistatus = true;
                    } else if (name == QLatin1String("response")) {
                        inResponse = true;
                        collection = false;
                        href.clear();
                        length.clear();
                        modified.clear();
                    } else if (!inResponse) {
                        continue;
                    } else if (name == QLatin1String("collection")) {
                        collection = true;
                    } else if (name == QLatin1String("href") || name == QLatin1String("getcontentlength") ||
                               name == QLatin1String("getlastmodified")) {
                        QString &target = name == QLatin1String("href") ? href : name == QLatin1String("getcontentlength") ? length : modified;
                        const QString text = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
                        if (target.isEmpty()) target = text;
                    }
                    continue;
                }
                if (token != QXmlStreamReader::EndElement || !inResponse || reader.namespaceUri() != dav ||
                    reader.name() != QLatin1String("response"))
                    continue;
                inResponse = false;
                if (href.isEmpty() || collection) continue;
                const QUrl resolved = folder.resolved(QUrl::fromEncoded(href.toUtf8(), QUrl::TolerantMode));
                if (WebDavTrimSlashes(resolved.path(QUrl::FullyDecoded)) == folderPath) continue;
                const QString encodedPath = resolved.path(QUrl::FullyEncoded);
                if (encodedPath.endsWith(u'/')) continue;
                const QString name = QUrl::fromPercentEncoding(encodedPath.mid(encodedPath.lastIndexOf(u'/') + 1).toUtf8());
                if (!WebDavSafeFileName(name) || seen.contains(name)) continue;
                seen.insert(name);
                WebDavListedFile file;
                file.name = name;
                bool ok = false;
                const qint64 size = length.toLongLong(&ok);
                file.size = ok && size >= 0 ? size : -1;
                file.modified = WebDavHttpDate(modified);
                files->append(file);
            }
            return !reader.hasError() && multistatus;
        }

        WebDavRequest WebDavPropfind(const QUrl &url, const QByteArray &depth) {
            WebDavRequest request;
            request.verb = "PROPFIND";
            request.url = url;
            request.headers = {{"Depth", depth}, {"Content-Type", "application/xml; charset=utf-8"}};
            request.body = QByteArray(kWebDavPropfindBody);
            request.maxBytes = kWebDavListingMaxBytes;
            return request;
        }

        // *missing: the folder does not exist yet.
        QString WebDavListFolder(WebDavSession &session, QList<WebDavListedFile> *files, bool *missing) {
            files->clear();
            *missing = false;
            const auto response = session.send(WebDavPropfind(session.folder(), "1"));
            if (!response.error.isEmpty()) return response.error;
            if (response.status == 404) {
                *missing = true;
                return {};
            }
            if (!WebDavMultiStatus(response.status)) return WebDavFailure(response);
            if (!WebDavParseListing(response.body, session.folder(), files)) return QObject::tr("The server's folder listing could not be read.");
            return {};
        }

        QString WebDavEnsureFolder(WebDavSession &session) {
            const auto probe = session.send(WebDavPropfind(session.folder(), "0"));
            if (!probe.error.isEmpty()) return probe.error;
            if (WebDavMultiStatus(probe.status)) return {};
            if (probe.status != 404) return WebDavFailure(probe);
            for (qsizetype count = 1; count <= session.segmentCount(); ++count) {
                WebDavRequest mkcol;
                mkcol.verb = "MKCOL";
                mkcol.url = session.collection(count);
                const auto made = session.send(mkcol);
                if (!made.error.isEmpty()) return made.error;
                if ((made.status >= 200 && made.status < 300) || made.status == 405) continue;
                // Some servers refuse MKCOL on a folder that is already there with something other than 405.
                const auto exists = session.send(WebDavPropfind(mkcol.url, "0"));
                if (exists.error.isEmpty() && WebDavMultiStatus(exists.status)) continue;
                return QObject::tr("Could not create the folder on the server: %1").arg(WebDavFailure(made));
            }
            return {};
        }

        // A missing index reads as empty with existed false, a malformed one as empty.
        QString WebDavFetchIndex(WebDavSession &session, WebDavIndex *index) {
            WebDavRequest request;
            request.url = session.file(QString::fromLatin1(kWebDavIndexName));
            request.maxBytes = kWebDavIndexMaxBytes;
            const auto response = session.get(request);
            if (!response.error.isEmpty()) return response.error;
            if (response.status == 404) return {};
            if (response.status != 200) return WebDavFailure(response);
            index->existed = true;
            if (!response.etag.isEmpty() && !response.etag.startsWith("W/")) index->etag = response.etag;
            const auto doc = QJsonDocument::fromJson(response.body);
            if (doc.isObject()) index->root = doc.object();
            return {};
        }

        bool WebDavEntryFromJson(const QJsonObject &object, Entry *entry) {
            static const QRegularExpression sha256(QStringLiteral("^[0-9a-f]{64}$"));
            const QString file = object.value("file").toString();
            if (!WebDavSafeFileName(file)) return false;
            entry->file = file;
            entry->createdAt = WebDavIsoDate(object.value("created_at"));
            const QJsonValue size = object.value("size");
            entry->size = size.isDouble() && size.toDouble() >= 0 ? size.toInteger(-1) : -1;
            const QString hash = object.value("sha256").toString().toLower();
            entry->sha256 = sha256.match(hash).hasMatch() ? hash : QString();
            entry->deviceId = object.value("device_id").toString();
            entry->deviceName = object.value("device_name").toString();
            entry->platform = object.value("platform").toString();
            entry->appVersion = object.value("app_version").toString();
            for (const auto &part : object.value("parts").toArray()) {
                if (part.isString()) entry->parts << part.toString();
            }
            entry->indexed = true;
            return true;
        }

        // Records added for file, or drops file from the index when added is empty. Returns why the index could not be updated.
        QString WebDavUpdateIndex(WebDavSession &session, const QString &file, const QJsonObject &added) {
            const QUrl url = session.file(QString::fromLatin1(kWebDavIndexName));
            for (int round = 1; round <= kWebDavIndexRounds; ++round) {
                // After three conflicts, one unconditional write for servers that mishandle conditional requests.
                const bool conditional = round < kWebDavIndexRounds;
                WebDavIndex index;
                if (auto error = WebDavFetchIndex(session, &index); !error.isEmpty()) return error;
                if (const int format = index.root.value("format").toInt(kWebDavIndexFormat); format > kWebDavIndexFormat) {
                    MW_show_log(QStringLiteral("[Backup] The WebDAV backup list uses format %1, newer than this Throne writes; left it unchanged.")
                                    .arg(format));
                    return {};
                }
                QList<WebDavListedFile> listed;
                bool missing = false;
                if (auto error = WebDavListFolder(session, &listed, &missing); !error.isEmpty()) return error;
                QSet<QString> present;
                for (const auto &entry : listed) present.insert(entry.name);

                QList<std::pair<QDateTime, QJsonObject>> kept;
                if (!added.isEmpty()) kept.emplaceBack(WebDavIsoDate(added.value("created_at")), added);
                QSet<QString> seen{file};
                for (const auto &value : index.root.value("backups").toArray()) {
                    if (!value.isObject()) continue;
                    const QJsonObject object = value.toObject();
                    const QString name = object.value("file").toString();
                    if (seen.contains(name) || !present.contains(name)) continue;
                    seen.insert(name);
                    kept.emplaceBack(WebDavIsoDate(object.value("created_at")), object);
                }
                std::stable_sort(kept.begin(), kept.end(), [](const auto &a, const auto &b) { return WebDavNewer(a.first, b.first); });
                while (kept.size() > kWebDavIndexMaxEntries) kept.removeLast();
                QJsonArray backups;
                for (const auto &entry : std::as_const(kept)) backups.append(entry.second);
                QJsonObject root = index.root;
                root["format"] = kWebDavIndexFormat;
                root["backups"] = backups;

                WebDavRequest request;
                request.verb = "PUT";
                request.url = url;
                request.headers = {{"Content-Type", "application/json"}};
                if (conditional && !index.etag.isEmpty()) request.headers.emplaceBack("If-Match", index.etag);
                else if (conditional && !index.existed) request.headers.emplaceBack("If-None-Match", "*");
                request.body = QJsonDocument(root).toJson(QJsonDocument::Compact);
                const auto response = session.send(request);
                if (!response.error.isEmpty()) return response.error;
                if (response.status == 412 && conditional) continue;
                if (response.status < 200 || response.status >= 300) return WebDavFailure(response);
                return {};
            }
            return QObject::tr("The backup list kept changing while it was being updated.");
        }
    } // namespace

    Config FromSettings() {
        ENGINE_CHECK_MAIN_THREAD();
        const auto device = BackupArchive::ThisDevice();
        const auto &settings = *Configs::dataManager->settingsRepo;
        Config config;
        config.server = settings.webdav_server;
        config.user = settings.webdav_user;
        config.pass = settings.webdav_pass;
        config.path = settings.webdav_path;
        config.useProxy = settings.webdav_use_proxy;
        config.deviceId = device.id;
        config.deviceName = device.name;
        return config;
    }

    QString Test(const Config &config) {
        WebDavSession session(config);
        if (!session.error().isEmpty()) return session.error();
        return WebDavEnsureFolder(session);
    }

    QString Upload(const Config &config, const QString &localPath, const QStringList &parts, QString *fileName, QString *indexWarning) {
        fileName->clear();
        indexWarning->clear();
        WebDavSession session(config);
        if (!session.error().isEmpty()) return session.error();
        if (auto error = WebDavEnsureFolder(session); !error.isEmpty()) return error;

        QFile file(localPath);
        if (!file.open(QIODevice::ReadOnly)) return QObject::tr("Could not read the backup file: %1").arg(file.errorString());
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!hash.addData(&file) || !file.seek(0)) return QObject::tr("Could not read the backup file: %1").arg(file.errorString());
        const qint64 size = file.size();

        const QDateTime now = QDateTime::currentDateTimeUtc();
        const QString name = WebDavNewFileName(now);
        WebDavRequest request;
        request.verb = "PUT";
        request.url = session.file(name);
        request.headers = {{"Content-Type", "application/octet-stream"}};
        request.upload = &file;
        request.timeoutMs = kWebDavTransferTimeoutMs;
        const auto response = session.send(request);
        if (!response.error.isEmpty()) return response.error;
        if (response.status < 200 || response.status >= 300) return WebDavFailure(response);
        *fileName = name;

        QJsonObject entry;
        entry["file"] = name;
        entry["created_at"] = now.toString(Qt::ISODate);
        entry["size"] = size;
        entry["sha256"] = QString::fromLatin1(hash.result().toHex());
        entry["device_id"] = config.deviceId;
        entry["device_name"] = config.deviceName;
        entry["platform"] = QSysInfo::kernelType();
        entry["app_version"] = QStringLiteral(NKR_VERSION);
        entry["parts"] = QJsonArray::fromStringList(parts);
        *indexWarning = WebDavUpdateIndex(session, name, entry);
        return {};
    }

    QString List(const Config &config, QList<Entry> *entries, QString *indexWarning) {
        entries->clear();
        indexWarning->clear();
        WebDavSession session(config);
        if (!session.error().isEmpty()) return session.error();
        QList<WebDavListedFile> files;
        bool missing = false;
        if (auto error = WebDavListFolder(session, &files, &missing); !error.isEmpty()) return error;
        if (files.isEmpty()) return {};

        WebDavIndex index;
        *indexWarning = WebDavFetchIndex(session, &index);
        QHash<QString, Entry> indexed;
        for (const auto &value : index.root.value("backups").toArray()) {
            Entry entry;
            if (value.isObject() && WebDavEntryFromJson(value.toObject(), &entry) && !indexed.contains(entry.file)) indexed.insert(entry.file, entry);
        }
        for (const auto &file : std::as_const(files)) {
            Entry entry;
            if (const auto it = indexed.constFind(file.name); it != indexed.cend()) {
                entry = it.value();
                if (entry.size < 0) entry.size = file.size;
            } else {
                entry.file = file.name;
                entry.deviceName = QObject::tr("Unknown device");
                entry.size = file.size;
            }
            if (!entry.createdAt.isValid()) entry.createdAt = file.modified.isValid() ? file.modified : WebDavNameStamp(file.name);
            entries->append(entry);
        }
        std::stable_sort(entries->begin(), entries->end(), [](const Entry &a, const Entry &b) { return WebDavNewer(a.createdAt, b.createdAt); });
        return {};
    }

    QString Download(const Config &config, const Entry &entry, const QString &localPath) {
        if (!WebDavSafeFileName(entry.file)) return QObject::tr("The backup's file name is not safe to use.");
        WebDavSession session(config);
        if (!session.error().isEmpty()) return session.error();
        QFile out(localPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return QObject::tr("Could not create a temporary file: %1").arg(out.errorString());
        QCryptographicHash hash(QCryptographicHash::Sha256);
        WebDavRequest request;
        request.url = session.file(entry.file);
        request.sink = &out;
        request.hash = &hash;
        request.maxBytes = kWebDavArchiveMaxBytes;
        request.timeoutMs = kWebDavTransferTimeoutMs;
        const auto response = session.get(request);
        const bool flushed = out.flush();
        out.close();

        QString error;
        if (!response.error.isEmpty()) {
            error = response.error;
        } else if (response.status != 200) {
            error = WebDavFailure(response);
        } else if (!flushed) {
            error = QObject::tr("Could not write the downloaded backup: %1").arg(out.errorString());
        } else if (entry.indexed && ((entry.size >= 0 && response.sunk != entry.size) ||
                                     (!entry.sha256.isEmpty() && hash.result().toHex() != entry.sha256.toLatin1()))) {
            error = QObject::tr("The downloaded backup does not match the backup list; it may have been replaced or damaged.");
        }
        if (!error.isEmpty()) QFile::remove(localPath);
        return error;
    }

    QString Delete(const Config &config, const Entry &entry, QString *indexWarning) {
        indexWarning->clear();
        if (!WebDavSafeFileName(entry.file)) return QObject::tr("The backup's file name is not safe to use.");
        WebDavSession session(config);
        if (!session.error().isEmpty()) return session.error();
        WebDavRequest request;
        request.verb = "DELETE";
        request.url = session.file(entry.file);
        const auto response = session.send(request);
        if (!response.error.isEmpty()) return response.error;
        if (response.status != 404 && (response.status < 200 || response.status >= 300)) return WebDavFailure(response);
        *indexWarning = WebDavUpdateIndex(session, entry.file, {});
        return {};
    }
} // namespace Engine::WebDavBackup
