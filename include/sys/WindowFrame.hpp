#pragma once

#include <QtGlobal>

class QPoint;
class QRect;
class QWidget;

// A title bar drawn in the client area of a window that keeps its native frame, and with it the shadow, resizing and
// snapping. Implemented on Windows only.
namespace WindowFrame {
    enum class Area {
        Client,
        Caption,
        // Hit-tested as the native one, so Windows 11 offers its snap layouts there.
        MaximizeButton,
        OtherButton,
    };

    class Caption {
    public:
        virtual ~Caption() = default;
        // pos: logical, in the top-level window's coordinates.
        [[nodiscard]] virtual Area AreaAt(const QPoint &pos) const = 0;
        // The maximize button's mouse input arrives as non-client messages, which its widget never sees.
        virtual void SetMaximizeButtonState(bool hovered, bool pressed) = 0;
        [[nodiscard]] virtual bool MaximizeButtonPressed() const = 0;
        virtual void ToggleMaximized() = 0;
    };

    // Re-runs the frame calculation after the caption moved in or out; the window keeps its outer size.
    void Refresh(QWidget *window);

    // Cuts the native caption's band off a window that was placed around it, so its client keeps the rect it was given.
    // Qt creates a window that way, and the geometry Throne saves and restores is the client's.
    void DropNativeCaption(QWidget *window);

    // The logical client rect a window restores to. Qt's normalGeometry() of a maximized one is measured with the
    // maximized frame, which hangs a border above the client where the restored frame has none.
    QRect NormalGeometry(QWidget *window);

    // For QWidget::nativeEvent of a window whose caption lives in its client area.
    bool HandleMessage(QWidget *window, Caption &caption, void *message, qintptr *result);

    // Windows 10 takes the top border away along with the caption.
    bool DrawsOwnTopBorder();
}
