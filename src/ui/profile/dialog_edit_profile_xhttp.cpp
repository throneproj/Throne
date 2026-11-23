#include "include/ui/profile/dialog_edit_profile.h"

#include "include/configs/common/xrayStreamSetting.h"
#include "include/ui/utils/GuiUtils.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"

#include <QAbstractButton>
#include <QGridLayout>
#include <QLabel>

#include <climits>

namespace {
void setXHTTPGridStretch(QGridLayout *layout) {
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(3, 1);
}
}

void DialogEditProfile::setXrayXHTTPHelp(QWidget *caption, QWidget *field,
                                         const QString &text,
                                         const QString &jsonKey,
                                         const QString &description) {
    const auto tooltip = tr("%1: %2").arg(jsonKey, description);
    if (auto *label = qobject_cast<QLabel *>(caption)) {
        label->setText(text);
    } else if (auto *button = qobject_cast<QAbstractButton *>(caption)) {
        button->setText(text);
    }
    caption->setToolTip(tooltip);
    if (field) field->setToolTip(tooltip);
}

void DialogEditProfile::setupXrayXHTTPControls() {
    setXHTTPGridStretch(ui->gridLayout_xray_xhttp_common);
    setXHTTPGridStretch(ui->gridLayout_xray_xhttp_padding);
    setXHTTPGridStretch(ui->gridLayout_xray_xhttp_packet);
    setXHTTPGridStretch(ui->gridLayout_xray_xhttp_xmux);

    ui->verticalLayout_xray_xhttp->setAlignment(Qt::AlignTop);
    ui->xray_mode->addItems(Configs::XrayXHTTPModes);
    ui->xray_xpadding_method->addItems({"", "repeat-x", "tokenish"});
    ui->xray_xpadding_method->setEditable(true);
    ui->xray_xpadding_placement->addItems({"", "queryInHeader", "cookie", "header", "query"});
    ui->xray_xpadding_placement->setEditable(true);
    ui->xray_session_placement->addItems(Configs::XrayXHTTPMetaPlacements);
    ui->xray_session_placement->setEditable(true);
    ui->xray_seq_placement->addItems(Configs::XrayXHTTPMetaPlacements);
    ui->xray_seq_placement->setEditable(true);
    ui->xray_uplink_data_placement->addItems(Configs::XrayXHTTPUplinkDataPlacements);
    ui->xray_uplink_data_placement->setEditable(true);
    ui->xray_uplink_http_method->addItems(Configs::XrayXHTTPUplinkMethods);
    ui->xray_uplink_http_method->setEditable(true);
    setupXrayXHTTPDescriptions();
}

void DialogEditProfile::setupXrayXHTTPDescriptions() {
    ui->xray_xhttp_packet_box->setTitle(tr("Upload / Stream Tuning"));
    ui->xray_xhttp_xmux_box->setTitle(tr("Xmux Reuse"));

    setXrayXHTTPHelp(ui->xray_mode_l, ui->xray_mode, tr("Mode"), "mode",
                     tr("XHTTP mode; auto picks packet-up, or stream-one with REALITY."));
    setXrayXHTTPHelp(ui->xray_xpaddingbytes_l, ui->xray_xpaddingbytes, tr("X Padding Bytes"), "xPaddingBytes",
                     tr("Range of extra padding bytes. Default: 100-1000."));
    setXrayXHTTPHelp(ui->xray_serverMaxHeaderBytes_l, ui->xray_serverMaxHeaderBytes,
                     tr("Server Max Header Bytes"), "serverMaxHeaderBytes",
                     tr("Largest request header the server accepts. Default: 8192."));
    setXrayXHTTPHelp(ui->xray_xpadding_obfs_mode, nullptr, tr("Enable padding obfuscation"),
                     "xPaddingObfsMode",
                     tr("Customize the X-Padding method, placement, key and header."));
    setXrayXHTTPHelp(ui->label_xpadding_method, ui->xray_xpadding_method,
                     tr("Padding Method"), "xPaddingMethod",
                     tr("Padding value format. Default: repeat-x."));
    setXrayXHTTPHelp(ui->label_xpadding_placement, ui->xray_xpadding_placement,
                     tr("Padding Placement"), "xPaddingPlacement",
                     tr("Where X-Padding is sent. Default: queryInHeader."));
    setXrayXHTTPHelp(ui->label_xpadding_key, ui->xray_xpadding_key,
                     tr("Padding Key"), "xPaddingKey",
                     tr("Query or cookie key for X-Padding. Default: x_padding."));
    setXrayXHTTPHelp(ui->label_xpadding_header, ui->xray_xpadding_header,
                     tr("Padding Header"), "xPaddingHeader",
                     tr("Header name for X-Padding. Default: X-Padding."));

    setXrayXHTTPHelp(ui->xray_scMaxEachPostBytes_l, ui->xray_scMaxEachPostBytes,
                     tr("Max Post Bytes"), "scMaxEachPostBytes",
                     tr("Largest upload POST in packet-up mode. Default: 1000000."));
    setXrayXHTTPHelp(ui->xray_scMinPostsIntervalMs_l, ui->xray_scMinPostsIntervalMs,
                     tr("Min Post Interval"), "scMinPostsIntervalMs",
                     tr("Milliseconds between upload POSTs. Default: 30."));
    setXrayXHTTPHelp(ui->xray_scMaxBufferedPosts_l, ui->xray_scMaxBufferedPosts,
                     tr("Max Buffered Posts"), "scMaxBufferedPosts",
                     tr("Upload POSTs the server queues per connection. Default: 30."));
    setXrayXHTTPHelp(ui->xray_uplink_http_method_l, ui->xray_uplink_http_method,
                     tr("Uplink HTTP Method"), "uplinkHTTPMethod",
                     tr("HTTP method for upload requests. Default: POST."));
    setXrayXHTTPHelp(ui->xray_uplink_data_placement_l, ui->xray_uplink_data_placement,
                     tr("Uplink Data Placement"), "uplinkDataPlacement",
                     tr("Where upload data is placed. Default: auto."));
    setXrayXHTTPHelp(ui->xray_uplink_data_key_l, ui->xray_uplink_data_key,
                     tr("Uplink Data Key"), "uplinkDataKey",
                     tr("Upload data key. Default: X-Data, or x_data for cookie."));
    setXrayXHTTPHelp(ui->xray_uplink_chunk_size_l, ui->xray_uplink_chunk_size,
                     tr("Uplink Chunk Size"), "uplinkChunkSize",
                     tr("Chunk size range for upload data in headers or cookies."));

    setXrayXHTTPHelp(ui->xray_no_grpc, nullptr, tr("No gRPC headers"), "noGRPCHeader",
                     tr("Don't send Content-Type: application/grpc on uploads."));
    setXrayXHTTPHelp(ui->xray_no_sse, nullptr, tr("No SSE headers"), "noSSEHeader",
                     tr("Server doesn't send Content-Type: text/event-stream."));
    setXrayXHTTPHelp(ui->xray_scStreamUpServerSecs_l, ui->xray_scStreamUpServerSecs,
                     tr("Stream Up Server Seconds"), "scStreamUpServerSecs",
                     tr("Stream-up keepalive interval, in seconds. Default: 20-80."));
    setXrayXHTTPHelp(ui->xray_session_placement_l, ui->xray_session_placement,
                     tr("Session Placement"), "sessionIDPlacement",
                     tr("Where the session ID is sent. Default: path."));
    setXrayXHTTPHelp(ui->xray_session_key_l, ui->xray_session_key,
                     tr("Session Key"), "sessionIDKey",
                     tr("Session ID key. Default: x_session, or X-Session for header."));
    setXrayXHTTPHelp(ui->xray_session_id_table_l, ui->xray_session_id_table,
                     tr("Session ID Table"), "sessionIDTable",
                     tr("Session ID charset, e.g. hex or base36. Empty: random UUID."));
    setXrayXHTTPHelp(ui->xray_session_id_length_l, ui->xray_session_id_length,
                     tr("Session ID Length"), "sessionIDLength",
                     tr("Session ID length range, e.g. 8-16; needs sessionIDTable."));
    setXrayXHTTPHelp(ui->xray_seq_placement_l, ui->xray_seq_placement,
                     tr("Sequence Placement"), "seqPlacement",
                     tr("Where the packet sequence is sent. Default: path."));
    setXrayXHTTPHelp(ui->xray_seq_key_l, ui->xray_seq_key,
                     tr("Sequence Key"), "seqKey",
                     tr("Sequence key. Default: x_seq, or X-Seq for header."));

    setXrayXHTTPHelp(ui->xray_max_concurrency_l, ui->xray_max_concurrency,
                     tr("Max Concurrency"), "xmux.maxConcurrency",
                     tr("Most concurrent uses per connection. Empty xmux: 1-1."));
    setXrayXHTTPHelp(ui->xray_max_connections_l, ui->xray_max_connections,
                     tr("Max Connections"), "xmux.maxConnections",
                     tr("Most parallel underlying H2/H3 connections."));
    setXrayXHTTPHelp(ui->xray_max_reuse_times_l, ui->xray_max_reuse_times,
                     tr("Max Reuse times"), "xmux.cMaxReuseTimes",
                     tr("Most times one connection is reused."));
    setXrayXHTTPHelp(ui->xray_hMaxRequestTimes_l, ui->xray_hMaxRequestTimes,
                     tr("Max Request Times"), "xmux.hMaxRequestTimes",
                     tr("Most requests per connection. Empty xmux: 600-900."));
    setXrayXHTTPHelp(ui->xray_hMaxReusableSecs_l, ui->xray_hMaxReusableSecs,
                     tr("Max Reusable Secs"), "xmux.hMaxReusableSecs",
                     tr("Seconds a connection is reusable. Empty xmux: 1800-3000."));
    setXrayXHTTPHelp(ui->xray_keep_alive_period_l, ui->xray_keep_alive_period,
                     tr("Keep Alive Period"), "xmux.hKeepAlivePeriod",
                     tr("Keepalive seconds. 0: Xray default; below 0: off."));
    setXrayXHTTPHelp(ui->xray_downloadsettings_l, ui->xray_downloadsettings_edit,
                     tr("Download Settings"), "downloadSettings",
                     tr("Separate streamSettings, address and port for downloads."));
}

void DialogEditProfile::loadXrayXHTTP(const Configs::xrayXHTTP &xhttp) {
    ui->xray_xpaddingbytes->setText(xhttp.xPaddingBytes);
    ui->xray_serverMaxHeaderBytes->setText(Int2String(xhttp.serverMaxHeaderBytes));

    ui->xray_xpadding_obfs_mode->setChecked(xhttp.xPaddingObfsMode);
    ui->xray_xpadding_method->setCurrentText(xhttp.xPaddingMethod);
    ui->xray_xpadding_placement->setCurrentText(xhttp.xPaddingPlacement);
    ui->xray_xpadding_key->setText(xhttp.xPaddingKey);
    ui->xray_xpadding_header->setText(xhttp.xPaddingHeader);

    ui->xray_scMaxEachPostBytes->setText(xhttp.scMaxEachPostBytes);
    ui->xray_scMinPostsIntervalMs->setText(xhttp.scMinPostsIntervalMs);
    ui->xray_scMaxBufferedPosts->setText(Int2String(xhttp.scMaxBufferedPosts));
    ui->xray_uplink_http_method->setCurrentText(xhttp.uplinkHTTPMethod);
    ui->xray_uplink_data_placement->setCurrentText(xhttp.uplinkDataPlacement);
    ui->xray_uplink_data_key->setText(xhttp.uplinkDataKey);
    ui->xray_uplink_chunk_size->setText(xhttp.uplinkChunkSize);
    ui->xray_no_grpc->setChecked(xhttp.noGRPCHeader);
    ui->xray_no_sse->setChecked(xhttp.noSSEHeader);
    ui->xray_scStreamUpServerSecs->setText(xhttp.scStreamUpServerSecs);
    ui->xray_session_placement->setCurrentText(xhttp.sessionIDPlacement);
    ui->xray_session_key->setText(xhttp.sessionIDKey);
    ui->xray_session_id_table->setText(xhttp.sessionIDTable);
    ui->xray_session_id_length->setText(xhttp.sessionIDLength);
    ui->xray_seq_placement->setCurrentText(xhttp.seqPlacement);
    ui->xray_seq_key->setText(xhttp.seqKey);

    ui->xray_max_concurrency->setText(xhttp.maxConcurrency);
    ui->xray_max_connections->setText(xhttp.maxConnections);
    ui->xray_hMaxRequestTimes->setText(xhttp.hMaxRequestTimes);
    ui->xray_hMaxReusableSecs->setText(xhttp.hMaxReusableSecs);
    ui->xray_max_reuse_times->setText(xhttp.cMaxReuseTimes);
    ui->xray_keep_alive_period->setText(Int2String(xhttp.hKeepAlivePeriod));
}

void DialogEditProfile::saveXrayXHTTP(Configs::xrayXHTTP &xhttp) {
    xhttp.xPaddingBytes = ui->xray_xpaddingbytes->text().trimmed();
    xhttp.serverMaxHeaderBytes = ui->xray_serverMaxHeaderBytes->text().trimmed().toInt();

    xhttp.xPaddingObfsMode = ui->xray_xpadding_obfs_mode->isChecked();
    xhttp.xPaddingMethod = ui->xray_xpadding_method->currentText().trimmed();
    xhttp.xPaddingPlacement = ui->xray_xpadding_placement->currentText().trimmed();
    xhttp.xPaddingKey = ui->xray_xpadding_key->text().trimmed();
    xhttp.xPaddingHeader = ui->xray_xpadding_header->text().trimmed();

    xhttp.scMaxEachPostBytes = ui->xray_scMaxEachPostBytes->text().trimmed();
    xhttp.scMinPostsIntervalMs = ui->xray_scMinPostsIntervalMs->text().trimmed();
    xhttp.scMaxBufferedPosts = ui->xray_scMaxBufferedPosts->text().trimmed().toLongLong();
    xhttp.uplinkHTTPMethod = ui->xray_uplink_http_method->currentText().trimmed();
    xhttp.uplinkDataPlacement = ui->xray_uplink_data_placement->currentText().trimmed();
    xhttp.uplinkDataKey = ui->xray_uplink_data_key->text().trimmed();
    xhttp.uplinkChunkSize = ui->xray_uplink_chunk_size->text().trimmed();
    xhttp.noGRPCHeader = ui->xray_no_grpc->isChecked();
    xhttp.noSSEHeader = ui->xray_no_sse->isChecked();
    xhttp.scStreamUpServerSecs = ui->xray_scStreamUpServerSecs->text().trimmed();
    xhttp.sessionIDPlacement = ui->xray_session_placement->currentText().trimmed();
    xhttp.sessionIDKey = ui->xray_session_key->text().trimmed();
    xhttp.sessionIDTable = ui->xray_session_id_table->text().trimmed();
    xhttp.sessionIDLength = ui->xray_session_id_length->text().trimmed();
    xhttp.seqPlacement = ui->xray_seq_placement->currentText().trimmed();
    xhttp.seqKey = ui->xray_seq_key->text().trimmed();
    xhttp.downloadSettings = xhttp.mode == "stream-one" ? QString() : CACHE.XrayDownloadSettings;

    xhttp.maxConcurrency = ui->xray_max_concurrency->text().trimmed();
    xhttp.maxConnections = ui->xray_max_connections->text().trimmed();
    xhttp.hMaxRequestTimes = ui->xray_hMaxRequestTimes->text().trimmed();
    xhttp.hMaxReusableSecs = ui->xray_hMaxReusableSecs->text().trimmed();
    xhttp.cMaxReuseTimes = ui->xray_max_reuse_times->text().trimmed();
    xhttp.hKeepAlivePeriod = ui->xray_keep_alive_period->text().trimmed().toLongLong();
}

void DialogEditProfile::updateXrayXHTTPControls() {
    setRowVisible(ui->xray_downloadsettings_l, ui->xray_downloadsettings_edit,
                  ui->xray_mode->currentText() != "stream-one");

    const bool obfs = ui->xray_xpadding_obfs_mode->isChecked();
    setRowVisible(ui->label_xpadding_method, ui->xray_xpadding_method, obfs);
    setRowVisible(ui->label_xpadding_placement, ui->xray_xpadding_placement, obfs);
    setRowVisible(ui->label_xpadding_key, ui->xray_xpadding_key, obfs);
    setRowVisible(ui->label_xpadding_header, ui->xray_xpadding_header, obfs);
}

void DialogEditProfile::bindXrayXHTTP() {
    namespace Checks = Configs::Checks;
    const auto mode = [this] { return ui->xray_mode->currentText(); };

    form->bind(ui->xray_xpaddingbytes, Checks::XrayXPaddingBytes);
    form->bind(ui->xray_serverMaxHeaderBytes, Validation::Rules::Int(0, INT_MAX));
    form->bind(ui->xray_xpadding_method, Checks::XrayPaddingMethod);
    form->bind(ui->xray_xpadding_placement, Checks::XrayPaddingPlacement);
    // Xray checks both even with obfuscation off, while their rows are hidden.
    form->addRule({ui->xray_xpadding_obfs_mode}, [this] {
        if (ui->xray_xpadding_obfs_mode->isChecked()) return QString();
        if (Checks::XrayPaddingMethod(ui->xray_xpadding_method->currentText().trimmed()).isEmpty() &&
            Checks::XrayPaddingPlacement(ui->xray_xpadding_placement->currentText().trimmed()).isEmpty()) {
            return QString();
        }
        return tr("The padding method or placement is not supported, and Xray refuses it even with obfuscation off. "
                  "Turn this on and fix the value, or clear it.");
    });
    for (auto *edit: {ui->xray_scMaxEachPostBytes, ui->xray_scMinPostsIntervalMs, ui->xray_uplink_chunk_size,
                      ui->xray_scStreamUpServerSecs, ui->xray_session_id_length, ui->xray_max_concurrency,
                      ui->xray_max_connections, ui->xray_hMaxRequestTimes, ui->xray_hMaxReusableSecs, ui->xray_max_reuse_times}) {
        form->bind(edit, Checks::XrayInt32Range);
    }
    form->bind(ui->xray_scMaxBufferedPosts, Validation::Rules::Int(0, LLONG_MAX));
    form->bind(ui->xray_keep_alive_period, Validation::Rules::Int(LLONG_MIN, LLONG_MAX));
    form->bind(ui->xray_uplink_http_method, Validation::Rules::HttpToken()).warning();
    form->addRule({ui->xray_uplink_http_method, ui->xray_mode}, [this, mode] {
        return Checks::XrayUplinkMethod(ui->xray_uplink_http_method->currentText(), mode());
    });
    form->addRule({ui->xray_uplink_data_placement, ui->xray_mode}, [this, mode] {
        return Checks::XrayUplinkDataPlacement(ui->xray_uplink_data_placement->currentText(), mode());
    });
    form->bind(ui->xray_session_placement, Checks::XrayMetaPlacement);
    form->bind(ui->xray_seq_placement, Checks::XrayMetaPlacement);
    form->addRule({ui->xray_session_id_table, ui->xray_session_id_length}, [this] {
        return Checks::XraySessionId(ui->xray_session_id_table->text().trimmed(), ui->xray_session_id_length->text());
    });
    form->addRule({ui->xray_max_connections, ui->xray_max_concurrency}, [this] {
        return Checks::XrayXmuxExclusive(ui->xray_max_connections->text(), ui->xray_max_concurrency->text());
    });
}
