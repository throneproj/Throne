#pragma once

#include <QString>

namespace Stats {
    enum class TrafficLineKind { Proxy, Route, Aux, Other, Direct };

    // One graph line. key is stable across restarts: "proxy", "direct", "other", "route:<profile id>", "aux:<profile id>".
    struct TrafficLine {
        QString key;
        QString name;
        TrafficLineKind kind = TrafficLineKind::Proxy;
    };
}
