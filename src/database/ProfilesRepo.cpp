#include "include/database/ProfilesRepo.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "include/database/GroupsRepo.h"


namespace Configs {
    ProfilesRepo::ProfilesRepo(Database& database) : db(database) {
        createTables();
    }

    void ProfilesRepo::createTables() const {
        // Note: This table has a foreign key to groups(id).
        // Ensure GroupsRepo::createTables() is called before this method
        // to avoid foreign key constraint errors.
        // Create profiles table
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS profiles (
                id INTEGER PRIMARY KEY,
                type TEXT NOT NULL,
                name TEXT,
                gid INTEGER NOT NULL DEFAULT 0,
                latency INTEGER NOT NULL DEFAULT 0,
                dl_speed TEXT,
                ul_speed TEXT,
                test_country TEXT,
                full_test_report TEXT,
                outbound_json TEXT NOT NULL,
                traffic_json TEXT,
                created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
                updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
                FOREIGN KEY(gid) REFERENCES groups(id) ON DELETE CASCADE
            )
        )");
        
        // Create indexes for faster lookups
        db.exec("CREATE INDEX IF NOT EXISTS idx_profiles_gid ON profiles(gid)");
        db.exec("CREATE INDEX IF NOT EXISTS idx_profiles_name ON profiles(name)");
    }

    QJsonObject ProfilesRepo::profileToJson(const Profile* profile) const {
        QJsonObject json;
        
        // Simple fields
        json["type"] = profile->type;
        json["name"] = profile->outbound->name;
        json["id"] = profile->id;
        json["gid"] = profile->gid;
        json["latency"] = profile->latency;
        json["dl_speed"] = profile->dl_speed;
        json["ul_speed"] = profile->ul_speed;
        json["test_country"] = profile->test_country;
        json["full_test_report"] = profile->full_test_report;
        
        // Complex objects - serialize to JSON strings
        if (profile->outbound) {
            json["outbound"] = profile->outbound->ExportToJson();
        }
        
        if (profile->traffic_data) {
            auto trafficJsonStore = dynamic_cast<JsonStore*>(profile->traffic_data.get());
            if (trafficJsonStore) {
                json["traffic"] = trafficJsonStore->ToJson();
            }
        }
        
        return json;
    }

    std::shared_ptr<Profile> ProfilesRepo::profileFromJson(const QJsonObject& json) const {
        auto profile = std::make_shared<Profile>();
        
        // Simple fields
        profile->type = json["type"].toString();
        profile->name = json["name"].toString();
        profile->id = json["id"].toInt();
        profile->gid = json["gid"].toInt();
        profile->latency = json["latency"].toInt();
        profile->dl_speed = json["dl_speed"].toString();
        profile->ul_speed = json["ul_speed"].toString();
        profile->test_country = json["test_country"].toString();
        profile->full_test_report = json["full_test_report"].toString();
        
        // Reconstruct outbound (bean is not needed in new implementation)
        QString type = profile->type;
        if (type == "hysteria2") {
            type = "hysteria";
        }
        
        Configs::outbound* outbound = nullptr;
        
        // Create outbound based on type (bean is legacy, not needed)
        if (type == "socks") {
            outbound = new Configs::socks();
        } else if (type == "http") {
            outbound = new Configs::http();
        } else if (type == "shadowsocks") {
            outbound = new Configs::shadowsocks();
        } else if (type == "chain") {
            outbound = new Configs::chain();
        } else if (type == "vmess") {
            outbound = new Configs::vmess();
        } else if (type == "trojan") {
            outbound = new Configs::Trojan();
        } else if (type == "vless") {
            outbound = new Configs::vless();
        } else if (type == "xrayvless") {
            outbound = new Configs::xrayVless();
        } else if (type == "hysteria" || type == "hysteria2") {
            outbound = new Configs::hysteria();
        } else if (type == "tuic") {
            outbound = new Configs::tuic();
        } else if (type == "anytls") {
            outbound = new Configs::anyTLS();
        } else if (type == "wireguard") {
            outbound = new Configs::wireguard();
        } else if (type == "tailscale") {
            outbound = new Configs::tailscale();
        } else if (type == "ssh") {
            outbound = new Configs::ssh();
        } else if (type == "custom") {
            outbound = new Configs::Custom();
        } else if (type == "extracore") {
            outbound = new Configs::extracore();
        } else {
            outbound = new Configs::outbound();
            outbound->invalid = true;
        }
        
        profile->outbound = std::shared_ptr<Configs::outbound>(outbound);
        profile->traffic_data = std::make_shared<Stats::TrafficData>("");
        
        // Parse complex objects from JSON
        if (json.contains("outbound") && json["outbound"].isObject()) {
            profile->outbound->ParseFromJson(json["outbound"].toObject());
        }
        
        if (json.contains("traffic") && json["traffic"].isObject() && profile->traffic_data) {
            if (auto trafficJsonStore = dynamic_cast<JsonStore*>(profile->traffic_data.get())) {
                trafficJsonStore->FromJson(json["traffic"].toObject());
            }
        }
        
        profile->name = profile->outbound->name;
        
        return profile;
    }

    void ProfilesRepo::saveToDatabase(const Profile* profile, int id) const {
        QJsonObject json = profileToJson(profile);
        QJsonDocument doc(json);
        QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        
        // Serialize complex objects separately for easier querying
        QString outboundJson = "";
        QString trafficJson = "";
        
        if (profile->outbound) {
            QJsonDocument outboundDoc(profile->outbound->ExportToJson());
            outboundJson = QString::fromUtf8(outboundDoc.toJson(QJsonDocument::Compact));
        }
        
        if (profile->traffic_data) {
            auto trafficJsonStore = dynamic_cast<JsonStore*>(profile->traffic_data.get());
            if (trafficJsonStore) {
                QJsonDocument trafficDoc(trafficJsonStore->ToJson());
                trafficJson = QString::fromUtf8(trafficDoc.toJson(QJsonDocument::Compact));
            }
        }
        
        // Sync name with outbound->name if outbound exists
        QString name = profile->outbound->name;
        
        // Check if profile exists
        auto checkQuery = db.query("SELECT id FROM profiles WHERE id = ?", id);
        bool exists = checkQuery && checkQuery->executeStep();
        
        if (exists) {
            // Update
            db.exec(R"(
                UPDATE profiles 
                SET type = ?, name = ?, gid = ?, latency = ?, dl_speed = ?, ul_speed = ?, 
                    test_country = ?, full_test_report = ?, outbound_json = ?, 
                    traffic_json = ?, updated_at = strftime('%s', 'now')
                WHERE id = ?
            )", 
                profile->type.toStdString(),
                name.toStdString(),
                profile->gid,
                profile->latency,
                profile->dl_speed.toStdString(),
                profile->ul_speed.toStdString(),
                profile->test_country.toStdString(),
                profile->full_test_report.toStdString(),
                outboundJson.toStdString(),
                trafficJson.toStdString(),
                id
            );
        } else {
            // Insert
            db.exec(R"(
                INSERT INTO profiles 
                (id, type, name, gid, latency, dl_speed, ul_speed, test_country, 
                 full_test_report, outbound_json, traffic_json)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )",
                id,
                profile->type.toStdString(),
                name.toStdString(),
                profile->gid,
                profile->latency,
                profile->dl_speed.toStdString(),
                profile->ul_speed.toStdString(),
                profile->test_country.toStdString(),
                profile->full_test_report.toStdString(),
                outboundJson.toStdString(),
                trafficJson.toStdString()
            );
        }
    }

    std::shared_ptr<Profile> ProfilesRepo::loadFromDatabase(int id) const {
        auto query = db.query(R"(
            SELECT id, type, name, gid, latency, dl_speed, ul_speed, test_country, 
                   full_test_report, outbound_json, traffic_json
            FROM profiles WHERE id = ?
        )", id);
        if (!query || !query->executeStep()) {
            return nullptr;
        }
        
        QJsonObject json;
        json["id"] = query->getColumn(0).getInt();
        json["type"] = QString::fromStdString(query->getColumn(1).getText());
        json["name"] = QString::fromStdString(query->getColumn(2).getText());
        json["gid"] = query->getColumn(3).getInt();
        json["latency"] = query->getColumn(4).getInt();
        json["dl_speed"] = QString::fromStdString(query->getColumn(5).getText());
        json["ul_speed"] = QString::fromStdString(query->getColumn(6).getText());
        json["test_country"] = QString::fromStdString(query->getColumn(7).getText());
        json["full_test_report"] = QString::fromStdString(query->getColumn(8).getText());
        
        // Parse complex objects
        QString outboundJsonStr = QString::fromStdString(query->getColumn(9).getText());
        QJsonDocument outboundDoc = QJsonDocument::fromJson(outboundJsonStr.toUtf8());
        if (!outboundDoc.isNull() && outboundDoc.isObject()) {
            json["outbound"] = outboundDoc.object();
        }
        
        QString trafficJsonStr = QString::fromStdString(query->getColumn(10).getText());
        if (!trafficJsonStr.isEmpty()) {
            QJsonDocument trafficDoc = QJsonDocument::fromJson(trafficJsonStr.toUtf8());
            if (!trafficDoc.isNull() && trafficDoc.isObject()) {
                json["traffic"] = trafficDoc.object();
            }
        }
        
        auto profile = profileFromJson(json);

        return profile;
    }

    std::shared_ptr<Profile> ProfilesRepo::NewProfile(const QString &type) {
        Configs::outbound *outbound = nullptr;
        
        // Create outbound based on type (bean is legacy, not needed)
        if (type == "socks") {
            outbound = new Configs::socks();
        } else if (type == "http") {
            outbound = new Configs::http();
        } else if (type == "shadowsocks") {
            outbound = new Configs::shadowsocks();
        } else if (type == "chain") {
            outbound = new Configs::chain();
        } else if (type == "vmess") {
            outbound = new Configs::vmess();
        } else if (type == "trojan") {
            outbound = new Configs::Trojan();
        } else if (type == "vless") {
            outbound = new Configs::vless();
        } else if (type == "xrayvless") {
            outbound = new Configs::xrayVless();
        } else if (type == "hysteria" || type == "hysteria2") {
            outbound = new Configs::hysteria();
        } else if (type == "tuic") {
            outbound = new Configs::tuic();
        } else if (type == "anytls") {
            outbound = new Configs::anyTLS();
        } else if (type == "wireguard") {
            outbound = new Configs::wireguard();
        } else if (type == "tailscale") {
            outbound = new Configs::tailscale();
        } else if (type == "ssh") {
            outbound = new Configs::ssh();
        } else if (type == "custom") {
            outbound = new Configs::Custom();
        } else if (type == "extracore") {
            outbound = new Configs::extracore();
        } else {
            outbound = new Configs::outbound();
            outbound->invalid = true;
        }
        
        // Bean is legacy, pass nullptr
        return std::make_shared<Profile>(outbound, type);
    }

    bool ProfilesRepo::AddProfile(std::shared_ptr<Profile>& profile, int gid) {
        QMutexLocker locker(&mutex);
        
        if (profile->id >= 0) {
            return false; // Already has an ID
        }
        
        int newId = NewProfileID();
        profile->id = newId;
        profile->gid = gid < 0 ? Configs::dataManager->settingsRepo->current_group : gid;

        // Add it to the group first
        if (auto group = dataManager->groupsRepo->GetGroup(profile->gid)) {
            group->AddProfile(profile->id);
            dataManager->groupsRepo->Save(group);
        } else {
            return false;
        }
        
        // Save to database first
        saveToDatabase(profile.get(), newId);
        
        // Add to identity map
        identityMap[newId] = std::weak_ptr<Profile>(profile);
        
        return true;
    }

    bool ProfilesRepo::AddProfileBatch(QList<std::shared_ptr<Profile>>& profiles, int gid) {
        QMutexLocker locker(&mutex);
        
        gid = gid < 0 ? Configs::dataManager->settingsRepo->current_group : gid;
        auto group = dataManager->groupsRepo->GetGroup(gid);
        if (!group) return false;
        
        for (auto& profile : profiles) {
            if (profile->id >= 0) continue; // Skip if already has ID
            
            int newId = NewProfileID();
            profile->id = newId;
            profile->gid = gid;

            // Add it to the group first
            group->AddProfile(profile->id);
            
            // Save to database first
            saveToDatabase(profile.get(), newId);
            
            // Add to identity map
            identityMap[newId] = std::weak_ptr<Profile>(profile);
        }

        dataManager->groupsRepo->Save(group);
        
        return true;
    }

    std::shared_ptr<Profile> ProfilesRepo::GetProfile(int id) const {
        QMutexLocker locker(&mutex);
        
        // Check identity map first
        if (auto it = identityMap.find(id); it != identityMap.end()) {
            if (auto shared = it->second.lock()) {
                return shared; // Return existing instance
            } else {
                // Weak pointer expired, remove from map
                identityMap.erase(it);
            }
        }
        
        // Load from database
        auto profile = loadFromDatabase(id);
        if (!profile) {
            return nullptr;
        }
        
        // Add to identity map
        identityMap[id] = std::weak_ptr<Profile>(profile);
        
        return profile;
    }

    QList<std::shared_ptr<Profile>> ProfilesRepo::GetProfileBatch(QList<int> ids) {
        QMutexLocker locker(&mutex);
        QList<std::shared_ptr<Profile>> profiles;
        for (auto& id : ids) {
            if (auto it = identityMap.find(id); it != identityMap.end()) {
                if (auto shared = it->second.lock()) {
                    profiles.push_back(shared);
                    continue;
                }
            }
            auto profile = loadFromDatabase(id);
            if (!profile) {
                MW_show_log("Failed to load profile from database, db is corrupted");
                return {};
            }
            identityMap[id] = std::weak_ptr<Profile>(profile);
            profiles.push_back(profile);
        }

        return profiles;
    }

    std::shared_ptr<Profile> ProfilesRepo::GetProfileByName(const QString& name) {
        // Query by name using the index
        auto query = db.query("SELECT id FROM profiles WHERE name = ? LIMIT 1", name.toStdString());
        if (!query || !query->executeStep()) {
            return nullptr;
        }
        
        int id = query->getColumn(0).getInt();
        return GetProfile(id);
    }

    QStringList ProfilesRepo::GetAllProfileNames() {
        auto query = db.query("SELECT name FROM profiles");
        if (!query || !query->executeStep()) {
            return {};
        }
        QStringList names;
        while (query->executeStep()) {
            names.append(QString(query->getColumn(0).getString().c_str()));
        }
        return names;
    }

    void ProfilesRepo::DeleteProfile(int id) {
        auto profile = GetProfile(id);
        auto group = dataManager->groupsRepo->GetGroup(profile->gid);
        if (group) {
            group->RemoveProfile(id);
            dataManager->groupsRepo->Save(group);
        }

        QMutexLocker locker(&mutex);
        // Remove from identity map
        identityMap.erase(id);
        
        // Delete from database
        db.exec("DELETE FROM profiles WHERE id = ?", id);
    }

    void ProfilesRepo::BatchDeleteProfiles(const QList<int>& ids) {
        QSet<std::shared_ptr<Group>> groups;
        for (auto& id : ids) {
            if (auto profile = GetProfile(id)) {
                if (auto group = dataManager->groupsRepo->GetGroup(profile->gid)) {
                    group->RemoveProfile(id);
                    groups.insert(group);
                }
            }
        }
        for (auto& group : groups) {
            dataManager->groupsRepo->Save(group);
        }

        QMutexLocker locker(&mutex);
        // Delete from database
        for (int id : ids) {
            identityMap.erase(id);
            db.exec("DELETE FROM profiles WHERE id = ?", id);
        }
    }

    QList<int> ProfilesRepo::GetAllProfileIds() const {
        QList<int> ids;
        auto query = db.query("SELECT id FROM profiles ORDER BY id");
        if (query) {
            while (query->executeStep()) {
                ids.append(query->getColumn(0).getInt());
            }
        }
        return ids;
    }

    int ProfilesRepo::NewProfileID() const {
        // Atomically increment and get the new ID using RETURNING clause
        // Note: This method is called from within methods that already hold the mutex lock
        auto query = db.query("UPDATE entity_ids SET profile_last_id = profile_last_id + 1 RETURNING profile_last_id");
        if (query && query->executeStep()) {
            return query->getColumn(0).getInt();
        }
        
        // Fallback if RETURNING is not supported (shouldn't happen with modern SQLite)
        return 0;
    }

    bool ProfilesRepo::Save(const std::shared_ptr<Profile>& profile) {
        if (!profile) {
            return false;
        }
        
        if (profile->id < 0) {
            return false; // Profile doesn't have an ID, use AddProfile instead
        }
        
        runOnNewThread([=, this] {
            QMutexLocker locker(&mutex);

            // Save to database
            saveToDatabase(profile.get(), profile->id);

            // Update identity map
            identityMap[profile->id] = std::weak_ptr<Profile>(profile);
        });
        
        return true;
    }
}
