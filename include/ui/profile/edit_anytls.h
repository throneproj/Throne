#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_anytls.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditAnyTls;
}
QT_END_NAMESPACE

class EditAnyTls : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditAnyTls(QWidget *parent = nullptr);

    ~EditAnyTls() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

private:
    Ui::EditAnyTls *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
