#pragma once

#include <QString>

bool AutoRun_SetEnabled(bool enable, QString *error = nullptr);

bool AutoRun_IsEnabled();

void AutoRun_FixTaskIfNeeded();

void AutoRun_MigrateIfNeeded();
