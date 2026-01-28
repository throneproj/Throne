#ifndef EDIT_HYSTERIA_H
#define EDIT_HYSTERIA_H

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_hysteria.h"

namespace Ui {
    class EditHysteria;
}

class EditHysteria : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditHysteria(QWidget *parent = nullptr);

    ~EditHysteria() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    QComboBox *_protocol_version;

    void editHysteriaLayout(const QString& version);
private:
    Ui::EditHysteria *ui;
    std::shared_ptr<Configs::Profile> ent;
};

#endif
