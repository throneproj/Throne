#include "include/ui/profile/edit_ssh.h"
#include <QFileDialog>

#include "include/configs/proxy/SSHBean.h"

EditSSH::EditSSH(QWidget *parent) : QWidget(parent), ui(new Ui::EditSSH) {
    ui->setupUi(this);
}

EditSSH::~EditSSH() {
    delete ui;
}

void EditSSH::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->SSHBean();

    ui->user->setText(bean->user);
    ui->password->setText(bean->password);
    ui->private_key->setText(bean->privateKey);
    ui->private_key_path->setText(bean->privateKeyPath);
    ui->private_key_pass->setText(bean->privateKeyPass);
    ui->host_key->setText(bean->hostKey.join(","));
    ui->host_key_algs->setText(bean->hostKeyAlgs.join(","));
    ui->client_version->setText(bean->clientVersion);

    connect(ui->choose_pk, &QPushButton::clicked, this, [=] {
        auto fn = QFileDialog::getOpenFileName(this, QObject::tr("Select"), QDir::currentPath(),
                                               "", nullptr, QFileDialog::Option::ReadOnly);
        if (!fn.isEmpty()) {
            ui->private_key_path->setText(fn);
        }
    });
}

bool EditSSH::onEnd() {
    auto bean = this->ent->SSHBean();

    bean->user = ui->user->text();
    bean->password = ui->password->text();
    bean->privateKey = ui->private_key->toPlainText();
    bean->privateKeyPath = ui->private_key_path->text();
    bean->privateKeyPass = ui->private_key_pass->text();
    if (!ui->host_key->text().trimmed().isEmpty()) bean->hostKey = ui->host_key->text().split(",");
    else bean->hostKey = {};
    if (!ui->host_key_algs->text().trimmed().isEmpty()) bean->hostKeyAlgs = ui->host_key_algs->text().split(",");
    else bean->hostKeyAlgs = {};
    bean->clientVersion = ui->client_version->text();

    return true;
}