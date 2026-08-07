#include "include/ui/stats/dialog_runtime_stats.h"

#include "include/ui/mainwindow.h"
#include "include/api/RPC.h"
#include "include/stats/traffic/TrafficLooper.hpp"
#include "include/database/DatabaseManager.h"
#include "include/database/SettingsRepo.h"
#include "include/global/Utils.hpp"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/global/CountryHelper.hpp"

#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

namespace {
    const QColor kRuntimeThroneColor(0x4F, 0x8A, 0xF7); // blue
    const QColor kRuntimeCoreColor(0x34, 0xC9, 0x8A);   // green

    QString humanizeDuration(qint64 s) {
        if (s < 0) s = 0;
        const qint64 d = s / 86400; s %= 86400;
        const qint64 h = s / 3600;  s %= 3600;
        const qint64 m = s / 60;    s %= 60;
        QStringList parts;
        if (d > 0) parts << QStringLiteral("%1d").arg(d);
        if (h > 0) parts << QStringLiteral("%1h").arg(h);
        if (m > 0) parts << QStringLiteral("%1m").arg(m);
        if (d == 0 && h == 0) parts << QStringLiteral("%1s").arg(s);
        return parts.join(QLatin1Char(' '));
    }

    QString formatCpu(const Sys::ProcessMetrics::Sample& s) {
        return s.ok ? QString::number(s.cpuPercent, 'f', 1) + QStringLiteral("%") : QStringLiteral("—");
    }

    QString formatRam(const Sys::ProcessMetrics::Sample& s) {
        return s.ok ? ReadableSize(s.rssBytes) : QStringLiteral("—");
    }
}

DialogRuntimeStats::DialogRuntimeStats(QWidget* parent) : QDialog(parent), ui(new Ui::DialogRuntimeStats) {
    ui->setupUi(this);

    ui->rootGrid->setColumnStretch(0, 1);
    ui->rootGrid->setColumnStretch(1, 1);
    ui->processLayout->setStretch(1, 1);
    ui->processLayout->setStretch(2, 1);

    ui->cpuChart->setColors(kRuntimeThroneColor, kRuntimeCoreColor);
    ui->ramChart->setColors(kRuntimeThroneColor, kRuntimeCoreColor);
    ui->cpuChart->setFormatter([](double v) { return QString::number(v, 'f', 0) + QStringLiteral("%"); });
    ui->ramChart->setFormatter([](double v) { return ReadableSize(static_cast<qint64>(v)); });
    ui->cpuChart->setCaption(tr("CPU"));
    ui->ramChart->setCaption(tr("RAM"));
    ui->labelThroneName->setText(QStringLiteral("<span style=\"color:%1\">●</span> %2")
                                     .arg(kRuntimeThroneColor.name(), "Throne"));
    ui->labelCoreName->setText(QStringLiteral("<span style=\"color:%1\">●</span> %2")
                                   .arg(kRuntimeCoreColor.name(), tr("Core")));

    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, [this]() { refreshLive(); });
    timer_->start();

    refreshLive();
}

DialogRuntimeStats::~DialogRuntimeStats() {
    delete ui;
}

void DialogRuntimeStats::refreshLive() {
    auto* mw = GetMainWindow();

    const auto selfSample = metrics_.sample(QCoreApplication::applicationPid());
    ui->vThroneRam->setText(formatRam(selfSample));
    ui->vThroneCpu->setText(formatCpu(selfSample));

    const qint64 corePid = mw ? mw->GetCorePid() : 0;
    const auto coreSample = corePid > 0 ? metrics_.sample(corePid) : Sys::ProcessMetrics::Sample{};
    ui->vCoreRam->setText(formatRam(coreSample));
    ui->vCoreCpu->setText(formatCpu(coreSample));

    ui->cpuChart->push(selfSample.ok ? selfSample.cpuPercent : 0.0,
                       coreSample.ok ? coreSample.cpuPercent : 0.0);
    ui->ramChart->push(selfSample.ok ? static_cast<double>(selfSample.rssBytes) : 0.0,
                       coreSample.ok ? static_cast<double>(coreSample.rssBytes) : 0.0);

    const auto rate = [](double bps) { return ReadableSize(static_cast<qint64>(bps)) + QStringLiteral("/s"); };
    if (Stats::trafficLooper && Stats::trafficLooper->proxy) {
        const auto p = Stats::trafficLooper->proxy;
        ui->vSpeedProxy->setText(QStringLiteral("↓ %1   ↑ %2").arg(rate(p->downlink_rate), rate(p->uplink_rate)));
    } else {
        ui->vSpeedProxy->setText(QStringLiteral("—"));
    }
    if (Stats::trafficLooper && Stats::trafficLooper->direct) {
        const auto dct = Stats::trafficLooper->direct;
        ui->vSpeedDirect->setText(QStringLiteral("↓ %1   ↑ %2").arg(rate(dct->downlink_rate), rate(dct->uplink_rate)));
    } else {
        ui->vSpeedDirect->setText(QStringLiteral("—"));
    }

    const auto nextUpd = [](int interval, qint64 last) -> QString {
        if (interval < 30) return DialogRuntimeStats::tr("Disabled");
        const qint64 remaining = last > 0
            ? last + static_cast<qint64>(interval) * 60 - QDateTime::currentSecsSinceEpoch()
            : 0;
        if (remaining <= 0) return DialogRuntimeStats::tr("Due now");
        return DialogRuntimeStats::tr("in %1").arg(humanizeDuration(remaining));
    };
    auto* settings = Configs::dataManager->settingsRepo.get();
    ui->vSubUpdate->setText(nextUpd(settings->sub_auto_update, settings->sub_auto_update_last));
    ui->vRouteUpdate->setText(nextUpd(settings->route_auto_update, settings->route_auto_update_last));

    // --- On-disk size of the databases (main + stats, incl. WAL/SHM sidecars) ---
    qint64 dbBytes = 0;
    const QDir dir(QDir::currentPath());
    for (const QFileInfo& fi : dir.entryInfoList(QStringList{QStringLiteral("throne*.db*")}, QDir::Files))
        dbBytes += fi.size();
    ui->vDbSize->setText(ReadableSize(dbBytes));

    // --- Throne uptime ---
    const qint64 up = appStartEpoch > 0 ? QDateTime::currentSecsSinceEpoch() - appStartEpoch : 0;
    ui->vUptime->setText(humanizeDuration(up));

    // --- Running config identity + button-free egress probe (on change or every 30s) ---
    const QString cfgName = mw ? mw->GetRunningConfigName() : QString();
    const qint64 nowSecs = QDateTime::currentSecsSinceEpoch();
    if (cfgName.isEmpty()) {
        ui->vCfgName->setText(tr("No active config"));
        ui->vOutIp->setText(QStringLiteral("—"));
        ui->vCountry->setText(QStringLiteral("—"));
        ui->vPing->setText(QStringLiteral("—"));
        lastProbedConfig_.clear();
        egressSnapshotDone_ = false;
    } else {
        ui->vCfgName->setText(cfgName);
        if (cfgName != lastProbedConfig_) {
            lastProbedConfig_ = cfgName;
            egressSnapshotDone_ = false;
            lastProbeSecs_ = 0;
        }
        if (!egressSnapshotDone_ && (lastProbeSecs_ == 0 || nowSecs - lastProbeSecs_ >= 30)) {
            lastProbeSecs_ = nowSecs;
            probeEgress();
        }
    }

    if (!connBusy_.exchange(true)) {
        QPointer<DialogRuntimeStats> self(this);
        runOnNewThread([self]() {
            const auto conns = API::defaultClient->QueryConnections();
            int tcp = 0, udp = 0, total = 0;
            for (const auto& c : conns.active) {
                ++total;
                const QString net = QString::fromStdString(c.network.value());
                if (net == QStringLiteral("tcp")) ++tcp;
                else if (net == QStringLiteral("udp")) ++udp;
            }
            runOnUiThread([self, tcp, udp, total]() {
                if (!self) return;
                self->ui->vConns->setText(
                    DialogRuntimeStats::tr("%1 active   ·   %2 TCP   ·   %3 UDP").arg(total).arg(tcp).arg(udp));
                self->connBusy_.store(false);
            });
        });
    }
}

void DialogRuntimeStats::probeEgress() {
    if (probing_.exchange(true)) return; // a probe is already running

    auto* mw = GetMainWindow();
    if (mw == nullptr || mw->GetRunningConfigName().isEmpty()) {
        probing_.store(false);
        return;
    }

    // Immediate feedback while the (blocking) probe runs.
    ui->vPing->setText(QStringLiteral("…"));
    if (ui->vOutIp->text() == QStringLiteral("—")) ui->vOutIp->setText(QStringLiteral("…"));
    if (ui->vCountry->text() == QStringLiteral("—")) ui->vCountry->setText(QStringLiteral("…"));

    QPointer<DialogRuntimeStats> self(this);
    runOnNewThread([self]() {
        // Ping: latency of the live instance (same call as "test current").
        QString pingText;
        {
            libcore::TestReq req;
            req.test_current = true;
            req.url = Configs::dataManager->settingsRepo->test_latency_url.toStdString();
            bool ok = false;
            const auto res = API::defaultClient->Test(&ok, req);
            if (ok && !res.results.empty()) {
                const int lat = res.results[0].latency_ms.value();
                pingText = lat > 0 ? QStringLiteral("%1 ms").arg(lat) : DialogRuntimeStats::tr("Unavailable");
            } else {
                pingText = DialogRuntimeStats::tr("N/A");
            }
        }

        // Out IP + country: ip-api through the running proxy (as done at profile start).
        QString ipText = DialogRuntimeStats::tr("N/A");
        QString countryText = DialogRuntimeStats::tr("N/A");
        bool egressOk = false; // did the lookup yield a real out IP?
        const auto resp = NetworkRequestHelper::HttpGet(QStringLiteral("http://ip-api.com/json/"), false, true);
        if (resp.error.isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(resp.data);
            if (doc.isObject()) {
                const QJsonObject obj = doc.object();
                const QString ip = obj[QStringLiteral("query")].toString();
                const QString countryName = obj[QStringLiteral("country")].toString();
                const QString countryCode = obj[QStringLiteral("countryCode")].toString();
                const QString city = obj[QStringLiteral("city")].toString();
                if (!ip.isEmpty()) {
                    ipText = ip;
                    egressOk = true;
                }
                if (!countryName.isEmpty()) {
                    countryText = CountryCodeToFlag(countryCode) + QStringLiteral(" ") + countryName;
                    if (!city.isEmpty()) countryText += QStringLiteral(", ") + city;
                }
            }
        }

        runOnUiThread([self, pingText, ipText, countryText, egressOk]() {
            if (!self) return;
            if (!self->egressSnapshotDone_) {
                self->ui->vPing->setText(pingText);
                self->ui->vOutIp->setText(ipText);
                self->ui->vCountry->setText(countryText);
                if (egressOk) self->egressSnapshotDone_ = true;
            }
            self->probing_.store(false);
        });
    });
}
