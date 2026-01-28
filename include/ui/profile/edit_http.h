#ifndef EDIT_HTTP_H
#define EDIT_HTTP_H

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_http.h"

namespace Ui {
    class EditHttp;
}

class EditHttp : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditHttp(QWidget *parent = nullptr);

    ~EditHttp() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

private:
    Ui::EditHttp *ui;
    std::shared_ptr<Configs::Profile> ent;
};

#endif
