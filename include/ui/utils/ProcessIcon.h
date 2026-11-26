#pragma once

#include <QIcon>
#include <QString>

namespace ProcessIcon {
    // The program's own icon, else a generic program icon, so every row lines up; cached per path. UI thread only.
    [[nodiscard]] QIcon ForPath(const QString &path);
} // namespace ProcessIcon
