#include "include/ui/group/dialog_edit_group_advanced.h"

#include "include/configs/sub/GroupUpdater.hpp"
#include "include/database/DatabaseManager.h"
#include "include/database/SettingsRepo.h"
#include "include/global/GuiUtils.hpp"

#include <QGuiApplication>
#include <QScreen>
#include <QSslSocket>
#include <QStandardItemModel>
#include <QStyle>

namespace {
    // Index 0 of an override combo is Keep Default; the enum values follow.
    template <typename E>
    int overrideIndex(const std::optional<E> &value) {
        return value ? static_cast<int>(*value) + 1 : 0;
    }

    template <typename E>
    std::optional<E> overrideAt(int index) {
        if (index <= 0) return std::nullopt;
        return static_cast<E>(index - 1);
    }
}

DialogEditGroupAdvanced::DialogEditGroupAdvanced(const Configs::SubscriptionOptions &options, int serverIntervalHours, QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogEditGroupAdvanced), options(options) {
    ui->setupUi(this);

    const auto &settings = Configs::dataManager->settingsRepo;
    ui->update_interval->setSuffix(tr(" min"));
    ui->update_interval->setSpecialValueText(
        tr("Keep Default (%1)").arg(settings->sub_auto_update >= 30 ? tr("%1 min").arg(settings->sub_auto_update) : tr("Off")));
    ui->update_interval->setValue(options.update_interval);
    ui->server_interval->setItemText(0, tr("Keep Default (%1)").arg(ui->server_interval->itemText(settings->sub_respect_server_interval ? 1 : 2)));
    ui->server_interval->setCurrentIndex(options.respect_server_interval ? (*options.respect_server_interval ? 1 : 2) : 0);
    if (serverIntervalHours > 0) {
        const auto tip = ui->server_interval_l->toolTip() + "\n\n" + tr("This server last sent %1 h.").arg(serverIntervalHours);
        ui->server_interval_l->setToolTip(tip);
        ui->server_interval->setToolTip(tip);
    }

    const auto defaults = Subscription::ResolveIdentity(nullptr);
    globalSendHwid = defaults.sendHwid;
    ui->send_hwid->setItemText(static_cast<int>(Configs::sendHwid::keepDefault),
                               tr("Keep Default (%1)").arg(globalSendHwid ? tr("On") : tr("Off")));
    ui->tls_version->setItemText(0, tr("Keep Default (%1)").arg(ui->tls_version->itemText(overrideIndex(std::optional(defaults.tlsVersion)))));
    ui->http_version->setItemText(0, tr("Keep Default (%1)").arg(ui->http_version->itemText(overrideIndex(std::optional(defaults.httpVersion)))));
    if (auto *model = qobject_cast<QStandardItemModel *>(ui->tls_version->model()); model && !QSslSocket::isProtocolSupported(QSsl::TlsV1_3)) {
        model->item(overrideIndex(std::optional(Configs::subTlsVersion::tls13)))->setEnabled(false);
    }
    ui->user_agent->setPlaceholderText(defaults.userAgent);
    ui->hwid->setPlaceholderText(defaults.device.hwid);
    ui->hwid_os->setPlaceholderText(defaults.device.os);
    ui->hwid_os_version->setPlaceholderText(defaults.device.osVersion);
    ui->hwid_model->setPlaceholderText(defaults.device.model);

    ui->user_agent->setText(options.user_agent);
    ui->tls_version->setCurrentIndex(overrideIndex(options.tls_version));
    ui->http_version->setCurrentIndex(overrideIndex(options.http_version));
    ui->send_hwid->setCurrentIndex(static_cast<int>(options.send_hwid));
    ui->hwid->setText(options.hwid);
    ui->hwid_os->setText(options.hwid_os);
    ui->hwid_os_version->setText(options.hwid_os_version);
    ui->hwid_model->setText(options.hwid_model);
    ui->keep_working->setChecked(options.keep_working);
    ui->remove_duplicates->setChecked(options.remove_duplicates);
    ui->remove_insecure->setChecked(options.remove_insecure);
    ui->remove_invalid->setChecked(options.remove_invalid);
    ui->url_test->setChecked(options.url_test);
    ui->remove_unavailable->setChecked(options.remove_unavailable);
    ui->sort_by_latency->setChecked(options.sort_by_latency);

    connect(ui->send_hwid, &QComboBox::currentIndexChanged, this, [this] { syncHwidFields(); });
    syncHwidFields();

    const auto *checkStyle = ui->url_test->style();
    ui->url_test_follow_ups->setContentsMargins(checkStyle->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, ui->url_test)
                                                    + checkStyle->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, nullptr, ui->url_test),
                                                0, 0, 0);
    connect(ui->url_test, &QCheckBox::toggled, this, [this] { syncUrlTestFollowUps(); });
    syncUrlTestFollowUps();

    ADD_ASTERISK(this)

    // adjustSize() clamps to 2/3 of the screen.
    const auto *scr = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
    if (scr != nullptr) resize(sizeHint().boundedTo(scr->availableGeometry().size()));
}

DialogEditGroupAdvanced::~DialogEditGroupAdvanced() {
    delete ui;
}

void DialogEditGroupAdvanced::syncHwidFields() {
    const auto mode = static_cast<Configs::sendHwid>(ui->send_hwid->currentIndex());
    const bool sent = mode == Configs::sendHwid::on || (mode == Configs::sendHwid::keepDefault && globalSendHwid);
    const QList<QWidget *> fields{ui->hwid_l, ui->hwid, ui->hwid_os_l, ui->hwid_os,
                                  ui->hwid_os_version_l, ui->hwid_os_version, ui->hwid_model_l, ui->hwid_model};
    for (auto *field : fields) field->setEnabled(sent);
}

void DialogEditGroupAdvanced::syncUrlTestFollowUps() {
    const bool tested = ui->url_test->isChecked();
    ui->remove_unavailable->setEnabled(tested);
    ui->sort_by_latency->setEnabled(tested);
}

void DialogEditGroupAdvanced::accept() {
    options.user_agent = ui->user_agent->text().trimmed();
    options.tls_version = overrideAt<Configs::subTlsVersion>(ui->tls_version->currentIndex());
    options.http_version = overrideAt<Configs::subHttpVersion>(ui->http_version->currentIndex());
    options.send_hwid = static_cast<Configs::sendHwid>(ui->send_hwid->currentIndex());
    options.hwid = ui->hwid->text().trimmed();
    options.hwid_os = ui->hwid_os->text().trimmed();
    options.hwid_os_version = ui->hwid_os_version->text().trimmed();
    options.hwid_model = ui->hwid_model->text().trimmed();
    // The same 30-minute floor the global interval enforces.
    const int minutes = ui->update_interval->value();
    options.update_interval = minutes > 0 ? std::max(minutes, 30) : 0;
    const int serverInterval = ui->server_interval->currentIndex();
    options.respect_server_interval = serverInterval > 0 ? std::optional<bool>(serverInterval == 1) : std::nullopt;
    options.keep_working = ui->keep_working->isChecked();
    options.remove_duplicates = ui->remove_duplicates->isChecked();
    options.remove_insecure = ui->remove_insecure->isChecked();
    options.remove_invalid = ui->remove_invalid->isChecked();
    options.url_test = ui->url_test->isChecked();
    options.remove_unavailable = ui->remove_unavailable->isChecked();
    options.sort_by_latency = ui->sort_by_latency->isChecked();
    QDialog::accept();
}
