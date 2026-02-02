#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_xrayvless.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditXrayVless;
}
QT_END_NAMESPACE

class EditXrayVless : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditXrayVless(QWidget *parent = nullptr);

    ~EditXrayVless() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;
private:
    Ui::EditXrayVless *ui;
    std::shared_ptr<Configs::Profile> ent;
};
