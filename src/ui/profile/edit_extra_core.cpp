#include "include/ui/profile/edit_extra_core.h"

#include <QFileDialog>



#include "include/ui/profile/dialog_edit_profile.h"

EditExtraCore::EditExtraCore(QWidget *parent) : QWidget(parent),
                                                ui(new Ui::EditExtraCore) {
    ui->setupUi(this);
}

EditExtraCore::~EditExtraCore() {
    delete ui;
}

void EditExtraCore::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;

    auto outbound = ent->ExtraCore();
    ui->socks_address->setText(outbound->socksAddress);
    ui->socks_port->setValidator(new QIntValidator(1, 65534));
    ui->socks_port->setText(Int2String(outbound->socksPort));
    ui->config->setPlainText(outbound->extraCoreConf);
    ui->args->setText(outbound->extraCoreArgs);
    ui->path_combo->addItems(Configs::dataManager->settingsRepo->GetExtraCorePaths());
    ui->path_combo->setCurrentText(outbound->extraCorePath);

    connect(ui->path_button, &QPushButton::pressed, this, [=,this]
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
        if (Configs::dataManager->settingsRepo->AddExtraCorePath(f)) ui->path_combo->addItem(f);
        ui->path_combo->setCurrentText(f);
        ui->path_combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        adjustSize();
    });
}

bool EditExtraCore::onEnd() {
    auto outbound = ent->ExtraCore();
    outbound->socksAddress = ui->socks_address->text();
    outbound->socksPort = ui->socks_port->text().toInt();
    outbound->extraCoreConf = ui->config->toPlainText();
    outbound->extraCorePath = ui->path_combo->currentText();
    outbound->extraCoreArgs = ui->args->text();

    return true;
}
