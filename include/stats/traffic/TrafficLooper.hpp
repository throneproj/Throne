#pragma once

#include <QString>
#include <QList>
#include <QMutex>

#include "TrafficData.hpp"

namespace NekoGui_traffic {
    class TrafficLooper {
    public:
        bool loop_enabled = false;
        bool looping = false;
        QMutex loop_mutex;

        QList<std::shared_ptr<TrafficData>> items;
        bool isChain;
        std::shared_ptr<TrafficData> proxy;
        std::shared_ptr<TrafficData> direct;

        void UpdateAll();

        void Loop();
    };

    extern TrafficLooper *trafficLooper;
} // namespace NekoGui_traffic
