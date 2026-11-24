#include "include/ui/profile/dialog_edit_profile.h"

#include "include/configs/common/utils.h"
#include "include/configs/common/xrayStreamSetting.h"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"
#include "include/ui/widget/json/JsonEditorDialog.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QJsonArray>
#include <QLabel>

namespace {
QString xhttpExtraIssues(const QJsonObject &extra, const QString &mode, Validation::Severity severity) {
    Validation::Issues issues;
    Configs::Checks::XrayXHTTPExtra(issues, {}, extra, mode);
    QStringList lines;
    for (const auto &issue: issues) {
        if (issue.severity == severity) lines << QCoreApplication::translate("Validation", "%1: %2").arg(issue.field, issue.message);
    }
    return lines.join('\n');
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
    ui->xray_mode->addItems(Configs::XrayXHTTPModes);
    setXrayXHTTPHelp(ui->xray_mode_l, ui->xray_mode, tr("Mode"), "mode",
                     tr("XHTTP mode; auto picks packet-up, or stream-one with REALITY."));
}

void DialogEditProfile::loadXrayXHTTP(const Configs::xrayXHTTP &xhttp) {
    CACHE.XrayXHTTPExtra = xhttp.ExportExtra();
    CACHE.XrayXHTTPExtraEdited = false;
}

void DialogEditProfile::saveXrayXHTTP(Configs::xrayXHTTP &xhttp) {
    if (CACHE.XrayXHTTPExtraEdited) xhttp.SetExtra(CACHE.XrayXHTTPExtra);
    if (xhttp.mode == "stream-one") xhttp.downloadSettings.clear();
}

QJsonObject DialogEditProfile::shownXrayXHTTPExtra() {
    auto extra = CACHE.XrayXHTTPExtra;
    if (ui->xray_mode->currentText() == "stream-one") extra.remove("downloadSettings");
    return extra;
}

void DialogEditProfile::bindXrayXHTTP() {
    using Validation::Severity;
    for (const auto severity: {Severity::Error, Severity::Warning}) {
        form->addRule({ui->xray_xhttp_extra_edit, ui->xray_mode}, [this, severity] {
            return xhttpExtraIssues(CACHE.XrayXHTTPExtra, ui->xray_mode->currentText(), severity);
        }, nullptr, severity);
    }
}

void DialogEditProfile::on_xray_xhttp_extra_edit_clicked() {
    const auto shown = shownXrayXHTTPExtra();
    auto editor = new JsonEdit::JsonEditorDialog(shown, this);
    // Client keys only: the server-only ones still round-trip but do nothing here.
    editor->SetPlaceholderText(tr("No extra settings are set. Available keys:") + QStringLiteral(
        "\nxPaddingBytes, xPaddingObfsMode, xPaddingKey, xPaddingHeader, xPaddingPlacement, xPaddingMethod, "
        "uplinkHTTPMethod, uplinkDataPlacement, uplinkDataKey, uplinkChunkSize, noGRPCHeader, "
        "sessionIDPlacement, sessionIDKey, sessionIDTable, sessionIDLength, seqPlacement, seqKey, "
        "scMaxEachPostBytes, scMinPostsIntervalMs, "
        "xmux: {maxConcurrency, maxConnections, cMaxReuseTimes, hMaxRequestTimes, hMaxReusableSecs, hKeepAlivePeriod}, "
        "downloadSettings"));
    auto extra = Configs::CanonicalXHTTPExtra(editor->OpenEditor());
    editor->deleteLater();
    // Headers have their own row.
    if (const auto headers = extra.take("headers"); headers.isObject() || headers.isArray()) {
        ui->xray_headers->setText(Configs::getHeadersString(headers.isObject() ? Configs::jsonObjectToQStringList(headers.toObject())
                                                                              : QJsonArray2QListString(headers.toArray())));
        form->revalidate(ui->xray_headers);
    }
    if (extra != shown) {
        // Stream-one hides downloadSettings without dropping it until the profile is saved.
        if (const auto hidden = CACHE.XrayXHTTPExtra.value("downloadSettings");
            !hidden.isUndefined() && !shown.contains("downloadSettings") && !extra.contains("downloadSettings")) {
            extra["downloadSettings"] = hidden;
        }
        CACHE.XrayXHTTPExtra = extra;
        CACHE.XrayXHTTPExtraEdited = true;
    }
    editor_cache_updated_impl();
    form->revalidate(ui->xray_xhttp_extra_edit);
}
