#include <include/database/entities/Group.h>

#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"
#include <QRegularExpression> 

namespace Configs
{
    QJsonObject SubUserInfo::toJson() const {
        QJsonObject json;
        if (!valid) return json;
        json["valid"] = valid;
        json["has_quota"] = has_quota;
        if (upload > 0) json["upload"] = upload;
        if (download > 0) json["download"] = download;
        if (total > 0) json["total"] = total;
        if (expire > 0) json["expire"] = expire;
        if (!title.isEmpty()) json["title"] = title;
        if (!web_url.isEmpty()) json["web_url"] = web_url;
        if (!support_url.isEmpty()) json["support_url"] = support_url;
        if (!announce.isEmpty()) json["announce"] = announce;
        if (server_interval > 0) json["server_interval"] = server_interval;
        return json;
    }

    SubUserInfo SubUserInfo::fromJson(const QJsonObject &json) {
        SubUserInfo res;
        if (json.isEmpty()) return res;
        res.valid = json["valid"].toBool(false);
        res.has_quota = json["has_quota"].toBool(false);
        res.upload = json["upload"].toVariant().toLongLong();
        res.download = json["download"].toVariant().toLongLong();
        res.total = json["total"].toVariant().toLongLong();
        res.expire = json["expire"].toVariant().toLongLong();
        res.title = json["title"].toString();
        res.web_url = json["web_url"].toString();
        res.support_url = json["support_url"].toString();
        res.announce = json["announce"].toString();
        res.server_interval = json["server_interval"].toInt(0);
        return res;
    }

    SubUserInfo ParseSubUserInfo(const QString &info) {
        SubUserInfo result;
        if (info.trimmed().isEmpty()) return result;

        static const QRegularExpression totalRe(R"((?:^|[;,\s])total=(\d+))", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression uploadRe(R"((?:^|[;,\s])upload=(\d+))", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression downloadRe(R"((?:^|[;,\s])download=(\d+))", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression expireRe(R"((?:^|[;,\s])expire=(\d+))", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression titleRe(R"((?:^|[;\s])title=([^;\n]+))", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression webUrlRe(R"((?:^|[;\s])(?:web_url|url)=([^;\n\s]+))", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression supportUrlRe(R"((?:^|[;\s])support_url=([^;\n\s]+))", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression announceRe(R"((?:^|[;\s])announce=(.+)$)", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression intervalRe(R"((?:^|[;\s])interval=(\d+))", QRegularExpression::CaseInsensitiveOption);

        auto mTotal = totalRe.match(info);
        if (mTotal.hasMatch()) {
            result.total = mTotal.captured(1).toLongLong();
            result.has_quota = true;
            result.valid = true;
        }

        auto mUpload = uploadRe.match(info);
        if (mUpload.hasMatch()) {
            result.upload = mUpload.captured(1).toLongLong();
            result.has_quota = true;
            result.valid = true;
        }

        auto mDownload = downloadRe.match(info);
        if (mDownload.hasMatch()) {
            result.download = mDownload.captured(1).toLongLong();
            result.has_quota = true;
            result.valid = true;
        }

        auto mExpire = expireRe.match(info);
        if (mExpire.hasMatch()) {
            result.expire = mExpire.captured(1).toLongLong();
            if (result.expire > 1000000000000LL) {
                result.expire /= 1000;
            }
            result.has_quota = true;
            result.valid = true;
        }

        auto mTitle = titleRe.match(info);
        if (mTitle.hasMatch()) {
            result.title = mTitle.captured(1).trimmed();
            result.valid = true;
        }

        auto mWeb = webUrlRe.match(info);
        if (mWeb.hasMatch()) {
            result.web_url = mWeb.captured(1).trimmed();
            result.valid = true;
        }

        auto mSup = supportUrlRe.match(info);
        if (mSup.hasMatch()) {
            result.support_url = mSup.captured(1).trimmed();
            result.valid = true;
        }

        auto mAnnounce = announceRe.match(info);
        if (mAnnounce.hasMatch()) {
            QString ann = mAnnounce.captured(1).trimmed();
            if (!ann.isEmpty() && ann.compare("base64:", Qt::CaseInsensitive) != 0) {
                result.announce = ann;
                result.valid = true;
            }
        }

        auto mInterval = intervalRe.match(info);
        if (mInterval.hasMatch()) {
            result.server_interval = mInterval.captured(1).toInt();
            result.valid = true;
        }

        return result;
    }

    QJsonObject SubscriptionOptions::ToJson() const {
        QJsonObject json;
        if (!user_agent.isEmpty()) json["user_agent"] = user_agent;
        if (send_hwid != sendHwid::keepDefault) json["send_hwid"] = static_cast<int>(send_hwid);
        if (!hwid.isEmpty()) json["hwid"] = hwid;
        if (!hwid_os.isEmpty()) json["hwid_os"] = hwid_os;
        if (!hwid_os_version.isEmpty()) json["hwid_os_version"] = hwid_os_version;
        if (!hwid_model.isEmpty()) json["hwid_model"] = hwid_model;
        if (keep_working) json["keep_working"] = true;
        if (remove_duplicates) json["remove_duplicates"] = true;
        if (remove_insecure) json["remove_insecure"] = true;
        if (remove_invalid) json["remove_invalid"] = true;
        if (url_test) json["url_test"] = true;
        if (remove_unavailable) json["remove_unavailable"] = true;
        if (sort_by_latency) json["sort_by_latency"] = true;
        return json;
    }

    SubscriptionOptions SubscriptionOptions::FromJson(const QJsonObject &json) {
        SubscriptionOptions options;
        options.user_agent = json["user_agent"].toString();
        const int mode = json["send_hwid"].toInt();
        if (mode == static_cast<int>(sendHwid::on) || mode == static_cast<int>(sendHwid::off)) {
            options.send_hwid = static_cast<sendHwid>(mode);
        }
        options.hwid = json["hwid"].toString();
        options.hwid_os = json["hwid_os"].toString();
        options.hwid_os_version = json["hwid_os_version"].toString();
        options.hwid_model = json["hwid_model"].toString();
        options.keep_working = json["keep_working"].toBool();
        options.remove_duplicates = json["remove_duplicates"].toBool();
        options.remove_insecure = json["remove_insecure"].toBool();
        options.remove_invalid = json["remove_invalid"].toBool();
        options.url_test = json["url_test"].toBool();
        options.remove_unavailable = json["remove_unavailable"].toBool();
        options.sort_by_latency = json["sort_by_latency"].toBool();
        return options;
    }

    void Group::clearCalculatedColumnWidth() {
        calculated_column_width.clear();
    }

    QList<int> Group::Profiles() const {
        return profiles;
    }

    double bitrateToBps(const QString& str)
    {
        if (str.endsWith("Gbps", Qt::CaseInsensitive)) {
            double val = str.left(str.size() - 4).toDouble();
            return val * 1e9;
        }
        if (str.endsWith("Mbps", Qt::CaseInsensitive)) {
            double val = str.left(str.size() - 4).toDouble();
            return val * 1e6;
        }
        if (str.endsWith("Kbps", Qt::CaseInsensitive)) {
            double val = str.left(str.size() - 4).toDouble();
            return val * 1e3;
        }
        if (str == "N/A") return -1;
        return 0.0;
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
            case GroupSortMethod::ByLatency:
            case GroupSortMethod::ByTraffic:
            case GroupSortMethod::BySecurity:
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
                                      } else if (sortAction.method == GroupSortMethod::BySecurity) {
                                          auto secA = profA->outbound->GetSecurity();
                                          auto secB = profB->outbound->GetSecurity();
                                          if (secA.level != secB.level) {
                                              return sortAction.descending ? secA.level > secB.level
                                                                           : secA.level < secB.level;
                                          }
                                          ms_a = secA.transport + secA.label;
                                          ms_b = secB.transport + secB.label;
                                      } else if (sortAction.method == GroupSortMethod::ByTestResult || sortAction.method == GroupSortMethod::ByLatency) {
                                          if (test_sort_by == testBy::latency || sortAction.method == GroupSortMethod::ByLatency) {
                                              return sortAction.descending ? get_latency_for_sort(profA) > get_latency_for_sort(profB) : get_latency_for_sort(profA) < get_latency_for_sort(profB);
                                          }
                                          if (test_sort_by == testBy::dlSpeed) {
                                              return sortAction.descending ? bitrateToBps(profA->dl_speed) > bitrateToBps(profB->dl_speed) : bitrateToBps(profA->dl_speed) < bitrateToBps(profB->dl_speed);
                                          }
                                          if (test_sort_by == testBy::ulSpeed) {
                                              return sortAction.descending ? bitrateToBps(profA->ul_speed) > bitrateToBps(profB->ul_speed) : bitrateToBps(profA->ul_speed) < bitrateToBps(profB->ul_speed);
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
