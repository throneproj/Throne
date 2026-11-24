#include "include/ui/profile/edit_anytls.h"

#include <QUuid>
#include <QRegularExpressionValidator>
#include <limits>
#include "include/ui/utils/Disclosure.hpp"
#include "include/ui/utils/GuiUtils.hpp"
#include "include/ui/utils/ValidationForm.h"

EditAnyTLS::EditAnyTLS(QWidget *parent) : QWidget(parent), ui(new Ui::EditAnyTLS) {
    ui->setupUi(this);
    ui->min->setValidator(QRegExpValidator_Number);
    Disclosure::Setup(ui->advanced_toggle, ui->advanced_box, [this] {
        if (editor_state_changed) editor_state_changed();
    });
}

EditAnyTLS::~EditAnyTLS() {
    delete ui;
}

void EditAnyTLS::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->AnyTLS();

    ui->password->setText(outbound->password);
    ui->interval->setText(outbound->idle_session_check_interval);
    ui->timeout->setText(outbound->idle_session_timeout);
    ui->min->setText(Int2String(outbound->min_idle_session));

    // An empty duration runs as the default too.
    const Configs::anyTLS defaults;
    const auto custom = [](const QString &value, const QString &fallback) { return !value.isEmpty() && value != fallback; };
    ui->advanced_toggle->setChecked(custom(outbound->idle_session_check_interval, defaults.idle_session_check_interval) ||
                                    custom(outbound->idle_session_timeout, defaults.idle_session_timeout) ||
                                    outbound->min_idle_session != defaults.min_idle_session);
}

bool EditAnyTLS::onEnd() {
    auto outbound = this->ent->AnyTLS();

    outbound->password = ui->password->text();
    outbound->idle_session_check_interval = ui->interval->text().trimmed();
    outbound->idle_session_timeout = ui->timeout->text().trimmed();
    outbound->min_idle_session = ui->min->text().trimmed().toInt();

    return true;
}

void EditAnyTLS::bindValidation(Validation::Form &form) {
    const auto duration = Validation::Rules::Duration();
    const auto count = Validation::Rules::Int(0, std::numeric_limits<int>::max());
    form.bind(ui->password, Validation::Rules::Required(), this).raw();
    form.bind(ui->interval, duration, this);
    form.bind(ui->timeout, duration, this);
    form.bind(ui->min, count, this);
    Disclosure::CheckCollapsed(form, ui->advanced_toggle, ui->advanced_box,
                               {Disclosure::FieldCheck(ui->interval, duration), Disclosure::FieldCheck(ui->timeout, duration),
                                Disclosure::FieldCheck(ui->min, count)}, this);
}
