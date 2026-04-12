#include "include/ui/setting/RouteItem.h"
#include "include/api/RPC.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"

#include <QMessageBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QGridLayout>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTextEdit>

void adjustComboBoxWidth(const QComboBox *comboBox) {
    int maxWidth = 0;

    for (int i = 0; i < comboBox->count(); ++i) {
        QFontMetrics fontMetrics(comboBox->font());
        int itemWidth = fontMetrics.horizontalAdvance(comboBox->itemText(i));
        maxWidth = qMax(maxWidth, itemWidth);
    }

    maxWidth += 30;
    comboBox->view()->setMinimumWidth(maxWidth);
}

QString get_outbound_name(int id) {
    if (id == -1) return "proxy";
    if (id == -2) return "direct";
    if (auto profile = Configs::dataManager->profilesRepo->GetProfile(id)) return profile->name;
    return "INVALID OUTBOUND";
}

// Вспомогательные функции для фильтрации текста правил по типу

/// Фильтрует строки, содержащие ip: и ruleset:geoip-* (но не ruleset URI)
QString RouteItem::filterIPs(const QString& text)
{
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    QStringList result;
    for (const QString& line : lines)
    {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        // Фильтруем комментарии по типу содержимого после #
        if (trimmed.startsWith('#')) {
            // Извлекаем часть после # для определения типа
            QString commentContent = trimmed.mid(1).trimmed();

            // Сохраняем комментарий если он относится к IP или ruleset:geoip
            if (commentContent.startsWith("ip:") || commentContent.startsWith("ruleset:geoip")) {
                result << trimmed;
            }
            // Пропускаем комментарии других типов
            continue;
        }

        // Сначала пропускаем все ruleset URI (http, https, file и т.д.)
        // кроме ruleset:geoip-* которые нам нужны
        if (trimmed.startsWith("ruleset:") && !trimmed.startsWith("ruleset:geoip"))
        {
            continue;
        }

        // Пропускаем строки, которые явно принадлежат другим категориям
        if (trimmed.startsWith("domain:") || trimmed.startsWith("suffix:") ||
            trimmed.startsWith("keyword:") || trimmed.startsWith("regex:") ||
            trimmed.startsWith("ruleset:geosite") ||
            trimmed.startsWith("processName:") || trimmed.startsWith("processPath:"))
        {
            continue;
        }

        // Если строка начинается с ip: или ruleset:geoip - сохраняем
        if (trimmed.startsWith("ip:") || trimmed.startsWith("ruleset:geoip"))
        {
            result << trimmed;
        }
        // Если нет префикса, но похоже на CIDR (содержит /) - добавляем ip:
        // Проверяем что это не URL (не содержит ://)
        else if (trimmed.contains('/') && !trimmed.contains("://"))
        {
            // Можно проверить валидность CIDR, но для простоты просто добавим ip:
            result << "ip:" + trimmed;
        }
    }
    return result.join('\n');
}

/// Фильтрует строки с domain, suffix, keyword, regex, ruleset:geosite- (но не ruleset URI)
QString RouteItem::filterDomains(const QString& text)
{
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    QStringList result;
    for (const QString& line : lines)
    {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        // Фильтруем комментарии по типу содержимого после #
        if (trimmed.startsWith('#')) {
            QString commentContent = trimmed.mid(1).trimmed();

            // Сохраняем комментарий если он относится к доменам или geosite ruleset
            if (commentContent.startsWith("domain:") || commentContent.startsWith("suffix:") ||
                commentContent.startsWith("keyword:") || commentContent.startsWith("regex:") ||
                commentContent.startsWith("ruleset:geosite")) {
                result << trimmed;
            }
            // Пропускаем комментарии других типов
            continue;
        }

        // Сначала пропускаем все ruleset URI (http, https, file и т.д.)
        // кроме ruleset:geosite-* которые нам нужны
        if (trimmed.startsWith("ruleset:") && !trimmed.startsWith("ruleset:geosite"))
        {
            continue;
        }

        // Пропускаем строки, которые явно принадлежат другим категориям
        if (trimmed.startsWith("ip:") || trimmed.startsWith("ruleset:geoip") ||
            trimmed.startsWith("processName:") || trimmed.startsWith("processPath:"))
        {
            continue;
        }

        // Проверяем известные префиксы доменов
        if (trimmed.startsWith("domain:") || trimmed.startsWith("suffix:") ||
            trimmed.startsWith("keyword:") || trimmed.startsWith("regex:") ||
            trimmed.startsWith("ruleset:geosite"))
        {
            result << trimmed;
        }
        // Автоопределение: если содержит точку и не похоже на IP/process
        else if (trimmed.contains('.') && !trimmed.contains(':') &&
                 !trimmed.contains('/') && !trimmed.contains('\\') && !trimmed.startsWith("/") && !trimmed.endsWith(".exe", Qt::CaseInsensitive))
        {
            // Просто домен без префикса - считаем что это domain:
            result << "domain:" + trimmed;
        }
        // Если начинается с . - это suffix
        else if (trimmed.startsWith('.') && !trimmed.contains(':'))
        {
            result << "suffix:" + trimmed;
        }
    }
    return result.join('\n');
}

/// Фильтрует строки с processName: и processPath:
QString RouteItem::filterProcesses(const QString& text)
{
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    QStringList result;
    for (const QString& line : lines)
    {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        // Фильтруем комментарии по типу содержимого после #
        if (trimmed.startsWith('#')) {
            QString commentContent = trimmed.mid(1).trimmed();

            // Сохраняем комментарий если он относится к процессам
            if (commentContent.startsWith("processName:") || commentContent.startsWith("processPath:")) {
                result << trimmed;
            }
            // Пропускаем комментарии других типов
            continue;
        }

        // Сначала пропускаем ruleset URI - проверяем ДО всех остальных проверок
        if (trimmed.startsWith("ruleset:"))
        {
            continue;
        }

        // Пропускаем строки, которые явно принадлежат другим категориям (IP и Domains)
        if (trimmed.startsWith("ip:") || trimmed.startsWith("ruleset:geoip") ||
            trimmed.startsWith("domain:") || trimmed.startsWith("suffix:") ||
            trimmed.startsWith("keyword:") || trimmed.startsWith("regex:") ||
            trimmed.startsWith("ruleset:geosite"))
        {
            continue;
        }

        // Если уже есть префикс processName: или processPath: - оставляем как есть
        if (trimmed.startsWith("processName:") || trimmed.startsWith("processPath:"))
        {
            result << trimmed;
        }
        // Автоопределение: если заканчивается на .exe - точно процесс
        else if (trimmed.endsWith(".exe", Qt::CaseInsensitive))
        {
            result << "processPath:" + trimmed;
        }
        // Если содержит путь-разделитель - processPath (ruleset уже исключен выше)
        else if (trimmed.contains('/') || trimmed.contains('\\'))
        {
            result << "processPath:" + trimmed;
        }
        // Простое имя без специальных символов - processName
        // Пропускаем если похоже на домен (содержит точку и не содержит пути)
        else if (trimmed.contains('.') && !trimmed.contains('/') && !trimmed.contains('\\') && !trimmed.startsWith('.'))
        {
            continue; // Это похоже на домен, не процесс
        }
        else
        {
            result << "processName:" + trimmed;
        }
    }
    return result.join('\n');
}

/// Фильтрует строки с ruleset URI (только URL/пути, исключая geoip и geosite)
QString RouteItem::filterRulesets(const QString& text)
{
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    QStringList result;
    for (const QString& line : lines)
    {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        // Фильтруем комментарии по типу содержимого после #
        if (trimmed.startsWith('#')) {
            QString commentContent = trimmed.mid(1).trimmed();

            // Сохраняем ТОЛЬКО комментарии ruleset URI (не geoip и не geosite)
            // Проверяем что после # идёт ruleset: и это не geoip/geosite
            if (commentContent.startsWith("ruleset:")) {
                // Извлекаем URI после ruleset:
                QString uri = commentContent.mid(8).trimmed(); // "ruleset:" = 8 chars
                if (!uri.startsWith("geoip") && !uri.startsWith("geosite")) {
                    result << trimmed;
                }
            }
            // Пропускаем комментарии других типов
            continue;
        }

        // Сохраняем только ruleset: с URL или путями
        // Исключаем ruleset:geoip-* (идут в IP) и ruleset:geosite-* (идут в Domains)
        if (trimmed.startsWith("ruleset:"))
        {
            QString uri = trimmed.mid(8).trimmed();
            if (uri.startsWith("geoip") || uri.startsWith("geosite"))
            {
                continue; // Эти идут в IP и Domains respectively
            }
            result << trimmed;
        }
    }
    return result.join('\n');
}

QStringList get_all_outbounds() {
    auto profilesNames = Configs::dataManager->profilesRepo->GetAllProfileNames();

    return profilesNames;
}

RouteItem::RouteItem(QWidget *parent, const std::shared_ptr<Configs::RouteProfile>& routeChain)
    : QDialog(parent), ui(new Ui::RouteItem) {
    ui->setupUi(this);

    chain = std::make_shared<Configs::RouteProfile>(*routeChain);

    // Последняя активная вкладка - начинаем с Basic (0)
    lastTabIndex = 0;

    // add the default rule if empty
    if (chain->IsEmpty()) {
        auto routeItem = std::make_shared<Configs::RouteRule>();
        routeItem->name = "dns-hijack";
        routeItem->protocol = "dns";
        routeItem->action = "hijack-dns";
        chain->Rules << routeItem;
    }

    std::map<QString, int> valueMap;
    for (auto &item: chain->Rules) {
        auto baseName = item->name;
        int randPart;
        if (baseName == "") {
            randPart = int(GetRandomUint64() % 1000);
            baseName = "rule_" + Int2String(randPart);
            lastNum = std::max(lastNum, randPart);
        }
        while (true) {
            valueMap[baseName]++;
            if (valueMap[baseName] > 1) {
                valueMap[baseName]--;
                randPart = int(GetRandomUint64() % 1000);
                baseName = "rule_" + Int2String(randPart);
                lastNum = std::max(lastNum, randPart);
                continue;
            }
            item->name = baseName;
            break;
        }
        ui->route_items->addItem(item->name);
    }

    outbounds = {"proxy", "direct"};
    auto outboundIdNamePairs = Configs::dataManager->profilesRepo->GetAllProfileIDNameMapped();
    outboundMap[0] = -1;
    outboundMap[1] = -2;
    for (const auto& item: outboundIdNamePairs) {
        outboundMap[outboundMap.size()] = item.first;
        outbounds << item.second;
    }

    for (const auto& item : ruleSetMap) {
        geo_items.append(QString::fromStdString(item.first));
    }

    ui->route_name->setText(chain->name);
    ui->rule_preview->setReadOnly(true);
    ui->rule_attr_tabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    ui->rule_attr_tabs->setTabsClosable(true);
    connect(ui->rule_attr_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (currentIndex < 0) return;
        const QString tabText = ui->rule_attr_tabs->tabText(index);
        if (tabText == QStringLiteral("+")) return;
        applyAttributeVisibilityChange(tabText, false);
    });

    connect(ui->rule_attr_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (currentIndex >= 0 && index >= 0 && index < ui->rule_attr_tabs->count())
            chain->Rules[currentIndex]->uiActiveAttributeTabLabel = ui->rule_attr_tabs->tabText(index);
        if (QWidget* w = ui->rule_attr_tabs->currentWidget()) {
            w->updateGeometry();
            w->adjustSize();
        }
        ui->rule_attr_tabs->updateGeometry();
    });

    ensurePlusTabBuiltOnce();

    ui->def_out->setCurrentText(Configs::outboundIDToString(chain->defaultOutboundID));

    QStringList ruleItems = {"domain:", "suffix:", "regex:", "keyword:", "ip:", "processName:", "processPath:", "ruleset:"};
    for (const auto& item : ruleSetMap) {
        ruleItems.append("ruleset:" + QString::fromStdString(item.first));
    }
    simpleDirect = new AutoCompleteTextEdit("", ruleItems, this);
    simpleBlock = new AutoCompleteTextEdit("", ruleItems, this);
    simpleProxy = new AutoCompleteTextEdit("", ruleItems, this);

    ui->simple_direct_box->layout()->replaceWidget(ui->simple_direct, simpleDirect);
    ui->simple_block_box->layout()->replaceWidget(ui->simple_block, simpleBlock);
    ui->simple_proxy_box->layout()->replaceWidget(ui->simple_proxy, simpleProxy);
    ui->simple_direct->hide();
    ui->simple_block->hide();
    ui->simple_proxy->hide();

    // Подключаем подсветку комментариев (#) для Basic полей
    directHighlighter = new RouteRuleHighlighter(simpleDirect->document(), false);
    blockHighlighter = new RouteRuleHighlighter(simpleBlock->document(), false);
    proxyHighlighter = new RouteRuleHighlighter(simpleProxy->document(), false);

    // Enhanced Basic поля - создаём AutoCompleteTextEdit для каждой комбинации действия и типа
    // Используем тот же список подсказок, что и для Basic
    enhanced_direct_ip = new AutoCompleteTextEdit("", ruleItems, this);
    enhanced_direct_domain = new AutoCompleteTextEdit("", ruleItems, this);
    enhanced_direct_process = new AutoCompleteTextEdit("", ruleItems, this);
    enhanced_proxy_ip = new AutoCompleteTextEdit("", ruleItems, this);
    enhanced_proxy_domain = new AutoCompleteTextEdit("", ruleItems, this);
    enhanced_proxy_process = new AutoCompleteTextEdit("", ruleItems, this);
    enhanced_block_ip = new AutoCompleteTextEdit("", ruleItems, this);
    enhanced_block_domain = new AutoCompleteTextEdit("", ruleItems, this);
    enhanced_block_process = new AutoCompleteTextEdit("", ruleItems, this);

    // Rulesets поля - используем QTextEdit из UI напрямую
    enhanced_direct_ruleset = ui->enhanced_direct_ruleset;
    enhanced_proxy_ruleset = ui->enhanced_proxy_ruleset;
    enhanced_block_ruleset = ui->enhanced_block_ruleset;

    // Заменяем стандартные QTextEdit в UI на наши AutoCompleteTextEdit
    ui->enhanced_direct_ip_box->layout()->replaceWidget(ui->enhanced_direct_ip, enhanced_direct_ip);
    ui->enhanced_direct_domain_box->layout()->replaceWidget(ui->enhanced_direct_domain, enhanced_direct_domain);
    ui->enhanced_direct_process_box->layout()->replaceWidget(ui->enhanced_direct_process, enhanced_direct_process);
    ui->enhanced_proxy_ip_box->layout()->replaceWidget(ui->enhanced_proxy_ip, enhanced_proxy_ip);
    ui->enhanced_proxy_domain_box->layout()->replaceWidget(ui->enhanced_proxy_domain, enhanced_proxy_domain);
    ui->enhanced_proxy_process_box->layout()->replaceWidget(ui->enhanced_proxy_process, enhanced_proxy_process);
    ui->enhanced_block_ip_box->layout()->replaceWidget(ui->enhanced_block_ip, enhanced_block_ip);
    ui->enhanced_block_domain_box->layout()->replaceWidget(ui->enhanced_block_domain, enhanced_block_domain);
    ui->enhanced_block_process_box->layout()->replaceWidget(ui->enhanced_block_process, enhanced_block_process);

    // Rulesets поля уже QTextEdit в UI, не нужно заменять
    // Скрываем оригинальные QTextEdit (кроме rulesets - они уже видны)
    ui->enhanced_direct_ip->hide();
    ui->enhanced_direct_domain->hide();
    ui->enhanced_direct_process->hide();
    ui->enhanced_proxy_ip->hide();
    ui->enhanced_proxy_domain->hide();
    ui->enhanced_proxy_process->hide();
    ui->enhanced_block_ip->hide();
    ui->enhanced_block_domain->hide();
    ui->enhanced_block_process->hide();

    // Подключаем подсветку комментариев (#) для Enhanced Basic полей
    enhanced_direct_ip_highlighter = new RouteRuleHighlighter(enhanced_direct_ip->document(), false);
    enhanced_direct_domain_highlighter = new RouteRuleHighlighter(enhanced_direct_domain->document(), false);
    enhanced_direct_process_highlighter = new RouteRuleHighlighter(enhanced_direct_process->document(), false);
    enhanced_direct_ruleset_highlighter = new RouteRuleHighlighter(enhanced_direct_ruleset->document(), false);
    enhanced_proxy_ip_highlighter = new RouteRuleHighlighter(enhanced_proxy_ip->document(), false);
    enhanced_proxy_domain_highlighter = new RouteRuleHighlighter(enhanced_proxy_domain->document(), false);
    enhanced_proxy_process_highlighter = new RouteRuleHighlighter(enhanced_proxy_process->document(), false);
    enhanced_proxy_ruleset_highlighter = new RouteRuleHighlighter(enhanced_proxy_ruleset->document(), false);
    enhanced_block_ip_highlighter = new RouteRuleHighlighter(enhanced_block_ip->document(), false);
    enhanced_block_domain_highlighter = new RouteRuleHighlighter(enhanced_block_domain->document(), false);
    enhanced_block_process_highlighter = new RouteRuleHighlighter(enhanced_block_process->document(), false);
    enhanced_block_ruleset_highlighter = new RouteRuleHighlighter(enhanced_block_ruleset->document(), false);

    simpleDirect->setPlainText(chain->GetSimpleRules(Configs::direct));
    simpleBlock->setPlainText(chain->GetSimpleRules(Configs::block));
    simpleProxy->setPlainText(chain->GetSimpleRules(Configs::proxy));

    // Инициализация Enhanced Basic полей - фильтруем текст из Basic для отображения
    // IPs: только ip: и ruleset:geoip-*
    enhanced_direct_ip->setPlainText(filterIPs(chain->GetSimpleRules(Configs::direct)));
    enhanced_proxy_ip->setPlainText(filterIPs(chain->GetSimpleRules(Configs::proxy)));
    enhanced_block_ip->setPlainText(filterIPs(chain->GetSimpleRules(Configs::block)));

    // Domains: domain, suffix, keyword, regex, ruleset:geosite-*
    enhanced_direct_domain->setPlainText(filterDomains(chain->GetSimpleRules(Configs::direct)));
    enhanced_proxy_domain->setPlainText(filterDomains(chain->GetSimpleRules(Configs::proxy)));
    enhanced_block_domain->setPlainText(filterDomains(chain->GetSimpleRules(Configs::block)));

    // Processes: processName: и processPath:
    enhanced_direct_process->setPlainText(filterProcesses(chain->GetSimpleRules(Configs::direct)));
    enhanced_proxy_process->setPlainText(filterProcesses(chain->GetSimpleRules(Configs::proxy)));
    enhanced_block_process->setPlainText(filterProcesses(chain->GetSimpleRules(Configs::block)));

    // Rulesets: все ruleset URI (кроме geoip- и geosite- которые идут в IP/Domains)
    enhanced_direct_ruleset->setPlainText(filterRulesets(chain->GetSimpleRules(Configs::direct)));
    enhanced_proxy_ruleset->setPlainText(filterRulesets(chain->GetSimpleRules(Configs::proxy)));
    enhanced_block_ruleset->setPlainText(filterRulesets(chain->GetSimpleRules(Configs::block)));
    

    connect(ui->tabWidget->tabBar(), &QTabBar::currentChanged, this, [=, this]() {
        if (ui->tabWidget->tabBar()->currentIndex() == 1) {
            QString res;
            res += chain->UpdateSimpleRules(simpleDirect->toPlainText(), Configs::direct);
            res += chain->UpdateSimpleRules(simpleBlock->toPlainText(), Configs::block);
            res += chain->UpdateSimpleRules(simpleProxy->toPlainText(), Configs::proxy);
            if (!res.isEmpty()) {
                runOnUiThread([=] {
                    MessageBoxWarning(tr("Invalid rules"), tr("Some rules could not be added:\n") + res);
                });
            }
            if (currentIndex >= 0)
                persistCurrentRuleAttrTabLabel();
            currentIndex = -1;
            updateRouteItemsView();
            updateRuleSection();
        } else {
            if (currentIndex >= 0)
                persistCurrentRuleAttrTabLabel();
            updateRouteItemsView();
            updateRuleSection();
            simpleDirect->setPlainText(chain->GetSimpleRules(Configs::direct));
            simpleBlock->setPlainText(chain->GetSimpleRules(Configs::block));
            simpleProxy->setPlainText(chain->GetSimpleRules(Configs::proxy));
        }
    });

    connect(ui->howtouse_button, &QPushButton::clicked, this, [=]() {
        runOnUiThread([=] {
            MessageBoxInfo(tr("Simple rule manual"), Configs::Information::SimpleRuleInfo);
        });
    });

    // Подключаем сворачивание/разворачивание для Rulesets полей
    // При клике на кнопку меняем значок и показываем/скрываем поле
    // Синхронизируем состояние всех трёх кнопок
    auto syncRulesetToggles = [this](bool checked) {
        ui->enhanced_direct_ruleset_toggle->setText(checked ? "▼ Ruleset (URI)" : "▶ Ruleset (URI)");
        ui->enhanced_direct_ruleset->setVisible(checked);
        ui->enhanced_proxy_ruleset_toggle->setText(checked ? "▼ Ruleset (URI)" : "▶ Ruleset (URI)");
        ui->enhanced_proxy_ruleset->setVisible(checked);
        ui->enhanced_block_ruleset_toggle->setText(checked ? "▼ Ruleset (URI)" : "▶ Ruleset (URI)");
        ui->enhanced_block_ruleset->setVisible(checked);
    };

    connect(ui->enhanced_direct_ruleset_toggle, &QPushButton::toggled, this, [=](bool checked) {
        syncRulesetToggles(checked);
    });
    connect(ui->enhanced_proxy_ruleset_toggle, &QPushButton::toggled, this, [=](bool checked) {
        syncRulesetToggles(checked);
    });
    connect(ui->enhanced_block_ruleset_toggle, &QPushButton::toggled, this, [=](bool checked) {
        syncRulesetToggles(checked);
    });

    // Нет textChanged сигналов - сохранение только при переключении вкладок

    // Обработчик переключения вкладок - СИНХРОНИЗАЦИЯ при переходе
    connect(ui->tabWidget->tabBar(), &QTabBar::currentChanged, this, [=,this]()
    {
        int currentIdx = ui->tabWidget->tabBar()->currentIndex();

        if (currentIdx == 2)  // Переход НА Advanced - сохраняем всё в цепь
        {
            // Сначала собираем данные с ТЕКУЩЕЙ вкладки перед переходом
            if (lastTabIndex == 0) {
                // С Basic - обновляем цепь из Basic
                chain->UpdateSimpleRules(simpleDirect->toPlainText(), Configs::direct);
                chain->UpdateSimpleRules(simpleBlock->toPlainText(), Configs::block);
                chain->UpdateSimpleRules(simpleProxy->toPlainText(), Configs::proxy);
            } else if (lastTabIndex == 1) {
                // С Basic+ - обновляем цепь из Enhanced
                updateChainFromEnhanced(Configs::direct);
                updateChainFromEnhanced(Configs::proxy);
                updateChainFromEnhanced(Configs::block);
            }

            currentIndex = -1;
            updateRouteItemsView();
            updateRuleSection();
        }
        else if (currentIdx == 1)  // Переход НА Basic+ - сохраняем Basic и загружаем Enhanced
        {
            // Сохраняем Basic в цепь
            chain->UpdateSimpleRules(simpleDirect->toPlainText(), Configs::direct);
            chain->UpdateSimpleRules(simpleBlock->toPlainText(), Configs::block);
            chain->UpdateSimpleRules(simpleProxy->toPlainText(), Configs::proxy);

            // Обновляем Enhanced из цепи (с фильтрацией)
            refreshEnhancedFromChain();
        }
        else  // Переход НА Basic (индекс 0) - сохраняем Enhanced и загружаем Basic
        {
            if (lastTabIndex == 1) {
                // Сохраняем Enhanced в цепь перед переходом
                updateChainFromEnhanced(Configs::direct);
                updateChainFromEnhanced(Configs::proxy);
                updateChainFromEnhanced(Configs::block);
            }

            // Обновляем Basic из цепи
            simpleDirect->setPlainText(chain->GetSimpleRules(Configs::direct));
            simpleBlock->setPlainText(chain->GetSimpleRules(Configs::block));
            simpleProxy->setPlainText(chain->GetSimpleRules(Configs::proxy));
        }

        lastTabIndex = currentIdx;
    });


    connect(ui->route_import_json, &QPushButton::clicked, this, [=,this] {
        auto w = new QDialog(this);
        w->setWindowTitle("Import JSON Array");
        w->setWindowModality(Qt::ApplicationModal);

        auto line = 0;
        auto layout = new QGridLayout(w);
        w->setLayout(layout);

        auto *tEdit = new QTextEdit(w);
        tEdit->setPlaceholderText("[\n"
            "      {\n"
            "        \"action\": \"hijack-dns\",\n"
            "        \"protocol\": \"dns\"\n"
            "      },\n"
            "      {\n"
            "        \"action\": \"reject\",\n"
            "        \"protocol\": \"udp\"\n"
            "      }\n"
            "    ]");
        layout->addWidget(tEdit, line++, 0);

        auto *buttons = new QDialogButtonBox(w);
        buttons->setOrientation(Qt::Horizontal);
        buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons, line, 0);

        connect(buttons, &QDialogButtonBox::accepted, w, [=, this] {
           auto err = new QString;
           auto parsed = Configs::RouteProfile::parseJsonArray(QString2QJsonArray(tEdit->toPlainText()), err);
           if (!err->isEmpty()) {
               MessageBoxInfo(tr("Invalid JSON Array"), tr("The provided input cannot be parsed to a valid route rule array:\n") + *err);
               return;
           }
           if (currentIndex >= 0)
               persistCurrentRuleAttrTabLabel();
           chain->ResetRules();
           chain->Rules << parsed;
           currentIndex = -1;
           updateRouteItemsView();
           updateRuleSection();

           w->accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

        w->exec();
        w->deleteLater();
    });

    connect(ui->rule_name, &QLineEdit::textChanged, this, [=, this](const QString& text) {
        if (currentIndex == -1) return;
        chain->Rules[currentIndex]->name = QString(text);
        auto ruleNameCursorPosition = ui->rule_name->cursorPosition();
        updateRouteItemsView();
        ui->rule_name->setCursorPosition(ruleNameCursorPosition);
    });

    connect(ui->route_items, &QListWidget::currentRowChanged, this, [=, this](const int idx) {
        if (idx == -1) {
            if (currentIndex >= 0)
                persistCurrentRuleAttrTabLabel();
            currentIndex = -1;
            updateRuleSection();
            return;
        }
        if (currentIndex >= 0)
            persistCurrentRuleAttrTabLabel();
        currentIndex = idx;
        updateRuleSection();
    });

    connect(ui->rule_action_combo, &QComboBox::currentTextChanged, this, [=, this](const QString& text) {
        if (currentIndex < 0) return;
        chain->Rules[currentIndex]->set_field_value(QStringLiteral("action"), {text});
        updateRulePreview();
    });

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [=, this] {
        accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, [=, this] {
       QDialog::reject();
    });

    deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);

    connect(deleteShortcut, &QShortcut::activated, this, [=, this] {
        on_delete_route_item_clicked();
    });

    updateRuleSection();
    adjustSize();
}

RouteItem::~RouteItem() {
    delete ui;
}

QString RouteItem::collectFromEnhancedFields(AutoCompleteTextEdit* ip, AutoCompleteTextEdit* domain,
                                              AutoCompleteTextEdit* process, QTextEdit* ruleset)
{
    // Просто собирает текст из 4 полей без нормализации
    QStringList result;

    if (ip && !ip->toPlainText().isEmpty())
        result << ip->toPlainText().trimmed();

    if (domain && !domain->toPlainText().isEmpty())
        result << domain->toPlainText().trimmed();

    if (process && !process->toPlainText().isEmpty())
        result << process->toPlainText().trimmed();

    if (ruleset && !ruleset->toPlainText().isEmpty())
        result << ruleset->toPlainText().trimmed();

    return result.join('\n');
}

// === Реализация вспомогательных методов Enhanced Basic ===

QString RouteItem::updateChainFromEnhanced(Configs::simpleAction action)
{
    // Обновляет chain из полей Enhanced Basic для заданного действия
    // Собирает текст из всех четырёх Enhanced полей и соответствующего Basic поля
    // для сохранения комментариев, и вызывает UpdateSimpleRules один раз
    // Возвращает строку с ошибками от UpdateSimpleRules

    AutoCompleteTextEdit* ipField = nullptr;
    AutoCompleteTextEdit* domainField = nullptr;
    AutoCompleteTextEdit* processField = nullptr;
    QTextEdit* rulesetField = nullptr;

    // Определяем, какие Enhanced поля соответствуют заданному действию
    switch (action)
    {
    case Configs::direct:
        ipField = enhanced_direct_ip;
        domainField = enhanced_direct_domain;
        processField = enhanced_direct_process;
        rulesetField = enhanced_direct_ruleset;
        break;
    case Configs::proxy:
        ipField = enhanced_proxy_ip;
        domainField = enhanced_proxy_domain;
        processField = enhanced_proxy_process;
        rulesetField = enhanced_proxy_ruleset;
        break;
    case Configs::block:
        ipField = enhanced_block_ip;
        domainField = enhanced_block_domain;
        processField = enhanced_block_process;
        rulesetField = enhanced_block_ruleset;
        break;
    default:
        return {};
    }

    // Собираем все нормализованные строки из полей
    QStringList allNormalizedLines;

    auto collectFromText = [this](QTextEdit* field, QStringList& out) {
        if (!field) return;

        QStringList lines = field->toPlainText().split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines)
        {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;

            // Сохраняем комментарии как есть
            if (trimmed.startsWith('#')) {
                out << trimmed;
                continue;
            }

            out << trimmed; // Правила сохраняем как есть
        }
    };

    auto collectFromAutoComplete = [this](AutoCompleteTextEdit* field, QStringList& out) {
        if (!field) return;

        QStringList lines = field->toPlainText().split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines)
        {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;

            // Сохраняем комментарии как есть
            if (trimmed.startsWith('#')) {
                out << trimmed;
                continue;
            }

            QString normalized = trimmed;

            // Определяем тип поля по указателю
            if (field == enhanced_direct_ip || field == enhanced_proxy_ip || field == enhanced_block_ip)
            {
                // IP поле: ожидаем ip: или ruleset:geoip-
                if (!trimmed.startsWith("ip:") && !trimmed.startsWith("ruleset:"))
                {
                    if (trimmed.contains('/'))
                        normalized = "ip:" + trimmed;
                    else if (trimmed.startsWith("geoip-"))
                        normalized = "ruleset:" + trimmed;
                }
            }
            else if (field == enhanced_direct_domain || field == enhanced_proxy_domain || field == enhanced_block_domain)
            {
                // Domain поле: domain:, suffix:, keyword:, regex:, ruleset:geosite-
                if (!trimmed.startsWith("domain:") && !trimmed.startsWith("suffix:") &&
                    !trimmed.startsWith("keyword:") && !trimmed.startsWith("regex:") &&
                    !trimmed.startsWith("ruleset:"))
                {
                    if (trimmed.startsWith('.'))
                        normalized = "suffix:" + trimmed;
                    else if (trimmed.contains('.'))
                        normalized = "domain:" + trimmed;
                }
            }
            else if (field == enhanced_direct_process || field == enhanced_proxy_process || field == enhanced_block_process)
            {
                // Process поле: processName: или processPath:
                if (!trimmed.startsWith("processName:") && !trimmed.startsWith("processPath:"))
                {
                    if (trimmed.contains('/') || trimmed.contains('\\') || trimmed.endsWith(".exe", Qt::CaseInsensitive))
                        normalized = "processPath:" + trimmed;
                    else
                        normalized = "processName:" + trimmed;
                }
            }

            out << normalized;
        }
    };

    collectFromAutoComplete(ipField, allNormalizedLines);
    collectFromAutoComplete(domainField, allNormalizedLines);
    collectFromAutoComplete(processField, allNormalizedLines);
    collectFromText(rulesetField, allNormalizedLines);

    // Вызываем UpdateSimpleRules один раз со всеми нормализованными строками
    QString combinedText = allNormalizedLines.join('\n');
    return chain->UpdateSimpleRules(combinedText, action);
}

QString RouteItem::collectEnhancedToSimple()
{
    // Собирает данные из всех 9 Enhanced Basic полей и обновляет chain
    // Возвращает агрегированную строку с ошибками
    QString errors;
    errors += updateChainFromEnhanced(Configs::direct);
    errors += updateChainFromEnhanced(Configs::proxy);
    errors += updateChainFromEnhanced(Configs::block);
    return errors;
}

void RouteItem::refreshEnhancedFromChain()
{
    // Обновляет все 12 Enhanced Basic полей из chain
    // Вызывается при переходе на Basic+ из Basic或 Advanced

    // Блокируем сигналы чтобы избежать лишних вызовов
    QSignalBlocker b1(enhanced_direct_ip);
    QSignalBlocker b2(enhanced_direct_domain);
    QSignalBlocker b3(enhanced_direct_process);
    QSignalBlocker b4(enhanced_direct_ruleset);
    QSignalBlocker b5(enhanced_proxy_ip);
    QSignalBlocker b6(enhanced_proxy_domain);
    QSignalBlocker b7(enhanced_proxy_process);
    QSignalBlocker b8(enhanced_proxy_ruleset);
    QSignalBlocker b9(enhanced_block_ip);
    QSignalBlocker b10(enhanced_block_domain);
    QSignalBlocker b11(enhanced_block_process);
    QSignalBlocker b12(enhanced_block_ruleset);

    enhanced_direct_ip->setPlainText(filterIPs(chain->GetSimpleRules(Configs::direct)));
    enhanced_proxy_ip->setPlainText(filterIPs(chain->GetSimpleRules(Configs::proxy)));
    enhanced_block_ip->setPlainText(filterIPs(chain->GetSimpleRules(Configs::block)));

    enhanced_direct_domain->setPlainText(filterDomains(chain->GetSimpleRules(Configs::direct)));
    enhanced_proxy_domain->setPlainText(filterDomains(chain->GetSimpleRules(Configs::proxy)));
    enhanced_block_domain->setPlainText(filterDomains(chain->GetSimpleRules(Configs::block)));

    enhanced_direct_process->setPlainText(filterProcesses(chain->GetSimpleRules(Configs::direct)));
    enhanced_proxy_process->setPlainText(filterProcesses(chain->GetSimpleRules(Configs::proxy)));
    enhanced_block_process->setPlainText(filterProcesses(chain->GetSimpleRules(Configs::block)));

    enhanced_direct_ruleset->setPlainText(filterRulesets(chain->GetSimpleRules(Configs::direct)));
    enhanced_proxy_ruleset->setPlainText(filterRulesets(chain->GetSimpleRules(Configs::proxy)));
    enhanced_block_ruleset->setPlainText(filterRulesets(chain->GetSimpleRules(Configs::block)));
}



void RouteItem::accept() {
    chain->name = ui->route_name->text();

    if (chain->name == "") {
        MessageBoxWarning(tr("Invalid operation"), tr("Cannot create Route Profile with empty name"));
        return;
    }

    // Сначала СИНХРОНИЗИРУЕМ вкладки чтобы они имели одинаковые данные
    int currentTab = ui->tabWidget->tabBar()->currentIndex();

    if (currentTab == 0) {
        // Активна Basic - обновляем Enhanced из Basic через цепь
        chain->UpdateSimpleRules(simpleDirect->toPlainText(), Configs::direct);
        chain->UpdateSimpleRules(simpleBlock->toPlainText(), Configs::block);
        chain->UpdateSimpleRules(simpleProxy->toPlainText(), Configs::proxy);
        // Enhanced теперь прочитает из цепи при следующем refreshEnhancedFromChain()
    } else {
        // Активна Basic+ - обновляем Basic из Enhanced через цепь
        updateChainFromEnhanced(Configs::direct);
        updateChainFromEnhanced(Configs::proxy);
        updateChainFromEnhanced(Configs::block);
        // Basic теперь прочитает из цепи
    }

    // Теперь сохраняем ВСЁ из цепи (теперь обе вкладки синхронизированы)
    chain->FilterEmptyRules();

    if (chain->IsEmpty()) {
        MessageBoxInfo(tr("Empty Route Profile"), tr("No valid rules are in the profile"));
        return;
    }

    chain->defaultOutboundID = Configs::stringToOutboundID(ui->def_out->currentText());

    emit settingsChanged(chain);

    QDialog::accept();
}

void RouteItem::updateRouteItemsView() {
    const QSignalBlocker listBlocker(ui->route_items);
    ui->route_items->clear();
    if (chain->IsEmpty()) return;

    for (const auto& item: chain->Rules) {
        ui->route_items->addItem(item->name);
    }
    if (currentIndex != -1) ui->route_items->setCurrentRow(currentIndex);
}

void RouteItem::syncRuleActionCombo() {
    if (currentIndex < 0) return;
    ui->rule_action_combo->blockSignals(true);
    ui->rule_action_combo->clear();
    ui->rule_action_combo->addItems(Configs::RouteRule::get_values_for_field(QStringLiteral("action")));
    ui->rule_action_combo->setCurrentText(chain->Rules[currentIndex]->action);
    adjustComboBoxWidth(ui->rule_action_combo);
    auto rule = chain->Rules[currentIndex];
    if (rule->canEditAttr("action")) {
        ui->rule_action_combo->setEnabled(true);
    } else {
        ui->rule_action_combo->setEnabled(false);
    }
    ui->rule_action_combo->blockSignals(false);
}

QWidget* RouteItem::makeAttributeEditorPage(const QString& attr) {
    auto* container = new QWidget;
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(8, 8, 8, 8);
    const auto rule = chain->Rules[currentIndex];
    const bool editable = rule->canEditAttr(attr);

    const auto addDisabled = [editable](QWidget* w) { w->setEnabled(editable); };

    switch (Configs::RouteRule::get_input_type(attr)) {
        case Configs::trufalse: {
            auto* cb = new QComboBox(container);
            cb->addItems({QStringLiteral("false"), QStringLiteral("true")});
            cb->setCurrentText(rule->get_current_value_bool(attr));
            connect(cb, &QComboBox::currentTextChanged, this, [this, attr](const QString& t) {
                if (currentIndex < 0) return;
                chain->Rules[currentIndex]->set_field_value(attr, {t});
                updateRulePreview();
            });
            addDisabled(cb);
            lay->addWidget(cb);
            break;
        }
        case Configs::select: {
            auto* cb = new QComboBox(container);
            if (attr == QStringLiteral("outbound")) {
                cb->addItems(outbounds);
                cb->setCurrentText(get_outbound_name(rule->outboundID));
                connect(cb, &QComboBox::currentTextChanged, this, [this, cb] {
                    if (currentIndex < 0) return;
                    chain->Rules[currentIndex]->set_field_value(QStringLiteral("outbound"),
                        {QString::number(outboundMap[cb->currentIndex()])});
                    updateRulePreview();
                });
            } else {
                cb->addItems(Configs::RouteRule::get_values_for_field(attr));
                const auto cur = rule->get_current_value_string(attr);
                cb->setCurrentText(cur.isEmpty() ? QString() : cur[0]);
                connect(cb, &QComboBox::currentTextChanged, this, [this, attr](const QString& t) {
                    if (currentIndex < 0) return;
                    chain->Rules[currentIndex]->set_field_value(attr, {t});
                    updateRulePreview();
                });
            }
            addDisabled(cb);
            adjustComboBoxWidth(cb);
            lay->addWidget(cb);
            break;
        }
        case Configs::text: {
            if (attr == QStringLiteral("rule_set")) {
                auto* ed = new AutoCompleteTextEdit("", geo_items, container);
                ed->setPlainText(rule->get_current_value_string(attr).join('\n'));
                ed->setMinimumHeight(100);
                ed->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                connect(ed, &QPlainTextEdit::textChanged, this, [this, attr, ed] {
                    if (currentIndex < 0) return;
                    chain->Rules[currentIndex]->set_field_value(attr, ed->toPlainText().split('\n'));
                    updateRulePreview();
                });
                addDisabled(ed);
                lay->addWidget(ed, 1);
            } else {
                auto* te = new QPlainTextEdit(container);
                te->setPlainText(rule->get_current_value_string(attr).join('\n'));
                te->setMinimumHeight(100);
                te->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                connect(te, &QPlainTextEdit::textChanged, this, [this, attr, te] {
                    if (currentIndex < 0) return;
                    chain->Rules[currentIndex]->set_field_value(attr, te->toPlainText().split('\n'));
                    updateRulePreview();
                });
                addDisabled(te);
                lay->addWidget(te, 1);
            }
            break;
        }
    }
    container->setMinimumHeight(100);
    container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    return container;
}

void RouteItem::ensurePlusTabBuiltOnce() {
    if (ruleAttrPlusList) return;

    auto* container = new QWidget;
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(8, 8, 8, 8);

    auto* hint = new QLabel(tr("Check attributes to show as tabs; unchecking clears their values."), container);
    hint->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ruleAttrPlusList = new QListWidget(container);
    ruleAttrPlusList->setMinimumHeight(100);
    ruleAttrPlusList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ruleAttrPlusList->setObjectName(QStringLiteral("route_rule_attr_plus_list"));
    ruleAttrPlusList->viewport()->installEventFilter(this);
    for (const QString& attr : Configs::RouteRule::tab_attributes()) {
        auto* it = new QListWidgetItem(attr);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Unchecked);
        ruleAttrPlusList->addItem(it);
    }
    lay->addWidget(hint);
    lay->addWidget(ruleAttrPlusList);
    container->setMinimumHeight(100);
    container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    ui->rule_attr_tabs->addTab(container, QStringLiteral("+"));
    ui->rule_attr_tabs->tabBar()->setTabButton(ui->rule_attr_tabs->count() - 1, QTabBar::RightSide, nullptr);
}

void RouteItem::removeAllAttributeTabsExceptPlus() {
    ensurePlusTabBuiltOnce();
    while (ui->rule_attr_tabs->count() > 1)
        ui->rule_attr_tabs->removeTab(0);
}

void RouteItem::syncPlusListCheckStatesFromRule() {
    if (!ruleAttrPlusList) return;
    const QSignalBlocker b(ruleAttrPlusList);
    if (currentIndex < 0) {
        ruleAttrPlusList->setEnabled(false);
        for (int i = 0; i < ruleAttrPlusList->count(); ++i)
            ruleAttrPlusList->item(i)->setCheckState(Qt::Unchecked);
        return;
    }
    ruleAttrPlusList->setEnabled(true);
    const auto rule = chain->Rules[currentIndex];
    for (int i = 0; i < ruleAttrPlusList->count(); ++i) {
        auto* it = ruleAttrPlusList->item(i);
        it->setCheckState(rule->uiVisibleAttributes.contains(it->text()) ? Qt::Checked : Qt::Unchecked);
        if (!rule->canEditAttr(it->text())) {
            it->setHidden(true);
        } else {
            it->setHidden(false);
        }
    }
}

void RouteItem::persistCurrentRuleAttrTabLabel() {
    if (currentIndex < 0) return;
    const int idx = ui->rule_attr_tabs->currentIndex();
    if (idx < 0 || idx >= ui->rule_attr_tabs->count()) return;
    chain->Rules[currentIndex]->uiActiveAttributeTabLabel = ui->rule_attr_tabs->tabText(idx);
}

void RouteItem::applyStoredRuleAttrTabSelection() {
    int sel = 0;
    if (currentIndex >= 0) {
        const QString& pref = chain->Rules[currentIndex]->uiActiveAttributeTabLabel;
        if (!pref.isEmpty()) {
            for (int i = 0; i < ui->rule_attr_tabs->count(); ++i) {
                if (ui->rule_attr_tabs->tabText(i) == pref) {
                    sel = i;
                    break;
                }
            }
        }
    }
    ui->rule_attr_tabs->setCurrentIndex(sel);
}

void RouteItem::applyAttributeVisibilityChange(const QString& attr, bool visible) {
    if (currentIndex < 0) return;
    persistCurrentRuleAttrTabLabel();
    auto r = chain->Rules[currentIndex];
    r->uiAttributeTabsSeeded = true;
    if (visible)
        r->uiVisibleAttributes.insert(attr);
    else {
        r->uiVisibleAttributes.remove(attr);
        r->clear_attribute_value(attr);
    }
    rebuildRuleAttributeTabs();
    updateRulePreview();
}

bool RouteItem::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* vp = qobject_cast<QWidget*>(watched);
        if (vp && vp->parentWidget()) {
            auto* lw = qobject_cast<QListWidget*>(vp->parentWidget());
            if (lw && lw->objectName() == QLatin1String("route_rule_attr_plus_list")) {
                auto* me = static_cast<QMouseEvent*>(event);
                if (me->button() == Qt::LeftButton) {
                    if (QListWidgetItem* item = lw->itemAt(me->pos())) {
                        lw->setCurrentItem(item);
                        const bool toChecked = (item->checkState() != Qt::Checked);
                        {
                            const QSignalBlocker b(lw);
                            item->setCheckState(toChecked ? Qt::Checked : Qt::Unchecked);
                        }
                        applyAttributeVisibilityChange(item->text(), toChecked);
                        return true;
                    }
                }
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

void RouteItem::rebuildRuleAttributeTabs() {
    ensurePlusTabBuiltOnce();

    ui->rule_attr_tabs->blockSignals(true);
    removeAllAttributeTabsExceptPlus();

    if (currentIndex < 0) {
        syncPlusListCheckStatesFromRule();
        applyStoredRuleAttrTabSelection();
        ui->rule_attr_tabs->blockSignals(false);
        return;
    }

    const auto rule = chain->Rules[currentIndex];
    for (const QString& attr : Configs::RouteRule::tab_attributes()) {
        if (!rule->uiVisibleAttributes.contains(attr)) continue;
        const int beforePlus = ui->rule_attr_tabs->count() - 1;
        ui->rule_attr_tabs->insertTab(beforePlus, makeAttributeEditorPage(attr), attr);
    }

    syncPlusListCheckStatesFromRule();
    applyStoredRuleAttrTabSelection();

    ui->rule_attr_tabs->blockSignals(false);
}

void RouteItem::updateRuleSection() {
    if (currentIndex < 0) {
        {
            const QSignalBlocker nameBlocker(ui->rule_name);
            ui->rule_name->clear();
        }
        ui->rule_preview->clear();
        ui->rule_action_combo->blockSignals(true);
        ui->rule_action_combo->clear();
        ui->rule_action_combo->blockSignals(false);
        rebuildRuleAttributeTabs();
        return;
    }

    auto rule = chain->Rules[currentIndex];
    rule->ensure_ui_visible_attribute_tabs_seeded();
    {
        const QSignalBlocker nameBlocker(ui->rule_name);
        ui->rule_name->setText(rule->name);
    }
    syncRuleActionCombo();
    rebuildRuleAttributeTabs();
    updateRulePreview();
}

void RouteItem::updateRulePreview() {
    if (currentIndex == -1) return;

    ui->rule_preview->setPlainText(QJsonObject2QString(chain->Rules[currentIndex]->get_rule_json(true), false));
}

void RouteItem::on_new_route_item_clicked() {
    auto routeItem = std::make_shared<Configs::RouteRule>();
    routeItem->name = "rule_" + Int2String(++lastNum);
    chain->Rules << routeItem;
    currentIndex = chain->Rules.size() - 1;

    updateRouteItemsView();
    updateRuleSection();
}

void RouteItem::on_moveup_route_item_clicked() {
    if (currentIndex == -1 || currentIndex == 0) return;
    auto curr = chain->Rules[currentIndex];
    chain->Rules[currentIndex] = chain->Rules[currentIndex - 1];
    chain->Rules[currentIndex - 1] = curr;
    currentIndex--;
    updateRouteItemsView();
}

void RouteItem::on_movedown_route_item_clicked() {
    if (currentIndex == -1 || currentIndex == chain->Rules.size() - 1) return;
    auto curr = chain->Rules[currentIndex];
    chain->Rules[currentIndex] = chain->Rules[currentIndex + 1];
    chain->Rules[currentIndex + 1] = curr;
    currentIndex++;
    updateRouteItemsView();
}

void RouteItem::on_delete_route_item_clicked() {
    if (currentIndex == -1) return;
    chain->Rules.removeAt(currentIndex);
    if (chain->Rules.empty()) currentIndex = -1;
    else {
        currentIndex--;
        if (currentIndex == -1) currentIndex = 0;
    }
    updateRouteItemsView();
    updateRuleSection();
}
