#pragma once

#include <QString>

bool Linux_HavePkexec();

QString Linux_FindCapProgsExec(const QString &name);

int Linux_Run_Command(const QString &commandName, const QString &args);

/** Run chown root:root and chmod u+s on core in a single pkexec (one password prompt). */
int Linux_ElevateCorePermissions(const QString &corePath);