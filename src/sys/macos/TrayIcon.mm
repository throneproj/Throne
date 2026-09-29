#include "include/ui/widget/TrayIcon.hpp"

#include <QGuiApplication>

#import <AppKit/AppKit.h>

namespace {
    constexpr int kStatusIconPoints = 18;

    NSStatusItem *item(void *statusItem) { return static_cast<NSStatusItem *>(statusItem); }
}

TrayIcon::TrayIcon(QObject *parent) : QObject(parent) {
    NSStatusItem *statusItem = [[NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength] retain];
    statusItem.visible = NO;
    m_statusItem = statusItem;
}

TrayIcon::~TrayIcon() {
    [NSStatusBar.systemStatusBar removeStatusItem:item(m_statusItem)];
    [item(m_statusItem) release];
}

void TrayIcon::setIcon(const QIcon &icon) {
    CGImageRef cgImage = icon.pixmap(QSize(kStatusIconPoints, kStatusIconPoints), qGuiApp->devicePixelRatio()).toImage().toCGImage();
    NSImage *image = [[NSImage alloc] initWithCGImage:cgImage size:NSMakeSize(kStatusIconPoints, kStatusIconPoints)];
    CGImageRelease(cgImage);
    item(m_statusItem).button.image = image;
    [image release];
}

void TrayIcon::setToolTip(const QString &text) {
    item(m_statusItem).button.toolTip = text.toNSString();
}

void TrayIcon::setContextMenu(QMenu *menu) {
    item(m_statusItem).menu = menu ? menu->toNSMenu() : nil;
}

void TrayIcon::setVisible(bool visible) {
    m_visible = visible;
    item(m_statusItem).visible = visible;
}

bool TrayIcon::isVisible() const {
    return m_visible;
}
