#include "include/ui/setting/RouteItem.h"
#include "include/api/RPC.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"

#include <QMessageBox>

void adjustComboBoxWidth(const QComboBox *comboBox) {
    int maxWidth = 0;

    // Iterate over all items and calculate the width required
    for (int i = 0; i < comboBox->count(); ++i) {
        QFontMetrics fontMetrics(comboBox->font());
        int itemWidth = fontMetrics.horizontalAdvance(comboBox->itemText(i));
        maxWidth = qMax(maxWidth, itemWidth);
    }

    // Add some padding to the width to avoid text being too close to the edge
    maxWidth += 30;

    // Set the minimum width for the drop-down menu
    comboBox->view()->setMinimumWidth(maxWidth);
}

int RouteItem::getIndexOf(const QString& name) const {
    for (int i=0;i<chain->Rules.size();i++) {
        if (chain->Rules[i]->name == name) return i;
    }

    return -1;
}

QString get_outbound_name(int id) {
    // -1 is proxy -2 is direct -3 is block -4 is dns-out
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

    // make a copy
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

    // setup rule set helper
    for (const auto& item : ruleSetMap) {
        geo_items.append(QString::fromStdString(item.first));
    }
    rule_set_editor = new AutoCompleteTextEdit("", geo_items, this);
    ui->rule_attr_data->layout()->addWidget(rule_set_editor);
    ui->rule_attr_data->adjustSize();
    rule_set_editor->hide();
    connect(rule_set_editor, &QPlainTextEdit::textChanged, this, [=,this]{
        if (currentIndex == -1) return;
        auto currentVal = rule_set_editor->toPlainText().split('\n');
        chain->Rules[currentIndex]->set_field_value(ui->rule_attr->currentText(), currentVal);
        updateRulePreview();
    });

    std::map<QString, int> valueMap;
    for (auto &item: chain->Rules) {
        auto baseName = item->name;
        int randPart;
        if (baseName == "") {
            randPart = int(GetRandomUint64()%1000);
            baseName = "rule_" + Int2String(randPart);
            lastNum = std::max(lastNum, randPart);
        }
        while (true) {
            valueMap[baseName]++;
            if (valueMap[baseName] > 1) {
                valueMap[baseName]--;
                randPart = int(GetRandomUint64()%1000);
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
    // init outbound map
    outboundMap[0] = -1;
    outboundMap[1] = -2;
    for (const auto& item: outboundIdNamePairs) {
        outboundMap[outboundMap.size()] = item.first;
        outbounds << item.second;
    }

    // limit
    ui->rule_attr_selector->setMaxCount(1000);

    ui->route_name->setText(chain->name);
    ui->rule_attr->addItems(Configs::RouteRule::get_attributes());
    adjustComboBoxWidth(ui->rule_attr);
    ui->rule_attr_text->hide();
    ui->rule_attr_data->setTitle("");
    ui->rule_preview->setReadOnly(true);
    updateRuleSection();

    ui->def_out->setCurrentText(Configs::outboundIDToString(chain->defaultOutboundID));

    // simple rules setup
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

    connect(ui->howtouse_button, &QPushButton::clicked, this, [=,this]()
    {
        runOnUiThread([=,this]
        {
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

    // Подключаем ОЧИСТКУ ошибок при изменении текста
    // Ошибки отключаются чтобы пользователь мог исправить
    connect(simpleDirect, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (directHighlighter && directHighlighter->isErrorHighlightEnabled())
            directHighlighter->setErrorHighlightEnabled(false);
    });
    connect(simpleBlock, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (blockHighlighter && blockHighlighter->isErrorHighlightEnabled())
            blockHighlighter->setErrorHighlightEnabled(false);
    });
    connect(simpleProxy, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (proxyHighlighter && proxyHighlighter->isErrorHighlightEnabled())
            proxyHighlighter->setErrorHighlightEnabled(false);
    });

    // Для Enhanced полей
    connect(enhanced_direct_ip, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (enhanced_direct_ip_highlighter && enhanced_direct_ip_highlighter->isErrorHighlightEnabled())
            enhanced_direct_ip_highlighter->setErrorHighlightEnabled(false);
    });
    connect(enhanced_direct_domain, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (enhanced_direct_domain_highlighter && enhanced_direct_domain_highlighter->isErrorHighlightEnabled())
            enhanced_direct_domain_highlighter->setErrorHighlightEnabled(false);
    });
    connect(enhanced_direct_process, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (enhanced_direct_process_highlighter && enhanced_direct_process_highlighter->isErrorHighlightEnabled())
            enhanced_direct_process_highlighter->setErrorHighlightEnabled(false);
    });
    connect(enhanced_direct_ruleset, &QTextEdit::textChanged, this, [=,this]() {
        if (enhanced_direct_ruleset_highlighter && enhanced_direct_ruleset_highlighter->isErrorHighlightEnabled())
            enhanced_direct_ruleset_highlighter->setErrorHighlightEnabled(false);
    });
    connect(enhanced_proxy_ip, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (enhanced_proxy_ip_highlighter && enhanced_proxy_ip_highlighter->isErrorHighlightEnabled())
            enhanced_proxy_ip_highlighter->setErrorHighlightEnabled(false);
    });
    connect(enhanced_proxy_domain, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (enhanced_proxy_domain_highlighter && enhanced_proxy_domain_highlighter->isErrorHighlightEnabled())
            enhanced_proxy_domain_highlighter->setErrorHighlightEnabled(false);
    });
    connect(enhanced_proxy_process, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (enhanced_proxy_process_highlighter && enhanced_proxy_process_highlighter->isErrorHighlightEnabled())
            enhanced_proxy_process_highlighter->setErrorHighlightEnabled(false);
    });
    connect(enhanced_proxy_ruleset, &QTextEdit::textChanged, this, [=,this]() {
        if (enhanced_proxy_ruleset_highlighter && enhanced_proxy_ruleset_highlighter->isErrorHighlightEnabled())
            enhanced_proxy_ruleset_highlighter->setErrorHighlightEnabled(false);
    });
    connect(enhanced_block_ip, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (enhanced_block_ip_highlighter && enhanced_block_ip_highlighter->isErrorHighlightEnabled())
            enhanced_block_ip_highlighter->setErrorHighlightEnabled(false);
    });
    connect(enhanced_block_domain, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (enhanced_block_domain_highlighter && enhanced_block_domain_highlighter->isErrorHighlightEnabled())
            enhanced_block_domain_highlighter->setErrorHighlightEnabled(false);
    });
    connect(enhanced_block_process, &AutoCompleteTextEdit::textChanged, this, [=,this]() {
        if (enhanced_block_process_highlighter && enhanced_block_process_highlighter->isErrorHighlightEnabled())
            enhanced_block_process_highlighter->setErrorHighlightEnabled(false);
    });
    connect(enhanced_block_ruleset, &QTextEdit::textChanged, this, [=,this]() {
        if (enhanced_block_ruleset_highlighter && enhanced_block_ruleset_highlighter->isErrorHighlightEnabled())
            enhanced_block_ruleset_highlighter->setErrorHighlightEnabled(false);
    });

    connect(ui->route_import_json, &QPushButton::clicked, this, [=,this] {
        auto w = new QDialog(this);
        w->setWindowTitle("Import JSON Array");
        w->setWindowModality(Qt::ApplicationModal);

        auto line = 0;
        auto layout = new QGridLayout;
        w->setLayout(layout);

        auto *tEdit = new QTextEdit;
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

        auto *buttons = new QDialogButtonBox;
        buttons->setOrientation(Qt::Horizontal);
        buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons, line, 0);

        connect(buttons, &QDialogButtonBox::accepted, w, [=,this]{
           auto err = new QString;
           auto parsed = Configs::RouteProfile::parseJsonArray(QString2QJsonArray(tEdit->toPlainText()), err);
           if (!err->isEmpty()) {
               MessageBoxInfo(tr("Invalid JSON Array"), tr("The provided input cannot be parsed to a valid route rule array:\n") + *err);
               return;
           }
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

    connect(ui->rule_name, &QLineEdit::textChanged, this, [=,this](const QString& text) {
        if (currentIndex == -1) return;
        chain->Rules[currentIndex]->name = QString(text);
        auto ruleNameCursorPosition = ui->rule_name->cursorPosition();
        updateRouteItemsView(); 
        ui->rule_name->setCursorPosition(ruleNameCursorPosition);
    });

    connect(ui->rule_attr_selector, &QComboBox::currentTextChanged, this, [=,this](const QString& text){
       if (currentIndex == -1) return;
       if (ui->rule_attr->currentText() == "outbound")
       {
           chain->Rules[currentIndex]->set_field_value("outbound", {QString::number(outboundMap[ui->rule_attr_selector->currentIndex()])});
           updateRulePreview();
           return;
       }
       chain->Rules[currentIndex]->set_field_value(ui->rule_attr->currentText(), {QString(text)});
       updateRulePreview();
    });

    connect(ui->rule_attr_text, &QPlainTextEdit::textChanged, this, [=,this] {
        if (currentIndex == -1) return;
        auto currentVal = ui->rule_attr_text->toPlainText().split('\n');
        chain->Rules[currentIndex]->set_field_value(ui->rule_attr->currentText(), currentVal);
        updateRulePreview();
    });

    connect(ui->route_items, &QListWidget::currentRowChanged, this, [=,this](const int idx) {
        if (idx == -1) return;
        currentIndex = idx;
        updateRuleSection();
    });

    connect(ui->rule_attr, &QComboBox::currentTextChanged, this, [=,this](const QString& text){
        updateRuleSection();
    });

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [=,this]{
        accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, [=,this]{
       QDialog::reject();
    });

    deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);

    connect(deleteShortcut, &QShortcut::activated, this, [=,this]{
        on_delete_route_item_clicked();
    });

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
    ui->route_items->clear();
    if (chain->IsEmpty()) return;

    for (const auto& item: chain->Rules) {
        ui->route_items->addItem(item->name);
    }
    if (currentIndex != -1) ui->route_items->setCurrentRow(currentIndex);
}

void RouteItem::updateRuleSection() {
    if (currentIndex == -1) return;

    auto ruleItem = chain->Rules[currentIndex];
    auto currentAttr = ui->rule_attr->currentText();
    switch (ruleItem->get_input_type(currentAttr)) {
        case Configs::trufalse: {
            if (ruleItem->canEditAttr(currentAttr)) {
                ui->rule_attr_selector->setEnabled(true);
            } else {
                ui->rule_attr_selector->setEnabled(false);
            }
            QStringList items = {"false", "true"};
            QString currentVal = ruleItem->get_current_value_bool(currentAttr);
            showSelectItem(items, currentVal);
            break;
        }
        case Configs::select: {
            if (ruleItem->canEditAttr(currentAttr)) {
                ui->rule_attr_selector->setEnabled(true);
            } else {
                ui->rule_attr_selector->setEnabled(false);
            }
            if (currentAttr == "outbound")
            {
                // due to the need for mapping, we handle this in a different way...
                showSelectItem(outbounds, get_outbound_name(ruleItem->outboundID));
                break;
            }
            auto items = Configs::RouteRule::get_values_for_field(currentAttr);
            auto currentVal = ruleItem->get_current_value_string(currentAttr)[0];
            showSelectItem(items, currentVal);
            break;
        }
        case Configs::text: {
            if (ruleItem->canEditAttr(currentAttr)) {
                rule_set_editor->setEnabled(true);
                ui->rule_attr_text->setEnabled(true);
            } else {
                rule_set_editor->setEnabled(false);
                ui->rule_attr_text->setEnabled(false);
            }
            auto currentItems = ruleItem->get_current_value_string(currentAttr);
            showTextEnterItem(currentItems, currentAttr == "rule_set");
            break;
        }
    }
    ui->rule_name->setText(ruleItem->name);

    updateRulePreview();
}

void RouteItem::updateRulePreview() {
    if (currentIndex == -1) return;

    ui->rule_preview->setText(QJsonObject2QString(chain->Rules[currentIndex]->get_rule_json(true), false));
}

void RouteItem::setDefaultRuleData(const QString& currentData) {
    ui->rule_attr->setCurrentText("ip_version");
    ui->rule_attr_data->setTitle("ip_version");
    showSelectItem(Configs::RouteRule::get_values_for_field("ip_version"), currentData);
}

void RouteItem::showSelectItem(const QStringList& items, const QString& currentItem) {
    ui->rule_attr_text->hide();
    rule_set_editor->hide();
    ui->rule_attr_selector->clear();
    ui->rule_attr_selector->show();
    ui->rule_attr_selector->addItems(items);
    ui->rule_attr_selector->setCurrentText(currentItem);
    adjustComboBoxWidth(ui->rule_attr_selector);
    adjustSize();
}

void RouteItem::showTextEnterItem(const QStringList& items, bool isRuleSet) {
    ui->rule_attr_selector->hide();
    if (isRuleSet) {
        ui->rule_attr_text->hide();
        rule_set_editor->clear();
        rule_set_editor->show();
        rule_set_editor->setPlainText(items.join('\n'));
    } else {
        rule_set_editor->hide();
        ui->rule_attr_text->clear();
        ui->rule_attr_text->show();
        ui->rule_attr_text->setPlainText(items.join('\n'));
    }
    adjustSize();
}

void RouteItem::on_new_route_item_clicked() {
    auto routeItem = std::make_shared<Configs::RouteRule>();
    routeItem->name = "rule_" + Int2String(++lastNum);
    chain->Rules << routeItem;
    currentIndex = chain->Rules.size() - 1;
    ui->rule_name->setText(routeItem->name);
    currentIndex = chain->Rules.size()-1;

    updateRouteItemsView();
    updateRuleSection();
}

void RouteItem::on_moveup_route_item_clicked() {
    if (currentIndex == -1 || currentIndex == 0) return;
    auto curr = chain->Rules[currentIndex];
    chain->Rules[currentIndex] = chain->Rules[currentIndex-1];
    chain->Rules[currentIndex-1] = curr;
    currentIndex--;
    updateRouteItemsView();
}

void RouteItem::on_movedown_route_item_clicked() {
    if (currentIndex == -1 || currentIndex == chain->Rules.size() - 1) return;
    auto curr = chain->Rules[currentIndex];
    chain->Rules[currentIndex] = chain->Rules[currentIndex+1];
    chain->Rules[currentIndex+1] = curr;
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
