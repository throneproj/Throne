#pragma once
#include <QJsonArray>
#include <QJsonObject>

#include "include/database/entities/Profile.h"

namespace Configs
{
    class ExtraCoreData
    {
        public:
        QString path;
        QString args;
        QString config;
        bool noLog = false;
    };

    struct TrafficChainGroup {
        QString watchTag;
        QList<std::shared_ptr<Profile>> profiles;
    };

    struct AutoSelectorBuildInfo {
        QString groupTag;
        std::shared_ptr<Profile> profile;
        QList<QPair<QString, std::shared_ptr<Profile>>> members;
    };

    class BuildConfigResult {
    public:
        QString error;
        QJsonObject coreConfig;
        QString tunIPv4CIDR;
        bool isXrayNeeded = false;
        // True when a child process or generated outbound can create network
        // traffic whose routing and DNS behavior Throne cannot constrain.
        bool hasUnverifiableNetworkConfig = false;
        QJsonObject xrayConfig;
        // Opaque full configs, one instance each; never merged into xrayConfig.
        QStringList xrayFullConfigs;
        std::shared_ptr<ExtraCoreData> extraCoreData = std::make_shared<ExtraCoreData>();

        QList<TrafficChainGroup> chainGroups;
        QList<AutoSelectorBuildInfo> autoSelectors;
    };

    class BuildTestConfigResult {
    public:
        QString error;
        QMap<int, QString> fullConfigs;
        QStringList xrayFullConfigs;
        QMap<QString, int> tag2entID;
        QJsonObject coreConfig;
        QJsonObject xrayConfig;
        bool isXrayNeeded = false;
        QStringList outboundTags;
    };

    inline QString get_jsdelivr_link(QString link)
    {
        if(Configs::dataManager->settingsRepo->ruleset_mirror == Mirrors::GITHUB)
            return link;
        if(auto url = QUrl(link); url.isValid() && url.host() == "raw.githubusercontent.com")
        {
            QStringList list = url.path().split('/');
            QString result;
            switch(Configs::dataManager->settingsRepo->ruleset_mirror) {
            case Mirrors::GCORE: result = "https://gcore.jsdelivr.net/gh"; break;
            case Mirrors::QUANTIL: result = "https://quantil.jsdelivr.net/gh"; break;
            case Mirrors::FASTLY: result = "https://fastly.jsdelivr.net/gh"; break;
            case Mirrors::CDN: result = "https://cdn.jsdelivr.net/gh"; break;
            default: result = "https://testingcf.jsdelivr.net/gh";
            }

            int index = 0;
            foreach(QString item, list)
            {
                if(!item.isEmpty())
                {
                    if(index == 2)
                        result += "@" + item;
                    else
                        result += "/" + item;
                    index++;
                }
            }
            return result;
        }
        return link;
    }

    constexpr int warpProfileID = -2408;

    std::shared_ptr<BuildConfigResult> BuildSingBoxConfig(const std::shared_ptr<Profile> &ent);

    bool IsValid(const std::shared_ptr<Profile> &ent);

    std::shared_ptr<BuildTestConfigResult> BuildTestConfig(const QList<std::shared_ptr<Profile> > &profiles);
}
