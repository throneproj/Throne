#pragma once

#include <QDialog>
#include "ui_dialog_edit_group_advanced.h"
#include "include/database/entities/Group.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogEditGroupAdvanced;
}
QT_END_NAMESPACE

class DialogEditGroupAdvanced : public QDialog {
    Q_OBJECT

public:
    // serverIntervalHours: the interval the group's server last sent, 0 if none.
    DialogEditGroupAdvanced(const Configs::SubscriptionOptions &options, int serverIntervalHours, QWidget *parent = nullptr);

    ~DialogEditGroupAdvanced() override;

    [[nodiscard]] Configs::SubscriptionOptions Options() const { return options; }

public slots:
    void accept() override;

private:
    Ui::DialogEditGroupAdvanced *ui;
    Configs::SubscriptionOptions options;
    bool globalSendHwid = false;

    void syncHwidFields();

    void syncUrlTestFollowUps();
};
