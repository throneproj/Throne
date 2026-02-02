#include "include/ui/profile/edit_ssh.h"
#include <QFileDialog>

EditSSH::EditSSH(QWidget *parent) : QWidget(parent), ui(new Ui::EditSSH) {
    ui->setupUi(this);
}

EditSSH::~EditSSH() {
    delete ui;
}

void EditSSH::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->SSH();

    ui->user->setText(outbound->user);
    ui->password->setText(outbound->password);
    ui->private_key->setText(outbound->private_key);
    ui->private_key_path->setText(outbound->private_key_path);
    ui->private_key_pass->setText(outbound->private_key_passphrase);
    ui->host_key->setText(outbound->host_key.join(","));
    ui->host_key_algs->setText(outbound->host_key_algorithms.join(","));
    ui->client_version->setText(outbound->client_version);

    connect(ui->choose_pk, &QPushButton::clicked, this, [=,this] {
        auto fn = QFileDialog::getOpenFileName(this, QObject::tr("Select"), QDir::currentPath(),
                                               "", nullptr, QFileDialog::Option::ReadOnly);
        if (!fn.isEmpty()) {
            ui->private_key_path->setText(fn);
        }
    });
}

bool EditSSH::onEnd() {
    auto outbound = this->ent->SSH();

    outbound->user = ui->user->text();
    outbound->password = ui->password->text();
    outbound->private_key = ui->private_key->toPlainText();
    outbound->private_key_path = ui->private_key_path->text();
    outbound->private_key_passphrase = ui->private_key_pass->text();
    if (!ui->host_key->text().trimmed().isEmpty()) outbound->host_key = ui->host_key->text().split(",");
    else outbound->host_key = {};
    if (!ui->host_key_algs->text().trimmed().isEmpty()) outbound->host_key_algorithms = ui->host_key_algs->text().split(",");
    else outbound->host_key_algorithms = {};
    outbound->client_version = ui->client_version->text();

    return true;
}