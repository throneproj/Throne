#include "include/database/SettingsRepo.h"
#include "NkrVersion.h"
#include <QUuid>

#include "include/database/SettingsSchema.h"
#include "include/global/Utils.hpp"
#include "include/global/Validation.hpp"
#include "include/sys/UrlScheme.hpp"

namespace Configs {
    SettingsRepo::SettingsRepo(Database& database) : db(database) {
        url_scheme_auto_register = UrlScheme_AutoRegisterByDefault();
        createTables();
        loadAllSettings();
        // An empty secret disables authentication on the API service outright.
        if (core_box_api_secret.isEmpty()) {
            core_box_api_secret = QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
        }
        // Rewrites rows missing or spelled otherwise ("1" for true, migrated values); no setting changed, so revision stays.
        saveAllSettings();
    }

    void SettingsRepo::createTables() const {
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS settings (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL
            )
        )");
    }

    void SettingsRepo::loadAllSettings() {
        auto query = db.query("SELECT key, value FROM settings");
        if (!query) return;

        bool freshInstall = true;
        while (query->executeStep()) {
            freshInstall = false;
            const QString key = QString::fromStdString(query->getColumn(0).getText());
            std::string raw = query->getColumn(1).getText();
            const QString str = QString::fromStdString(raw);
            stored[key] = std::move(raw);

            // Pre-1.14 DNS rule strategies: only the v4-only case survives as a filter, the rest were no-ops.
            if (key == "direct_dns_strategy") {
                direct_dns_disable_ipv6 = str == "ipv4_only";
                continue;
            }
            if (key == "remote_dns_strategy") {
                remote_dns_disable_ipv6 = str == "ipv4_only";
                continue;
            }
            if (const auto *entry = SettingsSchema::Find(key)) SettingsSchema::Parse(*this, *entry, str);
        }
        if (freshInstall) simple_mode = true;
        // Builds before 1.2.1 saved the translated interval label ("5 мин.", "۱۰ دقیقه"), which the core rejects.
        if (!Validation::Duration(ntp_interval).isEmpty()) {
            QString minutes;
            for (const QChar c : ntp_interval) {
                if (c.digitValue() >= 0) minutes += QChar(u'0' + c.digitValue());
            }
            ntp_interval = minutes.isEmpty() ? QStringLiteral("30m") : minutes + u'm';
        }
        // Retired keys nothing writes back; the DNS ones would otherwise keep overriding the migrated flags on every load.
        db.exec("DELETE FROM settings WHERE key IN ('direct_dns_strategy', 'remote_dns_strategy', 'log_auto_scroll')");
    }

    std::vector<std::pair<QString, std::string>> SettingsRepo::serializeAll() const {
        std::vector<std::pair<QString, std::string>> keyValues;
        keyValues.reserve(SettingsSchema::Keys().size());
        for (const auto& key : SettingsSchema::Keys()) keyValues.emplace_back(key.name, SettingsSchema::Serialize(*this, key));
        return keyValues;
    }

    QStringList SettingsRepo::saveAllSettings() {
        std::lock_guard lock(saveMutex);
        QStringList changedKeys;
        std::vector<std::pair<std::string, std::string>> writes;
        for (auto& [key, value] : serializeAll()) {
            const auto it = stored.constFind(key);
            const bool changed = it == stored.cend() || *it != value;
            if (!changed && !unsaved.contains(key)) continue;
            if (changed) changedKeys << key;
            writes.emplace_back(key.toStdString(), value);
            stored[key] = std::move(value);
        }
        // Under noSave the snapshot still moves, so the next Save() does not report the same change again.
        if (noSave || writes.empty()) return changedKeys;
        if (db.execBatchSettingsReplace(writes)) {
            unsaved.clear();
        } else {
            for (const auto& [key, value] : writes) unsaved.insert(QString::fromStdString(key));
        }
        return changedKeys;
    }

    static QStringView SubStrBefore(QStringView str, QStringView sub) {
        const qsizetype pos = str.indexOf(sub);
        return pos == -1 ? str : str.left(pos);
    }

    QString SettingsRepo::GetUserAgent(bool isDefault) const {
        if (user_agent.isEmpty() || isDefault) {
            const QStringView version = SubStrBefore(QStringLiteral(NKR_VERSION), u"-");
            if (version.contains(u'.')) {
                return QStringLiteral("Throne/") + version.toString();
            }
            return QStringLiteral("Throne/1.0.0");
        }
        return user_agent;
    }

    bool SettingsRepo::Save(QStringList *changedKeys) {
        QStringList changed = saveAllSettings();
        const bool any = !changed.isEmpty();
        if (any) ++revision;
        if (changedKeys != nullptr) *changedKeys = std::move(changed);
        return any;
    }

    QStringList SettingsRepo::GetExtraCorePaths() const {
        return extraCorePaths;
    }

    bool SettingsRepo::AddExtraCorePath(const QString &path) {
        if (extraCorePaths.contains(path)) {
            return false;
        }
        extraCorePaths.append(path);
        return true;
    }
}
