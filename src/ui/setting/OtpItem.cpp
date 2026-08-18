#include "include/ui/setting/OtpItem.h"

#include <QCursor>
#include <QPainter>

namespace {
    constexpr int READONLY_CODE_WIDTH = 110;

    QIcon RecolorOtpItemIcon(const QString &path, const QColor &color) {
        QPixmap pixmap(path);
        if (pixmap.isNull()) return QIcon(path);
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();
        return QIcon(pixmap);
    }
} // namespace

OtpItem::OtpItem(QWidget *parent, std::shared_ptr<Configs::OtpProfile> profile_, QListWidgetItem *item_,
                 const Mode mode_)
    : QWidget(parent), ui(new Ui::OtpItem), profile(std::move(profile_)), item(item_), mode(mode_) {
    ui->setupUi(this);
    setLayoutDirection(Qt::LeftToRight);

    if (mode == Mode::ReadOnly) {
        ui->actions->hide();
        ui->code->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 12pt; letter-spacing: 2px;"));
        ui->code->setMinimumWidth(READONLY_CODE_WIDTH);
    }

    // A child that consumes the press stops the click reaching the list, which selects, copies and drags.
    for (QWidget *passive : {static_cast<QWidget *>(ui->name), static_cast<QWidget *>(ui->code),
                             static_cast<QWidget *>(ui->timer_text), static_cast<QWidget *>(ui->timer_bar)})
        passive->setAttribute(Qt::WA_TransparentForMouseEvents);

    // Hiding the bar must not reflow the row, or HOTP rows would shift the code column.
    auto barPolicy = ui->timer_bar->sizePolicy();
    barPolicy.setRetainSizeWhenHidden(true);
    ui->timer_bar->setSizePolicy(barPolicy);

    connect(ui->edit, &QToolButton::clicked, this, [this] { emit editRequested(); });
    connect(ui->share, &QToolButton::clicked, this, [this] { emit exportRequested(); });
    connect(ui->remove, &QToolButton::clicked, this, [this] { emit deleteRequested(); });

    applyIconColors();
    setActionsVisible(false);
    Refresh();

    adjustSize();
    if (item != nullptr) item->setSizeHint(sizeHint());
}

OtpItem::~OtpItem() {
    delete ui;
}

void OtpItem::setActionsVisible(const bool visible) const {
    if (mode == Mode::ReadOnly) return;
    ui->edit->setVisible(visible);
    ui->share->setVisible(visible);
    ui->remove->setVisible(visible);
}

void OtpItem::applyIconColors() const {
    const auto color = palette().color(QPalette::ButtonText);
    ui->edit->setIcon(RecolorOtpItemIcon(":/icon/material/pencil-outline.png", color));
    ui->share->setIcon(RecolorOtpItemIcon(":/icon/material/qrcode.png", color));
    ui->remove->setIcon(RecolorOtpItemIcon(":/icon/material/delete.png", color));
}

void OtpItem::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange) applyIconColors();
    QWidget::changeEvent(event);
}

void OtpItem::enterEvent(QEnterEvent *event) {
    setActionsVisible(true);
    QWidget::enterEvent(event);
}

void OtpItem::leaveEvent(QEvent *event) {
    // Moving onto a child button sends Leave here, so re-check the cursor before hiding.
    if (!rect().contains(mapFromGlobal(QCursor::pos()))) setActionsVisible(false);
    QWidget::leaveEvent(event);
}

void OtpItem::updateNameLabel() const {
    // QLabel clips rather than elides.
    const QFontMetrics metrics(ui->name->font());
    ui->name->setText(metrics.elidedText(profile->name, Qt::ElideRight, ui->name->width()));
    ui->name->setToolTip(profile->DisplayName());
}

void OtpItem::resizeEvent(QResizeEvent *event) {
    updateNameLabel();
    QWidget::resizeEvent(event);
}

void OtpItem::Refresh() const {
    updateNameLabel();

    const auto code = profile->CurrentCode();
    if (code.isEmpty()) {
        ui->code->setText(tr("invalid"));
        ui->timer_text->clear();
        ui->timer_bar->hide();
        return;
    }

    ui->code->setText(code);
    if (profile->type == OTP::Type::HOTP) {
        ui->timer_text->setText("#" + QString::number(profile->counter));
        ui->timer_bar->hide();
        return;
    }

    const int remaining = profile->SecondsRemaining();
    ui->timer_bar->show();
    ui->timer_bar->setValue(profile->period > 0 ? remaining * 1000 / profile->period : 0);
    ui->timer_text->setText(tr("%1s").arg(remaining));
}
