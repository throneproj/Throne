#include "include/ui/profile/dialog_edit_profile.h"

#include "include/configs/common/xrayStreamSetting.h"
#include "include/global/GuiUtils.hpp"

#include <QAbstractButton>
#include <QGridLayout>
#include <QLabel>
#include <QWidget>

namespace {
constexpr int kXHTTPWideColumnWidth = 760;

struct GridPlacement {
    QWidget *widget;
    int row;
    int column;
    int rowSpan = 1;
    int columnSpan = 1;
};

void moveGridWidget(QGridLayout *layout, QWidget *widget, int row, int column,
                    int rowSpan = 1, int columnSpan = 1) {
    layout->removeWidget(widget);
    layout->addWidget(widget, row, column, rowSpan, columnSpan);
}

void applyGridPlacements(QGridLayout *layout, std::initializer_list<GridPlacement> placements) {
    for (const auto& placement : placements) {
        moveGridWidget(layout, placement.widget, placement.row, placement.column,
                       placement.rowSpan, placement.columnSpan);
    }
}

void setPairGridStretch(QGridLayout *layout, bool wide) {
    layout->setColumnStretch(0, 0);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 0);
    layout->setColumnStretch(3, wide ? 1 : 0);
}

int xhttpNaturalWidth(QWidget *widget) {
    if (!widget || widget->isHidden()) return 0;
    if (auto *layout = widget->layout()) {
        layout->invalidate();
        layout->activate();
    }
    return widget->sizeHint()
        .expandedTo(widget->minimumSizeHint())
        .expandedTo(widget->minimumSize())
        .width();
}

bool xhttpControlsShown(const Ui::DialogEditProfile *ui) {
    return !ui->xray_widget->isHidden() &&
           !ui->xray_network_box->isHidden() &&
           !ui->xray_xhttp_box->isHidden();
}

int xhttpAvailableColumnWidth(const Ui::DialogEditProfile *ui) {
    const auto viewportWidth = ui->dialog_scroll_area->viewport()
                                   ? ui->dialog_scroll_area->viewport()->width()
                                   : 0;
    if (viewportWidth <= 0) return ui->xray_widget->width();

    const auto margins = ui->dialog_layout->contentsMargins();
    const auto spacing = qMax(0, ui->dialog_layout->spacing());
    int consumedWidth = margins.left() + margins.right();
    int visibleColumnsBeforeXray = 0;

    for (auto *column : {ui->left_w, ui->right_all_w}) {
        if (column->isHidden()) continue;
        consumedWidth += xhttpNaturalWidth(column);
        visibleColumnsBeforeXray++;
    }
    consumedWidth += visibleColumnsBeforeXray * spacing;
    return qMax(0, viewportWidth - consumedWidth);
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

bool DialogEditProfile::updateXrayXHTTPResponsiveLayout() {
    const auto isXHTTP = xhttpControlsShown(ui);
    const auto availableXrayWidth = xhttpAvailableColumnWidth(ui);
    const auto currentWideOverflows = xhttpLayoutInitialized && xhttpWideLayout &&
                                      xhttpNaturalWidth(ui->xray_xhttp_box) > availableXrayWidth;
    const bool wide = isXHTTP && isVisible() &&
                      availableXrayWidth >= kXHTTPWideColumnWidth &&
                      !currentWideOverflows;

    if (xhttpLayoutInitialized && wide == xhttpWideLayout) return false;
    xhttpLayoutInitialized = true;
    xhttpWideLayout = wide;

    auto *paramsLayout = ui->gridLayout_xray_xhttp_params;
    auto *paddingLayout = ui->gridLayout_xray_xhttp_padding;
    auto *xmuxLayout = ui->gridLayout_xray_xhttp_xmux;
    setPairGridStretch(paramsLayout, wide);
    setPairGridStretch(paddingLayout, wide);
    setPairGridStretch(xmuxLayout, wide);

    if (wide) {
        applyGridPlacements(paramsLayout, {
            {ui->xray_no_grpc, 0, 0, 1, 2}, {ui->xray_no_sse, 0, 2, 1, 2},
            {ui->label_21, 1, 0}, {ui->xray_mode, 1, 1},
            {ui->label_23, 1, 2}, {ui->xray_xpaddingbytes, 1, 3},
            {ui->xray_serverMaxHeaderBytes_l, 2, 0}, {ui->xray_serverMaxHeaderBytes, 2, 1},
            {ui->label_24, 2, 2}, {ui->xray_scMaxEachPostBytes, 2, 3},
            {ui->label_25, 3, 0}, {ui->xray_scMinPostsIntervalMs, 3, 1},
            {ui->xray_scMaxBufferedPosts_l, 3, 2}, {ui->xray_scMaxBufferedPosts, 3, 3},
            {ui->xray_uplink_http_method_l, 4, 0}, {ui->xray_uplink_http_method, 4, 1},
            {ui->xray_uplink_data_placement_l, 4, 2}, {ui->xray_uplink_data_placement, 4, 3},
            {ui->xray_uplink_data_key_l, 5, 0}, {ui->xray_uplink_data_key, 5, 1},
            {ui->xray_uplink_chunk_size_l, 5, 2}, {ui->xray_uplink_chunk_size, 5, 3},
            {ui->xray_scStreamUpServerSecs_l, 6, 0}, {ui->xray_scStreamUpServerSecs, 6, 1},
            {ui->xray_session_placement_l, 6, 2}, {ui->xray_session_placement, 6, 3},
            {ui->xray_session_key_l, 7, 0}, {ui->xray_session_key, 7, 1},
            {ui->xray_seq_placement_l, 7, 2}, {ui->xray_seq_placement, 7, 3},
            {ui->xray_seq_key_l, 8, 0}, {ui->xray_seq_key, 8, 1},
            {ui->label_32, 9, 0}, {ui->xray_downloadsettings_edit, 9, 1, 1, 3},
        });
        applyGridPlacements(paddingLayout, {
            {ui->xray_xpadding_obfs_mode, 0, 0, 1, 4},
            {ui->label_xpadding_method, 1, 0}, {ui->xray_xpadding_method, 1, 1},
            {ui->label_xpadding_placement, 1, 2}, {ui->xray_xpadding_placement, 1, 3},
            {ui->label_xpadding_key, 2, 0}, {ui->xray_xpadding_key, 2, 1},
            {ui->label_xpadding_header, 2, 2}, {ui->xray_xpadding_header, 2, 3},
        });
        applyGridPlacements(xmuxLayout, {
            {ui->label_26, 0, 0}, {ui->xray_max_concurrency, 0, 1},
            {ui->label_31, 0, 2}, {ui->xray_max_connections, 0, 3},
            {ui->label_27, 1, 0}, {ui->xray_hMaxRequestTimes, 1, 1},
            {ui->label_28, 1, 2}, {ui->xray_hMaxReusableSecs, 1, 3},
            {ui->label_29, 2, 0}, {ui->xray_max_reuse_times, 2, 1},
            {ui->label_30, 2, 2}, {ui->xray_keep_alive_period, 2, 3},
        });
    } else {
        applyGridPlacements(paramsLayout, {
            {ui->xray_no_grpc, 0, 0}, {ui->xray_no_sse, 0, 1},
            {ui->label_21, 1, 0}, {ui->xray_mode, 1, 1},
            {ui->label_23, 2, 0}, {ui->xray_xpaddingbytes, 2, 1},
            {ui->xray_serverMaxHeaderBytes_l, 3, 0}, {ui->xray_serverMaxHeaderBytes, 3, 1},
            {ui->label_24, 4, 0}, {ui->xray_scMaxEachPostBytes, 4, 1},
            {ui->label_25, 5, 0}, {ui->xray_scMinPostsIntervalMs, 5, 1},
            {ui->xray_scMaxBufferedPosts_l, 6, 0}, {ui->xray_scMaxBufferedPosts, 6, 1},
            {ui->xray_uplink_http_method_l, 7, 0}, {ui->xray_uplink_http_method, 7, 1},
            {ui->xray_uplink_data_placement_l, 8, 0}, {ui->xray_uplink_data_placement, 8, 1},
            {ui->xray_uplink_data_key_l, 9, 0}, {ui->xray_uplink_data_key, 9, 1},
            {ui->xray_uplink_chunk_size_l, 10, 0}, {ui->xray_uplink_chunk_size, 10, 1},
            {ui->xray_scStreamUpServerSecs_l, 11, 0}, {ui->xray_scStreamUpServerSecs, 11, 1},
            {ui->xray_session_placement_l, 12, 0}, {ui->xray_session_placement, 12, 1},
            {ui->xray_session_key_l, 13, 0}, {ui->xray_session_key, 13, 1},
            {ui->xray_seq_placement_l, 14, 0}, {ui->xray_seq_placement, 14, 1},
            {ui->xray_seq_key_l, 15, 0}, {ui->xray_seq_key, 15, 1},
            {ui->label_32, 16, 0}, {ui->xray_downloadsettings_edit, 16, 1},
        });
        applyGridPlacements(paddingLayout, {
            {ui->xray_xpadding_obfs_mode, 0, 0, 1, 2},
            {ui->label_xpadding_method, 1, 0}, {ui->xray_xpadding_method, 1, 1},
            {ui->label_xpadding_placement, 2, 0}, {ui->xray_xpadding_placement, 2, 1},
            {ui->label_xpadding_key, 3, 0}, {ui->xray_xpadding_key, 3, 1},
            {ui->label_xpadding_header, 4, 0}, {ui->xray_xpadding_header, 4, 1},
        });
        applyGridPlacements(xmuxLayout, {
            {ui->label_26, 0, 0}, {ui->xray_max_concurrency, 0, 1},
            {ui->label_31, 1, 0}, {ui->xray_max_connections, 1, 1},
            {ui->label_27, 2, 0}, {ui->xray_hMaxRequestTimes, 2, 1},
            {ui->label_28, 3, 0}, {ui->xray_hMaxReusableSecs, 3, 1},
            {ui->label_29, 4, 0}, {ui->xray_max_reuse_times, 4, 1},
            {ui->label_30, 5, 0}, {ui->xray_keep_alive_period, 5, 1},
        });
    }

    ui->xray_xhttp_params_box->updateGeometry();
    ui->xray_xhttp_padding_box->updateGeometry();
    ui->xray_xhttp_xmux_box->updateGeometry();
    ui->xray_xhttp_box->updateGeometry();
    ui->xray_widget->updateGeometry();
    return true;
}

void DialogEditProfile::updateXrayXHTTPControls() {
    const auto obfsEnabled = ui->xray_xpadding_obfs_mode->isChecked();
    const auto showDownloadSettings = ui->xray_mode->currentText() != "stream-one";

    ui->label_32->setVisible(showDownloadSettings);
    ui->xray_downloadsettings_edit->setVisible(showDownloadSettings);

    const QList<QWidget *> obfsWidgets = {
        ui->label_xpadding_method, ui->xray_xpadding_method,
        ui->label_xpadding_placement, ui->xray_xpadding_placement,
        ui->label_xpadding_key, ui->xray_xpadding_key,
        ui->label_xpadding_header, ui->xray_xpadding_header,
    };
    for (auto *widget : obfsWidgets) {
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
