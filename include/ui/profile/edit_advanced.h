#pragma once

#include <QDialog>
#include <QStringList>
#include "ui_edit_advanced.h"
#include "include/database/entities/Profile.h"

namespace Ui {
class EditAdvanced;
}

namespace Validation { class Form; }

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

    void on_cert_public_key_sha256_clicked();

    void on_client_cert_clicked();

    void on_client_key_clicked();

private:
    struct InterfaceFields {
        bool *system = nullptr;
        QString *interface_name = nullptr;
        QString *udp_timeout = nullptr;
        QString *udp_mapping = nullptr;
        QString *udp_filtering = nullptr;
        int *udp_nat_max = nullptr;
    };

    [[nodiscard]] InterfaceFields GetInterfaceFields() const;

    void loadEndpoint();

    [[nodiscard]] Configs::EndpointSource endpointFromUi() const;

    void syncEndpoint();

    void setCacheButtonText(QPushButton *button, const QStringList &value);

    void editCachedList(QPushButton *button, const QString &title, QStringList &target);

    void bindValidation();

    Ui::EditAdvanced *ui;
    Validation::Form *form{};
    std::shared_ptr<Configs::Profile> ent;

    QStringList m_systemInterfaces;
    QStringList m_systemIpv4Addresses;
    QStringList m_systemIpv6Addresses;

    struct {
        QStringList echConfig;
        QStringList certSha256;
        QStringList certPublicKeySha256;
        QStringList clientCert;
        QStringList clientKey;
    } CACHE;
};