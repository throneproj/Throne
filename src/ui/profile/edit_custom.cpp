#include "include/ui/profile/edit_custom.h"

#include "include/ui/utils/ValidationForm.h"
#include "include/ui/widget/json/JsonEditorDialog.h"
#include "include/ui/widget/json/SchemaStore.h"

#include <QMessageBox>
#include <QClipboard>

EditCustom::EditCustom(QWidget *parent) : QWidget(parent), ui(new Ui::EditCustom) {
    ui->setupUi(this);
    ui->config_simple->setPlaceholderText(
        "example:\n"
        "  server-address: \"127.0.0.1:%mapping_port%\"\n"
        "  listen-address: \"127.0.0.1\"\n"
        "  listen-port: %socks_port%\n"
        "  host: your-domain.com\n"
        "  sni: your-domain.com\n");
}

EditCustom::~EditCustom() {
    delete ui;
}

void EditCustom::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Custom();

    if (preset_core == Configs::Custom::CustomOutbound) {
        preset_command = preset_config = "";
        ui->config_simple->setPlaceholderText(
            "{\n"
            "    \"type\": \"socks\",\n"
            "    // ...\n"
            "}");
    } else if (preset_core == Configs::Custom::CustomFullConfig) {
        preset_command = preset_config = "";
        ui->config_simple->setPlaceholderText(
            "{\n"
            "    \"inbounds\": [],\n"
            "    \"outbounds\": []\n"
            "}");
    } else if (preset_core == Configs::Custom::CustomXrayOutbound) {
        preset_command = preset_config = "";
        ui->config_simple->setPlaceholderText(
            "{\n"
            "    \"protocol\": \"vless\",\n"
            "    \"settings\": { /* ... */ }\n"
            "}");
    } else if (preset_core == Configs::Custom::CustomXrayFullConfig) {
        preset_command = preset_config = "";
        ui->config_simple->setPlaceholderText(
            "{\n"
            "    \"inbounds\": [],\n"
            "    \"outbounds\": [],\n"
            "    \"routing\": {}\n"
            "}");
    }

    ui->config_simple->setPlainText(outbound->config);

    if (preset_core == Configs::Custom::CustomOutbound) {
        ui->core_l->setText(tr("Outbound JSON, please read the documentation."));
    } else if (preset_core == Configs::Custom::CustomXrayOutbound) {
        ui->core_l->setText(tr("Xray outbound JSON, please read the Xray documentation."));
    } else if (preset_core == Configs::Custom::CustomXrayFullConfig) {
        ui->core_l->setText(tr("Xray full config (Throne adds a socks inbound and uses sing-box for tun/routing)."));
    } else {
        ui->core_l->setText(tr("Please fill the complete config."));
    }
    ui->w_ext1->hide();
    ui->w_ext2->hide();
}

bool EditCustom::onEnd() {
    if (get_edit_text_name().isEmpty()) {
        MessageBoxWarning(software_name, tr("Name cannot be empty."));
        return false;
    }

    auto outbound = this->ent->Custom();

    outbound->config = ui->config_simple->toPlainText();
    outbound->type = preset_core;

    return true;
}

void EditCustom::bindValidation(Validation::Form &form) {
    this->form = &form;
    // Untrimmed, so the reported line numbers match the editor.
    form.bind(ui->config_simple, [this](const QString &text) { return Configs::Checks::CustomConfig(preset_core, text); }, this)
        .raw()
        .label(tr("Config"));
}

void EditCustom::on_as_json_clicked() {
    auto editor = new JsonEdit::JsonEditorDialog(QString2QJsonObject(ui->config_simple->toPlainText()), this);
    if (preset_core == Configs::Custom::CustomOutbound) {
        editor->SetValidator(JsonEdit::SingBoxValidator({JsonEdit::SingBox::Outbound, JsonEdit::SingBox::Endpoint}));
    } else if (preset_core == Configs::Custom::CustomFullConfig) {
        editor->SetValidator(JsonEdit::SingBoxValidator(JsonEdit::SingBox::Config));
    }
    auto result = editor->OpenEditor();
    if (!result.isEmpty()) {
        ui->config_simple->setPlainText(QJsonObject2QString(result, false));
        if (form) form->revalidate(ui->config_simple);
    }
    editor->deleteLater();
}
