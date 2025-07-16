#pragma once

#include <QDialog>
#include "include/dataStore/Group.hpp"
#include "ui_dialog_edit_group.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogEditGroup;
}
QT_END_NAMESPACE

class DialogEditGroup : public QDialog {
    Q_OBJECT

public:
    explicit DialogEditGroup(const std::shared_ptr<Configs::Group> &ent, QWidget *parent = nullptr);

    ~DialogEditGroup() override;

private:
    Ui::DialogEditGroup *ui;

    std::shared_ptr<Configs::Group> ent;

    struct {
        int front_proxy;
    } CACHE;

    struct {
        int landing_proxy;
    } LANDING;

private slots:

    void accept() override;

    QStringList load_proxy_items();

    int get_proxy_id(QString);

    QString get_proxy_name(int id);
};
