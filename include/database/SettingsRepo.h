#pragma once

#include "Database.h"
#include "include/database/SettingsValues.h"
#include <QMutexLocker>
#include <QJsonObject>
#include <QHash>
#include <QSet>
#include <atomic>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Configs {
    class SettingsRepo : public SettingsValues {
    private:
        Database& db;

        // Each key's serialized value as last read from or written to the database.
        QHash<QString, std::string> stored;
        // Keys whose last write failed; rewritten by the next save without being reported as changed again.
        QSet<QString> unsaved;
        // Guards stored and unsaved. All known Save() callers run on the main thread (EngineState posts the resume target there).
        std::mutex saveMutex;
        std::atomic<quint64> revision{0};

        void createTables() const;
        void loadAllSettings();
        [[nodiscard]] std::vector<std::pair<QString, std::string>> serializeAll() const;
        QStringList saveAllSettings();

    public:
        bool noSave = false;

        explicit SettingsRepo(Database& database);
        
        // Writes only the keys whose value changed since the last Save(); false when none did. changedKeys gets their DB names.
        bool Save(QStringList *changedKeys = nullptr);
        
        // Any thread; moves with every Save() that changes a value, bookkeeping ones such as the traffic graph's state included.
        quint64 Revision() const { return revision.load(); }

        // Runtime state and flags below are never persisted.
        QString core_socket_name = "";
        bool core_running = false;
        bool prepare_exit = false;
        bool spmode_vpn = false;
        bool spmode_system_proxy = false;
        QString appdataDir = "";
        QStringList ignoreConnTag = {};
        int imported_count = 0;
        bool refreshing_group_list = false;
        bool refreshing_group = false;

        QStringList argv = {};
        bool flag_use_appdata = false;
        bool flag_many = false;
        bool flag_tray = false;
        bool flag_debug = false;
        bool flag_restart_tun_on = false;

        [[nodiscard]] QString GetUserAgent(bool isDefault = false) const;
        
        [[nodiscard]] QStringList GetExtraCorePaths() const;
        bool AddExtraCorePath(const QString &path);
    };
}
