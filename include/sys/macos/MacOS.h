#pragma once

#include <QString>

int Mac_Run_Command(QString command);

// Hides the title text of the window holding this NSView. The title still names the window in the Dock, Mission Control
// and VoiceOver.
void Mac_HideWindowTitle(quintptr view);
