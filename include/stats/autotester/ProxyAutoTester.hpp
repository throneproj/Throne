#pragma once

#include <QObject>
#include <QTimer>
#include <QList>
#include <QMap>
#include <QDateTime>
#include <memory>

namespace Configs {
    class ProxyEntity;
}

namespace Stats {
    class ProxyAutoTester : public QObject {
        Q_OBJECT

    public:
        explicit ProxyAutoTester(QObject *parent = nullptr);
        ~ProxyAutoTester() override;

        // Control methods
        void Start();
        void Stop();
        void Reset();
        
        // Testing methods
        void RunTestCycle();
        void CheckActiveProxyHealth();
        
        // Pool management
        QList<int> GetWorkingProxies() const;
        bool IsProxyWorking(int proxyId) const;
        
        // Failover handling
        void HandleProxyFailure(int proxyId, int attemptCount);
        int GetNextWorkingProxy();
        
    signals:
        void testCycleStarted();
        void testCycleCompleted(int testedCount, int workingCount);
        void workingPoolUpdated(QList<int> workingProxies);
        void activeProxyFailed(int proxyId);
        void failoverTriggered(int oldProxyId, int newProxyId);
        void noWorkingProxiesAvailable();

    private slots:
        void onTestTimerTimeout();
        void onHealthCheckTimerTimeout();

    private:
        // Configuration
        bool isEnabled() const;
        int getTestInterval() const;
        int getProxyCountPerCycle() const;
        int getPoolSize() const;
        int getLatencyThreshold() const;
        int getRetryCount() const;
        QString getTargetUrl() const;
        bool isTunFailoverEnabled() const;

        // Helper methods
        QList<std::shared_ptr<Configs::ProxyEntity>> getProxiesToTest();
        void updateWorkingPool(int proxyId, bool isWorking);
        void pruneWorkingPool();
        void logStatus(const QString &message);
        bool shouldTestProxy(std::shared_ptr<Configs::ProxyEntity> proxy);
        void performTest(const QList<int> &proxyIds);

        // Timers
        QTimer *testTimer;
        QTimer *healthCheckTimer;

        // State tracking
        QList<int> workingProxyPool;
        QMap<int, qint64> lastTestTime;
        QMap<int, int> failureCount;
        int currentTestOffset;
        int activeProxyFailureCount;
        bool isTestingInProgress;
        qint64 lastLogTime;
    };
} // namespace Stats

