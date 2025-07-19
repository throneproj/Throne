#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_extra_core.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditExtraCore;
}
QT_END_NAMESPACE

class EditExtraCore : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditExtraCore(QWidget *parent = nullptr);

    ~EditExtraCore() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

private:
    Ui::EditExtraCore *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
