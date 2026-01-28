#pragma once

// Dialogs

#define Dialog_DialogBasicSettings "DialogBasicSettings"
#define Dialog_DialogEditProfile "DialogEditProfile"
#define Dialog_DialogManageGroups "DialogManageGroups"
#define Dialog_DialogManageRoutes "DialogManageRoutes"
#define Dialog_DialogManageHotkeys "DialogManageHotkeys"

// Utils

#define QRegExpValidator_Number new QRegularExpressionValidator(QRegularExpression("^[0-9]+$"), this)

// Save&Load

#define P_C_LOAD_STRING(a) CACHE.a = bean->a;
#define P_C_SAVE_STRING(a) bean->a = CACHE.a;
#define D_C_LOAD_STRING(a) CACHE.a = Configs::dataManager->settingsRepo->a;
#define D_C_SAVE_STRING(a) Configs::dataManager->settingsRepo->a = CACHE.a;

#define P_LOAD_STRING(a) ui->a->setText(bean->a);
#define P_LOAD_STRING_PLAIN(a) ui->a->setPlainText(bean->a);
#define P_SAVE_STRING(a) bean->a = ui->a->text();
#define P_SAVE_STRING_PLAIN(a) bean->a = ui->a->toPlainText();

#define D_LOAD_STRING(a) ui->a->setText(Configs::dataManager->settingsRepo->a);
#define D_LOAD_STRING_PLAIN(a) ui->a->setPlainText(Configs::dataManager->settingsRepo->a);
#define D_SAVE_STRING(a) Configs::dataManager->settingsRepo->a = ui->a->text();
#define D_SAVE_STRING_PLAIN(a) Configs::dataManager->settingsRepo->a = ui->a->toPlainText();

#define P_LOAD_INT(a)                    \
    ui->a->setText(Int2String(bean->a)); \
    ui->a->setValidator(QRegExpValidator_Number);
#define P_SAVE_INT(a) bean->a = ui->a->text().toInt();

#define D_LOAD_INT(a)                                  \
    ui->a->setText(Int2String(Configs::dataManager->settingsRepo->a)); \
    ui->a->setValidator(QRegExpValidator_Number);
#define D_SAVE_INT(a) Configs::dataManager->settingsRepo->a = ui->a->text().toInt();

#define P_LOAD_COMBO_STRING(a) ui->a->setCurrentText(bean->a);
#define P_SAVE_COMBO_STRING(a) bean->a = ui->a->currentText();

#define D_LOAD_COMBO_STRING(a) ui->a->setCurrentText(Configs::dataManager->settingsRepo->a);
#define D_SAVE_COMBO_STRING(a) Configs::dataManager->settingsRepo->a = ui->a->currentText();

#define P_LOAD_COMBO_INT(a) ui->a->setCurrentIndex(bean->a);
#define P_SAVE_COMBO_INT(a) bean->a = ui->a->currentIndex();

#define D_LOAD_BOOL(a) ui->a->setChecked(Configs::dataManager->settingsRepo->a);
#define D_SAVE_BOOL(a) Configs::dataManager->settingsRepo->a = ui->a->isChecked();

#define P_LOAD_BOOL(a) ui->a->setChecked(bean->a);
#define P_SAVE_BOOL(a) bean->a = ui->a->isChecked();

#define D_LOAD_INT_ENABLE(i, e)                             \
    if (Configs::dataManager->settingsRepo->i > 0) {                        \
        ui->e->setChecked(true);                            \
        ui->i->setText(Int2String(Configs::dataManager->settingsRepo->i));  \
    } else {                                                \
        ui->e->setChecked(false);                           \
        ui->i->setText(Int2String(-Configs::dataManager->settingsRepo->i)); \
    }                                                       \
    ui->i->setValidator(QRegExpValidator_Number);
#define D_SAVE_INT_ENABLE(i, e)                         \
    if (ui->e->isChecked()) {                           \
        Configs::dataManager->settingsRepo->i = ui->i->text().toInt();  \
    } else {                                            \
        Configs::dataManager->settingsRepo->i = -ui->i->text().toInt(); \
    }

#define C_EDIT_JSON_ALLOW_EMPTY(a)                                    \
    auto editor = new JsonEditor(QString2QJsonObject(CACHE.a), this); \
    auto result = editor->OpenEditor();                               \
    CACHE.a = QJsonObject2QString(result, true);                      \
    if (result.isEmpty()) CACHE.a = "";                               \
    editor->deleteLater();

//

#define ADD_ASTERISK(parent)                                         \
    for (auto label: parent->findChildren<QLabel *>()) {             \
        auto text = label->text();                                   \
        if (!label->toolTip().isEmpty() && !text.endsWith("*")) {    \
            label->setText(text + "*");                              \
        }                                                            \
    }                                                                \
    for (auto checkBox: parent->findChildren<QCheckBox *>()) {       \
        auto text = checkBox->text();                                \
        if (!checkBox->toolTip().isEmpty() && !text.endsWith("*")) { \
            checkBox->setText(text + "*");                           \
        }                                                            \
    }
