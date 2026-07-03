#include "include/ui/widget/StartStopButton.hpp"

#include <QConicalGradient>
#include <QEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QTimer>
#include <QtMath>

namespace {
constexpr int kGlowPeriodMs = 3400;  // one full breath
constexpr int kGlowIntervalMs = 33;  // ~30fps; the slow breath needs no more
}  // namespace

StartStopButton::StartStopButton(QWidget *parent) : QToolButton(parent) {
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_ringColor = idleRingColor();

    m_morphAnim = new QPropertyAnimation(this, "morph", this);
    m_dimAnim = new QPropertyAnimation(this, "dim", this);
    m_pressAnim = new QPropertyAnimation(this, "press", this);
    m_ringColorAnim = new QPropertyAnimation(this, "ringColor", this);

    // Looping animations: the connecting spinner and the running glow.
    m_spinAnim = new QPropertyAnimation(this, "spin", this);
    m_spinAnim->setStartValue(0.0);
    m_spinAnim->setEndValue(360.0);
    m_spinAnim->setDuration(900);
    m_spinAnim->setLoopCount(-1);
    m_spinAnim->setEasingCurve(QEasingCurve::Linear);

    // glow is a 0..1 phase advanced at a constant rate; paint maps it through a
    // raised cosine, so the breath rises and falls at equal rates and dwells
    // equally at bright and dim (InOutSine here lingered dim and rushed the peak).
    // A 3.4s breath needs nowhere near the display refresh rate, so we drive it
    // from a ~30fps timer instead of a vsync-locked QPropertyAnimation; the phase
    // is read from an elapsed clock so the cadence is immune to timer jitter.
    m_glowTimer = new QTimer(this);
    m_glowTimer->setInterval(kGlowIntervalMs);
    m_glowTimer->setTimerType(Qt::CoarseTimer);
    connect(m_glowTimer, &QTimer::timeout, this,
            [this] { setGlow((m_glowClock.elapsed() % kGlowPeriodMs) / qreal(kGlowPeriodMs)); });

    connect(this, &QAbstractButton::pressed, this, [this] { animate(m_pressAnim, 1.0, 110); });
    connect(this, &QAbstractButton::released, this, [this] { animate(m_pressAnim, 0.0, 160); });

    // Establish the initial visuals without an entry animation.
    m_state = State::Disabled;
    applyState(false);
}

QSize StartStopButton::sizeHint() const {
    return {56, 56};
}

void StartStopButton::setState(State s) {
    if (s == m_state) return;
    m_state = s;
    applyState(true);
}

void StartStopButton::setMode(Mode m) {
    if (m == m_mode) return;
    m_mode = m;
    // The mode colour only shows while running; animate the ring if it's live.
    if (m_state == State::Running) animate(m_ringColorAnim, targetRingColor(), 320);
}

void StartStopButton::applyState(bool animated) {
    // Idle and Running are clickable; Connecting (no cancel) and Disabled are not.
    const bool interactive = (m_state == State::Idle || m_state == State::Running);
    setEnabled(interactive);
    setCursor(interactive ? Qt::PointingHandCursor : Qt::ArrowCursor);

    switch (m_state) {
        case State::Disabled: setToolTip(tr("Select a profile to start")); break;
        case State::Idle: setToolTip(tr("Start")); break;
        case State::Connecting: setToolTip(tr("Connecting…")); break;
        case State::Running: setToolTip(tr("Stop")); break;
        case State::Disconnecting: setToolTip(tr("Stopping…")); break;
    }

    // Keep the stop square while disconnecting; it morphs to the play triangle
    // only once the profile has actually stopped (mirrors Connecting -> Running).
    const qreal morphTarget = (m_state == State::Running || m_state == State::Disconnecting) ? 1.0 : 0.0;
    const qreal dimTarget = (m_state == State::Disabled) ? 0.45 : 1.0;
    const QColor ringTarget = targetRingColor();

    if (animated) {
        animate(m_morphAnim, morphTarget, 300);
        animate(m_dimAnim, dimTarget, 220);
        animate(m_ringColorAnim, ringTarget, 320);
    } else {
        m_morphAnim->stop();
        m_dimAnim->stop();
        m_ringColorAnim->stop();
        m_morph = morphTarget;
        m_dim = dimTarget;
        m_ringColor = ringTarget;
    }

    updateLoops();
    update();
}

void StartStopButton::animate(QPropertyAnimation *anim, const QVariant &to, int duration) {
    anim->stop();
    anim->setDuration(duration);
    anim->setEasingCurve(QEasingCurve::InOutCubic);
    anim->setStartValue(property(anim->propertyName().constData()));
    anim->setEndValue(to);
    anim->start();
}

void StartStopButton::setLoopRunning(QPropertyAnimation *anim, bool running) {
    if (running) {
        if (anim->state() != QAbstractAnimation::Running) anim->start();
        return;
    }
    anim->stop();
    if (anim == m_spinAnim) m_spin = 0.0;
    update();
}

void StartStopButton::setGlowRunning(bool running) {
    if (running) {
        if (!m_glowTimer->isActive()) {
            m_glowClock.restart();
            setGlow(0.0);
            m_glowTimer->start();
        }
        return;
    }
    m_glowTimer->stop();
    setGlow(0.0);
}

// Start/stop the looping animations for the current state, but only while the
// button is on screen; hideEvent/showEvent toggle m_shown so a minimised or
// hidden-to-tray window stops driving repaints for nobody.
void StartStopButton::updateLoops() {
    const bool spinning = m_state == State::Connecting || m_state == State::Disconnecting;
    setLoopRunning(m_spinAnim, m_shown && spinning);
    setGlowRunning(m_shown && m_state == State::Running);
}

void StartStopButton::showEvent(QShowEvent *e) {
    QToolButton::showEvent(e);
    m_shown = true;
    updateLoops();
}

void StartStopButton::hideEvent(QHideEvent *e) {
    m_shown = false;
    setLoopRunning(m_spinAnim, false);
    setGlowRunning(false);
    QToolButton::hideEvent(e);
}

void StartStopButton::changeEvent(QEvent *e) {
    switch (e->type()) {
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
        case QEvent::ThemeChange:
            // The cached chrome depends on the active style/palette; drop it so
            // the next paint re-renders it with the new look.
            m_chromeCache = QPixmap();
            break;
        default:
            break;
    }
    QToolButton::changeEvent(e);
}

// --- colours -------------------------------------------------------------

QColor StartStopButton::modeColor(Mode m) const {
    switch (m) {
        case Mode::Core: return {0x2E, 0xA0, 0x51};          // green
        case Mode::SystemProxy: return {0x37, 0x9B, 0xFF};   // blue
        case Mode::Tun: return {0x9C, 0x1A, 0x1A};           // crimson red
        case Mode::Dns: return {0xC8, 0x96, 0x00};           // dark gold
        case Mode::SystemProxyDns: return {0x7A, 0x82, 0xFF}; // indigo
        case Mode::Off:
        default: return idleRingColor();
    }
}

QColor StartStopButton::idleRingColor() const {
    // Almost invisible: while nothing is running the glyph carries the look and
    // the ring recedes to a faint hint.
    QColor c = palette().color(QPalette::WindowText);
    c.setAlphaF(0.12);
    return c;
}

QColor StartStopButton::glyphColor() const {
    // Semantic, muted glyph colours: a gray-green "go" triangle while idle, a
    // red "stop" square once a profile is running.
    if (m_state == State::Running || m_state == State::Disconnecting) return {0x99, 0x46, 0x46}; // dim, slightly darker red
    return Qt::darkGreen;                                // dim gray-green
}

QColor StartStopButton::targetRingColor() const {
    switch (m_state) {
        case State::Connecting:
        case State::Disconnecting: return {0xFF, 0xB3, 0x2C}; // amber "working"
        case State::Running: return modeColor(m_mode);
        default: return idleRingColor();
    }
}

// --- painting ------------------------------------------------------------

void StartStopButton::ensureChromeCache() {
    if (size().isEmpty()) {
        m_chromeCache = QPixmap();
        return;
    }

    QStyleOptionToolButton opt;
    initStyleOption(&opt);
    opt.text.clear();
    opt.icon = QIcon();
    opt.iconSize = QSize();
    opt.features &= ~QStyleOptionToolButton::HasMenu;
    opt.subControls &= ~QStyle::SC_ToolButtonMenu;
    opt.arrowType = Qt::NoArrow;
    if (m_state == State::Connecting || m_state == State::Disconnecting) {
        // Keep the frame looking active during a transition, even though clicks
        // are disabled (there is no cancel).
        opt.state |= QStyle::State_Enabled;
        opt.state &= ~QStyle::State_Sunken;
    }

    // Only the size, DPR, and style state (enabled/hover/sunken/…) affect the
    // chrome; when they are unchanged the previous render is reused verbatim.
    const qreal dpr = devicePixelRatioF();
    const uint stateKey = static_cast<uint>(opt.state);
    const uint subKey = static_cast<uint>(opt.activeSubControls);
    if (!m_chromeCache.isNull() && m_chromeKeySize == size() && qFuzzyCompare(m_chromeKeyDpr, dpr) &&
        m_chromeKeyState == stateKey && m_chromeKeySub == subKey) {
        return;
    }

    QPixmap pm(size() * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter pp(&pm);
    pp.setRenderHint(QPainter::Antialiasing, true);
    style()->drawComplexControl(QStyle::CC_ToolButton, &opt, &pp, this);
    pp.end();

    m_chromeCache = pm;
    m_chromeKeySize = size();
    m_chromeKeyDpr = dpr;
    m_chromeKeyState = stateKey;
    m_chromeKeySub = subKey;
}

void StartStopButton::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 1. Standard tool-button chrome, so it matches the sibling toolbar buttons.
    //    It never animates, so it is rendered through QStyle once and the cached
    //    pixmap is blitted here; the looping running/connecting repaints would
    //    otherwise re-run the full (possibly stylesheet-backed) style per frame.
    ensureChromeCache();
    p.drawPixmap(0, 0, m_chromeCache);

    // 2. Custom indicator, centred in the content area.
    const QRectF cr = contentsRect();
    const qreal D = qMin(cr.width(), cr.height());
    const QPointF c = cr.center();

    // Press feedback: scale only the indicator (the frame stays put).
    const qreal scale = 1.0 - 0.06 * m_press;
    p.translate(c);
    p.scale(scale, scale);
    p.translate(-c);

    const qreal penW = qMax(1.6, D * 0.063);
    const qreal R = D * 0.34;
    const QRectF rr(c.x() - R, c.y() - R, 2 * R, 2 * R);

    p.setOpacity(m_dim);

    if (m_state == State::Connecting || m_state == State::Disconnecting) {
        // Faint full track with a bright round-capped arc sweeping over it.
        p.setBrush(Qt::NoBrush);
        QColor track = m_ringColor;
        track.setAlphaF(0.20);
        QPen trackPen(track, penW);
        p.setPen(trackPen);
        p.drawEllipse(c, R, R);

        QPen arcPen(m_ringColor, penW);
        arcPen.setCapStyle(Qt::RoundCap);
        p.setPen(arcPen);
        const int startAngle = static_cast<int>(-m_spin * 16); // Qt: 1/16 deg
        const int spanAngle = -110 * 16;                       // sweep clockwise
        p.drawArc(rr, startAngle, spanAngle);
    } else if (m_state == State::Running) {
        // The whole ring "breathes": a soft interior wash + a gentle outer bloom
        // rise and fall with the glow phase. Kept subtle so it reads as a calm
        // breath rather than a hard pulsing light.
        // Linear phase -> raised cosine: smooth, symmetric, equal dwell/rise/fall.
        const qreal pulse = 0.5 - 0.5 * qCos(m_glow * 2.0 * M_PI);

        // Soft glow that peaks at the ring and fades to nothing outward, so the
        // halo melts into the background instead of being a flat band of colour.
        const qreal glowR = R + penW * 2.4;
        const qreal ringStop = R / glowR;
        QColor gPeak = m_ringColor;
        gPeak.setAlphaF(0.20 + 0.40 * pulse);
        QColor gEdge = m_ringColor;
        gEdge.setAlphaF(0.0);
        // Outward only: the interior stays clear (the inner stops are transparent,
        // the rise hidden under the ring stroke); the halo peaks at the ring and
        // fades to nothing beyond it.
        QRadialGradient g(c, glowR);
        g.setColorAt(0.0, gEdge);
        g.setColorAt(ringStop * 0.9, gEdge);
        g.setColorAt(ringStop, gPeak);
        g.setColorAt(1.0, gEdge);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(c, glowR, glowR);

        // Crisp core ring on top, barely brightening at the peak.
        p.setBrush(Qt::NoBrush);
        QColor base = m_ringColor.lighter(static_cast<int>(101 + 9 * pulse));
        base.setAlphaF(0.95); // a touch dimmer than the full mode colour
        QConicalGradient cg(c, 90.0);
        cg.setColorAt(0.0, base.lighter(116));
        cg.setColorAt(0.5, base);
        cg.setColorAt(1.0, base.lighter(116));
        QPen ringPen(QBrush(cg), penW);
        ringPen.setCapStyle(Qt::RoundCap);
        p.setPen(ringPen);
        p.drawEllipse(c, R, R);
    } else {
        // Idle / Disabled: a faint hint of a ring (the glyph carries the look).
        p.setBrush(Qt::NoBrush);
        QPen ringPen(m_ringColor, penW);
        ringPen.setCapStyle(Qt::RoundCap);
        p.setPen(ringPen);
        p.drawEllipse(c, R, R);
    }

    // Glyph: play triangle (morph 0) <-> stop square (morph 1).
    const qreal h = D * 0.136;
    const qreal t = m_morph;
    auto lerp = [](const QPointF &a, const QPointF &b, qreal k) { return a + (b - a) * k; };
    // Shift the play triangle right so it balances on its centroid; a bbox-
    // centred triangle reads as sitting too far left. The offset interpolates
    // away as it morphs into the (already centred) stop square.
    const qreal triShift = h / 3.0;
    const QPointF tri[4] = {
        c + QPointF(-h + triShift, -h),
        c + QPointF(h + triShift, 0),
        c + QPointF(h + triShift, 0),
        c + QPointF(-h + triShift, h),
    };
    const QPointF sq[4] = {
        c + QPointF(-h, -h),
        c + QPointF(h, -h),
        c + QPointF(h, h),
        c + QPointF(-h, h),
    };
    QPainterPath path;
    path.moveTo(lerp(tri[0], sq[0], t));
    for (int i = 1; i < 4; ++i) path.lineTo(lerp(tri[i], sq[i], t));
    path.closeSubpath();

    // Glossy vertical gradient + rounded corners (via a round-joined pen).
    QLinearGradient lg(c.x(), c.y() - h, c.x(), c.y() + h);
    const QColor g1 = glyphColor();
    lg.setColorAt(0.0, g1.lighter(118));
    lg.setColorAt(1.0, g1.darker(112));
    const qreal corner = D * 0.04;
    QPen gpen(QBrush(lg), corner);
    gpen.setJoinStyle(Qt::RoundJoin);
    gpen.setCapStyle(Qt::RoundCap);
    // The glyph fades back during a transition so the spinner reads as the action.
    const qreal glyphAlpha = (m_state == State::Connecting || m_state == State::Disconnecting) ? 0.5 : 1.0;
    p.setOpacity(m_dim * glyphAlpha);
    p.setPen(gpen);
    p.setBrush(QBrush(lg));
    p.drawPath(path);
}
