#include "include/ui/widget/StartStopButton.hpp"

#include <QConicalGradient>
#include <QContextMenuEvent>
#include <QEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QStyle>
#include <QStyleOptionToolButton>

#include "include/sys/KillSwitch.hpp"

StartStopButton::StartStopButton(QWidget *parent) : QToolButton(parent) {
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_ringColor = idleRingColor();

    m_morphAnim = new QPropertyAnimation(this, "morph", this);
    m_dimAnim = new QPropertyAnimation(this, "dim", this);
    m_pressAnim = new QPropertyAnimation(this, "press", this);
    m_ringColorAnim = new QPropertyAnimation(this, "ringColor", this);

    m_spinAnim = new QPropertyAnimation(this, "spin", this);
    m_spinAnim->setStartValue(0.0);
    m_spinAnim->setEndValue(360.0);
    m_spinAnim->setDuration(900);
    m_spinAnim->setLoopCount(-1);
    m_spinAnim->setEasingCurve(QEasingCurve::Linear);

    connect(this, &QAbstractButton::pressed, this, [this] { animate(m_pressAnim, 1.0, 110); });
    connect(this, &QAbstractButton::released, this, [this] { animate(m_pressAnim, 0.0, 160); });

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
    if (m_state == State::Running) animate(m_ringColorAnim, targetRingColor(), 320);
}

void StartStopButton::setLock(Lock l) {
    if (l == m_lock) return;
    m_lock = l;
    updateToolTip();
    update();
}

void StartStopButton::updateToolTip() {
    QString tip;
    switch (m_state) {
        case State::Disabled: tip = tr("Select a profile to start"); break;
        case State::Idle: tip = tr("Start"); break;
        case State::Connecting: tip = tr("Connecting…"); break;
        case State::Running: tip = tr("Stop"); break;
        case State::Disconnecting: tip = tr("Stopping…"); break;
    }
    switch (m_lock) {
        case Lock::Hidden: break;
        case Lock::Pending: tip += "\n" + tr("Kill switch: starting…"); break;
        case Lock::Blocking: tip += "\n" + tr("Kill switch: Tun is not running, so most traffic is blocked"); break;
        case Lock::Passing: tip += "\n" + tr("Kill switch: on — traffic only flows through Throne's Tun"); break;
        case Lock::Fault:
            tip += "\n" + (Sys::KillSwitch::instance()->failedForIdentity() ? tr("Kill switch is blocking traffic") : tr("Kill switch is not active"));
            break;
    }
    setToolTip(tip);
}

void StartStopButton::applyState(bool animated) {
    const bool interactive = (m_state == State::Idle || m_state == State::Running);
    setEnabled(interactive);
    setCursor(interactive ? Qt::PointingHandCursor : Qt::ArrowCursor);

    updateToolTip();

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


void StartStopButton::updateLoops() {
    const bool spinning = m_state == State::Connecting || m_state == State::Disconnecting;
    setLoopRunning(m_spinAnim, m_shown && spinning);
}

void StartStopButton::showEvent(QShowEvent *e) {
    QToolButton::showEvent(e);
    m_shown = true;
    updateLoops();
}

void StartStopButton::hideEvent(QHideEvent *e) {
    m_shown = false;
    setLoopRunning(m_spinAnim, false);
    QToolButton::hideEvent(e);
}

bool StartStopButton::event(QEvent *e) {
    // QWidget::event drops a disabled widget's context menu before consulting the policy, and the button is disabled when nothing can start.
    if (e->type() == QEvent::ContextMenu && !isEnabled() && contextMenuPolicy() == Qt::CustomContextMenu) {
        emit customContextMenuRequested(static_cast<QContextMenuEvent *>(e)->pos());
        return true;
    }
    // The failure kind behind a Fault can change while the lock stays Fault.
    if (e->type() == QEvent::ToolTip && m_lock == Lock::Fault) updateToolTip();
    return QToolButton::event(e);
}

void StartStopButton::changeEvent(QEvent *e) {
    switch (e->type()) {
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
        case QEvent::ThemeChange:
            // The cached chrome was rendered through the old style/palette.
            m_chromeCache = QPixmap();
            m_lockCache = QPixmap();
            break;
        default:
            break;
    }
    QToolButton::changeEvent(e);
}

QColor StartStopButton::modeColor(Mode m) const {
    switch (m) {
        case Mode::Core: return {0x2E, 0xA0, 0x51};          // green
        case Mode::SystemProxy: return {0x37, 0x9B, 0xFF};   // blue
        case Mode::Tun: return {0x9C, 0x1A, 0x1A};           // crimson red
        case Mode::Off:
        default: return idleRingColor();
    }
}

QColor StartStopButton::idleRingColor() const {
    QColor c = palette().color(QPalette::WindowText);
    c.setAlphaF(0.12);
    return c;
}

QColor StartStopButton::glyphColor() const {
    if (m_state == State::Running || m_state == State::Disconnecting) return {0x99, 0x46, 0x46}; // dim, slightly darker red
    return Qt::darkGreen;
}

QColor StartStopButton::targetRingColor() const {
    switch (m_state) {
        case State::Connecting:
        case State::Disconnecting: return {0xFF, 0xB3, 0x2C}; // amber "working"
        case State::Running: return modeColor(m_mode);
        default: return idleRingColor();
    }
}

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
        // Force the frame to look enabled: a transition is not clickable but must not look dead.
        opt.state |= QStyle::State_Enabled;
        opt.state &= ~QStyle::State_Sunken;
    }

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

    ensureChromeCache();
    p.drawPixmap(0, 0, m_chromeCache);

    const QRectF cr = contentsRect();
    const qreal D = qMin(cr.width(), cr.height());
    const QPointF c = cr.center();

    const qreal scale = 1.0 - 0.06 * m_press;
    p.translate(c);
    p.scale(scale, scale);
    p.translate(-c);

    const qreal penW = qMax(1.6, D * 0.063);
    const qreal R = D * 0.34;
    const QRectF rr(c.x() - R, c.y() - R, 2 * R, 2 * R);

    p.setOpacity(m_dim);

    if (m_state == State::Connecting || m_state == State::Disconnecting) {
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
        constexpr qreal kSteadyGlow = 0.5;

        const qreal glowR = R + penW * 2.4;
        const qreal ringStop = R / glowR;
        QColor gPeak = m_ringColor;
        gPeak.setAlphaF(0.20 + 0.40 * kSteadyGlow);
        QColor gEdge = m_ringColor;
        gEdge.setAlphaF(0.0);
        // Transparent up to the ring stop, so the halo spreads outward only.
        QRadialGradient g(c, glowR);
        g.setColorAt(0.0, gEdge);
        g.setColorAt(ringStop * 0.9, gEdge);
        g.setColorAt(ringStop, gPeak);
        g.setColorAt(1.0, gEdge);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(c, glowR, glowR);

        p.setBrush(Qt::NoBrush);
        QColor base = m_ringColor.lighter(static_cast<int>(101 + 9 * kSteadyGlow));
        base.setAlphaF(0.95);
        QConicalGradient cg(c, 90.0);
        cg.setColorAt(0.0, base.lighter(116));
        cg.setColorAt(0.5, base);
        cg.setColorAt(1.0, base.lighter(116));
        QPen ringPen(QBrush(cg), penW);
        ringPen.setCapStyle(Qt::RoundCap);
        p.setPen(ringPen);
        p.drawEllipse(c, R, R);
    } else {
        p.setBrush(Qt::NoBrush);
        QPen ringPen(m_ringColor, penW);
        ringPen.setCapStyle(Qt::RoundCap);
        p.setPen(ringPen);
        p.drawEllipse(c, R, R);
    }

    const qreal h = D * 0.136;
    const qreal t = m_morph;
    auto lerp = [](const QPointF &a, const QPointF &b, qreal k) { return a + (b - a) * k; };
    // Offset to the triangle's centroid; a bbox-centred triangle reads as too far left.
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

    // The glyph's rounded corners come from the round-joined pen, not from the path.
    QLinearGradient lg(c.x(), c.y() - h, c.x(), c.y() + h);
    const QColor g1 = glyphColor();
    lg.setColorAt(0.0, g1.lighter(118));
    lg.setColorAt(1.0, g1.darker(112));
    const qreal corner = D * 0.04;
    QPen gpen(QBrush(lg), corner);
    gpen.setJoinStyle(Qt::RoundJoin);
    gpen.setCapStyle(Qt::RoundCap);
    const qreal glyphAlpha = (m_state == State::Connecting || m_state == State::Disconnecting) ? 0.5 : 1.0;
    p.setOpacity(m_dim * glyphAlpha);
    p.setPen(gpen);
    p.setBrush(QBrush(lg));
    p.drawPath(path);

    if (m_lock != Lock::Hidden) {
        // Outside the press scale and m_dim: the lock must stay legible on a disabled button.
        p.resetTransform();
        paintLock(p, cr);
    }
}

QRectF StartStopButton::lockBox(const QRectF &area) const {
    const qreal D = qMin(area.width(), area.height());
    const qreal L = D * 0.31;
    const qreal margin = D * 0.01;
    return {area.right() - margin - L, area.bottom() - margin - L, L, L};
}

QColor StartStopButton::lockColor() const {
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    switch (m_lock) {
        case Lock::Pending: {
            QColor neutral = palette().color(QPalette::WindowText);
            neutral.setAlpha(255);
            return neutral;
        }
        case Lock::Blocking: return dark ? QColor(0xFF, 0xC1, 0x3B) : QColor(0xC7, 0x84, 0x00);
        case Lock::Fault: return dark ? QColor(0xFF, 0x5C, 0x5C) : QColor(0xD3, 0x2F, 0x2F);
        default: return dark ? QColor(0x4A, 0xD6, 0x7E) : QColor(0x1E, 0x8E, 0x45);
    }
}

qreal StartStopButton::lockOpacity() const {
    switch (m_lock) {
        case Lock::Pending: return 0.55;
        case Lock::Passing: return 0.75;
        default: return 1.0;
    }
}

void StartStopButton::paintLock(QPainter &p, const QRectF &area) {
    const QRectF box = lockBox(area);
    const qreal L = box.width();
    const qreal pad = L * 0.34;

    const QColor window = palette().color(QPalette::Window);
    const bool dark = window.lightness() < 128;
    const QColor fill = lockColor();

    const qreal dpr = devicePixelRatioF();
    if (m_lockCache.isNull() || m_lockKeyLock != m_lock || !qFuzzyCompare(m_lockKeySize, L) || !qFuzzyCompare(m_lockKeyDpr, dpr)) {
        const qreal side = L + 2 * pad;
        QPixmap pm(QSizeF(side * dpr, side * dpr).toSize());
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter lp(&pm);
        lp.setRenderHint(QPainter::Antialiasing, true);
        lp.translate(pad, pad);

        const qreal sw = L * 0.14;
        const QRectF body(L * 0.11, L * 0.45, L * 0.78, L * 0.55);
        const qreal r = L * 0.215;
        const qreal cx = L / 2;
        QPainterPath shackle;
        shackle.moveTo(cx - r, body.top() + sw / 2);
        shackle.lineTo(cx - r, sw / 2 + r);
        shackle.arcTo(QRectF(cx - r, sw / 2, 2 * r, 2 * r), 180, -180);
        shackle.lineTo(cx + r, body.top() + sw / 2);
        QPainterPathStroker stroker;
        stroker.setWidth(sw);
        stroker.setCapStyle(Qt::FlatCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        QPainterPath bodyPath;
        bodyPath.addRoundedRect(body, L * 0.12, L * 0.12);
        const QPainterPath outer = stroker.createStroke(shackle).united(bodyPath);

        QPainterPath mark;
        mark.setFillRule(Qt::WindingFill);
        if (m_lock == Lock::Blocking) {
            const qreal w = L * 0.09;
            mark.addRoundedRect(QRectF(cx - w / 2, body.top() + body.height() * 0.16, w, body.height() * 0.44), w / 2, w / 2);
            mark.addEllipse(QPointF(cx, body.top() + body.height() * 0.78), w * 0.62, w * 0.62);
        } else {
            const QPointF hole(cx, body.top() + body.height() * 0.40);
            mark.addEllipse(hole, L * 0.08, L * 0.08);
            mark.addRect(QRectF(cx - L * 0.035, hole.y(), L * 0.07, body.height() * 0.32));
        }

        // A cut-out in the window colour keeps the lock apart from the ring and its glow underneath.
        QColor cutout = window;
        cutout.setAlpha(255);
        lp.setPen(QPen(cutout, qMax(1.0, L * 0.08) * 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        lp.setBrush(cutout);
        lp.drawPath(outer);

        lp.setPen(Qt::NoPen);
        lp.setBrush(fill);
        lp.drawPath(outer.subtracted(mark));

        if (m_lock == Lock::Fault) {
            lp.setPen(QPen(dark ? QColor(255, 255, 255, 220) : QColor(0x5A, 0x00, 0x00), qMax(1.0, L * 0.06),
                           Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            lp.setBrush(Qt::NoBrush);
            lp.drawPath(outer);
        }
        lp.end();

        m_lockCache = pm;
        m_lockKeyLock = m_lock;
        m_lockKeySize = L;
        m_lockKeyDpr = dpr;
    }

    p.setOpacity(lockOpacity());
    p.drawPixmap(box.topLeft() - QPointF(pad, pad), m_lockCache);
}
