#include "include/ui/profile/edit_custom.h"

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "include/configs/proxy/CustomBean.hpp"
#include "include/configs/proxy/Preset.hpp"
#include "include/configs/ConfigBuilder.hpp"
#include "include/dataStore/Database.hpp"

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

void EditCustom::onStart(std::shared_ptr<NekoGui::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->CustomBean();

    if (preset_core == "internal") {
        preset_command = preset_config = "";
        ui->config_simple->setPlaceholderText(
            "{\n"
            "    \"type\": \"socks\",\n"
            "    // ...\n"
            "}");
    } else if (preset_core == "internal-full") {
        preset_command = preset_config = "";
        ui->config_simple->setPlaceholderText(
            "{\n"
            "    \"inbounds\": [],\n"
            "    \"outbounds\": []\n"
            "}");
    }

    // load core ui
    ui->config_simple->setPlainText(bean->config_simple);

    // custom internal
    if (preset_core == "internal" || preset_core == "internal-full") {
        if (preset_core == "internal") {
            ui->core_l->setText(tr("Outbound JSON, please read the documentation."));
        } else {
            ui->core_l->setText(tr("Please fill the complete config."));
        }
        ui->w_ext1->hide();
        ui->w_ext2->hide();
    }
}

bool EditCustom::onEnd() {
    if (get_edit_text_name().isEmpty()) {
        MessageBoxWarning(software_name, tr("Name cannot be empty."));
        return false;
    }

    auto bean = this->ent->CustomBean();

    P_SAVE_STRING_PLAIN(config_simple)

    return true;
}

void EditCustom::on_as_json_clicked() {
    auto editor = new JsonEditor(QString2QJsonObject(ui->config_simple->toPlainText()), this);
    auto result = editor->OpenEditor();
    if (!result.isEmpty()) {
        ui->config_simple->setPlainText(QJsonObject2QString(result, false));
    }
}
