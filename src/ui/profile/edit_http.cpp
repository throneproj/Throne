#include "include/ui/profile/edit_http.h"

#include "include/ui/utils/ValidationForm.h"

EditHttp::EditHttp(QWidget *parent) : QWidget(parent),
      ui(new Ui::EditHttp) {
    ui->setupUi(this);
}

EditHttp::~EditHttp() {
    delete ui;
}

void EditHttp::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Http();

    ui->username->setText(outbound->username);
    ui->password->setText(outbound->password);
}

bool EditHttp::onEnd() {
    auto outbound = this->ent->Http();
    outbound->username = ui->username->text().trimmed();
    outbound->password = ui->password->text();
    return true;
}

void EditHttp::bindValidation(Validation::Form &form) {
    form.addRule({ui->password, ui->username}, [this] {
        return Configs::Checks::HttpAuth(ui->username->text().trimmed(), ui->password->text());
    }, this, Validation::Severity::Warning);
}
