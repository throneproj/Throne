#include "include/ui/mainwindow.h"

#include <algorithm>
#include <utility>

#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#include <QInputDialog>
#include <QMessageBox>
#include <QStringConverter>
#include <QUrl>

#include "3rdparty/QrDecoder.h"
#include "include/api/remote/Server.hpp"
#include "include/configs/sub/GroupUpdater.hpp"
#include "include/configs/sub/RouteUpdater.hpp"
#include "include/database/GroupsRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/engine/EngineHost.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/app/ExitController.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/global/Logger.hpp"
#include "include/global/PeriodicRunner.hpp"
#include "include/sys/AutoRun.hpp"
#include "include/sys/KillSwitch.hpp"
#include "include/ui/mainWindow/MainWindowInternal.h"
#include "include/ui/setting/Icon.hpp"
#include "include/ui/utils/ProfilesTableModel.h"

namespace {

constexpr qint64 kMaxImportFileSize = 50 * 1024 * 1024;

// A BOM is dropped here: Qt's json parser rejects it.
bool decodeImportedText(const QByteArray &bytes, QString &out) {
    if (const auto encoding = QStringConverter::encodingForData(bytes)) {
        QStringDecoder decoder(*encoding);
        out = decoder(bytes);
        return !decoder.hasError();
    }

    const qsizetype sample = std::min<qsizetype>(bytes.size(), 8192);
    qsizetype control = 0;
    for (qsizetype i = 0; i < sample; i++) {
        const auto c = static_cast<unsigned char>(bytes[i]);
        if (c == 0) return false;
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != '\v' && c != '\f' && c != 0x1B) control++;
    }
    if (control * 20 > sample) return false;

    out = QString::fromUtf8(bytes);
    return true;
}

} // namespace

void MainWindow::importFromFiles(const QStringList &paths)
{
    QStringList payloads;
    QStringList problems;

    for (const QString &path : paths) {
        const auto name = QFileInfo(path).fileName();
        auto file = QFile(path);
        if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
            problems << tr("%1: cannot be opened").arg(name);
            continue;
        }
        if (file.size() > kMaxImportFileSize) {
            file.close();
            problems << tr("%1: larger than 50 MB, skipped").arg(name);
            continue;
        }
        const auto bytes = file.readAll();
        file.close();

        QBuffer buffer;
        buffer.setData(bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        reader.setDecideFormatFromContent(true);
        if (reader.canRead()) {
            const auto image = reader.read();
            const QVector<QString> texts = image.isNull()
                                               ? QVector<QString>{}
                                               : QrDecoder().decode(image.convertToFormat(QImage::Format_Grayscale8));
            if (texts.isEmpty()) {
                problems << tr("%1: no QR code found").arg(name);
                continue;
            }
            for (const QString &text : texts) {
                MW_show_log("QR Code Result:\n" + text);
                payloads << text.trimmed();
            }
            continue;
        }

        QString text;
        if (!decodeImportedText(bytes, text) || text.trimmed().isEmpty()) {
            problems << tr("%1: not a readable config file").arg(name);
            continue;
        }
        payloads << text.trimmed();
    }

    for (const QString &problem : problems) MW_show_log(problem);

    QStringList batch;
    for (const QString &payload : payloads) {
        if (payload.startsWith("http://") || payload.startsWith("https://")) {
            import_text(payload);
        } else {
            batch << payload;
        }
    }
    if (!batch.isEmpty()) Subscription::updater()->ImportBatch(batch);

    if (payloads.isEmpty() && !problems.isEmpty()) {
        MessageBoxWarning(software_name, tr("Nothing could be imported:") + "\n" + problems.join("\n"));
    }
}

void MainWindow::handle_deeplink_impl(const QString &url) {
    const QUrl u(url);
    // QUrl lowercases the host, so "throne://AddSub/" arrives with host "addsub".
    const QString cmd = u.host();

    if (cmd.compare("add", Qt::CaseInsensitive) == 0) {
        import_text(url);
        return;
    }

    if (cmd.compare("remoteroute", Qt::CaseInsensitive) == 0) {
        handle_add_remote_routes(url);
        return;
    }

    QString base64 = u.path();
    if (base64.startsWith('/')) {
        base64 = base64.mid(1);
    }
    else {
        return;
    }

    if (cmd.compare("route", Qt::CaseInsensitive) == 0) {
        handle_import_route(base64);
        return;
    }

    const QString data = DecodeB64IfValid(base64);
    if (data.isEmpty()) return;
    const QUrl link(data);
    if (!link.isValid()) return;

    if (cmd.compare("addsub", Qt::CaseInsensitive) == 0) {
        handle_addsub(link.toString(QUrl::RemoveFragment), link.fragment());
        return;
    }

    MW_show_log(tr("Ignored deeplink with unknown command: %1").arg(cmd));
}

void MainWindow::handle_import_route(const QString &url) {
    QString fatal, warnings;
    bool wasOldArray = false;
    auto profile = Configs::RouteProfile::FromShareInput(url, &fatal, &warnings, &wasOldArray);
    if (!profile) {
        MessageBoxWarning(tr("Import routing profile"), tr("The link could not be parsed:\n") + fatal);
        return;
    }
    if (profile->name.trimmed().isEmpty()) profile->name = tr("Imported profile");

    ActivateWindow(this);

    auto prompt = tr("Add this routing profile?\n\nName: %1").arg(profile->name);
    if (!warnings.isEmpty()) prompt += "\n\n" + tr("Note:") + "\n" + warnings.trimmed();
    QMessageBox box(QMessageBox::Question, tr("Import routing profile"), prompt, QMessageBox::Yes | QMessageBox::No, GetMessageBoxParent());
    // The name and the notes come from the link: AutoText would render a tag-like one as HTML.
    box.setTextFormat(Qt::PlainText);
    if (box.exec() != QMessageBox::Yes) return;

    Configs::dataManager->routesRepo->AddRouteProfile(profile);
}

void MainWindow::handle_add_remote_routes(const QString &url) {
    bool wasRemoteRouteLink = false;
    QString error;
    auto profiles = Configs::RouteProfile::FromRemoteRoutesLink(url, &wasRemoteRouteLink, &error);
    if (profiles.isEmpty()) {
        MessageBoxWarning(tr("Add remote routing profiles"),
                          error.isEmpty() ? tr("The link did not contain any valid remote routing profiles.") : error);
        return;
    }

    ActivateWindow(this);

    QString prompt = tr("Add these remote routing profiles?") + "\n";
    for (int i = 0; i < profiles.size(); ++i) {
        prompt += QString("\n%1. %2")
                      .arg(i + 1)
                      .arg(profiles[i]->remoteURL);
    }
    bool autoUpdate = true;
    if (MessageBoxCheck(tr("Add remote routing profiles"), prompt, tr("Auto update"), autoUpdate) != QMessageBox::Ok) {
        return;
    }

    for (auto &profile : profiles) {
        profile->autoUpdate = autoUpdate;
        Configs::dataManager->routesRepo->AddRouteProfile(profile);
    }

    const auto added = profiles;
    runOnNewThread([added] {
        int ok = 0;
        for (const auto &p : added) {
            QString warnings;
            const QString err = RouteUpdate::UpdateProfile(p, &warnings);
            Configs::dataManager->routesRepo->Save(p);
            if (err.isEmpty()) ok++;
            else MW_show_log(QObject::tr("Remote routing profile %1 failed: %2").arg(p->remoteURL, err));
        }
        MW_show_log(QObject::tr("Added remote routing profiles: %1 of %2 fetched").arg(ok).arg(added.size()));
    });
}

void MainWindow::handle_addsub(const QString &url, const QString &name) {
    if (url.isEmpty()) {
        MessageBoxWarning(tr("Add subscription"), tr("The link did not contain a subscription URL."));
        return;
    }

    ActivateWindow(this);

    const QString groupName = FIRST_OR_SECOND(name, QUrl(url).host());
    const auto prompt = tr("Add this subscription?\n\nName: %1\nURL: %2")
                            .arg(groupName, url);
    bool autoUpdate = true;
    if (MessageBoxCheck(tr("Add subscription"), prompt, tr("Auto update"), autoUpdate) != QMessageBox::Ok) {
        return;
    }

    auto group = Configs::GroupsRepo::NewGroup();
    group->name = groupName;
    group->url = url;
    group->skip_auto_update = !autoUpdate;
    Configs::dataManager->groupsRepo->AddGroup(group);
    refresh_groups();
    Subscription::updater()->RefreshGroup(group->id, nullptr, Subscription::GroupUpdater::Surface::Report);
}

void MainWindow::import_or_handle_deeplink(const QString &text) {
    if (const QString trimmed = text.trimmed(); trimmed.startsWith("throne://")) {
        handle_deeplink_impl(trimmed);
        return;
    }
    import_text(text);
}

void MainWindow::import_text(const QString &text) {
    const auto content = text.trimmed();
    if (content.startsWith("http://") || content.startsWith("https://")) {
        const QStringList items{
            QObject::tr("Add profiles to this group"),
            QObject::tr("Create new subscription group"),
            QObject::tr("Import HTTP proxy profile"),
        };
        bool ok = false;
        const auto choice = QInputDialog::getItem(nullptr, QObject::tr("url detected"),
                                                  QObject::tr("%1\nHow to update?").arg(content), items, 0, false, &ok);
        if (!ok) return;
        switch (items.indexOf(choice)) {
            case 0:
                Subscription::updater()->ImportUrl(content);
                return;
            case 1:
                Subscription::updater()->SubscribeUrl(content);
                return;
            default:
                break;
        }
    }
    Subscription::updater()->ImportText(content);
}

void MainWindow::dialog_message_impl(MwMessage cmd, const QStringList &args) {
    ENGINE_CHECK_MAIN_THREAD();
    const auto changed = [&](const QString &flag) { return args.contains(flag); };
    auto &settings = Configs::dataManager->settingsRepo;

    switch (cmd) {
    case MwMessage::UpdateSettings: {
        updateLogFilterFields();
        ui->actionTraffic_Stats->setVisible(!settings->disable_traffic_aggregation);
        refresh_traffic_graph_settings();
        if (changed(MwArg::TrayIcon)) {
            Icon::InvalidateTrayIconCache();
            icon_status.reset();
        }
        if (changed(MwArg::MaxLogLines)) {
            trimLogLines();
            rebuildLogView();
        }
        if (changed(MwArg::LogFont)) {
            applyLogBrowserFont();
        }
        if (changed(MwArg::DisableTray)) {
            tray->setVisible(!settings->disable_tray);
        }
        if (changed(MwArg::ProfileListDisplay)) {
            if (auto group = Configs::dataManager->groupsRepo->CurrentGroup();
                group && group->calculated_column_width.size() > ProfilesTableModel::ColType)
                group->calculated_column_width[ProfilesTableModel::ColType] = 0;
            // Both settings feed the security a Type sort reads.
            for (const int gid : Configs::dataManager->groupsRepo->GetAllGroupIds()) {
                const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
                if (group != nullptr && ProfilesTableModel::columnOfSort(group->sort.method) == ProfilesTableModel::ColType) {
                    Configs::ResortLater(gid);
                }
            }
            refresh_proxy_list({}, true);
        }
        Engine::host()->applySettingsChange(args, true);
        break;
    }
    case MwMessage::RestartProgram:
        exitController()->requestExit(ExitReason::Restart, false);
        break;
    case MwMessage::Raise:
        ActivateWindow(this);
        break;
    case MwMessage::UpdateShortcuts:
        loadShortcuts();
        break;
    case MwMessage::ProfileChanged:
        refresh_proxy_list({}, true);
        if (changed(MwArg::RestartProxy)) connection()->noteRestartNeeded(Engine::RestartReason::Profile);
        break;
    case MwMessage::GroupsChanged:
        refresh_groups();
        profilesTableModel->invalidateAddresses();
        if (changed(MwArg::RestartProxy)) connection()->noteRestartNeeded(Engine::RestartReason::Group);
        break;
    }
}
