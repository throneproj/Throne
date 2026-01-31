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
            json["traffic"] = profile->traffic_data->ExportToJson();
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
            profile->traffic_data->ParseFromJson(json["traffic"].toObject());
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
            QJsonDocument trafficDoc(profile->traffic_data->ExportToJson());
            trafficJson = QString::fromUtf8(trafficDoc.toJson(QJsonDocument::Compact));
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

    ProfileInsertRow ProfilesRepo::profileToInsertRow(const Profile* profile, int id, int gid) const {
        QString outboundJson;
        QString trafficJson;
        if (profile->outbound) {
            outboundJson = QString::fromUtf8(QJsonDocument(profile->outbound->ExportToJson()).toJson(QJsonDocument::Compact));
        }
        if (profile->traffic_data) {
            trafficJson = QString::fromUtf8(QJsonDocument(profile->traffic_data->ExportToJson()).toJson(QJsonDocument::Compact));
        }
        QString name = profile->outbound ? profile->outbound->name : QString();
        ProfileInsertRow row;
        row.id = id;
        row.type = profile->type.toStdString();
        row.name = name.toStdString();
        row.gid = gid;
        row.latency = profile->latency;
        row.dl_speed = profile->dl_speed.toStdString();
        row.ul_speed = profile->ul_speed.toStdString();
        row.test_country = profile->test_country.toStdString();
        row.full_test_report = profile->full_test_report.toStdString();
        row.outbound_json = outboundJson.toStdString();
        row.traffic_json = trafficJson.toStdString();
        return row;
    }

    std::shared_ptr<Profile> ProfilesRepo::profileFromRow(SQLite::Statement& stmt) const {
        QJsonObject json;
        json["id"] = stmt.getColumn(0).getInt();
        json["type"] = QString::fromStdString(stmt.getColumn(1).getText());
        json["name"] = QString::fromStdString(stmt.getColumn(2).getText());
        json["gid"] = stmt.getColumn(3).getInt();
        json["latency"] = stmt.getColumn(4).getInt();
        json["dl_speed"] = QString::fromStdString(stmt.getColumn(5).getText());
        json["ul_speed"] = QString::fromStdString(stmt.getColumn(6).getText());
        json["test_country"] = QString::fromStdString(stmt.getColumn(7).getText());
        json["full_test_report"] = QString::fromStdString(stmt.getColumn(8).getText());
        
        QString outboundJsonStr = QString::fromStdString(stmt.getColumn(9).getText());
        QJsonDocument outboundDoc = QJsonDocument::fromJson(outboundJsonStr.toUtf8());
        if (!outboundDoc.isNull() && outboundDoc.isObject()) {
            json["outbound"] = outboundDoc.object();
        }
        
        QString trafficJsonStr = QString::fromStdString(stmt.getColumn(10).getText());
        if (!trafficJsonStr.isEmpty()) {
            QJsonDocument trafficDoc = QJsonDocument::fromJson(trafficJsonStr.toUtf8());
            if (!trafficDoc.isNull() && trafficDoc.isObject()) {
                json["traffic"] = trafficDoc.object();
            }
        }
        
        return profileFromJson(json);
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
        return profileFromRow(*query);
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
        if (profile->id >= 0) return false;
        int newId = NewProfileID();
        profile->id = newId;
        profile->gid = gid < 0 ? Configs::dataManager->settingsRepo->current_group : gid;
        QMutexLocker locker(&mutex);
        identityMap[newId] = std::weak_ptr<Profile>(profile);
        saveToDatabase(profile.get(), profile->id);
        if (auto group = dataManager->groupsRepo->GetGroup(profile->gid)) {
            group->AddProfile(profile->id);
            dataManager->groupsRepo->Save(group);
        } else {
            return false;
        }
        return true;
    }

    bool ProfilesRepo::AddProfileBatch(QList<std::shared_ptr<Profile>>& profiles, int gid) {
        gid = gid < 0 ? Configs::dataManager->settingsRepo->current_group : gid;
        auto group = dataManager->groupsRepo->GetGroup(gid);
        if (!group) return false;

        QList<std::shared_ptr<Profile>> toAdd;
        for (auto& profile : profiles) {
            if (profile->id < 0) toAdd.append(profile);
        }
        if (toAdd.isEmpty()) return true;

        int n = toAdd.size();
        int firstId = NewProfileIDRange(n);

        QMutexLocker locker(&mutex);
        for (int i = 0; i < n; ++i) {
            int id = firstId + i;
            toAdd[i]->id = id;
            toAdd[i]->gid = gid;
            identityMap[id] = std::weak_ptr<Profile>(toAdd[i]);
        }

        std::vector<ProfileInsertRow> rows;
        rows.reserve(n);
        for (int i = 0; i < n; ++i) {
            rows.push_back(profileToInsertRow(toAdd[i].get(), toAdd[i]->id, toAdd[i]->gid));
        }
        db.execBatchInsertProfiles(rows);

        for (const auto& profile : toAdd) {
            group->AddProfile(profile->id);
        }
        dataManager->groupsRepo->Save(group);

        return true;
    }

    std::shared_ptr<Profile> ProfilesRepo::GetProfile(int id) const {
        QMutexLocker locker(&mutex);
        if (auto it = identityMap.find(id); it != identityMap.end()) {
            if (auto shared = it->second.lock()) return shared;
            identityMap.erase(it);
        }
        auto profile = loadFromDatabase(id);
        if (!profile) return nullptr;
        identityMap[id] = std::weak_ptr<Profile>(profile);
        return profile;
    }

    std::map<int, std::shared_ptr<Profile>> ProfilesRepo::loadProfilesByIdsChunk(const QList<int>& chunkIds) const {
        std::map<int, std::shared_ptr<Profile>> result;
        if (chunkIds.isEmpty()) return result;
        QString idList;
        for (int i = 0; i < chunkIds.size(); ++i) {
            if (i > 0) idList += ",";
            idList += QString::number(chunkIds[i]);
        }
        std::string sql = "SELECT id, type, name, gid, latency, dl_speed, ul_speed, test_country, "
                         "full_test_report, outbound_json, traffic_json FROM profiles WHERE id IN (" +
                         idList.toStdString() + ") ORDER BY id";
        auto query = db.query(sql);
        if (!query) return result;
        while (query->executeStep()) {
            auto profile = profileFromRow(*query);
            result[profile->id] = std::move(profile);
        }
        return result;
    }

    QList<std::shared_ptr<Profile>> ProfilesRepo::GetProfileBatch(QList<int> ids) {
        QList<std::shared_ptr<Profile>> profiles;
        if (ids.isEmpty()) return profiles;

        std::map<int, std::shared_ptr<Profile>> byId;
        QList<int> missingIds;
        QMutexLocker locker(&mutex);
        for (int id : ids) {
            auto it = identityMap.find(id);
            if (it != identityMap.end()) {
                if (auto shared = it->second.lock()) {
                    byId[id] = shared;
                    continue;
                }
                identityMap.erase(it);
            }
            missingIds.append(id);
        }
        if (missingIds.isEmpty()) {
            for (int id : ids) {
                auto it = byId.find(id);
                if (it != byId.end()) profiles.push_back(it->second);
            }
            return profiles;
        }

        for (int off = 0; off < missingIds.size(); off += Configs::BATCH_LIMIT) {
            int end = std::min(off + Configs::BATCH_LIMIT, static_cast<int>(missingIds.size()));
            QList<int> chunk;
            for (int i = off; i < end; ++i) chunk.append(missingIds[i]);
            std::map<int, std::shared_ptr<Profile>> loaded = loadProfilesByIdsChunk(chunk);
            for (auto& p : loaded) byId[p.first] = std::move(p.second);
        }
        for (const auto& p : byId) {
            if (missingIds.contains(p.first)) identityMap[p.first] = std::weak_ptr<Profile>(p.second);
        }
        for (int id : ids) {
            auto it = byId.find(id);
            if (it != byId.end()) profiles.push_back(it->second);
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
        if (!query) return {};
        QStringList names;
        while (query->executeStep()) {
            names.append(QString(query->getColumn(0).getString().c_str()));
        }
        return names;
    }

    void ProfilesRepo::DeleteProfile(int id) {
        auto profile = GetProfile(id);
        if (profile) {
            auto group = dataManager->groupsRepo->GetGroup(profile->gid);
            if (group) {
                group->RemoveProfile(id);
                dataManager->groupsRepo->Save(group);
            }
        }
        QMutexLocker locker(&mutex);
        identityMap.erase(id);
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
        for (int id : ids) identityMap.erase(id);
        if (!ids.isEmpty()) {
            std::vector<int> idVec(ids.begin(), ids.end());
            db.execDeleteByIdIn("profiles", "id", idVec);
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
        // Atomically increment and get the new ID using RETURNING clause (DB atomic, no lock required)
        auto query = db.query("UPDATE entity_ids SET profile_last_id = profile_last_id + 1 RETURNING profile_last_id");
        if (query && query->executeStep()) {
            return query->getColumn(0).getInt();
        }
        return 0;
    }

    int ProfilesRepo::NewProfileIDRange(int n) const {
        if (n <= 0) return 0;
        // Atomically reserve n IDs; RETURNING gives the new value (old + n), so first ID = newValue - n + 1
        auto query = db.query("UPDATE entity_ids SET profile_last_id = profile_last_id + ? RETURNING profile_last_id", n);
        if (query && query->executeStep()) {
            int newValue = query->getColumn(0).getInt();
            return newValue - n + 1;
        }
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
            saveToDatabase(profile.get(), profile->id);
            identityMap[profile->id] = std::weak_ptr<Profile>(profile);
        });
        
        return true;
    }
}
