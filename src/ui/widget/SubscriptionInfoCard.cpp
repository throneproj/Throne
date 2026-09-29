#include "include/ui/widget/SubscriptionInfoCard.hpp"

#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyle>
#include <QTextDocument>
#include <QTimer>
#include <algorithm>

#include "include/configs/sub/GroupUpdater.hpp"
#include "include/database/entities/Group.h"
#include "include/global/Utils.hpp"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/stats/dialog_endpoint_details.h"

namespace {
    enum class Glyph {
        Globe,
        Chat,
        Hourglass,
        Warn,
        Info,
        Sync,
    };

    // Drawn at 3x on a 12-unit grid; QLabel (scaled contents) and QIcon smooth-scale it to the screen.
    QPixmap renderGlyph(Glyph glyph, const QColor &color, int size) {
        constexpr int kScale = 3;
        QPixmap pix(QSize(size, size) * kScale);
        pix.setDevicePixelRatio(kScale);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.scale(size / 12.0, size / 12.0);
        const QPen pen(color, 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        switch (glyph) {
        case Glyph::Globe:
            p.drawEllipse(QRectF(1.0, 1.0, 10.0, 10.0));
            p.drawLine(QPointF(1.2, 6.0), QPointF(10.8, 6.0));
            p.drawEllipse(QRectF(3.6, 1.0, 4.8, 10.0));
            break;
        case Glyph::Chat: {
            QPainterPath path;
            path.addRoundedRect(QRectF(1.0, 1.2, 10.0, 7.2), 1.6, 1.6);
            path.moveTo(3.0, 8.4);
            path.lineTo(1.8, 10.8);
            path.lineTo(5.4, 8.4);
            p.drawPath(path);
            p.drawLine(QPointF(3.2, 3.8), QPointF(8.8, 3.8));
            p.drawLine(QPointF(3.2, 6.0), QPointF(7.2, 6.0));
            break;
        }
        case Glyph::Hourglass: {
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRoundedRect(QRectF(1.8, 0.8, 8.4, 1.2), 0.5, 0.5);
            p.drawRoundedRect(QRectF(1.8, 10.0, 8.4, 1.2), 0.5, 0.5);

            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            QPainterPath glass;
            glass.moveTo(2.8, 2.0);
            glass.lineTo(9.2, 2.0);
            glass.cubicTo(QPointF(8.8, 4.0), QPointF(7.2, 5.0), QPointF(7.0, 6.0));
            glass.cubicTo(QPointF(7.2, 7.0), QPointF(8.8, 8.0), QPointF(9.2, 10.0));
            glass.lineTo(2.8, 10.0);
            glass.cubicTo(QPointF(3.2, 8.0), QPointF(4.8, 7.0), QPointF(5.0, 6.0));
            glass.cubicTo(QPointF(4.8, 5.0), QPointF(3.2, 4.0), QPointF(2.8, 2.0));
            p.drawPath(glass);

            p.setPen(Qt::NoPen);
            p.setBrush(color);
            QPainterPath sand;
            sand.moveTo(3.8, 9.5);
            sand.lineTo(8.2, 9.5);
            sand.lineTo(7.2, 7.2);
            sand.lineTo(4.8, 7.2);
            sand.closeSubpath();
            p.drawPath(sand);

            p.setPen(pen);
            p.drawLine(QPointF(6.0, 5.2), QPointF(6.0, 7.2));
            break;
        }
        case Glyph::Warn: {
            QPolygonF tri;
            tri << QPointF(6.0, 0.5) << QPointF(11.5, 10.5) << QPointF(0.5, 10.5);
            p.drawPolygon(tri);
            p.drawLine(QPointF(6.0, 3.8), QPointF(6.0, 6.8));
            p.drawPoint(QPointF(6.0, 8.8));
            break;
        }
        case Glyph::Info:
            p.drawEllipse(QRectF(1.0, 1.0, 10.0, 10.0));
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawEllipse(QRectF(5.2, 2.8, 1.6, 1.6));
            p.drawRoundedRect(QRectF(5.3, 5.2, 1.4, 3.4), 0.5, 0.5);
            break;
        case Glyph::Sync:
            p.drawArc(QRectF(1.5, 1.5, 9.0, 9.0), 30 * 16, 270 * 16);
            p.drawLine(QPointF(8.0, 2.0), QPointF(10.5, 4.0));
            p.drawLine(QPointF(8.0, 6.0), QPointF(10.5, 4.0));
            break;
        }

        p.end();
        return pix;
    }

    QUrl linkUrl(const QString &raw, bool allowTg) {
        const QUrl url(raw.trimmed());
        const QString scheme = url.scheme().toLower();
        const bool allowed = scheme == QLatin1String("http") || scheme == QLatin1String("https") || (allowTg && scheme == QLatin1String("tg"));
        return url.isValid() && allowed ? url : QUrl();
    }

    // QSS hex drops alpha, and windows11 ships translucent Mid/AlternateBase/WindowText.
    QColor opaqueOn(const QColor &color, const QColor &base) {
        const float a = color.alphaF();
        return QColor::fromRgbF(color.redF() * a + base.redF() * (1 - a),
                                color.greenF() * a + base.greenF() * (1 - a),
                                color.blueF() * a + base.blueF() * (1 - a));
    }

    // Mirrors ThemeManager::ApplyTheme, which installs its chip sheet only for the windows11 style.
    bool themeDrawsChips() {
        const auto *tm = themeManager();
        const bool system = tm->current_theme.compare(QLatin1String("system"), Qt::CaseInsensitive) == 0;
        return (system ? tm->system_style_name : tm->current_theme).compare(QLatin1String("windows11"), Qt::CaseInsensitive) == 0;
    }

    // Attribute selectors are only re-evaluated on polish.
    void setStyleProperty(QWidget *widget, const char *name, const QString &value) {
        if (widget->property(name).toString() == value)
            return;
        widget->setProperty(name, value);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
} // namespace

SubscriptionInfoCard::SubscriptionInfoCard(QWidget *parent)
    : QFrame(parent) {
    // A new child of a not-yet-visible parent is not isHidden(), and reportGeometry() goes by isHidden().
    hide();
    setupUi();
    applyTheme();

    connect(themeManager(), &ThemeManager::themeChanged, this, [this](const QString &) {
        applyTheme();
        updateData(); });

    connect(Subscription::updater(), &Subscription::GroupUpdater::asyncUpdateCallback, this, [this](int gid) {
        if (m_group && (gid < 0 || gid == m_group->id)) updateData(); });

    auto *countdownTimer = new QTimer(this);
    countdownTimer->setInterval(60000);
    connect(countdownTimer, &QTimer::timeout, this, [this] {
        if (isVisible() && hasSubscription()) updateData(); });
    countdownTimer->start();
}

void SubscriptionInfoCard::setupUi() {
    setObjectName(QStringLiteral("SubscriptionInfoCard"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(10, 2, 10, 2);
    row->setSpacing(8);

    auto createDivider = [this]() -> QFrame * {
        auto *sep = new QFrame(this);
        sep->setObjectName(QStringLiteral("subDivider"));
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Plain);
        return sep;
    };

    auto createBadge = [this](const QString &name, QLabel *&icon, QLabel *&text) -> QFrame * {
        auto *badge = new QFrame(this);
        badge->setObjectName(name);
        auto *lay = new QHBoxLayout(badge);
        lay->setContentsMargins(6, 0, 6, 0);
        lay->setSpacing(4);
        icon = new QLabel(badge);
        icon->setScaledContents(true);
        text = new QLabel(badge);
        text->setTextFormat(Qt::PlainText);
        lay->addWidget(icon);
        lay->addWidget(text);
        return badge;
    };

    auto createLink = [this](const QString &accessibleName) -> QPushButton * {
        auto *btn = new QPushButton(this);
        btn->setObjectName(QStringLiteral("subLink"));
        btn->setAccessibleName(accessibleName);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setFlat(true);
        btn->hide();
        return btn;
    };

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("subTitle"));
    m_titleLabel->setTextFormat(Qt::PlainText);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    row->addWidget(m_titleLabel);

    m_sepQuota = createDivider();
    row->addWidget(m_sepQuota);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("subQuotaBar"));
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    row->addWidget(m_progressBar);

    m_quotaLabel = new QLabel(this);
    m_quotaLabel->setTextFormat(Qt::PlainText);
    m_quotaLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_quotaLabel->setProperty("colorRole", QStringLiteral("muted"));
    row->addWidget(m_quotaLabel);

    m_sepExpiry = createDivider();
    row->addWidget(m_sepExpiry);

    m_expiryBadge = createBadge(QStringLiteral("subExpiryBadge"), m_expiryIcon, m_expiryLabel);
    m_expiryLabel->setObjectName(QStringLiteral("subExpiryText"));
    m_expiryLabel->setProperty("colorRole", QStringLiteral("muted"));
    row->addWidget(m_expiryBadge);

    m_sepInterval = createDivider();
    row->addWidget(m_sepInterval);

    m_intervalBadge = createBadge(QStringLiteral("subIntervalBadge"), m_intervalIcon, m_intervalLabel);
    m_intervalLabel->setProperty("colorRole", QStringLiteral("muted"));
    row->addWidget(m_intervalBadge);

    m_sepAnnounce = createDivider();
    row->addWidget(m_sepAnnounce);

    m_announceBadge = createBadge(QStringLiteral("subAnnounceBadge"), m_announceIcon, m_announceLabel);
    m_announceBadge->setCursor(Qt::PointingHandCursor);
    m_announceBadge->installEventFilter(this);
    row->addWidget(m_announceBadge);

    row->addStretch(1);

    m_sepActions = createDivider();
    row->addWidget(m_sepActions);

    m_btnPortal = createLink(tr("Portal"));
    connect(m_btnPortal, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(m_webUrl); });
    row->addWidget(m_btnPortal);

    m_btnSupport = createLink(tr("Support"));
    connect(m_btnSupport, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(m_supportUrl); });
    row->addWidget(m_btnSupport);

    applyMetrics();
}

void SubscriptionInfoCard::applyMetrics() {
    const QFontMetrics fm = fontMetrics();
    m_chipHeight = fm.height() + 4;
    m_iconSize = std::max(12, fm.height() * 3 / 4);

    for (QFrame *badge : {m_expiryBadge, m_intervalBadge, m_announceBadge})
        badge->setFixedHeight(m_chipHeight);
    for (QLabel *icon : {m_expiryIcon, m_intervalIcon, m_announceIcon})
        icon->setFixedSize(m_iconSize, m_iconSize);
    for (QFrame *sep : {m_sepQuota, m_sepExpiry, m_sepInterval, m_sepAnnounce, m_sepActions})
        sep->setFixedSize(1, fm.height() - 2);
    for (QPushButton *btn : {m_btnPortal, m_btnSupport})
        btn->setIconSize(QSize(m_iconSize, m_iconSize));
    setLinksCompact(true);

    m_progressBar->setFixedHeight(std::max(10, fm.height() - 4));
    m_titleLabel->setMaximumWidth(fm.averageCharWidth() * 20);
    setFixedHeight(m_chipHeight + 8);
    reportGeometry();
}

void SubscriptionInfoCard::setLinksCompact(bool compact) {
    const auto apply = [&](QPushButton *btn, const QString &text) {
        btn->setText(compact ? QString() : text);
        if (compact) {
            btn->setFixedSize(m_chipHeight, m_chipHeight);
        } else {
            btn->setMinimumSize(0, m_chipHeight);
            btn->setMaximumSize(QWIDGETSIZE_MAX, m_chipHeight);
        }
    };
    apply(m_btnPortal, tr("Portal"));
    apply(m_btnSupport, tr("Support"));
}

bool SubscriptionInfoCard::hasSubscription() const {
    return m_group != nullptr && !m_group->url.isEmpty() && m_group->sub_info.valid;
}

QString SubscriptionInfoCard::expiryText(qint64 expire) {
    if (expire <= 0)
        return {};
    const qint64 left = expire - QDateTime::currentSecsSinceEpoch();
    if (left < 0)
        return tr("Expired");
    if (left < 3600)
        return tr("%1m left").arg(std::max<qint64>(1, left / 60));
    if (left < 86400)
        return tr("%1h left").arg(left / 3600);
    return tr("%1d left").arg(left / 86400);
}

void SubscriptionInfoCard::setGroup(const std::shared_ptr<Configs::Group> &group) {
    m_group = group;
    updateData();
}

void SubscriptionInfoCard::reportGeometry() {
    const int reserved = isHidden() ? 0 : height();
    if (reserved == m_reportedHeight)
        return;
    m_reportedHeight = reserved;
    emit cardVisibilityChanged();
}

void SubscriptionInfoCard::resizeEvent(QResizeEvent *event) {
    QFrame::resizeEvent(event);
    updateAnnouncementLayout();
}

void SubscriptionInfoCard::changeEvent(QEvent *event) {
    QFrame::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        applyMetrics();
        applyTheme();
        updateData();
    }
}

bool SubscriptionInfoCard::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_announceBadge && event->type() == QEvent::MouseButtonRelease && m_group && !m_fullAnnounce.isEmpty()) {
        const auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton && m_announceBadge->rect().contains(me->position().toPoint())) {
            MessageBoxScrollable(tr("Announcement - %1").arg(m_group->name), m_group->sub_info.announce.trimmed());
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

int SubscriptionInfoCard::requiredWidth() const {
    const auto *row = layout();
    int total = row->contentsMargins().left() + row->contentsMargins().right();
    int items = 0;
    for (int i = 0; i < row->count(); ++i) {
        const QWidget *w = row->itemAt(i)->widget();
        if (w == nullptr || w->isHidden())
            continue;
        total += qBound(w->minimumWidth(), w->sizeHint().width(), w->maximumWidth());
        ++items;
    }
    return total + std::max(0, items - 1) * row->spacing();
}

void SubscriptionInfoCard::updateAnnouncementLayout() {
    if (!hasSubscription())
        return;

    const auto &sub = m_group->sub_info;
    const int avgChar = fontMetrics().averageCharWidth();

    m_titleLabel->ensurePolished();
    const QFontMetrics titleFm = m_titleLabel->fontMetrics();
    const auto elideTitle = [&](int maxWidth) { m_titleLabel->setText(titleFm.elidedText(m_group->name, Qt::ElideRight, maxWidth)); };
    elideTitle(m_titleLabel->maximumWidth());

    const int barWidth = std::clamp(width() / 7, avgChar * 18, avgChar * 38);
    m_progressBar->setFixedWidth(barWidth);

    for (QWidget *w : std::initializer_list<QWidget *>{m_sepQuota, m_progressBar, m_quotaLabel})
        w->setVisible(sub.has_quota);
    m_sepExpiry->setVisible(sub.expire > 0);
    m_expiryBadge->setVisible(sub.expire > 0);
    for (QWidget *w : {m_sepInterval, m_intervalBadge, m_sepAnnounce, m_announceBadge})
        w->hide();
    m_btnPortal->setVisible(!m_webUrl.isEmpty());
    m_btnSupport->setVisible(!m_supportUrl.isEmpty());
    m_sepActions->setVisible(!m_webUrl.isEmpty() || !m_supportUrl.isEmpty());
    setLinksCompact(true);

    // A squeezed QLabel clips instead of eliding, so shed width by priority until the row fits.
    int overflow = requiredWidth() - width();
    if (overflow > 0 && sub.has_quota) {
        m_progressBar->setFixedWidth(std::max(avgChar * 6, barWidth - overflow));
        overflow = requiredWidth() - width();
    }
    if (overflow > 0) {
        elideTitle(std::max(avgChar * 6, m_titleLabel->sizeHint().width() - overflow));
        overflow = requiredWidth() - width();
    }
    const QList<QList<QWidget *>> dropOrder{{m_progressBar}, {m_sepQuota, m_quotaLabel}, {m_sepExpiry, m_expiryBadge}, {m_sepActions, m_btnPortal, m_btnSupport}};
    for (const auto &widgets : dropOrder) {
        if (overflow <= 0)
            break;
        for (QWidget *w : widgets)
            w->hide();
        overflow = requiredWidth() - width();
    }
    if (overflow > 0)
        elideTitle(std::max(0, m_titleLabel->sizeHint().width() - overflow));

    if (m_hasInterval) {
        m_sepInterval->show();
        m_intervalBadge->show();
        if (requiredWidth() + avgChar * 10 > width()) {
            m_sepInterval->hide();
            m_intervalBadge->hide();
        }
    }

    if (!m_fullAnnounce.isEmpty()) {
        m_announceLabel->setText(m_fullAnnounce);
        m_sepAnnounce->show();
        m_announceBadge->show();
        overflow = requiredWidth() - width();
        if (overflow > 0) {
            m_announceLabel->ensurePolished();
            const QFontMetrics fm = m_announceLabel->fontMetrics();
            const int textWidth = m_announceLabel->sizeHint().width() - overflow;
            if (textWidth < avgChar * 4) {
                m_sepAnnounce->hide();
                m_announceBadge->hide();
            } else {
                m_announceLabel->setText(fm.elidedText(m_fullAnnounce, Qt::ElideRight, textWidth));
            }
        }
    }

    if ((!m_btnPortal->isHidden() || !m_btnSupport->isHidden()) && width() - requiredWidth() > avgChar * 24) {
        setLinksCompact(false);
        if (requiredWidth() > width())
            setLinksCompact(true);
    }
}

void SubscriptionInfoCard::updateData() {
    if (!hasSubscription()) {
        hide();
        reportGeometry();
        return;
    }

    const auto &sub = m_group->sub_info;
    const auto &tk = themeManager()->tokens;
    const auto plan = Subscription::ResolveAutoUpdate(*m_group);
    const QString autoUpdate = plan.interval > 0 ? tr("Auto-update: every %1").arg(Stats::HumanizeDuration(plan.interval)) : QString();

    QStringList titleTip{m_group->name};
    if (!sub.title.isEmpty() && sub.title != m_group->name)
        titleTip << tr("Provider: %1").arg(sub.title);
    if (!autoUpdate.isEmpty())
        titleTip << autoUpdate;
    if (m_group->sub_last_update > 0)
        titleTip << tr("Last updated: %1").arg(DisplayTime(m_group->sub_last_update, QLocale::ShortFormat));
    m_titleLabel->setToolTip(Qt::convertFromPlainText(titleTip.join(QLatin1Char('\n'))));

    if (sub.has_quota) {
        const int pct = static_cast<int>(sub.percentUsed());
        m_progressBar->setValue(sub.total > 0 ? pct : 100);
        setStyleProperty(m_progressBar, "level", sub.isExpired() || (sub.total > 0 && pct >= 90) ? QStringLiteral("danger") : QStringLiteral("ok"));
        const QString used = ReadableSize(sub.used());
        m_quotaLabel->setText(sub.total > 0 ? QStringLiteral("%1 / %2 (%3%)").arg(used, ReadableSize(sub.total), QString::number(pct))
                                            : QStringLiteral("%1 / ∞").arg(used));
    }

    if (sub.expire > 0) {
        const bool urgent = (sub.expire - QDateTime::currentSecsSinceEpoch()) / 86400 <= 3;
        m_expiryIcon->setPixmap(renderGlyph(urgent ? Glyph::Warn : Glyph::Hourglass, urgent ? tk.danger : tk.muted, m_iconSize));
        m_expiryLabel->setText(expiryText(sub.expire));
        setStyleProperty(m_expiryLabel, "colorRole", urgent ? QStringLiteral("danger") : QStringLiteral("muted"));
        m_expiryBadge->setToolTip(Qt::convertFromPlainText(tr("Expires: %1").arg(DisplayTime(sub.expire, QLocale::ShortFormat))));
    }

    m_hasInterval = plan.interval > 0;
    if (m_hasInterval) {
        QStringList tip{autoUpdate};
        switch (plan.source) {
        case Subscription::AutoUpdatePlan::Source::server:
            tip << tr("Interval provided by the server");
            break;
        case Subscription::AutoUpdatePlan::Source::group:
            tip << tr("Interval set for this group");
            break;
        default:
            tip << tr("Global interval from Settings");
            break;
        }
        if (plan.source != Subscription::AutoUpdatePlan::Source::server && sub.server_interval > 0)
            tip << tr("Server suggests every %n hour(s)", nullptr, sub.server_interval);
        m_intervalLabel->setText(tr("Every %1").arg(Stats::HumanizeDuration(plan.interval)));
        m_intervalBadge->setToolTip(Qt::convertFromPlainText(tip.join(QLatin1Char('\n'))));
        m_intervalIcon->setPixmap(renderGlyph(Glyph::Sync, tk.muted, m_iconSize));
    }

    m_webUrl = linkUrl(sub.web_url, false);
    m_supportUrl = linkUrl(sub.support_url, true);
    m_btnPortal->setToolTip(Qt::convertFromPlainText(tr("Website / Portal: %1").arg(sub.web_url.trimmed())));
    m_btnSupport->setToolTip(Qt::convertFromPlainText(tr("Technical Support: %1").arg(sub.support_url.trimmed())));

    const QString announce = sub.announce.trimmed();
    if (!announce.isEmpty() && announce.compare(QLatin1String("base64:"), Qt::CaseInsensitive) != 0) {
        m_fullAnnounce = announce.simplified();
        m_announceBadge->setToolTip(Qt::convertFromPlainText(announce, Qt::WhiteSpaceNormal));
    } else {
        m_fullAnnounce.clear();
    }

    show();
    reportGeometry();
    updateAnnouncementLayout();
}

void SubscriptionInfoCard::applyTheme() {
    const auto &tk = themeManager()->tokens;
    const QPalette pal = qApp->palette();
    QColor surface = pal.color(QPalette::Active, QPalette::Window);
    surface.setAlpha(255);
    const auto solid = [&](const QColor &color) { return opaqueOn(color, surface).name(); };
    const QString line = solid(pal.color(QPalette::Active, QPalette::Mid));
    QColor hover = tk.accent;
    hover.setAlphaF(0.15f);
    const int radius = std::max(2, m_progressBar->maximumHeight() / 4);

    QString sheet = QStringLiteral(
                        "QFrame#SubscriptionInfoCard { background-color: %1; border: none; border-bottom: 1px solid %2; border-radius: 0px; }"
                        "QLabel { background: transparent; padding: 0px; }"
                        "QLabel#subTitle, QLabel#subExpiryText { font-weight: bold; }"
                        "QFrame#subDivider { color: %2; background-color: %2; border: none; }"
                        "QProgressBar#subQuotaBar { border: 1px solid %2; border-radius: %3px; background-color: %4; }"
                        "QProgressBar#subQuotaBar::chunk { border-radius: %5px; background-color: %6; }"
                        "QProgressBar#subQuotaBar[level=\"danger\"]::chunk { background-color: %7; }"
                        "QFrame#subExpiryBadge, QFrame#subIntervalBadge, QFrame#subAnnounceBadge { background: transparent; border: none; }"
                        "QFrame#subAnnounceBadge:hover { background-color: %8; }"
                        "QPushButton#subLink { padding: 0px 6px; }")
                        .arg(surface.name(), line, QString::number(radius), solid(pal.color(QPalette::Active, QPalette::AlternateBase)),
                             QString::number(std::max(1, radius - 1)), tk.success.name(), tk.danger.name(), solid(hover));
    if (themeDrawsChips()) {
        sheet += QStringLiteral(
                     "QFrame#subExpiryBadge, QFrame#subIntervalBadge, QFrame#subAnnounceBadge, QPushButton#subLink {"
                     " background: transparent; border: 1px solid %1; border-radius: 4px; color: %2; }"
                     "QFrame#subAnnounceBadge:hover, QPushButton#subLink:hover { background-color: %3; }")
                     .arg(line, solid(pal.color(QPalette::Active, QPalette::WindowText)), solid(hover));
    }
    if (styleSheet() != sheet)
        setStyleSheet(sheet);

    m_announceIcon->setPixmap(renderGlyph(Glyph::Info, tk.accent, m_iconSize));
    m_btnPortal->setIcon(QIcon(renderGlyph(Glyph::Globe, tk.onSurface, m_iconSize)));
    m_btnSupport->setIcon(QIcon(renderGlyph(Glyph::Chat, tk.onSurface, m_iconSize)));
}
