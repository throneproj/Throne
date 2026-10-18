#include "include/ui/profile/edit_xrayvless.h"

#include <QPointer>

#include "include/ui/utils/ValidationForm.h"

namespace {
    // The Xray network and security live in the outer dialog, and they decide whether Vision can work.
    QComboBox *xrayVlessOuterCombo(QWidget *dialog, const QWidget *editor, const QString &name) {
        if (dialog == nullptr) return nullptr;
        for (auto *combo : dialog->findChildren<QComboBox *>(name)) {
            if (!editor->isAncestorOf(combo)) return combo;
        }
        return nullptr;
    }
}

EditXrayVless::EditXrayVless(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditXrayVless) {
    ui->setupUi(this);

    QStringList flows = {""};
    flows << Configs::xrayFlows;
    ui->xray_flow->addItems(flows);
}

EditXrayVless::~EditXrayVless() {
    delete ui;
}

void EditXrayVless::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = _ent->XrayVLESS();

    ui->xray_uuid->setText(outbound->uuid);
    ui->xray_enc->setText(outbound->encryption);
    // Matching ignores case, so saving canonicalises it; an unknown flow is kept as an extra item for the rule to flag.
    const QString flow = outbound->flow.trimmed();
    int flowIndex = ui->xray_flow->findText(flow, Qt::MatchFixedString);
    if (flowIndex < 0 && !flow.isEmpty() && flow.compare("none", Qt::CaseInsensitive) != 0) {
        ui->xray_flow->addItem(flow);
        flowIndex = ui->xray_flow->count() - 1;
    }
    ui->xray_flow->setCurrentIndex(qMax(0, flowIndex));
}

bool EditXrayVless::onEnd() {
    auto outbound = ent->XrayVLESS();
    outbound->uuid = ui->xray_uuid->text().trimmed();
    outbound->encryption = ui->xray_enc->text().trimmed();
    outbound->flow = ui->xray_flow->currentText().trimmed();

    return true;
}

void EditXrayVless::bindValidation(Validation::Form &form) {
    form.bind(ui->xray_uuid, Validation::Rules::XrayId(), this);
    form.bind(ui->xray_enc, [](const QString &text) { return Configs::Checks::XrayVlessEncryption(text); }, this);
    QWidget *dialog = get_edit_dialog ? get_edit_dialog() : nullptr;
    const QPointer<QComboBox> network = xrayVlessOuterCombo(dialog, this, QStringLiteral("xray_network"));
    const QPointer<QComboBox> security = xrayVlessOuterCombo(dialog, this, QStringLiteral("xray_security"));
    form.addRule({ui->xray_flow, network, security, ui->xray_enc}, [this, network, security] {
        if (auto message = Validation::OneOf(ui->xray_flow->currentText(), QStringList{""} + Configs::xrayFlows); !message.isEmpty()) return message;
        if (network == nullptr || security == nullptr) return QString();
        return Configs::Checks::XrayVlessVision(ui->xray_flow->currentText(), ui->xray_enc->text().trimmed(),
                                                network->currentText().trimmed(), security->currentText().trimmed());
    }, this);
}