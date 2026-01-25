#include <include/database/entities/Group.h>

namespace Configs
{
    QList<int> Group::Profiles() const {
        return profiles;
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

    bool Group::EmplaceProfile(int idx, int newIdx)
    {
        if (profiles.size() <= idx || profiles.size() <= newIdx) return false;
        profiles.insert(newIdx+1, profiles[idx]);
        if (idx < newIdx) profiles.remove(idx);
        else profiles.remove(idx+1);
        return true;
    }

    bool Group::HasProfile(int id) const
    {
        return profiles.contains(id);
    }
}
