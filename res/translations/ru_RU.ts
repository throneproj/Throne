<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru_RU">
<context>
    <name>ConnectionsFilterHeader</name>
    <message>
        <source>Disable Filter</source>
        <translation>Выключить фильтр</translation>
    </message>
    <message>
        <source>Enable Filter</source>
        <translation>Включить фильтр</translation>
    </message>
    <message>
        <source>Filter...</source>
        <translation>Фильтр...</translation>
    </message>
</context>
<context>
    <name>ConnectionsTreeModel</name>
    <message>
        <source>Source</source>
        <translation>Источник</translation>
    </message>
    <message>
        <source>System</source>
        <translation>Система</translation>
    </message>
    <message>
        <source>Process: %1
Active connections: %2
Total traffic: %3↑ %4↓
Total speed: %5/s↑ %6/s↓</source>
        <translation>Процесс: %1
Активные подключения: %2
Общий трафик: %3↑ %4↓
Общая скорость: %5/s↑ %6/s↓</translation>
    </message>
    <message>
        <source>Destination: %1
Connections: %2
Process: %3
Protocol: %4
Outbound: %5
Total traffic: %6↑ %7↓
Total speed: %8/s↑ %9/s↓</source>
        <translation>Пункт назначения: %1
Подключения: %2
Процесс: %3
Протокол: %4
Outbound: %5
Общий трафик: %6↑ %7↓
Общая скорость: %8/s↑ %9/s↓</translation>
    </message>
    <message>
        <source>Destination: %1
Process: %2
Protocol: %3
Outbound: %4</source>
        <translation>Пункт назначения: %1
Процесс: %2
Протокол: %3
Outbound: %4</translation>
    </message>
    <message>
        <source>Process / Destination</source>
        <translation>Процесс / Пункт назначения</translation>
    </message>
    <message>
        <source>Protocol</source>
        <translation>Протокол</translation>
    </message>
    <message>
        <source>Outbound</source>
        <translation>Outbound</translation>
    </message>
    <message>
        <source>Traffic</source>
        <translation>Трафик</translation>
    </message>
    <message>
        <source>Speed</source>
        <translation>Скорость</translation>
    </message>
    <message>
        <source>Click To Sort By Source</source>
        <translation>Нажмите для сортировки по источнику</translation>
    </message>
    <message>
        <source>Click To Sort By Process</source>
        <translation>Нажмите для сортировки по процессу</translation>
    </message>
    <message>
        <source>Click To Sort By Protocol</source>
        <translation>Нажмите для сортировки по протоколу</translation>
    </message>
    <message>
        <source>Click To Sort By Outbound</source>
        <translation>Нажмите для сортировки по Outbound</translation>
    </message>
    <message>
        <source>Click to sort by traffic; right-click to choose total/down/up</source>
        <translation>Нажмите, чтобы отсортировать по трафику; ПКМ — выбор: всего/получено/отправлено</translation>
    </message>
    <message>
        <source>Click to sort by speed; right-click to choose total/down/up</source>
        <translation>Нажмите, чтобы отсортировать по скорости; ПКМ — выбор: всего/скачивание/отдача</translation>
    </message>
</context>
<context>
    <name>DiagnosticsTab</name>
    <message>
        <source>Performance profile</source>
        <translation>Профилирование</translation>
    </message>
    <message>
        <source>Profiles the Core using pprof. The profile itself does not contain any information about your configs, but if you include the logs too, there will be information about your configs in them.</source>
        <translation>Профилирует ядро с помощью pprof. Сам профиль не содержит сведений о ваших конфигурациях, но если добавить логи, такие сведения будут в них.</translation>
    </message>
    <message>
        <source>Records where goroutines wait on locks and channels.</source>
        <translation>Записывает, где горутины ждут блокировок и каналов.</translation>
    </message>
    <message>
        <source>Lock contention</source>
        <translation>Конфликт блокировок (Lock contention)</translation>
    </message>
    <message>
        <source>Execution trace (larger file)</source>
        <translation>Трассировка выполнения (файл большего размера)</translation>
    </message>
    <message>
        <source>Logs contain visited domains and server addresses.</source>
        <translation>Логи содержат данные о посещённых доменах и адресах серверов.</translation>
    </message>
    <message>
        <source>Include Throne logs</source>
        <translation>Добавить логи Throne</translation>
    </message>
    <message>
        <source>Start</source>
        <translation>Запустить</translation>
    </message>
    <message>
        <source>Stop</source>
        <translation>Остановить</translation>
    </message>
    <message>
        <source>Show in folder</source>
        <translation>Показать в папке</translation>
    </message>
    <message>
        <source>Open folder</source>
        <translation>Открыть папку</translation>
    </message>
    <message>
        <source>Saved %1 (%2)</source>
        <translation>Сохранено: %1 (%2)</translation>
    </message>
    <message>
        <source>The core is not running.</source>
        <translation>Ядро не запущено.</translation>
    </message>
    <message>
        <source>Finishing…</source>
        <translation>Завершение…</translation>
    </message>
    <message>
        <source>Recording… %1 left</source>
        <translation>Запись… Осталось %1</translation>
    </message>
</context>
<context>
    <name>DialogAutoSelector</name>
    <message>
        <source>Auto Selector Stats</source>
        <translation>Статистика автовыбора</translation>
    </message>
    <message>
        <source>Only show profiles with problems</source>
        <translation>Показывать только профили с проблемами</translation>
    </message>
    <message>
        <source>Use this profile</source>
        <translation>Использовать этот профиль</translation>
    </message>
    <message>
        <source>Keep the selected profile in use instead of letting the ranking choose. Useful when several profiles measure much the same and you prefer one of them.

It stays a preference, not a lock: if that profile stops working the selector still moves on, and comes back to your choice once it recovers.</source>
        <translation>Продолжать использовать выбранный профиль, а не полагаться на выбор по рейтингу. Это полезно, когда несколько профилей показывают практически одинаковые результаты, а вы отдаете предпочтение одному из них.

Это остается предпочтением, а не фиксацией: если этот профиль перестанет работать, автовыбор переключится на другой, а после восстановления работы вернётся к вашему выбору.</translation>
    </message>
    <message>
        <source>Back to automatic</source>
        <translation>Вернуть автовыбор</translation>
    </message>
    <message>
        <source>Stop preferring a particular profile and let the ranking decide again.</source>
        <translation>Перестать отдавать предпочтение конкретному профилю и снова доверить выбор результатам измерений.</translation>
    </message>
    <message>
        <source>Check all now</source>
        <translation>Проверить все сейчас</translation>
    </message>
    <message>
        <source>Re-measure every running profile immediately instead of waiting for the next scheduled check.</source>
        <translation>Сразу перепроверить все профили, не дожидаясь следующей плановой проверки.</translation>
    </message>
    <message>
        <source>Re-checking every profile...</source>
        <translation>Повторная проверка всех профилей...</translation>
    </message>
    <message>
        <source>Profile</source>
        <translation>Профиль</translation>
    </message>
    <message>
        <source>Status</source>
        <translation>Статус</translation>
    </message>
    <message>
        <source>Latency</source>
        <translation>Задержка</translation>
    </message>
    <message>
        <source>Jitter</source>
        <translation>Джиттер</translation>
    </message>
    <message>
        <source>Checks</source>
        <translation>Проверки</translation>
    </message>
    <message>
        <source>Connects</source>
        <translation>Подключения</translation>
    </message>
    <message>
        <source>Last OK</source>
        <translation>Последний успех</translation>
    </message>
    <message>
        <source>Notes</source>
        <translation>Примечания</translation>
    </message>
    <message>
        <source>Could not change the profile: %1</source>
        <translation>Не удалось изменить профиль: %1</translation>
    </message>
    <message>
        <source>Back to automatic — the selector will choose again.</source>
        <translation>Автовыбор возвращен — профиль снова выбирается автоматически.</translation>
    </message>
    <message>
        <source>Now using your chosen profile.</source>
        <translation>Сейчас используется выбранный вами профиль.</translation>
    </message>
    <message>
        <source>No auto selector is running.</source>
        <translation>Автовыбор не запущен.</translation>
    </message>
    <message>
        <source>Checks are paused because this machine has no network connection. No profile is being blamed for it, and the ranking is frozen until the connection returns.</source>
        <translation>Проверки приостановлены: у компьютера нет подключения к сети. Профили из-за этого не считаются неработающими, а рейтинг заморожен до восстановления подключения.</translation>
    </message>
    <message>
        <source>Last check round %1.</source>
        <translation>Последняя проверка %1.</translation>
    </message>
    <message>
        <source>Next in %1s.</source>
        <translation>Следующая через %1 сек.</translation>
    </message>
    <message>
        <source>Using %1 because you chose it.</source>
        <translation>Используется %1, так как вы его выбрали.</translation>
    </message>
    <message>
        <source>You chose %1, but it is not working right now, so the selector picked another. It will go back to yours once it recovers.</source>
        <translation>Вы выбрали %1, но сейчас он не работает, поэтому автовыбор выбрал другой профиль. Как только он возобновит работу, автовыбор вернётся к вашему выбору.</translation>
    </message>
    <message>
        <source>Last switch %1 (%2).</source>
        <translation>Последнее переключение %1 (%2).</translation>
    </message>
    <message>
        <source>Nothing is working right now — if this holds, the selector will rebuild from the next best profiles.</source>
        <translation>Сейчас ничего не работает — если так и останется, автовыбор пересоберет пул из следующих по качеству профилей.</translation>
    </message>
    <message>
        <source>%1 ms</source>
        <translation>%1 мс</translation>
    </message>
    <message>
        <source>Best %1 ms, worst %2 ms</source>
        <translation>Лучший %1 мс, худший %2 мс</translation>
    </message>
    <message>
        <source>How much the latency moves around. The selector prefers steady profiles over fast but erratic ones.</source>
        <translation>Насколько сильно колеблется задержка. Автовыбор отдаёт предпочтение стабильным профилям перед быстрыми, но нестабильными.</translation>
    </message>
    <message>
        <source>%1 / %2 ok</source>
        <translation>%1 / %2 ок</translation>
    </message>
    <message>
        <source>Real connection attempts made by apps through this profile.</source>
        <translation>Реальные попытки подключения, предпринятые приложениями через этот профиль.</translation>
    </message>
</context>
<context>
    <name>DialogBasicSettings</name>
    <message>
        <source>Basic Settings</source>
        <translation>Основные настройки</translation>
    </message>
    <message>
        <source>Common</source>
        <translation>Общие</translation>
    </message>
    <message>
        <source>Listen Address</source>
        <translation>Прослушиваемый адрес</translation>
    </message>
    <message>
        <source>Custom Inbound</source>
        <translation>Польз. inbound</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>Редактировать</translation>
    </message>
    <message>
        <source>Enable</source>
        <translation>Включить</translation>
    </message>
    <message>
        <source>Latency Test URL</source>
        <translation>URL теста задержки</translation>
    </message>
    <message>
        <source>Simple Download URL</source>
        <translation>URL для простого теста скачивания</translation>
    </message>
    <message>
        <source>Disable tray</source>
        <translation>Откл. сворачивание в трей</translation>
    </message>
    <message>
        <source>Allow updating to beta versions</source>
        <translation>Разрешить обновление до бета-версий</translation>
    </message>
    <message>
        <source>Prevents the app from minimizing to the tray when the window is closed. The app will exit instead</source>
        <translation>Предотвращает сворачивание приложения в системный трей при закрытии окна. Вместо этого приложение завершит работу</translation>
    </message>
    <message>
        <source>Style</source>
        <translation>Интерфейс</translation>
    </message>
    <message>
        <source>Theme</source>
        <translation>Тема</translation>
    </message>
    <message>
        <source>Connection statistics</source>
        <translation>Статистика подключений</translation>
    </message>
    <message>
        <source>Hide dashboard at startup</source>
        <translation>Запускать приложение свёрнутым</translation>
    </message>
    <message>
        <source>Max log lines</source>
        <translation>Макс. кол-во строк в логах</translation>
    </message>
    <message>
        <source>Subscription</source>
        <translation>Подписка</translation>
    </message>
    <message>
        <source>User Agent</source>
        <translation>User Agent</translation>
    </message>
    <message>
        <source>Use proxy</source>
        <translation>Использовать прокси</translation>
    </message>
    <message>
        <source>Ignore TLS errors</source>
        <translation>Игнорировать ошибки TLS</translation>
    </message>
    <message>
        <source>Clear servers before updating subscription</source>
        <translation>Очищать список серверов перед обновлением подписки</translation>
    </message>
    <message>
        <source>Enable sending HWID, device model, and OS version when updating subscription</source>
        <translation>Включить отправку HWID, модели устройства и версии ОС при обновлении подписки</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;HWID=%1&lt;/p&gt;&lt;p&gt;OS=%2&lt;/p&gt;&lt;p&gt;OS Version=%3&lt;/p&gt;&lt;p&gt;Model=%4&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;HWID=%1&lt;/p&gt;&lt;p&gt;ОС=%2&lt;/p&gt;&lt;p&gt;Версия ОС=%3&lt;/p&gt;&lt;p&gt;Модель=%4&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Skip confirmation When Deleting Profiles</source>
        <translation>Не запрашивать подтверждение при удалении профилей</translation>
    </message>
    <message>
        <source>Select</source>
        <translation>Выбрать</translation>
    </message>
    <message>
        <source>Enable Custom Icons</source>
        <translation>Включить пользовательские иконки</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Format: hwid=value,os=value,osVersion=value,model=value&lt;/p&gt;&lt;p&gt;Leave empty to use default values. Only specify the parameters you want to override.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Формат: hwid=значение,os=значение,osVersion=значение,model=значение&lt;/p&gt;&lt;p&gt;Оставьте пустым для использования значений по умолчанию. Указывайте только те параметры, которые хотите переназначить.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Custom System Parameters (optional)</source>
        <translation>Указать параметры системы вручную (опционально)</translation>
    </message>
    <message>
        <source>hwid=custom_value,os=custom_os,osVersion=custom_version,model=custom_model</source>
        <translation>hwid=ваше_значение,os=ваша_система,osVersion=ваша_версия_ос,model=ваша_модель</translation>
    </message>
    <message>
        <source>Core</source>
        <translation>Ядро</translation>
    </message>
    <message>
        <source>outbound</source>
        <translation>outbound</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Which outbound the NTP query is sent through&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Через какой outbound отправляется NTP-запрос&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Xray Core</source>
        <translation>Ядро Xray</translation>
    </message>
    <message>
        <source>Links and Files</source>
        <translation>Ссылки «throne://» и файлы</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Registers Throne as the handler for throne:// links, re-applying the registration whenever the install is moved.&lt;/p&gt;&lt;p&gt;Turning this off does not undo an existing registration: use Uninstall for that.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Регистрирует Throne в качестве обработчика ссылок «throne://», повторно применяя регистрацию при каждом перемещении исполняемых файлов.&lt;/p&gt;&lt;p&gt;Отключение этой функции не отменяет существующую регистрацию: для этого нажмите кнопку «Удалить».&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Removes the throne:// handler Throne wrote. Entries owned by other applications are left alone.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Удаляет обработчик «throne://», записанный Throne. Записи, принадлежащие другим приложениям, остаются без изменений.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;When to choose Xray when importing vless profiles&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Когда выбирать ядро Xray при импорте профилей с протоколом VLESS&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Xray VLESS Preference</source>
        <translation>Предпочтение Xray для VLESS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Settings for the sing-box&apos;s built-in NTP client&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Настройки встроенного NTP-клиента sing-box&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;In minutes&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;В минутах&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Xray Geo Assets</source>
        <translation>Geo-файлы Xray</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;URL for geoip.dat, downloaded when a full Xray config&apos;s routing needs geoip: rules.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;URL-адрес файла geoip.dat, загружаемого, когда для маршрутизации полной конфигурации Xray требуются правила geoip:.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>GeoIP Asset URL</source>
        <translation>URL файла GeoIP</translation>
    </message>
    <message>
        <source>Download geoip.dat now from the URL on the left</source>
        <translation>Загрузить файл geoip.dat прямо сейчас по ссылке слева</translation>
    </message>
    <message>
        <source>Download</source>
        <translation>Загрузить</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;URL for geosite.dat, downloaded when a full Xray config&apos;s routing needs geosite: rules.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;URL-адрес файла geosite.dat, загружаемого, когда для маршрутизации полной конфигурации Xray требуются правила geosite:.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>GeoSite Asset URL</source>
        <translation>URL файла GeoSite</translation>
    </message>
    <message>
        <source>Download geosite.dat now from the URL on the left</source>
        <translation>Загрузить файл geosite.dat прямо сейчас по ссылке слева</translation>
    </message>
    <message>
        <source>Backup and Restore</source>
        <translation>Резервное копирование и восстановление</translation>
    </message>
    <message>
        <source>Create a portable backup file. Choose which parts to include:</source>
        <translation>Создайте переносимый файл резервной копии. Выберите, какие разделы включить:</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>Настройки</translation>
    </message>
    <message>
        <source>OTP profiles</source>
        <translation>OTP-профили</translation>
    </message>
    <message>
        <source>IP lists and scans</source>
        <translation>Списки IP и сканирования</translation>
    </message>
    <message>
        <source>Create Backup...</source>
        <translation>Создать резервную копию</translation>
    </message>
    <message>
        <source>Restore from a backup file. You can choose which parts to restore; each selected part replaces the current data. The application restarts after restore.</source>
        <translation>Восстановление из файла резервной копии. Можно выбрать, какие разделы восстановить; каждый выбранный раздел заменит текущие данные. После восстановления приложение перезапустится.</translation>
    </message>
    <message>
        <source>Restore from Backup...</source>
        <translation>Восстановить из копии</translation>
    </message>
    <message>
        <source>Security</source>
        <translation>Безопасность</translation>
    </message>
    <message>
        <source>While Throne is running, block all traffic that does not go through Throne, including when no profile is running.
The local network stays reachable unless Private Range Bypass is turned off in Tun Settings.</source>
        <translation>Пока Throne запущен, блокировать весь трафик, идущий не через Throne, даже если ни один профиль не запущен.
Локальная сеть остается доступной, если не отключен «Обход частных диапазонов» в «Настройках режима TUN».</translation>
    </message>
    <message>
        <source>Kill Switch</source>
        <translation>Kill switch</translation>
    </message>
    <message>
        <source>Skip TLS certificate authentication by default (allowInsecure)</source>
        <translation>Не проверять TLS-сертификаты по умолчанию (allowInsecure)</translation>
    </message>
    <message>
        <source>Always Start as Standard User</source>
        <translation>Всегда запускать Throne от имени обычного пользователя</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Do not attempt to start as Admin unless explicitly requested&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Не пытаться запускать приложение от имени администратора, если явно не запрошено&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Inbound Username</source>
        <translation>Имя пользователя для inbound</translation>
    </message>
    <message>
        <source>Inbound Password</source>
        <translation>Пароль для inbound</translation>
    </message>
    <message>
        <source>Enable Authorization</source>
        <translation>Включить авторизацию</translation>
    </message>
    <message>
        <source>Disable Mixed Inbound</source>
        <translation>Отключить mixed inbound</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Use proxy for subscription request, update request etc&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Использовать прокси для запросов подписок, проверки обновлений и т. д.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Restart Proxy On System Proxy Disable</source>
        <translation>Перезапускать прокси при отключении системного прокси</translation>
    </message>
    <message>
        <source>Inbound Settings</source>
        <translation>Настройки inbound</translation>
    </message>
    <message>
        <source>Testing</source>
        <translation>Проверка</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fetched &lt;span style=&quot; font-weight:600;&quot;&gt;without&lt;/span&gt; any proxy, so features that must tell a dead internet connection apart from dead servers — such as the auto selector — can check the connection itself, and notice the moment it comes back.&lt;/p&gt;&lt;p&gt;Only put a URL here that is reachable &lt;span style=&quot; font-weight:600;&quot;&gt;directly&lt;/span&gt; on your network. Leave it empty to rely on the operating system&apos;s network state instead.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Запрашивается &lt;span style=&quot; font-weight:600;&quot;&gt;без&lt;/span&gt; прокси, чтобы функции, которым нужно отличать отсутствие интернета от неработающих серверов (например, автовыбор), могли сами проверять соединение и сразу замечать его восстановление.&lt;/p&gt;&lt;p&gt;Указывайте здесь только URL, доступный в вашей сети &lt;span style=&quot; font-weight:600;&quot;&gt;напрямую&lt;/span&gt;. Оставьте поле пустым, чтобы полагаться на состояние сети, определяемое операционной системой.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Direct Test URL</source>
        <translation>URL проверки Direct</translation>
    </message>
    <message>
        <source>optional — use the OS network state</source>
        <translation>необязательно — используется состояние сети, определяемое ОС</translation>
    </message>
    <message>
        <source>Concurrency</source>
        <translation>Кол-во потоков</translation>
    </message>
    <message>
        <source>Register throne:// links at startup</source>
        <translation>Регистрировать обработчик ссылок throne:// при запуске</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Registers the handler now, without waiting for the next start.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Регистрирует обработчик прямо сейчас, не дожидаясь следующего запуска.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Install</source>
        <translation>Установить</translation>
    </message>
    <message>
        <source>Uninstall</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Adds Throne to the &quot;Open with&quot; list for config files such as JSON and YAML, re-applying the registration whenever the install is moved.&lt;/p&gt;&lt;p&gt;Turning this off does not undo an existing registration: use Uninstall for that.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Добавляет Throne в список «Открыть с помощью» для файлов конфигурации, таких как JSON и YAML, и повторно применяет регистрацию при каждом перемещении исполняемых файлов.&lt;/p&gt;&lt;p&gt;Отключение этой функции не отменяет существующую регистрацию: для этого нажмите кнопку «Удалить».&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Register config files at startup</source>
        <translation>Регистрировать ассоциации файлов конфигурации при запуске</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Registers the associations now, without waiting for the next start.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Регистрирует ассоциации прямо сейчас, не дожидаясь следующего запуска.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Removes every config file association Throne wrote, including those added by older versions. Associations owned by other applications are left alone.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Удаляет все ассоциации конфигурационных файлов, созданные Throne, включая те, что были добавлены более старыми версиями. Ассоциации, принадлежащие другим приложениям, остаются без изменений.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Logging</source>
        <translation>Логирование</translation>
    </message>
    <message>
        <source>Sing-box Log level</source>
        <translation>Уровень логирования Sing-box</translation>
    </message>
    <message>
        <source>Xray Log level</source>
        <translation>Уровень логирования Xray</translation>
    </message>
    <message>
        <source>Log Filtering</source>
        <translation>Фильтрация логов</translation>
    </message>
    <message>
        <source>Include Keywords</source>
        <translation>Ключевые слова</translation>
    </message>
    <message>
        <source>Include Regex</source>
        <translation>Регулярные выражения</translation>
    </message>
    <message>
        <source>Enable Include Rules</source>
        <translation>Использовать правила включения</translation>
    </message>
    <message>
        <source>Enable Exclude Rules</source>
        <translation>Использовать правила исключения</translation>
    </message>
    <message>
        <source>Exclude Keywords</source>
        <translation>Ключевые слова</translation>
    </message>
    <message>
        <source>Exclude Regex</source>
        <translation>Регулярные выражения</translation>
    </message>
    <message>
        <source>Log Browser Font</source>
        <translation>Шрифт логов</translation>
    </message>
    <message>
        <source>Only monospaced fonts are listed. An unavailable family falls back to a built-in monospace list.</source>
        <translation>Показываются только моноширинные шрифты. Если выбранное семейство недоступно, используется встроенный список моноширинных шрифтов.</translation>
    </message>
    <message>
        <source>Log Browser Font Size</source>
        <translation>Размер шрифта логов</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fully disables the traffic aggregation feature (per-config and per-app usage history kept in a separate database, shown in the Traffic Stats dashboard). Does not affect the per-profile traffic counters in the main window.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Полностью отключает агрегацию трафика (история использования по конфигурациям и приложениям хранится в отдельной базе данных и отображается в окне «Статистика трафика»). Не влияет на счетчики трафика профилей в главном окне.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable Traffic Aggregation</source>
        <translation>Отключить агрегацию трафика</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Show each config&apos;s security next to its type in the proxy list, e.g. &quot;Trojan (WebSocket+TLS)&quot;. Insecure configs are marked with a warning sign.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Отображать безопасность каждой конфигурации рядом с ее типом в списке прокси-серверов, например, &quot;Trojan (WebSocket+TLS)&quot;. Небезопасные конфигурации помечены предупреждающим знаком.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Show Config Security</source>
        <translation>Отображать безопасность конфигураций</translation>
    </message>
    <message>
        <source>Follow the connection status icon in the dock/taskbar. When off, it always shows the Throne icon</source>
        <translation>Показывать иконку статуса подключения в доке/на панели задач. Если выключено, там всегда отображается иконка Throne</translation>
    </message>
    <message>
        <source>Follow Status Icon in Taskbar</source>
        <translation>Показывать иконку статуса на панели задач</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;When enabled, a window listing added / updated / deleted profiles is shown after a manual subscription update. The same information is always written to the log.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Если эта опция включена, после обновления подписки вручную отображается окно со списком добавленных, обновленных и удаленных профилей. Эта же информация всегда записывается в лог.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Show the changes window after a manual subscription update</source>
        <translation>Отображать окно с изменениями после обновления подписки вручную</translation>
    </message>
    <message>
        <source>Subscription auto update</source>
        <translation>Автообновление подписки</translation>
    </message>
    <message>
        <source>Interval (minute, invalid if less than 30)</source>
        <translation>Интервал (мин, не менее 30)</translation>
    </message>
    <message>
        <source>Subscriptions whose server sends an update interval (profile-update-interval) follow it; the interval above applies when a server sends none. Groups can override this in their Advanced settings.</source>
        <translation>Если сервер подписки передает интервал обновления (profile-update-interval), используется он; указанный выше интервал применяется, когда сервер его не передает. Группы могут переопределить это в своих дополнительных настройках.</translation>
    </message>
    <message>
        <source>Use the update interval sent by the server</source>
        <translation>Использовать интервал обновления, установленный сервером подписки</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;When enabled, If the currently active profile needs to be removed, it will be stopped and removed, otherwise it will not be removed at all&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Если включено и активный профиль нужно удалить (например, при обновлении подписки), он будет остановлен и удален. Если выключено, активный профиль не удаляется.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Allow stopping the active profile</source>
        <translation>Разрешить остановку активного профиля</translation>
    </message>
    <message>
        <source>TLS versions offered when fetching subscriptions. Some servers answer the default handshake with a CAPTCHA or a block page; TLS 1.2 often gets through.</source>
        <translation>Версии TLS, предлагаемые при загрузке подписок. Некоторые серверы отвечают на стандартный TLS-хендшейк капчей или страницей блокировки; с TLS 1.2 это часто удается обойти.</translation>
    </message>
    <message>
        <source>TLS Version</source>
        <translation>Версия TLS</translation>
    </message>
    <message>
        <source>Auto</source>
        <translation>Авто</translation>
    </message>
    <message>
        <source>TLS 1.2</source>
        <translation>TLS 1.2</translation>
    </message>
    <message>
        <source>TLS 1.3</source>
        <translation>TLS 1.3</translation>
    </message>
    <message>
        <source>Auto uses HTTP/2 when the server offers it.</source>
        <translation>В режиме «Авто» используется HTTP/2, если сервер его поддерживает.</translation>
    </message>
    <message>
        <source>HTTP Version</source>
        <translation>Версия HTTP</translation>
    </message>
    <message>
        <source>HTTP/1.1</source>
        <translation>HTTP/1.1</translation>
    </message>
    <message>
        <source>Routing profiles auto update</source>
        <translation>Автообновление профилей маршрутизации</translation>
    </message>
    <message>
        <source>Clash API</source>
        <translation>Clash API</translation>
    </message>
    <message>
        <source>Secret</source>
        <translation>Secret</translation>
    </message>
    <message>
        <source>sing-box API / Dashboard</source>
        <translation>sing-box API / Веб-панель</translation>
    </message>
    <message>
        <source>Loopback only. Leave the port empty to keep the web dashboard off.</source>
        <translation>Только локальный адрес (loopback). Оставьте порт пустым, чтобы отключить веб-панель.</translation>
    </message>
    <message>
        <source>Regenerate Secret</source>
        <translation>Сгенерировать Secret</translation>
    </message>
    <message>
        <source>Miscellaneous</source>
        <translation>Прочее</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Might Improve Ping and Performance&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Может улучшить пинг и производительность&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable Traffic Stats</source>
        <translation>Отключить статистику трафика</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;A DNS server for local use will be deployed on this port&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;На этом порту будет запущен DNS-сервер для локального использования&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>DNS Server Port</source>
        <translation>Порт DNS-сервера</translation>
    </message>
    <message>
        <source>NTP Settings</source>
        <translation>Настройки NTP</translation>
    </message>
    <message>
        <source>server</source>
        <translation>Сервер</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>Порт</translation>
    </message>
    <message>
        <source>sync interval</source>
        <translation>Интервал синхронизации</translation>
    </message>
    <message>
        <source>Listen Port</source>
        <translation>Прослушиваемый порт</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Socks
                                                                + HTTP Proxy&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;
                                                            </source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;SOCKS
                                                                + HTTP-прокси&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;
                                                            </translation>
    </message>
    <message>
        <source>Font Size</source>
        <translation>Размер шрифта</translation>
    </message>
    <message>
        <source>Font</source>
        <translation>Шрифт</translation>
    </message>
    <message>
        <source>Proxy Format</source>
        <translation>Формат прокси</translation>
    </message>
    <message>
        <source>Language</source>
        <translation>Язык</translation>
    </message>
    <message>
        <source>Speedtest mode</source>
        <translation>Режим проверки скорости</translation>
    </message>
    <message>
        <source>Download + Upload</source>
        <translation>Скачивание (↓) + отдача (↑)</translation>
    </message>
    <message>
        <source>Only Download</source>
        <translation>Только скачивание (↓)</translation>
    </message>
    <message>
        <source>Only Upload</source>
        <translation>Только отдача (↑)</translation>
    </message>
    <message>
        <source>Disable Privilege request</source>
        <translation>Не запрашивать повышение прав</translation>
    </message>
    <message>
        <source>Simple Download</source>
        <translation>Простое скачивание</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Selects a random available port on every run&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Выбирает случайный доступный порт при каждом запуске&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Random port</source>
        <translation>Случайный порт</translation>
    </message>
    <message>
        <source>Use Mozilla Certificate Store</source>
        <translation>Исп. хранилище сертификатов Mozilla</translation>
    </message>
    <message>
        <source>Network Settings</source>
        <translation>Настройки сети</translation>
    </message>
    <message>
        <source>Only Country</source>
        <translation>Только проверка страны</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;timeout in milliseconds&lt;br/&gt;applies to all tests individually&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Таймаут в миллисекундах&lt;br/&gt;Применяется к каждому тесту отдельно&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Timeout</source>
        <translation>Таймаут</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Timeout for URLtest in ms&lt;br/&gt;Note that muxed connections take a much longer time for their initial request, and setting this value too low will cause the test to falsely report that the config is not working&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Таймаут теста задержки в мс&lt;br/&gt;Учтите, что при мультиплексировании первый запрос выполняется намного дольше, поэтому при слишком малом значении тест может ошибочно показать, что конфигурация не работает&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Select png icons</source>
        <translation>Выберите иконки в формате PNG</translation>
    </message>
    <message>
        <source>Image Files (*.png)</source>
        <translation>Изображения (*.png)</translation>
    </message>
    <message>
        <source>Create Backup</source>
        <translation>Создать резервную копию</translation>
    </message>
    <message>
        <source>Restore Backup</source>
        <translation>Восстановить из копии</translation>
    </message>
    <message>
        <source>Backup Created</source>
        <translation>Резервная копия создана</translation>
    </message>
    <message>
        <source>Backup Failed</source>
        <translation>Ошибка создания копии</translation>
    </message>
    <message>
        <source>Failed to create temporary file for restore.</source>
        <translation>Не удалось создать временный файл для восстановления.</translation>
    </message>
    <message>
        <source>Failed to restore database: %1</source>
        <translation>Не удалось восстановить базу данных: %1</translation>
    </message>
    <message numerus="yes">
        <source>Skipped %n routing rule(s) that use conditions this version of Throne does not support.</source>
        <translation>
            <numerusform>Пропущено %n правило маршрутизации, которое использует условия, не поддерживаемые данной версией Throne.</numerusform>
            <numerusform>Пропущено %n правила маршрутизации, которые используют условия, не поддерживаемые данной версией Throne.</numerusform>
            <numerusform>Пропущено %n правил маршрутизации, которые используют условия, не поддерживаемые данной версией Throne.</numerusform>
        </translation>
    </message>
    <message>
        <source>Restore Complete</source>
        <translation>Восстановление завершено</translation>
    </message>
    <message>
        <source>Restore Failed</source>
        <translation>Ошибка восстановления</translation>
    </message>
    <message>
        <source>Profiles (groups and proxies)</source>
        <translation>Профили (группы и прокси)</translation>
    </message>
    <message>
        <source>Routing profiles</source>
        <translation>Профили маршрутизации</translation>
    </message>
    <message>
        <source>Custom icons</source>
        <translation>Пользовательские иконки</translation>
    </message>
    <message>
        <source>Throne Backup (*.thrbackup)</source>
        <translation>Резервная копия Throne (*.thrbackup)</translation>
    </message>
    <message>
        <source>Select at least one part to include in the backup.</source>
        <translation>Выберите хотя бы один раздел для резервной копии.</translation>
    </message>
    <message>
        <source>Backup restored successfully. Throne will now restart for the changes to take effect.</source>
        <translation>Резервная копия успешно восстановлена. Throne будет перезапущен для применения изменений.</translation>
    </message>
    <message>
        <source>Backup created on %1.
Select which parts to restore:</source>
        <translation>Резервная копия создана %1.
Выберите разделы для восстановления:</translation>
    </message>
    <message>
        <source>Restore</source>
        <translation>Восстановить</translation>
    </message>
    <message>
        <source>unknown date</source>
        <translation>неизвестная дата</translation>
    </message>
    <message>
        <source>Could not register the handler for throne:// links.</source>
        <translation>Не удалось зарегистрировать обработчик ссылок throne://.</translation>
    </message>
    <message>
        <source>Could not register the config file associations.</source>
        <translation>Не удалось зарегистрировать ассоциации файлов конфигурации.</translation>
    </message>
    <message>
        <source>Diagnostics</source>
        <translation>Диагностика</translation>
    </message>
    <message>
        <source>Not available for this installation</source>
        <translation>Недоступно для этого варианта установки</translation>
    </message>
    <message>
        <source>Installed</source>
        <translation>Установлено</translation>
    </message>
    <message>
        <source>Not installed</source>
        <translation>Не установлено</translation>
    </message>
    <message>
        <source>Download geo asset</source>
        <translation>Загрузка geo-файла</translation>
    </message>
    <message>
        <source>Please enter a URL for %1 first.</source>
        <translation>Пожалуйста, сначала введите URL-адрес для %1.</translation>
    </message>
    <message>
        <source>Downloading Xray geo asset: %1</source>
        <translation>Загрузка geo-файла Xray: %1</translation>
    </message>
    <message>
        <source>Failed to create database snapshot: %1</source>
        <translation>Не удалось создать снимок базы данных: %1</translation>
    </message>
    <message>
        <source>Failed to read database snapshot.</source>
        <translation>Не удалось прочитать снимок базы данных.</translation>
    </message>
    <message>
        <source>Cannot write to: %1</source>
        <translation>Не удается записать файл: %1</translation>
    </message>
    <message>
        <source>Profiles</source>
        <translation>Профили</translation>
    </message>
    <message>
        <source>Backup created successfully:
%1

Included: %2</source>
        <translation>Резервная копия успешно создана:
%1

Включено: %2</translation>
    </message>
    <message>
        <source>Cannot open backup file: %1</source>
        <translation>Не удается открыть файл резервной копии: %1</translation>
    </message>
    <message>
        <source>Not a valid Throne backup file.</source>
        <translation>Файл не является резервной копией Throne.</translation>
    </message>
    <message>
        <source>Unsupported backup format version: %1.
This backup may have been created with a newer version of the application.</source>
        <translation>Неподдерживаемая версия формата резервной копии: %1.
Возможно, эта резервная копия была создана в более новой версии приложения.</translation>
    </message>
    <message>
        <source>This backup file does not contain any restorable data.</source>
        <translation>Этот файл резервной копии не содержит восстанавливаемых данных.</translation>
    </message>
    <message>
        <source>Each selected part replaces the current data. This cannot be undone.
Throne will restart to complete the restore.</source>
        <translation>Каждый выбранный раздел заменит текущие данные. Это действие нельзя отменить.
Для завершения восстановления Throne перезапустится.</translation>
    </message>
    <message>
        <source>Select at least one part to restore.</source>
        <translation>Выберите хотя бы один раздел для восстановления.</translation>
    </message>
</context>
<context>
    <name>DialogEditGroup</name>
    <message>
        <source>Edit Group</source>
        <translation>Редактирование группы</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Название</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>Тип</translation>
    </message>
    <message>
        <source>Basic</source>
        <translation>Стандартный</translation>
    </message>
    <message>
        <source>Subscription</source>
        <translation>Подписка</translation>
    </message>
    <message>
        <source>Front Proxy</source>
        <translation>Фронт-прокси</translation>
    </message>
    <message>
        <source>Auto Clear Unavailable Profiles</source>
        <translation>Автоочистка недоступных профилей</translation>
    </message>
    <message>
        <source>URL</source>
        <translation>URL</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>Доп. настройки</translation>
    </message>
    <message>
        <source>Copy profile share links</source>
        <translation>Копировать ссылки профилей</translation>
    </message>
    <message>
        <source>Copied</source>
        <translation>Скопировано</translation>
    </message>
    <message>
        <source>Warning</source>
        <translation>Предупреждение</translation>
    </message>
    <message>
        <source>Please input URL</source>
        <translation>Пожалуйста, введите URL</translation>
    </message>
    <message>
        <source>Skip automatic update</source>
        <translation>Не обновлять автоматически</translation>
    </message>
    <message>
        <source>Common</source>
        <translation>Основные</translation>
    </message>
    <message>
        <source>Share</source>
        <translation>Поделиться</translation>
    </message>
    <message>
        <source>Landing Proxy</source>
        <translation>Выходной прокси</translation>
    </message>
    <message>
        <source>Copy profile share links (Deep Links)</source>
        <translation>Копировать ссылки профилей (deep links)</translation>
    </message>
</context>
<context>
    <name>DialogEditGroupAdvanced</name>
    <message>
        <source>Advanced Group Settings</source>
        <translation>Доп. настройки группы</translation>
    </message>
    <message>
        <source>Server Address</source>
        <translation>Адрес сервера</translation>
    </message>
    <message>
        <source>Host or IP address</source>
        <translation>Хост или IP-адрес</translation>
    </message>
    <message>
        <source>Request</source>
        <translation>Запрос</translation>
    </message>
    <message>
        <source>Empty uses the User Agent from the Subscription settings.</source>
        <translation>Если пусто, используется User Agent из настроек (вкладка «Подписка»).</translation>
    </message>
    <message>
        <source>User Agent</source>
        <translation>User Agent</translation>
    </message>
    <message>
        <source>TLS versions offered when fetching this subscription. Some servers answer the default handshake with a CAPTCHA or a block page; TLS 1.2 often gets through. Keep Default follows the Subscription settings.</source>
        <translation>Версии TLS, предлагаемые при загрузке этой подписки. Некоторые серверы отвечают на стандартный TLS-хендшейк CAPTCHA или страницей блокировки; с TLS 1.2 часто удаётся пройти. «По умолчанию» — значение из настроек (вкладка «Подписка»).</translation>
    </message>
    <message>
        <source>TLS Version</source>
        <translation>Версия TLS</translation>
    </message>
    <message>
        <source>Keep Default</source>
        <translation>По умолчанию</translation>
    </message>
    <message>
        <source>Auto</source>
        <translation>Авто</translation>
    </message>
    <message>
        <source>TLS 1.2</source>
        <translation>TLS 1.2</translation>
    </message>
    <message>
        <source>TLS 1.3</source>
        <translation>TLS 1.3</translation>
    </message>
    <message>
        <source>Auto uses HTTP/2 when the server offers it. Keep Default follows the Subscription settings.</source>
        <translation>«Авто» использует HTTP/2, если сервер его поддерживает. «По умолчанию» — значение из настроек (вкладка «Подписка»).</translation>
    </message>
    <message>
        <source>HTTP Version</source>
        <translation>Версия HTTP</translation>
    </message>
    <message>
        <source>HTTP/1.1</source>
        <translation>HTTP/1.1</translation>
    </message>
    <message>
        <source>Send the HWID, OS, OS version and device model headers with this subscription&apos;s requests. Keep Default follows the Subscription settings.</source>
        <translation>Отправлять с запросами этой подписки заголовки HWID, ОС, версии ОС и модели устройства. «По умолчанию» — значение из настроек (вкладка «Подписка»).</translation>
    </message>
    <message>
        <source>Send HWID</source>
        <translation>Отправлять HWID</translation>
    </message>
    <message>
        <source>On</source>
        <translation>Вкл.</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>Выкл.</translation>
    </message>
    <message>
        <source>HWID</source>
        <translation>HWID</translation>
    </message>
    <message>
        <source>OS</source>
        <translation>ОС</translation>
    </message>
    <message>
        <source>OS Version</source>
        <translation>Версия ОС</translation>
    </message>
    <message>
        <source>Device Model</source>
        <translation>Модель устройства</translation>
    </message>
    <message>
        <source>Update</source>
        <translation>Обновление</translation>
    </message>
    <message>
        <source>Minutes between automatic updates of this subscription. Keep Default follows the Subscription settings, where automatic updates must be enabled.</source>
        <translation>Интервал в минутах между автоматическими обновлениями этой подписки. «По умолчанию» — значение из настроек (вкладка «Подписка»), где должно быть включено автообновление.</translation>
    </message>
    <message>
        <source>Update Interval</source>
        <translation>Интервал обновления</translation>
    </message>
    <message>
        <source>Use the update interval sent by the subscription server; the interval above applies when it sends none. Keep Default follows the Subscription settings.</source>
        <translation>Использовать интервал обновления, присланный сервером подписки; если сервер его не присылает, действует интервал выше. «По умолчанию» — значение из настроек (вкладка «Подписка»).</translation>
    </message>
    <message>
        <source>Server Interval</source>
        <translation>Интервал сервера</translation>
    </message>
    <message>
        <source>Use</source>
        <translation>Использовать</translation>
    </message>
    <message>
        <source>Ignore</source>
        <translation>Игнорировать</translation>
    </message>
    <message>
        <source>Profiles whose last test succeeded are kept when the subscription no longer lists them.</source>
        <translation>Профили, для которых последнее тестирование прошло успешно, сохраняются даже после того, как они исчезают из списка подписки.</translation>
    </message>
    <message>
        <source>Keep working profiles</source>
        <translation>Сохранять рабочие профили</translation>
    </message>
    <message>
        <source>After each update:</source>
        <translation>После каждого обновления:</translation>
    </message>
    <message>
        <source>Remove duplicate profiles</source>
        <translation>Удалять дубликаты профилей</translation>
    </message>
    <message>
        <source>Remove insecure profiles</source>
        <translation>Удалять небезопасные профили</translation>
    </message>
    <message>
        <source>Profiles the core rejects. Skipped while the core is unreachable.</source>
        <translation>Профили, которые отклоняет ядро. Если ядро недоступно, этот шаг пропускается.</translation>
    </message>
    <message>
        <source>Remove invalid profiles</source>
        <translation>Удалять недействительные профили</translation>
    </message>
    <message>
        <source>Tests the group&apos;s profiles once the update finishes, after any test already running.</source>
        <translation>Запускает тест задержки профилей группы после обновления — когда завершатся уже запущенные тесты.</translation>
    </message>
    <message>
        <source>Run URL test</source>
        <translation>Запускать тест задержки</translation>
    </message>
    <message>
        <source>Remove unavailable profiles</source>
        <translation>Удалять недоступные профили</translation>
    </message>
    <message>
        <source>Sort by latency</source>
        <translation>Сортировать по задержке</translation>
    </message>
    <message>
        <source> min</source>
        <translation> мин.</translation>
    </message>
    <message>
        <source>Keep Default (%1)</source>
        <translation>По умолчанию (%1)</translation>
    </message>
    <message>
        <source>%1 min</source>
        <translation>%1 мин.</translation>
    </message>
    <message>
        <source>This server last sent %1 h.</source>
        <translation>Последний интервал, присланный этим сервером: %1 ч.</translation>
    </message>
    <message>
        <source>Profiles&apos; own addresses</source>
        <translation>Собственные адреса профилей</translation>
    </message>
    <message>
        <source>Fixed address</source>
        <translation>Фиксированный адрес</translation>
    </message>
    <message>
        <source>IP list</source>
        <translation>Список IP</translation>
    </message>
    <message>
        <source>Missing list</source>
        <translation>Список не найден</translation>
    </message>
    <message>
        <source>Profiles connect to %1 (%2).</source>
        <translation>Профили подключаются к %1 (%2).</translation>
    </message>
    <message>
        <source>%1, so profiles use their own addresses.</source>
        <translation>%1, поэтому профили используют собственные адреса.</translation>
    </message>
    <message>
        <source>Enter a host name or an IP address, without a port.</source>
        <translation>Введите имя хоста или IP-адрес без порта.</translation>
    </message>
</context>
<context>
    <name>DialogEditIpList</name>
    <message>
        <source>Edit IP list</source>
        <translation>Редактирование списка IP</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Название</translation>
    </message>
    <message>
        <source>Source</source>
        <translation>Источник</translation>
    </message>
    <message>
        <source>Manual</source>
        <translation>Вручную</translation>
    </message>
    <message>
        <source>URL</source>
        <translation>URL</translation>
    </message>
    <message>
        <source>Rule-set</source>
        <translation>Набор правил</translation>
    </message>
    <message>
        <source>Auto update</source>
        <translation>Автообновление</translation>
    </message>
    <message>
        <source>Enable</source>
        <translation>Включить</translation>
    </message>
    <message>
        <source>Every</source>
        <translation>Каждые</translation>
    </message>
    <message>
        <source>minutes</source>
        <translation>мин.</translation>
    </message>
    <message>
        <source>Entries</source>
        <translation>Записи</translation>
    </message>
    <message>
        <source>Fetch now</source>
        <translation>Загрузить сейчас</translation>
    </message>
    <message>
        <source>One entry per line: 1.2.3.4, 1.2.3.0/24, 1.2.3.4:443, [2606:4700::1]:443, 1.2.3.0/24 443 or 1.2.3.1-1.2.3.9</source>
        <translation>По одной записи в строке: 1.2.3.4, 1.2.3.0/24, 1.2.3.4:443, [2606:4700::1]:443, 1.2.3.0/24 443 или 1.2.3.1-1.2.3.9</translation>
    </message>
    <message numerus="yes">
        <source>%Ln unreadable</source>
        <translation>
            <numerusform>%Ln нераспознанная</numerusform>
            <numerusform>%Ln нераспознанные</numerusform>
            <numerusform>%Ln нераспознанных</numerusform>
        </translation>
    </message>
    <message>
        <source>New IP list</source>
        <translation>Новый список IP</translation>
    </message>
    <message>
        <source>Enter an http(s) URL.</source>
        <translation>Введите URL http(s).</translation>
    </message>
    <message>
        <source>Choose a rule-set.</source>
        <translation>Выберите набор правил.</translation>
    </message>
    <message>
        <source>Unknown rule-set &quot;%1&quot;. Pick a geoip-* name from the list or enter a rule-set URL.</source>
        <translation>Неизвестный набор правил &quot;%1&quot;. Выберите имя geoip-* из списка или введите URL набора правил.</translation>
    </message>
    <message>
        <source>IP list</source>
        <translation>Список IP</translation>
    </message>
    <message>
        <source>This list no longer exists.</source>
        <translation>Этот список больше не существует.</translation>
    </message>
    <message>
        <source>Managed by a scan that no longer exists.</source>
        <translation>Управляется сканированием, которое больше не существует.</translation>
    </message>
    <message>
        <source>Managed by scan %1</source>
        <translation>Управляется сканированием %1</translation>
    </message>
    <message>
        <source>geoip-ir, or a rule-set URL</source>
        <translation>geoip-ir или URL набора правил</translation>
    </message>
    <message>
        <source>Fetching…</source>
        <translation>Загрузка…</translation>
    </message>
    <message>
        <source>No entries yet.</source>
        <translation>Записей пока нет.</translation>
    </message>
    <message numerus="yes">
        <source>%Ln lines</source>
        <translation>
            <numerusform>%Ln строка</numerusform>
            <numerusform>%Ln строки</numerusform>
            <numerusform>%Ln строк</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%Ln entries</source>
        <translation>
            <numerusform>%Ln запись</numerusform>
            <numerusform>%Ln записи</numerusform>
            <numerusform>%Ln записей</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%Ln duplicates</source>
        <translation>
            <numerusform>%Ln дубликат</numerusform>
            <numerusform>%Ln дубликата</numerusform>
            <numerusform>%Ln дубликатов</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>Fetched %Ln entries</source>
        <translation>
            <numerusform>Загружена %Ln запись</numerusform>
            <numerusform>Загружено %Ln записи</numerusform>
            <numerusform>Загружено %Ln записей</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>showing the first %Ln</source>
        <translation>
            <numerusform>показана первая %Ln</numerusform>
            <numerusform>показаны первые %Ln</numerusform>
            <numerusform>показаны первые %Ln</numerusform>
        </translation>
    </message>
    <message>
        <source>The entries are fetched when you press Fetch now or OK.</source>
        <translation>Записи загружаются при нажатии «Загрузить сейчас» или OK.</translation>
    </message>
    <message>
        <source>updated %1</source>
        <translation>обновлено %1</translation>
    </message>
    <message>
        <source>last update failed: %1</source>
        <translation>последнее обновление не удалось: %1</translation>
    </message>
    <message>
        <source>Fetch failed: %1</source>
        <translation>Ошибка загрузки: %1</translation>
    </message>
    <message>
        <source>Could not fetch the list:
%1</source>
        <translation>Не удалось загрузить список:
%1</translation>
    </message>
    <message>
        <source>Auto update interval must be between %1 and %2 minutes.</source>
        <translation>Интервал автообновления должен быть от %1 до %2 минут.</translation>
    </message>
    <message>
        <source>This list is being refreshed. Save again when the refresh has finished.</source>
        <translation>Этот список сейчас обновляется. Сохраните его снова, когда обновление завершится.</translation>
    </message>
    <message numerus="yes">
        <source>%Ln entries could not be read and will be dropped.</source>
        <translation>
            <numerusform>%Ln запись не удалось распознать, она будет удалена.</numerusform>
            <numerusform>%Ln записи не удалось распознать, они будут удалены.</numerusform>
            <numerusform>%Ln записей не удалось распознать, они будут удалены.</numerusform>
        </translation>
    </message>
    <message>
        <source>For example: %1</source>
        <translation>Например: %1</translation>
    </message>
    <message>
        <source>Save the list without them?</source>
        <translation>Сохранить список без них?</translation>
    </message>
    <message>
        <source>Failed to store the IP list.</source>
        <translation>Не удалось сохранить список IP.</translation>
    </message>
</context>
<context>
    <name>DialogEditOtp</name>
    <message>
        <source>OTP Profile</source>
        <translation>OTP-профиль</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Название</translation>
    </message>
    <message>
        <source>How you will refer to this OTP</source>
        <translation>Как вы будете называть этот OTP</translation>
    </message>
    <message>
        <source>Secret</source>
        <translation>Secret</translation>
    </message>
    <message>
        <source>Base32 secret</source>
        <translation>Secret в формате Base32</translation>
    </message>
    <message>
        <source>Show secret</source>
        <translation>Показать Secret</translation>
    </message>
    <message>
        <source>Issuer</source>
        <translation>Издатель</translation>
    </message>
    <message>
        <source>Optional, kept for import and export</source>
        <translation>Необязательное поле, хранится для импорта и экспорта</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>Тип</translation>
    </message>
    <message>
        <source>TOTP (time based)</source>
        <translation>TOTP (на основе времени)</translation>
    </message>
    <message>
        <source>HOTP (counter based)</source>
        <translation>HOTP (на основе счетчика)</translation>
    </message>
    <message>
        <source>Algorithm</source>
        <translation>Алгоритм</translation>
    </message>
    <message>
        <source>SHA1</source>
        <translation>SHA1</translation>
    </message>
    <message>
        <source>SHA256</source>
        <translation>SHA256</translation>
    </message>
    <message>
        <source>SHA512</source>
        <translation>SHA512</translation>
    </message>
    <message>
        <source>Digits</source>
        <translation>Количество цифр</translation>
    </message>
    <message>
        <source>Period (seconds)</source>
        <translation>Период (секунды)</translation>
    </message>
    <message>
        <source>Counter</source>
        <translation>Счетчик</translation>
    </message>
    <message>
        <source>Hide secret</source>
        <translation>Скрыть Secret</translation>
    </message>
    <message>
        <source>Name cannot be empty</source>
        <translation>Название не может быть пустым</translation>
    </message>
    <message>
        <source>Secret is empty</source>
        <translation>Secret не указан</translation>
    </message>
    <message>
        <source>Secret is not valid base32</source>
        <translation>Secret не является корректной строкой Base32</translation>
    </message>
    <message>
        <source>No code yet</source>
        <translation>Код пока отсутствует</translation>
    </message>
    <message>
        <source>Current code: %1</source>
        <translation>Текущий код: %1</translation>
    </message>
    <message>
        <source>Current code: %1 (%2s)</source>
        <translation>Текущий код: %1 (%2 сек.)</translation>
    </message>
</context>
<context>
    <name>DialogEditProfile</name>
    <message>
        <source>Edit</source>
        <translation>Редактировать</translation>
    </message>
    <message>
        <source>Common</source>
        <translation>Основные</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>Тип</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>Порт</translation>
    </message>
    <message>
        <source>Address</source>
        <translation>Адрес</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Название</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>Настройки</translation>
    </message>
    <message>
        <source>The underlying transport method. It must be consistent with the server, otherwise, the connection cannot be established.</source>
        <translation>Метод передачи данных. Должен соответствовать значению на сервере, в противном случае соединение не будет установлено.</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>Сеть</translation>
    </message>
    <message>
        <source>Transport Layer Security. It must be consistent with the server, otherwise, the connection cannot be established.</source>
        <translation>TLS. Должно соответствовать значению на сервере, в противном случае соединение не будет установлено.</translation>
    </message>
    <message>
        <source>Security</source>
        <translation>Безопасность</translation>
    </message>
    <message>
        <source>Server support is required</source>
        <translation>Необходима поддержка со стороны сервера</translation>
    </message>
    <message>
        <source>Multiplex</source>
        <translation>Мультиплексирование</translation>
    </message>
    <message>
        <source>Keep Default</source>
        <translation>По умолчанию</translation>
    </message>
    <message>
        <source>Enabled</source>
        <translation>Включено</translation>
    </message>
    <message>
        <source>Disabled</source>
        <translation>Выключено</translation>
    </message>
    <message>
        <source>On</source>
        <translation>Вкл.</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>Выкл.</translation>
    </message>
    <message>
        <source>Brutal Download Speed</source>
        <translation>Скорость скачивания (Brutal)</translation>
    </message>
    <message>
        <source>Brutal Upload Speed</source>
        <translation>Скорость отдачи (Brutal)</translation>
    </message>
    <message>
        <source>Advanced Settings</source>
        <translation>Доп. настройки</translation>
    </message>
    <message>
        <source>Finalmask</source>
        <translation>Finalmask</translation>
    </message>
    <message>
        <source>Network Settings (%1)</source>
        <translation>Настройки соединения (%1)</translation>
    </message>
    <message>
        <source>Service Name</source>
        <translation>Service Name</translation>
    </message>
    <message>
        <source>TLS Security Settings</source>
        <translation>Настройки безопасности TLS</translation>
    </message>
    <message>
        <source>When enabled, V2Ray will not check the validity of the TLS certificate provided by the remote host (the security is equivalent to plaintext)</source>
        <translation>Если включено, клиент не будет проверять валидность передаваемого сервером TLS-сертификата (безопасность будет идентична plaintext-протоколам)</translation>
    </message>
    <message>
        <source>Allow insecure</source>
        <translation>Allow insecure</translation>
    </message>
    <message>
        <source>Certificate</source>
        <translation>Сертификат</translation>
    </message>
    <message>
        <source>Server name indication, clear text.</source>
        <translation>SNI (Server Name Indication) — имя сервера, передаётся в открытом виде.</translation>
    </message>
    <message>
        <source>Fragment</source>
        <translation>Фрагментация</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;TLS ClientHello fragmentation. The implementation (built-in / custom) and the &amp;quot;Keep Default&amp;quot; behaviour are configured in Settings &gt; Core.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Фрагментация TLS ClientHello. Реализация (встроенная / пользовательская) и поведение &amp;quot;По умолчанию&amp;quot; настраиваются в разделе Настройки &gt; Предустановленные настройки &gt; TLS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>TLS Tricks</source>
        <translation>TLS Tricks</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Mixed-case SNI to evade exact-match SNI filtering. The &amp;quot;Keep Default&amp;quot; behaviour is configured in Settings &gt; Core.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;SNI в смешанном регистре для обхода фильтрации по точному совпадению SNI. Поведение &amp;quot;По умолчанию&amp;quot; настраивается в разделе Настройки &gt; Предустановленные настройки &gt; TLS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Security Settings</source>
        <translation>Настройки безопасности</translation>
    </message>
    <message>
        <source>SNI</source>
        <translation>SNI</translation>
    </message>
    <message>
        <source>Fingerprint</source>
        <translation>Fingerprint</translation>
    </message>
    <message>
        <source>ALPN</source>
        <translation>ALPN</translation>
    </message>
    <message>
        <source>Pinned Cert SHA256</source>
        <translation>Pinned Cert SHA256</translation>
    </message>
    <message>
        <source>pinnedPeerCertSha256: hex SHA256 of the peer certificate (comma-separated for multiple). Replacement for allowInsecure.</source>
        <translation>pinnedPeerCertSha256: hex SHA256 сертификата сервера (несколько значений — через запятую). Замена allowInsecure.</translation>
    </message>
    <message>
        <source>Verify Cert Name</source>
        <translation>Verify Cert Name</translation>
    </message>
    <message>
        <source>verifyPeerCertByName: verify the peer certificate against this name instead of the SNI.</source>
        <translation>verifyPeerCertByName: проверять сертификат сервера по этому имени вместо SNI.</translation>
    </message>
    <message>
        <source>ECH Config List</source>
        <translation>ECH Config List</translation>
    </message>
    <message>
        <source>echConfigList: a base64 ECHConfigList, or &quot;domain+https://dns/dns-query&quot; to let Xray fetch it over DoH (e.g. cloudflare-ech.com+https://8.8.8.8/dns-query).</source>
        <translation>echConfigList: ECHConfigList в base64 или &quot;domain+https://dns/dns-query&quot;, чтобы Xray получил его через DoH (например, cloudflare-ech.com+https://8.8.8.8/dns-query).</translation>
    </message>
    <message>
        <source>Reality Pbk</source>
        <translation>Reality Pbk</translation>
    </message>
    <message>
        <source>Reality SpiderX</source>
        <translation>Reality SpiderX</translation>
    </message>
    <message>
        <source>Network Settings</source>
        <translation>Настройки соединения</translation>
    </message>
    <message>
        <source>Headers</source>
        <translation>Заголовки</translation>
    </message>
    <message>
        <source>EarlyData Length</source>
        <translation>EarlyData Length</translation>
    </message>
    <message>
        <source>Multi Mode</source>
        <translation>Multi Mode</translation>
    </message>
    <message>
        <source>xPaddingObfsMode</source>
        <translation>xPaddingObfsMode</translation>
    </message>
    <message>
        <source>xPaddingMethod</source>
        <translation>xPaddingMethod</translation>
    </message>
    <message>
        <source>xPaddingPlacement</source>
        <translation>xPaddingPlacement</translation>
    </message>
    <message>
        <source>xPaddingKey</source>
        <translation>xPaddingKey</translation>
    </message>
    <message>
        <source>xPaddingHeader</source>
        <translation>xPaddingHeader</translation>
    </message>
    <message>
        <source>Upload / Stream Tuning</source>
        <translation>Настройка upload / stream</translation>
    </message>
    <message>
        <source>scMaxEachPostBytes</source>
        <translation>scMaxEachPostBytes</translation>
    </message>
    <message>
        <source>scMinPostsIntervalMs</source>
        <translation>scMinPostsIntervalMs</translation>
    </message>
    <message>
        <source>scMaxBufferedPosts</source>
        <translation>scMaxBufferedPosts</translation>
    </message>
    <message>
        <source>uplinkHTTPMethod</source>
        <translation>uplinkHTTPMethod</translation>
    </message>
    <message>
        <source>uplinkDataPlacement</source>
        <translation>uplinkDataPlacement</translation>
    </message>
    <message>
        <source>uplinkDataKey</source>
        <translation>uplinkDataKey</translation>
    </message>
    <message>
        <source>uplinkChunkSize</source>
        <translation>uplinkChunkSize</translation>
    </message>
    <message>
        <source>noGRPCHeader</source>
        <translation>noGRPCHeader</translation>
    </message>
    <message>
        <source>noSSEHeader</source>
        <translation>noSSEHeader</translation>
    </message>
    <message>
        <source>scStreamUpServerSecs</source>
        <translation>scStreamUpServerSecs</translation>
    </message>
    <message>
        <source>sessionPlacement</source>
        <translation>sessionPlacement</translation>
    </message>
    <message>
        <source>sessionKey</source>
        <translation>sessionKey</translation>
    </message>
    <message>
        <source>sessionIDTable</source>
        <translation>sessionIDTable</translation>
    </message>
    <message>
        <source>sessionIDLength</source>
        <translation>sessionIDLength</translation>
    </message>
    <message>
        <source>seqPlacement</source>
        <translation>seqPlacement</translation>
    </message>
    <message>
        <source>seqKey</source>
        <translation>seqKey</translation>
    </message>
    <message>
        <source>Xmux</source>
        <translation>Xmux</translation>
    </message>
    <message>
        <source>maxConcurrency</source>
        <translation>maxConcurrency</translation>
    </message>
    <message>
        <source>maxConnections</source>
        <translation>maxConnections</translation>
    </message>
    <message>
        <source>hMaxRequestTimes</source>
        <translation>hMaxRequestTimes</translation>
    </message>
    <message>
        <source>hMaxReusableSecs</source>
        <translation>hMaxReusableSecs</translation>
    </message>
    <message>
        <source>cMaxReuseTimes</source>
        <translation>cMaxReuseTimes</translation>
    </message>
    <message>
        <source>hKeepAlivePeriod</source>
        <translation>hKeepAlivePeriod</translation>
    </message>
    <message>
        <source>Max Concurrency</source>
        <translation>Max Concurrency</translation>
    </message>
    <message>
        <source>Max Post Bytes</source>
        <translation>Max Post Bytes</translation>
    </message>
    <message>
        <source>Max Reusable Secs</source>
        <translation>Max Reusable Secs</translation>
    </message>
    <message>
        <source>Max Request Times</source>
        <translation>Max Request Times</translation>
    </message>
    <message>
        <source>Host</source>
        <translation>Host</translation>
    </message>
    <message>
        <source>X Padding Bytes</source>
        <translation>X Padding Bytes</translation>
    </message>
    <message>
        <source>Min Post Interval</source>
        <translation>Min Post Interval</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>Режим</translation>
    </message>
    <message>
        <source>No GRPC Headers</source>
        <translation>No GRPC Headers</translation>
    </message>
    <message>
        <source>Max Reuse times</source>
        <translation>Max Reuse Times</translation>
    </message>
    <message>
        <source>Keep Alive Period</source>
        <translation>Keep Alive Period</translation>
    </message>
    <message>
        <source>Download Settings</source>
        <translation>Download Settings</translation>
    </message>
    <message>
        <source>Padding Obfuscation</source>
        <translation>Padding Obfuscation</translation>
    </message>
    <message>
        <source>Max Connections</source>
        <translation>Max Connections</translation>
    </message>
    <message>
        <source>Path</source>
        <translation>Path</translation>
    </message>
    <message>
        <source>Application layer protocol negotiation, clear text. Please separate them with commas.</source>
        <translation>ALPN — согласование протокола прикладного уровня, передаётся в открытом виде. Разделяйте значения запятыми.</translation>
    </message>
    <message>
        <source>TLS Camouflage Settings</source>
        <translation>Настройки маскировки TLS</translation>
    </message>
    <message>
        <source>Reality public key. If not empty, turn TLS into REALITY.</source>
        <translation>Публичный ключ Reality. Если задан, вместо TLS используется REALITY.</translation>
    </message>
    <message>
        <source>Custom (%1 outbound)</source>
        <translation>Пользовательский (%1 outbound)</translation>
    </message>
    <message>
        <source>Custom (%1 config)</source>
        <translation>Пользовательский (конфигурация %1)</translation>
    </message>
    <message>
        <source>Custom (Xray outbound)</source>
        <translation>Пользовательский (Xray outbound)</translation>
    </message>
    <message>
        <source>Custom (Xray config)</source>
        <translation>Пользовательский (конфигурация Xray)</translation>
    </message>
    <message>
        <source>Set by %1.</source>
        <translation>Источник: %1.</translation>
    </message>
    <message>
        <source>Set by %1; the profile&apos;s own address is %2.</source>
        <translation>Источник: %1; собственный адрес профиля — %2.</translation>
    </message>
    <message>
        <source>Not set</source>
        <translation>Не задано</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>Уже задано</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Overrides mux to On&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Принудительно включает мультиплексирование&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Enable TCP Brutal</source>
        <translation>Включить TCP Brutal</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Server to Client TCP Brutal Speed&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Скорость TCP Brutal от сервера к клиенту&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Mb/s</source>
        <translation>Мбит/с</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;&lt;br/&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;&lt;br/&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Method of http request, will be converted to uppercase&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Метод HTTP-запроса, будет преобразован в верхний регистр&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Method</source>
        <translation>Метод</translation>
    </message>
    <message>
        <source>Reality SID</source>
        <translation>Reality SID</translation>
    </message>
    <message>
        <source>Extra Core</source>
        <translation>Доп. ядро</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;time format, like 500ms, 10ms etc&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;формат времени, например 500ms, 10ms и т.д.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Fallback Delay</source>
        <translation>Fallback Delay</translation>
    </message>
    <message>
        <source>Enable TLS Record Fragment</source>
        <translation>Включить фрагментацию записи TLS</translation>
    </message>
    <message>
        <source>Reality short id. Accept only one value.</source>
        <translation>Reality short id. Принимает только одно значение.</translation>
    </message>
    <message>
        <source>JSON: %1
%2</source>
        <translation>JSON: %1
%2</translation>
    </message>
    <message>
        <source>Xmux Reuse</source>
        <translation>Переиспользование Xmux</translation>
    </message>
    <message>
        <source>XHTTP mode: auto usually uses packet-up, REALITY uses stream-one, and REALITY with downloadSettings uses stream-up. downloadSettings is removed when saving stream-one mode.</source>
        <translation>Режим XHTTP: auto обычно использует packet-up, с REALITY — stream-one, а с REALITY и downloadSettings — stream-up. При сохранении режима stream-one параметр downloadSettings удаляется.</translation>
    </message>
    <message>
        <source>Range of extra XHTTP padding bytes. Default: 100-1000. If set, both bounds must be positive.</source>
        <translation>Диапазон дополнительных байтов заполнения XHTTP. По умолчанию: 100-1000. Если поле задано, обе границы должны быть положительными.</translation>
    </message>
    <message>
        <source>Server Max Header Bytes</source>
        <translation>Server Max Header Bytes</translation>
    </message>
    <message>
        <source>Maximum request header size accepted by the server. Default: 8192.</source>
        <translation>Максимальный размер заголовков запроса, принимаемый сервером. По умолчанию: 8192.</translation>
    </message>
    <message>
        <source>Enable Padding Obfuscation</source>
        <translation>Включить Padding Obfuscation</translation>
    </message>
    <message>
        <source>Enable custom X-Padding placement, key, header, and method. When disabled, the client uses Referer?...x_padding and the server uses X-Padding.</source>
        <translation>Включает настраиваемые placement, key, header и method для X-Padding. Если выключено, клиент использует Referer?...x_padding, а сервер использует X-Padding.</translation>
    </message>
    <message>
        <source>Padding Method</source>
        <translation>Padding Method</translation>
    </message>
    <message>
        <source>Padding value format: repeat-x or tokenish. Default: repeat-x.</source>
        <translation>Формат значения padding: repeat-x или tokenish. По умолчанию: repeat-x.</translation>
    </message>
    <message>
        <source>Padding Placement</source>
        <translation>Padding Placement</translation>
    </message>
    <message>
        <source>Where X-Padding is sent: queryInHeader, cookie, header, or query. Default: queryInHeader.</source>
        <translation>Куда отправляется X-Padding: queryInHeader, cookie, header или query. По умолчанию: queryInHeader.</translation>
    </message>
    <message>
        <source>Padding Key</source>
        <translation>Padding Key</translation>
    </message>
    <message>
        <source>Query or cookie key for X-Padding, and query key inside queryInHeader. Default: x_padding.</source>
        <translation>Ключ query или cookie для X-Padding, а также query-ключ внутри queryInHeader. По умолчанию: x_padding.</translation>
    </message>
    <message>
        <source>Padding Header</source>
        <translation>Padding Header</translation>
    </message>
    <message>
        <source>Header name used by header or queryInHeader padding. Default: X-Padding.</source>
        <translation>Имя заголовка для padding через header или queryInHeader. По умолчанию: X-Padding.</translation>
    </message>
    <message>
        <source>Packet-up upload POST size: client split size and server reject limit. Default: 1000000.</source>
        <translation>Размер upload POST-запроса в режиме packet-up: клиент делит данные на части такого размера, сервер отклоняет более крупные запросы. По умолчанию: 1000000.</translation>
    </message>
    <message>
        <source>Packet-up client interval between upload POST requests per proxied connection, in milliseconds. Default: 30.</source>
        <translation>Интервал между upload POST-запросами клиента в режиме packet-up для каждого проксируемого соединения, в миллисекундах. По умолчанию: 30.</translation>
    </message>
    <message>
        <source>Max Buffered Posts</source>
        <translation>Max Buffered Posts</translation>
    </message>
    <message>
        <source>Packet-up server upload queue size per proxied connection. Default: 30.</source>
        <translation>Размер очереди upload на сервере в режиме packet-up для каждого проксируемого соединения. По умолчанию: 30.</translation>
    </message>
    <message>
        <source>Uplink HTTP Method</source>
        <translation>Uplink HTTP Method</translation>
    </message>
    <message>
        <source>HTTP method for upload requests. Default: POST. Xray uppercases it; GET is accepted only in packet-up mode.</source>
        <translation>HTTP-метод upload-запросов. По умолчанию: POST. Xray переводит значение в верхний регистр; GET принимается только в режиме packet-up.</translation>
    </message>
    <message>
        <source>Uplink Data Placement</source>
        <translation>Uplink Data Placement</translation>
    </message>
    <message>
        <source>Where upload data is placed. Default: auto. cookie/header are accepted only in packet-up mode; auto/body are always accepted.</source>
        <translation>Куда помещаются upload-данные. По умолчанию: auto. cookie/header принимаются только в режиме packet-up; auto/body принимаются всегда.</translation>
    </message>
    <message>
        <source>Uplink Data Key</source>
        <translation>Uplink Data Key</translation>
    </message>
    <message>
        <source>Key used when upload data is placed in a cookie or header. Defaults: X-Data for auto/header, x_data for cookie.</source>
        <translation>Ключ, используемый при размещении upload-данных в cookie или header. По умолчанию: X-Data для auto/header, x_data для cookie.</translation>
    </message>
    <message>
        <source>Uplink Chunk Size</source>
        <translation>Uplink Chunk Size</translation>
    </message>
    <message>
        <source>Packet-up header/cookie payload chunk size range. Defaults: cookie 2-3 KiB, header 3-4 KB, otherwise scMaxEachPostBytes. Values below 64 are clamped.</source>
        <translation>Диапазон размера фрагмента payload для header/cookie в packet-up. По умолчанию: cookie 2-3 КиБ, header 3-4 КБ, иначе scMaxEachPostBytes. Значения ниже 64 ограничиваются.</translation>
    </message>
    <message>
        <source>Client-side stream-up/stream-one option: do not add Content-Type: application/grpc to upload requests.</source>
        <translation>Клиентская опция stream-up/stream-one: не добавлять Content-Type: application/grpc в upload-запросы.</translation>
    </message>
    <message>
        <source>No SSE Headers</source>
        <translation>No SSE Headers</translation>
    </message>
    <message>
        <source>Server-side downstream/stream-one option: do not send Content-Type: text/event-stream in responses.</source>
        <translation>Серверная опция downstream/stream-one: не отправлять Content-Type: text/event-stream в ответах.</translation>
    </message>
    <message>
        <source>Stream Up Server Seconds</source>
        <translation>Stream Up Server Seconds</translation>
    </message>
    <message>
        <source>Stream-up server interval for periodic xPaddingBytes keepalive writes, in seconds. Default: 20-80; values &lt;= 0 disable periodic padding.</source>
        <translation>Серверный интервал stream-up для периодических keepalive-записей xPaddingBytes, в секундах. По умолчанию: 20-80; значения &lt;= 0 отключают периодический padding.</translation>
    </message>
    <message>
        <source>Session Placement</source>
        <translation>Session Placement</translation>
    </message>
    <message>
        <source>Where the XHTTP session id is sent: path, cookie, header, or query. Default: path.</source>
        <translation>Куда отправляется XHTTP session id: path, cookie, header или query. По умолчанию: path.</translation>
    </message>
    <message>
        <source>Session Key</source>
        <translation>Session Key</translation>
    </message>
    <message>
        <source>Key used for the session id outside path placement. Defaults: x_session for cookie/query, X-Session for header.</source>
        <translation>Ключ для session id при размещении не через path. По умолчанию: x_session для cookie/query, X-Session для header.</translation>
    </message>
    <message>
        <source>Session ID Table</source>
        <translation>Session ID Table</translation>
    </message>
    <message>
        <source>Charset for generating the XHTTP session id: a predefined name (number, hex, HEX, base36, BASE36, alphabet, ALPHABET, Alphabet, Base62) or a literal ASCII string. Empty falls back to a random UUID.</source>
        <translation>Набор символов для генерации XHTTP session id: предопределённое имя (number, hex, HEX, base36, BASE36, alphabet, ALPHABET, Alphabet, Base62) или произвольная ASCII-строка. Если пусто, используется случайный UUID.</translation>
    </message>
    <message>
        <source>Session ID Length</source>
        <translation>Session ID Length</translation>
    </message>
    <message>
        <source>Length range of the generated session id, e.g. 8-16. Only used together with sessionIDTable; &quot;from&quot; must be greater than 0.</source>
        <translation>Диапазон длины генерируемого session id, например 8-16. Используется только вместе с sessionIDTable; значение &quot;from&quot; должно быть больше 0.</translation>
    </message>
    <message>
        <source>Sequence Placement</source>
        <translation>Sequence Placement</translation>
    </message>
    <message>
        <source>Where the XHTTP packet sequence is sent: path, cookie, header, or query. Default: path.</source>
        <translation>Куда отправляется sequence пакета XHTTP: path, cookie, header или query. По умолчанию: path.</translation>
    </message>
    <message>
        <source>Sequence Key</source>
        <translation>Sequence Key</translation>
    </message>
    <message>
        <source>Key used for the sequence value outside path placement. Defaults: x_seq for cookie/query, X-Seq for header.</source>
        <translation>Ключ для sequence при размещении не через path. По умолчанию: x_seq для cookie/query, X-Seq для header.</translation>
    </message>
    <message>
        <source>Client-side H2/H3 xmux limit: maximum concurrent uses per underlying connection. Cannot be used together with maxConnections. Empty xmux defaults to 1-1.</source>
        <translation>Клиентский лимит xmux для H2/H3: максимум одновременных использований одного нижележащего соединения. Нельзя использовать вместе с maxConnections. Пустой xmux по умолчанию: 1-1.</translation>
    </message>
    <message>
        <source>Client-side H2/H3 xmux limit: maximum parallel underlying connections. Cannot be used together with maxConcurrency.</source>
        <translation>Клиентский лимит xmux для H2/H3: максимум параллельных нижележащих соединений. Нельзя использовать вместе с maxConcurrency.</translation>
    </message>
    <message>
        <source>Client-side H2/H3 xmux limit: maximum times an underlying connection may be selected for reuse.</source>
        <translation>Клиентский лимит xmux для H2/H3: сколько раз максимум нижележащее соединение может быть выбрано для переиспользования.</translation>
    </message>
    <message>
        <source>Client-side H2/H3 xmux limit: maximum upload/download requests per underlying connection. Empty xmux defaults to 600-900.</source>
        <translation>Клиентский лимит xmux для H2/H3: максимум upload/download-запросов на одно нижележащее соединение. Пустой xmux по умолчанию: 600-900.</translation>
    </message>
    <message>
        <source>Client-side H2/H3 xmux limit: maximum seconds an underlying connection stays reusable. Empty xmux defaults to 1800-3000.</source>
        <translation>Клиентский лимит xmux для H2/H3: максимум секунд, пока нижележащее соединение остается переиспользуемым. Пустой xmux по умолчанию: 1800-3000.</translation>
    </message>
    <message>
        <source>Client-side H2/H3 keepalive interval for underlying connections, in seconds. 0 uses Xray defaults; negative values disable keepalive where supported.</source>
        <translation>Клиентский интервал keepalive для нижележащих H2/H3-соединений, в секундах. 0 использует значения Xray по умолчанию; отрицательные значения отключают keepalive там, где это поддерживается.</translation>
    </message>
    <message>
        <source>Client-only downstream streamSettings, including address and port, for an independent download path. Not allowed in stream-one and removed when saving stream-one mode.</source>
        <translation>Клиентские downstream streamSettings, включая адрес и порт, для независимого пути скачивания. Не разрешено в stream-one и удаляется при сохранении режима stream-one.</translation>
    </message>
    <message>
        <source>XHTTP maxConnections cannot be specified together with maxConcurrency.</source>
        <translation>В XHTTP нельзя одновременно указывать maxConnections и maxConcurrency.</translation>
    </message>
    <message>
        <source>Headers cannot contain &quot;|&quot;.</source>
        <translation>Заголовки не могут содержать символ &quot;|&quot;.</translation>
    </message>
</context>
<context>
    <name>DialogEndpointDetails</name>
    <message>
        <source>Endpoint Details</source>
        <translation>Сведения о конечной точке</translation>
    </message>
    <message>
        <source>Endpoint — %1</source>
        <translation>Конечная точка — %1</translation>
    </message>
    <message>
        <source>State</source>
        <translation>Состояние</translation>
    </message>
    <message>
        <source>Error</source>
        <translation>Ошибка</translation>
    </message>
    <message>
        <source>Server</source>
        <translation>Сервер</translation>
    </message>
    <message>
        <source>Transport</source>
        <translation>Транспорт</translation>
    </message>
    <message>
        <source>Cipher</source>
        <translation>Шифр</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>Connected for</source>
        <translation>Подключено</translation>
    </message>
    <message>
        <source>IPv4 address</source>
        <translation>Адрес IPv4</translation>
    </message>
    <message>
        <source>IPv6 address</source>
        <translation>Адрес IPv6</translation>
    </message>
    <message>
        <source>All domains — every DNS query goes through this tunnel</source>
        <translation>Все домены — все DNS-запросы идут через этот туннель</translation>
    </message>
    <message>
        <source>Routes through this tunnel</source>
        <translation>Маршруты через этот туннель</translation>
    </message>
    <message>
        <source>Kept outside this tunnel</source>
        <translation>Маршруты в обход этого туннеля</translation>
    </message>
    <message>
        <source>DNS servers pushed by the server</source>
        <translation>DNS-серверы, переданные сервером</translation>
    </message>
    <message>
        <source>Domains routed to this tunnel</source>
        <translation>Домены, направляемые в этот туннель</translation>
    </message>
    <message>
        <source>No longer running</source>
        <translation>Уже не работает</translation>
    </message>
</context>
<context>
    <name>DialogIntegration</name>
    <message>
        <source>Scripts connect with thronectl or the Throne API library.</source>
        <translation>Скрипты подключаются через thronectl или библиотеку Throne API.</translation>
    </message>
    <message>
        <source>Hide</source>
        <translation>Скрыть</translation>
    </message>
    <message>
        <source>Regenerate Key</source>
        <translation>Сгенерировать ключ</translation>
    </message>
    <message>
        <source>Generate a new key?</source>
        <translation>Сгенерировать новый ключ?</translation>
    </message>
    <message>
        <source>Scripts using the current key will stop working.</source>
        <translation>Скрипты, использующие текущий ключ, перестанут работать.</translation>
    </message>
    <message>
        <source>These entries are not IP addresses or CIDR ranges:</source>
        <translation>Эти записи не являются IP-адресами или диапазонами CIDR:</translation>
    </message>
    <message>
        <source>Hotkey</source>
        <translation>Горячие клавиши</translation>
    </message>
    <message>
        <source>These global hotkeys could not be registered:</source>
        <translation>Не удалось зарегистрировать эти глобальные горячие клавиши:</translation>
    </message>
    <message>
        <source>Show routes</source>
        <translation>Показать маршруты</translation>
    </message>
    <message>
        <source>Show groups</source>
        <translation>Показать группы</translation>
    </message>
    <message>
        <source>Integration Settings</source>
        <translation>Настройки интеграции</translation>
    </message>
    <message>
        <source>Global Hotkeys</source>
        <translation>Глобальные горячие клавиши</translation>
    </message>
    <message>
        <source>Trigger main window</source>
        <translation>Показать/скрыть главное окно</translation>
    </message>
    <message>
        <source>Proxy mode</source>
        <translation>Режим прокси</translation>
    </message>
    <message>
        <source>Toggle System Proxy</source>
        <translation>Вкл./выкл. системный прокси</translation>
    </message>
    <message>
        <source>Start/Stop Profile</source>
        <translation>Запустить/остановить профиль</translation>
    </message>
    <message>
        <source>Toggle Tun Mode</source>
        <translation>Вкл./выкл. режим TUN</translation>
    </message>
    <message>
        <source>Throne API</source>
        <translation>Throne API</translation>
    </message>
    <message>
        <source>Enable the Throne API</source>
        <translation>Включить Throne API</translation>
    </message>
    <message>
        <source>Listen on</source>
        <translation>Прослушивать</translation>
    </message>
    <message>
        <source>This computer only</source>
        <translation>Только этот компьютер</translation>
    </message>
    <message>
        <source>Local network</source>
        <translation>Локальная сеть</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>Порт</translation>
    </message>
    <message>
        <source>Allowed addresses</source>
        <translation>Разрешенные адреса</translation>
    </message>
    <message>
        <source>Any address, or e.g. 192.168.1.0/24, 10.0.0.5</source>
        <translation>Любой адрес или, например, 192.168.1.0/24, 10.0.0.5</translation>
    </message>
    <message>
        <source>Key</source>
        <translation>Ключ</translation>
    </message>
    <message>
        <source>Show</source>
        <translation>Показать</translation>
    </message>
    <message>
        <source>Copy</source>
        <translation>Копировать</translation>
    </message>
    <message>
        <source>Regenerate</source>
        <translation>Сгенерировать заново</translation>
    </message>
    <message>
        <source>Copy for scripts</source>
        <translation>Копировать для скриптов</translation>
    </message>
    <message>
        <source>Status</source>
        <translation>Статус</translation>
    </message>
    <message>
        <source>Devices on your network can control Throne with this key. Your firewall may ask to allow Throne.</source>
        <translation>Устройства в вашей сети смогут управлять Throne с помощью этого ключа. Брандмауэр может запросить разрешение для Throne.</translation>
    </message>
    <message>
        <source>Shortcuts</source>
        <translation>Сочетания клавиш</translation>
    </message>
</context>
<context>
    <name>DialogIpLists</name>
    <message>
        <source>IP Lists</source>
        <translation>Списки IP</translation>
    </message>
    <message>
        <source>New</source>
        <translation>Создать</translation>
    </message>
    <message>
        <source>Import</source>
        <translation>Импорт</translation>
    </message>
    <message>
        <source>Duplicate</source>
        <translation>Дублировать</translation>
    </message>
    <message>
        <source>Export</source>
        <translation>Экспорт</translation>
    </message>
    <message numerus="yes">
        <source>…and %Ln more</source>
        <translation>
            <numerusform>…и еще %Ln</numerusform>
            <numerusform>…и еще %Ln</numerusform>
            <numerusform>…и еще %Ln</numerusform>
        </translation>
    </message>
    <message>
        <source>From text…</source>
        <translation>Из текста…</translation>
    </message>
    <message>
        <source>From file…</source>
        <translation>Из файла…</translation>
    </message>
    <message>
        <source>From URL…</source>
        <translation>По URL…</translation>
    </message>
    <message>
        <source>From rule-set…</source>
        <translation>Из набора правил…</translation>
    </message>
    <message>
        <source>Add default lists</source>
        <translation>Добавить списки по умолчанию</translation>
    </message>
    <message>
        <source>Every default list is already present.</source>
        <translation>Все списки по умолчанию уже добавлены.</translation>
    </message>
    <message>
        <source>Importing…</source>
        <translation>Импорт…</translation>
    </message>
    <message>
        <source>Delete IP list</source>
        <translation>Удалить список IP</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is being refreshed. Delete it when the refresh has finished.</source>
        <translation>Список &quot;%1&quot; сейчас обновляется. Удалите его, когда обновление завершится.</translation>
    </message>
    <message>
        <source>Delete the IP list &quot;%1&quot;?</source>
        <translation>Удалить список IP &quot;%1&quot;?</translation>
    </message>
    <message>
        <source>Scan &quot;%1&quot; is running and stores its results in this list. Pause it first.</source>
        <translation>Сканирование &quot;%1&quot; выполняется и сохраняет результаты в этот список. Сначала приостановите его.</translation>
    </message>
    <message>
        <source>It holds the results of scan &quot;%1&quot;, which creates a new result list the next time it runs.</source>
        <translation>В нем хранятся результаты сканирования &quot;%1&quot;, которое при следующем запуске создаст новый список результатов.</translation>
    </message>
    <message>
        <source>These scans use it as their targets: %1</source>
        <translation>Этот список используется как цели в сканированиях: %1</translation>
    </message>
    <message>
        <source>%1 copy</source>
        <translation>%1 (копия)</translation>
    </message>
    <message>
        <source>Duplicate IP list</source>
        <translation>Дублировать список IP</translation>
    </message>
    <message>
        <source>Name of the copy:</source>
        <translation>Название копии:</translation>
    </message>
    <message>
        <source>Failed to copy the IP list.</source>
        <translation>Не удалось скопировать список IP.</translation>
    </message>
    <message>
        <source>Export IP list</source>
        <translation>Экспорт списка IP</translation>
    </message>
    <message>
        <source>Text files (*.txt);;All files (*)</source>
        <translation>Текстовые файлы (*.txt);;Все файлы (*)</translation>
    </message>
    <message>
        <source>Cannot write to: %1</source>
        <translation>Не удается записать файл: %1</translation>
    </message>
    <message numerus="yes">
        <source>Exported %Ln entries to:
%1</source>
        <translation>
            <numerusform>Экспортирована %Ln запись в:
%1</numerusform>
            <numerusform>Экспортировано %Ln записи в:
%1</numerusform>
            <numerusform>Экспортировано %Ln записей в:
%1</numerusform>
        </translation>
    </message>
    <message>
        <source>Import IP lists</source>
        <translation>Импорт списков IP</translation>
    </message>
    <message>
        <source>All files (*);;Text and CSV (*.txt *.csv *.lst *.list);;sing-box rule-sets (*.srs *.json)</source>
        <translation>Все файлы (*);;Текст и CSV (*.txt *.csv *.lst *.list);;Наборы правил sing-box (*.srs *.json)</translation>
    </message>
    <message>
        <source>%1: cannot be opened</source>
        <translation>%1: не удается открыть</translation>
    </message>
    <message>
        <source>%1: larger than 50 MB, skipped</source>
        <translation>%1: размер превышает 50 МБ, пропущено</translation>
    </message>
    <message>
        <source>%1: no addresses found</source>
        <translation>%1: адреса не найдены</translation>
    </message>
    <message>
        <source>%1: could not be stored</source>
        <translation>%1: не удалось сохранить</translation>
    </message>
    <message numerus="yes">
        <source>%1: %Ln unreadable entries skipped</source>
        <translation>
            <numerusform>%1: пропущена %Ln нераспознанная запись</numerusform>
            <numerusform>%1: пропущены %Ln нераспознанные записи</numerusform>
            <numerusform>%1: пропущено %Ln нераспознанных записей</numerusform>
        </translation>
    </message>
    <message>
        <source>Nothing could be imported:</source>
        <translation>Ничего не удалось импортировать:</translation>
    </message>
    <message numerus="yes">
        <source>Imported %Ln list(s)</source>
        <translation>
            <numerusform>Импортирован %Ln список</numerusform>
            <numerusform>Импортировано %Ln списка</numerusform>
            <numerusform>Импортировано %Ln списков</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>with %Ln entries.</source>
        <translation>
            <numerusform>с %Ln записью.</numerusform>
            <numerusform>с %Ln записями.</numerusform>
            <numerusform>с %Ln записями.</numerusform>
        </translation>
    </message>
</context>
<context>
    <name>DialogManageGroups</name>
    <message>
        <source>Groups</source>
        <translation>Группы</translation>
    </message>
    <message>
        <source>New group</source>
        <translation>Новая группа</translation>
    </message>
    <message>
        <source>Update all subscriptions</source>
        <translation>Обновить все подписки</translation>
    </message>
    <message>
        <source>Confirmation</source>
        <translation>Подтверждение</translation>
    </message>
    <message>
        <source>Update all subscriptions?</source>
        <translation>Обновить все подписки?</translation>
    </message>
</context>
<context>
    <name>DialogManageRoutes</name>
    <message>
        <source>Routes</source>
        <translation>Маршрутизация</translation>
    </message>
    <message>
        <source>Common</source>
        <translation>Основные</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Used as default strategy when resolving outbound/dns server Domain addresses&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Используется как стратегия по умолчанию при разрешении доменных адресов для исходящих соединений и DNS-серверов&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Default Domain Strategy</source>
        <translation>Стратегия разрешения доменов по умолчанию</translation>
    </message>
    <message>
        <source>Resolve Domain Strategy</source>
        <translation>Стратегия разрешения доменов в запросах</translation>
    </message>
    <message>
        <source>Warp</source>
        <translation>Warp</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Enables the following config to be used as egress, useful for bypassing services restrciting non-warp VPNs&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Использовать конфигурацию ниже как выходной узел (egress). Полезно для обхода сервисов, которые блокируют любые VPN, кроме Warp&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Enable Warp</source>
        <translation>Включить Warp</translation>
    </message>
    <message>
        <source>Endpoint</source>
        <translation>Endpoint</translation>
    </message>
    <message>
        <source>Private Key</source>
        <translation>Private Key</translation>
    </message>
    <message>
        <source>Public Key</source>
        <translation>Public Key</translation>
    </message>
    <message>
        <source>Reserved</source>
        <translation>Reserved</translation>
    </message>
    <message>
        <source>Interface Addresses</source>
        <translation>Адреса интерфейса</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Gets a new config from cloudflare API&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Получить новую конфигурацию через Cloudflare API&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Generate Warp Config</source>
        <translation>Сгенерировать конфигурацию Warp</translation>
    </message>
    <message>
        <source>DNS</source>
        <translation>DNS</translation>
    </message>
    <message>
        <source>Simple DNS Settings</source>
        <translation>Основные настройки DNS</translation>
    </message>
    <message>
        <source>Direct DNS</source>
        <translation>DNS для прямых запросов</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;When enabled, domains destined for direct outbound will also be resolved using direct dns&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Если включено, домены, трафик к которым идет напрямую (Direct), также разрешаются через «DNS для прямых запросов»&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Enable DNS Routing</source>
        <translation>Включить DNS-маршрутизацию</translation>
    </message>
    <message>
        <source>Remote DNS</source>
        <translation>Удалённый DNS</translation>
    </message>
    <message>
        <source>Format</source>
        <translation>Форматировать</translation>
    </message>
    <message>
        <source>Document</source>
        <translation>Документация</translation>
    </message>
    <message>
        <source>This is especially important and it is recommended to use the default value of &quot;localhost&quot;.
If the default value does not work, try changing it to &quot;223.5.5.5&quot;.
For more information, see the document &quot;Configuration/DNS&quot;.</source>
        <translation>Это особенно важно: рекомендуется использовать значение по умолчанию — &quot;localhost&quot;.
Если значение по умолчанию не работает, попробуйте изменить его на &quot;223.5.5.5&quot;.
Дополнительную информацию см. в документации &quot;Конфигурация/DNS&quot;.</translation>
    </message>
    <message>
        <source>Routing Profile</source>
        <translation>Профиль маршрутизации</translation>
    </message>
    <message>
        <source>Remote Rule-set Mirror</source>
        <translation>Зеркало наборов правил</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;If not empty, resolves domains in requests according to the strategy&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Если не пусто, разрешает домены в запросах в соответствии со стратегией&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Default DNS server</source>
        <translation>DNS-сервер по умолчанию</translation>
    </message>
    <message>
        <source>Disable Cache</source>
        <translation>Отключить кэш</translation>
    </message>
    <message>
        <source>Disable Expire</source>
        <translation>Отключить Expire</translation>
    </message>
    <message>
        <source>Reverse Mapping</source>
        <translation>Обратное сопоставление (Reverse Mapping)</translation>
    </message>
    <message>
        <source>Cache Capacity</source>
        <translation>Емкость кэша</translation>
    </message>
    <message>
        <source>Route</source>
        <translation>Маршрутизация</translation>
    </message>
    <message>
        <source>Routing Profiles</source>
        <translation>Профили маршрутизации</translation>
    </message>
    <message>
        <source>New</source>
        <translation>Создать</translation>
    </message>
    <message>
        <source>Import</source>
        <translation>Импорт</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>Редактировать</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Update the selected remote routing profile, or all of them</source>
        <translation>Обновить выбранный удалённый профиль маршрутизации или все профили</translation>
    </message>
    <message>
        <source>Update</source>
        <translation>Обновить</translation>
    </message>
    <message>
        <source>Enable FakeIP</source>
        <translation>Включить FakeIP</translation>
    </message>
    <message>
        <source>WireGuard runs over UDP. MASQUE tunnels over HTTP/3 or HTTP/2 and passes networks that block WireGuard.</source>
        <translation>WireGuard работает поверх UDP. MASQUE туннелирует трафик через HTTP/3 или HTTP/2 и проходит через сети, блокирующие WireGuard.</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>Режим</translation>
    </message>
    <message>
        <source>Base64 DER of the server&apos;s public key. When set, it pins the server certificate.</source>
        <translation>Публичный ключ сервера в формате Base64 DER. Если задан, сертификат сервера закрепляется (pinning).</translation>
    </message>
    <message>
        <source>Peer Public Key</source>
        <translation>Peer Public Key</translation>
    </message>
    <message>
        <source>HTTP/3 runs over UDP; HTTP/2 runs over TLS on TCP and works where UDP is blocked.</source>
        <translation>HTTP/3 работает поверх UDP; HTTP/2 — поверх TLS на TCP и работает там, где UDP заблокирован.</translation>
    </message>
    <message>
        <source>HTTP Version</source>
        <translation>Версия HTTP</translation>
    </message>
    <message>
        <source>HTTP/3 (fallback to HTTP/2)</source>
        <translation>HTTP/3 (с откатом на HTTP/2)</translation>
    </message>
    <message>
        <source>HTTP/3 only</source>
        <translation>Только HTTP/3</translation>
    </message>
    <message>
        <source>Cloudflare API domains to try, in order, when generating a WARP config</source>
        <translation>Домены API Cloudflare, которые следует проверять по порядку при генерации конфигурации WARP</translation>
    </message>
    <message>
        <source>Registration Domains…</source>
        <translation>Домены регистрации…</translation>
    </message>
    <message>
        <source>Answer AAAA queries sent to this server with an empty result.</source>
        <translation>Отвечать на запросы AAAA, отправленные на этот сервер, пустым результатом.</translation>
    </message>
    <message>
        <source>Disable IPv6</source>
        <translation>Отключить IPv6</translation>
    </message>
    <message>
        <source>Hand out no fake IPv6 addresses; AAAA queries are answered empty.</source>
        <translation>Не выдавать FakeIP-адреса IPv6; на запросы AAAA даются пустые ответы.</translation>
    </message>
    <message>
        <source>FakeIP Disable IPv6</source>
        <translation>FakeIP: отключить IPv6</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>Дополнительно</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Cache behaviour, optimistic cache and query timeout.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Поведение кэша, оптимистичный кэш и таймаут запроса.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Advanced Settings</source>
        <translation>Доп. настройки</translation>
    </message>
    <message>
        <source>Use Custom DNS Object</source>
        <translation>Использовать свой объект DNS</translation>
    </message>
    <message>
        <source>Edit DNS Object</source>
        <translation>Редактировать объект DNS</translation>
    </message>
    <message>
        <source>Clone</source>
        <translation>Дублировать</translation>
    </message>
    <message>
        <source>Export</source>
        <translation>Экспорт</translation>
    </message>
    <message>
        <source>Invalid state</source>
        <translation>Недопустимое состояние</translation>
    </message>
    <message>
        <source>The list of routing profiles is empty, this should be an unreachable state, crashes may occur now</source>
        <translation>Список профилей маршрутизации пуст. Такого не должно происходить; возможны сбои приложения</translation>
    </message>
    <message>
        <source>Predefined DNS Answers</source>
        <translation>Предопределенные DNS-ответы</translation>
    </message>
    <message>
        <source>Enable predefined answers</source>
        <translation>Включить предопределенные ответы</translation>
    </message>
    <message>
        <source>One entry per line, hosts-file syntax:

127.0.0.1 localhost
10.0.0.5 nas.lan files.lan
::1 localhost6

A domain listed with only one address family is answered NXDOMAIN for the other, so the override cannot be bypassed.</source>
        <translation>По одной записи в строке, синтаксис как в файле hosts:

127.0.0.1 localhost
10.0.0.5 nas.lan files.lan
::1 localhost6

Если для домена указаны адреса только одного семейства, на запросы другого семейства возвращается NXDOMAIN, поэтому переопределение нельзя обойти.</translation>
    </message>
    <message>
        <source>Not a valid predefined DNS entry:
</source>
        <translation>Недопустимая предопределённая запись DNS:
</translation>
    </message>
    <message>
        <source>Advanced DNS Settings</source>
        <translation>Доп. настройки DNS</translation>
    </message>
    <message>
        <source>10s (default)</source>
        <translation>10s (по умолчанию)</translation>
    </message>
    <message>
        <source>Query Timeout</source>
        <translation>Таймаут запроса</translation>
    </message>
    <message>
        <source>Optimistic Cache</source>
        <translation>Оптимистичный кэш</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Keep serving an expired answer while it is refreshed in the background. Cannot be combined with Disable Cache or Disable Expire.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Продолжать выдавать ответ с истёкшим сроком, пока он обновляется в фоне. Нельзя сочетать с параметрами «Отключить кэш» и «Отключить Expire».&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>3d (default)</source>
        <translation>3d (по умолчанию)</translation>
    </message>
    <message>
        <source>Optimistic Timeout</source>
        <translation>Оптимистичный таймаут</translation>
    </message>
    <message>
        <source>Save Cache To File</source>
        <translation>Сохранять кэш в файл</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Write cached DNS answers and FakeIP mappings to the core&apos;s cache file so they survive a restart. Off by default: each entry costs a disk write.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Записывать кэшированные ответы DNS и сопоставления FakeIP в файл кэша ядра, чтобы они сохранялись после перезапуска. По умолчанию отключено: каждая запись в кэш означает обращение к диску.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Invalid settings</source>
        <translation>Неверные настройки</translation>
    </message>
    <message>
        <source>Not a valid duration: </source>
        <translation>Недопустимый интервал времени: </translation>
    </message>
    <message>
        <source>
Use a number followed by ns, us, ms, s, m, h or d.</source>
        <translation>
Укажите число, за которым следует ns, us, ms, s, m, h или d.</translation>
    </message>
    <message>
        <source>DNS Object</source>
        <translation>Объект DNS</translation>
    </message>
    <message>
        <source>The DNS object is not a valid JSON object</source>
        <translation>Объект DNS не является допустимым объектом JSON</translation>
    </message>
    <message>
        <source>Generating config...</source>
        <translation>Генерация конфигурации...</translation>
    </message>
    <message>
        <source>Failed to generate warp config</source>
        <translation>Не удалось сгенерировать конфигурацию Warp</translation>
    </message>
    <message>
        <source>Success!</source>
        <translation>Успешно!</translation>
    </message>
    <message>
        <source>Routing profile cannot be empty</source>
        <translation>Профиль маршрутизации не может быть пустым</translation>
    </message>
    <message>
        <source>Exported with warnings</source>
        <translation>Экспортировано с предупреждениями</translation>
    </message>
    <message>
        <source>Copied!</source>
        <translation>Скопировано!</translation>
    </message>
    <message>
        <source>No valid remote routing profiles in the link.</source>
        <translation>По ссылке нет допустимых удалённых профилей маршрутизации.</translation>
    </message>
    <message>
        <source>auto update</source>
        <translation>автообновление</translation>
    </message>
    <message>
        <source>On</source>
        <translation>Вкл.</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>Выкл.</translation>
    </message>
    <message>
        <source>a routing rule list</source>
        <translation>список правил маршрутизации</translation>
    </message>
    <message>
        <source>routing profile &quot;%1&quot;</source>
        <translation>профиль маршрутизации &quot;%1&quot;</translation>
    </message>
    <message>
        <source>Import %1 from the clipboard?</source>
        <translation>Импортировать %1 из буфера обмена?</translation>
    </message>
    <message>
        <source>Note:</source>
        <translation>Примечание:</translation>
    </message>
    <message>
        <source>Import routing profile</source>
        <translation>Импорт профиля маршрутизации</translation>
    </message>
    <message>
        <source>Invalid input</source>
        <translation>Неверный ввод</translation>
    </message>
    <message>
        <source>Could not import this routing profile:
</source>
        <translation>Не удалось импортировать этот профиль маршрутизации:
</translation>
    </message>
    <message>
        <source>Invalid operation</source>
        <translation>Неверная операция</translation>
    </message>
    <message>
        <source>Routing Profiles cannot be empty, try adding another profile or editing this one</source>
        <translation>Список профилей маршрутизации не может быть пустым. Добавьте другой профиль или отредактируйте этот.</translation>
    </message>
    <message>
        <source>Updated %1, failed %2:
%3</source>
        <translation>Обновлено: %1, с ошибкой: %2:
%3</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;examples:&lt;br/&gt;tls://8.8.8.8&lt;br/&gt;https://domain/path&lt;/p&gt;&lt;p&gt;tcp://8.8.8.8:1234&lt;/p&gt;&lt;p&gt;dhcp://auto&lt;/p&gt;&lt;p&gt;h3://domain/path&lt;/p&gt;&lt;p&gt;quic://domain:4632&lt;/p&gt;&lt;p&gt;etc&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Примеры:&lt;br/&gt;tls://8.8.8.8&lt;br/&gt;https://domain/path&lt;br/&gt;tcp://8.8.8.8:1234&lt;br/&gt;dhcp://auto&lt;br/&gt;h3://domain/path&lt;br/&gt;quic://domain:4632&lt;br/&gt;и т. д.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>GitHub</source>
        <translation>GitHub</translation>
    </message>
    <message>
        <source>jsDelivr(Cloudflare)</source>
        <translation>jsDelivr (Cloudflare)</translation>
    </message>
    <message>
        <source>jsDelivr(Gcore)</source>
        <translation>jsDelivr (Gcore)</translation>
    </message>
    <message>
        <source>jsDelivr(Quantil)</source>
        <translation>jsDelivr (Quantil)</translation>
    </message>
    <message>
        <source>jsDelivr(Fastly)</source>
        <translation>jsDelivr (Fastly)</translation>
    </message>
    <message>
        <source>jsDelivr(CDN)</source>
        <translation>jsDelivr (CDN)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Answer A/AAAA queries from the system hosts file before consulting any DNS server. Only domains actually listed in the file are affected.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Отвечать на запросы A/AAAA из системного файла hosts, прежде чем обращаться к какому-либо DNS-серверу. Это касается только доменов, указанных в этом файле.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Respect Hosts File</source>
        <translation>Учитывать файл hosts</translation>
    </message>
    <message>
        <source>Predefined Answers</source>
        <translation>Предопределенные ответы</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Local DNS can not be used in some configurations, you will have to set an ip here to be used as local dns in such situations&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;В некоторых конфигурациях локальный DNS использовать нельзя — тогда укажите здесь IP-адрес, который будет использоваться как локальный DNS&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Local Override</source>
        <translation>Переопределение локального DNS</translation>
    </message>
    <message>
        <source>Import from clipboard</source>
        <translation>Импортировать из буфера обмена</translation>
    </message>
    <message>
        <source>Update all</source>
        <translation>Обновить все</translation>
    </message>
    <message>
        <source>Update selected</source>
        <translation>Обновить выбранные</translation>
    </message>
    <message>
        <source>Paste a Throne route link, a remoteRoute link, a base64 blob, or a JSON rule array</source>
        <translation>Вставьте ссылку маршрута Throne, ссылку remoteRoute, base64 или массив правил JSON</translation>
    </message>
    <message>
        <source>Add remote routing profiles</source>
        <translation>Добавить удалённые профили маршрутизации</translation>
    </message>
    <message>
        <source>WARP Registration Domains</source>
        <translation>Домены регистрации WARP</translation>
    </message>
    <message>
        <source>One domain per line. They are tried in order and the first one that accepts the registration is used. Leave empty to use %1.</source>
        <translation>По одному домену в строке. Домены проверяются по порядку, и используется первый, который принимает регистрацию. Оставьте поле пустым, чтобы использовать %1.</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>Отмена</translation>
    </message>
    <message>
        <source>Cancelling...</source>
        <translation>Отмена...</translation>
    </message>
    <message>
        <source>No remote profiles</source>
        <translation>Нет удалённых профилей</translation>
    </message>
    <message>
        <source>There are no remote routing profiles to update.</source>
        <translation>Нет удалённых профилей маршрутизации для обновления.</translation>
    </message>
    <message>
        <source>Updating...</source>
        <translation>Обновление...</translation>
    </message>
    <message>
        <source>Updating (%1 / %2)</source>
        <translation>Обновление (%1 / %2)</translation>
    </message>
    <message>
        <source>Cancelled: updated %1 of %2, %3 failed.</source>
        <translation>Отменено: обновлено %1 из %2, ошибок: %3.</translation>
    </message>
    <message>
        <source>Update complete</source>
        <translation>Обновление завершено</translation>
    </message>
    <message>
        <source>Update cancelled</source>
        <translation>Обновление отменено</translation>
    </message>
    <message>
        <source>Updated %1 remote routing profile(s).</source>
        <translation>Обновлено удалённых профилей маршрутизации: %1.</translation>
    </message>
    <message>
        <source>Update finished with errors</source>
        <translation>Обновление завершено с ошибками</translation>
    </message>
    <message>
        <source>Imported with warnings</source>
        <translation>Импортировано с предупреждениями</translation>
    </message>
    <message>
        <source>Structured profile</source>
        <translation>Структурированный профиль</translation>
    </message>
    <message>
        <source>Raw profile</source>
        <translation>JSON-профиль</translation>
    </message>
    <message>
        <source>Remote profile</source>
        <translation>Удалённый профиль</translation>
    </message>
    <message>
        <source>Add these remote routing profiles?</source>
        <translation>Добавить эти удалённые профили маршрутизации?</translation>
    </message>
</context>
<context>
    <name>DialogNewScan</name>
    <message>
        <source>Scan %1</source>
        <translation>Сканирование %1</translation>
    </message>
    <message>
        <source>New scan</source>
        <translation>Новое сканирование</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Название</translation>
    </message>
    <message>
        <source>Generic scan</source>
        <translation>Обычное сканирование</translation>
    </message>
    <message>
        <source>WARP scan</source>
        <translation>Сканирование WARP</translation>
    </message>
    <message>
        <source>Probe addresses with ping, TCP, TLS or HTTP, and optionally test them through one of your profiles.</source>
        <translation>Проверяет адреса с помощью ping, TCP, TLS или HTTP и при необходимости тестирует их через один из ваших профилей.</translation>
    </message>
    <message>
        <source>Find Cloudflare WARP endpoints that complete a WireGuard, AmneziaWG or MASQUE connection.</source>
        <translation>Находит конечные точки Cloudflare WARP, через которые удается установить подключение WireGuard, AmneziaWG или MASQUE.</translation>
    </message>
    <message>
        <source>Enter a name for the scan.</source>
        <translation>Введите название сканирования.</translation>
    </message>
    <message>
        <source>Failed to store the scan.</source>
        <translation>Не удалось сохранить сканирование.</translation>
    </message>
</context>
<context>
    <name>DialogOtpManager</name>
    <message>
        <source>OTP Manager</source>
        <translation>Менеджер OTP</translation>
    </message>
    <message>
        <source>Import</source>
        <translation>Импорт</translation>
    </message>
    <message>
        <source>Scan QR Code</source>
        <translation>Сканировать QR-код</translation>
    </message>
    <message>
        <source>OTP</source>
        <translation>OTP</translation>
    </message>
    <message>
        <source>No usable code; check the secret.</source>
        <translation>Не удалось получить код; проверьте Secret.</translation>
    </message>
    <message>
        <source>Copied</source>
        <translation>Скопировано</translation>
    </message>
    <message>
        <source>Confirmation</source>
        <translation>Подтверждение</translation>
    </message>
    <message>
        <source>Delete &quot;%1&quot;? Its secret cannot be recovered.</source>
        <translation>Удалить &quot;%1&quot;? Его Secret будет невозможно восстановить.</translation>
    </message>
    <message>
        <source>Failed to store the OTP profile.</source>
        <translation>Не удалось сохранить OTP-профиль.</translation>
    </message>
    <message>
        <source>This one as otpauth:// link and QR</source>
        <translation>Этот профиль как ссылку otpauth:// и QR-код</translation>
    </message>
    <message>
        <source>All as otpauth-migration:// link and QR</source>
        <translation>Все профили как ссылку otpauth-migration:// и QR-код</translation>
    </message>
    <message>
        <source>All as JSON file...</source>
        <translation>Все профили в JSON-файл...</translation>
    </message>
    <message>
        <source>Add manually...</source>
        <translation>Добавить вручную...</translation>
    </message>
    <message>
        <source>From link or text...</source>
        <translation>Из ссылки или из текста...</translation>
    </message>
    <message>
        <source>Import OTP</source>
        <translation>Импорт OTP</translation>
    </message>
    <message>
        <source>Paste otpauth:// links, an otpauth-migration:// link, a JSON export, or just a base32 secret.</source>
        <translation>Вставьте ссылки otpauth://, ссылку otpauth-migration://, экспорт в JSON или просто Secret в формате Base32.</translation>
    </message>
    <message>
        <source>From clipboard</source>
        <translation>Из буфера обмена</translation>
    </message>
    <message>
        <source>From QR image file...</source>
        <translation>Из файла с изображением QR-кода...</translation>
    </message>
    <message>
        <source>Unable to capture screen</source>
        <translation>Не удалось сделать снимок экрана</translation>
    </message>
    <message>
        <source>QR Code not found</source>
        <translation>QR-код не найден</translation>
    </message>
    <message>
        <source>No QR code found in the clipboard image.</source>
        <translation>На изображении из буфера обмена не найден QR-код.</translation>
    </message>
    <message>
        <source>Clipboard is empty.</source>
        <translation>Буфер обмена пуст.</translation>
    </message>
    <message>
        <source>QR images and exports (*.png *.jpg *.jpeg *.bmp *.gif *.json *.txt);;All files (*)</source>
        <translation>Изображения QR-кодов и файлы экспорта (*.png *.jpg *.jpeg *.bmp *.gif *.json *.txt);;Все файлы (*)</translation>
    </message>
    <message>
        <source>%1: cannot be opened</source>
        <translation>%1: не удаётся открыть</translation>
    </message>
    <message>
        <source>%1: larger than 50 MB, skipped</source>
        <translation>%1: размер превышает 50 МБ, пропущено</translation>
    </message>
    <message>
        <source>%1: no QR code found</source>
        <translation>%1: QR-код не найден</translation>
    </message>
    <message>
        <source>Nothing to import.</source>
        <translation>Нечего импортировать.</translation>
    </message>
    <message>
        <source>OTP %1</source>
        <translation>OTP %1</translation>
    </message>
    <message>
        <source>Imported %1 OTP profile(s).</source>
        <translation>Импортировано OTP-профилей: %1.</translation>
    </message>
    <message>
        <source>Skipped %1 duplicate or unusable entries.</source>
        <translation>Пропущено повторяющихся или непригодных записей: %1.</translation>
    </message>
    <message>
        <source>Export OTP</source>
        <translation>Экспорт OTP</translation>
    </message>
    <message>
        <source>None of the selected profiles could be exported.</source>
        <translation>Не удалось экспортировать ни один из выбранных профилей.</translation>
    </message>
    <message>
        <source>JSON (*.json)</source>
        <translation>JSON (*.json)</translation>
    </message>
    <message>
        <source>Cannot write to: %1</source>
        <translation>Не удалось записать файл: %1</translation>
    </message>
    <message>
        <source>Exported %1 OTP profile(s) to:
%2</source>
        <translation>Экспортировано OTP-профилей: %1. Файл:
%2</translation>
    </message>
</context>
<context>
    <name>DialogPresetSettings</name>
    <message>
        <source>Preset Settings</source>
        <translation>Предустановленные настройки</translation>
    </message>
    <message>
        <source>Multiplex</source>
        <translation>Мультиплексирование</translation>
    </message>
    <message>
        <source>Protocol</source>
        <translation>Протокол</translation>
    </message>
    <message>
        <source>Concurrency</source>
        <translation>Кол-во потоков</translation>
    </message>
    <message>
        <source>Default On</source>
        <translation>Вкл. по умолчанию</translation>
    </message>
    <message>
        <source>Default Mux On</source>
        <translation>Вкл. мультиплексирование по умолчанию</translation>
    </message>
    <message>
        <source>TLS Fragment</source>
        <translation>Фрагментация TLS</translation>
    </message>
    <message>
        <source>Implementation</source>
        <translation>Реализация</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Which TLS fragmentation implementation profiles use: built-in (sing-box tls.fragment) or custom (Hiddify dialer-level ClientHello fragmentation).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Какую реализацию фрагментации TLS используют профили: встроенную (built-in, tls.fragment в sing-box) или пользовательскую (custom, фрагментация ClientHello на уровне dialer из Hiddify).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Size</source>
        <translation>Размер</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Custom implementation only: bytes per ClientHello fragment, as a range min-max (e.g. 10-100).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Только для пользовательской реализации (custom): размер каждого фрагмента ClientHello в байтах, в виде диапазона мин-макс (например, 10-100).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Sleep</source>
        <translation>Пауза</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Custom implementation only: milliseconds to sleep between fragment bursts, as a range min-max (e.g. 2-5).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Только для пользовательской реализации (custom): пауза между отправками фрагментов в миллисекундах, в виде диапазона мин-макс (например, 2-5).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fragment profiles whose Fragment setting is left on &amp;quot;Keep Default&amp;quot;.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Применять фрагментацию к профилям, у которых параметр «Фрагментация» оставлен в значении &amp;quot;По умолчанию&amp;quot;.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>TLS Tricks</source>
        <translation>TLS Tricks</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Enable mixed-case SNI for profiles whose TLS Tricks setting is left on &amp;quot;Keep Default&amp;quot;.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Включать SNI в смешанном регистре (mixed-case) для профилей, у которых параметр «TLS Tricks» оставлен в значении &amp;quot;По умолчанию&amp;quot;.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Default Fingerprint</source>
        <translation>Fingerprint по умолчанию</translation>
    </message>
    <message>
        <source>TLS Spoof</source>
        <translation>Подмена TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Inject a forged TLS ClientHello carrying this SNI before the real one, to pass SNI filtering that allows specific hostnames. Profiles that set their own Spoof SNI override this one.&lt;/p&gt;&lt;p&gt;Client only. Requires elevated privileges (root on macOS, CAP_NET_RAW and CAP_NET_ADMIN on Linux, Administrator on Windows for the WinDivert driver). Not supported on Windows ARM64. &amp;quot;wrong-timestamp&amp;quot; is not supported on macOS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Перед настоящим TLS ClientHello отправляет поддельный ClientHello с этим SNI, чтобы пройти SNI-фильтрацию, которая пропускает только определенные имена хостов. Профили, в которых задан собственный поддельный SNI, переопределяют это значение.&lt;/p&gt;&lt;p&gt;Только для клиента. Требуются повышенные привилегии (root в macOS, CAP_NET_RAW и CAP_NET_ADMIN в Linux, права администратора в Windows для драйвера WinDivert). Не поддерживается в Windows ARM64. Метод &amp;quot;wrong-timestamp&amp;quot; не поддерживается в macOS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Spoof SNI</source>
        <translation>Поддельный SNI</translation>
    </message>
    <message>
        <source>Allowed SNI to forge</source>
        <translation>Разрешенный SNI для подмены</translation>
    </message>
    <message>
        <source>Method</source>
        <translation>Метод</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;How the forged ClientHello is made invalid for the server, so only the filter acts on it. Empty keeps the core&apos;s default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Способ, которым поддельный ClientHello делается недействительным для сервера, чтобы на него реагировал только фильтр. Если пусто, используется значение ядра по умолчанию.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Spoof profiles whose TLS Spoof setting is left on &amp;quot;Keep Default&amp;quot;.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Применять подмену TLS к профилям, у которых параметр «Подмена TLS» оставлен в значении &amp;quot;По умолчанию&amp;quot;.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>HTTP/2 &amp;&amp; QUIC</source>
        <translation>HTTP/2 &amp;&amp; QUIC</translation>
    </message>
    <message>
        <source>HTTP/2 parameters also apply to Hysteria, Hysteria2 and TUIC. Empty keeps the core&apos;s default.</source>
        <translation>Параметры HTTP/2 также применяются к Hysteria, Hysteria2 и TUIC. Если поле пустое, используется значение ядра по умолчанию.</translation>
    </message>
    <message>
        <source>HTTP/2 Parameters</source>
        <translation>Параметры HTTP/2</translation>
    </message>
    <message>
        <source>Idle Timeout</source>
        <translation>Idle Timeout</translation>
    </message>
    <message>
        <source>Keep Alive Period</source>
        <translation>Keep Alive Period</translation>
    </message>
    <message>
        <source>Stream Receive Window</source>
        <translation>Stream Receive Window</translation>
    </message>
    <message>
        <source>Connection Receive Window</source>
        <translation>Connection Receive Window</translation>
    </message>
    <message>
        <source>Max Concurrent Streams</source>
        <translation>Max Concurrent Streams</translation>
    </message>
    <message>
        <source>QUIC Parameters</source>
        <translation>Параметры QUIC</translation>
    </message>
    <message>
        <source>Initial Packet Size</source>
        <translation>Initial Packet Size</translation>
    </message>
    <message>
        <source>Disable Path MTU Discovery</source>
        <translation>Отключить Path MTU Discovery</translation>
    </message>
</context>
<context>
    <name>DialogScanInstance</name>
    <message>
        <source>IP Scanner</source>
        <translation>IP-сканер</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Название</translation>
    </message>
    <message>
        <source>Targets</source>
        <translation>Цели</translation>
    </message>
    <message>
        <source>Base IP list</source>
        <translation>Исходный список IP</translation>
    </message>
    <message>
        <source>Manage</source>
        <translation>Управление</translation>
    </message>
    <message>
        <source>Use built-in WARP ranges</source>
        <translation>Использовать встроенные диапазоны WARP</translation>
    </message>
    <message>
        <source>Ports</source>
        <translation>Порты</translation>
    </message>
    <message>
        <source>Ports to scan. Targets left without any port are probed on port 443 (80 for plain HTTP).</source>
        <translation>Порты для сканирования. Цели без порта проверяются на порту 443 (80 для обычного HTTP).</translation>
    </message>
    <message>
        <source>Fill in the ports Cloudflare&apos;s WARP endpoints listen on</source>
        <translation>Подставить порты, которые слушают конечные точки Cloudflare WARP</translation>
    </message>
    <message>
        <source>WARP ports</source>
        <translation>Порты WARP</translation>
    </message>
    <message>
        <source>How these ports combine with the ports stored in the IP list.</source>
        <translation>Как эти порты сочетаются с портами, сохраненными в списке IP.</translation>
    </message>
    <message>
        <source>Shuffle the target order</source>
        <translation>Перемешивать порядок целей</translation>
    </message>
    <message>
        <source>Off: IPv6 ranges in the list are skipped.</source>
        <translation>Выключено: диапазоны IPv6 из списка пропускаются.</translation>
    </message>
    <message>
        <source>Scan IPv6 addresses</source>
        <translation>Сканировать адреса IPv6</translation>
    </message>
    <message>
        <source>Stop after (results)</source>
        <translation>Лимит результатов</translation>
    </message>
    <message>
        <source>The scan completes once its result list holds this many entries. Empty or 0: no limit.</source>
        <translation>Сканирование завершается, когда в его списке результатов наберется столько записей. Пусто или 0: без ограничения.</translation>
    </message>
    <message>
        <source>Concurrency</source>
        <translation>Кол-во потоков</translation>
    </message>
    <message>
        <source>Targets probed or tested at the same time, in every phase.</source>
        <translation>Число целей, проверяемых или тестируемых одновременно, на каждом этапе.</translation>
    </message>
    <message>
        <source>Spawn interval (ms)</source>
        <translation>Интервал запуска (мс)</translation>
    </message>
    <message>
        <source>Delay between starting two probes.</source>
        <translation>Задержка между запусками двух проверок.</translation>
    </message>
    <message>
        <source>Phases</source>
        <translation>Этапы</translation>
    </message>
    <message>
        <source>Results</source>
        <translation>Результаты</translation>
    </message>
    <message>
        <source>Save as IP list</source>
        <translation>Сохранить как список IP</translation>
    </message>
    <message>
        <source>Copies the selected entries, or every entry when none is selected.</source>
        <translation>Копирует выбранные записи или все записи, если ничего не выбрано.</translation>
    </message>
    <message>
        <source>Copy</source>
        <translation>Копировать</translation>
    </message>
    <message>
        <source>Export</source>
        <translation>Экспорт</translation>
    </message>
    <message>
        <source>Remove selected</source>
        <translation>Удалить выбранные</translation>
    </message>
    <message>
        <source>Start</source>
        <translation>Запустить</translation>
    </message>
    <message>
        <source>Pause</source>
        <translation>Приостановить</translation>
    </message>
    <message>
        <source>OK</source>
        <translation>ОК</translation>
    </message>
    <message>
        <source>Ignore the list&apos;s ports</source>
        <translation>Игнорировать порты списка</translation>
    </message>
    <message>
        <source>Merge with the list&apos;s ports</source>
        <translation>Объединить с портами списка</translation>
    </message>
    <message>
        <source>Only the list&apos;s ports</source>
        <translation>Только порты списка</translation>
    </message>
    <message>
        <source>Your ports are ignored; entries without a port are probed on port 443 (80 for plain HTTP).</source>
        <translation>Ваши порты игнорируются; записи без порта проверяются на порту 443 (80 для обычного HTTP).</translation>
    </message>
    <message>
        <source>No limit</source>
        <translation>Без ограничения</translation>
    </message>
    <message>
        <source>Selects an IP list of Cloudflare&apos;s WARP ranges for the chosen mode, creating it when missing, and sets the matching ports.</source>
        <translation>Выбирает список IP с диапазонами Cloudflare WARP для выбранного режима (создает его, если его нет) и подставляет соответствующие порты.</translation>
    </message>
    <message>
        <source>ICMP (ping)</source>
        <translation>ICMP (ping)</translation>
    </message>
    <message>
        <source>Timeout (ms)</source>
        <translation>Таймаут (мс)</translation>
    </message>
    <message>
        <source>Echo requests</source>
        <translation>Число эхо-запросов</translation>
    </message>
    <message>
        <source>TCP connect</source>
        <translation>TCP-подключение</translation>
    </message>
    <message>
        <source>Attempts</source>
        <translation>Число попыток</translation>
    </message>
    <message>
        <source>TLS / HTTP</source>
        <translation>TLS / HTTP</translation>
    </message>
    <message>
        <source>Use TLS</source>
        <translation>Использовать TLS</translation>
    </message>
    <message>
        <source>The target address</source>
        <translation>Адрес цели</translation>
    </message>
    <message>
        <source>SNI</source>
        <translation>SNI</translation>
    </message>
    <message>
        <source>The SNI, or the target address</source>
        <translation>SNI или адрес цели</translation>
    </message>
    <message>
        <source>Host header</source>
        <translation>Заголовок Host</translation>
    </message>
    <message>
        <source>Path</source>
        <translation>Path</translation>
    </message>
    <message>
        <source>None (TLS handshake only)</source>
        <translation>Нет (только хендшейк TLS)</translation>
    </message>
    <message>
        <source>A request passes only when the answer is 200 OK.</source>
        <translation>Запрос считается успешным, только если получен ответ 200 OK.</translation>
    </message>
    <message>
        <source>Method</source>
        <translation>Метод</translation>
    </message>
    <message>
        <source>HTTP version</source>
        <translation>Версия HTTP</translation>
    </message>
    <message>
        <source>From the HTTP version</source>
        <translation>По версии HTTP</translation>
    </message>
    <message>
        <source>ALPN</source>
        <translation>ALPN</translation>
    </message>
    <message>
        <source>Default</source>
        <translation>По умолчанию</translation>
    </message>
    <message>
        <source>Minimum TLS version</source>
        <translation>Минимальная версия TLS</translation>
    </message>
    <message>
        <source>Maximum TLS version</source>
        <translation>Максимальная версия TLS</translation>
    </message>
    <message>
        <source>uTLS fingerprint</source>
        <translation>uTLS fingerprint</translation>
    </message>
    <message>
        <source>Allow insecure certificates</source>
        <translation>Allow insecure</translation>
    </message>
    <message>
        <source>Disable SNI</source>
        <translation>Отключить SNI</translation>
    </message>
    <message>
        <source>Fragment the TLS handshake</source>
        <translation>Фрагментировать хендшейк TLS</translation>
    </message>
    <message>
        <source>Fragment fallback delay (ms)</source>
        <translation>Резервная задержка фрагментации (мс)</translation>
    </message>
    <message>
        <source>Fragment TLS records</source>
        <translation>Фрагментировать записи TLS</translation>
    </message>
    <message>
        <source>Mixed-case SNI</source>
        <translation>SNI в смешанном регистре</translation>
    </message>
    <message>
        <source>Config test</source>
        <translation>Тест конфигурации</translation>
    </message>
    <message>
        <source>Candidates that pass the probes are tested through a copy of this profile pointed at each of them.</source>
        <translation>Кандидаты, прошедшие проверки, тестируются через копию этого профиля, направленную на каждый из них.</translation>
    </message>
    <message>
        <source>Profile</source>
        <translation>Профиль</translation>
    </message>
    <message>
        <source>Test URL</source>
        <translation>URL проверки</translation>
    </message>
    <message>
        <source>Report warm latency (two requests per target)</source>
        <translation>Измерять задержку на прогретом соединении (два запроса на цель)</translation>
    </message>
    <message>
        <source>WARP test</source>
        <translation>Тест WARP</translation>
    </message>
    <message>
        <source>Candidates that pass the probes are tested by connecting to WARP through them.</source>
        <translation>Кандидаты, прошедшие проверки, тестируются подключением к WARP через них.</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>Режим</translation>
    </message>
    <message>
        <source>HTTP/3, falling back to HTTP/2</source>
        <translation>HTTP/3 с откатом на HTTP/2</translation>
    </message>
    <message>
        <source>HTTP/3 only</source>
        <translation>Только HTTP/3</translation>
    </message>
    <message>
        <source>HTTP/2 only</source>
        <translation>Только HTTP/2</translation>
    </message>
    <message>
        <source>MASQUE transport</source>
        <translation>Транспорт MASQUE</translation>
    </message>
    <message>
        <source>Dedicated for this scan (recommended)</source>
        <translation>Отдельный для этого сканирования (рекомендуется)</translation>
    </message>
    <message>
        <source>From a profile</source>
        <translation>Из профиля</translation>
    </message>
    <message>
        <source>Built-in WARP settings</source>
        <translation>Встроенные настройки WARP</translation>
    </message>
    <message>
        <source>Identity</source>
        <translation>Идентификатор</translation>
    </message>
    <message>
        <source>Register</source>
        <translation>Зарегистрировать</translation>
    </message>
    <message>
        <source>Device</source>
        <translation>Устройство</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>Junk packets (Jc)</source>
        <translation>Junk packets (Jc)</translation>
    </message>
    <message>
        <source>Junk minimum size (Jmin)</source>
        <translation>Junk minimum size (Jmin)</translation>
    </message>
    <message>
        <source>Junk maximum size (Jmax)</source>
        <translation>Junk maximum size (Jmax)</translation>
    </message>
    <message>
        <source>WARP scan</source>
        <translation>Сканирование WARP</translation>
    </message>
    <message>
        <source>Generic scan</source>
        <translation>Обычное сканирование</translation>
    </message>
    <message>
        <source>%1 - IP Scanner</source>
        <translation>%1 - IP-сканер</translation>
    </message>
    <message>
        <source>Enter a name for the scan.</source>
        <translation>Введите название сканирования.</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a valid port (1-65535).</source>
        <translation>&quot;%1&quot; — недопустимый порт (1-65535).</translation>
    </message>
    <message>
        <source>Stop after must be between %1 and %2 results; leave it empty for no limit.</source>
        <translation>Лимит результатов должен быть от %1 до %2; оставьте поле пустым, чтобы снять ограничение.</translation>
    </message>
    <message>
        <source>Concurrency must be between %1 and %2.</source>
        <translation>Кол-во потоков должно быть от %1 до %2.</translation>
    </message>
    <message>
        <source>Spawn interval must be between %1 and %2 ms.</source>
        <translation>Интервал запуска должен быть от %1 до %2 мс.</translation>
    </message>
    <message>
        <source>ICMP (ping): the timeout must be between %1 and %2 ms.</source>
        <translation>ICMP (ping): таймаут должен быть от %1 до %2 мс.</translation>
    </message>
    <message>
        <source>ICMP (ping): echo requests must be between %1 and %2.</source>
        <translation>ICMP (ping): число эхо-запросов должно быть от %1 до %2.</translation>
    </message>
    <message>
        <source>TCP connect: the timeout must be between %1 and %2 ms.</source>
        <translation>TCP-подключение: таймаут должен быть от %1 до %2 мс.</translation>
    </message>
    <message>
        <source>TCP connect: attempts must be between %1 and %2.</source>
        <translation>TCP-подключение: число попыток должно быть от %1 до %2.</translation>
    </message>
    <message>
        <source>TLS / HTTP: the fragment fallback delay must be between %1 and %2 ms.</source>
        <translation>TLS / HTTP: резервная задержка фрагментации должна быть от %1 до %2 мс.</translation>
    </message>
    <message>
        <source>TLS / HTTP: the timeout must be between %1 and %2 ms.</source>
        <translation>TLS / HTTP: таймаут должен быть от %1 до %2 мс.</translation>
    </message>
    <message>
        <source>TLS / HTTP: the method &quot;None&quot; only checks the TLS handshake, so it needs TLS.</source>
        <translation>TLS / HTTP: метод &quot;Нет&quot; проверяет только хендшейк TLS, поэтому требует TLS.</translation>
    </message>
    <message>
        <source>TLS / HTTP: HTTP/3 needs TLS.</source>
        <translation>TLS / HTTP: HTTP/3 требует TLS.</translation>
    </message>
    <message>
        <source>Config test: the timeout must be between %1 and %2 ms.</source>
        <translation>Тест конфигурации: таймаут должен быть от %1 до %2 мс.</translation>
    </message>
    <message>
        <source>WARP test: the MTU must be between %1 and %2.</source>
        <translation>Тест WARP: MTU должен быть от %1 до %2.</translation>
    </message>
    <message>
        <source>WARP test: Jc must be between %1 and %2.</source>
        <translation>Тест WARP: Jc должен быть от %1 до %2.</translation>
    </message>
    <message>
        <source>WARP test: Jmin must be between %1 and %2.</source>
        <translation>Тест WARP: Jmin должен быть от %1 до %2.</translation>
    </message>
    <message>
        <source>WARP test: Jmax must be between %1 and %2.</source>
        <translation>Тест WARP: Jmax должен быть от %1 до %2.</translation>
    </message>
    <message>
        <source>WARP test: the timeout must be between %1 and %2 ms.</source>
        <translation>Тест WARP: таймаут должен быть от %1 до %2 мс.</translation>
    </message>
    <message>
        <source>WARP test: Jmin must not be larger than Jmax.</source>
        <translation>Тест WARP: Jmin не может быть больше Jmax.</translation>
    </message>
    <message>
        <source>Pause the scan before changing its settings.</source>
        <translation>Приостановите сканирование, прежде чем менять его настройки.</translation>
    </message>
    <message>
        <source>The scan no longer exists.</source>
        <translation>Сканирование больше не существует.</translation>
    </message>
    <message>
        <source>Failed to store the scan.</source>
        <translation>Не удалось сохранить сканирование.</translation>
    </message>
    <message>
        <source>The settings could not be saved:
%1

Close without saving?</source>
        <translation>Не удалось сохранить настройки:
%1

Закрыть без сохранения?</translation>
    </message>
    <message>
        <source>Select an IP list…</source>
        <translation>Выберите список IP…</translation>
    </message>
    <message numerus="yes">
        <source>%1 (%n entries)</source>
        <translation>
            <numerusform>%1 (%n запись)</numerusform>
            <numerusform>%1 (%n записи)</numerusform>
            <numerusform>%1 (%n записей)</numerusform>
        </translation>
    </message>
    <message>
        <source>None</source>
        <translation>Нет</translation>
    </message>
    <message>
        <source>Select a profile…</source>
        <translation>Выберите профиль…</translation>
    </message>
    <message>
        <source>Registering a WARP device…</source>
        <translation>Регистрация устройства WARP…</translation>
    </message>
    <message>
        <source>No device is registered for this scan yet.</source>
        <translation>Для этого сканирования еще не зарегистрировано устройство.</translation>
    </message>
    <message>
        <source>The registered device is for %1; register a new one for this mode.</source>
        <translation>Зарегистрированное устройство предназначено для %1; зарегистрируйте новое для этого режима.</translation>
    </message>
    <message>
        <source>A WARP device is registered for this scan.</source>
        <translation>Для этого сканирования зарегистрировано устройство WARP.</translation>
    </message>
    <message>
        <source>Register again</source>
        <translation>Зарегистрировать заново</translation>
    </message>
    <message>
        <source>Failed to store the IP list.</source>
        <translation>Не удалось сохранить список IP.</translation>
    </message>
    <message>
        <source>MASQUE: %1</source>
        <translation>MASQUE: %1</translation>
    </message>
    <message>
        <source>WireGuard: %1</source>
        <translation>WireGuard: %1</translation>
    </message>
    <message numerus="yes">
        <source>WireGuard, all %n known ports</source>
        <translation>
            <numerusform>WireGuard, все известные порты (%n)</numerusform>
            <numerusform>WireGuard, все известные порты (%n)</numerusform>
            <numerusform>WireGuard, все известные порты (%n)</numerusform>
        </translation>
    </message>
    <message>
        <source>WARP registration failed</source>
        <translation>Не удалось зарегистрировать устройство WARP</translation>
    </message>
    <message>
        <source>Unknown error</source>
        <translation>Неизвестная ошибка</translation>
    </message>
    <message>
        <source>Resume</source>
        <translation>Продолжить</translation>
    </message>
    <message>
        <source>No results yet.</source>
        <translation>Результатов пока нет.</translation>
    </message>
    <message>
        <source>Showing the first %1 of %2 results</source>
        <translation>Показаны первые результаты: %1 из %2</translation>
    </message>
    <message numerus="yes">
        <source>%n result(s)</source>
        <translation>
            <numerusform>%n результат</numerusform>
            <numerusform>%n результата</numerusform>
            <numerusform>%n результатов</numerusform>
        </translation>
    </message>
    <message>
        <source>Name of the new IP list:</source>
        <translation>Название нового списка IP:</translation>
    </message>
    <message>
        <source>%1 results</source>
        <translation>Результаты %1</translation>
    </message>
    <message numerus="yes">
        <source>Saved %n entries to the IP list &quot;%1&quot;.</source>
        <translation>
            <numerusform>Сохранена %n запись в список IP &quot;%1&quot;.</numerusform>
            <numerusform>Сохранено %n записи в список IP &quot;%1&quot;.</numerusform>
            <numerusform>Сохранено %n записей в список IP &quot;%1&quot;.</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>Copied %n entries</source>
        <translation>
            <numerusform>Скопирована %n запись</numerusform>
            <numerusform>Скопировано %n записи</numerusform>
            <numerusform>Скопировано %n записей</numerusform>
        </translation>
    </message>
    <message>
        <source>Export results</source>
        <translation>Экспорт результатов</translation>
    </message>
    <message>
        <source>Text files (*.txt);;All files (*)</source>
        <translation>Текстовые файлы (*.txt);;Все файлы (*)</translation>
    </message>
    <message>
        <source>Cannot write to: %1</source>
        <translation>Невозможно записать в: %1</translation>
    </message>
    <message>
        <source>Scan from the beginning</source>
        <translation>Сканировать с начала</translation>
    </message>
    <message>
        <source>Rescan current results</source>
        <translation>Перепроверить текущие результаты</translation>
    </message>
</context>
<context>
    <name>DialogScanner</name>
    <message>
        <source>IP Scanner</source>
        <translation>IP-сканер</translation>
    </message>
    <message>
        <source>No scans yet. Create one with &quot;New scan&quot;.</source>
        <translation>Сканирований пока нет. Создайте первое кнопкой &quot;Новое сканирование&quot;.</translation>
    </message>
    <message>
        <source>New scan</source>
        <translation>Новое сканирование</translation>
    </message>
    <message>
        <source>IP Lists</source>
        <translation>Списки IP</translation>
    </message>
    <message>
        <source>Pause the scan first.</source>
        <translation>Сначала приостановите сканирование.</translation>
    </message>
    <message>
        <source>Confirmation</source>
        <translation>Подтверждение</translation>
    </message>
    <message>
        <source>Delete the scan &quot;%1&quot;?</source>
        <translation>Удалить сканирование &quot;%1&quot;?</translation>
    </message>
    <message>
        <source>Also delete its result list</source>
        <translation>Также удалить его список результатов</translation>
    </message>
</context>
<context>
    <name>DialogTrafficStats</name>
    <message>
        <source>Download: %1     Upload: %2     Total: %3</source>
        <translation>Получено (↓): %1     Отправлено (↑): %2     Всего: %3</translation>
    </message>
    <message>
        <source>Direct</source>
        <translation>Напрямую</translation>
    </message>
    <message>
        <source>Profile #%1 (deleted)</source>
        <translation>Профиль №%1 (удалён)</translation>
    </message>
    <message>
        <source>Other</source>
        <translation>Другое</translation>
    </message>
    <message>
        <source>Unknown</source>
        <translation>Неизвестно</translation>
    </message>
    <message>
        <source>Traffic Statistics</source>
        <translation>Статистика трафика</translation>
    </message>
    <message>
        <source>Period:</source>
        <translation>Период:</translation>
    </message>
    <message>
        <source>Last 24 hours</source>
        <translation>Последние 24 часа</translation>
    </message>
    <message>
        <source>Last 7 days</source>
        <translation>Последние 7 дней</translation>
    </message>
    <message>
        <source>Last 30 days</source>
        <translation>Последние 30 дней</translation>
    </message>
    <message>
        <source>Last 90 days</source>
        <translation>Последние 90 дней</translation>
    </message>
    <message>
        <source>Refresh</source>
        <translation>Обновить</translation>
    </message>
    <message>
        <source>By Profile</source>
        <translation>По профилям</translation>
    </message>
    <message>
        <source>Profile</source>
        <translation>Профиль</translation>
    </message>
    <message>
        <source>Group</source>
        <translation>Группа</translation>
    </message>
    <message>
        <source>Download</source>
        <translation>Получено (↓)</translation>
    </message>
    <message>
        <source>Upload</source>
        <translation>Отправлено (↑)</translation>
    </message>
    <message>
        <source>Total</source>
        <translation>Всего</translation>
    </message>
    <message>
        <source>By App</source>
        <translation>По приложениям</translation>
    </message>
    <message>
        <source>App</source>
        <translation>Приложение</translation>
    </message>
</context>
<context>
    <name>DialogVPNSettings</name>
    <message>
        <source>Tun Settings</source>
        <translation>Настройки режима TUN</translation>
    </message>
    <message>
        <source>Tun Enable IPv6</source>
        <translation>Вкл. IPv6 в TUN</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Improves Tun routing and performance using nftables. While enabled, this machine cannot be used as a network gateway for other devices.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Улучшает маршрутизацию и производительность TUN с помощью nftables. Пока опция включена, этот компьютер нельзя использовать как сетевой шлюз для других устройств.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Address ranges handed straight to the physical NIC instead of being routed through the core, one CIDR per line. Uncheck to let the core hijack private range traffic instead. Loopback and broadcast are always bypassed and cannot be listed here.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Диапазоны адресов, передаваемые напрямую на физический сетевой адаптер вместо маршрутизации через ядро; по одному CIDR в строке. Снимите флажок, чтобы ядро перехватывало (hijack) трафик частных диапазонов. Loopback и broadcast всегда идут в обход и не могут быть указаны здесь.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Private Range Bypass</source>
        <translation>Обход частных диапазонов</translation>
    </message>
    <message>
        <source>Restore default ranges</source>
        <translation>Восстановить диапазоны по умолчанию</translation>
    </message>
    <message>
        <source>Troubleshooting</source>
        <translation>Устранение неполадок</translation>
    </message>
    <message>
        <source>Invalid Private Range</source>
        <translation>Недопустимый частный диапазон</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not a valid address or CIDR.</source>
        <translation>&quot;%1&quot; — недопустимый адрес или CIDR.</translation>
    </message>
    <message>
        <source>&quot;%1&quot; covers every address, which would stop Tun from routing anything.</source>
        <translation>&quot;%1&quot; охватывает все адреса — в этом случае TUN вообще перестанет маршрутизировать трафик.</translation>
    </message>
    <message>
        <source>If you have trouble starting VPN, you can force reset Core process here.

If still not working, see documentation for more information.
https://matsuridayo.github.io/n-configuration/#vpn-tun</source>
        <translation>Если возникают проблемы с запуском режима TUN, здесь можно принудительно сбросить процесс ядра.

Если это не помогло, подробности см. в документации:
https://matsuridayo.github.io/n-configuration/#vpn-tun</translation>
    </message>
    <message>
        <source>Reset</source>
        <translation>Сбросить</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>Отмена</translation>
    </message>
    <message>
        <source>Strict Route</source>
        <translation>Строгая маршрутизация</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;When enabled, Direct IP CIDRs and rule sets will also be added to the Tun setting to be excluded at OS routing level. IMPORTANT: Large rule sets have been reported to cause massive CPU usage in windows, please enable this option with caution.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Если включено, прямые IP CIDR и наборы правил также будут добавлены в настройки TUN, чтобы исключить их на уровне маршрутизации ОС. ВАЖНО: сообщалось, что крупные наборы правил могут вызвать повышенную нагрузку на процессор в Windows, поэтому включайте эту опцию с осторожностью.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Enable Tun Routing</source>
        <translation>Вкл. TUN-маршрутизацию</translation>
    </message>
    <message>
        <source>Auto Redirect</source>
        <translation>Автоматическое перенаправление</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Forwards bypassed traffic straight out of the network interface instead of rebuilding it as a new connection, which is faster and keeps NAT intact for games and P2P. Applies to UDP and ICMP only; TCP still needs to be sniffed and keeps taking the normal direct path.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Направляет обойдённый (bypassed) трафик напрямую через сетевой интерфейс без пересборки в новое соединение. Это быстрее и сохраняет исходный тип NAT для игр и P2P-сетей. Применяется только к UDP и ICMP; TCP-трафик по-прежнему должен проходить сниффинг и направляется по обычному прямому маршруту.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>L3 Bridge Bypass</source>
        <translation>Обход через L3-мост</translation>
    </message>
    <message>
        <source>Tun Address</source>
        <translation>Адрес TUN</translation>
    </message>
    <message>
        <source>Restore default addresses</source>
        <translation>Восстановить адреса по умолчанию</translation>
    </message>
    <message>
        <source>IPv4 CIDR</source>
        <translation>IPv4 CIDR</translation>
    </message>
    <message>
        <source>IPv6 CIDR</source>
        <translation>IPv6 CIDR</translation>
    </message>
    <message>
        <source>Invalid Tun Address</source>
        <translation>Неверный адрес TUN</translation>
    </message>
    <message>
        <source>IPv4 CIDR is invalid.</source>
        <translation>Некорректный IPv4 CIDR.</translation>
    </message>
    <message>
        <source>IPv6 CIDR is invalid.</source>
        <translation>Некорректный IPv6 CIDR.</translation>
    </message>
</context>
<context>
    <name>DialogVpnAuth</name>
    <message>
        <source>VPN Authentication</source>
        <translation>Аутентификация VPN</translation>
    </message>
    <message>
        <source>Endpoint: %1</source>
        <translation>Конечная точка: %1</translation>
    </message>
    <message>
        <source>Reconnect</source>
        <translation>Переподключить</translation>
    </message>
    <message>
        <source>Submit</source>
        <translation>Отправить</translation>
    </message>
    <message>
        <source>The VPN server sent a message.</source>
        <translation>VPN-сервер отправил сообщение.</translation>
    </message>
    <message>
        <source>Finish signing in at the address shown below, then close this window. The connection continues on its own once the server accepts it.</source>
        <translation>Завершите вход по адресу ниже, затем закройте это окно. Подключение продолжится автоматически, как только сервер его примет.</translation>
    </message>
    <message>
        <source>Open in Browser</source>
        <translation>Открыть в браузере</translation>
    </message>
    <message>
        <source>This server requires single sign-on in a browser, which Throne does not support yet. Cancel here and use a profile with direct credentials, or supply an authentication cookie in the profile&apos;s advanced settings.</source>
        <translation>Этот сервер требует единого входа (SSO) через браузер, а Throne его пока не поддерживает. Отмените запрос и используйте профиль с обычными учетными данными или укажите cookie аутентификации в доп. настройках профиля.</translation>
    </message>
    <message>
        <source>Unsupported authentication request: %1</source>
        <translation>Неподдерживаемый запрос аутентификации: %1</translation>
    </message>
    <message>
        <source>Expires in %1</source>
        <translation>Истекает через %1</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>Имя пользователя</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>Answer</source>
        <translation>Ответ</translation>
    </message>
    <message>
        <source>This request expired.</source>
        <translation>Срок действия запроса истек.</translation>
    </message>
    <message>
        <source>Submitting...</source>
        <translation>Отправка...</translation>
    </message>
</context>
<context>
    <name>EditAdvanced</name>
    <message>
        <source>Dialog</source>
        <translation>Дополнительные настройки</translation>
    </message>
    <message>
        <source>Dial Fields</source>
        <translation>Dial Fields</translation>
    </message>
    <message>
        <source>Connect Timeout</source>
        <translation>Таймаут подключения</translation>
    </message>
    <message>
        <source>Bind Interface</source>
        <translation>Bind Interface</translation>
    </message>
    <message>
        <source>IPv4 Bind Address</source>
        <translation>IPv4 Bind Address</translation>
    </message>
    <message>
        <source>IPv6 Bind Address</source>
        <translation>IPv6 Bind Address</translation>
    </message>
    <message>
        <source>TCP Fast Open</source>
        <translation>TCP Fast Open</translation>
    </message>
    <message>
        <source>TCP MultiPath</source>
        <translation>TCP MultiPath</translation>
    </message>
    <message>
        <source>Reuse Address</source>
        <translation>Reuse Address</translation>
    </message>
    <message>
        <source>Server Address</source>
        <translation>Адрес сервера</translation>
    </message>
    <message>
        <source>UDP Fragment</source>
        <translation>Фрагментация UDP</translation>
    </message>
    <message>
        <source>Interface</source>
        <translation>Интерфейс</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Requires privilege and cannot conflict with existing system interfaces. The internal network stack is used when disabled.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Требует повышенных привилегий и не должен конфликтовать с существующими системными интерфейсами. Если выключено, используется внутренний сетевой стек.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Use System Interface</source>
        <translation>Исп. системный интерфейс</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Custom system interface name. A generated name is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Пользовательское имя системного интерфейса. По умолчанию используется сгенерированное имя.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Interface Name</source>
        <translation>Имя интерфейса</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP NAT expiration time.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Время жизни записи UDP NAT.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>UDP Timeout</source>
        <translation>Таймаут UDP</translation>
    </message>
    <message>
        <source>5m (default)</source>
        <translation>5m (по умолчанию)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP NAT mapping behavior. endpoint_independent is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Режим сопоставления (mapping) UDP NAT. По умолчанию используется endpoint_independent.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>UDP Mapping</source>
        <translation>Сопоставление UDP</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP NAT filtering behavior. endpoint_independent is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Режим фильтрации (filtering) UDP NAT. По умолчанию используется endpoint_independent.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>UDP Filtering</source>
        <translation>Фильтрация UDP</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Maximum number of UDP NAT entries.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Максимальное количество записей UDP NAT.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>UDP NAT Max</source>
        <translation>Макс. записей UDP NAT</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>Enable ECH</source>
        <translation>Вкл. ECH</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Whether this profile injects a forged ClientHello. &amp;quot;Keep Default&amp;quot; follows Preset Settings, unless a Spoof SNI is set below.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Отправляет ли этот профиль поддельный ClientHello. &amp;quot;По умолчанию&amp;quot; — как в предустановленных настройках, если ниже не задан «Поддельный SNI».&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>TLS Spoof</source>
        <translation>Подмена TLS</translation>
    </message>
    <message>
        <source>Keep Default</source>
        <translation>По умолчанию</translation>
    </message>
    <message>
        <source>On</source>
        <translation>Вкл.</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>Выкл.</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Inject a forged TLS ClientHello carrying this SNI before the real one, to pass SNI filtering that allows specific hostnames.&lt;/p&gt;&lt;p&gt;Client only. Requires elevated privileges (root / Administrator). Not supported on Windows ARM64. &amp;quot;wrong-timestamp&amp;quot; is not supported on macOS.&lt;/p&gt;&lt;p&gt;Leave empty to inherit the global value from Preset Settings.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Перед настоящим TLS ClientHello отправляется поддельный с этим SNI, чтобы пройти фильтрацию SNI, которая пропускает только определённые имена хостов.&lt;/p&gt;&lt;p&gt;Только для клиента. Требуются повышенные привилегии (root / администратор). Не поддерживается в Windows ARM64. &amp;quot;wrong-timestamp&amp;quot; не поддерживается в macOS.&lt;/p&gt;&lt;p&gt;Оставьте пустым, чтобы использовать глобальное значение из предустановленных настроек.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Spoof SNI</source>
        <translation>Поддельный SNI</translation>
    </message>
    <message>
        <source>Allowed SNI to forge</source>
        <translation>Разрешённый SNI для подмены</translation>
    </message>
    <message>
        <source>Method</source>
        <translation>Метод</translation>
    </message>
    <message>
        <source>ECH Query Name / Resolver</source>
        <translation>ECH Query Name / Resolver</translation>
    </message>
    <message>
        <source>domain or domain+resolver</source>
        <translation>домен или домен+резолвер</translation>
    </message>
    <message>
        <source>ECH Config</source>
        <translation>ECH Config</translation>
    </message>
    <message>
        <source>Not Set</source>
        <translation>Не задано</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;SHA-256 of the server&apos;s whole certificate in base64, one per line (pcs in share links). A match replaces CA verification, so a self-signed certificate works.&lt;/p&gt;&lt;p&gt;Cannot be combined with a custom Certificate.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;SHA-256 всего сертификата сервера в base64, по одному на строку (pcs в ссылках профилей). При совпадении проверка через CA не нужна, поэтому подойдет и самоподписанный сертификат.&lt;/p&gt;&lt;p&gt;Нельзя использовать вместе с собственным сертификатом (поле «Сертификат»).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Certificate SHA256</source>
        <translation>Certificate SHA256</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;SHA-256 of the server certificate&apos;s public key in base64, one per line. A match replaces CA verification and survives renewals that keep the key.&lt;/p&gt;&lt;p&gt;Cannot be combined with a custom Certificate.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;SHA-256 публичного ключа сертификата сервера в base64, по одному на строку. При совпадении проверка через CA не нужна, и значение остается верным после перевыпуска сертификата с тем же ключом.&lt;/p&gt;&lt;p&gt;Нельзя использовать вместе с собственным сертификатом (поле «Сертификат»).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Public Key SHA256</source>
        <translation>Public Key SHA256</translation>
    </message>
    <message>
        <source>Client Certificate</source>
        <translation>Сертификат клиента</translation>
    </message>
    <message>
        <source>Client Key</source>
        <translation>Ключ клиента</translation>
    </message>
    <message>
        <source>QUIC Parameters</source>
        <translation>Параметры QUIC</translation>
    </message>
    <message>
        <source>Empty fields fall back to the global values in Preset Settings.</source>
        <translation>Для пустых полей используются глобальные значения из предустановленных настроек.</translation>
    </message>
    <message>
        <source>Idle Timeout</source>
        <translation>Idle Timeout</translation>
    </message>
    <message>
        <source>Keep Alive Period</source>
        <translation>Keep Alive Period</translation>
    </message>
    <message>
        <source>Stream Receive Window</source>
        <translation>Stream Receive Window</translation>
    </message>
    <message>
        <source>Connection Receive Window</source>
        <translation>Connection Receive Window</translation>
    </message>
    <message>
        <source>Max Concurrent Streams</source>
        <translation>Max Concurrent Streams</translation>
    </message>
    <message>
        <source>Initial Packet Size</source>
        <translation>Initial Packet Size</translation>
    </message>
    <message>
        <source>Disable Path MTU Discovery</source>
        <translation>Отключить Path MTU Discovery</translation>
    </message>
    <message>
        <source>Disable SNI</source>
        <translation>Отключить SNI</translation>
    </message>
    <message>
        <source>TLS Min Version</source>
        <translation>Мин. версия TLS</translation>
    </message>
    <message>
        <source>TLS Max Version</source>
        <translation>Макс. версия TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;h4 style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;&lt;a name=&quot;query_server_name&quot;/&gt;&lt;span style=&quot; font-size:medium; font-weight:700;&quot;&gt;q&lt;/span&gt;&lt;span style=&quot; font-size:medium; font-weight:700;&quot;&gt;uery_server_name in sing-box&lt;/span&gt;&lt;/h4&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;h4 style=&quot; margin-top:12px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;&quot;&gt;&lt;a name=&quot;query_server_name&quot;/&gt;&lt;span style=&quot; font-size:medium; font-weight:700;&quot;&gt;q&lt;/span&gt;&lt;span style=&quot; font-size:medium; font-weight:700;&quot;&gt;uery_server_name в sing-box&lt;/span&gt;&lt;/h4&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Inherit from group</source>
        <translation>Наследовать от группы</translation>
    </message>
    <message>
        <source>Own address</source>
        <translation>Собственный адрес</translation>
    </message>
    <message>
        <source>IP list</source>
        <translation>Список IP</translation>
    </message>
    <message>
        <source>Missing list</source>
        <translation>Список не найден</translation>
    </message>
    <message>
        <source>Connects to %1 (%2).</source>
        <translation>Подключение к %1 (%2).</translation>
    </message>
    <message>
        <source>%1, so the profile&apos;s own address is used.</source>
        <translation>%1, поэтому используется собственный адрес профиля.</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>Уже задано</translation>
    </message>
</context>
<context>
    <name>EditAnyTLS</name>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>Idle Session Check Interval</source>
        <translation>Idle Session Check Interval</translation>
    </message>
    <message>
        <source>Idle Session Timeout</source>
        <translation>Idle Session Timeout</translation>
    </message>
    <message>
        <source>Min Idle Session</source>
        <translation>Min Idle Session</translation>
    </message>
</context>
<context>
    <name>EditAutoSelector</name>
    <message>
        <source>Servers from</source>
        <translation>Серверы из</translation>
    </message>
    <message>
        <source>The group whose profiles this selector picks from. New servers added by a subscription update join automatically — you never have to edit this profile again.</source>
        <translation>Группа, из профилей которой выбирает автовыбор. Новые серверы, добавленные обновлением подписки, подхватываются автоматически — редактировать этот профиль больше не придется.</translation>
    </message>
    <message>
        <source>Only names matching</source>
        <translation>Фильтр по названию</translation>
    </message>
    <message>
        <source>leave empty to use every profile in the group</source>
        <translation>оставьте поле пустым, чтобы использовать все профили в группе</translation>
    </message>
    <message>
        <source>Optional regular expression. Only profiles whose name matches are considered — useful to pick out one country or provider.</source>
        <translation>Необязательное регулярное выражение. Учитываются только профили с подходящим названием — удобно, чтобы выбрать одну страну или провайдера.</translation>
    </message>
    <message>
        <source>Share traffic between the best profiles</source>
        <translation>Распределить трафик между лучшими профилями</translation>
    </message>
    <message>
        <source>Off by default: one profile carries everything and the rest stand by. Turn on to spread traffic over the profiles that are confirmed working.</source>
        <translation>По умолчанию выключено: весь трафик идет через один профиль, остальные в резерве. Включите, чтобы распределять трафик между профилями, работоспособность которых подтверждена.</translation>
    </message>
    <message>
        <source>Preferred profile</source>
        <translation>Предпочтительный профиль</translation>
    </message>
    <message>
        <source>Use automatic</source>
        <translation>Использовать автовыбор</translation>
    </message>
    <message>
        <source>Stop preferring this profile and let the selector choose on measurements again.</source>
        <translation>Перестать отдавать предпочтение этому профилю и позволить автовыбору снова выбирать на основе измерений.</translation>
    </message>
    <message>
        <source>Advanced…</source>
        <translation>Дополнительно…</translation>
    </message>
    <message>
        <source>Everything here has a working default. Open it only if you want to tune how the selector measures and switches.</source>
        <translation>У всех параметров здесь есть рабочие значения по умолчанию. Открывайте этот раздел, только если хотите настроить, как автовыбор измеряет профили и переключается между ними.</translation>
    </message>
    <message>
        <source>Which profiles to use</source>
        <translation>Какие профили использовать</translation>
    </message>
    <message>
        <source>Only countries</source>
        <translation>Только страны</translation>
    </message>
    <message>
        <source>e.g. DE,NL,FR</source>
        <translation>например, DE,NL,FR</translation>
    </message>
    <message>
        <source>Comma-separated country codes, empty means any. Uses the country found by the IP test, so profiles that were never IP-tested are skipped while this is set.</source>
        <translation>Коды стран через запятую; пустое поле — любая страна. Используется страна, определённая IP-тестом, поэтому, пока фильтр задан, профили без IP-теста пропускаются.</translation>
    </message>
    <message>
        <source>Run the best</source>
        <translation>Запускать лучших</translation>
    </message>
    <message>
        <source> profiles</source>
        <translation> проф.</translation>
    </message>
    <message>
        <source>How many profiles are loaded into the running config. The selector switches between these instantly, with no reconnect. Higher costs more memory and startup time.</source>
        <translation>Сколько профилей загружается в работающую конфигурацию. Автовыбор переключается между ними мгновенно, без переподключения. Чем больше значение, тем больше расход памяти и время запуска.</translation>
    </message>
    <message>
        <source>Rank at most</source>
        <translation>Ранжировать не более</translation>
    </message>
    <message>
        <source>Hard ceiling on how many profiles are measured and kept in the ranked list. When every running profile dies, replacements are taken from this list.</source>
        <translation>Жёсткий предел числа профилей, которые измеряются и хранятся в рейтинге. Когда все запущенные профили перестают работать, замена берётся из этого списка.</translation>
    </message>
    <message>
        <source>Trust results for</source>
        <translation>Срок доверия к результатам</translation>
    </message>
    <message>
        <source> min</source>
        <translation> мин</translation>
    </message>
    <message>
        <source>always re-test</source>
        <translation>всегда перепроверять</translation>
    </message>
    <message>
        <source>URL test results this recent are reused instead of being measured again — including tests you ran yourself on the group, so starting the selector right after one does not repeat the work. Older results are treated as if the profile had never been tested. Set to zero to always run a fresh test.</source>
        <translation>Результаты URL-тестов не старше этого срока используются повторно вместо новых замеров — в том числе тесты, которые вы сами запускали для группы, так что автовыбор, запущенный сразу после них, не повторяет работу. Профиль с более старыми результатами считается непроверенным. Установите 0, чтобы всегда выполнять новый тест.</translation>
    </message>
    <message>
        <source>Skip failed profiles</source>
        <translation>Пропускать профили, не прошедшие тест</translation>
    </message>
    <message>
        <source>Keeps servers whose last test failed out of the pool, as long as that result is still within the trust window. Turn off if your tests are unreliable and you would rather let the selector decide for itself.</source>
        <translation>Исключает из пула серверы, последний тест которых завершился неудачно, пока этот результат не старше срока доверия. Выключите, если ваши тесты ненадежны и вы предпочитаете, чтобы автовыбор решал сам.</translation>
    </message>
    <message>
        <source>Health checks</source>
        <translation>Проверка доступности</translation>
    </message>
    <message>
        <source>Keep ready</source>
        <translation>Держать наготове</translation>
    </message>
    <message>
        <source>How many profiles are kept confirmed working at all times, so a failure can be covered instantly. These are also the ones traffic is shared between when balancing is on.</source>
        <translation>Сколько профилей постоянно держать проверенными и рабочими, чтобы при сбое мгновенно переключиться. Между ними же распределяется трафик при включённой балансировке.</translation>
    </message>
    <message>
        <source>Check closely</source>
        <translation>Часто проверять</translation>
    </message>
    <message>
        <source>The top-ranked profiles are re-checked every interval. The rest are still checked, just spread over the slower full sweep.</source>
        <translation>Профили из верхней части рейтинга перепроверяются с заданным интервалом проверки. Остальные тоже проверяются, но реже — в ходе более медленного полного прохода.</translation>
    </message>
    <message>
        <source>Check interval</source>
        <translation>Интервал проверки</translation>
    </message>
    <message>
        <source> s</source>
        <translation> сек</translation>
    </message>
    <message>
        <source>How often the closely-checked profiles are measured again.</source>
        <translation>Как часто заново измеряются профили из пункта «Часто проверять».</translation>
    </message>
    <message>
        <source>Full sweep every</source>
        <translation>Полный проход каждые</translation>
    </message>
    <message>
        <source>How long one pass over every running profile takes. Checks are spread evenly across it, so a large pool never causes a burst of traffic.</source>
        <translation>Сколько длится один проход по всем запущенным профилям. Проверки распределяются по нему равномерно, поэтому даже большой пул не вызывает всплеска трафика.</translation>
    </message>
    <message>
        <source>Watch profile in use</source>
        <translation>Мониторинг текущего профиля</translation>
    </message>
    <message>
        <source>How often the profile currently carrying your traffic is checked on its own. Keep this short: a server can stop working while still accepting connections, and nothing else would notice until the next full check.</source>
        <translation>Как часто автоматически проверяется профиль, через который сейчас идёт ваш трафик. Делайте интервал коротким: сервер может перестать работать, продолжая принимать соединения, и до следующей полной проверки этого никто не заметит.</translation>
    </message>
    <message>
        <source>Samples kept</source>
        <translation>Число хранимых замеров</translation>
    </message>
    <message>
        <source>Ranking uses the average and the jitter over this many recent checks, so a single bad result cannot demote a good server.</source>
        <translation>Для ранжирования используются среднее значение и джиттер по этому числу последних проверок, поэтому один неудачный результат не понизит хороший сервер.</translation>
    </message>
    <message>
        <source>Switching</source>
        <translation>Переключение</translation>
    </message>
    <message>
        <source>Switch tolerance</source>
        <translation>Порог переключения</translation>
    </message>
    <message>
        <source> ms</source>
        <translation> мс</translation>
    </message>
    <message>
        <source>Another profile must be at least this much faster before the selector moves to it. Higher values mean fewer switches.</source>
        <translation>Другой профиль должен быть быстрее как минимум на это значение, чтобы автовыбор переключился на него. Чем больше значение, тем реже переключения.</translation>
    </message>
    <message>
        <source>Maximum latency</source>
        <translation>Максимальная задержка</translation>
    </message>
    <message>
        <source>no limit</source>
        <translation>без ограничений</translation>
    </message>
    <message>
        <source>Profiles slower than this are never selected, even if nothing better is available.</source>
        <translation>Профили медленнее этого значения никогда не выбираются, даже если лучших нет.</translation>
    </message>
    <message>
        <source>Failover attempts</source>
        <translation>Кол-во попыток при сбое</translation>
    </message>
    <message>
        <source>When the chosen profile fails to connect, how many others to try immediately before the app sees an error.</source>
        <translation>Сколько других профилей сразу попробовать, если выбранный не подключается, прежде чем приложение получит ошибку.</translation>
    </message>
    <message>
        <source>Drop connections on switch</source>
        <translation>Разрывать подключения при переключении</translation>
    </message>
    <message>
        <source>Applies when the selector switches because a profile got worse or failed. On: everything moves to the new profile at once. Off: current downloads finish on the old one. Balance rotations never drop connections either way.</source>
        <translation>Применяется, когда автовыбор переключается из-за ухудшения или сбоя профиля. Включено: всё сразу переходит на новый профиль. Выключено: текущие загрузки завершаются на старом. Ротация в режиме балансировки в любом случае не разрывает подключения.</translation>
    </message>
    <message>
        <source>Load balancing</source>
        <translation>Балансировка нагрузки</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>Режим</translation>
    </message>
    <message>
        <source>How traffic is spread over the ready profiles.</source>
        <translation>Как трафик распределяется между готовыми профилями.</translation>
    </message>
    <message>
        <source>Rotate every</source>
        <translation>Переключать каждые</translation>
    </message>
    <message>
        <source>How long the selector stays on one profile before moving to the next ready one.</source>
        <translation>Как долго автовыбор остаётся на одном профиле, прежде чем перейти к следующему готовому.</translation>
    </message>
    <message>
        <source>Test endpoints</source>
        <translation>Адреса для проверки</translation>
    </message>
    <message>
        <source>Test URL</source>
        <translation>URL проверки</translation>
    </message>
    <message>
        <source>empty = use the global latency test URL</source>
        <translation>если пусто, используется глобальный «URL теста задержки»</translation>
    </message>
    <message>
        <source>Fetched through each profile to measure it.</source>
        <translation>Запрашивается через каждый профиль, чтобы измерить его задержку.</translation>
    </message>
    <message>
        <source>Connectivity URL</source>
        <translation>URL проверки связи</translation>
    </message>
    <message>
        <source>optional — inherits the Direct Test URL</source>
        <translation>необязательно — наследует «URL проверки Direct»</translation>
    </message>
    <message>
        <source>Fetched WITHOUT the proxy to tell a dead internet connection apart from dead servers, and to notice the moment it comes back. Leave empty to inherit the Direct Test URL from Basic Settings, and empty there too to rely on the operating system&apos;s network state. A URL that turns out to be blocked can only delay a resume by a few minutes; it can never keep the selector paused.</source>
        <translation>Запрашивается БЕЗ прокси, чтобы отличить отсутствие интернета от неработающих серверов и сразу заметить, когда связь вернется. Оставьте пустым, чтобы использовать «URL проверки Direct» из основных настроек; если пусто и там, используется состояние сети операционной системы. Если этот URL окажется заблокирован, это лишь задержит возобновление на несколько минут, но никогда не оставит автовыбор на паузе.</translation>
    </message>
    <message>
        <source>Rotate on a timer (keeps sessions stable)</source>
        <translation>Переключение по таймеру (обеспечивает стабильность сеансов)</translation>
    </message>
    <message>
        <source>Per connection (widest spread)</source>
        <translation>На каждое подключение (максимальное распределение)</translation>
    </message>
    <message>
        <source>Advanced ▴</source>
        <translation>Дополнительно ▴</translation>
    </message>
    <message>
        <source>Name cannot be empty.</source>
        <translation>Название не может быть пустым.</translation>
    </message>
    <message>
        <source>Select the group this auto selector should track.</source>
        <translation>Выберите группу, за которой должен следить этот автовыбор.</translation>
    </message>
    <message>
        <source>The name filter is not a valid regular expression: %1</source>
        <translation>Фильтр по названию не является допустимым регулярным выражением: %1</translation>
    </message>
    <message>
        <source>%1 — chosen by you, so the selector stays on it while it works</source>
        <translation>%1 — выбран вами, поэтому автовыбор останется на нём, пока профиль работает</translation>
    </message>
    <message>
        <source>The best profile carries all traffic; the other ready profiles are kept verified so one can take over instantly.</source>
        <translation>Лучший профиль обрабатывает весь трафик; остальные готовые профили регулярно проверяются, чтобы любой из них мог мгновенно его заменить.</translation>
    </message>
    <message>
        <source>New connections move to another good profile on each rotation; open ones are left alone and finish where they started. Traffic is counted exactly.</source>
        <translation>При каждой ротации новые подключения переходят на другой хороший профиль; открытые остаются на прежнем и завершаются там же. Трафик подсчитывается точно.</translation>
    </message>
    <message>
        <source>Every new connection may take a different profile. Widest spread, but your exit IP changes mid-session and per-profile traffic becomes approximate.</source>
        <translation>Каждое новое подключение может пойти через другой профиль. Максимальное распределение, но ваш внешний IP меняется посреди сеанса, а трафик по профилям считается приблизительно.</translation>
    </message>
    <message>
        <source>%1 of %2 profiles in the group can be used; %3 would run.</source>
        <translation>Профилей в группе: %2, можно использовать: %1, будет запущено: %3.</translation>
    </message>
    <message>
        <source>%1 %2</source>
        <translation>%1 %2</translation>
    </message>
    <message>
        <source>Skipped: %1.</source>
        <translation>Пропущено: %1.</translation>
    </message>
    <message>
        <source>, </source>
        <translation>, </translation>
    </message>
    <message>
        <source>Every profile&apos;s last test failed, which usually means the network was down rather than the servers, so all %1 are kept and will be re-checked.</source>
        <translation>При последней проверке не сработал ни один профиль — обычно это значит, что не работала сеть, а не серверы, поэтому все %1 сохраняются и будут проверены повторно.</translation>
    </message>
    <message>
        <source>More than %1 profiles match, so only the best-ranked ones are kept.</source>
        <translation>Подходящих профилей больше %1, поэтому оставлены только лучшие по рейтингу.</translation>
    </message>
    <message>
        <source>%1 have a recent test result that will be reused.</source>
        <translation>Профилей с недавним результатом теста (он будет использован повторно): %1.</translation>
    </message>
    <message>
        <source>The rest will be measured before the selector starts.</source>
        <translation>Остальные профили будут измерены до запуска автовыбора.</translation>
    </message>
</context>
<context>
    <name>EditChain</name>
    <message>
        <source>Traffic order is from top to bottom</source>
        <translation>Трафик проходит по цепочке сверху вниз</translation>
    </message>
    <message>
        <source>Select Profile</source>
        <translation>Выбор профиля</translation>
    </message>
    <message>
        <source>Name cannot be empty.</source>
        <translation>Название не может быть пустым.</translation>
    </message>
    <message>
        <source>Profiles that use an extra core can only be the final hop in the chain. Move it to the top of the list.</source>
        <translation>Профили, использующие доп. ядро, могут быть только последним звеном в цепочке. Переместите его в начало списка.</translation>
    </message>
    <message>
        <source>Only one extra-core profile is allowed in a chain.</source>
        <translation>В цепочке допускается только один профиль с доп. ядром.</translation>
    </message>
</context>
<context>
    <name>EditCustom</name>
    <message>
        <source>Core</source>
        <translation>Ядро</translation>
    </message>
    <message>
        <source>Json Editor</source>
        <translation>Редактор JSON</translation>
    </message>
    <message>
        <source>Outbound JSON, please read the documentation.</source>
        <translation>Outbound JSON — пожалуйста, прочтите документацию.</translation>
    </message>
    <message>
        <source>Xray outbound JSON, please read the Xray documentation.</source>
        <translation>Xray Outbound JSON — пожалуйста, прочтите документацию Xray.</translation>
    </message>
    <message>
        <source>Xray full config (Throne adds a socks inbound and uses sing-box for tun/routing).</source>
        <translation>Полная конфигурация Xray (Throne добавляет socks inbound и использует sing-box для tun / маршрутизации).</translation>
    </message>
    <message>
        <source>Please fill the complete config.</source>
        <translation>Пожалуйста, заполните конфигурацию полностью.</translation>
    </message>
    <message>
        <source>Name cannot be empty.</source>
        <translation>Название не может быть пустым.</translation>
    </message>
</context>
<context>
    <name>EditDirect</name>
    <message>
        <source>Use the Advanced section to specify bind options</source>
        <translation>Используйте кнопку «Доп. настройки», чтобы указать параметры привязки</translation>
    </message>
</context>
<context>
    <name>EditExtraCore</name>
    <message>
        <source>Socks address</source>
        <translation>Адрес SOCKS</translation>
    </message>
    <message>
        <source>127.0.0.1</source>
        <translation>127.0.0.1</translation>
    </message>
    <message>
        <source>Socks port</source>
        <translation>Порт SOCKS</translation>
    </message>
    <message>
        <source>1080</source>
        <translation>1080</translation>
    </message>
    <message>
        <source>Core path</source>
        <translation>Путь к ядру</translation>
    </message>
    <message>
        <source>Choose from file</source>
        <translation>Выбрать файл</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;args to pass to the executable. You can insert a single %s and it will be replaced with the path to a file containing the config entered below.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Аргументы для исполняемого файла. Можно вставить один %s — он будет заменён на путь к файлу с конфигурацией, указанной ниже.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Args</source>
        <translation>Аргументы</translation>
    </message>
    <message>
        <source>run -confPath %s</source>
        <translation>run -confPath %s</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;contents of the config file that will be passed to the extra core process if %s is specified in the args&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Содержимое конфигурационного файла, которое будет передано процессу дополнительного ядра, если %s указано в аргументах&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Config</source>
        <translation>Конфигурация</translation>
    </message>
    <message>
        <source>No logs</source>
        <translation>Без логов</translation>
    </message>
</context>
<context>
    <name>EditHttp</name>
    <message>
        <source>Username</source>
        <translation>Имя пользователя</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
</context>
<context>
    <name>EditHysteria</name>
    <message>
        <source>Protocol Version</source>
        <translation>Версия протокола</translation>
    </message>
    <message>
        <source>Down Mbps</source>
        <translation>Down Mbps</translation>
    </message>
    <message>
        <source>Recv window conn</source>
        <translation>Recv Window Conn</translation>
    </message>
    <message>
        <source>Hop Interval</source>
        <translation>Hop Interval</translation>
    </message>
    <message>
        <source>Server Ports</source>
        <translation>Порты сервера</translation>
    </message>
    <message>
        <source>Authentication Type</source>
        <translation>Тип аутентификации</translation>
    </message>
    <message>
        <source>Up Mbps</source>
        <translation>Up Mbps</translation>
    </message>
    <message>
        <source>Hop Interval Max</source>
        <translation>Hop Interval Max</translation>
    </message>
    <message>
        <source>When set, each hop waits a random duration between Hop Interval and this value. Requires Hop Interval.</source>
        <translation>Если задано, перед каждой сменой порта ожидается случайное время между «Hop Interval» и этим значением. Требует заданного «Hop Interval».</translation>
    </message>
    <message>
        <source>BBR Profile</source>
        <translation>BBR Profile</translation>
    </message>
    <message>
        <source>Congestion control aggressiveness. Leave empty to use the default (standard).</source>
        <translation>Агрессивность контроля перегрузки. Оставьте пустым, чтобы использовать значение по умолчанию (standard).</translation>
    </message>
    <message>
        <source>Authentication Payload</source>
        <translation>Authentication Payload</translation>
    </message>
    <message>
        <source>Obfuscation</source>
        <translation>Obfuscation</translation>
    </message>
    <message>
        <source>salamander</source>
        <translation>salamander</translation>
    </message>
    <message>
        <source>gecko</source>
        <translation>gecko</translation>
    </message>
    <message>
        <source>Obfuscation Password</source>
        <translation>Obfuscation Password</translation>
    </message>
    <message>
        <source>Minimum Packet Size</source>
        <translation>Минимальный размер пакета</translation>
    </message>
    <message>
        <source>Maximum Packet Size</source>
        <translation>Максимальный размер пакета</translation>
    </message>
    <message>
        <source>Recv window</source>
        <translation>Recv Window</translation>
    </message>
    <message>
        <source>Disable Mtu Discovery</source>
        <translation>Отключить MTU Discovery</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>Parroting makes the QUIC handshake look like Chrome&apos;s. On by default.
It overrides idle timeout, max concurrent streams,
initial packet size and the initial receive windows.</source>
        <translation>Мимикрия (parroting) делает хендшейк QUIC похожим на хендшейк Chrome. Включена по умолчанию.
Она переопределяет «Idle Timeout», «Max Concurrent Streams»,
«Initial Packet Size» и начальные окна приёма.</translation>
    </message>
    <message>
        <source>Disable Chrome Parrot</source>
        <translation>Отключить мимикрию под Chrome</translation>
    </message>
    <message>
        <source>Reach a Hysteria2 server behind NAT through a Hysteria Realm
rendezvous service. Replaces the server address and port.</source>
        <translation>Подключаться к серверу Hysteria2 за NAT через сервис-посредник Hysteria Realm.
Заменяет адрес и порт сервера.</translation>
    </message>
    <message>
        <source>Realm (NAT Traversal)</source>
        <translation>Realm (обход NAT)</translation>
    </message>
    <message>
        <source>Realm Options</source>
        <translation>Параметры Realm</translation>
    </message>
    <message>
        <source>Realm needs a URL, an ID and at least one STUN server.</source>
        <translation>Для Realm нужны URL, ID и хотя бы один STUN-сервер.</translation>
    </message>
</context>
<context>
    <name>EditHysteriaRealm</name>
    <message>
        <source>Realm (NAT Traversal)</source>
        <translation>Realm (обход NAT)</translation>
    </message>
    <message>
        <source>The realm is a rendezvous service: it hands out the server&apos;s current public addresses so the connection can be hole-punched directly.</source>
        <translation>Realm — это сервис-посредник: он сообщает текущие публичные адреса сервера, чтобы соединение можно было установить напрямую через hole punching.</translation>
    </message>
    <message>
        <source>Realm URL</source>
        <translation>Realm URL</translation>
    </message>
    <message>
        <source>Realm ID</source>
        <translation>Realm ID</translation>
    </message>
    <message>
        <source>The slot the target Hysteria2 server registered on the realm.</source>
        <translation>Слот, под которым целевой сервер Hysteria2 зарегистрирован в Realm.</translation>
    </message>
    <message>
        <source>Realm Token</source>
        <translation>Realm Token</translation>
    </message>
    <message>
        <source>STUN Servers</source>
        <translation>STUN-серверы</translation>
    </message>
    <message>
        <source>Comma separated host or host:port entries used to discover this client&apos;s public addresses.</source>
        <translation>Записи вида host или host:port через запятую, по которым определяются публичные адреса этого клиента.</translation>
    </message>
    <message>
        <source>IP Version</source>
        <translation>Версия IP</translation>
    </message>
    <message>
        <source>Maintain a UDP port mapping on the local gateway via UPnP or NAT-PMP.
Improves hole-punching reliability. Requires IPv4.</source>
        <translation>Поддерживать проброс UDP-порта на локальном шлюзе через UPnP или NAT-PMP.
Повышает надёжность hole punching. Требуется IPv4.</translation>
    </message>
    <message>
        <source>Port Mapping</source>
        <translation>Проброс порта</translation>
    </message>
    <message>
        <source>Mapping Timeout</source>
        <translation>Таймаут проброса</translation>
    </message>
    <message>
        <source>Mapping Lifetime</source>
        <translation>Время жизни проброса</translation>
    </message>
</context>
<context>
    <name>EditJuicity</name>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>UUID</source>
        <translation>UUID</translation>
    </message>
</context>
<context>
    <name>EditMasque</name>
    <message>
        <source>Base64 DER of the enrolled ECDSA P-256 key; it signs the TLS client certificate.</source>
        <translation>Зарегистрированный ключ ECDSA P-256 в формате Base64 DER; им подписывается клиентский TLS-сертификат.</translation>
    </message>
    <message>
        <source>Private Key</source>
        <translation>Private Key</translation>
    </message>
    <message>
        <source>Base64 DER of the server&apos;s public key. When set, it pins the server certificate, even with Allow insecure.</source>
        <translation>Публичный ключ сервера в формате Base64 DER. Если он задан, сертификат сервера закрепляется (pinning) даже при включённом «Allow insecure».</translation>
    </message>
    <message>
        <source>Peer Public Key</source>
        <translation>Peer Public Key</translation>
    </message>
    <message>
        <source>Comma separated list of the tunnel&apos;s own addresses, e.g. 172.16.0.2/32,2606:4700::1/128</source>
        <translation>Собственные адреса туннеля через запятую, напр. 172.16.0.2/32,2606:4700::1/128</translation>
    </message>
    <message>
        <source>Local Address</source>
        <translation>Локальный адрес</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>HTTP/3 runs over UDP; HTTP/2 runs over TLS on TCP and works where UDP is blocked.</source>
        <translation>HTTP/3 работает по протоколу UDP; HTTP/2 работает по протоколу TLS над TCP и поддерживается там, где UDP заблокирован.</translation>
    </message>
    <message>
        <source>HTTP Version</source>
        <translation>Версия HTTP</translation>
    </message>
    <message>
        <source>HTTP/3 (fallback to HTTP/2)</source>
        <translation>HTTP/3 (с откатом на HTTP/2)</translation>
    </message>
    <message>
        <source>HTTP/3 only</source>
        <translation>Только HTTP/3</translation>
    </message>
    <message>
        <source>Register a new Cloudflare WARP device for MASQUE and fill the key, address and server fields.</source>
        <translation>Зарегистрировать новое устройство Cloudflare WARP для MASQUE и заполнить поля ключа, адреса и сервера.</translation>
    </message>
    <message>
        <source>Generate WARP identity</source>
        <translation>Сгенерировать учётные данные WARP</translation>
    </message>
    <message>
        <source>Generating identity...</source>
        <translation>Генерация учётных данных...</translation>
    </message>
    <message>
        <source>Failed to generate WARP identity</source>
        <translation>Не удалось сгенерировать учётные данные WARP</translation>
    </message>
    <message>
        <source>Success!</source>
        <translation>Успешно!</translation>
    </message>
</context>
<context>
    <name>EditMieru</name>
    <message>
        <source>Username</source>
        <translation>Имя пользователя</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>Transport</source>
        <translation>Транспорт</translation>
    </message>
    <message>
        <source>Multiplexing</source>
        <translation>Мультиплексирование</translation>
    </message>
    <message>
        <source>Traffic Pattern</source>
        <translation>Traffic Pattern</translation>
    </message>
    <message>
        <source>Server Ports</source>
        <translation>Порты сервера</translation>
    </message>
</context>
<context>
    <name>EditNaive</name>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>UDP over TCP</source>
        <translation>UDP over TCP</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>Имя пользователя</translation>
    </message>
    <message>
        <source>Congestion Control</source>
        <translation>Congestion Control</translation>
    </message>
    <message>
        <source>QUIC</source>
        <translation>QUIC</translation>
    </message>
    <message>
        <source>Extra Headers</source>
        <translation>Доп. заголовки</translation>
    </message>
    <message>
        <source>Headers sent to the proxy server, e.g. X-Username=&quot;user&quot; X-Password=&quot;pass&quot;</source>
        <translation>Заголовки, отправляемые прокси-серверу, напр. X-Username=&quot;user&quot; X-Password=&quot;pass&quot;</translation>
    </message>
    <message>
        <source>Insecure Concurrency</source>
        <translation>Insecure Concurrency</translation>
    </message>
    <message>
        <source>Parallel tunnel connections to the server, 0 to disable. More connections are easier to detect.</source>
        <translation>Число параллельных туннельных подключений к серверу, 0 — отключить. Чем больше подключений, тем проще их обнаружить.</translation>
    </message>
</context>
<context>
    <name>EditOpenConnect</name>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenConnect protocol flavor. Leave empty to use the core default (anyconnect).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Вариант протокола OpenConnect. Оставьте пустым, чтобы использовать значение ядра по умолчанию (anyconnect).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Flavor</source>
        <translation>Протокол</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Username used to fill matching authentication form fields.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Имя пользователя для заполнения соответствующих полей формы аутентификации.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>Имя пользователя</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Password used to fill matching authentication form fields.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Пароль для заполнения соответствующих полей формы аутентификации.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Preselects a matching group, realm, domain or gateway choice when the flavor supports it.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Заранее выбирает подходящую группу, realm, домен или шлюз, если протокол это поддерживает.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Auth Group</source>
        <translation>Группа аутентификации</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Path part of the server URL, appended to the address above. Load-bearing on gp, f5 and fortinet portals.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Путь в URL сервера, добавляется к адресу выше. Важен для порталов gp, f5 и fortinet.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Server Path</source>
        <translation>Путь на сервере</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Preferred tunnel MTU. Values below 576 are treated as 576; the maximum is 65535.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Предпочтительный MTU туннеля. Значения меньше 576 считаются равными 576; максимум — 65535.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>OTP</source>
        <translation>OTP</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Traffic that falls through to this profile reaches the tunnel only when the destination is inside a subnet the VPN server advertises; anything else is blocked. Routing rules that name this profile explicitly are not affected and always win.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Трафик, попадающий в этот профиль по умолчанию, идет в туннель, только если адрес назначения входит в подсеть, объявленную VPN-сервером; остальной трафик блокируется. Правила маршрутизации, явно указывающие этот профиль, не затрагиваются и всегда имеют приоритет.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Only route advertised network</source>
        <translation>Только объявленные сервером сети</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;How DNS uses the resolvers the VPN server pushes when this profile is started.&lt;/p&gt;&lt;p&gt;None: ignore them. Use this if the server claims every domain and breaks public DNS.&lt;br/&gt;Prefer: resolve the names the server claims - its split-DNS suffixes and search domains - through them, and everything else as usual.&lt;br/&gt;Strict: also send every remote DNS query to them instead of the remote DNS server, and fail the lookups they cannot answer. Direct lookups are unchanged.&lt;/p&gt;&lt;p&gt;As an endpoint in a routing profile, Prefer and Strict both resolve only the names the server claims.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Как DNS использует резолверы, которые VPN-сервер передает при запуске этого профиля.&lt;/p&gt;&lt;p&gt;Нет: игнорировать их. Выберите, если сервер объявляет все домены и ломает публичный DNS.&lt;br/&gt;Предпочитать: разрешать через них имена, объявленные сервером (его суффиксы split-DNS и домены поиска), а все остальное — как обычно.&lt;br/&gt;Строго: также отправлять им все удаленные DNS-запросы вместо удаленного DNS-сервера, а запросы, на которые они не могут ответить, завершать ошибкой. Прямые запросы не меняются.&lt;/p&gt;&lt;p&gt;Если профиль используется как конечная точка в профиле маршрутизации, в режимах «Предпочитать» и «Строго» через них разрешаются только имена, объявленные сервером.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Tunnel DNS</source>
        <translation>DNS туннеля</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>Дополнительно</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Disables verification of the server certificate and hostname. This lets an active attacker impersonate the VPN server; prefer a CA certificate or a peer fingerprint.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Отключает проверку сертификата и имени хоста сервера. Это позволяет активному злоумышленнику выдать себя за VPN-сервер; лучше используйте сертификат CA или отпечаток сервера.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Allow insecure</source>
        <translation>Allow insecure</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Server name used for TLS SNI and certificate hostname verification. The hostname from the address is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Имя сервера для TLS SNI и проверки имени хоста в сертификате. Если пусто, используется имя хоста из адреса.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Server Name</source>
        <translation>Server Name</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Additional trusted CA certificate content in PEM format, added to the system certificate pool.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Дополнительный доверенный сертификат CA в формате PEM, добавляется к системному хранилищу сертификатов.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CA Certificate</source>
        <translation>Сертификат CA</translation>
    </message>
    <message>
        <source>Not set</source>
        <translation>Не задано</translation>
    </message>
    <message>
        <source>Client Certificate</source>
        <translation>Сертификат клиента</translation>
    </message>
    <message>
        <source>Client Key</source>
        <translation>Ключ клиента</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Password for the encrypted client private key.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Пароль для зашифрованного закрытого ключа клиента.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Client Key Password</source>
        <translation>Пароль ключа клиента</translation>
    </message>
    <message>
        <source>Prefer</source>
        <translation>Предпочитать</translation>
    </message>
    <message>
        <source>Strict</source>
        <translation>Строго</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Binds an authenticator entry to this profile. %1 in the username, password, software token or form entry fields is replaced with a generated code at connect time.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Привязывает к этому профилю запись аутентификатора. %1 в полях имени пользователя, пароля, программного токена или формы при подключении заменяется сгенерированным кодом.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>%1 is replaced at connect time</source>
        <translation>%1 заменяется при подключении</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>Уже задано</translation>
    </message>
    <message>
        <source>None</source>
        <translation>Нет</translation>
    </message>
</context>
<context>
    <name>EditOpenConnectAdvanced</name>
    <message>
        <source>OpenConnect Advanced</source>
        <translation>Доп. настройки OpenConnect</translation>
    </message>
    <message>
        <source>Session</source>
        <translation>Сессия</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Existing authentication session used to connect without first prompting for credentials. The accepted format depends on the flavor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Существующая сессия аутентификации для подключения без предварительного запроса учетных данных. Допустимый формат зависит от протокола.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Cookie</source>
        <translation>Cookie</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Operating system identity reported to the server. anyconnect, gp and pulse accept linux, linux-64, win, mac-intel, android and apple-ios. The system platform is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Операционная система, которую клиент сообщает серверу. anyconnect, gp и pulse принимают linux, linux-64, win, mac-intel, android и apple-ios. По умолчанию используется платформа системы.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Reported OS</source>
        <translation>Сообщаемая ОС</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;User agent reported to the server. The default is flavor specific.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;User agent, сообщаемый серверу. Значение по умолчанию зависит от протокола.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>User Agent</source>
        <translation>User Agent</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Client version reported separately from the user agent. Currently used by AnyConnect XML authentication.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Версия клиента, сообщаемая отдельно от user agent. Сейчас используется при XML-аутентификации AnyConnect.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Version</source>
        <translation>Версия</translation>
    </message>
    <message>
        <source>v9.21 (default)</source>
        <translation>v9.21 (по умолчанию)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Local hostname reported to the server. The system hostname is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Локальное имя хоста, сообщаемое серверу. По умолчанию используется имя хоста системы.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Local Hostname</source>
        <translation>Локальное имя хоста</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;This is the token the core generates itself. To use a Throne OTP profile, bind it on the main editor instead.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Это токен, который генерирует само ядро. Чтобы использовать OTP-профиль Throne, привяжите его в основном редакторе.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Software Token</source>
        <translation>Программный токен</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;totp and hotp are one-time password modes, stoken is an RSA SecurID software token and oidc is an access token used for HTTP Bearer authentication.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;totp и hotp — режимы одноразовых паролей, stoken — программный токен RSA SecurID, oidc — токен доступа для аутентификации HTTP Bearer.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>Режим</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;A Base32 secret, a base32: prefixed secret or an otpauth:// URI for totp and hotp; the encoded CTF content for stoken; the access token for oidc. Conflicts with the secret path.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Secret в формате Base32, Secret с префиксом base32: или URI otpauth:// для totp и hotp; закодированное содержимое CTF для stoken; токен доступа для oidc. Несовместимо с полем «Путь к Secret».&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Secret</source>
        <translation>Secret</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Path to the token secret or access token. Conflicts with the inline secret.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Путь к Secret токена или к токену доступа. Несовместимо с полем «Secret».&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Secret Path</source>
        <translation>Путь к Secret</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;RSA SecurID PIN for stoken mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;PIN RSA SecurID для режима stoken.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>PIN</source>
        <translation>PIN</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Password for decrypting a password-protected RSA SecurID token.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Пароль для расшифровки защищенного паролем токена RSA SecurID.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Device ID for decrypting a device-bound RSA SecurID token.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;ID устройства для расшифровки токена RSA SecurID, привязанного к устройству.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Device ID</source>
        <translation>ID устройства</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Initial counter for hotp mode. Empty uses the counter from an otpauth:// URI, otherwise zero.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Начальное значение счетчика для режима hotp. Если пусто, используется счетчик из URI otpauth://, иначе ноль.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Counter</source>
        <translation>Счетчик</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Allowed server certificate fingerprints, one per line. Accepts a bare SHA-1 fingerprint or sha1:, sha256: and pin-sha256: values, abbreviated to at least four characters.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Разрешенные отпечатки сертификата сервера, по одному на строку. Принимается SHA-1 отпечаток без префикса или значения sha1:, sha256: и pin-sha256:, сокращенные не менее чем до четырех символов.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Peer Fingerprint</source>
        <translation>Отпечаток сервера</translation>
    </message>
    <message>
        <source>Not set</source>
        <translation>Не задано</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Use a CA certificate or a peer fingerprint to establish trust when this is enabled.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;При включении для установления доверия используйте сертификат CA или отпечаток сервера.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable the system CA certificate pool</source>
        <translation>Не использовать системное хранилище сертификатов CA</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline CA certificate on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Несовместимо со встроенным сертификатом CA в основном редакторе.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CA Certificate Path</source>
        <translation>Путь к сертификату CA</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline client certificate on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Несовместимо со встроенным сертификатом клиента в основном редакторе.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Client Certificate Path</source>
        <translation>Путь к сертификату клиента</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline client key on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Несовместимо со встроенным ключом клиента в основном редакторе.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Client Key Path</source>
        <translation>Путь к ключу клиента</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AnyConnect multiple-certificate authentication chain in PEM format. Conflicts with the MCA certificate path.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Цепочка сертификатов для аутентификации AnyConnect с несколькими сертификатами (MCA) в формате PEM. Несовместимо с путем к сертификату MCA.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MCA Certificate</source>
        <translation>Сертификат MCA</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline MCA certificate.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Несовместимо со встроенным сертификатом MCA.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MCA Certificate Path</source>
        <translation>Путь к сертификату MCA</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AnyConnect multiple-certificate authentication private key in PEM format. Conflicts with the MCA key path.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Закрытый ключ для аутентификации AnyConnect с несколькими сертификатами (MCA) в формате PEM. Несовместимо с путем к ключу MCA.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MCA Key</source>
        <translation>Ключ MCA</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline MCA key.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Несовместимо со встроенным ключом MCA.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MCA Key Path</source>
        <translation>Путь к ключу MCA</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Password for the encrypted MCA private key.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Пароль для зашифрованного закрытого ключа MCA.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MCA Key Password</source>
        <translation>Пароль ключа MCA</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;All three fields are required when a mobile identity is configured.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Если задана идентификация мобильного устройства, все три поля обязательны.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Mobile Identity</source>
        <translation>Идентификация мобильного устройства</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Mobile operating system version reported to the AnyConnect server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Версия мобильной ОС, сообщаемая серверу AnyConnect.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Platform Version</source>
        <translation>Версия платформы</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Mobile device model or type reported to the AnyConnect server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Модель или тип мобильного устройства, сообщаемые серверу AnyConnect.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Device Type</source>
        <translation>Тип устройства</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Mobile device identifier reported to the AnyConnect server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Идентификатор мобильного устройства, сообщаемый серверу AnyConnect.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Device Unique ID</source>
        <translation>Уникальный ID устройства</translation>
    </message>
    <message>
        <source>Compliance</source>
        <translation>Проверка соответствия</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;External AnyConnect CSD / host scan wrapper executable. Built-in handling is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Внешний исполняемый файл-обертка AnyConnect CSD / host scan. Если пусто, используется встроенная обработка.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CSD Wrapper Path</source>
        <translation>Путь к обертке CSD</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;External GlobalProtect HIP report wrapper executable. Built-in reporting is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Внешний исполняемый файл-обертка для отчетов HIP GlobalProtect. Если пусто, используются встроенные отчеты.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>HIP Wrapper Path</source>
        <translation>Путь к обертке HIP</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fortinet hostcheck result string, conventionally security-status,os-version such as 0100,10.0.19042. An empty value disables hostcheck.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Строка результата hostcheck Fortinet, обычно в виде security-status,os-version, например 0100,10.0.19042. Пустое значение отключает hostcheck.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Fortinet Hostcheck</source>
        <translation>Hostcheck Fortinet</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fortinet virtual desktop check result, conventionally colon separated MAC addresses joined by a vertical bar.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Результат проверки виртуального рабочего стола Fortinet: обычно MAC-адреса через двоеточие, объединенные вертикальной чертой.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Fortinet Virtual Desktop</source>
        <translation>Виртуальный рабочий стол Fortinet</translation>
    </message>
    <message>
        <source>TNCC</source>
        <translation>TNCC</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;External Network Connect TNCC wrapper executable. Conflicts with every other field in this group.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Внешний исполняемый файл-обертка TNCC для Network Connect. Несовместимо со всеми остальными полями этой группы.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Wrapper Path</source>
        <translation>Путь к обертке</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Device ID reported by the built-in TNCC handler.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;ID устройства, сообщаемый встроенным обработчиком TNCC.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;User agent used by the built-in TNCC handler. Neoteris HC Http is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;User agent встроенного обработчика TNCC. По умолчанию используется Neoteris HC Http.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Reports the platform, hostname and observed MAC addresses. Required before machine certificates are used.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Сообщает платформу, имя хоста и обнаруженные MAC-адреса. Требуется для использования сертификатов компьютера.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Enable machine identification</source>
        <translation>Включить идентификацию компьютера</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Machine certificates used by the built-in TNCC handler to answer certificate requests. Requires machine identification.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Сертификаты компьютера, которыми встроенный обработчик TNCC отвечает на запросы сертификатов. Требует идентификации компьютера.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Certificate</source>
        <translation>Сертификат</translation>
    </message>
    <message>
        <source>Certificate Path</source>
        <translation>Путь к сертификату</translation>
    </message>
    <message>
        <source>Add</source>
        <translation>Добавить</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Edit Certificate</source>
        <translation>Редактировать сертификат</translation>
    </message>
    <message>
        <source>Authentication Forms</source>
        <translation>Формы аутентификации</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;An entry matches by submission key when set, or by the combination of form ID and name. Later matching entries take precedence. Ask interactively conflicts with a value.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Запись сопоставляется по ключу отправки, если он задан, иначе по сочетанию ID формы и имени поля. Более поздние подходящие записи имеют приоритет. «Спрашивать» несовместимо с заданным значением.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Form ID</source>
        <translation>ID формы</translation>
    </message>
    <message>
        <source>Submission Key</source>
        <translation>Ключ отправки</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Название</translation>
    </message>
    <message>
        <source>Value</source>
        <translation>Значение</translation>
    </message>
    <message>
        <source>Ask interactively</source>
        <translation>Спрашивать</translation>
    </message>
    <message>
        <source>Transport</source>
        <translation>Транспорт</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Uses the TLS data channel only.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Использует только канал данных TLS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable the DTLS or ESP data channel</source>
        <translation>Отключить канал данных DTLS или ESP</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Local UDP port used by the secondary data channel. An ephemeral port is selected by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Локальный UDP-порт дополнительного канала данных. По умолчанию выбирается временный порт.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>DTLS Local Port</source>
        <translation>Локальный порт DTLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Compression can weaken traffic confidentiality. Conflicts with the compression mode all.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Сжатие может ослабить конфиденциальность трафика. Несовместимо с режимом сжатия all.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable compression negotiation</source>
        <translation>Отключить согласование сжатия</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;stateless is used by default. all additionally advertises stateful deflate for CSTP and carries extra confidentiality risk.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;По умолчанию используется stateless. all дополнительно объявляет stateful deflate для CSTP и несет дополнительный риск для конфиденциальности.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Compression Mode</source>
        <translation>Режим сжатия</translation>
    </message>
    <message>
        <source>Disable IPv6 tunnel configuration</source>
        <translation>Отключить настройку IPv6 в туннеле</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Applies to authentication and configuration requests.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Применяется к запросам аутентификации и конфигурации.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable HTTP connection reuse</source>
        <translation>Отключить повторное использование HTTP-соединений</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Starts AnyConnect authentication with the legacy GET flow.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Начинает аутентификацию AnyConnect по устаревшей схеме GET.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable XML POST authentication</source>
        <translation>Отключить аутентификацию XML POST</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;External authentication is not advertised and any unexpected SSO or SAML request is rejected.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Внешняя аутентификация не объявляется, а любой неожиданный запрос SSO или SAML отклоняется.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable external browser authentication</source>
        <translation>Отключить аутентификацию во внешнем браузере</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Matches OpenConnect --no-passwd. Does not affect other flavors or a session supplied by a cookie.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Соответствует OpenConnect --no-passwd. Не влияет на другие протоколы и на сессию, заданную через cookie.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Abort on a non-success authentication form</source>
        <translation>Прерывать при любой форме аутентификации, кроме успешной</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Disabled by default to match OpenConnect.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;По умолчанию отключено, как в OpenConnect.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Enable TCP keep alive</source>
        <translation>Включить TCP keep-alive</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;For TLS 1.2 and earlier. Disabled by default for servers that require RSA key exchange.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Для TLS 1.2 и более ранних версий. По умолчанию отключено ради серверов, требующих обмена ключами RSA.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Require forward-secret cipher suites</source>
        <translation>Требовать наборы шифров с прямой секретностью</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Enables weak cipher suites and TLS 1.0 compatibility. It does not disable server certificate verification.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Включает слабые наборы шифров и совместимость с TLS 1.0. Проверку сертификата сервера это не отключает.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Allow legacy TLS and DTLS crypto</source>
        <translation>Разрешить устаревшую криптографию TLS и DTLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Base path MTU used to calculate the tunnel MTU after outer IP, transport and protocol overhead. Values below 1280 are treated as 1280.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Базовый MTU пути, из которого вычисляется MTU туннеля за вычетом накладных расходов внешнего IP, транспорта и протокола. Значения меньше 1280 считаются равными 1280.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Base MTU</source>
        <translation>Базовый MTU</translation>
    </message>
    <message>
        <source>1406 (default)</source>
        <translation>1406 (по умолчанию)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Overrides the dead peer detection interval. Positive values below 2s are treated as 2s.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Переопределяет интервал обнаружения недоступного пира (DPD). Положительные значения меньше 2s считаются равными 2s.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>DPD Interval</source>
        <translation>Интервал DPD</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Maximum accumulated backoff time after failed reconnect attempts.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Максимальное суммарное время отсрочки после неудачных попыток переподключения.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Reconnect Timeout</source>
        <translation>Таймаут переподключения</translation>
    </message>
    <message>
        <source>300s (default)</source>
        <translation>300s (по умолчанию)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Overrides the interval between GlobalProtect HIP reports or Network Connect TNCC checks.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Переопределяет интервал между отчетами HIP GlobalProtect или проверками TNCC Network Connect.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Trojan Interval</source>
        <translation>Интервал trojan</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Inbound and outbound packet queue length between the VPN transport and the tunnel interface.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Длина очереди входящих и исходящих пакетов между транспортом VPN и интерфейсом туннеля.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Queue Length</source>
        <translation>Длина очереди</translation>
    </message>
    <message>
        <source>32 (default)</source>
        <translation>32 (по умолчанию)</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>Уже задано</translation>
    </message>
    <message>
        <source>TNCC Machine Certificate</source>
        <translation>Сертификат компьютера TNCC</translation>
    </message>
</context>
<context>
    <name>EditOpenVPN</name>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Default transport network. Leave empty to use the core default (udp).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Транспортная сеть по умолчанию. Оставьте пустым, чтобы использовать значение ядра по умолчанию (udp).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>Сеть</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>Имя пользователя</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Static challenge text shown by the server when it asks for a second authentication response. When a challenge is set and an OTP profile is bound, Throne sends the answer itself and no interactive prompt appears.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Текст статического запроса, который сервер показывает, когда запрашивает второй ответ для аутентификации. Если запрос задан и привязан OTP-профиль, Throne отправляет ответ сам и окно запроса не появляется.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Static Challenge</source>
        <translation>Статический запрос</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Show the static challenge response as plain text.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Показывать ответ на статический запрос открытым текстом.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Echo challenge response</source>
        <translation>Показывать ответ на запрос</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Tunnel MTU. Empty uses 1500 until the server pushes a value.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;MTU туннеля. Если пусто, используется 1500, пока сервер не передаст свое значение.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>OTP</source>
        <translation>OTP</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Traffic that falls through to this profile reaches the tunnel only when the destination is inside a subnet the VPN server advertises; anything else is blocked. Routing rules that name this profile explicitly are not affected and always win.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Трафик, попадающий в этот профиль по умолчанию, идет в туннель, только если адрес назначения входит в подсеть, объявленную VPN-сервером; остальной трафик блокируется. Правила маршрутизации, явно указывающие этот профиль, не затрагиваются и всегда имеют приоритет.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Only route advertised network</source>
        <translation>Только объявленные сервером сети</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;How DNS uses the resolvers the VPN server pushes when this profile is started.&lt;/p&gt;&lt;p&gt;None: ignore them. Use this if the server claims every domain and breaks public DNS.&lt;br/&gt;Prefer: resolve the names the server claims - its split-DNS suffixes and search domains - through them, and everything else as usual.&lt;br/&gt;Strict: also send every remote DNS query to them instead of the remote DNS server, and fail the lookups they cannot answer. Direct lookups are unchanged.&lt;/p&gt;&lt;p&gt;As an endpoint in a routing profile, Prefer and Strict both resolve only the names the server claims.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Как DNS использует резолверы, которые VPN-сервер передает при запуске этого профиля.&lt;/p&gt;&lt;p&gt;Нет: игнорировать их. Выберите, если сервер объявляет все домены и ломает публичный DNS.&lt;br/&gt;Предпочитать: разрешать через них имена, объявленные сервером (его суффиксы split-DNS и домены поиска), а все остальное — как обычно.&lt;br/&gt;Строго: также отправлять им все удаленные DNS-запросы вместо удаленного DNS-сервера, а запросы, на которые они не могут ответить, завершать ошибкой. Прямые запросы не меняются.&lt;/p&gt;&lt;p&gt;Если профиль используется как конечная точка в профиле маршрутизации, в режимах «Предпочитать» и «Строго» через них разрешаются только имена, объявленные сервером.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Tunnel DNS</source>
        <translation>DNS туннеля</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>Дополнительно</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Trusted CA certificate content (PEM).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Содержимое доверенного сертификата CA (PEM).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CA Certificate</source>
        <translation>Сертификат CA</translation>
    </message>
    <message>
        <source>Not set</source>
        <translation>Не задано</translation>
    </message>
    <message>
        <source>Client Certificate</source>
        <translation>Сертификат клиента</translation>
    </message>
    <message>
        <source>Client Key</source>
        <translation>Ключ клиента</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN tls-auth / tls-crypt / tls-crypt-v2 control channel wrapping. Disabled when the type is empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Защита управляющего канала OpenVPN через tls-auth / tls-crypt / tls-crypt-v2. Отключена, если тип не выбран.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Control Channel Wrap</source>
        <translation>Защита управляющего канала</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>Тип</translation>
    </message>
    <message>
        <source>Key</source>
        <translation>Ключ</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;tls-auth key direction. Only used when the type is tls_auth; the key is bidirectional when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Направление ключа tls-auth. Используется, только если выбран тип tls_auth; если пусто, ключ двунаправленный.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Direction</source>
        <translation>Направление</translation>
    </message>
    <message>
        <source>Prefer</source>
        <translation>Предпочитать</translation>
    </message>
    <message>
        <source>Strict</source>
        <translation>Строго</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Binds an authenticator entry to this profile. %1 in the username or password is replaced with a generated code at connect time, and a static challenge is answered with the same code.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Привязывает к этому профилю запись аутентификатора. При подключении %1 в имени пользователя или пароле заменяется сгенерированным кодом, и тот же код отправляется в ответ на статический запрос.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>%1 is replaced at connect time</source>
        <translation>%1 заменяется при подключении</translation>
    </message>
    <message>
        <source>Control Channel Wrap Key</source>
        <translation>Ключ защиты управляющего канала</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>Уже задано</translation>
    </message>
    <message>
        <source>None</source>
        <translation>Нет</translation>
    </message>
</context>
<context>
    <name>EditOpenVPNAdvanced</name>
    <message>
        <source>OpenVPN Advanced</source>
        <translation>Доп. настройки OpenVPN</translation>
    </message>
    <message>
        <source>Session</source>
        <translation>Сессия</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN session mode. tls is used by default; static_key is a deprecated mode without a TLS control channel.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Режим сессии OpenVPN. По умолчанию используется tls; static_key — устаревший режим без управляющего канала TLS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>Режим</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Tunnel topology. The topology pulled from the server is used when empty in TLS mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Топология туннеля. Если пусто, в режиме TLS используется топология, полученная от сервера.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Topology</source>
        <translation>Топология</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Behavior after username/password authentication fails. none is used by default and treats a permanent failure as terminal.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Поведение после неудачной аутентификации по имени пользователя и паролю. По умолчанию используется none: постоянный отказ считается окончательным.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Auth Retry</source>
        <translation>Повтор аутентификации</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Randomize the server list before connecting.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Перемешать список серверов перед подключением.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Randomize server order</source>
        <translation>Случайный порядок серверов</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Comma separated local tunnel prefixes. At least one is required in static_key mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Локальные префиксы туннеля через запятую. В режиме static_key требуется хотя бы один.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Local Address</source>
        <translation>Локальный адрес</translation>
    </message>
    <message>
        <source>10.8.0.2/24, fd00::2/64</source>
        <translation>10.8.0.2/24, fd00::2/64</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;IPv4 tunnel peer address and VPN gateway. Required with an IPv4 local address in static_key mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;IPv4-адрес пира в туннеле и шлюз VPN. Обязателен при локальном адресе IPv4 в режиме static_key.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Peer Address</source>
        <translation>Адрес пира</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;IPv6 tunnel peer address and VPN gateway. Required with an IPv6 local address in static_key mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;IPv6-адрес пира в туннеле и шлюз VPN. Обязателен при локальном адресе IPv6 в режиме static_key.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Peer Address (IPv6)</source>
        <translation>Адрес пира (IPv6)</translation>
    </message>
    <message>
        <source>Servers</source>
        <translation>Серверы</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Servers are tried in order and the client moves to the next one when a connection fails. Leave empty to use the address and port from the profile.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Серверы перебираются по порядку: если подключение не удалось, клиент переходит к следующему. Оставьте пустым, чтобы использовать адрес и порт из профиля.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Server</source>
        <translation>Сервер</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>Порт</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>Сеть</translation>
    </message>
    <message>
        <source>Add</source>
        <translation>Добавить</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Static Key Mode</source>
        <translation>Режим статического ключа</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Static key content. Required in static_key mode. Conflicts with the static key path.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Содержимое статического ключа. Обязательно в режиме static_key. Несовместимо с путем к статическому ключу.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Static Key</source>
        <translation>Статический ключ</translation>
    </message>
    <message>
        <source>Not set</source>
        <translation>Не задано</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline static key.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Несовместимо со встроенным статическим ключом.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Static Key Path</source>
        <translation>Путь к статическому ключу</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Static key direction. The key is used bidirectionally when empty. Only available in static_key mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Направление статического ключа. Если пусто, ключ используется в обоих направлениях. Доступно только в режиме static_key.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Key Direction</source>
        <translation>Направление ключа</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Data channel cipher used in static_key mode. The upstream default BF-CBC is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Шифр канала данных в режиме static_key. Если пусто, используется стандартный BF-CBC.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Data Cipher</source>
        <translation>Шифр данных</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Expected server certificate name. Name verification is disabled when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Ожидаемое имя в сертификате сервера. Если пусто, проверка имени отключена.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Server Name</source>
        <translation>Server Name</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Certificate field matched by the server name. name is used by default and matches the common name exactly.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Поле сертификата, с которым сравнивается «Server Name». По умолчанию используется name — точное совпадение с common name.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Server Name Type</source>
        <translation>Server Name Type</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline CA certificate on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Несовместимо со встроенным сертификатом CA в основном редакторе.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CA Certificate Path</source>
        <translation>Путь к сертификату CA</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline client certificate on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Несовместимо со встроенным сертификатом клиента в основном редакторе.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Client Certificate Path</source>
        <translation>Путь к сертификату клиента</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline client key on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Несовместимо со встроенным ключом клиента в основном редакторе.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Client Key Path</source>
        <translation>Путь к ключу клиента</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with the inline control channel wrap key on the main editor.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Несовместимо со встроенным ключом защиты управляющего канала в основном редакторе.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Control Wrap Key Path</source>
        <translation>Путь к ключу защиты упр. канала</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Allowed SHA-256 fingerprints of the server leaf certificate, one per line, 64 lowercase hexadecimal characters without separators.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Разрешенные отпечатки SHA-256 конечного сертификата сервера, по одному на строку: 64 шестнадцатеричных символа в нижнем регистре без разделителей.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Peer Fingerprint</source>
        <translation>Отпечаток сервера</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;PEM or DER certificate revocation list used to reject revoked server certificates.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Список отзыва сертификатов (CRL) в формате PEM или DER для отклонения отозванных сертификатов сервера.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>CRL Path</source>
        <translation>Путь к CRL</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Comma separated hexadecimal key usage masks in OpenVPN remote-cert-ku format.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Шестнадцатеричные маски key usage через запятую в формате OpenVPN remote-cert-ku.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Remote Certificate KU</source>
        <translation>KU сертификата сервера</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Required extended key usage. Conflicts with an explicit remote certificate TLS check.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Требуемое расширенное назначение ключа (EKU). Несовместимо с явно заданным параметром «Назначение сертификата (remote-cert-tls)».&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Remote Certificate EKU</source>
        <translation>EKU сертификата сервера</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Peer certificate purpose check. server is used by default; none disables the check.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Проверка назначения сертификата сервера. По умолчанию используется server; none отключает проверку.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Remote Certificate TLS</source>
        <translation>Назначение сертификата (remote-cert-tls)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;legacy is used by default. insecure accepts MD5 and SHA-1 signed chains; preferred requires stronger signatures and keys.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;По умолчанию используется legacy. insecure принимает цепочки с подписями MD5 и SHA-1; preferred требует более стойких подписей и ключей.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Certificate Profile</source>
        <translation>Профиль сертификатов</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Deprecated Netscape certificate type check. Prefer Remote Certificate TLS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Устаревшая проверка типа сертификата Netscape. Лучше используйте «Назначение сертификата (remote-cert-tls)».&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>NS Certificate Type</source>
        <translation>Тип сертификата NS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;1.2 is used by default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;По умолчанию используется 1.2.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Min TLS Version</source>
        <translation>Мин. версия TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;The maximum supported version is used by default. It cannot be lower than the minimum.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;По умолчанию используется максимальная поддерживаемая версия. Она не может быть ниже минимальной.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Max TLS Version</source>
        <translation>Макс. версия TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Colon separated OpenSSL cipher suite names allowed for TLS 1.2 and earlier.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Имена наборов шифров OpenSSL через двоеточие, разрешенные для TLS 1.2 и более ранних версий.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>TLS Cipher</source>
        <translation>Шифр TLS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Colon separated key exchange groups in preference order: X25519, SECP256R1, SECP384R1, SECP521R1.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Группы обмена ключами через двоеточие в порядке предпочтения: X25519, SECP256R1, SECP384R1, SECP521R1.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>TLS Groups</source>
        <translation>Группы TLS</translation>
    </message>
    <message>
        <source>Data Channel</source>
        <translation>Канал данных</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Comma separated allowed data channel ciphers. Only available in TLS mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Разрешенные шифры канала данных через запятую. Доступно только в режиме TLS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Data Ciphers</source>
        <translation>Шифры данных</translation>
    </message>
    <message>
        <source>AES-256-GCM,AES-128-GCM,CHACHA20-POLY1305</source>
        <translation>AES-256-GCM,AES-128-GCM,CHACHA20-POLY1305</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Data channel cipher for peers that do not support cipher negotiation.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Шифр канала данных для пиров, не поддерживающих согласование шифров.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Data Ciphers Fallback</source>
        <translation>Резервный шифр данных</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Data channel authentication digest. SHA1 is used by default and only applies to non-AEAD ciphers and tls_auth.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Дайджест аутентификации канала данных. По умолчанию используется SHA1; применяется только к шифрам без AEAD и к tls_auth.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Auth Digest</source>
        <translation>Дайджест аутентификации</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Maximum UDP packet size used to clamp the MSS of TCP connections sent through the tunnel.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Максимальный размер UDP-пакета, по которому ограничивается MSS TCP-соединений, идущих через туннель.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MSS Fix</source>
        <translation>MSS Fix</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Disables MSS clamping including the default clamp. Conflicts with MSS Fix and MSS Fix Mode.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Отключает ограничение MSS, включая ограничение по умолчанию. Несовместимо с MSS Fix и режимом MSS Fix.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable MSS clamping</source>
        <translation>Отключить ограничение MSS</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;mtu also accounts for the outer IP and transport headers; fixed treats MSS Fix as an inner IPv4 packet size. Requires MSS Fix.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;mtu также учитывает внешние заголовки IP и транспорта; fixed считает MSS Fix размером внутреннего IPv4-пакета. Требует MSS Fix.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>MSS Fix Mode</source>
        <translation>Режим MSS Fix</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Maximum UDP packet size used for data channel fragmentation. A non-zero value must be at least 68. Conflicts with TCP transport.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Максимальный размер UDP-пакета для фрагментации канала данных. Ненулевое значение должно быть не меньше 68. Несовместимо с транспортом TCP.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Fragment</source>
        <translation>Фрагментация</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP replay window size. 64 is used by default and the maximum is 65536.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Размер окна защиты от повтора для UDP. По умолчанию 64, максимум — 65536.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Replay Window</source>
        <translation>Окно защиты от повтора</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;UDP replay window duration in whole seconds. The maximum is 10m.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Длительность окна защиты от повтора для UDP в целых секундах. Максимум — 10m.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Replay Window Time</source>
        <translation>Время окна защиты от повтора</translation>
    </message>
    <message>
        <source>15s (default)</source>
        <translation>15s (по умолчанию)</translation>
    </message>
    <message>
        <source>Compression</source>
        <translation>Сжатие</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN compress framing mode. Disabled by default. Compression can weaken traffic confidentiality.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Режим кадрирования compress в OpenVPN. По умолчанию отключено. Сжатие может ослабить конфиденциальность трафика.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;OpenVPN comp-lzo mode. Disabled by default. Enable it only when required by the server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Режим comp-lzo в OpenVPN. По умолчанию отключено. Включайте, только если этого требует сервер.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Compression LZO</source>
        <translation>Сжатие LZO</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Policy for compression pushed by the server. no is used by default and permits only stub framing.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Политика для сжатия, передаваемого сервером. По умолчанию используется no: разрешено только кадрирование stub.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Allow Compression</source>
        <translation>Разрешить сжатие</translation>
    </message>
    <message>
        <source>Routing</source>
        <translation>Маршрутизация</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Ignores routes, DNS, route metrics, redirect-gateway, redirect-private and block-ipv6 pushed by the server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Игнорирует маршруты, DNS, метрики маршрутов, redirect-gateway, redirect-private и block-ipv6, передаваемые сервером.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Ignore pushed routes, DNS and gateway options</source>
        <translation>Игнорировать маршруты, DNS и параметры шлюза от сервера</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Comma separated prefixes preferred for this endpoint, in addition to routes accepted from the server.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Префиксы через запятую, предпочитаемые для этой конечной точки, в дополнение к маршрутам, принятым от сервера.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Routes</source>
        <translation>Маршруты</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;IPv4 gateway for routes through the endpoint. The gateway received from the server is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;IPv4-шлюз для маршрутов через конечную точку. Если пусто, используется шлюз, полученный от сервера.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Route Gateway</source>
        <translation>Шлюз маршрутов</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Default metric for routes through the endpoint. The platform default is used when empty.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Метрика по умолчанию для маршрутов через конечную точку. Если пусто, используется значение платформы по умолчанию.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Route Metric</source>
        <translation>Метрика маршрутов</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Does not install an operating system default route.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Не устанавливает маршрут по умолчанию в операционной системе.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Prefer this endpoint for all IPv4 destinations</source>
        <translation>Предпочитать эту конечную точку для всех адресов IPv4</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Comma separated OpenVPN redirect-gateway flags, for example !ipv4, def1 or ipv6.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Флаги OpenVPN redirect-gateway через запятую, например !ipv4, def1 или ipv6.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Redirect Gateway Flags</source>
        <translation>Флаги redirect-gateway</translation>
    </message>
    <message>
        <source>Accept redirect gateway flags without a default route preference</source>
        <translation>Принимать флаги redirect-gateway без предпочтения маршрута по умолчанию</translation>
    </message>
    <message>
        <source>Reject IPv6 traffic locally instead of sending it through the VPN</source>
        <translation>Отклонять IPv6-трафик локально, а не отправлять его через VPN</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Ordered filters for options pushed by the server. The first filter whose text is a case-sensitive prefix of the complete pushed option is applied.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Упорядоченные фильтры параметров, передаваемых сервером. Применяется первый фильтр, текст которого является началом полного переданного параметра (с учетом регистра).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Action</source>
        <translation>Действие</translation>
    </message>
    <message>
        <source>Text</source>
        <translation>Текст</translation>
    </message>
    <message>
        <source>Timers</source>
        <translation>Таймеры</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Whole seconds of silence before a data channel ping is sent. A server-pushed value overrides it.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Сколько целых секунд тишины должно пройти перед отправкой ping по каналу данных. Значение, переданное сервером, имеет приоритет.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Ping Interval</source>
        <translation>Интервал ping</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Whole seconds without a received packet before reconnecting. 120s is used for UDP with pull enabled.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Сколько целых секунд без входящих пакетов должно пройти перед переподключением. Для UDP с включенным pull используется 120s.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Ping Restart</source>
        <translation>Перезапуск по ping</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Disables the initial 120s UDP pull timeout and any configured ping restart. Conflicts with Ping Restart.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Отключает начальный 120-секундный таймаут pull для UDP и любой заданный перезапуск по ping. Несовместимо с параметром «Перезапуск по ping».&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable ping restart</source>
        <translation>Отключить перезапуск по ping</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;TLS renegotiation interval.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Интервал пересогласования TLS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Renegotiate Interval</source>
        <translation>Интервал пересогласования</translation>
    </message>
    <message>
        <source>1h (default)</source>
        <translation>1h (по умолчанию)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Conflicts with Renegotiate Interval.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Несовместимо с параметром «Интервал пересогласования».&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable time based renegotiation</source>
        <translation>Отключить пересогласование по времени</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Renegotiate data channel keys after this many bytes. Empty uses the cipher-dependent default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Пересогласовывать ключи канала данных после передачи указанного числа байтов. Если пусто, используется значение по умолчанию, зависящее от шифра.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Renegotiate Bytes</source>
        <translation>Байтов до пересогласования</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Renegotiate data channel keys after this many packets. Empty uses the cipher-dependent default.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Пересогласовывать ключи канала данных после передачи указанного числа пакетов. Если пусто, используется значение по умолчанию, зависящее от шифра.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Renegotiate Packets</source>
        <translation>Пакетов до пересогласования</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Initial retransmission timeout for TLS control packets.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Начальный таймаут повторной передачи управляющих пакетов TLS.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>TLS Timeout</source>
        <translation>Таймаут TLS</translation>
    </message>
    <message>
        <source>2s (default)</source>
        <translation>2s (по умолчанию)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Maximum time allowed for the initial handshake and each renegotiation.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Максимальное время на начальный хендшейк и каждое пересогласование.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Handshake Window</source>
        <translation>Окно хендшейка</translation>
    </message>
    <message>
        <source>1m (default)</source>
        <translation>1m (по умолчанию)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Number of exit notifications sent one second apart when closing a UDP connection.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Сколько уведомлений о выходе отправлять с интервалом в одну секунду при закрытии UDP-соединения.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Explicit Exit Notify</source>
        <translation>Уведомления о выходе</translation>
    </message>
    <message>
        <source>Already set</source>
        <translation>Уже задано</translation>
    </message>
</context>
<context>
    <name>EditSSH</name>
    <message>
        <source>User</source>
        <translation>Пользователь</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>Private Key</source>
        <translation>Private Key</translation>
    </message>
    <message>
        <source>Private Key Password</source>
        <translation>Пароль к Private Key</translation>
    </message>
    <message>
        <source>Host Key</source>
        <translation>Host Key</translation>
    </message>
    <message>
        <source>Host Key Algorithms</source>
        <translation>Host Key Algorithms</translation>
    </message>
    <message>
        <source>Client Version</source>
        <translation>Версия клиента</translation>
    </message>
    <message>
        <source>Private Key Path</source>
        <translation>Путь к Private Key</translation>
    </message>
    <message>
        <source>Choose File</source>
        <translation>Выбрать файл</translation>
    </message>
</context>
<context>
    <name>EditShadowSocks</name>
    <message>
        <source>Encryption</source>
        <translation>Шифрование</translation>
    </message>
    <message>
        <source>Plugin</source>
        <translation>Плагин</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>UDP over TCP</source>
        <translation>UDP over TCP</translation>
    </message>
    <message>
        <source>Plugin Args</source>
        <translation>Аргументы плагина</translation>
    </message>
</context>
<context>
    <name>EditShadowTLS</name>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>Version</source>
        <translation>Версия</translation>
    </message>
</context>
<context>
    <name>EditSnell</name>
    <message>
        <source>Version</source>
        <translation>Версия</translation>
    </message>
    <message>
        <source>PSK</source>
        <translation>PSK</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Per-user key on a multi-user server. Leave empty when the server only has a PSK.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Ключ пользователя на многопользовательском сервере. Оставьте пустым, если на сервере задан только PSK.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>User Key</source>
        <translation>User Key</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Restrict this outbound to one network. Leave empty to carry both TCP and UDP.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Ограничить этот outbound одной сетью. Оставьте пустым, чтобы передавать и TCP, и UDP.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>Сеть</translation>
    </message>
    <message>
        <source>Obfuscation</source>
        <translation>Obfuscation</translation>
    </message>
    <message>
        <source>Obfuscation Host</source>
        <translation>Obfuscation Host</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Traffic shaping used by Snell v6. &quot;unsafe-raw&quot; disables padding and is faster but easier to fingerprint.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Формирование трафика в Snell v6. «unsafe-raw» отключает заполнение (padding): так быстрее, но трафик легче распознать по отпечатку.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Mode</source>
        <translation>Режим</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Keep idle connections to the server open and reuse them for later requests.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Держать простаивающие соединения с сервером открытыми и повторно использовать их для последующих запросов.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Reuse connections</source>
        <translation>Переиспользовать соединения</translation>
    </message>
</context>
<context>
    <name>EditSocks</name>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>Version</source>
        <translation>Версия</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>Имя пользователя</translation>
    </message>
    <message>
        <source>5</source>
        <translation>5</translation>
    </message>
    <message>
        <source>4</source>
        <translation>4</translation>
    </message>
</context>
<context>
    <name>EditTailScale</name>
    <message>
        <source>State directory</source>
        <translation>Директория состояния</translation>
    </message>
    <message>
        <source>Auth key</source>
        <translation>Auth Key</translation>
    </message>
    <message>
        <source>Control URL</source>
        <translation>Control URL</translation>
    </message>
    <message>
        <source>Hostname</source>
        <translation>Имя хоста</translation>
    </message>
    <message>
        <source>Accept routes</source>
        <translation>Принимать маршруты</translation>
    </message>
    <message>
        <source>Ephemeral</source>
        <translation>Эфемерный узел</translation>
    </message>
    <message>
        <source>Exit node allow lan access</source>
        <translation>Выходной узел: разрешить доступ к LAN</translation>
    </message>
    <message>
        <source>Advertise exit node</source>
        <translation>Объявлять выходной узел</translation>
    </message>
    <message>
        <source>Exit node</source>
        <translation>Выходной узел</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;comma seperated list of subnets&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Список подсетей, разделенных запятыми&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Advertise routes</source>
        <translation>Объявлять маршруты</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;use tailscale dns as the remote dns&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Использовать DNS Tailscale в качестве удалённого DNS&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Global DNS</source>
        <translation>Глобальный DNS</translation>
    </message>
</context>
<context>
    <name>EditTrojan</name>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
</context>
<context>
    <name>EditTrustTunnel</name>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>Custom SNI</source>
        <translation>Пользовательский SNI</translation>
    </message>
    <message>
        <source>Server name sent in the TLS ClientHello instead of the TLS server name, which is still used to verify the certificate; not compatible with ECH or REALITY</source>
        <translation>Имя сервера, отправляемое в TLS ClientHello вместо имени сервера TLS (оно по-прежнему используется для проверки сертификата); несовместимо с ECH и REALITY</translation>
    </message>
    <message>
        <source>optional, e.g. cdn.example.net</source>
        <translation>необязательно, напр. cdn.example.net</translation>
    </message>
    <message>
        <source>Client Random</source>
        <translation>Client Random</translation>
    </message>
    <message>
        <source>TLS ClientHello random prefix required by the server&apos;s client_random_prefix rule; not compatible with ECH or REALITY</source>
        <translation>Префикс поля random в TLS ClientHello, которого требует правило client_random_prefix на сервере; несовместимо с ECH и REALITY</translation>
    </message>
    <message>
        <source>prefix[/mask] (hex), e.g. a0b0/f0f0</source>
        <translation>префикс[/маска] (hex), напр. a0b0/f0f0</translation>
    </message>
    <message>
        <source>Health Check</source>
        <translation>Проверка доступности (Health Check)</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>Имя пользователя</translation>
    </message>
    <message>
        <source>bbr</source>
        <translation>bbr</translation>
    </message>
    <message>
        <source>cubic</source>
        <translation>cubic</translation>
    </message>
    <message>
        <source>reno</source>
        <translation>reno</translation>
    </message>
    <message>
        <source>bbr_standard</source>
        <translation>bbr_standard</translation>
    </message>
    <message>
        <source>bbr2</source>
        <translation>bbr2</translation>
    </message>
    <message>
        <source>bbr_variant</source>
        <translation>bbr_variant</translation>
    </message>
    <message>
        <source>Congestion Control</source>
        <translation>Congestion Control</translation>
    </message>
    <message>
        <source>QUIC</source>
        <translation>QUIC</translation>
    </message>
</context>
<context>
    <name>EditTuic</name>
    <message>
        <source>Heartbeat</source>
        <translation>Heartbeat</translation>
    </message>
    <message>
        <source>UDP Relay Mode</source>
        <translation>UDP Relay Mode</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>Пароль</translation>
    </message>
    <message>
        <source>Zero RTT Handshake</source>
        <translation>Zero RTT Handshake</translation>
    </message>
    <message>
        <source>UUID</source>
        <translation>UUID</translation>
    </message>
    <message>
        <source>bbr</source>
        <translation>bbr</translation>
    </message>
    <message>
        <source>cubic</source>
        <translation>cubic</translation>
    </message>
    <message>
        <source>new_reno</source>
        <translation>new_reno</translation>
    </message>
    <message>
        <source>Congestion Control</source>
        <translation>Congestion Control</translation>
    </message>
    <message>
        <source>native</source>
        <translation>native</translation>
    </message>
    <message>
        <source>quic</source>
        <translation>quic</translation>
    </message>
    <message>
        <source>UDP over Stream</source>
        <translation>UDP over Stream</translation>
    </message>
</context>
<context>
    <name>EditVMess</name>
    <message>
        <source>Security</source>
        <translation>Шифрование</translation>
    </message>
    <message>
        <source>Alter Id</source>
        <translation>Alter ID</translation>
    </message>
    <message>
        <source>UUID</source>
        <translation>UUID</translation>
    </message>
    <message>
        <source>Packet Encoding</source>
        <translation>Packet Encoding</translation>
    </message>
    <message>
        <source>Generate UUID</source>
        <translation>Сгенерировать UUID</translation>
    </message>
</context>
<context>
    <name>EditVless</name>
    <message>
        <source>UUID</source>
        <translation>UUID</translation>
    </message>
    <message>
        <source>Packet Encoding</source>
        <translation>Packet Encoding</translation>
    </message>
</context>
<context>
    <name>EditWireguard</name>
    <message>
        <source>Reserved</source>
        <translation>Reserved</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;persistent_keepalive_interval in seconds, or an AmneziaWG range such as 22-30&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Интервал отправки Persistent Keepalive (поддержания активности) в секундах или диапазон AmneziaWG, например 22-30&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;comma seperated list of subnets&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Список подсетей, разделенных запятыми&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Local Address</source>
        <translation>Локальный адрес</translation>
    </message>
    <message>
        <source>Enable Amnezia</source>
        <translation>Включить Amnezia</translation>
    </message>
    <message>
        <source>Edit the AmneziaWG obfuscation parameters (junk packets, magic headers, signatures).</source>
        <translation>Редактировать параметры обфускации AmneziaWG (junk-пакеты, магические заголовки, сигнатуры).</translation>
    </message>
    <message>
        <source>Amnezia Options</source>
        <translation>Параметры Amnezia</translation>
    </message>
    <message>
        <source>Generate a Cloudflare WARP account and fill the key, address and endpoint fields above.</source>
        <translation>Сгенерировать учётную запись Cloudflare WARP и заполнить поля ключей, адреса и Endpoint выше.</translation>
    </message>
    <message>
        <source>Generate Warp Config</source>
        <translation>Сгенерировать конфиг Warp</translation>
    </message>
    <message>
        <source>Pre Shared Key</source>
        <translation>Pre-shared Key (PSK)</translation>
    </message>
    <message>
        <source>Use System Interface</source>
        <translation>Исп. системный интерфейс</translation>
    </message>
    <message>
        <source>Public Key</source>
        <translation>Public Key</translation>
    </message>
    <message>
        <source>Private Key</source>
        <translation>Private Key</translation>
    </message>
    <message>
        <source>1420</source>
        <translation>1420</translation>
    </message>
    <message>
        <source>MTU</source>
        <translation>MTU</translation>
    </message>
    <message>
        <source>Workers</source>
        <translation>Workers</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;&lt;br/&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;&lt;br/&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Persistent Keepalive</source>
        <translation>Поддержание активности (Persistent Keepalive)</translation>
    </message>
    <message>
        <source>Generating config...</source>
        <translation>Генерация конфигурации...</translation>
    </message>
    <message>
        <source>Failed to generate warp config</source>
        <translation>Не удалось сгенерировать конфигурацию Warp</translation>
    </message>
    <message>
        <source>Success!</source>
        <translation>Успешно!</translation>
    </message>
</context>
<context>
    <name>EditWireguardAmnezia</name>
    <message>
        <source>Cookie Reply Packet Junk Size (S3)</source>
        <translation>Cookie Reply Packet Junk Size (S3)</translation>
    </message>
    <message>
        <source>Cookie Reply Packet Magic Header (H3)</source>
        <translation>Cookie Reply Packet Magic Header (H3)</translation>
    </message>
    <message>
        <source>Junk Packet Count (Jc)</source>
        <translation>Junk Packet Count (Jc)</translation>
    </message>
    <message>
        <source>Response Packet Magic Header (H2)</source>
        <translation>Response Packet Magic Header (H2)</translation>
    </message>
    <message>
        <source>Init Packet Junk Size (S1)</source>
        <translation>Init Packet Junk Size (S1)</translation>
    </message>
    <message>
        <source>Junk Packet Min Size (Jmin)</source>
        <translation>Junk Packet Min Size (Jmin)</translation>
    </message>
    <message>
        <source>Junk Packet Max Size (Jmax)</source>
        <translation>Junk Packet Max Size (Jmax)</translation>
    </message>
    <message>
        <source>Signature Packet (I4)</source>
        <translation>Signature Packet (I4)</translation>
    </message>
    <message>
        <source>Signature Packet (I1)</source>
        <translation>Signature Packet (I1)</translation>
    </message>
    <message>
        <source>Response Packet Junk Size (S2)</source>
        <translation>Response Packet Junk Size (S2)</translation>
    </message>
    <message>
        <source>Transport Packet Magic Header (H4)</source>
        <translation>Transport Packet Magic Header (H4)</translation>
    </message>
    <message>
        <source>Init Packet Magic Header (H1)</source>
        <translation>Init Packet Magic Header (H1)</translation>
    </message>
    <message>
        <source>Amnezia Options</source>
        <translation>Параметры Amnezia</translation>
    </message>
    <message>
        <source>Transport Packet Junk Size (S4)</source>
        <translation>Transport Packet Junk Size (S4)</translation>
    </message>
    <message>
        <source>Signature Packet (I2)</source>
        <translation>Signature Packet (I2)</translation>
    </message>
    <message>
        <source>Signature Packet (I3)</source>
        <translation>Signature Packet (I3)</translation>
    </message>
    <message>
        <source>Signature Packet (I5)</source>
        <translation>Signature Packet (I5)</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Fills the signature packets with the first QUIC packets Chrome would send to the given domain, so the handshake starts like an HTTP/3 connection to it. Generated locally; nothing is sent. Replaces I1-I5.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Заполняет пакеты-сигнатуры первыми QUIC-пакетами, которые Chrome отправил бы на указанный домен, чтобы хендшейк начинался как HTTP/3-соединение с ним. Генерируется локально, ничего не отправляется. Заменяет I1-I5.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Generate QUIC Signature...</source>
        <translation>Сгенерировать QUIC-сигнатуру...</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Base64 key. Requires S1-S4 to be at least 12.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Ключ в Base64. Требует, чтобы S1-S4 были не меньше 12.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Header Protection Key</source>
        <translation>Header Protection Key</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Bytes of random trailing padding, as a value or range (e.g. 0-64).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Число байтов случайного заполнения в конце пакета — значение или диапазон (напр. 0-64).&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Content Padding Addition</source>
        <translation>Content Padding Addition</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Seconds before a rekey is attempted, as a value or range.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Через сколько секунд пытаться сменить ключи — значение или диапазон.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Rekey After Time</source>
        <translation>Rekey After Time</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Seconds before a handshake is retried, as a value or range.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Через сколько секунд повторять хендшейк — значение или диапазон.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Rekey Timeout</source>
        <translation>Rekey Timeout</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Seconds before keys are rejected, as a value or range.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Через сколько секунд ключи перестают приниматься — значение или диапазон.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Reject After Time</source>
        <translation>Reject After Time</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Seconds of idle time before a keepalive is sent, as a value or range.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Сколько секунд простоя до отправки keepalive — значение или диапазон.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Keepalive Timeout</source>
        <translation>Keepalive Timeout</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Handshake retries before giving up, as a value or range.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.0. Число повторов хендшейка, после которого попытки прекращаются, — значение или диапазон.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Max Handshake Attempts</source>
        <translation>Max Handshake Attempts</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.1. Appends a random trailer to every packet, so message types are no longer identifiable by size.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.1. Добавляет к каждому пакету случайный хвост, чтобы типы сообщений нельзя было определить по размеру.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Random Trailers</source>
        <translation>Random Trailers</translation>
    </message>
    <message>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.1. Never send cookie replies, so an under-load peer stays silent instead of answering with a fixed-size message.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;AmneziaWG 3.1. Никогда не отправлять cookie reply: перегруженный узел молчит, а не отвечает сообщением фиксированного размера.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <source>Disable Cookies</source>
        <translation>Отключить cookie</translation>
    </message>
    <message>
        <source>QUIC Signature</source>
        <translation>QUIC-сигнатура</translation>
    </message>
    <message>
        <source>Domain to imitate:</source>
        <translation>Имитируемый домен:</translation>
    </message>
    <message>
        <source>Generating...</source>
        <translation>Генерация...</translation>
    </message>
    <message>
        <source>Failed to reach the core.</source>
        <translation>Не удалось установить связь с ядром.</translation>
    </message>
    <message>
        <source>Failed to generate QUIC signature</source>
        <translation>Не удалось сгенерировать QUIC-сигнатуру</translation>
    </message>
</context>
<context>
    <name>EditXrayVless</name>
    <message>
        <source>UUID</source>
        <translation>UUID</translation>
    </message>
    <message>
        <source>Encryption</source>
        <translation>Шифрование</translation>
    </message>
    <message>
        <source>none</source>
        <translation>none</translation>
    </message>
    <message>
        <source>Flow</source>
        <translation>Flow</translation>
    </message>
</context>
<context>
    <name>GlobalHotkeys</name>
    <message>
        <source>Global hotkeys are not supported in this desktop session.</source>
        <translation>Глобальные горячие клавиши не поддерживаются в этом сеансе рабочего стола.</translation>
    </message>
    <message>
        <source>It needs Ctrl, Alt or Meta, otherwise it would block typing in other applications.</source>
        <translation>Нужен Ctrl, Alt или Meta, иначе сочетание мешало бы вводу текста в других приложениях.</translation>
    </message>
    <message>
        <source>&quot;%1&quot; already uses it.</source>
        <translation>Уже используется для &quot;%1&quot;.</translation>
    </message>
    <message>
        <source>KDE&apos;s shortcut service failed: %1</source>
        <translation>Сбой службы горячих клавиш KDE: %1</translation>
    </message>
    <message>
        <source>Registration failed: X error %1</source>
        <translation>Не удалось зарегистрировать: ошибка X %1</translation>
    </message>
    <message>
        <source>Registration failed: %1</source>
        <translation>Не удалось зарегистрировать: %1</translation>
    </message>
    <message>
        <source>This key can&apos;t be used in a global hotkey.</source>
        <translation>Эту клавишу нельзя использовать в глобальном сочетании клавиш.</translation>
    </message>
    <message>
        <source>Another application or a system shortcut already uses it.</source>
        <translation>Это сочетание уже занято другим приложением или системой.</translation>
    </message>
</context>
<context>
    <name>GroupItem</name>
    <message>
        <source>Update Subscription</source>
        <translation>Обновить подписку</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>Редактировать</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Basic</source>
        <translation>Стандартный</translation>
    </message>
    <message>
        <source>Subscription</source>
        <translation>Подписка</translation>
    </message>
    <message>
        <source>Archive</source>
        <translation>Архив</translation>
    </message>
    <message>
        <source>Last update: %1</source>
        <translation>Последнее обновление: %1</translation>
    </message>
    <message>
        <source>Confirmation</source>
        <translation>Подтверждение</translation>
    </message>
    <message>
        <source>Remove %1?</source>
        <translation>Удалить %1?</translation>
    </message>
</context>
<context>
    <name>IpEntriesModel</name>
    <message>
        <source>%1 ms</source>
        <translation>%1 мс</translation>
    </message>
    <message>
        <source>Address</source>
        <translation>Адрес</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>Порт</translation>
    </message>
    <message>
        <source>Latency</source>
        <translation>Задержка</translation>
    </message>
</context>
<context>
    <name>IpListItem</name>
    <message>
        <source>%1 d</source>
        <translation>%1 д.</translation>
    </message>
    <message>
        <source>%1 h</source>
        <translation>%1 ч.</translation>
    </message>
    <message>
        <source>%1 min</source>
        <translation>%1 мин.</translation>
    </message>
    <message>
        <source>updated just now</source>
        <translation>обновлено только что</translation>
    </message>
    <message>
        <source>updated %1 min ago</source>
        <translation>обновлено %1 мин. назад</translation>
    </message>
    <message>
        <source>updated %1 h ago</source>
        <translation>обновлено %1 ч. назад</translation>
    </message>
    <message>
        <source>updated %1 d ago</source>
        <translation>обновлено %1 д. назад</translation>
    </message>
    <message>
        <source>URL %1</source>
        <translation>URL %1</translation>
    </message>
    <message>
        <source>Rule-set %1</source>
        <translation>Набор правил %1</translation>
    </message>
    <message>
        <source>Refreshing…</source>
        <translation>Обновление…</translation>
    </message>
    <message>
        <source>Refresh now</source>
        <translation>Обновить сейчас</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Edit</source>
        <translation>Редактировать</translation>
    </message>
    <message>
        <source>View</source>
        <translation>Просмотреть</translation>
    </message>
    <message numerus="yes">
        <source>%Ln entries</source>
        <translation>
            <numerusform>%Ln запись</numerusform>
            <numerusform>%Ln записи</numerusform>
            <numerusform>%Ln записей</numerusform>
        </translation>
    </message>
    <message>
        <source>Result of a deleted scan</source>
        <translation>Результат удаленного сканирования</translation>
    </message>
    <message>
        <source>Result of scan %1</source>
        <translation>Результат сканирования %1</translation>
    </message>
    <message>
        <source>auto-update every %1</source>
        <translation>автообновление раз в %1</translation>
    </message>
    <message>
        <source>never updated</source>
        <translation>не обновлялся</translation>
    </message>
    <message>
        <source>Last update failed: %1</source>
        <translation>Последнее обновление не удалось: %1</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <source>Connect</source>
        <translation>Подключиться</translation>
    </message>
    <message>
        <source>Disconnect</source>
        <translation>Отключиться</translation>
    </message>
    <message>
        <source>Disconnecting</source>
        <translation>Отключение</translation>
    </message>
    <message>
        <source>Program</source>
        <translation>Программа</translation>
    </message>
    <message>
        <source>Preferences</source>
        <translation>Настройки</translation>
    </message>
    <message>
        <source>Server</source>
        <translation>Сервер</translation>
    </message>
    <message>
        <source>Tun Mode</source>
        <translation>Режим TUN</translation>
    </message>
    <message>
        <source>System Proxy</source>
        <translation>Системный прокси</translation>
    </message>
    <message>
        <source>Test Result</source>
        <translation>Результат теста</translation>
    </message>
    <message>
        <source>Share</source>
        <translation>Поделиться</translation>
    </message>
    <message>
        <source>Exit</source>
        <translation>Выход</translation>
    </message>
    <message>
        <source>Show Window</source>
        <translation>Показать окно</translation>
    </message>
    <message>
        <source>Basic Settings</source>
        <translation>Основные настройки</translation>
    </message>
    <message>
        <source>New profile</source>
        <translation>Новый профиль</translation>
    </message>
    <message>
        <source>Groups</source>
        <translation>Группы</translation>
    </message>
    <message>
        <source>Start</source>
        <translation>Запустить</translation>
    </message>
    <message>
        <source>Stop</source>
        <translation>Остановить</translation>
    </message>
    <message>
        <source>Routing Settings</source>
        <translation>Настройки маршрутизации</translation>
    </message>
    <message>
        <source>Integration Settings</source>
        <translation>Настройки интеграции</translation>
    </message>
    <message>
        <source>Resolve Domain for group</source>
        <translation>Преобразовать домены группы в IP</translation>
    </message>
    <message>
        <source>Restart Core</source>
        <translation>Перезапустить ядро</translation>
    </message>
    <message>
        <source>Hide window</source>
        <translation>Скрыть окно</translation>
    </message>
    <message>
        <source>Add profile from clipboard</source>
        <translation>Добавить профиль из буфера обмена</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Debug Info</source>
        <translation>Отладочная информация</translation>
    </message>
    <message>
        <source>QR Code and link</source>
        <translation>QR-код и ссылка</translation>
    </message>
    <message>
        <source>Copy Link</source>
        <translation>Копировать ссылку</translation>
    </message>
    <message>
        <source>Resolve out IP for group</source>
        <translation>Определить выходной IP группы</translation>
    </message>
    <message>
        <source>Clear Test Result</source>
        <translation>Очистить результат теста</translation>
    </message>
    <message>
        <source>Reset Traffic</source>
        <translation>Сбросить трафик</translation>
    </message>
    <message>
        <source>Tools</source>
        <translation>Утилиты</translation>
    </message>
    <message>
        <source>Manage Groups</source>
        <translation>Управление группами</translation>
    </message>
    <message>
        <source>Clear Group test result</source>
        <translation>Очистить результат теста для группы</translation>
    </message>
    <message>
        <source>Scan QR Code</source>
        <translation>Сканировать QR-код</translation>
    </message>
    <message>
        <source>Disable</source>
        <translation>Отключить</translation>
    </message>
    <message>
        <source>Clean up</source>
        <translation>Очистка</translation>
    </message>
    <message>
        <source>Filter logs</source>
        <translation>Фильтр логов</translation>
    </message>
    <message>
        <source>Match case</source>
        <translation>Учитывать регистр</translation>
    </message>
    <message>
        <source>Regular expression</source>
        <translation>Регулярное выражение</translation>
    </message>
    <message>
        <source>Remove Duplicates</source>
        <translation>Удалить дубликаты</translation>
    </message>
    <message>
        <source>fake</source>
        <translation>фейк</translation>
    </message>
    <message>
        <source>Move</source>
        <translation>Переместить</translation>
    </message>
    <message>
        <source>Start with system</source>
        <translation>Запускать вместе с системой</translation>
    </message>
    <message>
        <source>Remember last profile</source>
        <translation>Запоминать последний профиль</translation>
    </message>
    <message>
        <source>Allow other devices to connect</source>
        <translation>Разрешить подключаться другим устройствам</translation>
    </message>
    <message>
        <source>Remove Unavailable</source>
        <translation>Удалить недоступные</translation>
    </message>
    <message>
        <source>Full Test</source>
        <translation>Полный тест</translation>
    </message>
    <message>
        <source>Select All</source>
        <translation>Выбрать все</translation>
    </message>
    <message>
        <source>Copy links of selected (Deep Links)</source>
        <translation>Копировать ссылки выбранных (deep links)</translation>
    </message>
    <message>
        <source>Copy links of selected</source>
        <translation>Копировать ссылки выбранных</translation>
    </message>
    <message>
        <source>Clone</source>
        <translation>Дублировать</translation>
    </message>
    <message>
        <source>Update subscription</source>
        <translation>Обновить подписку</translation>
    </message>
    <message>
        <source>Preset Settings</source>
        <translation>Предустановленные настройки</translation>
    </message>
    <message>
        <source>OTP Manager</source>
        <translation>Менеджер OTP</translation>
    </message>
    <message>
        <source>IP Scanner</source>
        <translation>IP-сканер</translation>
    </message>
    <message>
        <source>Open Web dashboard</source>
        <translation>Открыть веб-панель</translation>
    </message>
    <message>
        <source>Remove Insecure Configs</source>
        <translation>Удалить небезопасные конфигурации</translation>
    </message>
    <message>
        <source>Edit current Group</source>
        <translation>Редактировать текущую группу</translation>
    </message>
    <message>
        <source>Delete current Group</source>
        <translation>Удалить текущую группу</translation>
    </message>
    <message>
        <source>Update all subscriptions</source>
        <translation>Обновить все подписки</translation>
    </message>
    <message>
        <source>Update Rule-Sets</source>
        <translation>Обновить наборы правил</translation>
    </message>
    <message>
        <source>Runtime Stats</source>
        <translation>Статистика</translation>
    </message>
    <message>
        <source>Auto Selector Stats</source>
        <translation>Статистика автовыбора</translation>
    </message>
    <message>
        <source>Traffic Stats</source>
        <translation>Статистика трафика</translation>
    </message>
    <message>
        <source>Check For Update</source>
        <translation>Проверить наличие обновлений</translation>
    </message>
    <message>
        <source>Tun Settings</source>
        <translation>Настройки режима TUN</translation>
    </message>
    <message>
        <source>Restart Program</source>
        <translation>Перезапустить программу</translation>
    </message>
    <message>
        <source>Open Config Folder</source>
        <translation>Открыть папку конфигурации</translation>
    </message>
    <message>
        <source>Resolve Selected Out IP</source>
        <translation>Определить выходной IP выбранных</translation>
    </message>
    <message>
        <source>Refresh Column Widths</source>
        <translation>Обновить ширину колонок</translation>
    </message>
    <message>
        <source>Export Xray config</source>
        <translation>Экспортировать конфигурацию Xray</translation>
    </message>
    <message>
        <source>Copy Test Result</source>
        <translation>Копировать результат теста</translation>
    </message>
    <message>
        <source>Provider: %1</source>
        <translation>Провайдер: %1</translation>
    </message>
    <message>
        <source>Type: Subscription</source>
        <translation>Тип: Подписка</translation>
    </message>
    <message>
        <source>Last updated</source>
        <translation>Последнее обновление</translation>
    </message>
    <message>
        <source>Auto-update: every %1</source>
        <translation>Автообновление: раз в %1</translation>
    </message>
    <message>
        <source>Used</source>
        <translation>Использовано</translation>
    </message>
    <message>
        <source>Remaining</source>
        <translation>Осталось</translation>
    </message>
    <message>
        <source>Expires</source>
        <translation>Истекает</translation>
    </message>
    <message>
        <source>Support</source>
        <translation>Поддержка</translation>
    </message>
    <message>
        <source>Portal</source>
        <translation>Портал</translation>
    </message>
    <message>
        <source>Announcement</source>
        <translation>Объявление</translation>
    </message>
    <message>
        <source>Error</source>
        <translation>Ошибка</translation>
    </message>
    <message>
        <source>Url Test selected Group</source>
        <translation>Тест задержки выбранной группы</translation>
    </message>
    <message>
        <source>Speed Test selected Group</source>
        <translation>Тест скорости выбранной группы</translation>
    </message>
    <message>
        <source>Ignored deeplink with unknown command: %1</source>
        <translation>Проигнорирован deep link с неизвестной командой: %1</translation>
    </message>
    <message>
        <source>Note:</source>
        <translation>Примечание:</translation>
    </message>
    <message>
        <source>Add remote routing profiles</source>
        <translation>Добавить удалённые профили маршрутизации</translation>
    </message>
    <message>
        <source>The link did not contain any valid remote routing profiles.</source>
        <translation>Ссылка не содержит допустимых удаленных профилей маршрутизации.</translation>
    </message>
    <message>
        <source>Add these remote routing profiles?</source>
        <translation>Добавить эти удалённые профили маршрутизации?</translation>
    </message>
    <message>
        <source>Tun Settings changed</source>
        <translation>Настройки режима TUN изменены</translation>
    </message>
    <message>
        <source>Restart Tun to take effect.</source>
        <translation>Перезапустите режим TUN, чтобы изменения вступили в силу.</translation>
    </message>
    <message>
        <source>Confirmation</source>
        <translation>Подтверждение</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>Типу</translation>
    </message>
    <message>
        <source>Security</source>
        <translation>Безопасности</translation>
    </message>
    <message>
        <source>Update all subscriptions?</source>
        <translation>Обновить все подписки?</translation>
    </message>
    <message>
        <source>Enable AdBlock</source>
        <translation>Включить AdBlock</translation>
    </message>
    <message>
        <source>Enable Warp</source>
        <translation>Включить Warp</translation>
    </message>
    <message>
        <source>Add this subscription?

Name: %1
URL: %2</source>
        <translation>Добавить эту подписку?

Название: %1
URL: %2</translation>
    </message>
    <message>
        <source>Auto update</source>
        <translation>Автообновление</translation>
    </message>
    <message>
        <source>%1: cannot be opened</source>
        <translation>%1: не удаётся открыть</translation>
    </message>
    <message>
        <source>%1: larger than 50 MB, skipped</source>
        <translation>%1: размер превышает 50 МБ, пропущено</translation>
    </message>
    <message>
        <source>%1: no QR code found</source>
        <translation>%1: QR-код не найден</translation>
    </message>
    <message>
        <source>%1: not a readable config file</source>
        <translation>%1: файл конфигурации не читается</translation>
    </message>
    <message>
        <source>Nothing could be imported:</source>
        <translation>Ничего не удалось импортировать:</translation>
    </message>
    <message>
        <source>Settings changed</source>
        <translation>Настройки изменены</translation>
    </message>
    <message>
        <source>Restart the program to take effect.</source>
        <translation>Перезапустите программу, чтобы изменения вступили в силу.</translation>
    </message>
    <message>
        <source>Profile</source>
        <translation>Профиль</translation>
    </message>
    <message>
        <source>Group</source>
        <translation>Группа</translation>
    </message>
    <message>
        <source>Imported %1 profile(s)</source>
        <translation>Импортировано профилей: %1</translation>
    </message>
    <message>
        <source>Not Running</source>
        <translation>Не запущен</translation>
    </message>
    <message>
        <source>Listening on all interfaces (%1)</source>
        <translation>Прослушивание на всех интерфейсах (%1)</translation>
    </message>
    <message>
        <source>Select</source>
        <translation>Выбор</translation>
    </message>
    <message>
        <source>Select mode, double-click or press Enter to select a profile, press ESC to exit.</source>
        <translation>Режим выбора: дважды кликните или нажмите Enter для выбора профиля, нажмите ESC для выхода.</translation>
    </message>
    <message>
        <source>Kill switch: not active</source>
        <translation>Kill switch: не активен</translation>
    </message>
    <message>
        <source>Connect OK</source>
        <translation>Подключено</translation>
    </message>
    <message>
        <source>Connecting</source>
        <translation>Подключение</translation>
    </message>
    <message>
        <source>Waiting for authentication</source>
        <translation>Ожидание аутентификации</translation>
    </message>
    <message>
        <source>Tunnel error</source>
        <translation>Ошибка туннеля</translation>
    </message>
    <message>
        <source>Clone %1 item(s)</source>
        <translation>Дублировать %1 записей?</translation>
    </message>
    <message>
        <source>Remove %1 item(s) ?</source>
        <translation>Удалить %1 записей?</translation>
    </message>
    <message>
        <source>Copied %1 item(s)</source>
        <translation>Скопировано %1 записей</translation>
    </message>
    <message>
        <source>Config copied</source>
        <translation>Конфигурация скопирована</translation>
    </message>
    <message>
        <source>QR Code not found</source>
        <translation>QR-код не найден</translation>
    </message>
    <message>
        <source>New logs are held while the view is scrolled up</source>
        <translation>Новые записи не выводятся, пока лог прокручен вверх</translation>
    </message>
    <message>
        <source>Disable Filter</source>
        <translation>Выключить фильтр</translation>
    </message>
    <message>
        <source>Jump to latest</source>
        <translation>К последним записям</translation>
    </message>
    <message>
        <source>Jump to latest (%1 new)</source>
        <translation>К последним записям (новых: %1)</translation>
    </message>
    <message>
        <source>Clear</source>
        <translation>Очистить</translation>
    </message>
    <message>
        <source>[%1] test error: %2</source>
        <translation>[%1] ошибка теста: %2</translation>
    </message>
    <message>
        <source>a tested profile</source>
        <translation>тестируемый профиль</translation>
    </message>
    <message>
        <source>[%1] IP test error: %2</source>
        <translation>[%1] Ошибка теста IP: %2</translation>
    </message>
    <message>
        <source>Failed to build test config for batch: </source>
        <translation>Не удалось собрать тестовую конфигурацию для пакета профилей: </translation>
    </message>
    <message>
        <source>Testing</source>
        <translation>Тестирование</translation>
    </message>
    <message>
        <source>Unavailable</source>
        <translation>Недоступен</translation>
    </message>
    <message>
        <source>The last test did not exit completely, please wait. If it persists, please restart the program.</source>
        <translation>Предыдущий тест еще не завершился, подождите. Если проблема не исчезнет, перезапустите программу.</translation>
    </message>
    <message>
        <source>IP test finished!</source>
        <translation>Тест IP завершён!</translation>
    </message>
    <message>
        <source>The last test did not finish completely, please wait. If it persists, please restart the program.</source>
        <translation>Предыдущий тест еще не завершился, подождите. Если проблема не исчезнет, перезапустите программу.</translation>
    </message>
    <message>
        <source>Failed to build batch test config: </source>
        <translation>Не удалось собрать тестовую конфигурацию для пакета профилей: </translation>
    </message>
    <message>
        <source>a required category</source>
        <translation>обязательная категория</translation>
    </message>
    <message>
        <source>Geo asset missing category</source>
        <translation>В Geo-файле нет категории</translation>
    </message>
    <message>
        <source>The Xray config &quot;%1&quot; needs &quot;%2&quot;, but the installed %3 does not contain it.

Re-downloading from the same source will not fix this — the data file does not include that category. Set the GeoIP/GeoSite asset URL in Settings to a source that provides &quot;%2&quot;, then delete %3 from the app folder and download it again.</source>
        <translation>В конфигурации Xray &quot;%1&quot; требуется &quot;%2&quot;, но установленный %3 его не содержит.

Повторная загрузка из того же источника не поможет — в этом файле данных нет такой категории. В настройках укажите в поле «URL GeoIP-ресурса» или «URL GeoSite-ресурса» источник, который содержит &quot;%2&quot;, затем удалите %3 из папки приложения и загрузите его снова.</translation>
    </message>
    <message>
        <source>Geo asset files required</source>
        <translation>Требуются Geo-файлы</translation>
    </message>
    <message>
        <source>The Xray config &quot;%1&quot; uses geoip/geosite routing rules, but the required data files (geoip.dat / geosite.dat) are not installed.

Download them now?</source>
        <translation>В конфигурации Xray &quot;%1&quot; используются правила маршрутизации geoip/geosite, но необходимые файлы данных (geoip.dat / geosite.dat) не установлены.

Загрузить их сейчас?</translation>
    </message>
    <message>
        <source>Geo asset download failed</source>
        <translation>Ошибка загрузки Geo-файлов</translation>
    </message>
    <message>
        <source>Downloaded Xray geo asset files.</source>
        <translation>Загружены гео-файлы Xray.</translation>
    </message>
    <message>
        <source>Geo assets installed</source>
        <translation>Geo-файлы установлены</translation>
    </message>
    <message>
        <source>[Kill switch] Not starting %1: the kill switch is not active.</source>
        <translation>[Kill switch] %1 не запускается: kill switch не активен.</translation>
    </message>
    <message>
        <source>[Kill switch] The extra core %1 could not be allowed through the kill switch.</source>
        <translation>[Kill switch] Не удалось пропустить дополнительное ядро %1 через kill switch.</translation>
    </message>
    <message>
        <source>Kill switch</source>
        <translation>Kill switch</translation>
    </message>
    <message>
        <source>The kill switch could not be updated to allow this profile&apos;s extra core, so the profile was not started. See the log for details.</source>
        <translation>Не удалось обновить kill switch, чтобы пропустить дополнительное ядро этого профиля, поэтому профиль не запущен. Подробности в логе.</translation>
    </message>
    <message>
        <source>[Auto selector] Running the best %1 of %2 ranked profiles.</source>
        <translation>[Автовыбор] Запущены лучшие профили: %1 из %2.</translation>
    </message>
    <message>
        <source>If there is no response for a long time, it is recommended to restart the software.</source>
        <translation>Если нет ответа в течение долгого времени, рекомендуем перезапустить программу.</translation>
    </message>
    <message>
        <source>Starting profile %1</source>
        <translation>Запуск профиля %1</translation>
    </message>
    <message>
        <source>Failed to start profile %1</source>
        <translation>Не удалось запустить профиль %1</translation>
    </message>
    <message>
        <source>Stopping profile %1</source>
        <translation>Остановка профиля %1</translation>
    </message>
    <message>
        <source>Failed to stop, please restart the program.</source>
        <translation>Не удалось остановить, пожалуйста, перезапустите программу.</translation>
    </message>
    <message>
        <source>[VPN] %1: %2</source>
        <translation>[VPN] %1: %2</translation>
    </message>
    <message>
        <source>VPN endpoint problem</source>
        <translation>Проблема конечной точки VPN</translation>
    </message>
    <message>
        <source>VPN endpoint</source>
        <translation>Конечная точка VPN</translation>
    </message>
    <message>
        <source>[VPN] %1: the core did not answer the sign-in prompt.</source>
        <translation>[VPN] %1: ядро не ответило на запрос входа.</translation>
    </message>
    <message>
        <source>[VPN] %1: could not answer the sign-in prompt: %2</source>
        <translation>[VPN] %1: не удалось ответить на запрос входа: %2</translation>
    </message>
    <message>
        <source>[VPN] %1: signed in again with a new one-time code.</source>
        <translation>[VPN] %1: выполнен повторный вход с новым одноразовым кодом.</translation>
    </message>
    <message>
        <source>[VPN] %1 rejected the saved credentials; restarting the profile with a new one-time code.</source>
        <translation>[VPN] %1: сохраненные учетные данные отклонены; профиль перезапускается с новым одноразовым кодом.</translation>
    </message>
    <message>
        <source>The server refused the credentials saved with this profile. Enter the ones to use for this session; the profile itself is left unchanged.</source>
        <translation>Сервер отклонил учетные данные, сохраненные в этом профиле. Введите данные для этого сеанса; сам профиль не изменится.</translation>
    </message>
    <message>
        <source>[VPN] %1 rejected the saved credentials.</source>
        <translation>[VPN] %1: сохраненные учетные данные отклонены.</translation>
    </message>
    <message>
        <source>Stop Testing</source>
        <translation>Остановить тестирование</translation>
    </message>
    <message>
        <source>Tun device misbehaving</source>
        <translation>Некорректное поведение устройства TUN</translation>
    </message>
    <message>
        <source>Strict routing unavailable</source>
        <translation>Строгая маршрутизация недоступна</translation>
    </message>
    <message>
        <source>Windows could not enable strict routing. Open Tun Settings, disable Strict Route, and start the profile again.

Disabling Strict Route may cause DNS leaks.

Error: %1</source>
        <translation>Windows не удалось включить строгую маршрутизацию. Откройте «Настройки режима TUN», отключите «Строгая маршрутизация» и снова запустите профиль.

Отключение строгой маршрутизации может привести к утечкам DNS.

Ошибка: %1</translation>
    </message>
    <message>
        <source>Reset</source>
        <translation>Сбросить</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>Отмена</translation>
    </message>
    <message>
        <source>Add new Group</source>
        <translation>Добавить новую группу</translation>
    </message>
    <message>
        <source>Delete selected Group</source>
        <translation>Удалить выбранную группу</translation>
    </message>
    <message>
        <source>Edit selected Group</source>
        <translation>Редактировать выбранную группу</translation>
    </message>
    <message>
        <source>Remove %1?</source>
        <translation>Удалить %1?</translation>
    </message>
    <message>
        <source>Url Test Selected</source>
        <translation>Тест задержки</translation>
    </message>
    <message>
        <source>Url Test Group</source>
        <translation>Тест задержки всей группы</translation>
    </message>
    <message>
        <source>Hidden menu</source>
        <translation>Скрытое меню</translation>
    </message>
    <message>
        <source>Resolve Selected Domain</source>
        <translation>Преобразовать выбранный домен в IP</translation>
    </message>
    <message>
        <source>Remove Invalid</source>
        <translation>Удалить недействительные</translation>
    </message>
    <message>
        <source>If you have trouble starting VPN, you can force reset Core process here and then try starting the profile again. The error is %1</source>
        <translation>Если у вас возникли проблемы с запуском режима TUN, можете принудительно сбросить процесс ядра здесь и попробовать перезапустить профиль. Ошибка: %1</translation>
    </message>
    <message>
        <source>Remove %1 Unavailable item(s) ?</source>
        <translation>Удалить %1 недоступных записей?</translation>
    </message>
    <message>
        <source>Remove %1 Invalid item(s) ?</source>
        <translation>Удалить %1 неверных записей?</translation>
    </message>
    <message>
        <source>Enable Filter</source>
        <translation>Включить фильтр</translation>
    </message>
    <message>
        <source>Include:</source>
        <translation>Показывать:</translation>
    </message>
    <message>
        <source>Out IP</source>
        <translation>Выходной IP</translation>
    </message>
    <message>
        <source>Speed</source>
        <translation>Скорость</translation>
    </message>
    <message>
        <source>Sort By:</source>
        <translation>Сортировать по:</translation>
    </message>
    <message numerus="yes">
        <source>%n remote rule-set(s) refreshed</source>
        <translation>
            <numerusform>%n удаленный набор правил был обновлён</numerusform>
            <numerusform>%n удаленных набора правил были обновлены</numerusform>
            <numerusform>%n удаленных наборов правил было обновлено</numerusform>
        </translation>
    </message>
    <message>
        <source>Stop Sorting</source>
        <translation>Отключить сортировку</translation>
    </message>
    <message>
        <source>Latency</source>
        <translation>Пингу</translation>
    </message>
    <message>
        <source>Download Speed</source>
        <translation>Скорости скачивания (↓)</translation>
    </message>
    <message>
        <source>Upload Speed</source>
        <translation>Скорости отдачи (↑)</translation>
    </message>
    <message>
        <source>IP Out</source>
        <translation>Выходному IP</translation>
    </message>
    <message>
        <source>Total</source>
        <translation>Всего</translation>
    </message>
    <message>
        <source>Downloaded</source>
        <translation>Получено (↓)</translation>
    </message>
    <message>
        <source>Uploaded</source>
        <translation>Отправлено (↑)</translation>
    </message>
    <message>
        <source>Select Profile</source>
        <translation>Выбор профиля</translation>
    </message>
    <message>
        <source>Could not update the autostart entry:</source>
        <translation>Не удалось обновить запись автозапуска:</translation>
    </message>
    <message>
        <source> Test result(s) copied to clipboard!</source>
        <translation> результаты тестов скопированы в буфер обмена!</translation>
    </message>
    <message>
        <source>Config files (*.json *.conf *.txt *.yaml *.yml *.ini *.ovpn *.xml)</source>
        <translation>Файлы конфигурации (*.json *.conf *.txt *.yaml *.yml *.ini *.ovpn *.xml)</translation>
    </message>
    <message>
        <source>No insecure configs found.</source>
        <translation>Небезопасных конфигураций не обнаружено.</translation>
    </message>
    <message>
        <source>Remove %1 insecure config(s)?</source>
        <translation>Удалить %1 небезопасных конфигураций?</translation>
    </message>
    <message>
        <source>Replace domain server addresses with their resolved IPs?</source>
        <translation>Заменить домены в адресах серверов на их разрешённые IP-адреса?</translation>
    </message>
    <message>
        <source>Connections</source>
        <translation>Подключения</translation>
    </message>
    <message>
        <source>Logs</source>
        <translation>Логи</translation>
    </message>
    <message>
        <source>Restart as administrator was cancelled, Throne keeps running</source>
        <translation>Перезапуск от имени администратора отменен, Throne продолжает работу</translation>
    </message>
    <message>
        <source>This installation cannot grant the core privileges by itself.</source>
        <translation>Эта установка не может самостоятельно выдать ядру необходимые права.</translation>
    </message>
    <message>
        <source>Please give the core root privileges</source>
        <translation>Пожалуйста, предоставьте ядру права root.</translation>
    </message>
    <message>
        <source>Requesting permission</source>
        <translation>Запрос разрешения</translation>
    </message>
    <message>
        <source>Please Enter your password in the opened terminal, then try again</source>
        <translation>Введите пароль в открывшемся терминале и повторите попытку.</translation>
    </message>
    <message>
        <source>System proxy: %1</source>
        <translation>Системный прокси: %1</translation>
    </message>
    <message>
        <source>Kill switch is not active</source>
        <translation>Kill switch не активен</translation>
    </message>
    <message>
        <source>Restart as Administrator</source>
        <translation>Перезапустить от имени администратора</translation>
    </message>
    <message>
        <source>Retry</source>
        <translation>Повторить</translation>
    </message>
    <message>
        <source>Grant Privileges</source>
        <translation>Предоставить права</translation>
    </message>
    <message>
        <source>Disable Kill Switch</source>
        <translation>Отключить kill switch</translation>
    </message>
    <message>
        <source>Disable kill switch</source>
        <translation>Отключение kill switch</translation>
    </message>
    <message>
        <source>Turn off the kill switch? Traffic that does not go through Throne will no longer be blocked.</source>
        <translation>Отключить kill switch? Трафик, идущий не через Throne, больше не будет блокироваться.</translation>
    </message>
    <message>
        <source>[Kill switch] Restarting the core so that its own traffic passes the kill switch...</source>
        <translation>[Kill switch] Перезапуск ядра, чтобы его собственный трафик проходил через kill switch...</translation>
    </message>
    <message>
        <source>The sing-box API is disabled. Set a listen port in Preferences &gt; Basic Settings &gt; Core.</source>
        <translation>API sing-box отключен. Укажите прослушиваемый порт в разделе Настройки &gt; Основные настройки &gt; Ядро.</translation>
    </message>
    <message>
        <source>Start a profile first; the dashboard is served by the running core.</source>
        <translation>Сначала запустите профиль; веб-панель обслуживается запущенным ядром.</translation>
    </message>
    <message>
        <source>Web dashboard</source>
        <translation>Веб-панель</translation>
    </message>
    <message>
        <source>The dashboard is not installed yet. Download it now?</source>
        <translation>Веб-панель ещё не установлена. Загрузить сейчас?</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>Настройки</translation>
    </message>
    <message>
        <source>Routing</source>
        <translation>Маршрутизация</translation>
    </message>
    <message>
        <source>Operation Mode</source>
        <translation>Режим работы</translation>
    </message>
    <message>
        <source>Routing Menu</source>
        <translation>Меню маршрутизации</translation>
    </message>
    <message>
        <source>Export Sing-box config</source>
        <translation>Экспортировать конфигурацию Sing-box</translation>
    </message>
    <message>
        <source>Open Manager</source>
        <translation>Открыть менеджер</translation>
    </message>
    <message>
        <source>Profile test aborted</source>
        <translation>Тест профиля прерван</translation>
    </message>
    <message>
        <source>Something is very wrong, the subject ent cannot be found!</source>
        <translation>Что-то явно не так, объект ent не может быть найден!</translation>
    </message>
    <message>
        <source>Profile manager data is corrupted, try again.</source>
        <translation>Данные менеджера профилей повреждены, попробуйте еще раз.</translation>
    </message>
    <message>
        <source>Speedtest finished!</source>
        <translation>Тест скорости завершён!</translation>
    </message>
    <message>
        <source>Failed to stop tests</source>
        <translation>Не удалось остановить тесты</translation>
    </message>
    <message>
        <source>Geo data files were downloaded successfully.

Please try again.</source>
        <translation>Гео-файлы были успешно загружены.

Пожалуйста, попробуйте еще раз.</translation>
    </message>
    <message>
        <source>BuildConfig return error</source>
        <translation>Ошибка BuildConfig</translation>
    </message>
    <message>
        <source>Another profile is starting...</source>
        <translation>Другой профиль запускается...</translation>
    </message>
    <message>
        <source>Another profile is stopping...</source>
        <translation>Другой профиль останавливается...</translation>
    </message>
    <message>
        <source>Try to start the config, but the core has not listened to the RPC port, so restart it...</source>
        <translation>Попытка запустить конфигурацию, но ядро не слушает порт RPC — перезапуск ядра...</translation>
    </message>
    <message>
        <source>Stop return error</source>
        <translation>Ошибка остановки</translation>
    </message>
    <message>
        <source>Copy core config</source>
        <translation>Копировать конфигурацию ядра</translation>
    </message>
    <message>
        <source>Copy test config</source>
        <translation>Копировать тестовую конфигурацию</translation>
    </message>
    <message>
        <source>Cannot start</source>
        <translation>Невозможно запустить</translation>
    </message>
    <message>
        <source>A dashboard download is already running</source>
        <translation>Загрузка веб-панели уже выполняется</translation>
    </message>
    <message>
        <source>The core did not answer.</source>
        <translation>Ядро не ответило.</translation>
    </message>
    <message>
        <source>Failed to install the dashboard</source>
        <translation>Не удалось установить веб-панель</translation>
    </message>
    <message>
        <source>Last download request has not finished yet</source>
        <translation>Предыдущая загрузка ещё не завершилась</translation>
    </message>
    <message>
        <source>Traffic Graph</source>
        <translation>График</translation>
    </message>
    <message>
        <source>Failed to download update assets</source>
        <translation>Не удалось скачать файлы обновления</translation>
    </message>
    <message>
        <source>Speedtest Current</source>
        <translation>Тест скорости текущего профиля</translation>
    </message>
    <message>
        <source>Speedtest Selected</source>
        <translation>Тест скорости</translation>
    </message>
    <message>
        <source>Speedtest Group</source>
        <translation>Тест скорости всей группы</translation>
    </message>
    <message>
        <source>The last url test did not exit completely, please wait. If it persists, please restart the program.</source>
        <translation>Предыдущий тест задержки еще не завершился, подождите. Если проблема не исчезнет, перезапустите программу.</translation>
    </message>
    <message>
        <source>URL test finished!</source>
        <translation>Тест задержки завершён!</translation>
    </message>
    <message>
        <source>Profile speed test aborted</source>
        <translation>Тест скорости профиля прерван</translation>
    </message>
    <message>
        <source>[%1] speed test error: %2</source>
        <translation>[%1] ошибка теста скорости: %2</translation>
    </message>
    <message>
        <source>User opted for no privilege req, some features may not work</source>
        <translation>Пользователь решил не запрашивать привилегии, некоторые функции могут не работать</translation>
    </message>
    <message>
        <source>Unable to capture screen</source>
        <translation>Невозможно захватить экран</translation>
    </message>
    <message>
        <source>Please run Throne as admin</source>
        <translation>Пожалуйста, запустите Throne от имени администратора</translation>
    </message>
    <message>
        <source>Add profile from File(s)</source>
        <translation>Добавить профиль из файла</translation>
    </message>
    <message>
        <source>Add subscription</source>
        <translation>Добавить подписку</translation>
    </message>
    <message>
        <source>Select Routing</source>
        <translation>Выбор маршрутизации</translation>
    </message>
    <message>
        <source>OTP Codes</source>
        <translation>Коды OTP</translation>
    </message>
    <message>
        <source>All files (*)</source>
        <translation>Все файлы (*)</translation>
    </message>
    <message>
        <source>QR code images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)</source>
        <translation>Изображения QR-кодов (*.png *.jpg *.jpeg *.bmp *.gif *.webp)</translation>
    </message>
    <message>
        <source>Select profile files</source>
        <translation>Выберите файлы профилей</translation>
    </message>
    <message>
        <source>routing profiles</source>
        <translation>профилей маршрутизации</translation>
    </message>
    <message>
        <source>Import routing profile</source>
        <translation>Импорт профиля маршрутизации</translation>
    </message>
    <message>
        <source>Imported profile</source>
        <translation>Импортированный профиль</translation>
    </message>
    <message>
        <source>Add this routing profile?

Name: %1</source>
        <translation>Добавить этот профиль маршрутизации?

Название: %1</translation>
    </message>
    <message>
        <source>The link did not contain a subscription URL.</source>
        <translation>Ссылка не содержит URL подписки.</translation>
    </message>
    <message>
        <source>The link could not be parsed:
</source>
        <translation>Не удалось распознать ссылку:
</translation>
    </message>
    <message>
        <source>[Auto selector] Reusing existing test results; ranked %1 profiles.</source>
        <translation>[Автовыбор] Использованы сохранённые результаты замеров; ранжировано профилей: %1.</translation>
    </message>
    <message>
        <source>[Auto selector] Measuring %1 not-yet-tested profiles...</source>
        <translation>[Автовыбор] Замер ещё не протестированных профилей: %1...</translation>
    </message>
    <message>
        <source>[Auto selector] Ranked %1 profiles.</source>
        <translation>[Автовыбор] Ранжировано профилей: %1.</translation>
    </message>
    <message>
        <source>[Auto selector] The subscription replaced profiles it was running on — rebuilding.</source>
        <translation>[Автовыбор] Подписка заменила используемые профили — пересборка.</translation>
    </message>
    <message>
        <source>[Auto selector] Every running profile stopped working — rebuilding from the next best candidates.</source>
        <translation>[Автовыбор] Все запущенные профили перестали работать — выполняется пересборка из следующих наилучших кандидатов.</translation>
    </message>
    <message>
        <source>Close every connection listed below</source>
        <translation>Закрыть все подключения, перечисленные ниже</translation>
    </message>
    <message>
        <source>Failed to close connections: %1</source>
        <translation>Не удалось закрыть подключения: %1</translation>
    </message>
    <message>
        <source>IPC error</source>
        <translation>Ошибка IPC</translation>
    </message>
    <message>
        <source>Collapse All</source>
        <translation>Свернуть всё</translation>
    </message>
    <message>
        <source>Expand All</source>
        <translation>Развернуть всё</translation>
    </message>
    <message>
        <source>No active routing profile found.</source>
        <translation>Активный профиль маршрутизации не найден.</translation>
    </message>
    <message>
        <source>The current routing profile is locked against modifications.</source>
        <translation>Текущий профиль маршрутизации защищен от изменений.</translation>
    </message>
    <message>
        <source>The current routing profile is raw JSON.</source>
        <translation>Текущий профиль маршрутизации задан в формате raw JSON.</translation>
    </message>
    <message>
        <source>The current routing profile auto-updates from a URL.</source>
        <translation>Текущий профиль маршрутизации автоматически обновляется с URL-адреса.</translation>
    </message>
    <message>
        <source>Removed %1 from the %2 rules of &quot;%3&quot;</source>
        <translation>Удалено %1 из правил %2 профиля &quot;%3&quot;</translation>
    </message>
    <message>
        <source>Failed to add routing rule: %1</source>
        <translation>Не удалось добавить правило маршрутизации: %1</translation>
    </message>
    <message>
        <source>Moved %1 from the %2 to the %3 rules of &quot;%4&quot;</source>
        <translation>Перемещено %1 из правил %2 в правила %3 профиля &quot;%4&quot;</translation>
    </message>
    <message>
        <source>Failed to save routing rule: %1</source>
        <translation>Не удалось сохранить правило маршрутизации: %1</translation>
    </message>
    <message>
        <source>Warp-bypass</source>
        <translation>Warp-bypass</translation>
    </message>
    <message>
        <source>Modify rules in &quot;%1&quot;</source>
        <translation>Изменить правила в &quot;%1&quot;</translation>
    </message>
    <message>
        <source>Modify rules</source>
        <translation>Изменить правила</translation>
    </message>
    <message>
        <source>%1 rules</source>
        <translation>Правила «%1»</translation>
    </message>
    <message>
        <source>Routing profile &quot;%1&quot;. Checked targets go %2; unchecking one removes its rule.</source>
        <translation>Профиль маршрутизации &quot;%1&quot;. Отмеченные цели попадают в правила «%2»; снятие флажка удаляет правило.</translation>
    </message>
    <message>
        <source>in %1</source>
        <translation>уже в «%1»</translation>
    </message>
    <message>
        <source>Checking it moves the rule from %1 to %2</source>
        <translation>При отметке правило переместится из «%1» в «%2»</translation>
    </message>
    <message>
        <source>covered by %1 in %2</source>
        <translation>перекрыто правилом %1 в «%2»</translation>
    </message>
    <message>
        <source>%1 in %2 is matched first, so this rule would never apply</source>
        <translation>Правило %1 в «%2» срабатывает раньше, поэтому это правило никогда не применится</translation>
    </message>
    <message>
        <source>%1  (%2)</source>
        <translation>%1  (%2)</translation>
    </message>
    <message>
        <source>Added %1</source>
        <translation>Добавлено %1</translation>
    </message>
    <message>
        <source>Moved %1</source>
        <translation>Перемещено %1</translation>
    </message>
    <message>
        <source>Removed %1</source>
        <translation>Удалено %1</translation>
    </message>
    <message>
        <source>%1 rules:
%2</source>
        <translation>Правила «%1»:
%2</translation>
    </message>
    <message>
        <source>Process %1</source>
        <translation>Процесс %1</translation>
    </message>
    <message>
        <source>Appended %1 to the %2 rules of &quot;%3&quot;</source>
        <translation>Добавлено %1 в правила %2 профиля &quot;%3&quot;</translation>
    </message>
    <message>
        <source>Direct</source>
        <translation>Напрямую</translation>
    </message>
    <message>
        <source>Proxy</source>
        <translation>Прокси</translation>
    </message>
    <message>
        <source>Block</source>
        <translation>Блокировать</translation>
    </message>
    <message>
        <source>Copied: %1</source>
        <translation>Скопировано: %1</translation>
    </message>
    <message>
        <source>Copy Destination (%1)</source>
        <translation>Копировать пункт назначения (%1)</translation>
    </message>
    <message>
        <source>Copy Process Name (%1)</source>
        <translation>Копировать имя процесса (%1)</translation>
    </message>
    <message>
        <source>Close all connections (%1)</source>
        <translation>Закрыть все подключения (%1)</translation>
    </message>
    <message>
        <source>Close connection</source>
        <translation>Закрыть подключение</translation>
    </message>
    <message>
        <source>Copy Process Name</source>
        <translation>Копировать имя процесса</translation>
    </message>
    <message>
        <source>Close all connections for &quot;%1&quot; (%2)</source>
        <translation>Закрыть все подключения &quot;%1&quot; (%2)</translation>
    </message>
    <message>
        <source>Trigger main window</source>
        <translation>Показать/скрыть главное окно</translation>
    </message>
    <message>
        <source>Show groups</source>
        <translation>Показать группы</translation>
    </message>
    <message>
        <source>Show routes</source>
        <translation>Показать маршруты</translation>
    </message>
    <message>
        <source>Proxy mode</source>
        <translation>Режим прокси</translation>
    </message>
    <message>
        <source>Toggle System Proxy</source>
        <translation>Вкл./выкл. системный прокси</translation>
    </message>
    <message>
        <source>Start/Stop Profile</source>
        <translation>Запустить/остановить профиль</translation>
    </message>
    <message>
        <source>Toggle Tun Mode</source>
        <translation>Вкл./выкл. режим TUN</translation>
    </message>
    <message>
        <source>Global hotkey not registered: %1</source>
        <translation>Глобальная горячая клавиша не зарегистрирована: %1</translation>
    </message>
</context>
<context>
    <name>OtpItem</name>
    <message>
        <source>Edit</source>
        <translation>Редактировать</translation>
    </message>
    <message>
        <source>Export</source>
        <translation>Экспорт</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>invalid</source>
        <translation>неверный</translation>
    </message>
    <message>
        <source>%1s</source>
        <translation>%1 сек.</translation>
    </message>
</context>
<context>
    <name>ProfilesTableFilterHeader</name>
    <message>
        <source>Filter...</source>
        <translation>Фильтр...</translation>
    </message>
    <message>
        <source>Filter by country...</source>
        <translation>Фильтр по странам...</translation>
    </message>
    <message>
        <source>Disable Filter</source>
        <translation>Выключить фильтр</translation>
    </message>
    <message>
        <source>Enable Filter</source>
        <translation>Включить фильтр</translation>
    </message>
</context>
<context>
    <name>ProfilesTableModel</name>
    <message>
        <source>Certificate checks are turned off by the &quot;Skip TLS certificate authentication&quot; setting.</source>
        <translation>Проверка сертификатов отключена настройкой «Не проверять TLS-сертификаты по умолчанию».</translation>
    </message>
    <message>
        <source>This config&apos;s traffic is not properly protected.</source>
        <translation>Трафик этой конфигурации не защищен должным образом.</translation>
    </message>
    <message>
        <source>Type</source>
        <translation>Тип</translation>
    </message>
    <message>
        <source>Address</source>
        <translation>Адрес</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Название</translation>
    </message>
    <message>
        <source>Test Result</source>
        <translation>Результат теста</translation>
    </message>
    <message>
        <source>Traffic</source>
        <translation>Трафик</translation>
    </message>
</context>
<context>
    <name>ProxyItem</name>
    <message>
        <source>Confirmation</source>
        <translation>Подтверждение</translation>
    </message>
    <message>
        <source>Remove %1?</source>
        <translation>Удалить %1?</translation>
    </message>
</context>
<context>
    <name>QGuiApplication</name>
    <message>
        <source>QT_LAYOUT_DIRECTION</source>
        <translation>QT_LAYOUT_DIRECTION</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <source>The selected TLS version is not supported on this system.</source>
        <translation>Выбранная версия TLS не поддерживается в этой системе.</translation>
    </message>
    <message>
        <source>Request with proxy but no profile started.</source>
        <translation>Запрос через прокси, но профиль не запущен.</translation>
    </message>
    <message>
        <source>Response larger than %1 MB</source>
        <translation>Размер ответа превышает %1 МБ</translation>
    </message>
    <message>
        <source>Download failed: server returned HTTP status %1.</source>
        <translation>Загрузка не удалась: сервер вернул HTTP-статус %1.</translation>
    </message>
    <message>
        <source>Download failed: the server returned an empty response.</source>
        <translation>Загрузка не удалась: сервер вернул пустой ответ.</translation>
    </message>
    <message>
        <source>Could not open file.</source>
        <translation>Не удалось открыть файл.</translation>
    </message>
    <message>
        <source>Could not write file.</source>
        <translation>Не удалось записать файл.</translation>
    </message>
    <message>
        <source>Could not save downloaded file.</source>
        <translation>Не удалось сохранить загруженный файл.</translation>
    </message>
    <message>
        <source>Proxy: %1
Direct: %2</source>
        <translation>Прокси: %1
Напрямую: %2</translation>
    </message>
    <message>
        <source>Add profiles to this group</source>
        <translation>Добавить профили в эту группу</translation>
    </message>
    <message>
        <source>Create new subscription group</source>
        <translation>Создать новую группу подписки</translation>
    </message>
    <message>
        <source>Import HTTP proxy profile</source>
        <translation>Импортировать профиль HTTP-прокси</translation>
    </message>
    <message>
        <source>url detected</source>
        <translation>Обнаружен URL</translation>
    </message>
    <message>
        <source>%1
How to update?</source>
        <translation>%1
Как обновить?</translation>
    </message>
    <message>
        <source>Requesting subscription: %1</source>
        <translation>Запрос подписки: %1</translation>
    </message>
    <message>
        <source>Requesting subscription %1 error: %2</source>
        <translation>Ошибка запроса подписки %1: %2</translation>
    </message>
    <message>
        <source>Subscription request fininshed: %1</source>
        <translation>Запрос подписки завершён: %1</translation>
    </message>
    <message>
        <source>Clearing servers...</source>
        <translation>Очистка серверов...</translation>
    </message>
    <message>
        <source>Processing subscription data...</source>
        <translation>Идёт обработка данных подписки...</translation>
    </message>
    <message>
        <source>Failed to parse the Clash configuration.</source>
        <translation>Не удалось разобрать конфигурацию Clash.</translation>
    </message>
    <message>
        <source>Failed to import the OpenVPN profile.</source>
        <translation>Не удалось импортировать профиль OpenVPN.</translation>
    </message>
    <message>
        <source>Failed to import the OpenConnect profile.</source>
        <translation>Не удалось импортировать профиль OpenConnect.</translation>
    </message>
    <message>
        <source>Failed to decode the vpn:// link.</source>
        <translation>Не удалось декодировать ссылку vpn://.</translation>
    </message>
    <message>
        <source>No importable profile found in the vpn:// link.</source>
        <translation>В ссылке vpn:// не найдено профилей для импорта.</translation>
    </message>
    <message>
        <source>Removed %1 duplicate profiles:</source>
        <translation>Удалено дубликатов профилей (%1):</translation>
    </message>
    <message>
        <source>Removed %1 insecure profiles:</source>
        <translation>Удалено небезопасных профилей (%1):</translation>
    </message>
    <message>
        <source>Skipped removing invalid profiles: the core is unreachable.</source>
        <translation>Удаление недействительных профилей пропущено: ядро недоступно.</translation>
    </message>
    <message>
        <source>Removed %1 invalid profiles:</source>
        <translation>Удалено недействительных профилей (%1):</translation>
    </message>
    <message>
        <source>The running profile was kept.</source>
        <translation>Запущенный профиль сохранён.</translation>
    </message>
    <message>
        <source>Removed %1 unavailable profiles:</source>
        <translation>Удалено недоступных профилей (%1):</translation>
    </message>
    <message>
        <source>Subscription update already queued: %1</source>
        <translation>Обновление подписки уже в очереди: %1</translation>
    </message>
    <message>
        <source>Subscription server update interval: %1 hour(s)</source>
        <translation>Интервал обновления, заданный сервером подписки: %1 ч.</translation>
    </message>
    <message>
        <source>Process complete, applying...</source>
        <translation>Процесс завершён, применение...</translation>
    </message>
    <message>
        <source>The URL of %1 changed during the update, so the result was discarded.</source>
        <translation>URL подписки %1 изменился во время обновления, поэтому результат отброшен.</translation>
    </message>
    <message>
        <source>The server returned a web page instead of a subscription. If it is a CAPTCHA or a bot check, try another TLS version, HTTP version or User Agent in the group&apos;s Advanced settings.</source>
        <translation>Сервер вернул веб-страницу вместо подписки. Если это CAPTCHA или проверка на бота, попробуйте другую версию TLS, версию HTTP или User Agent в окне «Доп. настройки группы».</translation>
    </message>
    <message>
        <source>Announcement from %1: %2</source>
        <translation>Объявление от %1: %2</translation>
    </message>
    <message>
        <source>No profiles found in the subscription: %1 was left unchanged.</source>
        <translation>В подписке не найдено профилей: группа %1 оставлена без изменений.</translation>
    </message>
    <message>
        <source>Added %1 profiles:
%2
Updated %3 profiles:
%4
Deleted %5 Profiles:
%6</source>
        <translation>Добавлено %1 профилей:
%2
Обновлено %3 профилей:
%4
Удалено %5 профилей:
%6</translation>
    </message>
    <message>
        <source>Still in use, so kept instead of deleted:
%1</source>
        <translation>Используются, поэтому сохранены, а не удалены:
%1</translation>
    </message>
    <message>
        <source>Working, so kept instead of deleted:
%1</source>
        <translation>Работают, поэтому сохранены, а не удалены:
%1</translation>
    </message>
    <message>
        <source>Nothing</source>
        <translation>Без изменений</translation>
    </message>
    <message>
        <source>Change of %1:</source>
        <translation>Изменения в группе %1:</translation>
    </message>
    <message>
        <source>Change of %1</source>
        <translation>Изменения в группе %1</translation>
    </message>
    <message>
        <source>After the URL test of %1:</source>
        <translation>После теста задержки группы %1:</translation>
    </message>
    <message>
        <source>IPv4 forwarding breaks Tun mode</source>
        <translation>Пересылка IPv4 нарушает работу режима TUN</translation>
    </message>
    <message>
        <source>IPv4 forwarding is on for the network adapter &quot;%1&quot;, usually because Mobile Hotspot or Internet Connection Sharing is sharing it.

Windows then ignores the adapter binding that keeps Throne&apos;s own connections out of the Tun, so they loop back into it and fail.

To fix this, share the hotspot from the throne-tun adapter instead of &quot;%1&quot; (Settings &gt; Mobile hotspot &gt; Share my internet connection from), or turn the hotspot off while using Tun mode.</source>
        <translation>Для сетевого адаптера &quot;%1&quot; включена пересылка IPv4 — обычно потому, что через него раздаёт интернет мобильный хот-спот или общий доступ к подключению к Интернету (ICS).

В этом случае Windows игнорирует привязку к адаптеру, которая не пускает собственные подключения Throne в TUN, поэтому они зацикливаются обратно в него и не работают.

Чтобы это исправить, раздавайте хот-спот с адаптера throne-tun вместо &quot;%1&quot; (Параметры &gt; Мобильный хот-спот &gt; Совместное использование подключения к Интернету) или отключайте хот-спот на время работы в режиме TUN.</translation>
    </message>
    <message>
        <source>Core exited, cleaning up...</source>
        <translation>Ядро завершило работу, очистка...</translation>
    </message>
    <message>
        <source>Core exits too frequently, stop automatic restart this profile.</source>
        <translation>Ядро завершается слишком часто — автоматический перезапуск этого профиля остановлен.</translation>
    </message>
    <message>
        <source>Restarting the core ...</source>
        <translation>Перезапуск ядра...</translation>
    </message>
    <message>
        <source>Download Profiles</source>
        <translation>Скачать профили</translation>
    </message>
    <message>
        <source>Requesting profile error: %1</source>
        <translation>Ошибка запроса профиля: %1</translation>
    </message>
    <message>
        <source>Added remote routing profiles: %1 of %2 fetched</source>
        <translation>Добавлены удалённые профили маршрутизации: получено %1 из %2</translation>
    </message>
    <message>
        <source>Update</source>
        <translation>Обновление</translation>
    </message>
    <message>
        <source>Not official support platform</source>
        <translation>Платформа официально не поддерживается</translation>
    </message>
    <message>
        <source>Requesting update error: %1</source>
        <translation>Ошибка запроса обновления: %1</translation>
    </message>
    <message>
        <source>No update</source>
        <translation>Обновлений нет</translation>
    </message>
    <message>
        <source>Update found: %1
Release note:
%2</source>
        <translation>Найдено обновление: %1
Список изменений:
%2</translation>
    </message>
    <message>
        <source>Open in browser</source>
        <translation>Открыть в браузере</translation>
    </message>
    <message>
        <source>Close</source>
        <translation>Закрыть</translation>
    </message>
    <message>
        <source>Update is ready, restart to install?</source>
        <translation>Обновление готово, перезапустить программу для установки?</translation>
    </message>
    <message>
        <source>Select</source>
        <translation>Выбор</translation>
    </message>
    <message>
        <source>Chain Proxy</source>
        <translation>Цепочка прокси</translation>
    </message>
    <message>
        <source>None</source>
        <translation>Нет</translation>
    </message>
    <message>
        <source>Used: %1 Remain: %2 Expire: %3</source>
        <translation>Использовано: %1, осталось: %2, истекает: %3</translation>
    </message>
    <message>
        <source>Auto-update: every %1</source>
        <translation>Автообновление: раз в %1</translation>
    </message>
    <message>
        <source>Default</source>
        <translation>По умолчанию</translation>
    </message>
    <message>
        <source>%1 dials its own address: %2</source>
        <translation>%1 подключается к собственному адресу: %2</translation>
    </message>
    <message>
        <source>The routing profile lists an endpoint profile (id %1) that no longer exists</source>
        <translation>В профиле маршрутизации указан профиль конечной точки (id %1), который больше не существует</translation>
    </message>
    <message>
        <source>%1 is listed as a routing profile endpoint but is empty or corrupted</source>
        <translation>%1 указан как конечная точка профиля маршрутизации, но он пуст или повреждён</translation>
    </message>
    <message>
        <source>%1 is listed as a routing profile endpoint but a hop of it no longer exists</source>
        <translation>%1 указан как конечная точка профиля маршрутизации, но одно из его звеньев больше не существует</translation>
    </message>
    <message>
        <source>%1 is listed as a routing profile endpoint, so its last hop must be an OpenVPN or OpenConnect profile</source>
        <translation>%1 указан как конечная точка профиля маршрутизации, поэтому его последним звеном должен быть профиль OpenVPN или OpenConnect</translation>
    </message>
    <message>
        <source>Hops of the routing profile endpoint %1 cannot use an extra core, a full config, or be a chain</source>
        <translation>Звенья конечной точки профиля маршрутизации %1 не могут использовать доп. ядро или полную конфигурацию и не могут быть цепочкой</translation>
    </message>
    <message>
        <source>Hops of the routing profile endpoint %1 cannot run on the Xray core</source>
        <translation>Звенья конечной точки профиля маршрутизации %1 не могут работать на ядре Xray</translation>
    </message>
    <message>
        <source>%1 is used as an endpoint of the routing profile and by the started profile at the same time, remove it from one of them</source>
        <translation>%1 одновременно используется как конечная точка профиля маршрутизации и запущенным профилем — удалите его из одного из них</translation>
    </message>
    <message>
        <source>%1 is listed twice in the endpoints of the routing profile</source>
        <translation>%1 указан в конечных точках профиля маршрутизации дважды</translation>
    </message>
    <message>
        <source>The profile no longer exists</source>
        <translation>Профиль больше не существует</translation>
    </message>
    <message>
        <source>%1 profiles have no single server to scan</source>
        <translation>У профилей типа %1 нет единого сервера для сканирования</translation>
    </message>
    <message>
        <source>Hysteria2 realm profiles have no fixed server to scan</source>
        <translation>У профилей Hysteria2 с realm нет фиксированного сервера для сканирования</translation>
    </message>
    <message>
        <source>The chain has no hops</source>
        <translation>В цепочке нет звеньев</translation>
    </message>
    <message>
        <source>A hop of the chain no longer exists</source>
        <translation>Одно из звеньев цепочки больше не существует</translation>
    </message>
    <message>
        <source>Chains with an extra-core or Xray full config hop cannot be tested</source>
        <translation>Цепочки со звеном на доп. ядре или с полной конфигурацией Xray нельзя протестировать</translation>
    </message>
    <message>
        <source>the group&apos;s fixed address</source>
        <translation>фиксированный адрес группы</translation>
    </message>
    <message>
        <source>The IP list no longer exists</source>
        <translation>Список IP больше не существует</translation>
    </message>
    <message>
        <source>IP list &quot;%1&quot;</source>
        <translation>список IP &quot;%1&quot;</translation>
    </message>
    <message>
        <source>The IP list &quot;%1&quot; is empty</source>
        <translation>Список IP &quot;%1&quot; пуст</translation>
    </message>
    <message>
        <source>%1 profiles have no single server address to replace</source>
        <translation>У профилей типа %1 нет единого адреса сервера, который можно заменить</translation>
    </message>
    <message>
        <source>Hysteria2 realm profiles have no fixed server address to replace</source>
        <translation>У профилей Hysteria2 с realm нет фиксированного адреса сервера, который можно заменить</translation>
    </message>
    <message>
        <source>The profile could not be copied for the scan</source>
        <translation>Не удалось скопировать профиль для сканирования</translation>
    </message>
    <message>
        <source>The profile could not be checked: core unreachable</source>
        <translation>Не удалось проверить профиль: ядро недоступно</translation>
    </message>
    <message>
        <source>The profile is not valid; the log has the details</source>
        <translation>Профиль недействителен; подробности в логе</translation>
    </message>
    <message>
        <source>The profile cannot be pointed at the scanned addresses</source>
        <translation>Профиль нельзя перенаправить на просканированные адреса</translation>
    </message>
    <message>
        <source>Auto-update: running %1</source>
        <translation>Запуск автообновления %1</translation>
    </message>
    <message>
        <source>internal error: null profile</source>
        <translation>внутренняя ошибка: пустой профиль</translation>
    </message>
    <message>
        <source>not a remote routing profile</source>
        <translation>не является удалённым профилем маршрутизации</translation>
    </message>
    <message>
        <source>remote URL is empty</source>
        <translation>не указан URL удалённого профиля</translation>
    </message>
    <message>
        <source>could not parse a routing profile from the response</source>
        <translation>не удалось разобрать профиль маршрутизации из ответа</translation>
    </message>
    <message>
        <source>the remote content is a raw routing profile, which is not supported for remote profiles yet</source>
        <translation>удаленное содержимое — Raw-профиль маршрутизации, а такие профили пока не поддерживаются как удаленные</translation>
    </message>
    <message>
        <source>Updating remote routing profile: %1</source>
        <translation>Обновление удалённого профиля маршрутизации: %1</translation>
    </message>
    <message>
        <source>Remote routing profile %1 failed: %2</source>
        <translation>Ошибка удалённого профиля маршрутизации %1: %2</translation>
    </message>
    <message>
        <source>Remote routing profile updated: %1</source>
        <translation>Удалённый профиль маршрутизации обновлён: %1</translation>
    </message>
    <message>
        <source>Remote routing profiles: %1 updated, %2 failed</source>
        <translation>Удаленные профили маршрутизации: обновлено — %1, с ошибкой — %2</translation>
    </message>
    <message>
        <source>Downloaded Xray geo asset: %1</source>
        <translation>Загружен гео-файл Xray: %1</translation>
    </message>
    <message>
        <source>Download geo asset</source>
        <translation>Загрузка гео-файла</translation>
    </message>
    <message>
        <source>%1 was downloaded successfully.</source>
        <translation>Файл %1 успешно загружен.</translation>
    </message>
    <message>
        <source>Failed to download %1:
%2</source>
        <translation>Не удалось загрузить %1:
%2</translation>
    </message>
    <message>
        <source>Reality</source>
        <translation>Reality</translation>
    </message>
    <message>
        <source>Insecure TLS</source>
        <translation>Небезопасный TLS</translation>
    </message>
    <message>
        <source>Private</source>
        <translation>Частный</translation>
    </message>
    <message>
        <source>Compromised</source>
        <translation>Скомпрометированный</translation>
    </message>
    <message>
        <source>Static Key</source>
        <translation>Статический ключ</translation>
    </message>
    <message>
        <source>Unverified TLS</source>
        <translation>Непроверенный TLS</translation>
    </message>
    <message>
        <source>TLS</source>
        <translation>TLS</translation>
    </message>
    <message>
        <source>Raw</source>
        <translation>Raw</translation>
    </message>
    <message>
        <source>Encrypted</source>
        <translation>Зашифрованный</translation>
    </message>
    <message>
        <source>Insecure</source>
        <translation>Небезопасный</translation>
    </message>
    <message>
        <source>Weak Cipher</source>
        <translation>Слабый шифр</translation>
    </message>
    <message>
        <source>[Warn] Throne did not shut down cleanly last time. Diagnostics were saved to: %1</source>
        <translation>[Предупреждение] В прошлый раз Throne не завершил работу корректно. Результаты диагностики сохранены в: %1</translation>
    </message>
    <message>
        <source>An auto selector cannot be a hop in a chain: it moves to a different server on its own whenever one degrades.</source>
        <translation>Автовыбор не может быть звеном цепочки: он сам переключается на другой сервер, когда текущий начинает работать хуже.</translation>
    </message>
    <message>
        <source>a profile that no longer exists</source>
        <translation>профиль, которого больше нет</translation>
    </message>
    <message>
        <source>Auto Selector</source>
        <translation>Автовыбор</translation>
    </message>
    <message>
        <source>no group</source>
        <translation>нет группы</translation>
    </message>
    <message>
        <source>missing group</source>
        <translation>группа отсутствует</translation>
    </message>
    <message>
        <source>missing profile</source>
        <translation>профиль отсутствует</translation>
    </message>
    <message>
        <source>chain or auto selector</source>
        <translation>цепочка или автовыбор</translation>
    </message>
    <message>
        <source>needs too many core switches</source>
        <translation>требуется слишком много переключений ядра</translation>
    </message>
    <message>
        <source>extra-core profile</source>
        <translation>профиль доп. ядра</translation>
    </message>
    <message>
        <source>full config profile</source>
        <translation>профиль полной конфигурации</translation>
    </message>
    <message>
        <source>config does not parse</source>
        <translation>не удаётся разобрать конфиг</translation>
    </message>
    <message>
        <source>Tailscale profile</source>
        <translation>профиль Tailscale</translation>
    </message>
    <message>
        <source>OpenVPN or OpenConnect profile</source>
        <translation>профиль OpenVPN или OpenConnect</translation>
    </message>
    <message>
        <source>filtered out by name</source>
        <translation>отфильтрованы по названию</translation>
    </message>
    <message>
        <source>filtered out by country</source>
        <translation>отфильтрованы по стране</translation>
    </message>
    <message>
        <source>last test failed</source>
        <translation>последний тест завершился с ошибкой</translation>
    </message>
    <message>
        <source>Xray full config cannot be combined with the group&apos;s proxies</source>
        <translation>Полную конфигурацию Xray нельзя сочетать с прокси группы</translation>
    </message>
    <message>
        <source>never</source>
        <translation>никогда</translation>
    </message>
    <message>
        <source>%1s ago</source>
        <translation>%1 сек. назад</translation>
    </message>
    <message>
        <source>%1m ago</source>
        <translation>%1 мин. назад</translation>
    </message>
    <message>
        <source>%1h ago</source>
        <translation>%1 ч. назад</translation>
    </message>
    <message>
        <source>Auto selector paused — no network connection (%1 profiles held)</source>
        <translation>Автовыбор приостановлен — нет подключения к сети (профилей удерживается: %1)</translation>
    </message>
    <message>
        <source>Auto selector starting — checking %1 profiles</source>
        <translation>Запуск автовыбора — проверка %1 профилей</translation>
    </message>
    <message>
        <source>Auto selector checking profiles (%1/%2 measured)</source>
        <translation>Автовыбор проверяет профили (проверено %1/%2)</translation>
    </message>
    <message>
        <source>Auto selector — no working profile out of %1, still checking</source>
        <translation>Автовыбор — нет рабочих профилей из %1, проверка продолжается</translation>
    </message>
    <message>
        <source>Auto selector on %1 (%2 of %3 working)</source>
        <translation>Автовыбор: %1 (работает %2 из %3)</translation>
    </message>
    <message>
        <source>, switched %1</source>
        <translation>, переключено %1</translation>
    </message>
    <message>
        <source>%1 working</source>
        <translation>работает %1</translation>
    </message>
    <message>
        <source>%1 cooling down</source>
        <translation>приостановлено %1</translation>
    </message>
    <message>
        <source>%1 not checked yet</source>
        <translation>ещё не проверено %1</translation>
    </message>
    <message>
        <source>%1 being checked</source>
        <translation>проверяется %1</translation>
    </message>
    <message>
        <source>balancing over %1</source>
        <translation>в балансировке %1</translation>
    </message>
    <message>
        <source>%1 profiles: %2</source>
        <translation>%1 профилей: %2</translation>
    </message>
    <message>
        <source>, </source>
        <translation>, </translation>
    </message>
    <message>
        <source>No auto selector is running.</source>
        <translation>Автовыбор не запущен.</translation>
    </message>
    <message>
        <source>That profile is not in the running pool.</source>
        <translation>Этот профиль не находится в активном пуле.</translation>
    </message>
    <message>
        <source>Could not reach the core.</source>
        <translation>Не удалось установить связь с ядром.</translation>
    </message>
    <message>
        <source>[Auto selector] Every profile is failing, but this machine has no network connection — keeping the current pool.</source>
        <translation>[Автовыбор] Все профили дают сбой, но у этого компьютера нет подключения к сети — сохраняется текущий пул.</translation>
    </message>
    <message>
        <source>just now</source>
        <translation>только что</translation>
    </message>
    <message>
        <source>Working</source>
        <translation>Работает</translation>
    </message>
    <message>
        <source>Unstable</source>
        <translation>Нестабильный</translation>
    </message>
    <message>
        <source>Not checked</source>
        <translation>Не проверен</translation>
    </message>
    <message>
        <source>Failing</source>
        <translation>Не работает</translation>
    </message>
    <message>
        <source>Paused</source>
        <translation>Приостановлен</translation>
    </message>
    <message>
        <source>your choice, carrying traffic now</source>
        <translation>ваш выбор, сейчас через него идёт трафик</translation>
    </message>
    <message>
        <source>your choice, but not usable right now</source>
        <translation>ваш выбор, но пока что не работает</translation>
    </message>
    <message>
        <source>carrying traffic now</source>
        <translation>сейчас через него идёт трафик</translation>
    </message>
    <message>
        <source>failed to connect, retrying in %1s</source>
        <translation>не удалось установить соединение, повтор через %1 сек.</translation>
    </message>
    <message>
        <source>failed to connect</source>
        <translation>не удалось установить соединение</translation>
    </message>
    <message>
        <source>every check failed</source>
        <translation>все проверки завершились неудачно</translation>
    </message>
    <message>
        <source>ready to take over</source>
        <translation>готов принять трафик</translation>
    </message>
    <message>
        <source>check in progress</source>
        <translation>проверка выполняется</translation>
    </message>
    <message>
        <source>queued for checking</source>
        <translation>в очереди на проверку</translation>
    </message>
    <message>
        <source>%1 of %2 checks failed</source>
        <translation>неудачных проверок: %1 из %2</translation>
    </message>
    <message>
        <source>Connect OK</source>
        <translation>Подключение успешно</translation>
    </message>
    <message>
        <source>Unavailable</source>
        <translation>Недоступен</translation>
    </message>
    <message>
        <source>%1 route prefer</source>
        <translation>%1 — предпочтительный маршрут</translation>
    </message>
    <message>
        <source>Connected</source>
        <translation>Подключено</translation>
    </message>
    <message>
        <source>Connecting…</source>
        <translation>Подключение…</translation>
    </message>
    <message>
        <source>Waiting for sign-in</source>
        <translation>Ожидание входа</translation>
    </message>
    <message>
        <source>Error</source>
        <translation>Ошибка</translation>
    </message>
    <message>
        <source>Unknown</source>
        <translation>Неизвестно</translation>
    </message>
    <message>
        <source>Too much data to fit in a QR code.</source>
        <translation>Слишком много данных для QR-кода.</translation>
    </message>
    <message>
        <source>Copy</source>
        <translation>Копировать</translation>
    </message>
    <message>
        <source>null</source>
        <translation>null</translation>
    </message>
    <message>
        <source>boolean</source>
        <translation>логическое значение</translation>
    </message>
    <message>
        <source>number</source>
        <translation>число</translation>
    </message>
    <message>
        <source>string</source>
        <translation>строка</translation>
    </message>
    <message>
        <source>array</source>
        <translation>массив</translation>
    </message>
    <message>
        <source>object</source>
        <translation>объект</translation>
    </message>
    <message>
        <source>Empty document</source>
        <translation>Пустой документ</translation>
    </message>
    <message>
        <source>Unexpected content after the end of the document</source>
        <translation>Лишнее содержимое после конца документа</translation>
    </message>
    <message>
        <source>The document is nested too deeply</source>
        <translation>Слишком глубокая вложенность документа</translation>
    </message>
    <message>
        <source>Unexpected end of document</source>
        <translation>Неожиданный конец документа</translation>
    </message>
    <message>
        <source>Expected a property name in quotes</source>
        <translation>Ожидается имя свойства в кавычках</translation>
    </message>
    <message>
        <source>Expected a colon after the property name</source>
        <translation>Ожидается двоеточие после имени свойства</translation>
    </message>
    <message>
        <source>Trailing comma</source>
        <translation>Лишняя запятая в конце</translation>
    </message>
    <message>
        <source>Expected a comma or a closing brace</source>
        <translation>Ожидается запятая или закрывающая фигурная скобка</translation>
    </message>
    <message>
        <source>Expected a comma or a closing bracket</source>
        <translation>Ожидается запятая или закрывающая квадратная скобка</translation>
    </message>
    <message>
        <source>Unterminated string</source>
        <translation>Незакрытая строка</translation>
    </message>
    <message>
        <source>Unterminated escape sequence</source>
        <translation>Незавершённая escape-последовательность</translation>
    </message>
    <message>
        <source>Incomplete unicode escape sequence</source>
        <translation>Неполная escape-последовательность Unicode</translation>
    </message>
    <message>
        <source>Invalid unicode escape sequence</source>
        <translation>Недопустимая escape-последовательность Unicode</translation>
    </message>
    <message>
        <source>Invalid escape sequence</source>
        <translation>Недопустимая escape-последовательность</translation>
    </message>
    <message>
        <source>Control character in string</source>
        <translation>Управляющий символ в строке</translation>
    </message>
    <message>
        <source>Invalid value</source>
        <translation>Недопустимое значение</translation>
    </message>
    <message>
        <source>Invalid number</source>
        <translation>Недопустимое число</translation>
    </message>
    <message>
        <source>The schema is empty</source>
        <translation>Схема пуста</translation>
    </message>
    <message>
        <source>The schema has no definition at %1</source>
        <translation>В схеме нет определения для %1</translation>
    </message>
    <message>
        <source>Expected %1, got %2</source>
        <translation>Ожидается %1, получено: %2</translation>
    </message>
    <message>
        <source> or </source>
        <translation> или </translation>
    </message>
    <message>
        <source>Expected %1 here</source>
        <translation>Здесь ожидается %1</translation>
    </message>
    <message>
        <source>%1 is not valid here (expected: %2)</source>
        <translation>%1 здесь недопустимо (ожидается: %2)</translation>
    </message>
    <message>
        <source>This option is deprecated</source>
        <translation>Этот параметр устарел</translation>
    </message>
    <message>
        <source>&quot;%1&quot; is not in the expected format</source>
        <translation>&quot;%1&quot; не соответствует ожидаемому формату</translation>
    </message>
    <message>
        <source>Value must be at least %1</source>
        <translation>Значение должно быть не меньше %1</translation>
    </message>
    <message>
        <source>Value must be at most %1</source>
        <translation>Значение должно быть не больше %1</translation>
    </message>
    <message>
        <source>Unknown field &quot;%1&quot;</source>
        <translation>Неизвестное поле &quot;%1&quot;</translation>
    </message>
    <message>
        <source>Missing required field &quot;%1&quot;</source>
        <translation>Отсутствует обязательное поле &quot;%1&quot;</translation>
    </message>
    <message>
        <source>Unknown %1 %2 (expected: %3)</source>
        <translation>Неизвестное значение поля %1: %2 (ожидается: %3)</translation>
    </message>
    <message>
        <source>Value does not match any accepted form</source>
        <translation>Значение не соответствует ни одному допустимому формату</translation>
    </message>
    <message>
        <source>Empty OpenVPN configuration.</source>
        <translation>Пустая конфигурация OpenVPN.</translation>
    </message>
    <message>
        <source>&lt;connection&gt; block without a remote, skipped.</source>
        <translation>Блок &lt;connection&gt; без remote пропущен.</translation>
    </message>
    <message>
        <source>Ignored an unreadable peer fingerprint: %1</source>
        <translation>Проигнорирован нечитаемый отпечаток сервера: %1</translation>
    </message>
    <message>
        <source>PKCS#12 bundles are not supported; export the CA, certificate and key as PEM.</source>
        <translation>Пакеты PKCS#12 не поддерживаются; экспортируйте CA, сертификат и ключ в формате PEM.</translation>
    </message>
    <message>
        <source>Ignored inline block: &lt;%1&gt;</source>
        <translation>Проигнорирован встроенный блок: &lt;%1&gt;</translation>
    </message>
    <message>
        <source>This is an OpenVPN server configuration (%1), not a client profile.</source>
        <translation>Это конфигурация сервера OpenVPN (%1), а не клиентский профиль.</translation>
    </message>
    <message>
        <source>TAP (layer 2) tunnels are not supported; only `dev tun` profiles can be imported.</source>
        <translation>Туннели TAP (уровень 2) не поддерживаются; импортировать можно только профили с `dev tun`.</translation>
    </message>
    <message>
        <source>This is an OpenVPN server configuration (mode server), not a client profile.</source>
        <translation>Это конфигурация сервера OpenVPN (mode server), а не клиентский профиль.</translation>
    </message>
    <message>
        <source>`proto %1` is a server transport.</source>
        <translation>`proto %1` — серверный транспорт.</translation>
    </message>
    <message>
        <source>Unknown transport: proto %1</source>
        <translation>Неизвестный транспорт: proto %1</translation>
    </message>
    <message>
        <source>Ignored an unreadable IPv6 interface address: %1</source>
        <translation>Проигнорирован нечитаемый IPv6-адрес интерфейса: %1</translation>
    </message>
    <message>
        <source>Credentials live in %1; enter them in the profile editor.</source>
        <translation>Учётные данные хранятся в %1; введите их в редакторе профиля.</translation>
    </message>
    <message>
        <source>A hash-directory CRL is not supported; point `crl-verify` at a PEM or DER file.</source>
        <translation>CRL в виде каталога хешей не поддерживается; укажите в `crl-verify` файл PEM или DER.</translation>
    </message>
    <message>
        <source>Ignored a symbolic route target: route %1</source>
        <translation>Проигнорирована символьная цель маршрута: route %1</translation>
    </message>
    <message>
        <source>Ignored an unreadable route: %1</source>
        <translation>Проигнорирован нечитаемый маршрут: %1</translation>
    </message>
    <message>
        <source>redirect-gateway flag has no sing-box equivalent: %1</source>
        <translation>Флаг redirect-gateway не имеет аналога в sing-box: %1</translation>
    </message>
    <message>
        <source>Not supported by the OpenVPN endpoint, ignored: %1</source>
        <translation>Не поддерживается конечной точкой OpenVPN, проигнорировано: %1</translation>
    </message>
    <message>
        <source>Unknown OpenVPN directive, ignored: %1</source>
        <translation>Неизвестная директива OpenVPN, проигнорирована: %1</translation>
    </message>
    <message>
        <source>`%1` turns the client certificate off; it was dropped and the server has to accept password login.</source>
        <translation>`%1` отключает сертификат клиента; он был удалён, и сервер должен разрешать вход по паролю.</translation>
    </message>
    <message>
        <source>`cert` without a `key`: add the private key, or `client-cert-not-required` for password-only login.</source>
        <translation>`cert` без `key`: добавьте ключ клиента или `client-cert-not-required` для входа только по паролю.</translation>
    </message>
    <message>
        <source>`key` without a `cert`: add the client certificate.</source>
        <translation>`key` без `cert`: добавьте сертификат клиента.</translation>
    </message>
    <message>
        <source>No `remote` server in the OpenVPN configuration.</source>
        <translation>В конфигурации OpenVPN нет сервера `remote`.</translation>
    </message>
    <message>
        <source>Ignored an unreadable interface address: %1</source>
        <translation>Проигнорирован нечитаемый адрес интерфейса: %1</translation>
    </message>
    <message>
        <source>`remote-cert-eku` replaces `remote-cert-tls`; the latter was dropped.</source>
        <translation>`remote-cert-eku` заменяет `remote-cert-tls`; последний был удалён.</translation>
    </message>
    <message>
        <source>Unsupported OpenConnect protocol, ignored: %1</source>
        <translation>Неподдерживаемый протокол OpenConnect, проигнорирован: %1</translation>
    </message>
    <message>
        <source>Unknown compression mode, ignored: %1</source>
        <translation>Неизвестный режим сжатия, проигнорирован: %1</translation>
    </message>
    <message>
        <source>Unknown reported OS, ignored: %1</source>
        <translation>Неизвестное значение «Сообщаемая ОС», проигнорировано: %1</translation>
    </message>
    <message>
        <source>Unsupported token mode, ignored: %1</source>
        <translation>Неподдерживаемый режим токена, проигнорирован: %1</translation>
    </message>
    <message>
        <source>Expected --form-entry=FORM:OPTION=VALUE, ignored: %1</source>
        <translation>Ожидается --form-entry=FORM:OPTION=VALUE, проигнорировано: %1</translation>
    </message>
    <message>
        <source>Configure a proxy through Throne&apos;s chain instead, ignored: %1</source>
        <translation>Вместо этого настройте прокси через цепочку Throne; проигнорировано: %1</translation>
    </message>
    <message>
        <source>Unknown OpenConnect option, ignored: %1</source>
        <translation>Неизвестный параметр OpenConnect, проигнорирован: %1</translation>
    </message>
    <message>
        <source>Unknown OpenConnect option, ignored: -%1</source>
        <translation>Неизвестный параметр OpenConnect, проигнорирован: -%1</translation>
    </message>
    <message>
        <source>An AnyConnect profile carrying a DTD is not accepted.</source>
        <translation>Профиль AnyConnect, содержащий DTD, не принимается.</translation>
    </message>
    <message>
        <source>Malformed AnyConnect profile: %1</source>
        <translation>Некорректный профиль AnyConnect: %1</translation>
    </message>
    <message>
        <source>The AnyConnect profile lists no host entry.</source>
        <translation>В профиле AnyConnect нет ни одной записи хоста.</translation>
    </message>
    <message>
        <source>Skipped a host entry without an address.</source>
        <translation>Пропущена запись хоста без адреса.</translation>
    </message>
    <message>
        <source>Skipped &quot;%1&quot;: IKEv2/IPsec is not spoken by the OpenConnect endpoint.</source>
        <translation>Пропущен &quot;%1&quot;: конечная точка OpenConnect не поддерживает IKEv2/IPsec.</translation>
    </message>
    <message>
        <source>Skipped an unreadable host address: %1</source>
        <translation>Пропущен нечитаемый адрес хоста: %1</translation>
    </message>
    <message>
        <source>Empty OpenConnect profile.</source>
        <translation>Пустой профиль OpenConnect.</translation>
    </message>
    <message>
        <source>The AnyConnect profile has no host entry this endpoint can use.</source>
        <translation>В профиле AnyConnect нет записи хоста, которую может использовать эта конечная точка.</translation>
    </message>
    <message>
        <source>No OpenConnect options found.</source>
        <translation>Параметры OpenConnect не найдены.</translation>
    </message>
    <message>
        <source>No OpenConnect server address found.</source>
        <translation>Адрес сервера OpenConnect не найден.</translation>
    </message>
    <message>
        <source>Secret is empty</source>
        <translation>Secret не указан</translation>
    </message>
    <message>
        <source>Secret is not valid base32</source>
        <translation>Secret не является корректной строкой Base32</translation>
    </message>
    <message>
        <source>Secret is too short</source>
        <translation>Secret слишком короткий</translation>
    </message>
    <message>
        <source>Digits must be between %1 and %2</source>
        <translation>Значение «Цифры» должно быть от %1 до %2</translation>
    </message>
    <message>
        <source>Period must be between %1 and %2 seconds</source>
        <translation>Период должен быть от %1 до %2 секунд</translation>
    </message>
    <message>
        <source>Not a readable OTP export</source>
        <translation>Не удалось прочитать экспорт OTP</translation>
    </message>
    <message>
        <source>Unreadable migration link</source>
        <translation>Нечитаемая ссылка миграции</translation>
    </message>
    <message>
        <source>Not an otpauth link or secret: %1</source>
        <translation>Не ссылка otpauth и не Secret: %1</translation>
    </message>
    <message>
        <source>Endpoint</source>
        <translation>Конечная точка</translation>
    </message>
    <message>
        <source>Local</source>
        <translation>Локальный</translation>
    </message>
    <message>
        <source>Database error</source>
        <translation>Ошибка базы данных</translation>
    </message>
    <message>
        <source>%1 failed: %2. Details are in the log file.</source>
        <translation>Ошибка операции %1: %2. Подробности — в файле лога.</translation>
    </message>
    <message>
        <source>Traffic statistics paused</source>
        <translation>Статистика трафика приостановлена</translation>
    </message>
    <message>
        <source>The statistics database is unusable (%1). Statistics are paused for this session and the file will be rebuilt when Throne restarts.</source>
        <translation>База данных статистики непригодна для использования (%1). Сбор статистики приостановлен на этот сеанс, а файл будет пересоздан при перезапуске Throne.</translation>
    </message>
    <message>
        <source>Writing statistics keeps failing (%1). Statistics are paused until Throne restarts.</source>
        <translation>Запись статистики постоянно завершается ошибкой (%1). Сбор статистики приостановлен до перезапуска Throne.</translation>
    </message>
    <message>
        <source>Empty</source>
        <translation>Пусто</translation>
    </message>
    <message numerus="yes">
        <source>%n problem(s)</source>
        <translation>
            <numerusform>%n проблема</numerusform>
            <numerusform>%n проблемы</numerusform>
            <numerusform>%n проблем</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n warning(s)</source>
        <translation>
            <numerusform>%n предупреждение</numerusform>
            <numerusform>%n предупреждения</numerusform>
            <numerusform>%n предупреждений</numerusform>
        </translation>
    </message>
    <message>
        <source>Valid JSON</source>
        <translation>Корректный JSON</translation>
    </message>
    <message>
        <source>Line %1: %2</source>
        <translation>Строка %1: %2</translation>
    </message>
    <message>
        <source>JSON Editor</source>
        <translation>Редактор JSON</translation>
    </message>
    <message>
        <source>Format</source>
        <translation>Форматировать</translation>
    </message>
    <message>
        <source>Invalid JSON</source>
        <translation>Некорректный JSON</translation>
    </message>
    <message>
        <source>Fix the errors before formatting the document.</source>
        <translation>Исправьте ошибки перед форматированием документа.</translation>
    </message>
    <message>
        <source>The document must be a JSON object.</source>
        <translation>Документ должен представлять собой объект JSON.</translation>
    </message>
    <message>
        <source>%1 scan in progress %2/%3 (%4%)</source>
        <translation>Сканирование %1: %2/%3 (%4%)</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>Настройки</translation>
    </message>
    <message numerus="yes">
        <source> · +%n more</source>
        <translation>
            <numerusform> · еще %n сканирование</numerusform>
            <numerusform> · еще %n сканирования</numerusform>
            <numerusform> · еще %n сканирований</numerusform>
        </translation>
    </message>
    <message>
        <source> · %1 found</source>
        <translation> · найдено: %1</translation>
    </message>
    <message>
        <source>+%1 more</source>
        <translation>и еще %1</translation>
    </message>
    <message>
        <source>Settings changed, restart to apply</source>
        <translation>Настройки изменены, перезапустите для применения</translation>
    </message>
    <message>
        <source>Restart</source>
        <translation>Перезапустить</translation>
    </message>
    <message>
        <source>Ignore</source>
        <translation>Игнорировать</translation>
    </message>
    <message>
        <source>%1: the private key is empty</source>
        <translation>%1: не указан Private Key</translation>
    </message>
    <message>
        <source>%1: no address is set</source>
        <translation>%1: не указан адрес</translation>
    </message>
    <message>
        <source>Failed to reach the core.</source>
        <translation>Не удалось установить связь с ядром.</translation>
    </message>
    <message>
        <source>Cloudflare WARP</source>
        <translation>Cloudflare WARP</translation>
    </message>
    <message>
        <source>Generating a WARP identity registers a new device with Cloudflare.&lt;br&gt;&lt;br&gt;Do you accept the &lt;a href=&quot;%1&quot;&gt;Cloudflare WARP terms of service&lt;/a&gt;?</source>
        <translation>При создании идентификатора WARP в Cloudflare регистрируется новое устройство.&lt;br&gt;&lt;br&gt;Вы принимаете &lt;a href=&quot;%1&quot;&gt;условия использования Cloudflare WARP&lt;/a&gt;?</translation>
    </message>
    <message>
        <source>Unverified Host Key</source>
        <translation>Непроверенный Host Key</translation>
    </message>
    <message>
        <source>The built-in WARP MASQUE identity has not been generated yet (Routing Settings)</source>
        <translation>Встроенный идентификатор WARP MASQUE еще не создан (Настройки маршрутизации)</translation>
    </message>
    <message>
        <source>The built-in WARP WireGuard identity has not been generated yet (Routing Settings)</source>
        <translation>Встроенный идентификатор WARP WireGuard еще не создан (Настройки маршрутизации)</translation>
    </message>
    <message>
        <source>Choose a profile for the config test</source>
        <translation>Выберите профиль для теста конфигурации</translation>
    </message>
    <message>
        <source>This type of profile cannot be pointed at scanned addresses</source>
        <translation>Профиль этого типа нельзя перенаправить на просканированные адреса</translation>
    </message>
    <message>
        <source>The profile for the config test could not be copied</source>
        <translation>Не удалось скопировать профиль для теста конфигурации</translation>
    </message>
    <message>
        <source>Choose the WARP profile to take the identity from</source>
        <translation>Выберите профиль WARP, из которого будет взят идентификатор</translation>
    </message>
    <message>
        <source>This mode needs a MASQUE profile</source>
        <translation>Для этого режима нужен профиль MASQUE</translation>
    </message>
    <message>
        <source>This mode needs a WireGuard profile</source>
        <translation>Для этого режима нужен профиль WireGuard</translation>
    </message>
    <message>
        <source>Register a new identity for this mode</source>
        <translation>Зарегистрируйте новый идентификатор для этого режима</translation>
    </message>
    <message>
        <source>The WARP identity could not be read; register a new one</source>
        <translation>Не удалось прочитать идентификатор WARP; зарегистрируйте новый</translation>
    </message>
    <message>
        <source>Direct</source>
        <translation>Напрямую</translation>
    </message>
</context>
<context>
    <name>QPlatformTheme</name>
    <message>
        <source>Cancel</source>
        <translation>Отмена</translation>
    </message>
    <message>
        <source>Apply</source>
        <translation>Применить</translation>
    </message>
    <message>
        <source>Yes</source>
        <translation>Да</translation>
    </message>
    <message>
        <source>No</source>
        <translation>Нет</translation>
    </message>
    <message>
        <source>OK</source>
        <translation>ОК</translation>
    </message>
</context>
<context>
    <name>Qv2ray::ui::widgets::AutoCompleteTextEdit</name>
    <message>
        <source>You can not input space characters here.</source>
        <translation>Здесь нельзя вводить пробелы.</translation>
    </message>
</context>
<context>
    <name>RawRouteItem</name>
    <message>
        <source>Raw routing profile</source>
        <translation>Raw-профиль маршрутизации</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Название</translation>
    </message>
    <message>
        <source>Use the route object exactly as written (outbound ids are still resolved to tags).
Throne will NOT add its DNS-hijack or xray bridge plumbing, so DNS, chained/xray
outbounds and other Throne features may break. For advanced users only.</source>
        <translation>Использовать объект route точно в том виде, как он написан (ID outbound по-прежнему преобразуются в теги).
Throne НЕ будет добавлять свой перехват DNS (DNS-hijack) и мост xray, поэтому DNS, цепочки/xray
outbound и другие функции Throne могут не работать. Только для опытных пользователей.</translation>
    </message>
    <message>
        <source>Format JSON</source>
        <translation>Форматировать JSON</translation>
    </message>
    <message>
        <source>Raw route</source>
        <translation>Raw-маршрут</translation>
    </message>
    <message>
        <source>The route must be a valid JSON object</source>
        <translation>Маршрут должен быть корректным JSON-объектом</translation>
    </message>
    <message>
        <source>Invalid operation</source>
        <translation>Неверная операция</translation>
    </message>
    <message>
        <source>Cannot create Route Profile with empty name</source>
        <translation>Невозможно создать профиль маршрутизации с пустым названием</translation>
    </message>
    <message>
        <source>Invalid route</source>
        <translation>Неверный маршрут</translation>
    </message>
    <message>
        <source>Prevent modifications</source>
        <translation>Запретить изменения</translation>
    </message>
</context>
<context>
    <name>RemoteApi</name>
    <message>
        <source>The request is not valid JSON: %1</source>
        <translation>The request is not valid JSON: %1</translation>
    </message>
    <message>
        <source>The request is not a JSON object</source>
        <translation>The request is not a JSON object</translation>
    </message>
    <message>
        <source>The method must be GET, POST, PUT or DELETE</source>
        <translation>The method must be GET, POST, PUT or DELETE</translation>
    </message>
    <message>
        <source>The path must start with / and carry no query string or fragment</source>
        <translation>The path must start with / and carry no query string or fragment</translation>
    </message>
    <message>
        <source>The query must be an object</source>
        <translation>The query must be an object</translation>
    </message>
    <message>
        <source>Query value %1 is not a string</source>
        <translation>Query value %1 is not a string</translation>
    </message>
</context>
<context>
    <name>RemoteApi::Router</name>
    <message>
        <source>[API] %1 %2 from %3</source>
        <translation>[API] %1 %2 от %3</translation>
    </message>
    <message>
        <source>[API] %1</source>
        <translation>[API] %1</translation>
    </message>
    <message numerus="yes">
        <source>[API] Testing %n profile(s) to find the lowest latency</source>
        <translation>
            <numerusform>[API] Тестирование %n профиля для поиска наименьшей задержки</numerusform>
            <numerusform>[API] Тестирование %n профилей для поиска наименьшей задержки</numerusform>
            <numerusform>[API] Тестирование %n профилей для поиска наименьшей задержки</numerusform>
        </translation>
    </message>
    <message>
        <source>[API] Lowest latency: %1 (%2 ms)</source>
        <translation>[API] Наименьшая задержка: %1 (%2 мс)</translation>
    </message>
</context>
<context>
    <name>RemoteApi::Server</name>
    <message>
        <source>The request was dropped without an answer</source>
        <translation>Запрос отброшен без ответа</translation>
    </message>
    <message>
        <source>Off</source>
        <translation>Выключен</translation>
    </message>
    <message>
        <source>Not listening: the key is missing or invalid</source>
        <translation>Не принимает подключения: ключ отсутствует или недействителен</translation>
    </message>
    <message>
        <source>Port %1 is in use</source>
        <translation>Порт %1 занят</translation>
    </message>
    <message>
        <source>Cannot listen on port %1: %2</source>
        <translation>Не удалось начать прослушивание порта %1: %2</translation>
    </message>
    <message>
        <source>Listening on port %1 (local network)</source>
        <translation>Прослушивается порт %1 (локальная сеть)</translation>
    </message>
    <message>
        <source>Listening on %1:%2</source>
        <translation>Прослушивается %1:%2</translation>
    </message>
    <message>
        <source>Stopped listening</source>
        <translation>Прослушивание остановлено</translation>
    </message>
    <message>
        <source>Refused %1: too many failed attempts</source>
        <translation>Отклонено подключение %1: слишком много неудачных попыток</translation>
    </message>
    <message>
        <source>Refused %1: not in the allowed addresses</source>
        <translation>Отклонено подключение %1: адрес не входит в разрешенные</translation>
    </message>
    <message>
        <source>Refused %1: too many connections</source>
        <translation>Отклонено подключение %1: слишком много подключений</translation>
    </message>
    <message>
        <source>unsupported protocol version</source>
        <translation>неподдерживаемая версия протокола</translation>
    </message>
    <message>
        <source>invalid frame length</source>
        <translation>недопустимая длина фрейма</translation>
    </message>
    <message>
        <source>a frame failed authentication, most likely the key is wrong</source>
        <translation>фрейм не прошел аутентификацию, скорее всего, ключ неверный</translation>
    </message>
    <message>
        <source>The response is too large</source>
        <translation>Ответ слишком большой</translation>
    </message>
    <message>
        <source>Closed the connection from %1: %2</source>
        <translation>Закрыто подключение от %1: %2</translation>
    </message>
    <message>
        <source>Throne is shutting down</source>
        <translation>Throne завершает работу</translation>
    </message>
</context>
<context>
    <name>RouteItem</name>
    <message>
        <source>Route Profile</source>
        <translation>Профиль маршрутизации</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>Название</translation>
    </message>
    <message>
        <source>Remote source</source>
        <translation>Удаленный источник</translation>
    </message>
    <message>
        <source>URL</source>
        <translation>URL</translation>
    </message>
    <message>
        <source>https://... (throne://route link, base64, or JSON)</source>
        <translation>https://... (throne://route link, base64, или JSON)</translation>
    </message>
    <message>
        <source>Re-fetch this profile from the URL when subscriptions auto-update.
Your local edits are overwritten on each update.</source>
        <translation>Повторно загружать этот профиль из URL при автоматическом обновлении подписок.
Ваши локальные правки перезаписываются при каждом обновлении.</translation>
    </message>
    <message>
        <source>Auto update</source>
        <translation>Автообновление</translation>
    </message>
    <message>
        <source>Fetch the URL and show the routing profile without changing the current rules.</source>
        <translation>Загрузить URL и показать профиль маршрутизации без изменения текущих правил.</translation>
    </message>
    <message>
        <source>Fetch the URL and replace the current rules with the fetched ones.</source>
        <translation>Загрузить URL и заменить текущие правила загруженными.</translation>
    </message>
    <message>
        <source>Fetch</source>
        <translation>Загрузить</translation>
    </message>
    <message>
        <source>Warp-bypass</source>
        <translation>Warp-bypass</translation>
    </message>
    <message>
        <source>Endpoints</source>
        <translation>Конечные точки</translation>
    </message>
    <message>
        <source>Add</source>
        <translation>Добавить</translation>
    </message>
    <message>
        <source>Remove</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Rules</source>
        <translation>Правила</translation>
    </message>
    <message>
        <source>New</source>
        <translation>Создать</translation>
    </message>
    <message>
        <source>Move Up</source>
        <translation>Поднять выше</translation>
    </message>
    <message>
        <source>Move Down</source>
        <translation>Опустить ниже</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>This rule is managed by the Endpoints tab. You can only move it.</source>
        <translation>Это правило управляется вкладкой «Конечные точки». Вы можете только перемещать его.</translation>
    </message>
    <message>
        <source>Preview</source>
        <translation>Предпросмотр</translation>
    </message>
    <message>
        <source>Rule Settings</source>
        <translation>Настройки правила</translation>
    </message>
    <message>
        <source>Action</source>
        <translation>Действие</translation>
    </message>
    <message>
        <source>Basic</source>
        <translation>Основные настройки</translation>
    </message>
    <message>
        <source>How to use</source>
        <translation>Как пользоваться</translation>
    </message>
    <message>
        <source>Direct</source>
        <translation>Напрямую</translation>
    </message>
    <message>
        <source>Proxy</source>
        <translation>Прокси</translation>
    </message>
    <message>
        <source>Block</source>
        <translation>Блокировать</translation>
    </message>
    <message>
        <source>OpenVPN / OpenConnect profiles started alongside this routing profile. Traffic for the networks they advertise goes through them; everything else follows this profile&apos;s rules. Each endpoint gets a rule in the Advanced tab that you can move among your own rules. A chain endpoint can hand out one rule per inner OpenVPN / OpenConnect hop too, so traffic can be routed to a hop before the exit.</source>
        <translation>Профили OpenVPN / OpenConnect, запускаемые вместе с этим профилем маршрутизации. Трафик для сетей, которые они анонсируют, идёт через них; всё остальное подчиняется правилам этого профиля. Для каждой конечной точки на вкладке «Дополнительно» создаётся правило, которое можно перемещать среди ваших правил. Конечная точка-цепочка также может выдавать по правилу на каждое внутреннее звено OpenVPN / OpenConnect, чтобы трафик можно было направить на звено до выхода.</translation>
    </message>
    <message>
        <source>Advanced</source>
        <translation>Дополнительно</translation>
    </message>
    <message>
        <source>Invalid rules</source>
        <translation>Некорректные правила</translation>
    </message>
    <message>
        <source>Some rules could not be added:
</source>
        <translation>Некоторые правила не удалось добавить:
</translation>
    </message>
    <message>
        <source>Simple rule manual</source>
        <translation>Инструкция по простым правилам</translation>
    </message>
    <message>
        <source>Profile #%1 — deleted, dropped when you save</source>
        <translation>Профиль #%1 — удалён, будет убран при сохранении</translation>
    </message>
    <message>
        <source>Allow routing to inner hops</source>
        <translation>Разрешить маршрутизацию на внутренние звенья</translation>
    </message>
    <message>
        <source>Give each OpenVPN/OpenConnect hop behind the exit its own rule, so traffic can be routed to it as well.</source>
        <translation>Создать для каждого звена OpenVPN/OpenConnect перед выходным собственное правило, чтобы трафик можно было направлять и на него.</translation>
    </message>
    <message>
        <source>%1 route prefer</source>
        <translation>%1 — предпочтительный маршрут</translation>
    </message>
    <message>
        <source>Invalid operation</source>
        <translation>Неверная операция</translation>
    </message>
    <message>
        <source>Cannot create Route Profile with empty name</source>
        <translation>Невозможно создать профиль маршрутизации с пустым названием</translation>
    </message>
    <message>
        <source>Empty Route Profile</source>
        <translation>Пустой профиль маршрутизации</translation>
    </message>
    <message>
        <source>No valid rules are in the profile</source>
        <translation>В профиле нет корректных правил</translation>
    </message>
    <message>
        <source>%1 endpoint profile(s) no longer exist and were removed from this routing profile.</source>
        <translation>Профили конечных точек больше не существуют и удалены из этого профиля маршрутизации: %1.</translation>
    </message>
    <message>
        <source>Endpoint rule: move it to choose where the endpoint claims traffic. Managed by the Endpoints tab.</source>
        <translation>Правило для конечной точки: перемещайте его, чтобы выбрать, в каком месте конечная точка забирает трафик. Управляется вкладкой «Конечные точки».</translation>
    </message>
    <message>
        <source>Check attributes to show as tabs; unchecking clears their values.</source>
        <translation>Отметьте атрибуты, чтобы отобразить их во вкладках; снятие отметки очистит их значения.</translation>
    </message>
    <message>
        <source>This rule installs a &apos;preferred by&apos; rule so that the networks advertised by the endpoint %1 get routed into the endpoint tunnel.</source>
        <translation>Это правило добавляет правило &apos;preferred by&apos;, чтобы сети, анонсируемые конечной точкой %1, направлялись в её туннель.</translation>
    </message>
    <message>
        <source>Endpoint rule</source>
        <translation>Правило для конечной точки</translation>
    </message>
    <message>
        <source>This rule belongs to &quot;%1&quot;, an inner hop of the endpoint &quot;%2&quot;.

Stop routing to that endpoint&apos;s inner hops?</source>
        <translation>Это правило относится к &quot;%1&quot; — внутреннему звену конечной точки &quot;%2&quot;.

Отключить маршрутизацию на внутренние звенья этой конечной точки?</translation>
    </message>
    <message>
        <source>This rule belongs to the endpoint &quot;%1&quot; and cannot be deleted on its own.

Remove that endpoint from this routing profile as well?</source>
        <translation>Это правило относится к конечной точке &quot;%1&quot; и не может быть удалено отдельно.

Удалить эту конечную точку из данного профиля маршрутизации?</translation>
    </message>
    <message>
        <source>Some rules could not be added, fix them before saving:
</source>
        <translation>Некоторые правила не удалось добавить, исправьте их перед сохранением:
</translation>
    </message>
    <message>
        <source>General</source>
        <translation>Основное</translation>
    </message>
    <message>
        <source>Default outbound</source>
        <translation>Outbound по умолчанию</translation>
    </message>
    <message>
        <source>%1 — %2 rule(s)</source>
        <translation>%1 — правил: %2</translation>
    </message>
    <message>
        <source>(unnamed)</source>
        <translation>(без названия)</translation>
    </message>
    <message>
        <source>Fetch from remote</source>
        <translation>Загрузить</translation>
    </message>
    <message>
        <source>Invalid URL</source>
        <translation>Неверный URL</translation>
    </message>
    <message>
        <source>Fetching...</source>
        <translation>Загрузка...</translation>
    </message>
    <message>
        <source>Fetched</source>
        <translation>Загружено</translation>
    </message>
    <message>
        <source>Remote routing profile preview</source>
        <translation>Предпросмотр удалённого профиля</translation>
    </message>
    <message>
        <source>This will replace the current rules with the ones fetched from the URL. Continue?</source>
        <translation>Текущие правила будут заменены правилами, загруженными по URL. Продолжить?</translation>
    </message>
    <message>
        <source>Enter a valid http(s) URL first.</source>
        <translation>Сначала введите корректный http(s) URL.</translation>
    </message>
    <message>
        <source>Remote routing profiles need a URL.</source>
        <translation>Для удалённых профилей маршрутизации необходим URL.</translation>
    </message>
    <message>
        <source>Could not fetch routing profile</source>
        <translation>Не удалось загрузить профиль маршрутизации</translation>
    </message>
    <message>
        <source>Loaded %1 rule(s) from the remote URL.</source>
        <translation>Загружено правил по URL: %1.</translation>
    </message>
    <message>
        <source>Fetched with warnings</source>
        <translation>Загружено с предупреждениями</translation>
    </message>
</context>
<context>
    <name>RuntimeStatsWidget</name>
    <message>
        <source>%1 active   ·   %2 TCP   ·   %3 UDP</source>
        <translation>Активных: %1   ·   TCP: %2   ·   UDP: %3</translation>
    </message>
    <message>
        <source>No active config</source>
        <translation>Нет активного подключения</translation>
    </message>
    <message>
        <source>CPU</source>
        <translation>ЦП</translation>
    </message>
    <message>
        <source>RAM</source>
        <translation>ОЗУ</translation>
    </message>
    <message>
        <source>Disabled</source>
        <translation>Выключено</translation>
    </message>
    <message>
        <source>Due now</source>
        <translation>Прямо сейчас</translation>
    </message>
    <message>
        <source>in %1</source>
        <translation>через %1</translation>
    </message>
    <message>
        <source>Details</source>
        <translation>Подробности</translation>
    </message>
    <message>
        <source>Unavailable</source>
        <translation>Недоступен</translation>
    </message>
    <message>
        <source>N/A</source>
        <translation>Недоступно</translation>
    </message>
    <message>
        <source>Process</source>
        <translation>Процесс</translation>
    </message>
    <message>
        <source>Core</source>
        <translation>Ядро</translation>
    </message>
    <message>
        <source>Network</source>
        <translation>Сеть</translation>
    </message>
    <message>
        <source>Connections</source>
        <translation>Подключения</translation>
    </message>
    <message>
        <source>Proxy</source>
        <translation>Прокси</translation>
    </message>
    <message>
        <source>Direct</source>
        <translation>Напрямую</translation>
    </message>
    <message>
        <source>Next sub update</source>
        <translation>След. обновл. подписки</translation>
    </message>
    <message>
        <source>Next remote route update</source>
        <translation>След. обновл. удаленной маршрутизации</translation>
    </message>
    <message>
        <source>System</source>
        <translation>Система</translation>
    </message>
    <message>
        <source>Databases</source>
        <translation>Базы данных</translation>
    </message>
    <message>
        <source>Uptime</source>
        <translation>Время работы</translation>
    </message>
    <message>
        <source>Running Config</source>
        <translation>Текущая конфигурация</translation>
    </message>
    <message>
        <source>Country</source>
        <translation>Страна</translation>
    </message>
    <message>
        <source>Config</source>
        <translation>Конфигурация</translation>
    </message>
    <message>
        <source>Out IP</source>
        <translation>Выходной IP</translation>
    </message>
    <message>
        <source>Ping</source>
        <translation>Задержка</translation>
    </message>
    <message>
        <source>Endpoint</source>
        <translation>Конечная точка</translation>
    </message>
    <message>
        <source>State</source>
        <translation>Состояние</translation>
    </message>
</context>
<context>
    <name>ScanItem</name>
    <message numerus="yes">
        <source>Starting over clears the %n result(s) this scan has found. Continue?</source>
        <translation>
            <numerusform>Перезапуск с начала удалит %n результат, найденный этим сканированием. Продолжить?</numerusform>
            <numerusform>Перезапуск с начала удалит %n результата, найденных этим сканированием. Продолжить?</numerusform>
            <numerusform>Перезапуск с начала удалит %n результатов, найденных этим сканированием. Продолжить?</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>This scan was paused partway. Starting over discards its progress and clears its %n result(s). Continue?</source>
        <translation>
            <numerusform>Это сканирование было приостановлено на полпути. Перезапуск с начала сбросит его прогресс и удалит %n результат. Продолжить?</numerusform>
            <numerusform>Это сканирование было приостановлено на полпути. Перезапуск с начала сбросит его прогресс и удалит %n результата. Продолжить?</numerusform>
            <numerusform>Это сканирование было приостановлено на полпути. Перезапуск с начала сбросит его прогресс и удалит %n результатов. Продолжить?</numerusform>
        </translation>
    </message>
    <message>
        <source>This scan was paused partway. Starting over discards its progress. Continue?</source>
        <translation>Сканирование было приостановлено. Если начать заново, его прогресс будет потерян. Продолжить?</translation>
    </message>
    <message>
        <source>IP Scanner</source>
        <translation>IP-сканер</translation>
    </message>
    <message numerus="yes">
        <source>The targets changed since this scan was paused, so it cannot resume. Start over from the beginning and clear its %n result(s)?</source>
        <translation>
            <numerusform>Цели изменились после приостановки сканирования, поэтому продолжить его нельзя. Начать заново и удалить %n результат?</numerusform>
            <numerusform>Цели изменились после приостановки сканирования, поэтому продолжить его нельзя. Начать заново и удалить %n результата?</numerusform>
            <numerusform>Цели изменились после приостановки сканирования, поэтому продолжить его нельзя. Начать заново и удалить %n результатов?</numerusform>
        </translation>
    </message>
    <message>
        <source>The targets changed since this scan was paused, so it cannot resume. Start over from the beginning?</source>
        <translation>Цели изменились после приостановки сканирования, поэтому продолжить его нельзя. Начать заново?</translation>
    </message>
    <message>
        <source>The scan could not be started.</source>
        <translation>Не удалось запустить сканирование.</translation>
    </message>
    <message>
        <source>Pausing…</source>
        <translation>Приостановка…</translation>
    </message>
    <message>
        <source>Running</source>
        <translation>Выполняется</translation>
    </message>
    <message>
        <source>Idle</source>
        <translation>Не запускалось</translation>
    </message>
    <message>
        <source>Paused</source>
        <translation>Приостановлено</translation>
    </message>
    <message>
        <source>Completed</source>
        <translation>Завершено</translation>
    </message>
    <message>
        <source>Failed</source>
        <translation>Ошибка</translation>
    </message>
    <message>
        <source>%1 / %2 (%3%)</source>
        <translation>%1 / %2 (%3%)</translation>
    </message>
    <message numerus="yes">
        <source>%n found</source>
        <translation>
            <numerusform>найден %n</numerusform>
            <numerusform>найдено %n</numerusform>
            <numerusform>найдено %n</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>%n removed</source>
        <translation>
            <numerusform>удален %n</numerusform>
            <numerusform>удалено %n</numerusform>
            <numerusform>удалено %n</numerusform>
        </translation>
    </message>
    <message>
        <source>WARP</source>
        <translation>WARP</translation>
    </message>
    <message>
        <source>Generic</source>
        <translation>Обычное</translation>
    </message>
    <message>
        <source>Open</source>
        <translation>Открыть</translation>
    </message>
    <message>
        <source>Delete</source>
        <translation>Удалить</translation>
    </message>
    <message>
        <source>Pause</source>
        <translation>Приостановить</translation>
    </message>
    <message>
        <source>Resume</source>
        <translation>Продолжить</translation>
    </message>
    <message>
        <source>Start</source>
        <translation>Запустить</translation>
    </message>
</context>
<context>
    <name>Scanner::IpListUpdater</name>
    <message>
        <source>The IP list no longer exists</source>
        <translation>Список IP больше не существует</translation>
    </message>
    <message>
        <source>Only lists with a URL or rule-set source can be updated</source>
        <translation>Обновлять можно только списки с источником «URL» или «Набор правил»</translation>
    </message>
    <message>
        <source>Updating IP list %1</source>
        <translation>Обновление списка IP %1</translation>
    </message>
    <message>
        <source>Updating IP list %1 failed: %2</source>
        <translation>Не удалось обновить список IP %1: %2</translation>
    </message>
    <message>
        <source>its source changed during the update</source>
        <translation>его источник изменился во время обновления</translation>
    </message>
    <message>
        <source>The source changed during the update</source>
        <translation>Источник изменился во время обновления</translation>
    </message>
    <message>
        <source>The entries could not be saved</source>
        <translation>Не удалось сохранить записи</translation>
    </message>
    <message numerus="yes">
        <source>IP list %1 updated (%n entries)</source>
        <translation>
            <numerusform>Список IP %1 обновлен (%n запись)</numerusform>
            <numerusform>Список IP %1 обновлен (%n записи)</numerusform>
            <numerusform>Список IP %1 обновлен (%n записей)</numerusform>
        </translation>
    </message>
    <message>
        <source>The list has no source to fetch</source>
        <translation>У списка нет источника для загрузки</translation>
    </message>
    <message>
        <source>Unknown rule-set: %1</source>
        <translation>Неизвестный набор правил: %1</translation>
    </message>
    <message>
        <source>The source must be an http(s) URL</source>
        <translation>Источником должен быть URL http(s)</translation>
    </message>
    <message>
        <source>The core is not running</source>
        <translation>Ядро не запущено</translation>
    </message>
    <message>
        <source>The core could not read the rule-set</source>
        <translation>Ядру не удалось прочитать набор правил</translation>
    </message>
    <message>
        <source>The address returned a web page instead of an IP list</source>
        <translation>По этому адресу находится веб-страница, а не список IP</translation>
    </message>
    <message>
        <source>No addresses found</source>
        <translation>Адреса не найдены</translation>
    </message>
    <message>
        <source>IP list update failed: %1</source>
        <translation>Не удалось обновить список IP: %1</translation>
    </message>
    <message>
        <source>IP list update failed</source>
        <translation>Не удалось обновить список IP</translation>
    </message>
</context>
<context>
    <name>Scanner::ScanManager</name>
    <message>
        <source>The scan is already running</source>
        <translation>Сканирование уже выполняется</translation>
    </message>
    <message>
        <source>The scan no longer exists</source>
        <translation>Сканирование больше не существует</translation>
    </message>
    <message>
        <source>Turn on at least one test phase</source>
        <translation>Включите хотя бы один этап проверки</translation>
    </message>
    <message>
        <source>A TLS handshake check needs TLS turned on</source>
        <translation>Для проверки TLS-хендшейка нужно включить TLS</translation>
    </message>
    <message>
        <source>HTTP/3 needs TLS turned on</source>
        <translation>Для HTTP/3 нужно включить TLS</translation>
    </message>
    <message>
        <source>The profile for the config test cannot be used</source>
        <translation>Профиль для проверки конфигурации нельзя использовать</translation>
    </message>
    <message>
        <source>Choose the IP list to scan</source>
        <translation>Выберите список IP для сканирования</translation>
    </message>
    <message>
        <source>The IP list to scan is empty</source>
        <translation>Список IP для сканирования пуст</translation>
    </message>
    <message>
        <source>The result list is empty</source>
        <translation>Список результатов пуст</translation>
    </message>
    <message>
        <source>Pause the scan first</source>
        <translation>Сначала приостановите сканирование</translation>
    </message>
    <message>
        <source>Scan %1 Result</source>
        <translation>Результат сканирования %1</translation>
    </message>
    <message>
        <source>config test</source>
        <translation>проверка конфигурации</translation>
    </message>
    <message>
        <source>Scanning %1 with %2</source>
        <translation>Сканирование %1 (%2)</translation>
    </message>
    <message>
        <source>%1 failed %2: %3</source>
        <translation>%1 — сбой (%2): %3</translation>
    </message>
    <message>
        <source>Connected to %1 with TCP (%2 ms)</source>
        <translation>TCP-подключение к %1 установлено (%2 мс)</translation>
    </message>
    <message>
        <source>%1 replied to ping (%2 ms)</source>
        <translation>%1 ответил на пинг (%2 мс)</translation>
    </message>
    <message>
        <source>TLS handshake with %1 succeeded (%2 ms)</source>
        <translation>TLS-хендшейк с %1 выполнен (%2 мс)</translation>
    </message>
    <message>
        <source>%1 answered over HTTP (%2 ms)</source>
        <translation>%1 ответил по HTTP (%2 мс)</translation>
    </message>
    <message>
        <source>%1 passed the config test (%2 ms)</source>
        <translation>%1 прошел проверку конфигурации (%2 мс)</translation>
    </message>
    <message>
        <source>Preparing</source>
        <translation>Подготовка</translation>
    </message>
    <message>
        <source>The scan stopped on an unexpected error</source>
        <translation>Сканирование остановлено из-за непредвиденной ошибки</translation>
    </message>
    <message>
        <source>Scan %1: saving its final state failed</source>
        <translation>Сканирование %1: не удалось сохранить итоговое состояние</translation>
    </message>
    <message>
        <source>Preparing targets…</source>
        <translation>Подготовка целей…</translation>
    </message>
    <message>
        <source>The scan has no profile for the config test</source>
        <translation>У сканирования нет профиля для проверки конфигурации</translation>
    </message>
    <message>
        <source>The result list could not be created</source>
        <translation>Не удалось создать список результатов</translation>
    </message>
    <message>
        <source>The base IP list no longer exists</source>
        <translation>Базовый список IP больше не существует</translation>
    </message>
    <message>
        <source>The base IP list is empty</source>
        <translation>Базовый список IP пуст</translation>
    </message>
    <message>
        <source>The base IP list holds only IPv6 ranges; enable IPv6 scanning</source>
        <translation>Базовый список IP содержит только диапазоны IPv6; включите сканирование IPv6</translation>
    </message>
    <message>
        <source>%1 snapshot</source>
        <translation>%1 (снимок)</translation>
    </message>
    <message>
        <source>The base IP list could not be copied</source>
        <translation>Не удалось скопировать базовый список IP</translation>
    </message>
    <message>
        <source>The base IP list holds no valid target</source>
        <translation>Базовый список IP не содержит допустимых целей</translation>
    </message>
    <message>
        <source>The result list holds only IPv6 entries; enable IPv6 scanning</source>
        <translation>Список результатов содержит только записи IPv6; включите сканирование IPv6</translation>
    </message>
    <message>
        <source>%1 rescan snapshot</source>
        <translation>%1 (снимок для повторного сканирования)</translation>
    </message>
    <message>
        <source>The result list could not be copied</source>
        <translation>Не удалось скопировать список результатов</translation>
    </message>
    <message>
        <source>The result list holds no valid target</source>
        <translation>Список результатов не содержит допустимых целей</translation>
    </message>
    <message>
        <source>The copy of the result list is gone; rescan the results again</source>
        <translation>Копия списка результатов пропала; повторите сканирование результатов</translation>
    </message>
    <message>
        <source>The copy of the base IP list is gone; restart the scan</source>
        <translation>Копия базового списка IP пропала; перезапустите сканирование</translation>
    </message>
    <message numerus="yes">
        <source>Found %n result(s); stopping as configured</source>
        <translation>
            <numerusform>Найден %n результат; остановка согласно настройкам</numerusform>
            <numerusform>Найдено %n результата; остановка согласно настройкам</numerusform>
            <numerusform>Найдено %n результатов; остановка согласно настройкам</numerusform>
        </translation>
    </message>
    <message>
        <source>IPv6 is unreachable from this network; scan paused (turn off IPv6 scanning to go on)</source>
        <translation>IPv6 недоступен из этой сети; сканирование приостановлено (чтобы продолжить, отключите сканирование IPv6)</translation>
    </message>
    <message>
        <source>Network appears to be down; scan paused</source>
        <translation>Похоже, сеть недоступна; сканирование приостановлено</translation>
    </message>
    <message>
        <source>Scan %1: network check failed: %2</source>
        <translation>Сканирование %1: ошибка проверки сети: %2</translation>
    </message>
    <message>
        <source>IPv4 is unreachable from this network; IPv4 results were kept</source>
        <translation>IPv4 недоступен из этой сети; результаты IPv4 сохранены</translation>
    </message>
    <message>
        <source>IPv6 is unreachable from this network; IPv6 results were kept</source>
        <translation>IPv6 недоступен из этой сети; результаты IPv6 сохранены</translation>
    </message>
    <message>
        <source>The core ended the scan session early</source>
        <translation>Ядро досрочно завершило сеанс сканирования</translation>
    </message>
    <message>
        <source>The core made no progress on the scan</source>
        <translation>Сканирование в ядре не продвинулось</translation>
    </message>
    <message numerus="yes">
        <source>%n result(s) passed only on a retry; the network fails some attempts at random</source>
        <translation>
            <numerusform>%n результат прошел проверку только с повторной попытки; сеть случайным образом обрывает часть попыток</numerusform>
            <numerusform>%n результата прошли проверку только с повторной попытки; сеть случайным образом обрывает часть попыток</numerusform>
            <numerusform>%n результатов прошли проверку только с повторной попытки; сеть случайным образом обрывает часть попыток</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <source>Re-testing %n failed result(s)…</source>
        <translation>
            <numerusform>Повторная проверка %n неудачного результата…</numerusform>
            <numerusform>Повторная проверка %n неудачных результатов…</numerusform>
            <numerusform>Повторная проверка %n неудачных результатов…</numerusform>
        </translation>
    </message>
    <message>
        <source>The core did not answer the probe request</source>
        <translation>Ядро не ответило на запрос проверки</translation>
    </message>
    <message numerus="yes">
        <source>Testing %n candidate(s) through the config…</source>
        <translation>
            <numerusform>Проверка %n кандидата через конфигурацию…</numerusform>
            <numerusform>Проверка %n кандидатов через конфигурацию…</numerusform>
            <numerusform>Проверка %n кандидатов через конфигурацию…</numerusform>
        </translation>
    </message>
    <message>
        <source>The core did not answer the config test</source>
        <translation>Ядро не ответило на проверку конфигурации</translation>
    </message>
    <message>
        <source>Checking the network…</source>
        <translation>Проверка сети…</translation>
    </message>
    <message>
        <source>Scan %1 stopped: %2</source>
        <translation>Сканирование %1 остановлено: %2</translation>
    </message>
    <message numerus="yes">
        <source>Skipped %n invalid entry(ies)</source>
        <translation>
            <numerusform>Пропущена %n недопустимая запись</numerusform>
            <numerusform>Пропущено %n недопустимые записи</numerusform>
            <numerusform>Пропущено %n недопустимых записей</numerusform>
        </translation>
    </message>
    <message>
        <source>Probing</source>
        <translation>Проверка адресов</translation>
    </message>
    <message>
        <source>Testing configs</source>
        <translation>Проверка конфигураций</translation>
    </message>
    <message>
        <source>Checking network</source>
        <translation>Проверка сети</translation>
    </message>
</context>
<context>
    <name>SpeedWidget</name>
    <message>
        <source>Proxy</source>
        <translation>Прокси</translation>
    </message>
    <message>
        <source>Direct</source>
        <translation>Напрямую</translation>
    </message>
</context>
<context>
    <name>StartStopButton</name>
    <message>
        <source>Select a profile to start</source>
        <translation>Выберите профиль для подключения</translation>
    </message>
    <message>
        <source>Start</source>
        <translation>Запустить</translation>
    </message>
    <message>
        <source>Connecting…</source>
        <translation>Подключение…</translation>
    </message>
    <message>
        <source>Stop</source>
        <translation>Остановить</translation>
    </message>
    <message>
        <source>Stopping…</source>
        <translation>Отключение…</translation>
    </message>
    <message>
        <source>Kill switch: starting…</source>
        <translation>Kill switch: запускается…</translation>
    </message>
    <message>
        <source>Kill switch: Tun is not running, so most traffic is blocked</source>
        <translation>Kill switch: TUN не запущен, поэтому большая часть трафика блокируется</translation>
    </message>
    <message>
        <source>Kill switch: on — traffic only flows through Throne&apos;s Tun</source>
        <translation>Kill switch: включен — трафик идет только через TUN Throne</translation>
    </message>
    <message>
        <source>Kill switch is not active</source>
        <translation>Kill switch не активен</translation>
    </message>
</context>
<context>
    <name>SubscriptionInfoCard</name>
    <message>
        <source>Portal</source>
        <translation>Портал</translation>
    </message>
    <message>
        <source>Support</source>
        <translation>Поддержка</translation>
    </message>
    <message>
        <source>Expired</source>
        <translation>Истекла</translation>
    </message>
    <message>
        <source>%1m left</source>
        <translation>Осталось %1 мин.</translation>
    </message>
    <message>
        <source>%1h left</source>
        <translation>Осталось %1 ч.</translation>
    </message>
    <message>
        <source>%1d left</source>
        <translation>Осталось %1 д.</translation>
    </message>
    <message>
        <source>Announcement - %1</source>
        <translation>Объявление - %1</translation>
    </message>
    <message>
        <source>Auto-update: every %1</source>
        <translation>Автообновление: раз в %1</translation>
    </message>
    <message>
        <source>Provider: %1</source>
        <translation>Провайдер: %1</translation>
    </message>
    <message>
        <source>Last updated: %1</source>
        <translation>Последнее обновление: %1</translation>
    </message>
    <message>
        <source>Expires: %1</source>
        <translation>Истекает: %1</translation>
    </message>
    <message>
        <source>Interval provided by the server</source>
        <translation>Интервал, установленный сервером</translation>
    </message>
    <message>
        <source>Interval set for this group</source>
        <translation>Интервал, заданный для этой группы</translation>
    </message>
    <message>
        <source>Global interval from Settings</source>
        <translation>Глобальный интервал из настроек</translation>
    </message>
    <message numerus="yes">
        <source>Server suggests every %n hour(s)</source>
        <translation>
            <numerusform>Сервер предлагает каждый %n час</numerusform>
            <numerusform>Сервер предлагает каждые %n часа</numerusform>
            <numerusform>Сервер предлагает каждые %n часов</numerusform>
        </translation>
    </message>
    <message>
        <source>Every %1</source>
        <translation>Раз в %1</translation>
    </message>
    <message>
        <source>Website / Portal: %1</source>
        <translation>Веб-сайт / Портал: %1</translation>
    </message>
    <message>
        <source>Technical Support: %1</source>
        <translation>Техническая поддержка: %1</translation>
    </message>
</context>
<context>
    <name>Sys::CoreDiagnostics</name>
    <message>
        <source>The core is not running.</source>
        <translation>Ядро не запущено.</translation>
    </message>
    <message>
        <source>Could not reach the core. See the log for details.</source>
        <translation>Не удалось установить связь с ядром. Подробности смотрите в логах.</translation>
    </message>
    <message>
        <source>The core could not capture the profile: %1</source>
        <translation>Ядру не удалось выполнить профилирование: %1</translation>
    </message>
    <message>
        <source>The core returned an empty profile.</source>
        <translation>Ядро вернуло пустой результат профилирования.</translation>
    </message>
    <message>
        <source>Could not save %1: %2</source>
        <translation>Не удалось сохранить %1: %2</translation>
    </message>
    <message>
        <source>Performance profile saved to %1</source>
        <translation>Результат профилирования сохранен в %1</translation>
    </message>
    <message>
        <source>The performance profile was saved (%1).</source>
        <translation>Результат профилирования сохранен (%1).</translation>
    </message>
    <message>
        <source>Performance profile failed: %1</source>
        <translation>Ошибка профилирования: %1</translation>
    </message>
    <message>
        <source>Performance profile</source>
        <translation>Профилирование</translation>
    </message>
    <message>
        <source>Show in folder</source>
        <translation>Показать в папке</translation>
    </message>
</context>
<context>
    <name>Sys::KillSwitch</name>
    <message>
        <source>The kill switch needs Throne to run as administrator.</source>
        <translation>Для kill switch нужно запустить Throne от имени администратора.</translation>
    </message>
    <message>
        <source>The kill switch needs the core to have root privileges.</source>
        <translation>Для kill switch ядру нужны права root.</translation>
    </message>
    <message>
        <source>The kill switch is not supported on this system.</source>
        <translation>Kill switch не поддерживается в этой системе.</translation>
    </message>
    <message>
        <source>Another program removed the kill switch rules or disabled the system firewall.</source>
        <translation>Другая программа удалила правила kill switch или отключила системный брандмауэр.</translation>
    </message>
    <message>
        <source>The kill switch rejected its settings.</source>
        <translation>Kill switch не принял настройки.</translation>
    </message>
    <message>
        <source>The kill switch process stopped unexpectedly.</source>
        <translation>Процесс kill switch неожиданно остановился.</translation>
    </message>
    <message>
        <source>Details: %1</source>
        <translation>Подробности: %1</translation>
    </message>
    <message>
        <source>[Kill switch] Off.</source>
        <translation>[Kill switch] Выключен.</translation>
    </message>
    <message>
        <source>[Kill switch] Allowing the extra core %1.</source>
        <translation>[Kill switch] Разрешено дополнительное ядро %1.</translation>
    </message>
    <message>
        <source>[Kill switch] Warning: %1</source>
        <translation>[Kill switch] Предупреждение: %1</translation>
    </message>
    <message>
        <source>[Kill switch] On: only traffic through Throne can leave this device.</source>
        <translation>[Kill switch] Включен: покинуть это устройство может только трафик через Throne.</translation>
    </message>
    <message>
        <source>[Kill switch] New settings applied.</source>
        <translation>[Kill switch] Новые настройки применены.</translation>
    </message>
    <message>
        <source>the kill switch did not respond within %1 seconds</source>
        <translation>kill switch не ответил в течение %1 с</translation>
    </message>
    <message>
        <source>could not start %1: %2</source>
        <translation>не удалось запустить %1: %2</translation>
    </message>
    <message>
        <source>the kill switch process crashed</source>
        <translation>процесс kill switch аварийно завершился</translation>
    </message>
    <message>
        <source>the kill switch process exited with code %1</source>
        <translation>процесс kill switch завершился с кодом %1</translation>
    </message>
    <message>
        <source>the kill switch process crashed before it was ready</source>
        <translation>процесс kill switch аварийно завершился до готовности</translation>
    </message>
    <message>
        <source>the kill switch process exited with code %1 before it was ready</source>
        <translation>процесс kill switch завершился с кодом %1 до готовности</translation>
    </message>
    <message>
        <source>[Kill switch] The new settings could not be applied, the previous ones stay in force: %1</source>
        <translation>[Kill switch] Не удалось применить новые настройки, продолжают действовать прежние: %1</translation>
    </message>
    <message>
        <source>[Kill switch] Protection lost: %1</source>
        <translation>[Kill switch] Защита потеряна: %1</translation>
    </message>
    <message>
        <source>[Kill switch] Starting it again...</source>
        <translation>[Kill switch] Повторный запуск...</translation>
    </message>
    <message>
        <source>[Kill switch] Not active: %1</source>
        <translation>[Kill switch] Не активен: %1</translation>
    </message>
</context>
<context>
    <name>TrafficChartWidget</name>
    <message>
        <source>Download</source>
        <translation>Получено (↓)</translation>
    </message>
    <message>
        <source>Upload</source>
        <translation>Отправлено (↑)</translation>
    </message>
    <message>
        <source>No traffic recorded for this period</source>
        <translation>Нет данных о трафике за этот период</translation>
    </message>
</context>
<context>
    <name>TrayOtpCodes</name>
    <message>
        <source>No OTP profiles yet</source>
        <translation>Профилей OTP пока нет</translation>
    </message>
    <message>
        <source>No matches</source>
        <translation>Нет совпадений</translation>
    </message>
    <message>
        <source>Invalid secret</source>
        <translation>Неверный Secret</translation>
    </message>
    <message>
        <source>Copied</source>
        <translation>Скопировано</translation>
    </message>
</context>
<context>
    <name>TrayPopupFrame</name>
    <message>
        <source>Search…</source>
        <translation>Поиск…</translation>
    </message>
    <message>
        <source>Close</source>
        <translation>Закрыть</translation>
    </message>
</context>
<context>
    <name>TrayProfileSelector</name>
    <message>
        <source>Back to groups</source>
        <translation>Назад к группам</translation>
    </message>
    <message>
        <source>Select Routing</source>
        <translation>Выбор маршрутизации</translation>
    </message>
    <message>
        <source>No matches</source>
        <translation>Нет совпадений</translation>
    </message>
    <message>
        <source>No routing profiles</source>
        <translation>Нет профилей маршрутизации</translation>
    </message>
    <message>
        <source>Select Server</source>
        <translation>Выбор сервера</translation>
    </message>
    <message>
        <source>Stop: %1</source>
        <translation>Остановить: %1</translation>
    </message>
    <message>
        <source>No servers</source>
        <translation>Нет серверов</translation>
    </message>
    <message>
        <source>Page %1/%2</source>
        <translation>Страница %1/%2</translation>
    </message>
</context>
</TS>
