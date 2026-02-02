#include <include/database/entities/Group.h>

#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"

namespace Configs
{
    QList<int> Group::Profiles() const {
        return profiles;
    }

    bool Group::SortProfiles(GroupSortAction sortAction) {
        if (!mutex.tryLock()) {
            return false;
        }
        auto allProfs = dataManager->profilesRepo->GetProfileBatch(profiles); // to warm up the cache
        switch (sortAction.method) {
            case GroupSortMethod::Raw: {
                break;
            }
            case GroupSortMethod::ById: {
                break;
            }
            case GroupSortMethod::ByAddress:
            case GroupSortMethod::ByName:
            case GroupSortMethod::ByLatency:
            case GroupSortMethod::ByType: {
                std::ranges::sort(profiles,
                                  [&](int a, int b) {
                                      auto profA = dataManager->profilesRepo->GetProfile(a);
                                      auto profB = dataManager->profilesRepo->GetProfile(b);
                                      QString ms_a;
                                      QString ms_b;
                                      if (sortAction.method == GroupSortMethod::ByType) {
                                          ms_a = profA->outbound->DisplayType();
                                          ms_b = profB->outbound->DisplayType();
                                      } else if (sortAction.method == GroupSortMethod::ByName) {
                                          ms_a = profA->outbound->name;
                                          ms_b = profB->outbound->name;
                                      } else if (sortAction.method == GroupSortMethod::ByAddress) {
                                          ms_a = profA->outbound->DisplayAddress();
                                          ms_b = profB->outbound->DisplayAddress();
                                      } else if (sortAction.method == GroupSortMethod::ByLatency) {
                                          ms_a = profA->full_test_report;
                                          ms_b = profB->full_test_report;
                                      }
                                      auto get_latency_for_sort = [](const std::shared_ptr<Profile>& prof) {
                                          auto i = prof->latency;
                                          if (i == 0) i = 100000;
                                          if (i < 0) i = 99999;
                                          return i;
                                      };
                                      if (sortAction.descending) {
                                          if (sortAction.method == GroupSortMethod::ByLatency) {
                                              if (ms_a.isEmpty() && ms_b.isEmpty()) {
                                                  // compare latency if full_test_report is empty
                                                  return get_latency_for_sort(profA) > get_latency_for_sort(profB);
                                              }
                                          }
                                          return ms_a > ms_b;
                                      } else {
                                          if (sortAction.method == GroupSortMethod::ByLatency) {
                                              if (ms_a.isEmpty() && ms_b.isEmpty()) {
                                                  // compare latency if full_test_report is empty
                                                  return get_latency_for_sort(profA) < get_latency_for_sort(profB);
                                              }
                                          }
                                          return ms_a < ms_b;
                                      }
                                  });
                break;
            }
        }
        mutex.unlock();
        return true;
    }

    bool Group::AddProfile(int id)
    {
        QMutexLocker locker(&mutex);
        if (HasProfile(id))
        {
            return false;
        }
        column_width.clear();
        profiles.append(id);
        return true;
    }

    bool Group::RemoveProfile(int id)
    {
        QMutexLocker locker(&mutex);
        if (!HasProfile(id)) return false;
        profiles.removeAll(id);
        return true;
    }

    bool Group::SwapProfiles(int idx1, int idx2)
    {
        QMutexLocker locker(&mutex);
        if (profiles.size() <= idx1 || profiles.size() <= idx2) return false;
        profiles.swapItemsAt(idx1, idx2);
        return true;
    }

    bool Group::EmplaceProfile(int idx, int newIdx)
    {
        QMutexLocker locker(&mutex);
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
