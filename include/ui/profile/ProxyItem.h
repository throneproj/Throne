#pragma once

#include <QWidget>
#include <QListWidgetItem>

#include "include/dataStore/ProxyEntity.hpp"
#include "ui_ProxyItem.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class ProxyItem;
}
QT_END_NAMESPACE

class QPushButton;

class ProxyItem : public QWidget {
    Q_OBJECT

public:
    explicit ProxyItem(QWidget *parent, const std::shared_ptr<Configs::ProxyEntity> &ent, QListWidgetItem *item);

    ~ProxyItem() override;

    void refresh_data();

    QPushButton *get_change_button();

    std::shared_ptr<Configs::ProxyEntity> ent;
    QListWidgetItem *item;
    bool remove_confirm = false;

private:
    Ui::ProxyItem *ui;

private slots:

    void on_remove_clicked();
};
