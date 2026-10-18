#include "include/ui/profile/edit_wireguard_amnezia.h"

#include <QGuiApplication>
#include <QInputDialog>
#include <QLayout>
#include <QPointer>
#include <QScreen>
#include <QScrollBar>

#include <limits>
#include <optional>

#include "include/api/RPC.h"
#include "include/ui/utils/GuiUtils.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"

namespace {
    QString lastSignatureDomain;

    // Text the field's own rule rejects yields nothing, so cross rules stay quiet about it.
    std::optional<qint64> amneziaEditorNumber(const QLineEdit *edit, qint64 max) {
        const QString text = edit->text().trimmed();
        if (text.isEmpty()) return 0;
        const auto value = Validation::ToInt(text);
        if (!value || *value < 0 || *value > max) return std::nullopt;
        return value;
    }
}

EditWireguardAmnezia::EditWireguardAmnezia(QWidget *parent, const std::shared_ptr<Configs::Profile> &_ent)
    : QDialog(parent)
    , ui(new Ui::EditWireguardAmnezia)
{
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);
    bindValidation();
    ent = _ent;
    auto outbound = ent->Wireguard();

    ui->jc->setText(Int2String(outbound->jc));
    ui->jmin->setText(Int2String(outbound->jmin));
    ui->jmax->setText(Int2String(outbound->jmax));
    ui->s1->setText(Int2String(outbound->s1));
    ui->s2->setText(Int2String(outbound->s2));
    ui->s3->setText(Int2String(outbound->s3));
    ui->s4->setText(Int2String(outbound->s4));
    ui->h1->setText(outbound->h1);
    ui->h2->setText(outbound->h2);
    ui->h3->setText(outbound->h3);
    ui->h4->setText(outbound->h4);
    ui->i1->setText(outbound->i1);
    ui->i2->setText(outbound->i2);
    ui->i3->setText(outbound->i3);
    ui->i4->setText(outbound->i4);
    ui->i5->setText(outbound->i5);
    ui->header_protection_key->setText(outbound->header_protection_key);
    ui->content_padding_addition->setText(outbound->content_padding_addition);
    ui->rekey_after_time->setText(outbound->rekey_after_time);
    ui->rekey_timeout->setText(outbound->rekey_timeout);
    ui->reject_after_time->setText(outbound->reject_after_time);
    ui->keepalive_timeout->setText(outbound->keepalive_timeout);
    ui->max_handshake_attempts->setText(outbound->max_handshake_attempts);
    ui->random_trailers->setChecked(outbound->random_trailers);
    ui->disable_cookies->setChecked(outbound->disable_cookies);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui->generate_signature, &QPushButton::clicked, this, [this] { generateSignature(); });

    ADD_ASTERISK(this)
    form->markAll();

    // QScrollArea::sizeHint() caps at 36x24 font cells, so size the window off the content instead.
    for (auto *nested: ui->scroll_content->findChildren<QLayout *>()) nested->activate();
    const auto margins = layout()->contentsMargins();
    const auto frame = 2 * ui->scroll->frameWidth();
    const auto wanted = ui->scroll->widget()->sizeHint() +
                        QSize(frame + ui->scroll->verticalScrollBar()->sizeHint().width() +
                              margins.left() + margins.right(),
                              frame + margins.top() + margins.bottom() +
                              ui->buttonBox->sizeHint().height() + layout()->spacing());

    const auto *scr = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
    resize(scr != nullptr ? wanted.boundedTo(scr->availableGeometry().size()) : wanted);
}

EditWireguardAmnezia::~EditWireguardAmnezia()
{
    delete ui;
}

void EditWireguardAmnezia::generateSignature() {
    bool ok = false;
    const auto domain = QInputDialog::getText(this, tr("QUIC Signature"), tr("Domain to imitate:"), QLineEdit::Normal,
                                              lastSignatureDomain, &ok).trimmed();
    if (!ok || domain.isEmpty()) return;
    lastSignatureDomain = domain;

    const auto originalText = ui->generate_signature->text();
    ui->generate_signature->setEnabled(false);
    ui->generate_signature->setText(tr("Generating..."));

    QPointer<EditWireguardAmnezia> self(this);
    runOnNewThread([self, domain, originalText] {
        bool rpcOK = false;
        const auto reply = API::defaultClient->GenQuicSignature(&rpcOK, domain);
        runOnUiThread([self, originalText, rpcOK, reply] {
            if (self == nullptr) return;
            self->ui->generate_signature->setText(originalText);
            self->ui->generate_signature->setEnabled(true);
            const auto error = rpcOK ? QString::fromStdString(reply.error.value_or("")) : tr("Failed to reach the core.");
            if (!error.isEmpty() || reply.packets.empty()) {
                MessageBoxWarning(tr("Failed to generate QUIC signature"), error);
                return;
            }
            const QList<QLineEdit *> fields = {self->ui->i1, self->ui->i2, self->ui->i3, self->ui->i4, self->ui->i5};
            for (qsizetype i = 0; i < fields.size(); ++i) {
                fields[i]->setText(i < static_cast<qsizetype>(reply.packets.size())
                                       ? QString::fromStdString(reply.packets[i])
                                       : QString());
            }
        });
    });
}

void EditWireguardAmnezia::bindValidation() {
    using namespace Validation;
    constexpr qint64 kIntMax = std::numeric_limits<int>::max();

    for (auto *edit: {ui->jc, ui->jmin, ui->jmax}) form->bind(edit, Rules::Int(0, kIntMax));
    for (auto *edit: {ui->s1, ui->s2, ui->s3, ui->s4}) form->bind(edit, Rules::Int(0, 65535));
    for (auto *edit: {ui->h1, ui->h2, ui->h3, ui->h4, ui->content_padding_addition, ui->rekey_after_time, ui->rekey_timeout,
                      ui->reject_after_time, ui->keepalive_timeout, ui->max_handshake_attempts}) {
        form->bind(edit, Configs::Checks::AmneziaRange);
    }
    for (auto *edit: {ui->i1, ui->i2, ui->i3, ui->i4, ui->i5}) form->bind(edit, Configs::Checks::AmneziaSignature);
    form->bind(ui->header_protection_key, Rules::Base64Key(32, false, true, Empty::Allowed));

    form->addRule({ui->h1, ui->h2, ui->h3, ui->h4}, [this] {
        return Configs::Checks::AmneziaHeaders({ui->h1->text(), ui->h2->text(), ui->h3->text(), ui->h4->text()});
    });
    form->addRule({ui->header_protection_key, ui->s1, ui->s2, ui->s3, ui->s4}, [this] {
        QList<qint64> paddings;
        for (const auto *edit: {ui->s1, ui->s2, ui->s3, ui->s4}) {
            const auto value = amneziaEditorNumber(edit, 65535);
            if (!value) return QString();
            paddings << *value;
        }
        return Configs::Checks::AmneziaHeaderProtection(ui->header_protection_key->text(), paddings);
    });
    form->addRule({ui->jmin, ui->jmax}, [this, kIntMax] {
        const auto jmin = amneziaEditorNumber(ui->jmin, kIntMax), jmax = amneziaEditorNumber(ui->jmax, kIntMax);
        if (!jmin || !jmax) return QString();
        return Configs::Checks::AmneziaJunkSizes(*jmin, *jmax);
    }, nullptr, Severity::Warning);
}

void EditWireguardAmnezia::accept() {
    if (!form->validateAll()) return;
    auto outbound = ent->Wireguard();

    outbound->jc = ui->jc->text().trimmed().toInt();
    outbound->jmin = ui->jmin->text().trimmed().toInt();
    outbound->jmax = ui->jmax->text().trimmed().toInt();
    outbound->s1 = ui->s1->text().trimmed().toInt();
    outbound->s2 = ui->s2->text().trimmed().toInt();
    outbound->s3 = ui->s3->text().trimmed().toInt();
    outbound->s4 = ui->s4->text().trimmed().toInt();
    outbound->h1 = ui->h1->text().trimmed();
    outbound->h2 = ui->h2->text().trimmed();
    outbound->h3 = ui->h3->text().trimmed();
    outbound->h4 = ui->h4->text().trimmed();
    outbound->i1 = ui->i1->text().trimmed();
    outbound->i2 = ui->i2->text().trimmed();
    outbound->i3 = ui->i3->text().trimmed();
    outbound->i4 = ui->i4->text().trimmed();
    outbound->i5 = ui->i5->text().trimmed();
    outbound->header_protection_key = ui->header_protection_key->text().trimmed();
    outbound->content_padding_addition = ui->content_padding_addition->text().trimmed();
    outbound->rekey_after_time = ui->rekey_after_time->text().trimmed();
    outbound->rekey_timeout = ui->rekey_timeout->text().trimmed();
    outbound->reject_after_time = ui->reject_after_time->text().trimmed();
    outbound->keepalive_timeout = ui->keepalive_timeout->text().trimmed();
    outbound->max_handshake_attempts = ui->max_handshake_attempts->text().trimmed();
    outbound->random_trailers = ui->random_trailers->isChecked();
    outbound->disable_cookies = ui->disable_cookies->isChecked();

    QDialog::accept();
}
