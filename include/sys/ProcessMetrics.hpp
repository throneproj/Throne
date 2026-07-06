#pragma once

#include <QHash>
#include <QtGlobal>

namespace Sys {
    // Per-process CPU% and resident-memory sampler.
    //
    // CPU% is a rate: it is derived from the change in the process's cumulative
    // CPU time between two successive samples divided by the elapsed wall-clock
    // time, then normalized by the logical CPU count (so a value of 100 means the
    // whole machine). Because a rate needs two data points, the first sample for a
    // given pid reports cpuPercent = 0 and just seeds the baseline. Sample on a
    // fixed cadence (e.g. from a timer) for a stable reading.
    class ProcessMetrics {
    public:
        struct Sample {
            bool ok = false;         // false when the pid could not be queried
            qint64 rssBytes = 0;     // resident set / working-set size, bytes
            double cpuPercent = 0.0; // 0..100 of total machine CPU; 0 on first sample
        };

        // Sample the given process id (0/negative -> not ok). Keeps a per-pid
        // baseline internally so consecutive calls yield the CPU rate. A pid that
        // is reused by a new process is handled by the non-negative-delta guard
        // (the fresh process reports less cumulative CPU, so that tick reads 0).
        Sample sample(qint64 pid);

    private:
        struct Prior {
            quint64 cpuTimeNs = 0; // cumulative process CPU time (user+kernel), ns
            qint64 wallNs = 0;     // monotonic wall clock at that sample, ns
        };
        QHash<qint64, Prior> prior_;
    };
} // namespace Sys
