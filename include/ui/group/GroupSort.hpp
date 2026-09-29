#pragma once

namespace GroupSortMethod {
    enum GroupSortMethod {
        Raw,
        ByType,
        ByAddress,
        ByName,
        ByTestResult,
        ById,
        ByTraffic,
        BySecurity,
        // ByTestResult pinned to latency, whatever the group's test_sort_by.
        ByLatency,
    };
}

struct GroupSortAction {
    GroupSortMethod::GroupSortMethod method = GroupSortMethod::Raw;
    bool descending = false;
};
