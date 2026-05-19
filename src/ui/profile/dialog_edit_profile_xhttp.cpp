#include "include/ui/profile/dialog_edit_profile.h"

#include "include/configs/common/xrayStreamSetting.h"
#include "include/global/GuiUtils.hpp"

#include <QAbstractButton>
#include <QGridLayout>
#include <QLabel>

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
    const auto tooltip = tr("JSON: %1\n%2").arg(jsonKey, description);
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

    setXrayXHTTPHelp(ui->label_21, ui->xray_mode, tr("Mode"), "mode",
                     tr("XHTTP mode: auto usually uses packet-up, REALITY uses stream-one, "
                        "and REALITY with downloadSettings uses stream-up. downloadSettings "
                        "is removed when saving stream-one mode."));
    setXrayXHTTPHelp(ui->label_23, ui->xray_xpaddingbytes, tr("X Padding Bytes"), "xPaddingBytes",
                     tr("Range of extra XHTTP padding bytes. Default: 100-1000. If set, both bounds must be positive."));
    setXrayXHTTPHelp(ui->xray_serverMaxHeaderBytes_l, ui->xray_serverMaxHeaderBytes,
                     tr("Server Max Header Bytes"), "serverMaxHeaderBytes",
                     tr("Maximum request header size accepted by the server. Default: 8192."));
    setXrayXHTTPHelp(ui->xray_xpadding_obfs_mode, nullptr, tr("Enable Padding Obfuscation"),
                     "xPaddingObfsMode",
                     tr("Enable custom X-Padding placement, key, header, and method. When disabled, "
                        "the client uses Referer?...x_padding and the server uses X-Padding."));
    setXrayXHTTPHelp(ui->label_xpadding_method, ui->xray_xpadding_method,
                     tr("Padding Method"), "xPaddingMethod",
                     tr("Padding value format: repeat-x or tokenish. Default: repeat-x."));
    setXrayXHTTPHelp(ui->label_xpadding_placement, ui->xray_xpadding_placement,
                     tr("Padding Placement"), "xPaddingPlacement",
                     tr("Where X-Padding is sent: queryInHeader, cookie, header, or query. Default: queryInHeader."));
    setXrayXHTTPHelp(ui->label_xpadding_key, ui->xray_xpadding_key,
                     tr("Padding Key"), "xPaddingKey",
                     tr("Query or cookie key for X-Padding, and query key inside queryInHeader. Default: x_padding."));
    setXrayXHTTPHelp(ui->label_xpadding_header, ui->xray_xpadding_header,
                     tr("Padding Header"), "xPaddingHeader",
                     tr("Header name used by header or queryInHeader padding. Default: X-Padding."));

    setXrayXHTTPHelp(ui->label_24, ui->xray_scMaxEachPostBytes,
                     tr("Max Post Bytes"), "scMaxEachPostBytes",
                     tr("Packet-up upload POST size: client split size and server reject limit. Default: 1000000."));
    setXrayXHTTPHelp(ui->label_25, ui->xray_scMinPostsIntervalMs,
                     tr("Min Post Interval"), "scMinPostsIntervalMs",
                     tr("Packet-up client interval between upload POST requests per proxied connection, in milliseconds. Default: 30."));
    setXrayXHTTPHelp(ui->xray_scMaxBufferedPosts_l, ui->xray_scMaxBufferedPosts,
                     tr("Max Buffered Posts"), "scMaxBufferedPosts",
                     tr("Packet-up server upload queue size per proxied connection. Default: 30."));
    setXrayXHTTPHelp(ui->xray_uplink_http_method_l, ui->xray_uplink_http_method,
                     tr("Uplink HTTP Method"), "uplinkHTTPMethod",
                     tr("HTTP method for upload requests. Default: POST. Xray uppercases it; GET is accepted only in packet-up mode."));
    setXrayXHTTPHelp(ui->xray_uplink_data_placement_l, ui->xray_uplink_data_placement,
                     tr("Uplink Data Placement"), "uplinkDataPlacement",
                     tr("Where upload data is placed. Default: auto. cookie/header are accepted only in packet-up mode; auto/body are always accepted."));
    setXrayXHTTPHelp(ui->xray_uplink_data_key_l, ui->xray_uplink_data_key,
                     tr("Uplink Data Key"), "uplinkDataKey",
                     tr("Key used when upload data is placed in a cookie or header. Defaults: X-Data for auto/header, x_data for cookie."));
    setXrayXHTTPHelp(ui->xray_uplink_chunk_size_l, ui->xray_uplink_chunk_size,
                     tr("Uplink Chunk Size"), "uplinkChunkSize",
                     tr("Packet-up header/cookie payload chunk size range. Defaults: cookie 2-3 KiB, header 3-4 KB, otherwise scMaxEachPostBytes. Values below 64 are clamped."));

    setXrayXHTTPHelp(ui->xray_no_grpc, nullptr, tr("No GRPC Headers"), "noGRPCHeader",
                     tr("Client-side stream-up/stream-one option: do not add Content-Type: application/grpc to upload requests."));
    setXrayXHTTPHelp(ui->xray_no_sse, nullptr, tr("No SSE Headers"), "noSSEHeader",
                     tr("Server-side downstream/stream-one option: do not send Content-Type: text/event-stream in responses."));
    setXrayXHTTPHelp(ui->xray_scStreamUpServerSecs_l, ui->xray_scStreamUpServerSecs,
                     tr("Stream Up Server Seconds"), "scStreamUpServerSecs",
                     tr("Stream-up server interval for periodic xPaddingBytes keepalive writes, in seconds. Default: 20-80; values <= 0 disable periodic padding."));
    setXrayXHTTPHelp(ui->xray_session_placement_l, ui->xray_session_placement,
                     tr("Session Placement"), "sessionPlacement",
                     tr("Where the XHTTP session id is sent: path, cookie, header, or query. Default: path."));
    setXrayXHTTPHelp(ui->xray_session_key_l, ui->xray_session_key,
                     tr("Session Key"), "sessionKey",
                     tr("Key used for the session id outside path placement. Defaults: x_session for cookie/query, X-Session for header."));
    setXrayXHTTPHelp(ui->xray_seq_placement_l, ui->xray_seq_placement,
                     tr("Sequence Placement"), "seqPlacement",
                     tr("Where the XHTTP packet sequence is sent: path, cookie, header, or query. Default: path."));
    setXrayXHTTPHelp(ui->xray_seq_key_l, ui->xray_seq_key,
                     tr("Sequence Key"), "seqKey",
                     tr("Key used for the sequence value outside path placement. Defaults: x_seq for cookie/query, X-Seq for header."));

    setXrayXHTTPHelp(ui->label_26, ui->xray_max_concurrency,
                     tr("Max Concurrency"), "xmux.maxConcurrency",
                     tr("Client-side H2/H3 xmux limit: maximum concurrent uses per underlying connection. Cannot be used together with maxConnections. Empty xmux defaults to 1-1."));
    setXrayXHTTPHelp(ui->label_31, ui->xray_max_connections,
                     tr("Max Connections"), "xmux.maxConnections",
                     tr("Client-side H2/H3 xmux limit: maximum parallel underlying connections. Cannot be used together with maxConcurrency."));
    setXrayXHTTPHelp(ui->label_29, ui->xray_max_reuse_times,
                     tr("Max Reuse times"), "xmux.cMaxReuseTimes",
                     tr("Client-side H2/H3 xmux limit: maximum times an underlying connection may be selected for reuse."));
    setXrayXHTTPHelp(ui->label_27, ui->xray_hMaxRequestTimes,
                     tr("Max Request Times"), "xmux.hMaxRequestTimes",
                     tr("Client-side H2/H3 xmux limit: maximum upload/download requests per underlying connection. Empty xmux defaults to 600-900."));
    setXrayXHTTPHelp(ui->label_28, ui->xray_hMaxReusableSecs,
                     tr("Max Reusable Secs"), "xmux.hMaxReusableSecs",
                     tr("Client-side H2/H3 xmux limit: maximum seconds an underlying connection stays reusable. Empty xmux defaults to 1800-3000."));
    setXrayXHTTPHelp(ui->label_30, ui->xray_keep_alive_period,
                     tr("Keep Alive Period"), "xmux.hKeepAlivePeriod",
                     tr("Client-side H2/H3 keepalive interval for underlying connections, in seconds. 0 uses Xray defaults; negative values disable keepalive where supported."));
    setXrayXHTTPHelp(ui->label_32, ui->xray_downloadsettings_edit,
                     tr("Download Settings"), "downloadSettings",
                     tr("Client-only downstream streamSettings, including address and port, for an independent download path. Not allowed in stream-one and removed when saving stream-one mode."));
}

void DialogEditProfile::updateXrayXHTTPControls() {
    const auto obfsEnabled = ui->xray_xpadding_obfs_mode->isChecked();
    const auto showDownloadSettings = ui->xray_mode->currentText() != "stream-one";

    ui->xray_xhttp_packet_box->setVisible(true);
    ui->xray_xhttp_xmux_box->setVisible(true);
    ui->label_32->setVisible(showDownloadSettings);
    ui->xray_downloadsettings_edit->setVisible(showDownloadSettings);

    const QList<QWidget *> obfsWidgets = {
        ui->label_xpadding_method,    ui->xray_xpadding_method,
        ui->label_xpadding_placement, ui->xray_xpadding_placement,
        ui->label_xpadding_key,       ui->xray_xpadding_key,
        ui->label_xpadding_header,    ui->xray_xpadding_header,
    };
    for (auto widget : obfsWidgets) {
        widget->setEnabled(obfsEnabled);
        widget->setVisible(obfsEnabled);
    }
}

bool DialogEditProfile::validateXrayXHTTPSettings() {
    if (!ent->outbound->IsXray() || ui->xray_network->currentText() != "xhttp") return true;

    if (!ui->xray_max_connections->text().trimmed().isEmpty() &&
        !ui->xray_max_concurrency->text().trimmed().isEmpty()) {
        MessageBoxWarning(software_name,
                          tr("XHTTP maxConnections cannot be specified together with maxConcurrency."));
        return false;
    }

    return true;
}
