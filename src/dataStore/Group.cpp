#include <include/dataStore/Group.hpp>

#include "include/ui/profile/dialog_edit_profile.h"

namespace Configs
{
    Group::Group() {
        _add(new configItem("id", &id, itemType::integer));
        _add(new configItem("front_proxy_id", &front_proxy_id, itemType::integer));
        _add(new configItem("landing_proxy_id", &landing_proxy_id, itemType::integer));
        _add(new configItem("archive", &archive, itemType::boolean));
        _add(new configItem("skip_auto_update", &skip_auto_update, itemType::boolean));
        _add(new configItem("name", &name, itemType::string));
        _add(new configItem("profiles", &profiles, itemType::integerList));
        _add(new configItem("url", &url, itemType::string));
        _add(new configItem("info", &info, itemType::string));
        _add(new configItem("lastup", &sub_last_update, itemType::integer64));
        _add(new configItem("manually_column_width", &manually_column_width, itemType::boolean));
        _add(new configItem("column_width", &column_width, itemType::integerList));
    }

    QList<int> Group::Profiles() const {
        return profiles;
    }

    QList<std::shared_ptr<ProxyEntity>> Group::GetProfileEnts() const
    {
        auto res = QList<std::shared_ptr<ProxyEntity>>{};
        for (auto id : profiles)
        {
            res.append(profileManager->GetProfile(id));
        }
        return res;
    }


    bool Group::AddProfile(int id)
    {
        if (HasProfile(id))
        {
            return false;
        }
        profiles.append(id);
        return true;
    }

    bool Group::RemoveProfile(int id)
    {
        if (!HasProfile(id)) return false;
        profiles.removeAll(id);
        return true;
    }

    bool Group::SwapProfiles(int idx1, int idx2)
    {
        if (profiles.size() <= idx1 || profiles.size() <= idx2) return false;
        profiles.swapItemsAt(idx1, idx2);
        return true;
    }

    bool Group::HasProfile(int id) const
    {
        return profiles.contains(id);
    }
}
