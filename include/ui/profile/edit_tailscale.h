#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_tailscale.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditTailScale;
}
QT_END_NAMESPACE

class EditTailScale : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditTailScale(QWidget *parent = nullptr);

    ~EditTailScale() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

private:
    Ui::EditTailScale *ui;
    std::shared_ptr<Configs::Profile> ent;
};
