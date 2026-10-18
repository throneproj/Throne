#pragma once

#include <functional>
#include <optional>

#include <QString>

#include "include/configs/sub/GroupUpdater.hpp"
#include "include/engine/services/ServiceTypes.h"

namespace Engine::ImportService {
    enum class Source {
        // payload is a document: links, a subscription body, a config file.
        Text,
        // payload is an http or https URL fetched once.
        Url,
    };

    struct Request {
        Source source = Source::Text;
        QString payload;
        // -1: the group the window shows, resolved when the request is made.
        int groupId = -1;
    };

    using Done = std::function<void(const Subscription::UpdateOutcome &outcome)>;

    // Main thread. Checks the request and the target group, then queues a quiet import (no report box; one log entry);
    // done runs on the main thread. *groupId receives the resolved group. Returns the refusal instead when it queued nothing.
    std::optional<ServiceError> Import(const Request &request, const Done &done, int *groupId = nullptr);
} // namespace Engine::ImportService
