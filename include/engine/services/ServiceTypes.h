#pragma once

#include <QString>

namespace Engine {
    // A request a service turned down before changing anything. status and code are the Throne API's; the GUI shows message.
    struct ServiceError {
        int status = 400;
        QString code;
        QString message;
    };
} // namespace Engine
