#include "include/ui/utils/DataViewHtmlGenerator.h"

#include "include/global/CountryHelper.hpp"
#include "include/global/Configs.hpp"

void DataViewHtmlGenerator::setDownloadReport(const DownloadProgressReport &report, bool show) {
    download_.visible = show;
    download_.report = report;
}

void DataViewHtmlGenerator::seedSpeedTest(int totalProfiles) {
    testProgress.store(0);
    Configs::dataManager->settingsRepo->speed_test_mode == Configs::TestConfig::COUNTRY ? speedtest_.kind = SpeedtestPanelState::Kind::Country : speedtest_.kind = SpeedtestPanelState::Kind::Speed;
    speedtest_.totalProfiles = totalProfiles;
    speedtest_.visible = true;
}

void DataViewHtmlGenerator::setSpeedtestProgress(const QString &profileName, const libcore::SpeedTestResult &result) {
    speedtest_.profileName = profileName;
    speedtest_.dlSpeed = QString::fromStdString(result.dl_speed.value());
    speedtest_.ulSpeed = QString::fromStdString(result.ul_speed.value());
    speedtest_.serverCountryFlag = CountryCodeToFlag(CountryNameToCode(QString::fromStdString(result.server_country.value())));
    speedtest_.serverCountry = QString::fromStdString(result.server_country.value());
    speedtest_.serverName = QString::fromStdString(result.server_name.value());
}

void DataViewHtmlGenerator::seedLatencyTest(LatencyTestPanelState::Kind kind, int totalProfiles) {
    testProgress.store(0);
    latencyTest_.visible = true;
    latencyTest_.kind = kind;
    latencyTest_.totalProfiles = totalProfiles;
}

void DataViewHtmlGenerator::clearTestSections() {
    latencyTest_ = {};
    speedtest_ = {};
    testProgress.store(0);
}

void DataViewHtmlGenerator::addTestProgress(int count) {
    testProgress.fetch_add(count);
}

QString DataViewHtmlGenerator::buildHtml() {
    QString html;
    if (download_.visible) {
        html += downloadSectionHtml();
    }
    if (speedtest_.visible) {
        html += speedtestSectionHtml();
    }
    if (latencyTest_.visible) {
        html += latencyTestSectionHtml();
    }
    return html;
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

QString DataViewHtmlGenerator::speedtestSectionHtml() {
    if (speedtest_.kind == SpeedtestPanelState::Kind::Speed) {
        auto firstLine = QStringLiteral("Running Speedtest: %1").arg(speedtest_.profileName);
        if (speedtest_.totalProfiles > 1) {
            firstLine += QString(" (%1 / %2)").arg(Int2String(testProgress.load()), Int2String(speedtest_.totalProfiles));
        }
        if (speedtest_.serverName.isEmpty()) return QString("<p style='text-align:center;margin:0;'>%1</p>").arg(firstLine);
        return QString(
           "<p style='text-align:center;margin:0;'>%1</p>"
           "<div style='text-align: center;'>"
           "<span style='color: #3299FF;'>Dl↓ %2</span>  "
           "<span style='color: #86C43F;'>Ul↑ %3</span>"
           "</div>"
           "<p style='text-align:center;margin:0;'>Server: %4%5, %6</p>")
            .arg(firstLine, speedtest_.dlSpeed, speedtest_.ulSpeed, speedtest_.serverCountryFlag, speedtest_.serverCountry,
                speedtest_.serverName);
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
        return res;
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
