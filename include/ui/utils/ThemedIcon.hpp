#pragma once

#include <QIcon>

// A :/icon/lucide/<name> mask, tinted each time it is painted: with the application palette's button text,
// or with a muted colour of hue (0-359) that keeps its contrast against the window.
QIcon ThemedIcon(const QString &name, int hue = -1);
