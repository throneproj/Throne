#include "include/ui/setting/dialog_edit_otp.h"

#include <QMessageBox>
#include <QPainter>

#include "include/global/Utils.hpp"
#include "include/global/Validation.hpp"
#include "include/ui/utils/ValidationForm.h"

#include <algorithm>
#include <climits>

namespace {
    QIcon RecolorIcon(const QString &path, const QColor &color) {
        QPixmap pixmap(path);
        if (pixmap.isNull()) return QIcon(path);
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();
        return QIcon(pixmap);
    }
}

DialogEditOtp::DialogEditOtp(QWidget *parent, std::shared_ptr<Configs::OtpProfile> profile_)
    : QDialog(parent), ui(new Ui::DialogEditOtp), profile(std::move(profile_)) {
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);

    ui->name->setText(profile->name);
    ui->secret->setText(profile->secret);
    ui->issuer->setText(profile->issuer);
    ui->type->setCurrentIndex(profile->type == OTP::Type::HOTP ? 1 : 0);
    ui->algorithm->setCurrentIndex(static_cast<int>(profile->algorithm));
    ui->digits->setValue(profile->digits);
    ui->period->setValue(profile->period);
    ui->counter->setValue(static_cast<int>(std::clamp<qint64>(profile->counter, 0, INT_MAX)));

    connect(ui->show_secret, &QToolButton::toggled, this, [this](const bool on) {
        ui->secret->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
        ui->show_secret->setToolTip(on ? tr("Hide secret") : tr("Show secret"));
        applyIconColors();
    });
    applyIconColors();
    connect(ui->type, &QComboBox::currentIndexChanged, this, [this](int) {
        updateTypeFields();
        updatePreview();
    });
    for (auto *edit : {ui->secret, ui->issuer}) connect(edit, &QLineEdit::textChanged, this, [this] { updatePreview(); });
    connect(ui->algorithm, &QComboBox::currentIndexChanged, this, [this](int) { updatePreview(); });
    for (auto *spin : {ui->digits, ui->period, ui->counter})
        connect(spin, &QSpinBox::valueChanged, this, [this](int) { updatePreview(); });

    previewTimer = new QTimer(this);
    previewTimer->setInterval(1000);
    connect(previewTimer, &QTimer::timeout, this, [this] { updatePreview(); });
    previewTimer->start();

    form->bind(ui->name, Validation::Rules::Required());
    form->bind(ui->secret, [](const QString &text) {
        if (text.trimmed().isEmpty()) return Validation::Required(text);
        bool ok = false;
        const auto key = OTP::Base32Decode(text, &ok);
        if (!ok) return tr("Not valid base32: use the letters A-Z and the digits 2-7.");
        return key.isEmpty() ? tr("Too short to be a secret.") : QString();
    }).raw();

    updateTypeFields();
    updatePreview();
    form->markAll();
}

DialogEditOtp::~DialogEditOtp() {
    delete ui;
}

void DialogEditOtp::applyIconColors() const {
    const auto color = palette().color(QPalette::ButtonText);
    ui->show_secret->setIcon(RecolorIcon(ui->show_secret->isChecked() ? ":/icon/material/eye-off-outline.png"
                                                                     : ":/icon/material/eye-outline.png",
                                         color));
}

void DialogEditOtp::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange) applyIconColors();
    QDialog::changeEvent(event);
}

void DialogEditOtp::updateTypeFields() const {
    const bool isHotp = ui->type->currentIndex() == 1;
    ui->label_period->setVisible(!isHotp);
    ui->period->setVisible(!isHotp);
    ui->label_counter->setVisible(isHotp);
    ui->counter->setVisible(isHotp);
}

QString DialogEditOtp::collect(Configs::OtpProfile &out) const {
    out.id = profile->id;
    out.name = ui->name->text().trimmed();
    out.issuer = ui->issuer->text().trimmed();
    out.secret = OTP::NormalizeSecret(ui->secret->text());
    out.type = ui->type->currentIndex() == 1 ? OTP::Type::HOTP : OTP::Type::TOTP;
    out.algorithm = static_cast<OTP::Algorithm>(ui->algorithm->currentIndex());
    out.digits = ui->digits->value();
    out.period = ui->period->value();
    // The spin box tops out at INT_MAX; a larger stored counter is kept while the field still shows that cap.
    const bool counterCapped = profile->counter > ui->counter->maximum() && ui->counter->value() == ui->counter->maximum();
    out.counter = counterCapped ? profile->counter : ui->counter->value();

    if (out.name.isEmpty()) return tr("Name cannot be empty");
    if (ui->secret->text().trimmed().isEmpty()) return tr("Secret is empty");
    if (out.secret.isEmpty()) return tr("Secret is not valid base32");
    return out.Validate();
}

void DialogEditOtp::updatePreview() const {
    Configs::OtpProfile candidate;
    collect(candidate);
    if (const auto error = candidate.Validate(); !error.isEmpty()) {
        ui->preview->setText(error);
        return;
    }

    const auto code = candidate.CurrentCode();
    if (code.isEmpty()) {
        ui->preview->setText(tr("No code yet"));
        return;
    }
    if (candidate.type == OTP::Type::HOTP) {
        ui->preview->setText(tr("Current code: %1").arg(code));
        return;
    }
    ui->preview->setText(tr("Current code: %1 (%2s)").arg(code).arg(candidate.SecondsRemaining()));
}

void DialogEditOtp::accept() {
    if (!form->validateAll()) return;
    Configs::OtpProfile candidate;
    if (const auto error = collect(candidate); !error.isEmpty()) {
        MessageBoxWarning(tr("OTP Profile"), error);
        return;
    }

    *profile = candidate;
    QDialog::accept();
}
