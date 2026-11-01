#include "include/ui/profile/dialog_edit_profile.h"

#include "include/configs/common/xrayStreamSetting.h"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"
#include "include/ui/widget/json/JsonEditorDialog.h"

#include <QRegularExpressionValidator>
#include <QSignalBlocker>

#include <array>

namespace {
struct XDriveTuningEdit {
    QLineEdit *edit;
    qint64 Configs::xrayXDrive::*field;
    const char *xrayDefault;
};

std::array<XDriveTuningEdit, 8> xdriveTuningEdits(const Ui::DialogEditProfile *ui) {
    return {{
        {ui->xray_xdrive_segment_bytes, &Configs::xrayXDrive::segmentBytes, "524288"},
        {ui->xray_xdrive_flush_interval, &Configs::xrayXDrive::flushIntervalMs, "20"},
        {ui->xray_xdrive_poll_interval, &Configs::xrayXDrive::pollIntervalMs, "50"},
        {ui->xray_xdrive_max_poll_interval, &Configs::xrayXDrive::maxPollIntervalMs, "500"},
        {ui->xray_xdrive_eager_window, &Configs::xrayXDrive::eagerWindowMs, "2000"},
        {ui->xray_xdrive_hole_timeout, &Configs::xrayXDrive::holeTimeoutMs, "30000"},
        {ui->xray_xdrive_session_ttl, &Configs::xrayXDrive::sessionTtlSeconds, "300"},
        {ui->xray_xdrive_concurrency, &Configs::xrayXDrive::concurrency, "8"},
    }};
}

QStringList xdriveSecretsFromUi(const Ui::DialogEditProfile *ui, const Configs::xrayXDrive &stored) {
    const auto service = ui->xray_xdrive_service->currentText();
    if (service == "template") return SplitAndTrim(ui->xray_xdrive_secrets->text(), ",", false);
    if (service == "s3") {
        QStringList keys = {ui->xray_xdrive_s3_access_key->text().trimmed(), ui->xray_xdrive_s3_secret_key->text().trimmed()};
        if (const auto token = ui->xray_xdrive_s3_session_token->text().trimmed(); !token.isEmpty()) keys << token;
        return keys;
    }
    if (service != "google-drive") return stored.secrets;
    // Xray takes exactly these three, so entries a list carried past them are dropped.
    return {ui->xray_xdrive_client_id->text().trimmed(), ui->xray_xdrive_client_secret->text().trimmed(),
            ui->xray_xdrive_refresh_token->text().trimmed()};
}
}

void DialogEditProfile::setupXrayXDriveControls() {
    for (auto *grid: {ui->gridLayout_xray_xdrive_storage, ui->gridLayout_xray_xdrive_tuning}) {
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(3, 1);
    }
    ui->verticalLayout_xray_xdrive->setAlignment(Qt::AlignTop);
    ui->xray_xdrive_service->addItems(Configs::XrayXDriveServices);

    auto *digits = new QRegularExpressionValidator(QRegularExpression("^[0-9]*$"), this);
    for (const auto &tuning: xdriveTuningEdits(ui)) {
        tuning.edit->setValidator(digits);
        tuning.edit->setPlaceholderText(tuning.xrayDefault);
    }
    ui->xray_xdrive_front_port->setValidator(digits);
    ui->xray_xdrive_front_port->setPlaceholderText("443");
    ui->xray_xdrive_s3_endpoint->setPlaceholderText("https://s3.example.com");
    ui->xray_xdrive_s3_region->setPlaceholderText("us-east-1");

    connect(ui->xray_xdrive_service, &QComboBox::currentTextChanged, this, [this] { relayout(); });
    setupXrayXDriveDescriptions();
}

void DialogEditProfile::setupXrayXDriveDescriptions() {
    ui->xray_xdrive_storage_box->setTitle(tr("Storage"));
    ui->xray_xdrive_tuning_box->setTitle(tr("Tuning"));

    setXrayXHTTPHelp(ui->xray_xdrive_service_l, ui->xray_xdrive_service, tr("Service"), "service",
                     tr("Where both ends exchange the stream: google-drive uses a Google Drive folder, s3 a bucket of any "
                        "S3-compatible storage, template any HTTP storage API described by Template, local a folder on this machine."));
    setXrayXHTTPHelp(ui->xray_xdrive_folder_l, ui->xray_xdrive_folder, tr("Folder"), "remoteFolder",
                     tr("google-drive: the folder ID. s3: the key prefix inside the bucket. local: a directory path. "
                        "template: read as {folder}."));
    setXrayXHTTPHelp(ui->xray_xdrive_client_id_l, ui->xray_xdrive_client_id, tr("Client ID"), "secrets[0]",
                     tr("The Google Drive OAuth client ID."));
    setXrayXHTTPHelp(ui->xray_xdrive_client_secret_l, ui->xray_xdrive_client_secret, tr("Client Secret"), "secrets[1]",
                     tr("The Google Drive OAuth client secret."));
    setXrayXHTTPHelp(ui->xray_xdrive_refresh_token_l, ui->xray_xdrive_refresh_token, tr("Refresh Token"), "secrets[2]",
                     tr("The OAuth refresh token for the Google account that owns the folder."));
    setXrayXHTTPHelp(ui->xray_xdrive_s3_endpoint_l, ui->xray_xdrive_s3_endpoint, tr("Endpoint"), "s3.endpoint",
                     tr("The S3 API URL, such as https://s3.us-east-1.amazonaws.com or a MinIO or R2 endpoint."));
    setXrayXHTTPHelp(ui->xray_xdrive_s3_region_l, ui->xray_xdrive_s3_region, tr("Region"), "s3.region",
                     tr("The region requests are signed for. Default: us-east-1."));
    setXrayXHTTPHelp(ui->xray_xdrive_s3_bucket_l, ui->xray_xdrive_s3_bucket, tr("Bucket"), "s3.bucket",
                     tr("The bucket both ends share."));
    setXrayXHTTPHelp(ui->xray_xdrive_s3_path_style, nullptr, tr("Path-Style Addressing"), "s3.pathStyle",
                     tr("Puts the bucket in the URL path. When off, the bucket becomes part of the endpoint's host name, "
                        "which needs a DNS endpoint and a DNS-compatible bucket name. Default: on."));
    setXrayXHTTPHelp(ui->xray_xdrive_s3_access_key_l, ui->xray_xdrive_s3_access_key, tr("Access Key ID"), "secrets[0]",
                     tr("The S3 access key ID."));
    setXrayXHTTPHelp(ui->xray_xdrive_s3_secret_key_l, ui->xray_xdrive_s3_secret_key, tr("Secret Access Key"), "secrets[1]",
                     tr("The S3 secret access key."));
    setXrayXHTTPHelp(ui->xray_xdrive_s3_session_token_l, ui->xray_xdrive_s3_session_token, tr("Session Token"), "secrets[2]",
                     tr("Optional: the session token of temporary credentials."));
    setXrayXHTTPHelp(ui->xray_xdrive_secrets_l, ui->xray_xdrive_secrets, tr("Secrets"), "secrets",
                     tr("Values the template reads as {secret0}, {secret1}, …, separated by commas."));
    setXrayXHTTPHelp(ui->xray_xdrive_template_l, ui->xray_xdrive_template_edit, tr("Template"), "template",
                     tr("The storage API's auth, put, get, list and delete operations, as JSON."));
    setXrayXHTTPHelp(ui->xray_xdrive_front_l, ui->xray_xdrive_front, tr("Front Address"), "address",
                     tr("Optional domain front: the storage API is dialed at this address while TLS still names the API host."));
    setXrayXHTTPHelp(ui->xray_xdrive_front_port_l, ui->xray_xdrive_front_port, tr("Front Port"), "port",
                     tr("Port of the front address. Default: 443."));

    setXrayXHTTPHelp(ui->xray_xdrive_segment_bytes_l, ui->xray_xdrive_segment_bytes, tr("Segment Bytes"), "segmentBytes",
                     tr("Largest upload per file, in bytes. Default: 524288; Xray caps it at 16 MiB."));
    setXrayXHTTPHelp(ui->xray_xdrive_flush_interval_l, ui->xray_xdrive_flush_interval, tr("Flush Interval"), "flushIntervalMs",
                     tr("How long writes are gathered before an upload, in milliseconds. Default: 20."));
    setXrayXHTTPHelp(ui->xray_xdrive_poll_interval_l, ui->xray_xdrive_poll_interval, tr("Poll Interval"), "pollIntervalMs",
                     tr("Shortest wait between checks for new files, in milliseconds. Default: 50."));
    setXrayXHTTPHelp(ui->xray_xdrive_max_poll_interval_l, ui->xray_xdrive_max_poll_interval, tr("Max Poll Interval"),
                     "maxPollIntervalMs",
                     tr("Longest wait between checks while the stream is idle, in milliseconds. Default: 500."));
    setXrayXHTTPHelp(ui->xray_xdrive_eager_window_l, ui->xray_xdrive_eager_window, tr("Eager Window"), "eagerWindowMs",
                     tr("How long after traffic polling stays at the shortest wait, in milliseconds. Default: 2000."));
    setXrayXHTTPHelp(ui->xray_xdrive_hole_timeout_l, ui->xray_xdrive_hole_timeout, tr("Hole Timeout"), "holeTimeoutMs",
                     tr("How long a missing segment is waited for before the stream fails, in milliseconds. Default: 30000."));
    setXrayXHTTPHelp(ui->xray_xdrive_session_ttl_l, ui->xray_xdrive_session_ttl, tr("Session TTL"), "sessionTtlSeconds",
                     tr("How long an idle session lives before its files are cleaned up, in seconds. Default: 300."));
    setXrayXHTTPHelp(ui->xray_xdrive_concurrency_l, ui->xray_xdrive_concurrency, tr("Concurrency"), "concurrency",
                     tr("Parallel storage requests per stream. Default: 8; Xray caps it at 64."));
}

void DialogEditProfile::loadXrayXDrive(const Configs::xrayXDrive &xdrive) {
    {
        const QSignalBlocker blocker(ui->xray_xdrive_service);
        keepXrayComboValue(ui->xray_xdrive_service, xdrive.service);
        selectComboText(ui->xray_xdrive_service, xdrive.service);
    }
    ui->xray_xdrive_folder->setText(xdrive.remoteFolder);
    ui->xray_xdrive_client_id->setText(xdrive.secrets.value(0));
    ui->xray_xdrive_client_secret->setText(xdrive.secrets.value(1));
    ui->xray_xdrive_refresh_token->setText(xdrive.secrets.value(2));
    ui->xray_xdrive_s3_endpoint->setText(xdrive.s3Endpoint);
    ui->xray_xdrive_s3_region->setText(xdrive.s3Region);
    ui->xray_xdrive_s3_bucket->setText(xdrive.s3Bucket);
    ui->xray_xdrive_s3_path_style->setChecked(xdrive.s3PathStyle);
    ui->xray_xdrive_s3_access_key->setText(xdrive.secrets.value(0));
    ui->xray_xdrive_s3_secret_key->setText(xdrive.secrets.value(1));
    ui->xray_xdrive_s3_session_token->setText(xdrive.secrets.value(2));
    ui->xray_xdrive_secrets->setText(xdrive.secrets.join(", "));
    ui->xray_xdrive_front->setText(xdrive.frontAddress);
    ui->xray_xdrive_front_port->setText(xdrive.frontPort == 0 ? QString() : QString::number(xdrive.frontPort));
    for (const auto &tuning: xdriveTuningEdits(ui)) {
        const auto value = xdrive.*tuning.field;
        tuning.edit->setText(value == 0 ? QString() : QString::number(value));
    }
}

void DialogEditProfile::saveXrayXDrive(Configs::xrayXDrive &xdrive) {
    xdrive.secrets = xdriveSecretsFromUi(ui, xdrive);
    xdrive.service = ui->xray_xdrive_service->currentText().trimmed();
    xdrive.remoteFolder = ui->xray_xdrive_folder->text().trimmed();
    xdrive.templateSpec = CACHE.XrayXDriveTemplate;
    xdrive.s3Endpoint = ui->xray_xdrive_s3_endpoint->text().trimmed();
    xdrive.s3Region = ui->xray_xdrive_s3_region->text().trimmed();
    xdrive.s3Bucket = ui->xray_xdrive_s3_bucket->text().trimmed();
    xdrive.s3PathStyle = ui->xray_xdrive_s3_path_style->isChecked();
    xdrive.frontAddress = ui->xray_xdrive_front->text().trimmed();
    xdrive.frontPort = ui->xray_xdrive_front_port->text().trimmed().toInt();
    for (const auto &tuning: xdriveTuningEdits(ui)) {
        xdrive.*tuning.field = tuning.edit->text().trimmed().toLongLong();
    }
}

void DialogEditProfile::updateXrayXDriveControls() {
    const auto service = ui->xray_xdrive_service->currentText();
    const bool googleDrive = service == "google-drive";
    const bool s3 = service == "s3";
    const bool storageTemplate = service == "template";
    const bool local = service == "local";
    setRowVisible(ui->xray_xdrive_client_id_l, ui->xray_xdrive_client_id, googleDrive);
    setRowVisible(ui->xray_xdrive_client_secret_l, ui->xray_xdrive_client_secret, googleDrive);
    setRowVisible(ui->xray_xdrive_refresh_token_l, ui->xray_xdrive_refresh_token, googleDrive);
    setRowVisible(ui->xray_xdrive_s3_endpoint_l, ui->xray_xdrive_s3_endpoint, s3);
    setRowVisible(ui->xray_xdrive_s3_region_l, ui->xray_xdrive_s3_region, s3);
    setRowVisible(ui->xray_xdrive_s3_bucket_l, ui->xray_xdrive_s3_bucket, s3);
    ui->xray_xdrive_s3_path_style->setVisible(s3);
    setRowVisible(ui->xray_xdrive_s3_access_key_l, ui->xray_xdrive_s3_access_key, s3);
    setRowVisible(ui->xray_xdrive_s3_secret_key_l, ui->xray_xdrive_s3_secret_key, s3);
    setRowVisible(ui->xray_xdrive_s3_session_token_l, ui->xray_xdrive_s3_session_token, s3);
    setRowVisible(ui->xray_xdrive_secrets_l, ui->xray_xdrive_secrets, storageTemplate);
    setRowVisible(ui->xray_xdrive_template_l, ui->xray_xdrive_template_edit, storageTemplate);
    setRowVisible(ui->xray_xdrive_front_l, ui->xray_xdrive_front, !local);
    setRowVisible(ui->xray_xdrive_front_port_l, ui->xray_xdrive_front_port, !local);
    ui->xray_xdrive_folder->setPlaceholderText(googleDrive ? tr("Google Drive folder ID")
                                               : s3        ? tr("Key prefix in the bucket")
                                               : local     ? tr("A directory path")
                                                           : QString());
}

void DialogEditProfile::bindXrayXDrive() {
    using Validation::Severity;
    namespace Checks = Configs::Checks;
    const auto service = [this] { return ui->xray_xdrive_service->currentText(); };

    form->addRule({ui->xray_xdrive_service}, [service] { return Checks::XrayXDriveService(service()); });
    form->addRule({ui->xray_xdrive_folder, ui->xray_xdrive_service}, [this, service] {
        return Checks::XrayXDriveFolder(service(), ui->xray_xdrive_folder->text());
    });
    form->addRule({ui->xray_xdrive_client_id, ui->xray_xdrive_client_secret, ui->xray_xdrive_refresh_token,
                   ui->xray_xdrive_s3_access_key, ui->xray_xdrive_s3_secret_key, ui->xray_xdrive_s3_session_token,
                   ui->xray_xdrive_secrets, ui->xray_xdrive_service}, [this, service] {
        if (!ent->outbound->HasXrayStream()) return QString();
        return Checks::XrayXDriveSecrets(service(), xdriveSecretsFromUi(ui, *ent->outbound->GetXrayStream()->xdrive));
    });
    form->addRule({ui->xray_xdrive_template_edit, ui->xray_xdrive_service}, [this, service] {
        return Checks::XrayXDriveTemplate(service(), CACHE.XrayXDriveTemplate);
    });
    form->addRule({ui->xray_xdrive_s3_endpoint, ui->xray_xdrive_service}, [this, service] {
        return Checks::XrayXDriveS3Endpoint(service(), ui->xray_xdrive_s3_endpoint->text());
    });
    form->addRule({ui->xray_xdrive_s3_region, ui->xray_xdrive_service}, [this, service] {
        return Checks::XrayXDriveS3Region(service(), ui->xray_xdrive_s3_region->text().trimmed());
    });
    form->addRule({ui->xray_xdrive_s3_bucket, ui->xray_xdrive_s3_path_style, ui->xray_xdrive_s3_endpoint, ui->xray_xdrive_service},
                  [this, service] {
        return Checks::XrayXDriveS3Bucket(service(), ui->xray_xdrive_s3_bucket->text().trimmed(),
                                          ui->xray_xdrive_s3_endpoint->text(), ui->xray_xdrive_s3_path_style->isChecked());
    });
    for (const auto &tuning: xdriveTuningEdits(ui)) form->bind(tuning.edit, Checks::XrayXDriveTuning);
    form->bind(ui->xray_xdrive_front_port, Checks::XrayXDriveFrontPort);
    form->addRule({ui->xray_xdrive_front_port, ui->xray_xdrive_front}, [this] {
        return Checks::XrayXDriveFrontPortAlone(ui->xray_xdrive_front->text(), ui->xray_xdrive_front_port->text());
    }, nullptr, Severity::Warning);
}

void DialogEditProfile::on_xray_xdrive_template_edit_clicked() {
    auto editor = new JsonEdit::JsonEditorDialog(CACHE.XrayXDriveTemplate, this);
    CACHE.XrayXDriveTemplate = editor->OpenEditor();
    editor->deleteLater();
    editor_cache_updated_impl();
    form->revalidate(ui->xray_xdrive_template_edit);
}
