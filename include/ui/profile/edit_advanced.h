#pragma once

#include <QDialog>
#include "ui_edit_advanced.h"
#include "include/database/entities/Profile.h"

namespace Ui {
class EditAdvanced;
}

class EditAdvanced : public QDialog
{
    Q_OBJECT

public:
    EditAdvanced(QWidget *parent, const std::shared_ptr<Configs::Profile> &_ent);

    ~EditAdvanced() override;

public slots:
    void accept() override;

private slots:
    void on_ech_config_clicked();

    void on_cert_sha256_clicked();

    void on_client_cert_clicked();

    void on_client_key_clicked();

private:
    Ui::EditAdvanced *ui;
    std::shared_ptr<Configs::Profile> ent;

    struct {
        QStringList echConfig;
        QStringList certSha256;
        QStringList clientCert;
        QStringList clientKey;
    } CACHE;
};