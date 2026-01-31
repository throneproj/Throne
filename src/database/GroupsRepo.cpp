#include "include/database/entities/Group.h"
#include "include/database/GroupsRepo.h"
#include "include/global/Utils.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutexLocker>

#include "include/global/Configs.hpp"


namespace Configs {
    GroupsRepo::GroupsRepo(Database& database) : db(database) {
        createTables();
    }

    void GroupsRepo::createTables() const {
        // Create groups table
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS groups (
                id INTEGER PRIMARY KEY,
                archive INTEGER NOT NULL DEFAULT 0,
                skip_auto_update INTEGER NOT NULL DEFAULT 0,
                name TEXT NOT NULL DEFAULT '',
                url TEXT,
                info TEXT,
                sub_last_update INTEGER NOT NULL DEFAULT 0,
                front_proxy_id INTEGER NOT NULL DEFAULT -1,
                landing_proxy_id INTEGER NOT NULL DEFAULT -1,
                column_width_json TEXT,
                profiles_json TEXT NOT NULL DEFAULT '[]',
                scroll_last_profile INTEGER NOT NULL DEFAULT -1,
                created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
                updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
            )
        )");
        
        // Create groups_order table to store UI tab order
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS groups_order (
                group_id INTEGER NOT NULL PRIMARY KEY,
                display_order INTEGER NOT NULL
            )
        )");
    }

    QJsonObject GroupsRepo::groupToJson(const Group* group) const {
        QJsonObject json;
        
        json["id"] = group->id;
        json["archive"] = group->archive;
        json["skip_auto_update"] = group->skip_auto_update;
        json["name"] = group->name;
        json["url"] = group->url;
        json["info"] = group->info;
        json["sub_last_update"] = static_cast<qint64>(group->sub_last_update);
        json["front_proxy_id"] = group->front_proxy_id;
        json["landing_proxy_id"] = group->landing_proxy_id;
        json["column_width"] = QListInt2QJsonArray(group->column_width);
        json["profiles"] = QListInt2QJsonArray(group->profiles);
        json["scroll_last_profile"] = group->scroll_last_profile;
        
        return json;
    }

    std::shared_ptr<Group> GroupsRepo::groupFromJson(const QJsonObject& json) const {
        auto group = std::make_shared<Group>();
        
        group->id = json["id"].toInt();
        group->archive = json["archive"].toBool();
        group->skip_auto_update = json["skip_auto_update"].toBool();
        group->name = json["name"].toString();
        group->url = json["url"].toString();
        group->info = json["info"].toString();
        group->sub_last_update = json["sub_last_update"].toVariant().toLongLong();
        group->front_proxy_id = json["front_proxy_id"].toInt();
        group->landing_proxy_id = json["landing_proxy_id"].toInt();
        group->column_width = QJsonArray2QListInt(json["column_width"].toArray());
        group->profiles = QJsonArray2QListInt(json["profiles"].toArray());
        group->scroll_last_profile = json["scroll_last_profile"].toInt(-1);
        
        return group;
    }

    void GroupsRepo::saveToDatabase(const Group* group, int id) const {
        // Serialize lists to JSON strings
        QJsonArray columnWidthArray = QListInt2QJsonArray(group->column_width);
        QJsonArray profilesArray = QListInt2QJsonArray(group->profiles);
        
        QJsonDocument columnWidthDoc(columnWidthArray);
        QJsonDocument profilesDoc(profilesArray);
        
        QString columnWidthJson = QString::fromUtf8(columnWidthDoc.toJson(QJsonDocument::Compact));
        QString profilesJson = QString::fromUtf8(profilesDoc.toJson(QJsonDocument::Compact));
        
        // Check if group exists
        auto checkQuery = db.query("SELECT id FROM groups WHERE id = ?", id);
        bool exists = checkQuery && checkQuery->executeStep();
        
        if (exists) {
            // Update
            db.exec(R"(
                UPDATE groups 
                SET archive = ?, skip_auto_update = ?, name = ?, url = ?, info = ?,
                    sub_last_update = ?, front_proxy_id = ?, landing_proxy_id = ?,
                    column_width_json = ?, profiles_json = ?, scroll_last_profile = ?,
                    updated_at = strftime('%s', 'now')
                WHERE id = ?
            )",
                group->archive ? 1 : 0,
                group->skip_auto_update ? 1 : 0,
                group->name.toStdString(),
                group->url.toStdString(),
                group->info.toStdString(),
                static_cast<long long>(group->sub_last_update),
                group->front_proxy_id,
                group->landing_proxy_id,
                columnWidthJson.toStdString(),
                profilesJson.toStdString(),
                group->scroll_last_profile,
                id
            );
        } else {
            // Insert
            db.exec(R"(
                INSERT INTO groups 
                (id, archive, skip_auto_update, name, url, info, sub_last_update,
                 front_proxy_id, landing_proxy_id,
                 column_width_json, profiles_json, scroll_last_profile)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )",
                id,
                group->archive ? 1 : 0,
                group->skip_auto_update ? 1 : 0,
                group->name.toStdString(),
                group->url.toStdString(),
                group->info.toStdString(),
                static_cast<long long>(group->sub_last_update),
                group->front_proxy_id,
                group->landing_proxy_id,
                columnWidthJson.toStdString(),
                profilesJson.toStdString(),
                group->scroll_last_profile
            );
        }
    }

    std::shared_ptr<Group> GroupsRepo::loadFromDatabase(int id) const {
        auto query = db.query(R"(
            SELECT id, archive, skip_auto_update, name, url, info, sub_last_update,
                   front_proxy_id, landing_proxy_id,
                   column_width_json, profiles_json, scroll_last_profile
            FROM groups WHERE id = ?
        )", id);
        if (!query || !query->executeStep()) {
            return nullptr;
        }
        
        QJsonObject json;
        json["id"] = query->getColumn(0).getInt();
        json["archive"] = query->getColumn(1).getInt() != 0;
        json["skip_auto_update"] = query->getColumn(2).getInt() != 0;
        json["name"] = QString::fromStdString(query->getColumn(3).getText());
        json["url"] = QString::fromStdString(query->getColumn(4).getText());
        json["info"] = QString::fromStdString(query->getColumn(5).getText());
        json["sub_last_update"] = static_cast<qint64>(query->getColumn(6).getInt64());
        json["front_proxy_id"] = query->getColumn(7).getInt();
        json["landing_proxy_id"] = query->getColumn(8).getInt();

        // Parse JSON arrays
        QString columnWidthJsonStr = QString::fromStdString(query->getColumn(9).getText());
        if (!columnWidthJsonStr.isEmpty()) {
            QJsonDocument columnWidthDoc = QJsonDocument::fromJson(columnWidthJsonStr.toUtf8());
            if (!columnWidthDoc.isNull() && columnWidthDoc.isArray()) {
                json["column_width"] = columnWidthDoc.array();
            }
        }
        
        QString profilesJsonStr = QString::fromStdString(query->getColumn(10).getText());
        if (!profilesJsonStr.isEmpty()) {
            QJsonDocument profilesDoc = QJsonDocument::fromJson(profilesJsonStr.toUtf8());
            if (!profilesDoc.isNull() && profilesDoc.isArray()) {
                json["profiles"] = profilesDoc.array();
            }
        }

        if (query->getColumnCount() > 11) {
            json["scroll_last_profile"] = query->getColumn(11).getInt();
        }
        
        return groupFromJson(json);
    }

    std::shared_ptr<Group> GroupsRepo::NewGroup() {
        return std::make_shared<Group>();
    }

    bool GroupsRepo::AddGroup(std::shared_ptr<Group>& group) {
        QMutexLocker locker(&mutex);
        if (group->id >= 0) return false;
        int newId = NewGroupID();
        group->id = newId;
        identityMap[newId] = std::weak_ptr<Group>(group);
        saveToDatabase(group.get(), group->id);
        int maxOrder = -1;
        auto maxOrderQuery = db.query("SELECT MAX(display_order) FROM groups_order");
        if (maxOrderQuery && maxOrderQuery->executeStep()) {
            maxOrder = maxOrderQuery->getColumn(0).getInt();
        }
        db.exec("INSERT INTO groups_order (group_id, display_order) VALUES (?, ?)",
            group->id,
            maxOrder + 1
        );
        return true;
    }

    std::shared_ptr<Group> GroupsRepo::GetGroup(int id) const {
        QMutexLocker locker(&mutex);
        if (auto it = identityMap.find(id); it != identityMap.end()) {
            if (auto shared = it->second.lock()) return shared;
            identityMap.erase(it);
        }
        auto group = loadFromDatabase(id);
        if (!group) return nullptr;
        identityMap[id] = std::weak_ptr<Group>(group);
        return group;
    }

    std::shared_ptr<Group> GroupsRepo::CurrentGroup() const {
        // Read current_group from SettingsRepo
        if (!Configs::dataManager || !Configs::dataManager->settingsRepo) {
            return nullptr;
        }
        
        int currentGroupId = Configs::dataManager->settingsRepo->current_group;
        
        // Retrieve and return the group with that ID
        return GetGroup(currentGroupId);
    }

    void GroupsRepo::DeleteGroup(int id) {
        QMutexLocker locker(&mutex);
        identityMap.erase(id);
        db.exec("DELETE FROM groups_order WHERE group_id = ?", id);
        db.exec("DELETE FROM groups WHERE id = ?", id);
    }

    QList<int> GroupsRepo::GetAllGroupIds() const {
        QList<int> ids;
        auto query = db.query("SELECT id FROM groups ORDER BY id");
        if (query) {
            while (query->executeStep()) {
                ids.append(query->getColumn(0).getInt());
            }
        }
        return ids;
    }

    int GroupsRepo::NewGroupID() const {
        // Atomically increment and get the new ID using RETURNING clause
        // Note: This method is called from within methods that already hold the mutex lock
        auto query = db.query("UPDATE entity_ids SET group_last_id = group_last_id + 1 RETURNING group_last_id");
        if (query && query->executeStep()) {
            return query->getColumn(0).getInt();
        }
        
        // Fallback if RETURNING is not supported (shouldn't happen with modern SQLite)
        return 0;
    }

    QList<int> GroupsRepo::GetGroupsTabOrder() const {
        QList<int> order;
        auto query = db.query("SELECT group_id FROM groups_order ORDER BY display_order");
        if (query) {
            while (query->executeStep()) {
                order.append(query->getColumn(0).getInt());
            }
        }
        return order;
    }

    void GroupsRepo::SetGroupsTabOrder(const QList<int>& order) {
        runOnNewThread([=, this] {
            db.exec("DELETE FROM groups_order");
            if (!order.isEmpty()) {
                std::vector<int> pairs;
                pairs.reserve(order.size() * 2);
                for (int i = 0; i < order.size(); ++i) {
                    pairs.push_back(order[i]);
                    pairs.push_back(i);
                }
                db.execBatchInsertIntPairs("groups_order", "group_id", "display_order", pairs);
            }
        });
    }

    bool GroupsRepo::Save(const std::shared_ptr<Group>& group) {
        if (!group) {
            return false;
        }
        
        if (group->id < 0) {
            return false; // Group doesn't have an ID, use AddGroup instead
        }
        
        runOnNewThread([=, this] {
            QMutexLocker locker(&mutex);
            saveToDatabase(group.get(), group->id);
            identityMap[group->id] = std::weak_ptr<Group>(group);
        });
        
        return true;
    }
}
