#pragma once

#include "ProxyEntity.hpp"
#include "include/global/Configs.hpp"

namespace Configs
{
    class Group : public JsonStore {
    public:
        int id = -1;
        bool archive = false;
        bool skip_auto_update = false;
        QString name = "";
        QString url = "";
        QString info = "";
        qint64 sub_last_update = 0;
        int front_proxy_id = -1;
        int landing_proxy_id = -1;

        // list ui
        bool manually_column_width = false;
        QList<int> column_width;
        QList<int> profiles;

        Group();

        [[nodiscard]] QList<int> Profiles() const;

        [[nodiscard]] QList<std::shared_ptr<ProxyEntity>> GetProfileEnts() const;

        bool RemoveProfile(int id);

        bool AddProfile(int id);

        bool SwapProfiles(int idx1, int idx2);

        bool HasProfile(int id) const;
    };
}// namespace Configs
