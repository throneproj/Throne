#include "include/database/entities/IpScan.h"

#include <QCryptographicHash>
#include <QJsonArray>

#include <algorithm>
#include <limits>

namespace Configs {
    namespace {
        void ipScanReadBool(const QJsonObject &object, const char *key, bool &value) {
            const auto v = object.value(QLatin1String(key));
            if (v.isBool()) value = v.toBool();
        }

        void ipScanReadInt(const QJsonObject &object, const char *key, int &value, int min, int max) {
            const auto v = object.value(QLatin1String(key));
            if (v.isDouble()) value = static_cast<int>(std::clamp(v.toDouble(), double(min), double(max)));
        }

        QList<int> ipScanCleanPorts(const QList<int> &ports) {
            QList<int> out;
            for (const int port : ports) {
                if (port >= 1 && port <= 65535 && !out.contains(port)) out.append(port);
            }
            return out;
        }
    } // namespace

    QJsonObject ScanConfig::ToJson() const {
        QJsonObject object;
        QJsonArray portsArray;
        for (const int port : ports) portsArray.append(port);
        object["ports"] = portsArray;
        object["portMode"] = PortModeKey();
        object["shuffle"] = shuffle;
        object["ipv6"] = scanIPv6;
        object["stopAfter"] = stopAfter;
        object["concurrency"] = concurrency;
        object["spawnIntervalMs"] = spawnIntervalMs;

        QJsonObject warpObject;
        warpObject["profileId"] = warp.profileId;
        object["warp"] = warpObject;

        QJsonObject resolverObject;
        resolverObject["profileId"] = resolver.profileId;
        object["resolver"] = resolverObject;
        return object;
    }

    void ScanConfig::FromJson(const QJsonObject &object) {
        if (const auto v = object.value(QLatin1String("ports")); v.isArray()) {
            QList<int> list;
            for (const auto &item : v.toArray()) {
                if (item.isDouble()) list.append(item.toInt());
            }
            ports = ipScanCleanPorts(list);
        }
        ipScanReadBool(object, "shuffle", shuffle);
        if (const auto mode = object.value(QLatin1String("portMode")).toString(); !mode.isEmpty()) {
            if (mode == QLatin1String("ignore")) portMode = PortMode::Ignore;
            else if (mode == QLatin1String("list")) portMode = PortMode::List;
            else portMode = PortMode::Merge;
        }
        ipScanReadBool(object, "ipv6", scanIPv6);
        ipScanReadInt(object, "stopAfter", stopAfter, 0, 1000000000);
        ipScanReadInt(object, "concurrency", concurrency, 1, 1000);
        ipScanReadInt(object, "spawnIntervalMs", spawnIntervalMs, 0, 1000);

        // A scan from before the redesign may name no profile (it registered its own identity); Start then asks for one.
        const QJsonObject warpObject = object.value(QLatin1String("warp")).toObject();
        ipScanReadInt(warpObject, "profileId", warp.profileId, -1, std::numeric_limits<int>::max());

        const QJsonObject resolverObject = object.value(QLatin1String("resolver")).toObject();
        ipScanReadInt(resolverObject, "profileId", resolver.profileId, -1, std::numeric_limits<int>::max());
    }

    QString ScanConfig::PortModeKey() const {
        switch (portMode) {
        case PortMode::Ignore: return QStringLiteral("ignore");
        case PortMode::List: return QStringLiteral("list");
        case PortMode::Merge: break;
        }
        return QStringLiteral("merge");
    }

    QString ScanConfig::TargetSpecHash(int baseListId) const {
        // Stored order, not sorted: the core maps each position to ports[i % n], so a reorder is a different target set.
        QStringList portTexts;
        for (const int port : ipScanCleanPorts(ports)) portTexts << QString::number(port);
        const QString text = QStringLiteral("%1|%2|%3|%4|%5")
                                 .arg(baseListId)
                                 .arg(portTexts.join(QLatin1Char(',')))
                                 .arg(PortModeKey())
                                 .arg(shuffle ? 1 : 0)
                                 .arg(scanIPv6 ? 1 : 0);
        return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha1).toHex());
    }

    ScanConfig IpScan::DefaultConfig(Kind kind) {
        ScanConfig config;
        config.concurrency = 64;
        config.spawnIntervalMs = 2;
        if (kind == Kind::Resolver) {
            // A portless entry is probed on 53; ports never multiply resolver targets.
            config.ports.clear();
            config.portMode = ScanConfig::PortMode::List;
            // The seed lists are pre-shuffled below their first tier, which must go first.
            config.shuffle = false;
            config.stopAfter = 20;
        }
        return config;
    }
} // namespace Configs
