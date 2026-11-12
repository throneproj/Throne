#include "include/ui/setting/dialog_preset_settings.h"

#include "include/ui/utils/GuiUtils.hpp"
#include "include/global/Configs.hpp"
#include "include/global/Validation.hpp"
#include "include/configs/common/Outbound.h"
#include "include/configs/common/QUICFields.h"
#include "include/ui/utils/ValidationForm.h"

#include <QRegularExpression>
#include <QScreen>
#include <QValidator>

namespace {
    // 0 means "keep the core default" and is omitted from the config, so show it as blank.
    void LoadOptionalInt(QLineEdit *edit, const int value, QObject *parent) {
        edit->setText(value > 0 ? Int2String(value) : "");
        edit->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9]+$"), parent));
    }
}

DialogPresetSettings::DialogPresetSettings(QWidget *parent) : QDialog(parent), ui(new Ui::DialogPresetSettings) {
    ui->setupUi(this);
    ADD_ASTERISK(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);

    ui->mux_singbox->setSettings(Configs::MuxSettings::Preset());
    D_LOAD_BOOL(mux_default_on)
    ui->mux_xray->setSettings(Configs::XrayMuxSettings::Preset());
    ui->xray_default_mux->setChecked(Configs::dataManager->settingsRepo->xray_mux_default_on);

    ui->fragment_implementation->addItems({"built-in", "custom"});
    D_LOAD_COMBO_STRING(fragment_implementation)
    D_LOAD_STRING(fragment_size)
    D_LOAD_STRING(fragment_sleep)
    D_LOAD_BOOL(fragment_default_on)
    D_LOAD_BOOL(tls_tricks_default_on)
    ui->fragment_size->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9]+(-[0-9]+)?$"), this));
    ui->fragment_sleep->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9]+(-[0-9]+)?$"), this));
    auto syncFragParams = [this](const QString &impl) {
        bool custom = impl == "custom";
        ui->fragment_size->setEnabled(custom);
        ui->fragment_sleep->setEnabled(custom);
        ui->fragment_size_l->setEnabled(custom);
        ui->fragment_sleep_l->setEnabled(custom);
    };
    connect(ui->fragment_implementation, &QComboBox::currentTextChanged, this, syncFragParams);
    syncFragParams(ui->fragment_implementation->currentText());
    ui->utlsFingerprint->addItems(Configs::tlsFingerprints);
    ui->utlsFingerprint->setCurrentText(Configs::dataManager->settingsRepo->utlsFingerprint);
    // a non-editable combo drops setCurrentText while it holds no items
    ui->tls_spoof_method->addItems(Configs::tlsSpoofMethods);
    D_LOAD_STRING(tls_spoof)
    D_LOAD_COMBO_STRING(tls_spoof_method)
    D_LOAD_BOOL(tls_spoof_default_on)
    auto syncSpoofFields = [this](const QString &sni) {
        ui->tls_spoof_method->setEnabled(!sni.isEmpty());
        ui->tls_spoof_method_l->setEnabled(!sni.isEmpty());
        ui->tls_spoof_default_on->setEnabled(!sni.isEmpty());
    };
    connect(ui->tls_spoof, &QLineEdit::textChanged, this, syncSpoofFields);
    syncSpoofFields(ui->tls_spoof->text());

    D_LOAD_STRING(h2_idle_timeout)
    D_LOAD_STRING(h2_keep_alive_period)
    D_LOAD_STRING(h2_stream_receive_window)
    D_LOAD_STRING(h2_connection_receive_window)
    LoadOptionalInt(ui->h2_max_concurrent_streams, Configs::dataManager->settingsRepo->h2_max_concurrent_streams, this);
    LoadOptionalInt(ui->quic_initial_packet_size, Configs::dataManager->settingsRepo->quic_initial_packet_size, this);
    D_LOAD_BOOL(quic_disable_path_mtu_discovery)

    using namespace Validation;
    const auto boxLabel = [](const QGroupBox *box, const QLabel *label) { return QStringLiteral("%1 %2").arg(box->title(), label->text()); };
    ui->mux_singbox->bindValidation(form, ui->mux_singbox_box->title());
    ui->mux_xray->bindValidation(form, ui->mux_xray_box->title());
    const QString fragmentSizeLabel = boxLabel(ui->tls_fragment_box, ui->fragment_size_l);
    form->bind(ui->fragment_size, Configs::Checks::TlsFragmentSize).label(fragmentSizeLabel);
    form->bind(ui->fragment_size, Configs::Checks::TlsFragmentSizeLarge).label(fragmentSizeLabel).warning();
    form->bind(ui->fragment_sleep, Configs::Checks::TlsFragmentSleep).label(boxLabel(ui->tls_fragment_box, ui->fragment_sleep_l));
    form->bind(ui->utlsFingerprint, Rules::OneOf(Configs::tlsFingerprints)).label(ui->utlsFingerprint_l->text());
    form->bind(ui->tls_spoof, Rules::Host(Empty::Allowed)).label(ui->tls_spoof_l->text()).warning();
    form->bind(ui->h2_idle_timeout, Rules::Duration());
    form->bind(ui->h2_keep_alive_period, Rules::Duration());
    form->bind(ui->h2_stream_receive_window, Rules::MemoryBytes(true));
    form->bind(ui->h2_connection_receive_window, Rules::MemoryBytes(true));
    form->bind(ui->quic_initial_packet_size, Configs::Checks::QuicInitialPacketSize);
    form->markAll();

    QSize want = sizeHint();
    if (const QScreen *scr = parent ? parent->screen() : screen()) {
        const QRect avail = scr->availableGeometry();
        want = want.boundedTo(QSize(avail.width() - 24, avail.height() - 72));
    }
    resize(want);
}

DialogPresetSettings::~DialogPresetSettings() {
    delete ui;
}

void DialogPresetSettings::accept() {
    if (!form->validateAll()) return;
    ui->mux_singbox->settings().SaveAsPreset();
    D_SAVE_BOOL(mux_default_on)
    ui->mux_xray->settings().SaveAsPreset();
    Configs::dataManager->settingsRepo->xray_mux_default_on = ui->xray_default_mux->isChecked();

    D_SAVE_COMBO_STRING(fragment_implementation)
    Configs::dataManager->settingsRepo->fragment_size = ui->fragment_size->text().trimmed();
    Configs::dataManager->settingsRepo->fragment_sleep = ui->fragment_sleep->text().trimmed();
    D_SAVE_BOOL(fragment_default_on)
    D_SAVE_BOOL(tls_tricks_default_on)
    Configs::dataManager->settingsRepo->utlsFingerprint = ui->utlsFingerprint->currentText().trimmed();
    Configs::dataManager->settingsRepo->tls_spoof = ui->tls_spoof->text().trimmed();
    D_SAVE_COMBO_STRING(tls_spoof_method)
    D_SAVE_BOOL(tls_spoof_default_on)

    Configs::dataManager->settingsRepo->h2_idle_timeout = ui->h2_idle_timeout->text().trimmed();
    Configs::dataManager->settingsRepo->h2_keep_alive_period = ui->h2_keep_alive_period->text().trimmed();
    Configs::dataManager->settingsRepo->h2_stream_receive_window = ui->h2_stream_receive_window->text().trimmed();
    Configs::dataManager->settingsRepo->h2_connection_receive_window = ui->h2_connection_receive_window->text().trimmed();
    D_SAVE_INT(h2_max_concurrent_streams)
    D_SAVE_INT(quic_initial_packet_size)
    D_SAVE_BOOL(quic_disable_path_mtu_discovery)

    MW_dialog_message(MwMessage::UpdateSettings, {});
    QDialog::accept();
}
