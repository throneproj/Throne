#pragma once

// implement in mainwindow
namespace GroupSortMethod {
    enum GroupSortMethod {
        Raw,
        ByType,
        ByAddress,
        ByName,
        ByTestResult,
        ById,
        ByTraffic,
    };
}

struct GroupSortAction {
    GroupSortMethod::GroupSortMethod method = GroupSortMethod::Raw;
    bool descending = false; // 默认升序，开这个就是降序
};
