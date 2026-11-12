#pragma once

class QString;
class QWidget;

void Windows_QWidget_SetForegroundWindow(QWidget* w);

bool Windows_IsInAdmin();

// The path with links resolved and every name in its on-disk case, as the OS reports a running image; empty on failure.
QString Windows_FinalPath(const QString& path);
