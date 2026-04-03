#pragma once

#include <QWidget>
#include <QListWidgetItem>
#include <QDialog>
#include <QStringListModel>
#include <QShortcut>

#include "3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.hpp"
#include "ui_RouteItem.h"
#include "include/database/entities/RouteProfile.h"
#include "include/ui/setting/RouteRuleHighlighter.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class RouteItem;
}
QT_END_NAMESPACE

class RouteItem : public QDialog {
    Q_OBJECT

public:
    explicit RouteItem(QWidget *parent = nullptr, const std::shared_ptr<Configs::RouteProfile>& routeChain = nullptr);
    ~RouteItem() override;

    std::shared_ptr<Configs::RouteProfile> chain;
signals:
    void settingsChanged(std::shared_ptr<Configs::RouteProfile> routingChain);

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

    // Enhanced Basic поля (12 текстовых редакторов для 3 действий × 4 типов)
    AutoCompleteTextEdit* enhanced_direct_ip;
    AutoCompleteTextEdit* enhanced_direct_domain;
    AutoCompleteTextEdit* enhanced_direct_process;
    QPushButton* enhanced_direct_ruleset_toggle;
    QTextEdit* enhanced_direct_ruleset;
    AutoCompleteTextEdit* enhanced_proxy_ip;
    AutoCompleteTextEdit* enhanced_proxy_domain;
    AutoCompleteTextEdit* enhanced_proxy_process;
    QPushButton* enhanced_proxy_ruleset_toggle;
    QTextEdit* enhanced_proxy_ruleset;
    AutoCompleteTextEdit* enhanced_block_ip;
    AutoCompleteTextEdit* enhanced_block_domain;
    AutoCompleteTextEdit* enhanced_block_process;
    QPushButton* enhanced_block_ruleset_toggle;
    QTextEdit* enhanced_block_ruleset;

    // Хайлайтеры для подсветки комментариев (#) в текстовых полях
    RouteRuleHighlighter* directHighlighter = nullptr;
    RouteRuleHighlighter* blockHighlighter = nullptr;
    RouteRuleHighlighter* proxyHighlighter = nullptr;

    // Хайлайтеры для Enhanced Basic полей (12 хайлайтеров)
    RouteRuleHighlighter* enhanced_direct_ip_highlighter = nullptr;
    RouteRuleHighlighter* enhanced_direct_domain_highlighter = nullptr;
    RouteRuleHighlighter* enhanced_direct_process_highlighter = nullptr;
    RouteRuleHighlighter* enhanced_direct_ruleset_highlighter = nullptr;
    RouteRuleHighlighter* enhanced_proxy_ip_highlighter = nullptr;
    RouteRuleHighlighter* enhanced_proxy_domain_highlighter = nullptr;
    RouteRuleHighlighter* enhanced_proxy_process_highlighter = nullptr;
    RouteRuleHighlighter* enhanced_proxy_ruleset_highlighter = nullptr;
    RouteRuleHighlighter* enhanced_block_ip_highlighter = nullptr;
    RouteRuleHighlighter* enhanced_block_domain_highlighter = nullptr;
    RouteRuleHighlighter* enhanced_block_process_highlighter = nullptr;
    RouteRuleHighlighter* enhanced_block_ruleset_highlighter = nullptr;

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

private:
    // Индекс последней активной вкладки для определения источника данных при переключении
    int lastTabIndex = 0;

    // Вспомогательные функции фильтрации текста правил по типу для Enhanced Basic
    QString filterIPs(const QString& text);
    QString filterDomains(const QString& text);
    QString filterProcesses(const QString& text);
    QString filterRulesets(const QString& text);

    // Обновляет chain из полей Enhanced Basic для заданного действия, возвращает строку с ошибками
    QString updateChainFromEnhanced(Configs::simpleAction action);

    // Собирает данные из всех Enhanced Basic полей и обновляет chain, возвращает строку с ошибками
    QString collectEnhancedToSimple();

    // Обновляет все Enhanced Basic поля из chain (при переходе из Advanced)
    void refreshEnhancedFromChain();

    // Вспомогательная функция для сбора текста из 4 Enhanced полей
    QString collectFromEnhancedFields(AutoCompleteTextEdit* ip, AutoCompleteTextEdit* domain,
                                      AutoCompleteTextEdit* process, QTextEdit* ruleset);

    // Показывает ошибки синтаксиса в диалоговом окне
    void showSyncErrors(const QString& errors);

    // Включает подсветку ошибок во всех текстовых полях
    void enableErrorHighlight();
};
