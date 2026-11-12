#include "include/database/entities/IpScan.h"
#include "include/scanner/WarpPresets.h"

#include <QCryptographicHash>
#include <QJsonArray>

#include <algorithm>
#include <initializer_list>
#include <limits>

namespace Configs {
    namespace {
        constexpr int kIpScanMinTimeoutMs = 100;
        constexpr int kIpScanMaxTimeoutMs = 60000;
        // The core's own bound on one resolver probe.
        constexpr int kIpScanMaxResolverTimeoutMs = 300000;

        void ipScanReadBool(const QJsonObject &object, const char *key, bool &value) {
            const auto v = object.value(QLatin1String(key));
            if (v.isBool()) value = v.toBool();
        }

        void ipScanReadInt(const QJsonObject &object, const char *key, int &value, int min, int max) {
            const auto v = object.value(QLatin1String(key));
            if (v.isDouble()) value = static_cast<int>(std::clamp(v.toDouble(), double(min), double(max)));
        }

        void ipScanReadString(const QJsonObject &object, const char *key, QString &value) {
            const auto v = object.value(QLatin1String(key));
            if (v.isString()) value = v.toString();
        }

        void ipScanReadChoice(const QJsonObject &object, const char *key, QString &value,
                              std::initializer_list<const char *> allowed) {
            const auto v = object.value(QLatin1String(key));
            if (!v.isString()) return;
            const QString text = v.toString().trimmed();
            for (const char *choice : allowed) {
                if (text.compare(QLatin1String(choice), Qt::CaseInsensitive) == 0) {
                    value = QLatin1String(choice);
                    return;
                }
            }
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

        QJsonObject icmpObject;
        icmpObject["enabled"] = icmp.enabled;
        icmpObject["timeoutMs"] = icmp.timeoutMs;
        icmpObject["count"] = icmp.count;
        object["icmp"] = icmpObject;

        QJsonObject tcpObject;
        tcpObject["enabled"] = tcp.enabled;
        tcpObject["timeoutMs"] = tcp.timeoutMs;
        tcpObject["attempts"] = tcp.attempts;
        object["tcp"] = tcpObject;

        QJsonObject httpObject;
        httpObject["enabled"] = http.enabled;
        httpObject["tls"] = http.tls;
        httpObject["serverName"] = http.serverName;
        httpObject["host"] = http.host;
        httpObject["path"] = http.path;
        httpObject["method"] = http.method;
        httpObject["httpVersion"] = http.httpVersion;
        httpObject["alpn"] = QJsonArray::fromStringList(http.alpn);
        httpObject["minVersion"] = http.minVersion;
        httpObject["maxVersion"] = http.maxVersion;
        httpObject["fingerprint"] = http.fingerprint;
        httpObject["insecure"] = http.insecure;
        httpObject["disableSni"] = http.disableSni;
        httpObject["fragment"] = http.fragment;
        httpObject["fragmentFallbackDelayMs"] = http.fragmentFallbackDelayMs;
        httpObject["recordFragment"] = http.recordFragment;
        httpObject["mixedCaseSni"] = http.mixedCaseSni;
        httpObject["timeoutMs"] = http.timeoutMs;
        object["http"] = httpObject;

        QJsonObject configObject;
        configObject["enabled"] = config.enabled;
        configObject["profileId"] = config.profileId;
        configObject["url"] = config.url;
        configObject["timeoutMs"] = config.timeoutMs;
        configObject["warmLatency"] = config.warmLatency;
        object["config"] = configObject;

        QJsonObject warpObject;
        warpObject["mode"] = warp.mode;
        warpObject["httpMode"] = warp.httpMode;
        warpObject["identity"] = warp.identity;
        warpObject["profileId"] = warp.profileId;
        warpObject["generatedIdentity"] = warp.generatedIdentity;
        warpObject["generatedMode"] = warp.generatedMode;
        warpObject["mtu"] = warp.mtu;
        warpObject["sni"] = warp.sni;
        warpObject["quicOpener"] = warp.quicOpener;
        warpObject["jc"] = warp.jc;
        warpObject["jmin"] = warp.jmin;
        warpObject["jmax"] = warp.jmax;
        warpObject["i1"] = warp.i1;
        warpObject["i2"] = warp.i2;
        warpObject["i3"] = warp.i3;
        warpObject["i4"] = warp.i4;
        warpObject["i5"] = warp.i5;
        warpObject["url"] = warp.url;
        warpObject["timeoutMs"] = warp.timeoutMs;
        warpObject["warmLatency"] = warp.warmLatency;
        object["warp"] = warpObject;

        QJsonObject resolverObject;
        resolverObject["profileId"] = resolver.profileId;
        resolverObject["level"] = resolver.level;
        resolverObject["timeoutMs"] = resolver.timeoutMs;
        object["resolver"] = resolverObject;

        QJsonObject tuneObject;
        tuneObject["profileId"] = tune.profileId;
        tuneObject["fingerprints"] = tune.fingerprints;
        tuneObject["fingerprintList"] = tune.fingerprintList;
        tuneObject["fragment"] = tune.fragment;
        tuneObject["fragmentDelays"] = tune.fragmentDelays;
        tuneObject["recordFragment"] = tune.recordFragment;
        tuneObject["mixedCaseSni"] = tune.mixedCaseSni;
        tuneObject["quicOpener"] = tune.quicOpener;
        tuneObject["mtu"] = tune.mtu;
        tuneObject["mtuMin"] = tune.mtuMin;
        tuneObject["mtuMax"] = tune.mtuMax;
        tuneObject["mtuStep"] = tune.mtuStep;
        tuneObject["mtuUrl"] = tune.mtuUrl;
        tuneObject["mtuBytes"] = tune.mtuBytes;
        tuneObject["attempts"] = tune.attempts;
        tuneObject["perMinute"] = tune.perMinute;
        tuneObject["maxAttempts"] = tune.maxAttempts;
        tuneObject["maxMinutes"] = tune.maxMinutes;
        tuneObject["url"] = tune.url;
        tuneObject["timeoutMs"] = tune.timeoutMs;
        object["tune"] = tuneObject;
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

        const QJsonObject icmpObject = object.value(QLatin1String("icmp")).toObject();
        ipScanReadBool(icmpObject, "enabled", icmp.enabled);
        ipScanReadInt(icmpObject, "timeoutMs", icmp.timeoutMs, kIpScanMinTimeoutMs, kIpScanMaxTimeoutMs);
        ipScanReadInt(icmpObject, "count", icmp.count, 1, 10);

        const QJsonObject tcpObject = object.value(QLatin1String("tcp")).toObject();
        ipScanReadBool(tcpObject, "enabled", tcp.enabled);
        ipScanReadInt(tcpObject, "timeoutMs", tcp.timeoutMs, kIpScanMinTimeoutMs, kIpScanMaxTimeoutMs);
        ipScanReadInt(tcpObject, "attempts", tcp.attempts, 1, 10);

        const QJsonObject httpObject = object.value(QLatin1String("http")).toObject();
        ipScanReadBool(httpObject, "enabled", http.enabled);
        ipScanReadBool(httpObject, "tls", http.tls);
        ipScanReadString(httpObject, "serverName", http.serverName);
        ipScanReadString(httpObject, "host", http.host);
        ipScanReadString(httpObject, "path", http.path);
        ipScanReadChoice(httpObject, "method", http.method, {"GET", "HEAD", "NONE"});
        ipScanReadChoice(httpObject, "httpVersion", http.httpVersion, {"1.1", "2", "3"});
        if (const auto v = httpObject.value(QLatin1String("alpn")); v.isArray()) {
            QStringList alpn;
            for (const auto &item : v.toArray()) {
                const QString value = item.toString().trimmed();
                if (!value.isEmpty() && !alpn.contains(value)) alpn.append(value);
            }
            http.alpn = alpn;
        }
        ipScanReadChoice(httpObject, "minVersion", http.minVersion, {"", "1.0", "1.1", "1.2", "1.3"});
        ipScanReadChoice(httpObject, "maxVersion", http.maxVersion, {"", "1.0", "1.1", "1.2", "1.3"});
        ipScanReadString(httpObject, "fingerprint", http.fingerprint);
        ipScanReadBool(httpObject, "insecure", http.insecure);
        ipScanReadBool(httpObject, "disableSni", http.disableSni);
        ipScanReadBool(httpObject, "fragment", http.fragment);
        ipScanReadInt(httpObject, "fragmentFallbackDelayMs", http.fragmentFallbackDelayMs, 0, kIpScanMaxTimeoutMs);
        ipScanReadBool(httpObject, "recordFragment", http.recordFragment);
        ipScanReadBool(httpObject, "mixedCaseSni", http.mixedCaseSni);
        ipScanReadInt(httpObject, "timeoutMs", http.timeoutMs, kIpScanMinTimeoutMs, kIpScanMaxTimeoutMs);

        const QJsonObject configObject = object.value(QLatin1String("config")).toObject();
        ipScanReadBool(configObject, "enabled", config.enabled);
        ipScanReadInt(configObject, "profileId", config.profileId, -1, std::numeric_limits<int>::max());
        ipScanReadString(configObject, "url", config.url);
        ipScanReadInt(configObject, "timeoutMs", config.timeoutMs, kIpScanMinTimeoutMs, kIpScanMaxTimeoutMs);
        ipScanReadBool(configObject, "warmLatency", config.warmLatency);

        const QJsonObject warpObject = object.value(QLatin1String("warp")).toObject();
        ipScanReadChoice(warpObject, "mode", warp.mode, {"wireguard", "amneziawg", "masque"});
        ipScanReadInt(warpObject, "httpMode", warp.httpMode, 0, 2);
        ipScanReadChoice(warpObject, "identity", warp.identity, {"generated", "profile", "builtin"});
        ipScanReadInt(warpObject, "profileId", warp.profileId, -1, std::numeric_limits<int>::max());
        if (const auto v = warpObject.value(QLatin1String("generatedIdentity")); v.isObject())
            warp.generatedIdentity = v.toObject();
        ipScanReadChoice(warpObject, "generatedMode", warp.generatedMode, {"", "wireguard", "masque"});
        ipScanReadInt(warpObject, "mtu", warp.mtu, 576, 9000);
        ipScanReadString(warpObject, "sni", warp.sni);
        ipScanReadBool(warpObject, "quicOpener", warp.quicOpener);
        ipScanReadInt(warpObject, "jc", warp.jc, 0, 128);
        ipScanReadInt(warpObject, "jmin", warp.jmin, 0, 1280);
        ipScanReadInt(warpObject, "jmax", warp.jmax, 0, 1280);
        ipScanReadString(warpObject, "i1", warp.i1);
        ipScanReadString(warpObject, "i2", warp.i2);
        ipScanReadString(warpObject, "i3", warp.i3);
        ipScanReadString(warpObject, "i4", warp.i4);
        ipScanReadString(warpObject, "i5", warp.i5);
        ipScanReadString(warpObject, "url", warp.url);
        ipScanReadInt(warpObject, "timeoutMs", warp.timeoutMs, kIpScanMinTimeoutMs, kIpScanMaxTimeoutMs);
        ipScanReadBool(warpObject, "warmLatency", warp.warmLatency);

        const QJsonObject resolverObject = object.value(QLatin1String("resolver")).toObject();
        ipScanReadInt(resolverObject, "profileId", resolver.profileId, -1, std::numeric_limits<int>::max());
        ipScanReadChoice(resolverObject, "level", resolver.level, {"", "poll", "qname", "handshake", "quick", "full"});
        ipScanReadInt(resolverObject, "timeoutMs", resolver.timeoutMs, 0, kIpScanMaxResolverTimeoutMs);
        if (resolver.timeoutMs > 0) resolver.timeoutMs = std::max(resolver.timeoutMs, kIpScanMinTimeoutMs);

        const QJsonObject tuneObject = object.value(QLatin1String("tune")).toObject();
        ipScanReadInt(tuneObject, "profileId", tune.profileId, -1, std::numeric_limits<int>::max());
        ipScanReadBool(tuneObject, "fingerprints", tune.fingerprints);
        ipScanReadString(tuneObject, "fingerprintList", tune.fingerprintList);
        ipScanReadBool(tuneObject, "fragment", tune.fragment);
        ipScanReadString(tuneObject, "fragmentDelays", tune.fragmentDelays);
        ipScanReadBool(tuneObject, "recordFragment", tune.recordFragment);
        ipScanReadBool(tuneObject, "mixedCaseSni", tune.mixedCaseSni);
        ipScanReadBool(tuneObject, "quicOpener", tune.quicOpener);
        ipScanReadBool(tuneObject, "mtu", tune.mtu);
        ipScanReadInt(tuneObject, "mtuMin", tune.mtuMin, 576, 9000);
        ipScanReadInt(tuneObject, "mtuMax", tune.mtuMax, 0, 9000);
        ipScanReadInt(tuneObject, "mtuStep", tune.mtuStep, 1, 1000);
        ipScanReadString(tuneObject, "mtuUrl", tune.mtuUrl);
        ipScanReadInt(tuneObject, "mtuBytes", tune.mtuBytes, 1000, 100000000);
        ipScanReadInt(tuneObject, "attempts", tune.attempts, 1, 50);
        ipScanReadInt(tuneObject, "perMinute", tune.perMinute, 1, 120);
        ipScanReadInt(tuneObject, "maxAttempts", tune.maxAttempts, 1, 5000);
        ipScanReadInt(tuneObject, "maxMinutes", tune.maxMinutes, 1, 1440);
        ipScanReadString(tuneObject, "url", tune.url);
        ipScanReadInt(tuneObject, "timeoutMs", tune.timeoutMs, kIpScanMinTimeoutMs, kIpScanMaxTimeoutMs);
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
        config.shuffle = true;
        config.concurrency = 64;
        config.spawnIntervalMs = 2;
        config.icmp.enabled = false;
        config.http.enabled = false;
        config.config.enabled = false;
        if (kind == Kind::Warp) {
            config.ports = Scanner::WarpPresets::kWireGuardPorts;
            config.concurrency = 16;
            config.stopAfter = 10;
            config.tcp.enabled = false;
        } else if (kind == Kind::Resolver) {
            // A portless entry is probed on 53; ports never multiply resolver targets.
            config.ports.clear();
            config.portMode = ScanConfig::PortMode::List;
            config.stopAfter = 0;
            config.tcp.enabled = false;
        } else if (kind == Kind::Tune) {
            config.ports.clear();
            config.stopAfter = 0;
            config.tcp.enabled = false;
        } else {
            config.ports.clear();
            config.stopAfter = 0;
            config.tcp.enabled = true;
            config.tcp.timeoutMs = 2000;
        }
        return config;
    }
} // namespace Configs
