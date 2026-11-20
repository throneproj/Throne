#include "include/ui/utils/WindowTitleBar.h"

#include <QAbstractButton>
#include <QDataStream>
#include <QEvent>
#include <QHBoxLayout>
#include <QIODevice>
#include <QImage>
#include <QMainWindow>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#ifdef Q_OS_MACOS
#include "include/sys/macos/MacOS.h"
#endif

namespace {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // GNOME and the shells built on it draw no app icon in a title bar.
    bool titleBarShowsNoIcon() {
        const auto desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':'), Qt::SkipEmptyParts);
        for (const auto &desktop : desktops) {
            for (const char *name : {"GNOME", "Unity", "Pantheon", "Budgie"}) {
                if (desktop.compare(QLatin1String(name), Qt::CaseInsensitive) == 0) return true;
            }
        }
        return false;
    }
#endif

    QColor titleBarMix(const QColor &from, const QColor &to, float amount) {
        return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * amount,
                                from.greenF() + (to.greenF() - from.greenF()) * amount,
                                from.blueF() + (to.blueF() - from.blueF()) * amount);
    }

    QColor titleBarAlpha(QColor color, float alpha) {
        color.setAlphaF(alpha);
        return color;
    }

    // Text and glyphs of an inactive window, as native title bars dim them.
    QColor titleBarInactive(const QPalette &palette) {
        return titleBarMix(palette.color(QPalette::WindowText), palette.color(QPalette::Window), 0.45f);
    }
}

class WindowCaptionButton : public QAbstractButton {
public:
    enum Kind { Minimize, Maximize, Close };

    WindowCaptionButton(Kind kind, QWidget *parent) : QAbstractButton(parent), m_kind(kind) {
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
    }

    void setMaximized(bool maximized) {
        if (m_maximized == maximized) return;
        m_maximized = maximized;
        update();
    }

    void setNativeState(bool hovered, bool pressed) {
        if (m_nativeHovered == hovered && m_nativePressed == pressed) return;
        m_nativeHovered = hovered;
        m_nativePressed = pressed;
        update();
    }

    [[nodiscard]] bool nativePressed() const { return m_nativePressed; }

protected:
    bool event(QEvent *event) override {
        if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate) update();
        return QAbstractButton::event(event);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        const QPalette &palette = this->palette();
        const bool hovered = underMouse() || m_nativeHovered;
        const bool pressed = isDown() || m_nativePressed;
        QColor glyph = palette.color(QPalette::WindowText);
        if (m_kind == Close && (hovered || pressed)) {
            painter.fillRect(rect(), QColor(0xc4, 0x2b, 0x1c, pressed ? 0xe6 : 0xff));
            glyph = Qt::white;
        } else {
            if (hovered || pressed) painter.fillRect(rect(), titleBarAlpha(glyph, pressed ? 0.06f : 0.1f));
            if (!isActiveWindow() && !hovered) glyph = titleBarInactive(palette);
        }

        // Laid out in device pixels, so the strokes stay one crisp pixel wide at any scale.
        const qreal ratio = devicePixelRatioF();
        painter.scale(1 / ratio, 1 / ratio);
        const int size = qRound(10 * ratio);
        const int stroke = qMax(1, qRound(ratio));
        const int left = (qRound(width() * ratio) - size) / 2;
        const int top = (qRound(height() * ratio) - size) / 2;
        const qreal inset = stroke / 2.0;
        QPen pen(glyph, stroke);
        pen.setJoinStyle(Qt::MiterJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        switch (m_kind) {
        case Minimize: {
            const qreal y = top + size / 2 + inset;
            painter.drawLine(QPointF(left, y), QPointF(left + size, y));
            break;
        }
        case Maximize:
            if (m_maximized) {
                const int shift = qRound(2 * ratio);
                const qreal frontRight = left + size - shift - inset;
                painter.drawRect(QRectF(left + inset, top + shift + inset, size - shift - stroke, size - shift - stroke));
                QPainterPath back;
                back.moveTo(left + shift + inset, top + shift + inset);
                back.lineTo(left + shift + inset, top + inset);
                back.lineTo(left + size - inset, top + inset);
                back.lineTo(left + size - inset, top + size - shift - inset);
                back.lineTo(frontRight, top + size - shift - inset);
                painter.drawPath(back);
            } else {
                painter.drawRect(QRectF(left + inset, top + inset, size - stroke, size - stroke));
            }
            break;
        case Close:
            painter.setRenderHint(QPainter::Antialiasing);
            painter.drawLine(QPointF(left + inset, top + inset), QPointF(left + size - inset, top + size - inset));
            painter.drawLine(QPointF(left + size - inset, top + inset), QPointF(left + inset, top + size - inset));
            break;
        }
    }

private:
    Kind m_kind;
    bool m_maximized = false;
    bool m_nativeHovered = false;
    bool m_nativePressed = false;
};

bool WindowTitleBar::Supported() {
#if defined(Q_OS_WIN) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    return true;
#else
    return false;
#endif
}

WindowTitleBar *WindowTitleBar::Install(QMainWindow *window) {
    auto *titleBar = new WindowTitleBar(window);
    auto *host = new QWidget(window);
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(titleBar);
    layout->addWidget(window->takeCentralWidget(), 1);
    window->setCentralWidget(host);
    return titleBar;
}

void WindowTitleBar::ApplyNativeTitle(QWidget *window) {
#ifdef Q_OS_MACOS
    // Hidden rather than cleared, so the title still names the window in the Dock, Mission Control and VoiceOver.
    Mac_HideWindowTitle(window->winId());
#elif defined(Q_OS_UNIX)
    // A space: Qt shows the application name for an empty title.
    static const bool blank = titleBarShowsNoIcon();
    if (blank) window->setWindowTitle(QStringLiteral(" "));
#else
    Q_UNUSED(window)
#endif
}

WindowTitleBar::WindowTitleBar(QMainWindow *window)
    : QWidget(window),
      m_window(window),
      m_minimize(new WindowCaptionButton(WindowCaptionButton::Minimize, this)),
      m_maximize(new WindowCaptionButton(WindowCaptionButton::Maximize, this)),
      m_close(new WindowCaptionButton(WindowCaptionButton::Close, this)) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addStretch();
    layout->addWidget(m_minimize);
    layout->addWidget(m_maximize);
    layout->addWidget(m_close);
    m_minimize->setAccessibleName(tr("Minimize"));
    m_close->setAccessibleName(tr("Close"));
    connect(m_minimize, &QAbstractButton::clicked, window, &QWidget::showMinimized);
    connect(m_maximize, &QAbstractButton::clicked, this, [this] { ToggleMaximized(); });
    connect(m_close, &QAbstractButton::clicked, window, &QWidget::close);
    applyMetrics();
    syncWindowState();
    window->installEventFilter(this);
    hide();
}

void WindowTitleBar::setCustomFrame(bool enabled) {
    enabled = enabled && Supported();
    if (enabled == m_customFrame) return;
    m_customFrame = enabled;
    setVisible(enabled);
#ifdef Q_OS_WIN
    // A window on screen keeps its size, and its content takes or gives up the caption's room.
    WindowFrame::Refresh(m_window);
    if (enabled && !m_window->isVisible()) WindowFrame::DropNativeCaption(m_window);
#endif
}

bool WindowTitleBar::handleWindowMessage(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    // Native until Qt has created the window: it would take the caption's room as the size the window was given.
    if (m_customFrame && m_window->internalWinId() != 0 && eventType == "windows_generic_MSG") {
        return WindowFrame::HandleMessage(m_window, *this, message, result);
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return false;
}

QByteArray WindowTitleBar::saveWindowGeometry() const {
    QByteArray geometry = m_window->saveGeometry();
#ifdef Q_OS_WIN
    const QRect normal = m_customFrame && m_window->isMaximized() ? WindowFrame::NormalGeometry(m_window) : QRect();
    if (!normal.isValid()) return geometry;
    // Format 3.0: magic, major and minor version, frame geometry, then the normal geometry at byte 24.
    QDataStream reader(geometry);
    reader.setVersion(QDataStream::Qt_4_0);
    quint32 magic = 0;
    quint16 major = 0;
    reader >> magic >> major;
    if (magic != 0x1D9D0CB || major != 3) return geometry;
    QByteArray rect;
    QDataStream writer(&rect, QIODevice::WriteOnly);
    writer.setVersion(QDataStream::Qt_4_0);
    writer << normal;
    geometry.replace(24, rect.size(), rect);
#endif
    return geometry;
}

WindowFrame::Area WindowTitleBar::AreaAt(const QPoint &pos) const {
    using WindowFrame::Area;
    if (isHidden()) return Area::Client;
    const QPoint local = mapFrom(m_window, pos);
    if (!rect().contains(local)) return Area::Client;
    if (m_maximize->geometry().contains(local)) return Area::MaximizeButton;
    if (m_minimize->geometry().contains(local) || m_close->geometry().contains(local)) return Area::OtherButton;
    return Area::Caption;
}

void WindowTitleBar::SetMaximizeButtonState(bool hovered, bool pressed) {
    m_maximize->setNativeState(hovered, pressed);
}

bool WindowTitleBar::MaximizeButtonPressed() const {
    return m_maximize->nativePressed();
}

void WindowTitleBar::ToggleMaximized() {
    if (m_window->isMaximized()) m_window->showNormal();
    else m_window->showMaximized();
}

bool WindowTitleBar::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::FontChange:
        applyMetrics();
        break;
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
        update();
        break;
    default:
        break;
    }
    return QWidget::event(event);
}

bool WindowTitleBar::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_window) {
        if (event->type() == QEvent::WindowStateChange) syncWindowState();
#ifdef Q_OS_WIN
        if (event->type() == QEvent::WinIdChange && m_customFrame) {
            WindowFrame::Refresh(m_window);
            WindowFrame::DropNativeCaption(m_window);
        }
#endif
    }
    return QWidget::eventFilter(watched, event);
}

void WindowTitleBar::applyMetrics() {
    const int height = qMax(30, fontMetrics().height() + 14);
    setFixedHeight(height);
    for (auto *button : {m_minimize, m_maximize, m_close}) button->setFixedSize(46, height);
}

void WindowTitleBar::syncWindowState() {
    const bool maximized = m_window->isMaximized();
    m_maximize->setMaximized(maximized);
    m_maximize->setAccessibleName(maximized ? tr("Restore") : tr("Maximize"));
    update();
}

const QPixmap &WindowTitleBar::icon(int size) {
    const qreal ratio = devicePixelRatioF();
    const int pixels = qRound(size * ratio);
    if (m_icon.width() != pixels || !qFuzzyCompare(m_icon.devicePixelRatio(), ratio)) {
        m_icon = QPixmap::fromImage(QImage(QStringLiteral(":/Throne/Throne.png"))
                                        .scaled(pixels, pixels, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_icon.setDevicePixelRatio(ratio);
    }
    return m_icon;
}

void WindowTitleBar::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    const QPalette &palette = this->palette();
    const QColor text = palette.color(QPalette::WindowText);
    painter.fillRect(rect(), titleBarMix(palette.color(QPalette::Window), text, 0.05f));
#ifdef Q_OS_WIN
    if (WindowFrame::DrawsOwnTopBorder() && !m_window->isMaximized()) {
        painter.fillRect(QRect(0, 0, width(), 1), titleBarAlpha(text, isActiveWindow() ? 0.35f : 0.2f));
    }
#endif

    QFont font = this->font();
    font.setWeight(QFont::DemiBold);
    const QFontMetrics metrics(font);
    const QString name = QStringLiteral("Throne");
    const int iconSize = qRound(metrics.height() * 1.15);
    constexpr int gap = 6;
    const int contentWidth = iconSize + gap + metrics.horizontalAdvance(name);
    // Centred on the whole bar, but never under the buttons.
    const int x = qMax(gap, qMin((width() - contentWidth) / 2, m_minimize->x() - gap - contentWidth));
    painter.drawPixmap(QPoint(x, (height() - iconSize) / 2), icon(iconSize));
    painter.setFont(font);
    painter.setPen(isActiveWindow() ? text : titleBarInactive(palette));
    painter.drawText(QRect(x + iconSize + gap, 0, width() - x - iconSize - gap, height()), Qt::AlignLeft | Qt::AlignVCenter, name);
}
