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
#include "include/database/SettingsSchema.h"
#include "include/engine/EditLocks.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/EngineHost.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/app/ExitController.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/engine/services/GroupService.h"
#include "include/engine/services/ImportService.h"
#include "include/engine/services/RouteService.h"
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

// The updater files a gid -1 import under the current group, as ProfilesRepo::AddProfile does a route import's endpoints.
bool importTargetAccepted(const QString &title = software_name) {
    const auto refusal = Engine::ImportService::CheckTarget(Configs::dataManager->settingsRepo->current_group);
    if (refusal) MessageBoxWarning(title, ImportTargetRefusalText(*refusal));
    return !refusal;
}

// Main thread; route is the copy a worker fetched the remote rules into.
void commitRemoteRouteUpdate(const std::shared_ptr<Configs::RouteProfile> &route) {
    QString error;
    if (Engine::IsBeingEdited(Engine::EditTarget::Routes, route->id)) error = QObject::tr("the routing profiles are open for editing");
    else if (const auto refusal = Engine::RouteService::Save(route)) error = refusal->message;
    if (!error.isEmpty()) MW_show_log(QObject::tr("Remote routing profile %1 failed: %2").arg(route->remoteURL, error));
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
        } else if (payload.startsWith("throne://") && !payload.contains('\n')) {
            import_or_handle_deeplink(payload);
        } else {
            batch << payload;
        }
    }
    if (!batch.isEmpty() && importTargetAccepted()) Subscription::updater()->ImportBatch(batch);

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
    int newEndpoints = 0;
    auto profile = Configs::RouteProfile::FromShareInput(url, &fatal, &warnings, &wasOldArray, false, &newEndpoints);
    if (!profile) {
        MessageBoxWarning(tr("Import routing profile"), tr("The link could not be parsed:\n") + fatal);
        return;
    }
    const auto name = profile->name.trimmed().isEmpty() ? tr("Imported profile") : profile->name;

    ActivateWindow(this);
    if (newEndpoints > 0 && !importTargetAccepted(tr("Import routing profile"))) return;

    auto prompt = tr("Add this routing profile?\n\nName: %1").arg(name);
    if (newEndpoints > 0) {
        const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
        prompt += "\n\n" + tr("Its endpoints will be added as %n new profile(s) to the group \"%1\".", nullptr, newEndpoints)
                               .arg(group != nullptr ? group->name : QString());
    }
    if (!warnings.isEmpty()) prompt += "\n\n" + tr("Note:") + "\n" + warnings.trimmed();
    QMessageBox box(QMessageBox::Question, tr("Import routing profile"), prompt, QMessageBox::Yes | QMessageBox::No, GetMessageBoxParent());
    // The name and the notes come from the link: AutoText would render a tag-like one as HTML.
    box.setTextFormat(Qt::PlainText);
    if (box.exec() != QMessageBox::Yes) return;

    if (newEndpoints > 0) profile = Configs::RouteProfile::FromShareInput(url, &fatal, &warnings, &wasOldArray);
    if (profile == nullptr) return;
    profile->name = name;
    if (const auto error = Engine::RouteService::Add(profile)) MW_show_log(error->message);
    // FromShareInput adds the endpoint profiles to the current group itself, so no ProfilesChanged refreshes the list.
    if (newEndpoints > 0) refresh_proxy_list({}, true);
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

    QList<std::shared_ptr<Configs::RouteProfile>> updates;
    for (auto &profile : profiles) {
        profile->autoUpdate = autoUpdate;
        if (const auto error = Engine::RouteService::Add(profile)) {
            MW_show_log(QObject::tr("Remote routing profile %1 failed: %2").arg(profile->remoteURL, error->message));
            continue;
        }
        // The added profile is live, so the worker fetches into a copy.
        updates << std::make_shared<Configs::RouteProfile>(*profile);
    }
    if (updates.isEmpty()) return;

    runOnNewThread([updates] {
        int ok = 0;
        for (const auto &p : updates) {
            QString warnings;
            const QString err = RouteUpdate::UpdateProfile(p, &warnings);
            if (err.isEmpty()) {
                ok++;
                runOnUiThread([p] { commitRemoteRouteUpdate(p); });
            } else {
                MW_show_log(QObject::tr("Remote routing profile %1 failed: %2").arg(p->remoteURL, err));
            }
        }
        MW_show_log(QObject::tr("Added remote routing profiles: %1 of %2 fetched").arg(ok).arg(updates.size()));
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
    if (const auto error = Engine::GroupService::Add(group)) {
        MW_show_log(error->message);
        return;
    }
    Engine::EngineEvents::SubscriptionNewGroup(group->id);
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
                if (importTargetAccepted()) Subscription::updater()->ImportUrl(content);
                return;
            case 1:
                Subscription::updater()->SubscribeUrl(content);
                return;
            default:
                break;
        }
    }
    if (importTargetAccepted()) Subscription::updater()->ImportText(content);
}

void MainWindow::applySettingsUi(quint32 effects) {
    namespace Schema = Configs::SettingsSchema;
    const auto has = [effects](quint32 bit) { return (effects & bit) != 0; };
    const auto &settings = Configs::dataManager->settingsRepo;
    if (has(Schema::UiLogFilter)) updateLogFilterFields();
    if (has(Schema::UiTrafficStats)) {
        ui->actionTraffic_Stats->setVisible(!settings->disable_traffic_aggregation);
        refresh_traffic_graph_settings();
    }
    if (has(Schema::UiTrayIcon)) {
        Icon::InvalidateTrayIconCache();
        icon_status.reset();
    }
    if (has(Schema::UiLogLines)) {
        trimLogLines();
        rebuildLogView();
    }
    if (has(Schema::UiLogFont)) applyLogBrowserFont();
    if (has(Schema::UiMode)) applyUiMode();
    if (has(Schema::UiTray)) tray->setVisible(!settings->disable_tray);
    if (has(Schema::UiProfileList)) {
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
    if (has(Schema::UiShortcuts)) loadShortcuts();
}

void MainWindow::refreshTrayIcon() {
    Icon::InvalidateTrayIconCache();
    icon_status.reset();
    refresh_status();
}

void MainWindow::dialog_message_impl(MwMessage cmd, const QStringList &) {
    ENGINE_CHECK_MAIN_THREAD();
    switch (cmd) {
    case MwMessage::RestartProgram:
        exitController()->requestExit(ExitReason::Restart, false);
        break;
    case MwMessage::Raise:
        ActivateWindow(this);
        break;
    }
}
