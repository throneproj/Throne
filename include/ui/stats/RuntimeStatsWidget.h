#pragma once

#include <QWidget>
#include <QList>
#include <QPointer>
#include <QStringList>

#include <atomic>
#include <memory>

#include "ui_RuntimeStatsWidget.h"

#include "include/sys/ProcessMetrics.hpp"
#include "include/ui/stats/dialog_endpoint_details.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class RuntimeStatsWidget;
}
QT_END_NAMESPACE

class QTimer;
class LiveChart;

class RuntimeStatsWidget : public QWidget {
    Q_OBJECT

public:
    explicit RuntimeStatsWidget(QWidget* parent = nullptr);
    ~RuntimeStatsWidget() override;

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void refreshLive();
    void refreshServices();
    void probeEgress();

    void applyEndpoints(const QList<Stats::VpnEndpointView>& views);
    void fitEndpointTable();
    void openEndpointDetails(const QString& tag);
    [[nodiscard]] QString selectedEndpointTag() const;
    [[nodiscard]] bool panelActive() const;

    Ui::RuntimeStatsWidget* ui;
    QTimer* timer_ = nullptr;
    LiveChart* cpuChart_ = nullptr;
    LiveChart* ramChart_ = nullptr;
    Sys::ProcessMetrics metrics_;
    std::atomic<bool> probing_{false};
    std::atomic<bool> connBusy_{false};
    std::atomic<bool> vpnBusy_{false};

    QString lastProbedConfig_;
    qint64 lastProbeSecs_ = 0;
    bool egressSnapshotDone_ = false;
    // The exit lookup provider the egress rows belong to; empty while the lookup is off.
    QString egressLookup_;

    QPointer<DialogEndpointDetails> details_;
    // Shared with the check's onFinished, which runs on a test thread and may outlive the widget.
    std::shared_ptr<std::atomic<bool>> serviceChecking_ = std::make_shared<std::atomic<bool>>(false);
    QList<Stats::VpnEndpointView> endpointViews_;
    QStringList endpointTags_;
};
