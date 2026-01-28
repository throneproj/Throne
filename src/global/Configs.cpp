#include "include/global/Configs.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QNetworkAccessManager>
#include <QStandardPaths>
#include <utility>
#include <include/api/RPC.h>



#ifdef Q_OS_WIN
#include "include/sys/windows/guihelper.h"
#else
#ifdef Q_OS_LINUX
#include <include/sys/linux/LinuxCap.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace Configs_ConfigItem {
    void JsonStore::_add(configItem *item) {
        _map.insert(item->name, std::shared_ptr<configItem>(item));
    }

    void JsonStore::_remove(const QString &key) {
        _map.remove(key);
    }

    QString JsonStore::_name(void *p) {
        for (const auto &_item: _map) {
            if (_item->ptr == p) return _item->name;
        }
        return {};
    }

    std::shared_ptr<configItem> JsonStore::_get(const QString &name) {
        if (_map.contains(name)) {
            return _map[name];
        }
        return nullptr;
    }

    void JsonStore::_setValue(const QString &name, void *p) {
        auto item = _get(name);
        if (item == nullptr) return;

        switch (item->type) {
            case itemType::string:
                *(QString *) item->ptr = *(QString *) p;
                break;
            case itemType::boolean:
                *(bool *) item->ptr = *(bool *) p;
                break;
            case itemType::integer:
                *(int *) item->ptr = *(int *) p;
                break;
            case itemType::integer64:
                *(long long *) item->ptr = *(long long *) p;
                break;
                // others...
            case stringList:
            case integerList:
            case jsonStore:
                break;
        }
    }

    QJsonObject JsonStore::ToJson(const QStringList &without) {
        QJsonObject object;
        for (const auto &_item: _map) {
            auto item = _item.get();
            if (without.contains(item->name)) continue;
            switch (item->type) {
                case itemType::string:
                    // Allow Empty
                    object.insert(item->name, *(QString *) item->ptr);
                    break;
                case itemType::integer:
                    object.insert(item->name, *(int *) item->ptr);
                    break;
                case itemType::integer64:
                    object.insert(item->name, *(long long *) item->ptr);
                    break;
                case itemType::boolean:
                    object.insert(item->name, *(bool *) item->ptr);
                    break;
                case itemType::stringList: {
                    if (QListStr2QJsonArray(*(QList<QString> *) item->ptr).isEmpty()) continue;
                    object.insert(item->name, QListStr2QJsonArray(*(QList<QString> *) item->ptr));
                    break;
                }
                case itemType::integerList: {
                    if (QListInt2QJsonArray(*(QList<int> *) item->ptr).isEmpty()) continue;
                    object.insert(item->name, QListInt2QJsonArray(*(QList<int> *) item->ptr));
                    break;
                }
                case itemType::jsonStore:
                    // _add 时应关联对应 JsonStore 的指针
                    object.insert(item->name, ((JsonStore *) item->ptr)->ToJson());
                    break;
                case itemType::jsonStoreList:
                    QJsonArray jsonArray;
                    auto arr = *(QList<JsonStore*> *) item->ptr;
                    for ( JsonStore* obj : arr) {
                        jsonArray.push_back(obj->ToJson());
                    }
                    object.insert(item->name, jsonArray);
                    break;
            }
        }
        return object;
    }

    QByteArray JsonStore::ToJsonBytes() {
        QJsonDocument document;
        document.setObject(ToJson());
        return document.toJson(save_control_compact ? QJsonDocument::Compact : QJsonDocument::Indented);
    }

    void JsonStore::FromJson(QJsonObject object) {
        for (const auto &key: object.keys()) {
            if (_map.count(key) == 0) {
                continue;
            }

            auto value = object[key];
            auto item = _map[key].get();

            if (item == nullptr)
                continue; // 故意忽略

            // 根据类型修改ptr的内容
            switch (item->type) {
                case itemType::string:
                    if (value.type() != QJsonValue::String) {
                        continue;
                    }
                    *(QString *) item->ptr = value.toString();
                    break;
                case itemType::integer:
                    if (value.type() != QJsonValue::Double) {
                        continue;
                    }
                    *(int *) item->ptr = value.toInt();
                    break;
                case itemType::integer64:
                    if (value.type() != QJsonValue::Double) {
                        continue;
                    }
                    *(long long *) item->ptr = value.toDouble();
                    break;
                case itemType::boolean:
                    if (value.type() != QJsonValue::Bool) {
                        continue;
                    }
                    *(bool *) item->ptr = value.toBool();
                    break;
                case itemType::stringList:
                    if (value.type() != QJsonValue::Array) {
                        continue;
                    }
                    *(QList<QString> *) item->ptr = QJsonArray2QListString(value.toArray());
                    break;
                case itemType::integerList:
                    if (value.type() != QJsonValue::Array) {
                        continue;
                    }
                    *(QList<int> *) item->ptr = QJsonArray2QListInt(value.toArray());
                    break;
                case itemType::jsonStore:
                    if (value.type() != QJsonValue::Object) {
                        continue;
                    }
                    ((JsonStore *) item->ptr)->FromJson(value.toObject());
                    break;
            }
        }

        if (callback_after_load != nullptr) callback_after_load();
    }

    void JsonStore::FromJsonBytes(const QByteArray &data) {
        QJsonParseError error{};
        auto document = QJsonDocument::fromJson(data, &error);

        if (error.error != error.NoError) {
            qDebug() << "QJsonParseError" << error.errorString();
            return;
        }

        FromJson(document.object());
    }

    bool JsonStore::Save() {
        if (callback_before_save != nullptr) callback_before_save();
        if (save_control_no_save) return false;

        auto save_content = ToJsonBytes();
        auto changed = last_save_content_hash != QCryptographicHash::hash(save_content, QCryptographicHash::Md5);
        last_save_content_hash = QCryptographicHash::hash(save_content, QCryptographicHash::Md5);

        QFile file;
        file.setFileName(fn);
        file.open(QIODevice::ReadWrite | QIODevice::Truncate);
        file.write(save_content);
        file.close();

        return changed;
    }

    bool JsonStore::Load(const QString& content) {
        if (!content.isEmpty()) {
            FromJsonBytes(content.toUtf8());
            return true;
        }
        QFile file;
        file.setFileName(fn);

        if (!file.exists() && !load_control_must) {
            return false;
        }

        bool ok = file.open(QIODevice::ReadOnly);
        if (!ok) {
            MessageBoxWarning("error", "can not open config " + fn + "\n" + file.errorString());
        } else {
            QByteArray data = file.readAll();
            last_save_content_hash = QCryptographicHash::hash(data, QCryptographicHash::Md5);
            FromJsonBytes(data);
        }

        file.close();
        return ok;
    }
} // namespace Configs_ConfigItem

    // System Utils
namespace Configs {
    void initDB(const std::string& dbPath) {
        dataManager = new DatabaseManager(dbPath);
    }

    QString FindCoreRealPath() {
        auto fn = QApplication::applicationDirPath() + "/Core";
#ifdef Q_OS_WIN
        fn += ".exe";
#endif
        auto fi = QFileInfo(fn);
        QString path;
        if (fi.isSymLink()) path =  fi.symLinkTarget();
        path = fn;
#ifdef Q_OS_WIN
        path.replace("/", "\\");
#endif
        return path;
    }

    short isAdminCache = -1;

    bool isSetuidSet(const std::string& path) {
#ifdef Q_OS_MACOS
        struct stat fileInfo;

        if (stat(path.c_str(), &fileInfo) != 0) {
            return false;
        }

        if (fileInfo.st_mode & S_ISUID) {
            return true;
        } else {
            return false;
        }
#else
        return false;
#endif
    }

    // IsAdmin 主要判断：有无权限启动 Tun
    bool IsAdmin(bool forceRenew) {
        if (isAdminCache >= 0 && !forceRenew) return isAdminCache;

        bool admin = false;
#ifdef Q_OS_WIN
        admin = Windows_IsInAdmin();
        Configs::dataManager->settingsRepo->windows_set_admin = admin;
#else
        bool ok;
        auto isPrivileged = API::defaultClient->IsPrivileged(&ok);
        admin = ok && isPrivileged;
#endif
        isAdminCache = admin;
        return admin;
    };

    QString GetBasePath() {
        if (Configs::dataManager->settingsRepo->flag_use_appdata) return QStandardPaths::writableLocation(
              QStandardPaths::AppConfigLocation);
        return qApp->applicationDirPath();
    }
} // namespace Configs
