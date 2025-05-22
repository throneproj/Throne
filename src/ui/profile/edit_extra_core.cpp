#include "include/ui/profile/edit_extra_core.h"

#include <QFileDialog>

#include "include/configs/proxy/ExtraCore.h"
#include "include/configs/proxy/Preset.hpp"
#include "include/ui/profile/dialog_edit_profile.h"

EditExtraCore::EditExtraCore(QWidget *parent) : QWidget(parent),
                                                ui(new Ui::EditExtraCore) {
    ui->setupUi(this);
}

EditExtraCore::~EditExtraCore() {
    delete ui;
}

void EditExtraCore::onStart(std::shared_ptr<NekoGui::ProxyEntity> _ent) {
    this->ent = _ent;

    auto bean = ent->ExtraCoreBean();
    ui->socks_address->setText(bean->socksAddress);
    ui->socks_port->setValidator(new QIntValidator(1, 65534));
    ui->socks_port->setText(Int2String(bean->socksPort));
    ui->config->setPlainText(bean->extraCoreConf);
    ui->args->setText(bean->extraCoreArgs);
    ui->path_combo->addItems(NekoGui::profileManager->GetExtraCorePaths());
    ui->path_combo->setCurrentText(bean->extraCorePath);

    connect(ui->path_button, &QPushButton::pressed, this, [=]
    {
        auto f = QFileDialog::getOpenFileName();
        if (f.isEmpty())
        {
            return;
        }
        if (!QDir::current().relativeFilePath(f).startsWith("../../"))
        {
            f = QDir::current().relativeFilePath(f);
        }
        if (NekoGui::profileManager->AddExtraCorePath(f)) ui->path_combo->addItem(f);
        ui->path_combo->setCurrentText(f);
        ui->path_combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        adjustSize();
    });
}

bool EditExtraCore::onEnd() {
    auto bean = ent->ExtraCoreBean();
    bean->socksAddress = ui->socks_address->text();
    bean->socksPort = ui->socks_port->text().toInt();
    bean->extraCoreConf = ui->config->toPlainText();
    bean->extraCorePath = ui->path_combo->currentText();
    bean->extraCoreArgs = ui->args->text();

    return true;
}
