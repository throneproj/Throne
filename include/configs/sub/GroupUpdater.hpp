#pragma once

#include <include/database/entities/Profile.h>

namespace Subscription {
    enum class SingBoxSubType {
        fullConfig,
        outboundInJson,
        outboundJsonArray,
        outboundObject,
        invalid,
    };
    enum class XraySubType {
        fullConfig,
        fullConfigJsonArray,
        outboundInJson,
        outboundJsonArray,
        outboundObject,
        invalid,
    };
    class RawUpdater {
    public:
        void update(const QString &str, bool needParse = true, bool isBase64Decoded = false);

        void updateSingBox(const QJsonDocument &doc, SingBoxSubType type);

        void updateXray(const QJsonDocument &doc, XraySubType type);

        void updateClash(const QString &str);

        void updateWireguardFileConfig(const QString &str);

        void updateSIP008(const QString &str);

        int gid_add_to = -1;

        QList<std::shared_ptr<Configs::Profile>> updated_order;
    };

    class GroupUpdater : public QObject {
        Q_OBJECT

    public:
        void AsyncUpdate(const QString &str, int _sub_gid = -1, const std::function<void()> &finish = nullptr);

        void Update(const QString &_str, int _sub_gid = -1, bool _not_sub_as_url = false);

    signals:

        void asyncUpdateCallback(int gid);
    };

    extern GroupUpdater *groupUpdater;
} // namespace Subscription

void UI_update_all_groups(bool onlyAllowed = false);
