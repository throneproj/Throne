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
            case GroupSortMethod::ByTestResult:
            case GroupSortMethod::ByTraffic:
            case GroupSortMethod::ByType: {
                auto get_latency_for_sort = [](const std::shared_ptr<Profile>& prof) {
                    auto i = prof->latency;
                    if (i == 0) i = 100000;
                    if (i < 0) i = 99999;
                    return i;
                };
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
                                      } else if (sortAction.method == GroupSortMethod::ByTestResult) {
                                          if (test_sort_by == testBy::latency) {
                                              return sortAction.descending ? get_latency_for_sort(profA) > get_latency_for_sort(profB) : get_latency_for_sort(profA) < get_latency_for_sort(profB);
                                          }
                                          if (test_sort_by == testBy::dlSpeed) {
                                              return sortAction.descending ? profA->dl_speed > profB->dl_speed : profA->dl_speed < profB->dl_speed;
                                          }
                                          if (test_sort_by == testBy::ulSpeed) {
                                              return sortAction.descending ? profA->ul_speed > profB->ul_speed : profA->ul_speed < profB->ul_speed;
                                          }
                                          if (test_sort_by == testBy::ipOut) {
                                              return sortAction.descending ? profA->ip_out > profB->ip_out : profA->ip_out < profB->ip_out;
                                          }
                                      } else if (sortAction.method == GroupSortMethod::ByTraffic) {
                                          if (traffic_sort_by == trafficBy::total) {
                                              auto totalA = profA->traffic_downlink + profA->traffic_uplink;
                                              auto totalB = profB->traffic_downlink + profB->traffic_uplink;
                                              return sortAction.descending ? totalA > totalB  : totalA < totalB;
                                          }
                                          if (traffic_sort_by == trafficBy::dl) {
                                              return sortAction.descending ? profA->traffic_downlink > profB->traffic_downlink : profA->traffic_downlink < profB->traffic_downlink;
                                          }
                                          if (traffic_sort_by == trafficBy::ul) {
                                              return sortAction.descending ? profA->traffic_uplink > profB->traffic_uplink : profA->traffic_uplink < profB->traffic_uplink;
                                          }
                                      }
                                      return sortAction.descending ? ms_a > ms_b : ms_a < ms_b;
                                  });
                break;
            }
        }
        mutex.unlock();
        return true;
    }

    bool Group::AddProfile(int ID)
    {
        QMutexLocker locker(&mutex);
        if (HasProfile(ID))
        {
            return false;
        }
        profiles.append(ID);
        return true;
    }

    bool Group::AddProfileBatch(const QList<int>& IDs) {
        QSet<int> currentProfiles;
        for (const auto& profileID : profiles) {
            currentProfiles.insert(profileID);
        }
        QMutexLocker locker(&mutex);
        for (auto profileID : IDs) {
            if (!currentProfiles.contains(profileID)) {
                profiles.append(profileID);
            }
        }
        return true;
    }

    bool Group::RemoveProfile(int ID)
    {
        QMutexLocker locker(&mutex);
        if (!HasProfile(ID)) return false;
        profiles.removeAll(ID);
        return true;
    }

    bool Group::RemoveProfileBatch(const QList<int>& IDs) {
        QSet<int> toDel;
        for (auto ID : IDs) {
            toDel.insert(ID);
        }
        QList<int> newIDs;
        QMutexLocker locker(&mutex);
        for (auto inID : profiles) {
            if (!toDel.contains(inID)) {
                newIDs.append(inID);
            }
        }
        profiles = newIDs;
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

    bool Group::HasProfile(int ID) const
    {
        return profiles.contains(ID);
    }
}
