#pragma once

#include "include/global/Configs.hpp"
#include "include/dataStore/Database.hpp"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/stats/traffic/TrafficLooper.hpp"
#include "include/stats/connections/connectionLister.hpp"

namespace Configs {

/**
 * @brief Централизованный контекст приложения для управления глобальными синглтонами
 * 
 * Цель: постепенно заменить глобальные указатели на dependency injection через AppContext.
 * Это упростит тестирование и уменьшит coupling между компонентами.
 * 
 * Текущее состояние: базовая структура. Полный рефакторинг требует постепенной миграции.
 */
class AppContext {
public:
    static AppContext& instance() {
        static AppContext ctx;
        return ctx;
    }

    // Доступ к глобальным синглтонам
    DataStore* dataStore() const { return Configs::dataStore; }
    ProfileManager* profileManager() const { return Configs::profileManager; }
    ThemeManager* themeManager() const { return ThemeManager::instance(); }
    Stats::TrafficLooper* trafficLooper() const { return Stats::trafficLooper; }
    ConnectionLister* connectionLister() const { return Stats::connection_lister; }

private:
    AppContext() = default;
    ~AppContext() = default;
    AppContext(const AppContext&) = delete;
    AppContext& operator=(const AppContext&) = delete;
};

} // namespace Configs
