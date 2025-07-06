#pragma once

#include <QString>

bool Linux_HavePkexec();

QString Linux_FindCapProgsExec(const QString &name);

int Linux_Run_Command(const QString &commandName, const QString &args);

int Linux_Run_RootScript(const QStringList &commands);