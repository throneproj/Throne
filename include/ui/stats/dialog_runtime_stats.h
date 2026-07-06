#pragma once

#include <QDialog>
#include <QPointer>

#include <atomic>

#include "ui_dialog_runtime_stats.h"

#include "include/sys/ProcessMetrics.hpp"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogRuntimeStats;
}
QT_END_NAMESPACE

class QTimer;

// Live process/system health panel for Throne and its core: per-process memory
// and CPU, active connection counts, current speed, subscription/routing update
// countdowns, on-disk database size, app uptime, and a probe of the running
// config's egress (out IP, country, ping). A timer refreshes the cheap in-process
// metrics ~1/s; the network-dependent egress probe runs once on open (and on the
// Refresh button) off the UI thread. Complements the historical Traffic Stats view.
class DialogRuntimeStats : public QDialog {
    Q_OBJECT

public:
    explicit DialogRuntimeStats(QWidget* parent = nullptr);
    ~DialogRuntimeStats() override;

private:
    // Cheap in-process metrics, driven by the refresh timer.
    void refreshLive();
    // Network egress probe (out IP / country / ping); runs on a worker thread.
    void probeEgress();

    Ui::DialogRuntimeStats* ui;
    QTimer* timer_ = nullptr;
    Sys::ProcessMetrics metrics_;
    // Guards against overlapping egress probes (each does a blocking RPC + HTTP).
    std::atomic<bool> probing_{false};
    // Guards the connections poll (QueryConnections is heavy — the closed ring
    // rides along) so timer ticks don't stack while one is in flight.
    std::atomic<bool> connBusy_{false};

    // The egress probe has no button: it re-runs when the active config changes
    // or after an interval. Tracked here (UI thread only).
    QString lastProbedConfig_;
    qint64 lastProbeSecs_ = 0;
};
