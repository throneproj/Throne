#include "include/sys/WindowFrame.hpp"
#include "include/sys/windows/WinVersion.h"

#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QWidget>
#include <QtMath>

#include <windows.h>

namespace WindowFrame {
    namespace {
        // One window at a time can be under the mouse.
        HWND frameLeaveTracked = nullptr;

        // The side border DefWindowProc keeps; the resize band along the top gets the same height.
        int frameBorderThickness(HWND hwnd) {
            RECT frame{};
            GetWindowRect(hwnd, &frame);
            POINT origin{0, 0};
            ClientToScreen(hwnd, &origin);
            return origin.x - frame.left;
        }

        LRESULT frameTopEdgeHit(HWND hwnd, const POINT &client, int thickness) {
            RECT area{};
            GetClientRect(hwnd, &area);
            if (client.x < 2 * thickness) return HTTOPLEFT;
            if (client.x >= area.right - 2 * thickness) return HTTOPRIGHT;
            return HTTOP;
        }
    }

    void Refresh(QWidget *window) {
        const auto hwnd = reinterpret_cast<HWND>(window->internalWinId());
        if (hwnd == nullptr) return;
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
    }

    void DropNativeCaption(QWidget *window) {
        const auto hwnd = reinterpret_cast<HWND>(window->internalWinId());
        if (hwnd == nullptr || IsZoomed(hwnd) || IsIconic(hwnd)) return;
        RECT frame{};
        GetWindowRect(hwnd, &frame);
        // The client DefWindowProc would leave in this window rect, caption included.
        RECT native = frame;
        DefWindowProcW(hwnd, WM_NCCALCSIZE, FALSE, reinterpret_cast<LPARAM>(&native));
        const int caption = native.top - frame.top;
        if (caption <= 0) return;
        SetWindowPos(hwnd, nullptr, frame.left, frame.top + caption, frame.right - frame.left, frame.bottom - frame.top - caption,
                     SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
    }

    QRect NormalGeometry(QWidget *window) {
        const auto hwnd = reinterpret_cast<HWND>(window->internalWinId());
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        if (hwnd == nullptr || !GetWindowPlacement(hwnd, &placement)) return {};
        RECT frame = placement.rcNormalPosition;
        // Workspace coordinates, which leave out a taskbar along the top or left.
        if (!(GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) {
            MONITORINFO monitor{};
            monitor.cbSize = sizeof(monitor);
            if (GetMonitorInfoW(MonitorFromRect(&frame, MONITOR_DEFAULTTONEAREST), &monitor)) {
                OffsetRect(&frame, monitor.rcWork.left - monitor.rcMonitor.left, monitor.rcWork.top - monitor.rcMonitor.top);
            }
        }
        // The frame of a restored window: side and bottom borders only. Mapped as Qt maps native geometry, about the
        // screen's top-left, which is the same point in both.
        const int border = frameBorderThickness(hwnd);
        const qreal ratio = window->devicePixelRatioF();
        const QPoint origin = window->screen()->geometry().topLeft();
        const QPoint topLeft = QPoint(frame.left + border, frame.top) - origin;
        return {QPoint(qRound(topLeft.x() / ratio), qRound(topLeft.y() / ratio)) + origin,
                QSize(qRound((frame.right - frame.left - 2 * border) / ratio), qRound((frame.bottom - frame.top - border) / ratio))};
    }

    bool HandleMessage(QWidget *window, Caption &caption, void *message, qintptr *result) {
        const auto *msg = static_cast<const MSG *>(message);
        const HWND hwnd = msg->hwnd;
        switch (msg->message) {
        case WM_NCCALCSIZE: {
            if (msg->wParam == FALSE) return false;
            auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
            const RECT proposed = params->rgrc[0];
            *result = DefWindowProcW(hwnd, msg->message, msg->wParam, msg->lParam);
            // DefWindowProc keeps the side and bottom borders. The caption and top border join the client area, except
            // that a maximized window hangs its border off the screen on every side.
            params->rgrc[0].top = proposed.top;
            if (IsZoomed(hwnd)) params->rgrc[0].top += params->rgrc[0].left - proposed.left;
            return true;
        }
        case WM_NCHITTEST: {
            const bool maximized = IsZoomed(hwnd);
            if (!maximized) {
                const LRESULT native = DefWindowProcW(hwnd, msg->message, msg->wParam, msg->lParam);
                if (native != HTCLIENT) {
                    *result = native;
                    return true;
                }
            }
            // Signed: a monitor left of or above the primary one has negative coordinates.
            POINT point{static_cast<short>(LOWORD(msg->lParam)), static_cast<short>(HIWORD(msg->lParam))};
            ScreenToClient(hwnd, &point);
            const qreal ratio = window->devicePixelRatioF();
            const Area area = caption.AreaAt(QPoint(qFloor(point.x / ratio), qFloor(point.y / ratio)));
            const int thickness = maximized ? 0 : frameBorderThickness(hwnd);
            // The buttons win over the resize band, as native ones do at the top of a maximized window.
            if (area == Area::MaximizeButton) *result = HTMAXBUTTON;
            else if (area == Area::OtherButton) *result = HTCLIENT;
            else if (point.y < thickness) *result = frameTopEdgeHit(hwnd, point, thickness);
            else *result = area == Area::Caption ? HTCAPTION : HTCLIENT;
            return true;
        }
        case WM_NCMOUSEMOVE: {
            const bool over = msg->wParam == HTMAXBUTTON;
            if (over && frameLeaveTracked != hwnd) {
                TRACKMOUSEEVENT track{sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_NONCLIENT, hwnd, HOVER_DEFAULT};
                if (TrackMouseEvent(&track)) frameLeaveTracked = hwnd;
            }
            caption.SetMaximizeButtonState(over, over && caption.MaximizeButtonPressed());
            // DefWindowProc still sees the hover, which is what opens the snap layouts.
            return false;
        }
        case WM_NCMOUSELEAVE:
            if (frameLeaveTracked == hwnd) frameLeaveTracked = nullptr;
            caption.SetMaximizeButtonState(false, false);
            return false;
        // DefWindowProc would track a native button nobody can see.
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONDBLCLK:
            if (msg->wParam != HTMAXBUTTON) return false;
            caption.SetMaximizeButtonState(true, true);
            *result = 0;
            return true;
        case WM_NCLBUTTONUP:
            if (msg->wParam != HTMAXBUTTON) return false;
            if (caption.MaximizeButtonPressed()) {
                caption.SetMaximizeButtonState(true, false);
                caption.ToggleMaximized();
            }
            *result = 0;
            return true;
        default:
            return false;
        }
    }

    bool DrawsOwnTopBorder() {
        static const bool windows10 = !WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_11_21H2);
        return windows10;
    }
}
