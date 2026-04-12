#pragma once

#include <QDialog>
#include "ui_dialog_edit_group.h"
#include "include/database/entities/Group.h"

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

    QMap<int, QString> proxyMapping;

    struct {
        int front_proxy;
    } CACHE;

    struct {
        int landing_proxy;
    } LANDING;

private slots:

    void accept() override;

    QString get_proxy_name(int id);
};
