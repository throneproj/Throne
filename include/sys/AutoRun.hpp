#pragma once

#include <qglobal.h>

void AutoRun_SetEnabled(bool enable);

bool AutoRun_IsEnabled();

void AutoRun_FixPrivilegeIfNeeded();

void AutoRun_MigrateIfNeeded();
