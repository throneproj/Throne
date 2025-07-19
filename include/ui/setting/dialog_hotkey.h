#pragma once

#include <QDialog>
#include "include/global/Configs.hpp"
#include "ui_dialog_hotkey.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogHotkey;
}
QT_END_NAMESPACE

class DialogHotkey : public QDialog {
    Q_OBJECT

public:
    explicit DialogHotkey(QWidget *parent = nullptr);

    ~DialogHotkey() override;

private:
    Ui::DialogHotkey *ui;
};
