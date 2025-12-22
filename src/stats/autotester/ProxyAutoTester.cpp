#include "include/stats/autotester/ProxyAutoTester.hpp"
#include "include/dataStore/ProxyEntity.hpp"
#include "include/dataStore/Database.hpp"
#include "include/global/Configs.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/mainwindow.h"
#include "include/api/RPC.h"
#include "include/configs/generate.h"

#include <QDebug>
#include <algorithm>

namespace Stats {

ProxyAutoTester::ProxyAutoTester(QObject *parent)
    : QObject(parent)
    , testTimer(new QTimer(this))
    , healthCheckTimer(new QTimer(this))
    , currentTestOffset(0)
    , activeProxyFailureCount(0)
    , isTestingInProgress(false)
    , lastLogTime(0)
{
    connect(testTimer, &QTimer::timeout, this, &ProxyAutoTester::onTestTimerTimeout);
    connect(healthCheckTimer, &QTimer::timeout, this, &ProxyAutoTester::onHealthCheckTimerTimeout);
}

ProxyAutoTester::~ProxyAutoTester() {
    Stop();
}

void ProxyAutoTester::Start() {
    if (!isEnabled()) {
        logStatus("Auto-testing is disabled in settings");
        return;
    }

    Stop(); // Ensure clean state
    
    logStatus("Starting auto-testing system");
    
    // Start periodic testing
    int intervalMs = getTestInterval() * 1000;
    testTimer->start(intervalMs);
    
    // Start health monitoring
    healthCheckTimer->start(30000); // Check every 30 seconds
    if (Configs::dataStore->spmode_vpn) {
        logStatus("Health monitoring enabled (TUN mode)");
    } else {
        logStatus("Health monitoring enabled (Proxy mode)");
    }
    
    // Run initial test cycle
    RunTestCycle();
}

void ProxyAutoTester::Stop() {
    testTimer->stop();
    healthCheckTimer->stop();
    isTestingInProgress = false;
    logStatus("Auto-testing system stopped");
}

void ProxyAutoTester::Reset() {
    Stop();
    workingProxyPool.clear();
    lastTestTime.clear();
    failureCount.clear();
    currentTestOffset = 0;
    activeProxyFailureCount = 0;
    emit workingPoolUpdated(workingProxyPool);
    logStatus("Auto-testing state reset");
}

void ProxyAutoTester::RunTestCycle() {
    if (isTestingInProgress) {
        logStatus("Test cycle already in progress, skipping");
        return;
    }

    if (!isEnabled()) {
        logStatus("Auto-test is not enabled, skipping test cycle");
        return;
    }

    if (!Configs::dataStore->core_running) {
        logStatus("Core is not running, skipping test cycle");
        return;
    }

    isTestingInProgress = true;
    emit testCycleStarted();
    
    logStatus("Starting test cycle");
    
    auto proxiesToTest = getProxiesToTest();
    
    if (proxiesToTest.isEmpty()) {
        logStatus("No proxies available to test");
        isTestingInProgress = false;
        emit testCycleCompleted(0, workingProxyPool.size());
        return;
    }

    QList<int> proxyIds;
    for (const auto &proxy : proxiesToTest) {
        proxyIds.append(proxy->id);
    }

    logStatus(QString("Testing %1 proxies in this cycle").arg(proxyIds.size()));
    performTest(proxyIds);
}

void ProxyAutoTester::CheckActiveProxyHealth() {
    if (!Configs::dataStore->core_running) {
        return; // Core not running
    }

    int activeProxyId = Configs::dataStore->started_id;
    if (activeProxyId < 0) {
        return; // No active proxy
    }

    auto proxy = Configs::profileManager->GetProfile(activeProxyId);
    if (!proxy) {
        return;
    }

    // Test the active proxy
    QList<int> testIds;
    testIds.append(activeProxyId);
    
    runOnNewThread([=, this] {
        libcore::TestReq req;
        req.url = getTargetUrl().toStdString();
        req.test_timeout_ms = Configs::dataStore->url_test_timeout_ms;
        req.test_current = true;
        
        // Add the active proxy tag
        QString tag = QString("out-%1-%2").arg(proxy->type, proxy->outbound->name);
        req.outbound_tags.push_back(tag.toStdString());
        
        bool rpcOK;
        auto result = API::defaultClient->Test(&rpcOK, req);
        
        if (!rpcOK || result.results.empty()) {
            activeProxyFailureCount++;
            logStatus(QString("Active proxy health check failed (attempt %1/%2)")
                .arg(activeProxyFailureCount).arg(getRetryCount()));
                
            if (activeProxyFailureCount >= getRetryCount()) {
                emit activeProxyFailed(activeProxyId);
                HandleProxyFailure(activeProxyId, activeProxyFailureCount);
            }
            return;
        }
        
        const auto &res = result.results[0];
        bool isHealthy = res.error.value().empty() && res.latency_ms.value() > 0 
                        && res.latency_ms.value() < getLatencyThreshold();
        
        if (isHealthy) {
            activeProxyFailureCount = 0; // Reset failure count on success
            proxy->is_working = true;
            proxy->last_auto_test_time = QDateTime::currentSecsSinceEpoch();
            proxy->Save();
        } else {
            activeProxyFailureCount++;
            logStatus(QString("Active proxy unhealthy: %1 (attempt %2/%3)")
                .arg(QString::fromStdString(res.error.value()))
                .arg(activeProxyFailureCount).arg(getRetryCount()));
                
            if (activeProxyFailureCount >= getRetryCount()) {
                emit activeProxyFailed(activeProxyId);
                HandleProxyFailure(activeProxyId, activeProxyFailureCount);
            }
        }
    });
}

void ProxyAutoTester::HandleProxyFailure(int proxyId, int attemptCount) {
    logStatus(QString("Handling failure for proxy %1 after %2 attempts").arg(proxyId).arg(attemptCount));
    
    // Mark proxy as not working
    auto proxy = Configs::profileManager->GetProfile(proxyId);
    if (proxy) {
        proxy->is_working = false;
        proxy->Save();
    }
    
    // Remove from working pool
    workingProxyPool.removeAll(proxyId);
    emit workingPoolUpdated(workingProxyPool);
    
    // Try to failover if this is the active proxy
    if (Configs::dataStore->started_id == proxyId) {
        int nextProxyId = GetNextWorkingProxy();
        
        if (nextProxyId >= 0) {
            logStatus(QString("Triggering automatic failover from proxy %1 to %2").arg(proxyId).arg(nextProxyId));
            emit failoverTriggered(proxyId, nextProxyId);
            
            auto nextProxy = Configs::profileManager->GetProfile(nextProxyId);
            QString proxyName = nextProxy ? nextProxy->outbound->DisplayTypeAndName() : QString::number(nextProxyId);
            
            // Trigger the switch in the main window
            runOnUiThread([=] {
                MW_show_log(QString("[Auto-Test] Active proxy failed, switching to: %1").arg(proxyName));
                auto mw = GetMainWindow();
                if (mw) mw->profile_start(nextProxyId);
            });
            
            activeProxyFailureCount = 0;
        } else {
            logStatus("No working proxies available for failover");
            emit noWorkingProxiesAvailable();
            
            runOnUiThread([] {
                MW_show_log("[Auto-Test] Active proxy failed but no backup proxy available!");
            });
        }
    }
}

int ProxyAutoTester::GetNextWorkingProxy() {
    pruneWorkingPool();
    
    if (workingProxyPool.isEmpty()) {
        return -1;
    }
    
    // Return the first working proxy (most recently verified)
    return workingProxyPool.first();
}

QList<int> ProxyAutoTester::GetWorkingProxies() const {
    return workingProxyPool;
}

bool ProxyAutoTester::IsProxyWorking(int proxyId) const {
    return workingProxyPool.contains(proxyId);
}

void ProxyAutoTester::onTestTimerTimeout() {
    RunTestCycle();
}

void ProxyAutoTester::onHealthCheckTimerTimeout() {
    CheckActiveProxyHealth();
}

// Configuration getters
bool ProxyAutoTester::isEnabled() const {
    return Configs::dataStore->auto_test_enable;
}

int ProxyAutoTester::getTestInterval() const {
    return Configs::dataStore->auto_test_interval_seconds;
}

int ProxyAutoTester::getProxyCountPerCycle() const {
    return Configs::dataStore->auto_test_proxy_count;
}

int ProxyAutoTester::getPoolSize() const {
    return Configs::dataStore->auto_test_working_pool_size;
}

int ProxyAutoTester::getLatencyThreshold() const {
    return Configs::dataStore->auto_test_latency_threshold_ms;
}

int ProxyAutoTester::getRetryCount() const {
    return Configs::dataStore->auto_test_failure_retry_count;
}

QString ProxyAutoTester::getTargetUrl() const {
    return Configs::dataStore->auto_test_target_url;
}

bool ProxyAutoTester::isTunFailoverEnabled() const {
    return Configs::dataStore->auto_test_tun_failover;
}

// Helper methods
QList<std::shared_ptr<Configs::ProxyEntity>> ProxyAutoTester::getProxiesToTest() {
    QList<std::shared_ptr<Configs::ProxyEntity>> result;
    
    // Get all proxies from the current group
    int currentGroupId = Configs::dataStore->current_group;
    auto group = Configs::profileManager->GetGroup(currentGroupId);
    
    if (!group) {
        logStatus("No group found for auto-test");
        return result;
    }
    
    auto allProxies = group->GetProfileEnts();
    
    if (allProxies.isEmpty()) {
        logStatus("No proxies in group");
        return result;
    }
    
    logStatus(QString("Found %1 total proxies in group").arg(allProxies.size()));
    
    int proxyCountPerCycle = getProxyCountPerCycle();
    
    // Rotate through proxies
    int startIdx = currentTestOffset % allProxies.size();
    int count = 0;
    
    for (int i = 0; i < allProxies.size() && count < proxyCountPerCycle; i++) {
        int idx = (startIdx + i) % allProxies.size();
        auto proxy = allProxies[idx];
        
        if (shouldTestProxy(proxy)) {
            result.append(proxy);
            count++;
        }
    }
    
    // Update offset for next cycle
    currentTestOffset = (startIdx + count) % allProxies.size();
    
    return result;
}

bool ProxyAutoTester::shouldTestProxy(std::shared_ptr<Configs::ProxyEntity> proxy) {
    if (!proxy) {
        return false;
    }
    
    // Skip certain proxy types that shouldn't be auto-tested
    if (proxy->type == "custom" || proxy->type == "chain") {
        return false;
    }
    
    return true;
}

void ProxyAutoTester::performTest(const QList<int> &proxyIds) {
    if (proxyIds.isEmpty()) {
        isTestingInProgress = false;
        emit testCycleCompleted(0, workingProxyPool.size());
        return;
    }
    
    runOnNewThread([=, this] {
        // Build list of proxy entities
        QList<std::shared_ptr<Configs::ProxyEntity>> proxies;
        for (int proxyId : proxyIds) {
            auto proxy = Configs::profileManager->GetProfile(proxyId);
            if (proxy) {
                proxies.append(proxy);
            }
        }
        
        if (proxies.isEmpty()) {
            logStatus("No valid proxies to test");
            isTestingInProgress = false;
            emit testCycleCompleted(0, workingProxyPool.size());
            return;
        }
        
        // Build test configuration
        auto buildObject = Configs::BuildTestConfig(proxies);
        if (!buildObject->error.isEmpty()) {
            logStatus("Failed to build test config: " + buildObject->error);
            isTestingInProgress = false;
            emit testCycleCompleted(0, workingProxyPool.size());
            return;
        }
        
        // Check if we should test with combined config or individually
        if (buildObject->outboundTags.empty() && buildObject->fullConfigs.empty()) {
            logStatus("No testable configs generated");
            isTestingInProgress = false;
            emit testCycleCompleted(0, workingProxyPool.size());
            return;
        }
        
        int workingCount = 0;
        qint64 currentTime = QDateTime::currentSecsSinceEpoch();
        QMap<QString, int> tag2entID = buildObject->tag2entID;
        
        // Test with combined config if available (multiple outbounds in one config)
        if (!buildObject->outboundTags.empty()) {
            libcore::TestReq req;
            req.config = QJsonObject2QString(buildObject->coreConfig, false).toStdString();
            req.xray_config = QJsonObject2QString(buildObject->xrayConfig, false).toStdString();
            req.need_xray = !buildObject->xrayConfig.isEmpty();
            req.url = getTargetUrl().toStdString();
            req.test_timeout_ms = Configs::dataStore->url_test_timeout_ms;
            req.use_default_outbound = false;
            
            for (const auto &tag : buildObject->outboundTags) {
                req.outbound_tags.push_back(tag.toStdString());
            }
            
            bool rpcOK;
            auto result = API::defaultClient->Test(&rpcOK, req);
            
            if (!rpcOK) {
                logStatus("RPC test call failed");
                isTestingInProgress = false;
                emit testCycleCompleted(0, workingProxyPool.size());
                return;
            }
            
            // Process results
            for (const auto &res : result.results) {
                QString tag = QString::fromStdString(res.outbound_tag.value());
                if (!tag2entID.contains(tag)) {
                    continue;
                }
                
                int proxyId = tag2entID[tag];
                auto proxy = Configs::profileManager->GetProfile(proxyId);
                
                if (!proxy) {
                    continue;
                }
                
                bool isWorking = res.error.value().empty() && res.latency_ms.value() > 0 
                                && res.latency_ms.value() < getLatencyThreshold();
                
                // Update proxy status
                proxy->is_working = isWorking;
                proxy->last_auto_test_time = currentTime;
                
                if (isWorking) {
                    proxy->latency = res.latency_ms.value();
                    workingCount++;
                } else {
                    proxy->latency = -1;
                }
                
                proxy->Save();
                
                // Update working pool
                updateWorkingPool(proxyId, isWorking);
                
                lastTestTime[proxyId] = currentTime;
            }
        }
        
        pruneWorkingPool();
        
        logStatus(QString("Test cycle completed: %1 proxies tested, %2 working, pool size: %3")
            .arg(proxyIds.size()).arg(workingCount).arg(workingProxyPool.size()));
        
        isTestingInProgress = false;
        emit testCycleCompleted(proxyIds.size(), workingProxyPool.size());
        emit workingPoolUpdated(workingProxyPool);
        
        // Auto-start a working proxy if none is running
        int currentStartedId = Configs::dataStore->started_id;
        int workingPoolSize = workingProxyPool.size();
        bool hasWorkingProxies = !workingProxyPool.isEmpty();
        
        runOnUiThread([=] {
            MW_show_log(QString("[Auto-Test] Current status - started_id: %1, working proxies: %2")
                .arg(currentStartedId).arg(workingPoolSize));
        });
        
        if (currentStartedId < 0 && hasWorkingProxies) {
            int proxyToStart = GetNextWorkingProxy();
            if (proxyToStart >= 0) {
                auto proxy = Configs::profileManager->GetProfile(proxyToStart);
                if (proxy) {
                    QString proxyName = proxy->outbound->DisplayTypeAndName();
                    
                    runOnUiThread([=] {
                        MW_show_log(QString("[Auto-Test] No proxy running, automatically starting: %1 (ID: %2)")
                            .arg(proxyName).arg(proxyToStart));
                        auto mw = GetMainWindow();
                        if (mw) {
                            mw->profile_start(proxyToStart);
                        } else {
                            MW_show_log("[Auto-Test] ERROR: Could not get MainWindow instance");
                        }
                    });
                } else {
                    runOnUiThread([] {
                        MW_show_log("[Auto-Test] ERROR: Could not get proxy profile for auto-start");
                    });
                }
            } else {
                runOnUiThread([] {
                    MW_show_log("[Auto-Test] ERROR: GetNextWorkingProxy returned invalid ID");
                });
            }
        } else if (currentStartedId >= 0) {
            runOnUiThread([currentStartedId] {
                MW_show_log(QString("[Auto-Test] Proxy already running (ID: %1), skipping auto-start").arg(currentStartedId));
            });
        }
        
        // Refresh UI
        runOnUiThread([] {
            auto mw = GetMainWindow();
            if (mw) mw->refresh_proxy_list();
        });
    });
}

void ProxyAutoTester::updateWorkingPool(int proxyId, bool isWorking) {
    if (isWorking) {
        // Add to pool if not already present
        if (!workingProxyPool.contains(proxyId)) {
            workingProxyPool.prepend(proxyId);
        } else {
            // Move to front (most recently verified)
            workingProxyPool.removeAll(proxyId);
            workingProxyPool.prepend(proxyId);
        }
    } else {
        // Remove from pool
        workingProxyPool.removeAll(proxyId);
    }
}

void ProxyAutoTester::pruneWorkingPool() {
    int maxPoolSize = getPoolSize();
    
    // Remove excess entries
    while (workingProxyPool.size() > maxPoolSize) {
        workingProxyPool.removeLast();
    }
    
    // Remove invalid proxies
    QList<int> toRemove;
    for (int proxyId : workingProxyPool) {
        auto proxy = Configs::profileManager->GetProfile(proxyId);
        if (!proxy || !proxy->is_working) {
            toRemove.append(proxyId);
        }
    }
    
    for (int proxyId : toRemove) {
        workingProxyPool.removeAll(proxyId);
    }
}

void ProxyAutoTester::logStatus(const QString &message) {
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    // Rate limit logging to once per 5 seconds
    if (currentTime - lastLogTime < 5000) {
        return;
    }
    
    lastLogTime = currentTime;
    
    runOnUiThread([=] {
        MW_show_log("[Auto-Test] " + message);
    });
}

} // namespace Stats

