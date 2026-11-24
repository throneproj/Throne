#include "include/ui/profile/edit_naive.h"

#include "include/configs/common/utils.h"
#include "include/ui/utils/Disclosure.hpp"
#include "include/ui/utils/ValidationForm.h"

namespace {
    // Index 0 is the translated "Default", stored empty; a stored value missing from the list stays as an extra item.
    void editNaiveSelectCongestion(QComboBox *combo, const QString &value) {
        const auto congestion = Configs::LegacyComboValue(value, Configs::naiveCongestionControls);
        if (congestion.isEmpty()) {
            combo->setCurrentIndex(0);
            return;
        }
        int index = combo->findText(congestion);
        if (index <= 0) {
            combo->addItem(congestion);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index);
    }
}

EditNaive::EditNaive(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditNaive) {

    ui->setupUi(this);
    Disclosure::Setup(ui->advanced_toggle, ui->advanced_box, [this] {
        if (editor_state_changed) editor_state_changed();
    });
}

EditNaive::~EditNaive() {
    delete ui;
}

void EditNaive::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = ent->Naive();

    ui->username->setText(outbound->username);
    ui->password->setText(outbound->password);
    ui->extra_headers->setText(Configs::getHeadersString(outbound->extra_headers));
    // The core has no upper bound; a larger imported value must not be clamped by just opening and saving.
    ui->insecure_concurrency->setMaximum(qMax(ui->insecure_concurrency->maximum(), outbound->insecure_concurrency));
    ui->insecure_concurrency->setValue(outbound->insecure_concurrency);
    ui->uot->setChecked(outbound->uot);
    ui->quic->setChecked(outbound->quic);
    editNaiveSelectCongestion(ui->congestion_control, outbound->congestion_control);
    ui->advanced_toggle->setChecked(!outbound->extra_headers.isEmpty() || outbound->insecure_concurrency != 0 || outbound->uot ||
                                    ui->congestion_control->currentIndex() != 0);
}

bool EditNaive::onEnd() {
    auto outbound = ent->Naive();

    outbound->username = ui->username->text().trimmed();
    outbound->password = ui->password->text();
    outbound->extra_headers = Configs::parseHeaderPairs(ui->extra_headers->text());
    outbound->insecure_concurrency = ui->insecure_concurrency->value();
    outbound->uot = ui->uot->isChecked();
    outbound->quic = ui->quic->isChecked();
    outbound->congestion_control = ui->congestion_control->currentIndex() <= 0 ? QString() : ui->congestion_control->currentText().trimmed();
    return true;
}

void EditNaive::bindValidation(Validation::Form &form) {
    form.bind(ui->extra_headers, Configs::Checks::NaiveExtraHeaders, this);
    form.bind(ui->extra_headers, [](const QString &text) {
        return Configs::Checks::NaiveExtraHeaderNames(Configs::parseHeaderPairs(text));
    }, this).warning();
    Disclosure::CheckCollapsed(form, ui->advanced_toggle, ui->advanced_box,
                               {Disclosure::FieldCheck(ui->extra_headers, Configs::Checks::NaiveExtraHeaders)}, this);
}
