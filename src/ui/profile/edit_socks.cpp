#include "include/ui/profile/edit_socks.h"

#include "include/ui/utils/ValidationForm.h"

namespace {
    // Version combo rows: 5, 4 (from the .ui) and 4a.
    constexpr int socksVersion5Index = 0;
    constexpr int socksVersion4Index = 1;
    constexpr int socksVersion4aIndex = 2;
}

EditSocks::EditSocks(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditSocks) {

    ui->setupUi(this);
    ui->version->addItem(QStringLiteral("4a"));
}

EditSocks::~EditSocks() {
    delete ui;
}

void EditSocks::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Socks();

    ui->version->setCurrentIndex(outbound->version != 4 ? socksVersion5Index : outbound->socks4a ? socksVersion4aIndex : socksVersion4Index);
    ui->username->setText(outbound->username);
    ui->password->setText(outbound->password);
}

bool EditSocks::onEnd() {
    auto outbound = this->ent->Socks();

    const int version = ui->version->currentIndex();
    outbound->version = version == socksVersion5Index ? 5 : 4;
    outbound->socks4a = version == socksVersion4aIndex;
    outbound->username = ui->username->text().trimmed();
    outbound->password = ui->password->text();
    return true;
}

void EditSocks::bindValidation(Validation::Form &form) {
    form.addRule({ui->password, ui->username, ui->version}, [this] {
        const int version = ui->version->currentIndex() == socksVersion5Index ? 5 : 4;
        return Configs::Checks::SocksAuth(version, ui->username->text().trimmed(), ui->password->text());
    }, this, Validation::Severity::Warning);
}