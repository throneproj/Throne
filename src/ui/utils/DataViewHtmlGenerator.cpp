#include "include/ui/utils/DataViewHtmlGenerator.h"

#include "include/global/CountryHelper.hpp"
#include "include/global/Configs.hpp"
#include "include/ui/setting/ThemeManager.hpp"

#include <QTextDocument>
#include <array>

namespace {
    struct ItemPriority {
        DataViewItem item;
        DataViewPriority priority;
    };

    // Within one level, the first visible panel in this order is the one shown; ambient status comes last.
    constexpr std::array kItemPriorities = {
        ItemPriority{DataViewItem::SpeedTest, DataViewPriority::Critical},
        ItemPriority{DataViewItem::LatencyTest, DataViewPriority::Critical},
        ItemPriority{DataViewItem::PendingRestart, DataViewPriority::Medium},
        ItemPriority{DataViewItem::Scanner, DataViewPriority::Medium},
        ItemPriority{DataViewItem::Download, DataViewPriority::Medium},
        ItemPriority{DataViewItem::VpnEndpoint, DataViewPriority::Medium},
        ItemPriority{DataViewItem::AutoSelector, DataViewPriority::Medium},
    };

    constexpr int kScannerPanelLines = 2;

    QString dataViewToolTip(const QStringList &lines) {
        return lines.isEmpty() ? QString() : Qt::convertFromPlainText(lines.join(u'\n'), Qt::WhiteSpaceNormal);
    }

    constexpr std::array kPriorityOrder = {
        DataViewPriority::Critical,
        DataViewPriority::High,
        DataViewPriority::Medium,
        DataViewPriority::Low,
    };

    QString dataViewScannerProgressLine(const DataViewHtmlGenerator::ScannerPanelItem &item) {
        const quint64 percent = item.total > 0 ? qMin<quint64>(100, item.tested * 100 / item.total) : 0;
        return QObject::tr("%1 scan in progress %2/%3 (%4%)")
            .arg(item.name, QString::number(item.tested), QString::number(item.total), QString::number(percent));
    }
}

void DataViewHtmlGenerator::setDownloadReport(const DownloadProgressReport &report, bool show) {
    QMutexLocker lk(&mu_);
    download_.visible = show;
    download_.report = report;
}

void DataViewHtmlGenerator::seedSpeedTest(int totalProfiles) {
    QMutexLocker lk(&mu_);
    testProgress.store(0);
    Configs::dataManager->settingsRepo->speed_test_mode == Configs::TestConfig::COUNTRY ? speedtest_.kind = SpeedtestPanelState::Kind::Country : speedtest_.kind = SpeedtestPanelState::Kind::Speed;
    speedtest_.totalProfiles = totalProfiles;
    speedtest_.visible = true;
}

void DataViewHtmlGenerator::setSpeedtestProgress(const QString &profileName, const libcore::SpeedTestResult &result) {
    QMutexLocker lk(&mu_);
    speedtest_.profileName = profileName;
    speedtest_.dlSpeed = QString::fromStdString(result.dl_speed.value());
    speedtest_.ulSpeed = QString::fromStdString(result.ul_speed.value());
    speedtest_.serverCountryFlag = CountryCodeToFlag(CountryNameToCode(QString::fromStdString(result.server_country.value())));
    speedtest_.serverCountry = QString::fromStdString(result.server_country.value());
    speedtest_.serverName = QString::fromStdString(result.server_name.value());
}

void DataViewHtmlGenerator::seedLatencyTest(LatencyTestPanelState::Kind kind, int totalProfiles) {
    QMutexLocker lk(&mu_);
    testProgress.store(0);
    latencyTest_.visible = true;
    latencyTest_.kind = kind;
    latencyTest_.totalProfiles = totalProfiles;
}

void DataViewHtmlGenerator::setAutoSelectorStatus(const QString &summary, const QString &detail) {
    QMutexLocker lk(&mu_);
    autoSelector_.summary = summary;
    autoSelector_.detail = detail;
    autoSelector_.visible = !summary.isEmpty();
}

void DataViewHtmlGenerator::setVpnEndpointStatus(const QString &summary, const QString &detail, bool problem) {
    QMutexLocker lk(&mu_);
    vpnEndpoint_.summary = summary;
    vpnEndpoint_.detail = detail;
    vpnEndpoint_.problem = problem;
    vpnEndpoint_.visible = !summary.isEmpty();
}

void DataViewHtmlGenerator::setPendingRestartReasons(const QStringList &reasons) {
    QMutexLocker lk(&mu_);
    pendingRestart_.reasons = reasons;
    pendingRestart_.visible = !reasons.isEmpty();
}

void DataViewHtmlGenerator::setScannerPanel(const QList<ScannerPanelItem> &items) {
    QMutexLocker lk(&mu_);
    scanner_ = items;
}

void DataViewHtmlGenerator::clearTestSections() {
    QMutexLocker lk(&mu_);
    latencyTest_ = {};
    speedtest_ = {};
    testProgress.store(0);
}

void DataViewHtmlGenerator::addTestProgress(int count) {
    testProgress.fetch_add(count);
}

DataViewHtmlGenerator::Content DataViewHtmlGenerator::build() {
    QMutexLocker lk(&mu_);
    for (const auto priority : kPriorityOrder) {
        for (const auto &entry : kItemPriorities) {
            if (entry.priority == priority && itemVisible(entry.item)) return itemContent(entry.item);
        }
    }
    return {};
}

bool DataViewHtmlGenerator::itemVisible(DataViewItem item) const {
    switch (item) {
        case DataViewItem::Download:       return download_.visible;
        case DataViewItem::SpeedTest:      return speedtest_.visible;
        case DataViewItem::LatencyTest:    return latencyTest_.visible;
        case DataViewItem::AutoSelector:   return autoSelector_.visible;
        case DataViewItem::VpnEndpoint:    return vpnEndpoint_.visible;
        case DataViewItem::PendingRestart: return pendingRestart_.visible;
        case DataViewItem::Scanner:        return !scanner_.isEmpty();
    }
    return false;
}

DataViewHtmlGenerator::Content DataViewHtmlGenerator::itemContent(DataViewItem item) {
    switch (item) {
        case DataViewItem::Download:       return {downloadSectionHtml(), {}};
        case DataViewItem::SpeedTest:      return speedtestSection();
        case DataViewItem::LatencyTest:    return {latencyTestSectionHtml(), {}};
        case DataViewItem::AutoSelector:   return {autoSelectorSectionHtml(), {}};
        case DataViewItem::VpnEndpoint:    return {vpnEndpointSectionHtml(), {}};
        case DataViewItem::PendingRestart: return pendingRestartSection();
        case DataViewItem::Scanner:        return scannerSection();
    }
    return {};
}

DataViewHtmlGenerator::Content DataViewHtmlGenerator::scannerSection() {
    const auto &tokens = themeManager()->tokens;
    const qsizetype shown = scanner_.size() > kScannerPanelLines ? kScannerPanelLines - 1 : scanner_.size();
    Content res;
    QStringList all;
    for (qsizetype i = 0; i < scanner_.size(); ++i) {
        const auto &item = scanner_.at(i);
        QString line = dataViewScannerProgressLine(item);
        if (item.found > 0) line += QObject::tr(" · %1 found").arg(item.found);
        all << line;
        if (i >= shown) continue;
        res.html += QString("<p style='text-align:center;margin:0;color:%1;'>%2</p>")
                        .arg(tokens.info.name(), line.toHtmlEscaped());
    }
    if (shown < scanner_.size()) {
        res.html += QString("<p style='text-align:center;margin:0;color:%1;'>%2</p>")
                        .arg(tokens.muted.name(), QObject::tr("+%1 more").arg(scanner_.size() - shown).toHtmlEscaped());
        res.toolTip = dataViewToolTip(all);
    }
    return res;
}

DataViewHtmlGenerator::Content DataViewHtmlGenerator::pendingRestartSection() {
    const auto &tokens = themeManager()->tokens;
    Content res;
    res.html = QString("<p style='text-align:center;margin:0;color:%1;'>%2</p>")
                   .arg(tokens.info.name(), QObject::tr("Settings changed, restart to apply").toHtmlEscaped());
    res.html += QString("<p style='text-align:center;margin:0;'>"
                        "<a style='color:%1;' href='%2'>%3</a>"
                        "&nbsp;&nbsp;&#183;&nbsp;&nbsp;"
                        "<a style='color:%4;' href='%5'>%6</a>"
                        "</p>")
                    .arg(tokens.accent.name(), QString(RestartActionUrl), QObject::tr("Restart").toHtmlEscaped(),
                         tokens.muted.name(), QString(DismissRestartActionUrl), QObject::tr("Ignore").toHtmlEscaped());
    if (!pendingRestart_.reasons.isEmpty()) {
        res.toolTip = dataViewToolTip({QObject::tr("Changed: %1").arg(pendingRestart_.reasons.join(QStringLiteral(", ")))});
    }
    return res;
}

QString DataViewHtmlGenerator::vpnEndpointSectionHtml() {
    const auto colour = vpnEndpoint_.problem ? themeManager()->tokens.danger : themeManager()->tokens.info;
    QString res = QString("<p style='text-align:center;margin:0;color:%1;'>%2</p>")
                      .arg(colour.name(), vpnEndpoint_.summary.toHtmlEscaped());
    if (!vpnEndpoint_.detail.isEmpty()) {
        res += QString("<p style='text-align:center;margin:0;opacity:0.75;'>%1</p>")
                   .arg(vpnEndpoint_.detail.toHtmlEscaped());
    }
    return res;
}

QString DataViewHtmlGenerator::autoSelectorSectionHtml() {
    QString res = QString("<p style='text-align:center;margin:0;'>%1</p>").arg(autoSelector_.summary.toHtmlEscaped());
    if (!autoSelector_.detail.isEmpty()) {
        res += QString("<p style='text-align:center;margin:0;opacity:0.75;'>%1</p>")
                   .arg(autoSelector_.detail.toHtmlEscaped());
    }
    return res;
}

QString DataViewHtmlGenerator::getProgressBar(long long current, long long total) {
    qint64 count = 0;
    if (total > 0) {
        count = 10 * current / total;
    }
    QString progressText;
    for (int i = 0; i < 10; i++) {
        if (count--; count >= 0) {
            progressText += "#";
        } else {
            progressText += "-";
        }
    }
    return progressText;
}

QString DataViewHtmlGenerator::downloadSectionHtml() {
    auto progressText = getProgressBar(download_.report.downloadedSize, download_.report.totalSize);
    const QString stat =
        ReadableSize(download_.report.downloadedSize) + "/" + ReadableSize(download_.report.totalSize);
    return QString("<p style='text-align:center;margin:0;'>Downloading %1: %2 %3</p>")
        .arg(download_.report.fileName, stat, progressText);
}

DataViewHtmlGenerator::Content DataViewHtmlGenerator::speedtestSection() {
    if (speedtest_.kind == SpeedtestPanelState::Kind::Speed) {
        auto firstLine = QStringLiteral("Running Speedtest: %1").arg(speedtest_.profileName);
        if (speedtest_.totalProfiles > 1) {
            firstLine += QString(" (%1 / %2)").arg(Int2String(testProgress.load()), Int2String(speedtest_.totalProfiles));
        }
        if (speedtest_.serverName.isEmpty()) return {QString("<p style='text-align:center;margin:0;'>%1</p>").arg(firstLine), {}};
        return {QString(
                    "<p style='text-align:center;margin:0;'>%1</p>"
                    "<div style='text-align: center;'>"
                    "<span style='color: %4;'>Dl↓ %2</span>  "
                    "<span style='color: %5;'>Ul↑ %3</span>"
                    "</div>")
                    .arg(firstLine, speedtest_.dlSpeed, speedtest_.ulSpeed, themeManager()->tokens.info.name(),
                         themeManager()->tokens.success.name()),
                dataViewToolTip({QStringLiteral("Server: %1%2, %3").arg(speedtest_.serverCountryFlag, speedtest_.serverCountry,
                                                                         speedtest_.serverName)})};
    } else {
        QString res;
        auto content = QString("Running Country Test");
        if (speedtest_.totalProfiles > 1) {
            auto progress = getProgressBar(testProgress.load(), speedtest_.totalProfiles);
            progress += QString(" ") + Int2String(100 * testProgress.load() / speedtest_.totalProfiles) + "%";
            res = QString("<p style='text-align:center;margin:0;'>%1</p>").arg(progress);
            content += QString(" (%1 / %2)").arg(Int2String(testProgress.load()), Int2String(speedtest_.totalProfiles));
        }
        res += QString("<p style='text-align:center;margin:0;'>%1</p>").arg(content);
        return {res, {}};
    }
}

QString DataViewHtmlGenerator::latencyTestSectionHtml() {
    QString res;
    auto content =
        latencyTest_.kind == LatencyTestPanelState::Kind::Url ? QString("Running URL test") : QString("Running IP test");
    if (latencyTest_.totalProfiles > 1) {
        auto progress = getProgressBar(testProgress.load(), latencyTest_.totalProfiles);
        progress += QString(" ") + Int2String(100 * testProgress.load() / latencyTest_.totalProfiles) + "%";
        res = QString("<p style='text-align:center;margin:0;'>%1</p>").arg(progress);
        content += QString(" (%1 / %2)").arg(Int2String(testProgress.load()), Int2String(latencyTest_.totalProfiles));
    }
    res += QString("<p style='text-align:center;margin:0;'>%1</p>").arg(content);
    return res;
}
