#pragma once

#include <QDialog>
#include "include/engine/EditLocks.h"
#include "include/global/Configs.hpp"
#include "ui_dialog_integration.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogIntegration;
}
QT_END_NAMESPACE

namespace Validation {
    class Form;
}

class DialogIntegration : public QDialog {
    Q_OBJECT

public:
    explicit DialogIntegration(QWidget *parent = nullptr, const QList<QAction*>& actions = {});

    ~DialogIntegration() override;

public slots:
    void accept() override;

    void reject() override;

private:
    void generateShortcutItems(const QList<QAction*>& actions);

    void bindHotkeyValidation();

    void setupApiTab();

    void updateApiControls();

    void regenerateApiKey();

    void copyForScripts();

    struct HotkeyEdit {
        QtExtKeySequenceEdit *edit = nullptr;
        QString label;
        bool global = false;
    };

    QMap<QtExtKeySequenceEdit*, QString> seqEdit2ID;
    QList<HotkeyEdit> hotkeyEdits;
    Validation::Form *form = nullptr;
    QString savedApiKey;
    Engine::EditLock settingsLock{Engine::EditTarget::Settings};
    Ui::DialogIntegration *ui;
};
