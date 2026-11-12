#pragma once

#include <QString>

bool AutoRun_SetEnabled(bool enable, QString *error = nullptr);

bool AutoRun_IsEnabled();

void AutoRun_FixTaskIfNeeded();

void AutoRun_MigrateIfNeeded();

#ifdef Q_OS_WIN
// The elevated grant helper, which has no settings: this user's existing autorun task for exePath starts it unelevated.
void AutoRun_SetLeastPrivilege(const QString &exePath);
#endif
