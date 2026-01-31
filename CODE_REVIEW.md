# Отчёт о ревизии кода Throne

**Дата:** 1 февраля 2025  
**Проект:** Throne (Qt/Sing-box proxy GUI)  
**Область проверки:** C++ (src/, include/), Go (core/), CMake, общая архитектура

---

## Критические проблемы

### 1. Возможный краш и утечка в `mainwindow_rpc.cpp` (runURLTest, runSpeedTest)

**Файл:** `src/ui/mainwindow_rpc.cpp`

При раннем выходе (`return` при `!rpcOK || result.results.empty()`) главный поток не вызывает `done->unlock()` / `doneMu->unlock()` перед выходом:

- **runURLTest** (строки 104–108): при `return` mutex остаётся заблокированным.
- **runSpeedTest** (строки 398–403): та же ситуация.

**Последствия:**
- Worker‑поток бесконечно ждёт `try_lock()` и не завершается.
- Утечка mutex (`delete` в worker никогда не выполнится).
- Потенциальная утечка потока.

**Рекомендация:**
- Всегда вызывать `done->unlock()` / `doneMu->unlock()` перед любым `return` в этих функциях.
- Или перейти на `std::shared_ptr<QMutex>` / RAII и единую точку выхода.

---

### 2. Потенциальный null pointer dereference в `LoadProxyEntity`

**Файл:** `src/dataStore/Database.cpp`, строки 230–236

```cpp
if (!QString2QJsonObject(QString(fileContent.toStdString().c_str())).contains("outbound"))
{
    migrateBeanToOutbound(ent);
    ent->Save();
}
ent->_bean = nullptr;
return ent;
```

При `validJson == false` или `validType == false` переменная `ent` остаётся `nullptr`, но используется без проверки.

**Рекомендация:**
- Обернуть блок миграции в `if (ent)`.
- Или выполнять миграцию только внутри блока `if (validType)`.

---

### 3. Утечка памяти в RouteItem.cpp

**Файл:** `src/ui/setting/RouteItem.cpp`, строки 208–214

```cpp
auto err = new QString;
auto parsed = Configs::RoutingChain::parseJsonArray(QString2QJsonObject(tEdit->toPlainText()), err);
if (!err->isEmpty()) {
    ...
}
// err никогда не удаляется
```

**Рекомендация:**
- Заменить на `QString err;` и передавать `&err` в `parseJsonArray`.

---

## Важные замечания

### 4. Дублирование исходников в CMakeLists.txt

**Файл:** `CMakeLists.txt`

Один и тот же код указан дважды:
- `src/sys/AutoRun.cpp`, `src/sys/Process.cpp` (строки 116–117 и 226–227)
- `edit_socks`, `edit_http`, `edit_trojan`, `edit_hysteria`, `edit_tuic` (строки 157–167 и 279–293)

**Рекомендация:**
- Удалить дубликаты из `PROJECT_SOURCES`.

---

### 5. Небезопасный QLocalServer

**Файл:** `src/main.cpp`, строка 248

```cpp
server.setSocketOptions(QLocalServer::WorldAccessOption);
```

`WorldAccessOption` даёт доступ к сокету всем пользователям системы.

**Рекомендация:**
- Рассмотреть `UserAccessOption` или `GroupAccessOption`.
- Добавить проверку прав и, при необходимости, ограничение доступа (ACL).

---

### 6. Магическое число вместо константы

**Файл:** `src/ui/mainwindow.cpp`, `edit_chain.cpp`, `dialog_manage_groups.cpp`

Число `114514` используется как role для хранения ID в `QListWidgetItem`:

```cpp
item->data(114514).toInt()
item->setData(114514, profileId);
```

**Рекомендация:**
- Ввести именованную константу, например:
  ```cpp
  constexpr int PROXY_ID_ROLE = 114514;  // или Qt::UserRole + N
  ```

---

### 7. Поля паролей без маскирования

**Файл:** `include/ui/profile/edit_ssh.ui`

Поля `password` и `private_key_pass` объявлены как обычные `QLineEdit` без `echoMode = Password`.

**Рекомендация:**
- Установить `echoMode="Password"` для полей паролей в UI или в коде.

---

### 8. Лишнее преобразование QString

**Файл:** `src/dataStore/Database.cpp`, строка 230

```cpp
QString2QJsonObject(QString(fileContent.toStdString().c_str()))
```

`fileContent` уже `QString`, преобразование через `std::string` избыточно.

**Рекомендация:**
- Заменить на `QString2QJsonObject(fileContent)`.

---

## Рекомендации по архитектуре и качеству

### 9. Глобальные синглтоны

Используются глобальные указатели:
- `Configs::dataStore`
- `Configs::profileManager`
- `ThemeManager *themeManager`
- `Stats::trafficLooper`
- `ConnectionLister* connection_lister`
- и др.

**Рекомендация:**
- Постепенно заменить на dependency injection или явные фасады.
- Это упростит тестирование и уменьшит coupling.

---

### 10. Слабая криптография для single-instance

**Файл:** `src/main.cpp`, строки 233–235

```cpp
QCryptographicHash::hash(wd.absolutePath().toUtf8(), QCryptographicHash::Md5)
```

MD5 не рекомендуется для криптографических целей (хотя здесь используется для имени сокета, а не для подписи).

**Рекомендация:**
- Использовать SHA-256 или другой современный хеш, если важна устойчивость к коллизиям.

---

### 11. Потенциальная гонка в lambda `[=,this]`

В нескольких lambda используется `[=,this]`, что копирует указатели и может привести к использованию уже освобождённых объектов.

**Рекомендация:**
- Проверить жизненный цикл захваченных объектов.
- При необходимости использовать `[this]` или `[&]` только там, где гарантирован короткий жизненный цикл.

---

### 12. Group::GetProfileEnts() возвращает null в списке

**Файл:** `src/dataStore/Group.cpp`, строки 27–35

```cpp
for (auto id : profiles) {
    res.append(profileManager->GetProfile(id));  // может быть nullptr
}
```

`GetProfile(id)` может вернуть `nullptr`, в список попадают null-указатели.

**Рекомендация:**
- Добавить проверку `if (auto p = profileManager->GetProfile(id)) res.append(p);`.

---

## Плюсы проекта

- Структурированная организация кода (configs, ui, dataStore, sys).
- Логичное использование shared_ptr для сущностей.
- Поддержка нескольких платформ (Windows, Linux, macOS).
- Проверка SSL и валидация параметров в `HTTPRequestHelper`.
- Корректная обработка ошибок в Go-части (core/server).

---

## Приоритеты исправлений

| # | Проблема                       | Приоритет | Сложность |
|---|--------------------------------|-----------|-----------|
| 1 | Unlock mutex перед return      | Критично  | Низкая    |
| 2 | Проверка ent в LoadProxyEntity | Критично  | Низкая    |
| 3 | Утечка QString в RouteItem     | Высокий   | Низкая    |
| 4 | Дубли в CMakeLists.txt         | Средний   | Низкая    |
| 5 | WorldAccessOption              | Средний   | Низкая    |
| 6 | Константа вместо 114514        | Низкий    | Низкая    |
| 7 | echoMode для паролей           | Средний   | Низкая    |
| 8 | Упростить fileContent          | Низкий    | Низкая    |
