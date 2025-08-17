#pragma once

#include "include/dataStore/ProxyEntity.hpp"
#include "include/sys/Process.hpp"

namespace Configs {
    class ExtraCoreData
    {
    public:
        QString path;
        QString args;
        QString config;
        QString configDir;
        bool noLog;
    };

    class BuildConfigResult {
    public:
        QString error;
        QJsonObject coreConfig;
        std::shared_ptr<ExtraCoreData> extraCoreData;

        QList<std::shared_ptr<Stats::TrafficData>> outboundStats; // all, but not including "bypass" "block"
    };

    class BuildTestConfigResult {
    public:
        QString error;
        QMap<int, QString> fullConfigs;
        QMap<QString, int> tag2entID;
        QJsonObject coreConfig;
        QStringList outboundTags;
    };

    class BuildConfigStatus {
    public:
        std::shared_ptr<BuildConfigResult> result;
        std::shared_ptr<ProxyEntity> ent;
        int chainID = 0;
        bool forTest;
        bool forExport;

        // xxList is V2Ray format string list

        QStringList domainListDNSDirect;

        // config format

        QJsonArray routingRules;
        QJsonArray inbounds;
        QJsonArray outbounds;
        QJsonArray endpoints;
    };

    bool IsValid(const std::shared_ptr<ProxyEntity> &ent);

    std::shared_ptr<BuildTestConfigResult> BuildTestConfig(const QList<std::shared_ptr<ProxyEntity>>& profiles, const std::map<std::string, std::string>& ruleSetMap);

    std::shared_ptr<BuildConfigResult> BuildConfig(const std::shared_ptr<ProxyEntity> &ent, const std::map<std::string, std::string>& ruleSetMap, bool forTest, bool forExport, int chainID = 0);

    void BuildConfigSingBox(const std::shared_ptr<BuildConfigStatus> &status, const std::map<std::string, std::string>& ruleSetMap);

    QJsonObject BuildDnsObject(QString address, bool tunEnabled);

    QString BuildChain(int chainId, const std::shared_ptr<BuildConfigStatus> &status);

    QString BuildChainInternal(int chainId, const QList<std::shared_ptr<ProxyEntity>> &ents,
                               const std::shared_ptr<BuildConfigStatus> &status);

    void BuildOutbound(const std::shared_ptr<ProxyEntity> &ent, const std::shared_ptr<BuildConfigStatus> &status, QJsonObject& outbound, const QString& tag);

    inline QString get_jsdelivr_link(QString link)
    {
        if(dataStore->routing->ruleset_mirror == Mirrors::GITHUB)
            return link;
        if(auto url = QUrl(link); url.isValid() && url.host() == "raw.githubusercontent.com")
        {
            QStringList list = url.path().split('/');      
            QString result;
            switch(dataStore->routing->ruleset_mirror) {
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
} // namespace Configs
