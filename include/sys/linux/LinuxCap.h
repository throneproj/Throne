#pragma once

#include <QString>
#include <QStringList>

bool Linux_HavePkexec();

QString Linux_FindCapProgsExec(const QString &name);

int Linux_Run_Command(const QString &commandName, const QStringList &args);