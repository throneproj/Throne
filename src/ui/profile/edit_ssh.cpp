#include "include/ui/profile/edit_ssh.h"
#include <QFileDialog>

#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"
#include "include/ui/utils/NativeFileDialog.h"

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
        auto fn = NativeFileDialog::getOpenFileName(this, QObject::tr("Select"), QDir::currentPath(),
                                                    "", nullptr, QFileDialog::Option::ReadOnly);
        if (!fn.isEmpty()) {
            ui->private_key_path->setText(fn);
            if (form) form->revalidate(ui->private_key_path);
        }
    });
}

bool EditSSH::onEnd() {
    auto outbound = this->ent->SSH();

    outbound->user = ui->user->text().trimmed();
    outbound->password = ui->password->text();
    outbound->private_key = ui->private_key->toPlainText();
    outbound->private_key_path = ui->private_key_path->text().trimmed();
    outbound->private_key_passphrase = ui->private_key_pass->text();
    outbound->host_key = SplitAndTrim(ui->host_key->text(), ",", false);
    outbound->host_key_algorithms = SplitAndTrim(ui->host_key_algs->text(), ",", false);
    outbound->client_version = ui->client_version->text().trimmed();

    return true;
}

void EditSSH::bindValidation(Validation::Form &form) {
    using namespace Validation;
    using namespace Configs::Checks;
    this->form = &form;
    form.bind(ui->host_key, Rules::List(Split::Comma, SshHostKey), this);
    form.bind(ui->client_version, SshClientVersion, this).warning();
    form.addRule({ui->private_key}, [this] { return SshPrivateKey(ui->private_key->toPlainText()); }, this);
    form.addRule({ui->password, ui->private_key, ui->private_key_path}, [this] {
        return SshAuthentication(ui->password->text(), ui->private_key->toPlainText(), ui->private_key_path->text());
    }, this, Severity::Warning);
    form.addRule({ui->private_key, ui->private_key_path}, [this] {
        return SshKeySources(ui->private_key->toPlainText(), ui->private_key_path->text());
    }, this, Severity::Warning);
}

bool EditSSH::serverPortOptional() {
    return true;
}