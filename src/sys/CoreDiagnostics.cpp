#include "include/sys/CoreDiagnostics.hpp"
#include "NkrVersion.h"

#include "include/api/RPC.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/entities/RouteProfile.h"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <string>
#include <vector>

namespace {
    constexpr qint64 kLogTailBytes = 4 * 1024 * 1024;
    constexpr int kReplySlackMs = 60000;

    std::vector<std::byte> toBytes(const QByteArray &data) {
        const auto *begin = reinterpret_cast<const std::byte *>(data.constData());
        return {begin, begin + data.size()};
    }

    libcore::DiagnosticsAttachment makeAttachment(const QString &name, const QByteArray &data) {
        libcore::DiagnosticsAttachment attachment;
        attachment.name = name.toStdString();
        attachment.data = toBytes(data);
        return attachment;
    }

    QByteArray clientInfo() {
        const auto *settings = Configs::dataManager->settingsRepo.get();
        QString profileType;
        if (settings->started_id >= 0) {
            if (const auto profile = Configs::dataManager->profilesRepo->GetProfile(settings->started_id))
                profileType = profile->type;
        }

        QJsonObject info;
        info["throne_version"] = QStringLiteral(NKR_VERSION);
        info["qt_runtime"] = QString::fromLatin1(qVersion());
        info["qt_build"] = QStringLiteral(QT_VERSION_STR);
        info["os"] = QSysInfo::prettyProductName();
        info["kernel"] = QStringLiteral("%1 %2").arg(QSysInfo::kernelType(), QSysInfo::kernelVersion());
        info["cpu_arch"] = QSysInfo::currentCpuArchitecture();
        info["core_running"] = settings->core_running;
        info["proxy_running"] = settings->started_id >= 0;
        info["running_profile_type"] = profileType;
        info["tun"] = settings->spmode_vpn;
        info["system_proxy"] = settings->spmode_system_proxy;
        info["enable_stats"] = settings->enable_stats;
        info["traffic_stats"] = !settings->disable_traffic_stats;
        info["traffic_aggregation"] = !settings->disable_traffic_aggregation;
        info["resolve_domain_strategy"] = settings->resolve_domain_strategy;
        bool routeOwnsDns = false;
        info["fake_dns"] = Configs::RouteDns::Current(&routeOwnsDns).fake_dns;
        info["route_custom_dns"] = routeOwnsDns;
        info["enable_tun_routing"] = settings->enable_tun_routing;
        return QJsonDocument(info).toJson();
    }

    QByteArray logTail(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        if (file.size() > kLogTailBytes) file.seek(file.size() - kLogTailBytes);
        return file.read(kLogTailBytes);
    }

    QString archivePath(const QDir &dir) {
        const QString stem = QStringLiteral("throne-profile-") +
                             QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        QString path = dir.absoluteFilePath(stem + QStringLiteral(".zip"));
        for (int n = 2; QFileInfo::exists(path); n++)
            path = dir.absoluteFilePath(QStringLiteral("%1-%2.zip").arg(stem).arg(n));
        return path;
    }

    QString saveArchive(const std::vector<std::byte> &archive, const QString &path) {
        QSaveFile file(path);
        const auto size = static_cast<qint64>(archive.size());
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(reinterpret_cast<const char *>(archive.data()), size) != size ||
            !file.commit())
            return file.errorString();
        return {};
    }
} // namespace

namespace Sys {
    CoreDiagnostics::CoreDiagnostics(QObject *parent) : QObject(parent), ticker_(new QTimer(this)) {
        ticker_->setInterval(250);
        connect(ticker_, &QTimer::timeout, this, [this] { emit progress(elapsedMs()); });
    }

    CoreDiagnostics *CoreDiagnostics::instance() {
        static QPointer<CoreDiagnostics> self;
        if (!self) self = new CoreDiagnostics(QCoreApplication::instance());
        return self;
    }

    QString CoreDiagnostics::directory() {
        return QDir(Configs::GetBasePath()).absoluteFilePath(QStringLiteral("diagnostics"));
    }

    void CoreDiagnostics::showInFolder(const QString &path) {
        if (!IsGuiApp()) {
            LOG_DEBUG(QString("diagnostics: show in folder skipped without a GUI: %1").arg(path));
            return;
        }
        const QFileInfo file(path);
        if (path.isEmpty() || !file.isFile()) {
            openDirectory();
            return;
        }
#if defined(Q_OS_WIN)
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                {QStringLiteral("/select,"), QDir::toNativeSeparators(file.absoluteFilePath())});
#elif defined(Q_OS_MACOS)
        QProcess::startDetached(QStringLiteral("/usr/bin/open"), {QStringLiteral("-R"), file.absoluteFilePath()});
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(file.absolutePath()));
#endif
    }

    void CoreDiagnostics::openDirectory() {
        if (!IsGuiApp()) {
            LOG_DEBUG("diagnostics: open directory skipped without a GUI");
            return;
        }
        const QString dir = directory();
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    }

    qint64 CoreDiagnostics::elapsedMs() const {
        return capturing_ ? clock_.elapsed() : 0;
    }

    void CoreDiagnostics::attachView(QWidget *view) {
        view_ = view;
    }

    bool CoreDiagnostics::viewShowing() const {
        return view_ && view_->isVisible();
    }

    void CoreDiagnostics::start(const Options &options) {
        if (capturing_) return;
        options_ = options;

        auto *client = API::defaultClient;
        if (client == nullptr || !Configs::dataManager->settingsRepo->core_running) {
            fail(tr("The core is not running."));
            return;
        }

        auto request = std::make_shared<libcore::DiagnosticsRequest>();
        request->duration_ms = kCaptureMs;
        request->contention = options_.contention;
        request->trace = options_.trace;
        request->attachments.push_back(makeAttachment(QStringLiteral("client.json"), clientInfo()));
        const QString logPath = options_.includeLogs
                                    ? QDir(Logging::LogDir()).absoluteFilePath(QStringLiteral("throne.log"))
                                    : QString();
        const QString dir = directory();

        capturing_ = true;
        stopping_ = false;
        lastError_.clear();
        clock_.start();
        ticker_->start();
        emit stateChanged();

        QPointer<CoreDiagnostics> self(this);
        runOnNewThread([self, client, request, logPath, dir] {
            if (!logPath.isEmpty()) {
                if (const QByteArray log = logTail(logPath); !log.isEmpty())
                    request->attachments.push_back(makeAttachment(QStringLiteral("throne.log"), log));
            }

            Outcome outcome;
            const auto response = client->CaptureDiagnostics(&outcome.rpcOK, *request, kCaptureMs + kReplySlackMs);
            outcome.coreError = QString::fromStdString(response.error.value_or(std::string()));
            if (outcome.rpcOK && outcome.coreError.isEmpty() && response.archive.has_value() && !response.archive->empty()) {
                outcome.empty = false;
                QDir().mkpath(dir);
                outcome.path = archivePath(QDir(dir));
                outcome.size = static_cast<qint64>(response.archive->size());
                outcome.saveError = saveArchive(*response.archive, outcome.path);
            }

            runOnUiThread([self, outcome] {
                if (self) self->complete(outcome);
            });
        });
    }

    void CoreDiagnostics::stop() {
        auto *client = API::defaultClient;
        if (!capturing_ || stopping_ || client == nullptr) return;
        stopping_ = true;
        emit stateChanged();

        QPointer<CoreDiagnostics> self(this);
        runOnNewThread([self, client] {
            bool ok = false;
            client->StopDiagnostics(&ok);
            if (ok) return;
            runOnUiThread([self] {
                if (!self || !self->capturing_) return;
                self->stopping_ = false;
                emit self->stateChanged();
            });
        });
    }

    void CoreDiagnostics::complete(const Outcome &outcome) {
        capturing_ = false;
        stopping_ = false;
        ticker_->stop();

        if (!outcome.rpcOK) {
            fail(tr("Could not reach the core. See the log for details."));
        } else if (!outcome.coreError.isEmpty()) {
            fail(tr("The core could not capture the profile: %1").arg(outcome.coreError));
        } else if (outcome.empty) {
            fail(tr("The core returned an empty profile."));
        } else if (!outcome.saveError.isEmpty()) {
            fail(tr("Could not save %1: %2").arg(QDir::toNativeSeparators(outcome.path), outcome.saveError));
        } else {
            succeed(outcome.path, outcome.size);
        }
    }

    void CoreDiagnostics::succeed(const QString &path, qint64 size) {
        lastPath_ = path;
        lastSize_ = size;
        lastError_.clear();
        MW_show_log(tr("Performance profile saved to %1").arg(QDir::toNativeSeparators(path)));
        emit stateChanged();
        emit finished(path, size);
        if (!viewShowing()) notify(true, tr("The performance profile was saved (%1).").arg(ReadableSize(size)), path);
    }

    void CoreDiagnostics::fail(const QString &error) {
        lastError_ = error;
        MW_show_log(tr("Performance profile failed: %1").arg(error));
        emit stateChanged();
        emit failed(error);
        if (!viewShowing()) notify(false, error, {});
    }

    void CoreDiagnostics::notify(bool saved, const QString &text, const QString &path) {
        if (!IsGuiApp()) {
            LOG_DEBUG(QString("diagnostics: notice skipped without a GUI: %1").arg(text));
            return;
        }
        if (notice_) notice_->close();
        auto *box = new QMessageBox(saved ? QMessageBox::Information : QMessageBox::Warning,
                                    tr("Performance profile"), text, QMessageBox::NoButton, mainwindow);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setWindowModality(Qt::NonModal);
        if (saved) {
            box->setInformativeText(QDir::toNativeSeparators(path));
            auto *reveal = box->addButton(tr("Show in folder"), QMessageBox::ActionRole);
            connect(reveal, &QPushButton::clicked, box, [path] { showInFolder(path); });
            box->addButton(QMessageBox::Close);
        } else {
            box->addButton(QMessageBox::Ok);
        }
        notice_ = box;
        box->show();
    }
} // namespace Sys
