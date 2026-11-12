#include "include/ui/profile/ProxyItem.h"

#include <QMessageBox>
#include <QStyle>

#include "include/configs/generate.h"

void ProxyItem::setColorRole(QWidget *widget, const QString &role) {
    if (widget->property("colorRole").toString() == role) return;
    widget->setProperty("colorRole", role);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

ProxyItem::ProxyItem(QWidget *parent, const std::shared_ptr<Configs::Profile> &ent, QListWidgetItem *item)
    : QWidget(parent), ui(new Ui::ProxyItem) {
    ui->setupUi(this);
    this->setLayoutDirection(Qt::LeftToRight);

    this->item = item;
    this->ent = ent;
    if (ent == nullptr) return;

    refresh_data();
}

ProxyItem::~ProxyItem() {
    delete ui;
}

void ProxyItem::refresh_data() {
    ui->type->setText(ent->outbound->DisplayType());
    ui->name->setText(ent->outbound->DisplayName());
    ui->address->setText(Configs::DisplayEffectiveAddress(ent));
    ui->traffic->setText(ent->DisplayTraffic());
    ui->test_result->setText(ent->DisplayTestResult());
    ui->test_result->setToolTip({});
    setColorRole(ui->test_result, {});

    runOnThread(
        [=,this] {
            adjustSize();
            item->setSizeHint(sizeHint());
            dynamic_cast<QWidget *>(parent())->adjustSize();
        },
        this);
}

void ProxyItem::showStepResult(const QString &text, const QString &toolTip, const QString &colorRole) {
    ui->test_result->setText(text);
    ui->test_result->setToolTip(toolTip);
    setColorRole(ui->test_result, colorRole);
}

void ProxyItem::on_remove_clicked() {
    if (!this->remove_confirm ||
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1?").arg(ent->outbound->DisplayName())) == QMessageBox::StandardButton::Yes) {
        // TODO do remove (or not) -> callback
        delete item;
    }
}

QPushButton *ProxyItem::get_change_button() {
    return ui->change;
}
