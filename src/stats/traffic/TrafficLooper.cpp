#include "include/stats/traffic/TrafficLooper.hpp"

#include "include/api/gRPC.h"
#include "include/ui/mainwindow_interface.h"

#include <QThread>
#include <QJsonDocument>
#include <QElapsedTimer>

namespace NekoGui_traffic {

    TrafficLooper *trafficLooper = new TrafficLooper;
    QElapsedTimer elapsedTimer;

    void TrafficLooper::UpdateAll() {
        if (NekoGui::dataStore->disable_traffic_stats) {
            return;
        }

        auto resp = NekoGui_rpc::defaultClient->QueryStats();
        proxy->uplink_rate = 0;
        proxy->downlink_rate = 0;

        int proxyUp = 0, proxyDown = 0;

        for (const auto &item: this->items) {
            if (!resp.ups().contains(item->tag)) continue;
            auto now = elapsedTimer.elapsed();
            auto interval = now - item->last_update;
            item->last_update = now;
            if (interval <= 0) continue;
            auto up = resp.ups().at(item->tag);
            auto down = resp.downs().at(item->tag);
            if (item->tag == "proxy")
            {
                proxyUp = up;
                proxyDown = down;
            }
            item->uplink += up;
            item->downlink += down;
            item->uplink_rate = static_cast<double>(up) * 1000.0 / static_cast<double>(interval);
            item->downlink_rate = static_cast<double>(down) * 1000.0 / static_cast<double>(interval);
            if (item->ignoreForRate) continue;
            if (item->tag == "direct")
            {
                direct->uplink_rate = item->uplink_rate;
                direct->downlink_rate = item->downlink_rate;
            } else
            {
                proxy->uplink_rate += item->uplink_rate;
                proxy->downlink_rate += item->downlink_rate;
            }
        }
        if (isChain)
        {
            for (const auto &item: this->items)
            {
                if (item->isChainTail)
                {
                    item->uplink += proxyUp;
                    item->downlink += proxyDown;
                }
            }
        }
    }

    void TrafficLooper::Loop() {
        elapsedTimer.start();
        while (true) {
            QThread::msleep(1000); // refresh every one second

            if (NekoGui::dataStore->disable_traffic_stats) {
                continue;
            }

            // profile start and stop
            if (!loop_enabled) {
                // 停止
                if (looping) {
                    looping = false;
                    runOnUiThread([=] {
                        auto m = GetMainWindow();
                        m->refresh_status("STOP");
                    });
                }
                runOnUiThread([=]
                {
                   auto m = GetMainWindow();
                   m->update_traffic_graph(0, 0, 0, 0);
                });
                continue;
            } else {
                // 开始
                if (!looping) {
                    looping = true;
                }
            }

            // do update
            loop_mutex.lock();

            UpdateAll();

            loop_mutex.unlock();

            // post to UI
            runOnUiThread([=] {
                auto m = GetMainWindow();
                if (proxy != nullptr) {
                    m->refresh_status(QObject::tr("Proxy: %1\nDirect: %2").arg(proxy->DisplaySpeed(), direct->DisplaySpeed()));
                    m->update_traffic_graph(proxy->downlink_rate, proxy->uplink_rate, direct->downlink_rate, direct->uplink_rate);
                }
                for (const auto &item: items) {
                    if (item->id < 0) continue;
                    m->refresh_proxy_list(item->id);
                }
            });
        }
    }

} // namespace NekoGui_traffic
