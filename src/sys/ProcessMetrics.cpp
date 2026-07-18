#include "include/sys/ProcessMetrics.hpp"

#include <QThread>

#include <chrono>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <psapi.h>
#elif defined(Q_OS_MACOS)
#include <libproc.h>
#include <sys/resource.h>
#else // Linux / other POSIX
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#endif

namespace {
    // One platform read: cumulative CPU time (ns) + resident memory (bytes).
    struct Raw {
        bool ok = false;
        quint64 cpuNs = 0;
        qint64 rss = 0;
    };

    int cpuCount() {
        const int n = QThread::idealThreadCount();
        return n > 0 ? n : 1;
    }

    qint64 monotonicNs() {
        return static_cast<qint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

#if defined(Q_OS_WIN)
    Raw readRaw(qint64 pid) {
        Raw r;
        if (pid <= 0) return r;

        HANDLE h;
        bool ownHandle = false;
        if (static_cast<DWORD>(pid) == GetCurrentProcessId()) {
            h = GetCurrentProcess(); // pseudo-handle, must not be closed
        } else {
            h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
            if (h == nullptr) return r;
            ownHandle = true;
        }

        FILETIME ftCreate, ftExit, ftKernel, ftUser;
        if (GetProcessTimes(h, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
            ULARGE_INTEGER k, u;
            k.LowPart = ftKernel.dwLowDateTime;
            k.HighPart = ftKernel.dwHighDateTime;
            u.LowPart = ftUser.dwLowDateTime;
            u.HighPart = ftUser.dwHighDateTime;
            // FILETIME counts are in 100-nanosecond units.
            r.cpuNs = (k.QuadPart + u.QuadPart) * 100ULL;
        }

        PROCESS_MEMORY_COUNTERS_EX2 ex2{};
        ex2.cb = sizeof(ex2);
        if (GetProcessMemoryInfo(h, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&ex2), sizeof(ex2))
            && ex2.PrivateWorkingSetSize > 0) {
            r.rss = static_cast<qint64>(ex2.PrivateWorkingSetSize);
        } else {
            PROCESS_MEMORY_COUNTERS_EX pmcx{};
            pmcx.cb = sizeof(pmcx);
            if (GetProcessMemoryInfo(h, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmcx), sizeof(pmcx))) {
                r.rss = static_cast<qint64>(pmcx.PrivateUsage);
            }
        }

        r.ok = true;
        if (ownHandle) CloseHandle(h);
        return r;
    }
#elif defined(Q_OS_MACOS)
    Raw readRaw(qint64 pid) {
        Raw r;
        if (pid <= 0) return r;
        struct rusage_info_v2 ri;
        if (proc_pid_rusage(static_cast<int>(pid), RUSAGE_INFO_V2,
                            reinterpret_cast<rusage_info_t*>(&ri)) == 0) {
            // ri_user_time / ri_system_time are already in nanoseconds.
            r.cpuNs = ri.ri_user_time + ri.ri_system_time;
            r.rss = static_cast<qint64>(ri.ri_phys_footprint);
            r.ok = true;
        }
        return r;
    }
#else // Linux / other POSIX
    Raw readRaw(qint64 pid) {
        Raw r;
        if (pid <= 0) return r;

        char path[64];
        // CPU: utime (field 14) + stime (field 15) of /proc/<pid>/stat, in clock
        // ticks. The comm field (2) may contain spaces/parens, so parse after the
        // last ')'.
        std::snprintf(path, sizeof(path), "/proc/%lld/stat", static_cast<long long>(pid));
        if (FILE* f = std::fopen(path, "r")) {
            char buf[1024];
            const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
            std::fclose(f);
            if (n > 0) {
                buf[n] = '\0';
                char* p = std::strrchr(buf, ')');
                if (p != nullptr) {
                    ++p; // now at the space before the state field (field 3)
                    unsigned long long utime = 0, stime = 0;
                    int idx = 0;
                    char* save = nullptr;
                    for (char* tok = strtok_r(p, " ", &save); tok != nullptr && idx <= 12;
                         tok = strtok_r(nullptr, " ", &save), ++idx) {
                        if (idx == 11) utime = std::strtoull(tok, nullptr, 10);
                        else if (idx == 12) stime = std::strtoull(tok, nullptr, 10);
                    }
                    long ticks = sysconf(_SC_CLK_TCK);
                    if (ticks <= 0) ticks = 100;
                    r.cpuNs = (utime + stime) * (1000000000ULL / static_cast<quint64>(ticks));
                }
            }
        }

        // RSS: resident pages (field 2) of /proc/<pid>/statm.
        std::snprintf(path, sizeof(path), "/proc/%lld/statm", static_cast<long long>(pid));
        if (FILE* fm = std::fopen(path, "r")) {
            long long totalPages = 0, rssPages = 0;
            if (std::fscanf(fm, "%lld %lld", &totalPages, &rssPages) == 2) {
                r.rss = static_cast<qint64>(rssPages) * static_cast<qint64>(sysconf(_SC_PAGESIZE));
            }
            std::fclose(fm);
        }

        r.ok = true;
        return r;
    }
#endif
} // namespace

namespace Sys {
    ProcessMetrics::Sample ProcessMetrics::sample(qint64 pid) {
        Sample s;
        const Raw raw = readRaw(pid);
        if (!raw.ok) {
            prior_.remove(pid);
            return s;
        }
        s.ok = true;
        s.rssBytes = raw.rss;

        const qint64 nowNs = monotonicNs();
        if (const auto it = prior_.constFind(pid); it != prior_.constEnd()) {
            const qint64 dWall = nowNs - it->wallNs;
            const qint64 dCpu = static_cast<qint64>(raw.cpuNs) - static_cast<qint64>(it->cpuTimeNs);
            if (dWall > 0 && dCpu >= 0) {
                s.cpuPercent = static_cast<double>(dCpu) / static_cast<double>(dWall)
                               * 100.0 / static_cast<double>(cpuCount());
            }
        }
        prior_.insert(pid, Prior{raw.cpuNs, nowNs});
        return s;
    }
} // namespace Sys
