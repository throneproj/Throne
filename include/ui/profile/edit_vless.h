#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_vless.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditVless;
}
QT_END_NAMESPACE

class EditVless : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditVless(QWidget *parent = nullptr);

    ~EditVless() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    QComboBox *_flow;
private:
    Ui::EditVless *ui;
    std::shared_ptr<Configs::Profile> ent;
};
