#pragma once

#include <QFont>
#include <QList>
#include <QObject>

#include "include/engine/services/ServiceTypes.h"

inline QString GroupDeleteRefusalText(const Engine::ServiceError &error) {
    if (error.code == QLatin1String("last_group")) return QObject::tr("Throne needs at least one group.");
    if (error.code == QLatin1String("busy")) return QObject::tr("This group's subscription is being updated. Try again when it finishes.");
    if (error.code == QLatin1String("editing")) return QObject::tr("Close the editor of this group or of one of its profiles first.");
    if (error.code == QLatin1String("routes_editing")) return QObject::tr("Close Routing Settings or the editor of this group's routing profile first.");
    if (error.code == QLatin1String("not_found")) return QObject::tr("This group no longer exists.");
    return error.message;
}

QString ImportTargetRefusalText(const Engine::ServiceError &error);

// For a settings window whose commit SettingsService refused.
QString SettingsApplyErrorText(const Engine::ServiceError &error);

// Appended to a delete confirmation; empty unless a group uses one of the profiles as its front or landing proxy.
QString HopReferenceWarning(const QList<int> &profileIds, int deletedGroup = -1);

QFont BuildMonospaceFont(const QString &family, int pointSize);
QFont ConfiguredMonospaceFont();

#define QRegExpValidator_Number new QRegularExpressionValidator(QRegularExpression("^[0-9]+$"), this)

// Settings windows: S_LOAD_* read the snapshot_ member, S_SAVE_* write a local Configs::SettingsValues named proposed.
#define S_LOAD_STRING(a) ui->a->setText(snapshot_.a);
#define S_SAVE_STRING(a) proposed.a = ui->a->text();

#define S_LOAD_INT(a)                        \
    ui->a->setText(Int2String(snapshot_.a)); \
    ui->a->setValidator(QRegExpValidator_Number);
#define S_SAVE_INT(a) proposed.a = ui->a->text().toInt();

#define S_LOAD_COMBO_STRING(a) ui->a->setCurrentText(snapshot_.a);
#define S_SAVE_COMBO_STRING(a) proposed.a = ui->a->currentText();

#define S_LOAD_BOOL(a) ui->a->setChecked(snapshot_.a);
#define S_SAVE_BOOL(a) proposed.a = ui->a->isChecked();

#define S_LOAD_INT_ENABLE(i, e)                   \
    if (snapshot_.i > 0) {                        \
        ui->e->setChecked(true);                  \
        ui->i->setText(Int2String(snapshot_.i));  \
    } else {                                      \
        ui->e->setChecked(false);                 \
        ui->i->setText(Int2String(-snapshot_.i)); \
    }                                             \
    ui->i->setValidator(QRegExpValidator_Number);
#define S_SAVE_INT_ENABLE(i, e)              \
    if (ui->e->isChecked()) {                \
        proposed.i = ui->i->text().toInt();  \
    } else {                                 \
        proposed.i = -ui->i->text().toInt(); \
    }

#define C_EDIT_JSON_ALLOW_EMPTY(a, schemaRef)                                            auto editor = new JsonEdit::JsonEditorDialog(QString2QJsonObject(CACHE.a), this);     editor->SetValidator(JsonEdit::SingBoxValidator(schemaRef));                          auto result = editor->OpenEditor();                                                   CACHE.a = QJsonObject2QString(result, true);                                          if (result.isEmpty()) CACHE.a = "";                                                   editor->deleteLater();

#define ADD_ASTERISK(parent)                                         \
    for (auto label: parent->findChildren<QLabel *>()) {             \
        auto text = label->text();                                   \
        if (!label->toolTip().isEmpty() && !text.endsWith("*")) {    \
            label->setText(text + "*");                              \
        }                                                            \
    }                                                                \
    for (auto button: parent->findChildren<QAbstractButton *>()) {   \
        if (!button->inherits("QCheckBox") && !button->inherits("QRadioButton")) continue; \
        auto text = button->text();                                  \
        if (!button->toolTip().isEmpty() && !text.endsWith("*")) {   \
            button->setText(text + "*");                             \
        }                                                            \
    }
