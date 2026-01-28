#ifndef EDIT_TUIC_H
#define EDIT_TUIC_H

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_tuic.h"

namespace Ui {
    class EditTuic;
}

class EditTuic : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditTuic(QWidget *parent = nullptr);

    ~EditTuic() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

private:
    Ui::EditTuic *ui;
    std::shared_ptr<Configs::Profile> ent;
};

#endif // EDIT_TUIC_H
