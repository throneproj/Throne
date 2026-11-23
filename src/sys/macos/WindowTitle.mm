#include "include/sys/macos/MacOS.h"

#import <AppKit/AppKit.h>

void Mac_HideWindowTitle(quintptr view) {
    NSWindow *window = ((__bridge NSView *)reinterpret_cast<void *>(view)).window;
    if (window != nil) window.titleVisibility = NSWindowTitleHidden;
}
