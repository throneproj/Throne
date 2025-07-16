#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_socks_http.h"

namespace Ui {
    class EditSocksHttp;
}

class EditSocksHttp : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditSocksHttp(QWidget *parent = nullptr);

    ~EditSocksHttp() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

private:
    Ui::EditSocksHttp *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
