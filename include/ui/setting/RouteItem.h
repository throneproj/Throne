#pragma once

#include <QWidget>
#include <QListWidgetItem>
#include <QDialog>
#include <QStringListModel>
#include <QShortcut>

#include "include/dataStore/RouteEntity.h"
#include "3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.hpp"
#include "ui_RouteItem.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class RouteItem;
}
QT_END_NAMESPACE

class RouteItem : public QDialog {
    Q_OBJECT

public:
    explicit RouteItem(QWidget *parent = nullptr, const std::shared_ptr<Configs::RoutingChain>& routeChain = nullptr, const std::map<std::string, std::string>& ruleSetMap = {});
    ~RouteItem() override;

    std::shared_ptr<Configs::RoutingChain> chain;
signals:
    void settingsChanged(std::shared_ptr<Configs::RoutingChain> routingChain);

private:
    Ui::RouteItem *ui;
    int currentIndex = -1;

    int lastNum = 0;

    QStringList geo_items;

    AutoCompleteTextEdit* rule_set_editor;

    QStringList current_helper_items;

    QStringListModel* helperModel;

    QShortcut* deleteShortcut;

    QStringList outbounds;

    std::map<int,int> outboundMap;

    AutoCompleteTextEdit* simpleDirect;

    AutoCompleteTextEdit* simpleBlock;

    AutoCompleteTextEdit* simpleProxy;

    [[nodiscard]] int getIndexOf(const QString& name) const;

    void showSelectItem(const QStringList& items, const QString& currentItem);

    void showTextEnterItem(const QStringList& items, bool isRuleSet);

    void setDefaultRuleData(const QString& currentData);

    void updateRuleSection();

    void updateRulePreview();

    void updateRouteItemsView();
private slots:
    void accept() override;

    void on_new_route_item_clicked();
    void on_moveup_route_item_clicked();
    void on_movedown_route_item_clicked();
    void on_delete_route_item_clicked();
};
