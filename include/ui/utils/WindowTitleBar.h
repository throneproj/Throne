#pragma once

#include <QPixmap>
#include <QWidget>

#include "include/sys/WindowFrame.hpp"

class QMainWindow;
class WindowCaptionButton;

// Throne's own title bar: the app icon and name centred on a band of the theme's window colour, then the window buttons.
class WindowTitleBar : public QWidget, public WindowFrame::Caption {
    Q_OBJECT

public:
    // Windows on Qt 6.6+: older Qt keeps working its frame margins out from the window style, so the geometry drifts.
    static bool Supported();

    // Moves the window's central widget below a new title bar, which shows once setCustomFrame() turns it on.
    static WindowTitleBar *Install(QMainWindow *window);

    // A native title bar shows the window's name only where the app icon sits beside it: a bare name looks worse than
    // none. For every show of the window, since macOS can replace its NSWindow.
    static void ApplyNativeTitle(QWidget *window);

    void setCustomFrame(bool enabled);

    // For the window's QWidget::nativeEvent.
    bool handleWindowMessage(const QByteArray &eventType, void *message, qintptr *result);

    // The window's QWidget::saveGeometry(), with the normal geometry it really restores to while maximized.
    [[nodiscard]] QByteArray saveWindowGeometry() const;

    [[nodiscard]] WindowFrame::Area AreaAt(const QPoint &pos) const override;
    void SetMaximizeButtonState(bool hovered, bool pressed) override;
    [[nodiscard]] bool MaximizeButtonPressed() const override;
    void ToggleMaximized() override;

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    explicit WindowTitleBar(QMainWindow *window);

    void applyMetrics();
    void syncWindowState();
    const QPixmap &icon(int size);

    QMainWindow *m_window;
    WindowCaptionButton *m_minimize;
    WindowCaptionButton *m_maximize;
    WindowCaptionButton *m_close;
    QPixmap m_icon;
    bool m_customFrame = false;
};
