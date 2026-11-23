#pragma once

#include <QList>
#include <QString>

#include "include/ui/utils/ServiceIcons.h"

namespace Configs {
    class Profile;
}

// A profile's stored service check for display; the rich forms are colored with the theme's tokens.
namespace ServiceCheckHtml {
    // UI thread only: parsed results are cached in a static. Skips the services turned off in the settings.
    [[nodiscard]] QList<ServiceIcons::Item> items(const Configs::Profile &profile);

    [[nodiscard]] QString ipKind(const Configs::Profile &profile, bool rich);

    // Why the whole check failed; empty when it ran.
    [[nodiscard]] QString failure(const Configs::Profile &profile);

    [[nodiscard]] QString summaryLine(const Configs::Profile &profile);
}
