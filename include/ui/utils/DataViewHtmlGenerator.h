#pragma once

#include <QString>
#include <QStringList>
#include <QMutex>
#include "include/global/HTTPRequestHelper.hpp"
#ifndef Q_MOC_RUN
#include <core/gen/libcore.pb.h>
#endif

// Declaration order is descending urgency; only the highest occupied level is ever rendered.
enum class DataViewPriority { Critical, High, Medium, Low };

enum class DataViewItem { Download, SpeedTest, LatencyTest, AutoSelector, VpnEndpoint, PendingRestart, Scanner };

class DataViewHtmlGenerator {
public:
    struct DownloadPanelState {
        bool visible = false;
        DownloadProgressReport report;
    };

    struct SpeedtestPanelState {
        enum class Kind { Speed, Country };
        bool visible = false;
        Kind kind = Kind::Speed;
        QString profileName;
        QString dlSpeed;
        QString ulSpeed;
        QString serverCountryFlag;
        QString serverCountry;
        QString serverName;
        int totalProfiles = 0;
    };

    struct LatencyTestPanelState {
        enum class Kind { Url, Ip };
        bool visible = false;
        Kind kind = Kind::Url;
        int totalProfiles = 0;
    };

    struct AutoSelectorPanelState {
        bool visible = false;
        QString summary;
        QString detail;
    };

    struct VpnEndpointPanelState {
        bool visible = false;
        bool problem = false;
        QString summary;
        QString detail;
    };

    struct PendingRestartPanelState {
        bool visible = false;
        QStringList reasons;
    };

    struct ScannerPanelItem {
        QString name;
        quint64 tested = 0;
        quint64 total = 0;
        int found = 0;
    };

    static constexpr auto RestartActionUrl = "throne-action:restart-proxy";
    static constexpr auto DismissRestartActionUrl = "throne-action:dismiss-restart";

    void setDownloadReport(const DownloadProgressReport &report, bool show);

    void seedSpeedTest(int totalProfiles);

    void setSpeedtestProgress(const QString &profileName, const libcore::SpeedTestResult &result);

    void seedLatencyTest(LatencyTestPanelState::Kind kind, int totalProfiles);

    void setAutoSelectorStatus(const QString &summary, const QString &detail);

    void setVpnEndpointStatus(const QString &summary, const QString &detail, bool problem);

    // An empty list hides the panel.
    void setPendingRestartReasons(const QStringList &reasons);

    void setScannerPanel(const QList<ScannerPanelItem> &items);

    void clearTestSections();

    void addTestProgress(int count = 1);

    QString buildHtml();

private:
    static QString getProgressBar(long long current, long long total);

    // Assume mu_ is held.
    [[nodiscard]] bool itemVisible(DataViewItem item) const;

    QString itemHtml(DataViewItem item, bool shared);

    // The *SectionHtml helpers assume buildHtml already holds mu_.
    QString downloadSectionHtml();

    QString speedtestSectionHtml();

    QString latencyTestSectionHtml();

    QString autoSelectorSectionHtml();

    QString vpnEndpointSectionHtml();

    QString pendingRestartSectionHtml(bool compact);

    QString scannerSectionHtml(bool compact);

    // Pool threads seed panels while buildHtml reads them.
    mutable QMutex mu_;

    DownloadPanelState download_ = {};
    SpeedtestPanelState speedtest_ = {};
    LatencyTestPanelState latencyTest_ = {};
    AutoSelectorPanelState autoSelector_ = {};
    VpnEndpointPanelState vpnEndpoint_ = {};
    PendingRestartPanelState pendingRestart_ = {};
    QList<ScannerPanelItem> scanner_;

    std::atomic<int> testProgress{0};
};
