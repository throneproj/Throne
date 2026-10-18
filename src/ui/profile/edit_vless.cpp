#include "include/ui/profile/edit_vless.h"

#include <QPointer>

#include "include/ui/utils/ValidationForm.h"

namespace {
    // TLS and the transport live in the outer dialog, and they decide whether Vision can work.
    QComboBox *vlessOuterCombo(QWidget *dialog, const QWidget *editor, const QString &name) {
        if (dialog == nullptr) return nullptr;
        for (auto *combo : dialog->findChildren<QComboBox *>(name)) {
            if (!editor->isAncestorOf(combo)) return combo;
        }
        return nullptr;
    }
}

EditVless::EditVless(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditVless) {
    ui->setupUi(this);

    QStringList flows = {""};
    flows << Configs::vlessFlows;
    ui->flow->addItems(flows);
    ui->packet_encoding->addItems(Configs::vPacketEncoding);
    connect(ui->flow, &QComboBox::currentTextChanged, this, [this] {
        if (editor_state_changed) editor_state_changed();
    });
}

EditVless::~EditVless() {
    delete ui;
}

void EditVless::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->VLESS();

    ui->uuid->setText(outbound->uuid);
    ui->flow->setCurrentText(outbound->flow);
    ui->packet_encoding->setCurrentText(outbound->packet_encoding);
}

bool EditVless::onEnd() {
    auto outbound = this->ent->VLESS();

    outbound->uuid = ui->uuid->text().trimmed();
    outbound->flow = ui->flow->currentText().trimmed();
    outbound->packet_encoding = ui->packet_encoding->currentText().trimmed();
    return true;
}

bool EditVless::blocksMultiplex() {
    return ui->flow->currentText() == "xtls-rprx-vision";
}

void EditVless::bindValidation(Validation::Form &form) {
    form.bind(ui->uuid, Validation::Rules::Required(), this);
    QWidget *dialog = get_edit_dialog ? get_edit_dialog() : nullptr;
    const QPointer<QComboBox> security = vlessOuterCombo(dialog, this, QStringLiteral("security"));
    const QPointer<QComboBox> network = vlessOuterCombo(dialog, this, QStringLiteral("network"));
    form.addRule({ui->flow, security, network}, [this, security, network] {
        const bool tls = security == nullptr || security->currentText() == "tls";
        const QString type = network != nullptr ? network->currentText().trimmed() : QString();
        return Configs::Checks::VlessFlow(ui->flow->currentText().trimmed(), tls, type);
    }, this);
}