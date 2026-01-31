#pragma once
#include <QList>
#include <QMutex>
#include <QString>

#include "include/ui/group/GroupSort.hpp"

namespace Configs
{
    class Group {
    public:
        QMutex mutex;
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
        QList<int> column_width;
        QList<int> profiles;
        /// Last profile index (row in profiles list / QTableView) for scroll position; -1 = none
        int scroll_last_profile = -1;

        Group() = default;

        [[nodiscard]] QList<int> Profiles() const;

        bool SortProfiles(GroupSortAction method);

        bool RemoveProfile(int id);

        bool AddProfile(int id);

        bool SwapProfiles(int idx1, int idx2);

        bool EmplaceProfile(int idx, int newIdx);

        bool HasProfile(int id) const;
    };
}// namespace Configs
