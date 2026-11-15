#pragma once

#include <QDialog>

#include "ui_dialog_preset_settings.h"
#include "include/database/SettingsValues.h"
#include "include/engine/EditLocks.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogPresetSettings;
}
QT_END_NAMESPACE

namespace Validation {
    class Form;
}

class DialogPresetSettings : public QDialog {
    Q_OBJECT

public:
    explicit DialogPresetSettings(QWidget *parent = nullptr);

    ~DialogPresetSettings() override;

private:
    Ui::DialogPresetSettings *ui;

    Validation::Form *form = nullptr;

    Engine::EditLock settingsLock{Engine::EditTarget::Settings};

    Configs::SettingsValues snapshot_;

public slots:

    void accept() override;
};
