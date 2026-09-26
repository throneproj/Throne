#include "include/ui/widget/SubscriptionInfoCard.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QTimer>
#include <QPalette>
#include <algorithm>

#include "include/database/entities/Group.h"
#include "include/database/GroupsRepo.h"
#include "include/configs/sub/GroupUpdater.hpp"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/global/GuiUtils.hpp"
#include "include/global/Utils.hpp"
#include "include/global/Configs.hpp"

namespace
{
    QPixmap renderVectorPixmap(const QString &kind, const QColor &color, int size = 12)
    {
        QPixmap pix(size, size);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(color, 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        if (kind == "globe")
        {
            p.drawEllipse(QRectF(1.0, 1.0, 10.0, 10.0));
            p.drawLine(QPointF(1.2, 6.0), QPointF(10.8, 6.0));
            p.drawEllipse(QRectF(3.6, 1.0, 4.8, 10.0));
        }
        else if (kind == "chat")
        {
            QPainterPath path;
            path.addRoundedRect(QRectF(1.0, 1.2, 10.0, 7.2), 1.6, 1.6);
            path.moveTo(3.0, 8.4);
            path.lineTo(1.8, 10.8);
            path.lineTo(5.4, 8.4);
            p.drawPath(path);
            p.drawLine(QPointF(3.2, 3.8), QPointF(8.8, 3.8));
            p.drawLine(QPointF(3.2, 6.0), QPointF(7.2, 6.0));
        }
        else if (kind == "hourglass")
        {
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
        }
        else if (kind == "warn")
        {
            QPolygonF tri;
            tri << QPointF(6.0, 0.5) << QPointF(11.5, 10.5) << QPointF(0.5, 10.5);
            p.drawPolygon(tri);
            p.drawLine(QPointF(6.0, 3.8), QPointF(6.0, 6.8));
            p.drawPoint(QPointF(6.0, 8.8));
        }
        else if (kind == "info")
        {
            p.drawEllipse(QRectF(1.0, 1.0, 10.0, 10.0));
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawEllipse(QRectF(5.2, 2.8, 1.6, 1.6));
            p.drawRoundedRect(QRectF(5.3, 5.2, 1.4, 3.4), 0.5, 0.5);
        }
        else if (kind == "sync")
        {
            p.drawArc(QRectF(1.5, 1.5, 9.0, 9.0), 30 * 16, 270 * 16);
            p.drawLine(QPointF(8.0, 2.0), QPointF(10.5, 4.0));
            p.drawLine(QPointF(8.0, 6.0), QPointF(10.5, 4.0));
        }

        p.end();
        return pix;
    }

    bool openSafeUrl(const QString &rawUrl, bool allowTg = false)
    {
        const QUrl url(rawUrl.trimmed());
        if (!url.isValid())
            return false;
        const QString scheme = url.scheme().toLower();
        if (scheme == "http" || scheme == "https" || (allowTg && scheme == "tg"))
        {
            return QDesktopServices::openUrl(url);
        }
        return false;
    }
}

SubscriptionInfoCard::SubscriptionInfoCard(QWidget *parent)
    : QFrame(parent)
{
    setupUi();
    applyTheme();

    connect(themeManager(), &ThemeManager::themeChanged, this, [this](const QString &)
            {
        applyTheme();
        updateData(); });

    connect(Subscription::updater(), &Subscription::GroupUpdater::asyncUpdateCallback, this, [this](int gid)
            {
        if (m_group && (gid < 0 || m_group->id == gid)) {
            auto freshGroup = Configs::dataManager->groupsRepo->GetGroup(m_group->id);
            if (freshGroup != nullptr) {
                setGroup(freshGroup);
            }
        } });

    auto *countdownTimer = new QTimer(this);
    countdownTimer->setInterval(60000);
    connect(countdownTimer, &QTimer::timeout, this, [this]
            {
        if (isVisible() && hasSubscription()) updateData(); });
    countdownTimer->start();
}

void SubscriptionInfoCard::setupUi()
{
    setObjectName(QStringLiteral("SubscriptionInfoCard"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(28);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(10, 2, 10, 2);
    row->setSpacing(8);
    row->setAlignment(Qt::AlignVCenter);

    const int btnHeight = std::max(20, fontMetrics().height() + 4);

    auto createButton = [this, btnHeight](const QString &text, const QString &tooltip) -> QPushButton *
    {
        auto *btn = new QPushButton(this);
        btn->setText(text.trimmed());
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFlat(true);
        btn->setFixedHeight(btnHeight);
        return btn;
    };

    auto createDivider = [this]() -> QFrame *
    {
        auto *sep = new QFrame(this);
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Plain);
        sep->setFixedHeight(std::max(12, fontMetrics().height() - 2));
        return sep;
    };

    m_titleLabel = new QLabel(this);
    m_titleLabel->setTextFormat(Qt::PlainText);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setMaximumWidth(fontMetrics().averageCharWidth() * 20);
    row->addWidget(m_titleLabel);

    m_sepQuota = createDivider();
    row->addWidget(m_sepQuota);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(std::max(10, fontMetrics().height() - 4));
    m_progressBar->setTextVisible(false);
    row->addWidget(m_progressBar);

    m_quotaLabel = new QLabel(this);
    m_quotaLabel->setTextFormat(Qt::PlainText);
    m_quotaLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    row->addWidget(m_quotaLabel);

    m_sepExpiry = createDivider();
    row->addWidget(m_sepExpiry);

    m_expiryBadge = new QFrame(this);
    m_expiryBadge->setObjectName(QStringLiteral("subExpiryBadge"));
    m_expiryBadge->setFixedHeight(20);
    auto *layExp = new QHBoxLayout(m_expiryBadge);
    layExp->setContentsMargins(6, 0, 6, 0);
    layExp->setSpacing(4);
    layExp->setAlignment(Qt::AlignVCenter);

    m_expiryIcon = new QLabel(m_expiryBadge);
    m_expiryIcon->setFixedSize(12, 12);
    m_expiryLabel = new QLabel(m_expiryBadge);
    m_expiryLabel->setTextFormat(Qt::PlainText);

    layExp->addWidget(m_expiryIcon);
    layExp->addWidget(m_expiryLabel);
    row->addWidget(m_expiryBadge);

    m_sepInterval = createDivider();
    row->addWidget(m_sepInterval);

    m_intervalBadge = new QFrame(this);
    m_intervalBadge->setObjectName(QStringLiteral("subIntervalBadge"));
    m_intervalBadge->setFixedHeight(20);
    auto *layInt = new QHBoxLayout(m_intervalBadge);
    layInt->setContentsMargins(6, 0, 6, 0);
    layInt->setSpacing(4);
    layInt->setAlignment(Qt::AlignVCenter);

    m_intervalIcon = new QLabel(m_intervalBadge);
    m_intervalIcon->setFixedSize(12, 12);
    m_intervalLabel = new QLabel(m_intervalBadge);
    m_intervalLabel->setTextFormat(Qt::PlainText);

    layInt->addWidget(m_intervalIcon);
    layInt->addWidget(m_intervalLabel);
    row->addWidget(m_intervalBadge);

    m_sepAnnounce = createDivider();
    row->addWidget(m_sepAnnounce);

    m_announceBadge = new QFrame(this);
    m_announceBadge->setObjectName(QStringLiteral("subAnnounceBadge"));
    m_announceBadge->setFixedHeight(20);
    m_announceBadge->setCursor(Qt::PointingHandCursor);
    m_announceBadge->installEventFilter(this);

    auto *layAnn = new QHBoxLayout(m_announceBadge);
    layAnn->setContentsMargins(6, 0, 6, 0);
    layAnn->setSpacing(5);
    layAnn->setAlignment(Qt::AlignVCenter);

    m_announceIcon = new QLabel(m_announceBadge);
    m_announceIcon->setFixedSize(12, 12);
    m_announceIcon->setAlignment(Qt::AlignCenter);

    m_announceLabel = new QLabel(m_announceBadge);
    m_announceLabel->setTextFormat(Qt::PlainText);

    layAnn->addWidget(m_announceIcon);
    layAnn->addWidget(m_announceLabel);
    row->addWidget(m_announceBadge);

    row->addStretch(1);

    m_sepActions = createDivider();
    row->addWidget(m_sepActions);

    m_btnPortal = createButton(tr("Portal"), tr("Website / Portal"));
    connect(m_btnPortal, &QPushButton::clicked, this, [this]
            {
        if (m_group) openSafeUrl(m_group->GetSubUserInfo().web_url); });
    m_btnPortal->hide();
    row->addWidget(m_btnPortal);

    m_btnSupport = createButton(tr("Support"), tr("Technical Support"));
    connect(m_btnSupport, &QPushButton::clicked, this, [this]
            {
        if (m_group) openSafeUrl(m_group->GetSubUserInfo().support_url, true); });
    m_btnSupport->hide();
    row->addWidget(m_btnSupport);
}

QSize SubscriptionInfoCard::sizeHint() const
{
    return {QFrame::sizeHint().width(), 28};
}

QSize SubscriptionInfoCard::minimumSizeHint() const
{
    return {0, 28};
}

bool SubscriptionInfoCard::hasSubscription() const
{
    return m_group != nullptr && !m_group->url.isEmpty();
}

void SubscriptionInfoCard::setGroup(const std::shared_ptr<Configs::Group> &group)
{
    m_group = group;
    updateData();
}

void SubscriptionInfoCard::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    if (m_progressBar && m_group && m_group->GetSubUserInfo().has_quota)
    {
        const int minBarWidth = fontMetrics().averageCharWidth() * 18;
        const int maxBarWidth = fontMetrics().averageCharWidth() * 38;
        const int dynamicBarWidth = std::clamp(width() > 0 ? (width() / 7) : minBarWidth, minBarWidth, maxBarWidth);
        m_progressBar->setFixedWidth(dynamicBarWidth);
    }
    updateAnnouncementLayout();
}

void SubscriptionInfoCard::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::FontChange)
    {
        updateData();
    }
}

bool SubscriptionInfoCard::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_announceBadge && event->type() == QEvent::MouseButtonRelease)
    {
        if (m_group)
        {
            auto sub = m_group->GetSubUserInfo();
            if (!sub.announce.isEmpty())
            {
                QString title = !sub.title.isEmpty() ? sub.title : m_group->name;
                MessageBoxScrollable(tr("Announcement - %1").arg(title), sub.announce);
                return true;
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}

void SubscriptionInfoCard::updateAnnouncementLayout()
{
    const auto sub = m_group ? m_group->GetSubUserInfo() : Configs::SubUserInfo{};
    const bool isManual = m_group && (m_group->sub_update_interval > 0);
    const int displayInterval = isManual ? m_group->sub_update_interval : sub.server_interval;
    const bool hasInterval = (displayInterval > 0);
    const bool hasQuota = sub.has_quota;
    const bool hasExpiry = (sub.expire > 0);
    const bool hasWeb = !sub.web_url.isEmpty() && (sub.web_url.startsWith("http://", Qt::CaseInsensitive) || sub.web_url.startsWith("https://", Qt::CaseInsensitive));
    const bool hasSupport = !sub.support_url.isEmpty() && (sub.support_url.startsWith("http://", Qt::CaseInsensitive) || sub.support_url.startsWith("https://", Qt::CaseInsensitive) || sub.support_url.startsWith("tg://", Qt::CaseInsensitive));
    const bool hasActions = hasWeb || hasSupport;
    const int btnHeight = std::max(20, fontMetrics().height() + 4);

    if (hasWeb)
    {
        m_btnPortal->setText(QString());
        m_btnPortal->setFixedSize(btnHeight, btnHeight);
    }
    if (hasSupport)
    {
        m_btnSupport->setText(QString());
        m_btnSupport->setFixedSize(btnHeight, btnHeight);
    }

    auto calculateBaseWidth = [&]() -> int
    {
        int w = layout()->contentsMargins().left() + layout()->contentsMargins().right();
        w += m_titleLabel->sizeHint().width();
        if (hasQuota)
        {
            w += m_sepQuota->sizeHint().width() + layout()->spacing();
            w += m_progressBar->width() + layout()->spacing();
            w += m_quotaLabel->sizeHint().width() + layout()->spacing();
        }
        if (hasExpiry && hasQuota)
        {
            w += m_sepExpiry->sizeHint().width() + layout()->spacing();
            w += m_expiryBadge->sizeHint().width() + layout()->spacing();
        }
        if (hasActions)
        {
            w += m_sepActions->sizeHint().width() + layout()->spacing();
            if (hasWeb)
                w += m_btnPortal->width() + layout()->spacing();
            if (hasSupport)
                w += m_btnSupport->width() + layout()->spacing();
        }
        return w;
    };

    int baseWidth = calculateBaseWidth();
    const QFontMetrics fm(m_announceLabel->font());

    const int intervalNeed = hasInterval ? (m_intervalBadge->sizeHint().width() + m_sepInterval->sizeHint().width() + 2 * layout()->spacing()) : 0;
    const bool canShowInterval = hasInterval && (width() - baseWidth > intervalNeed + fm.averageCharWidth() * 10);

    m_intervalBadge->setVisible(canShowInterval);
    m_sepInterval->setVisible(canShowInterval && (hasQuota || hasExpiry));

    if (canShowInterval)
    {
        baseWidth += intervalNeed;
    }

    if (m_fullAnnounce.isEmpty())
    {
        m_announceBadge->hide();
        m_sepAnnounce->hide();

        if (width() - baseWidth > fm.averageCharWidth() * 24)
        {
            if (hasWeb)
            {
                m_btnPortal->setText(tr("Portal"));
                m_btnPortal->setMinimumSize(0, btnHeight);
                m_btnPortal->setMaximumSize(QWIDGETSIZE_MAX, btnHeight);
            }
            if (hasSupport)
            {
                m_btnSupport->setText(tr("Support"));
                m_btnSupport->setMinimumSize(0, btnHeight);
                m_btnSupport->setMaximumSize(QWIDGETSIZE_MAX, btnHeight);
            }
        }
        return;
    }

    auto *layAnn = m_announceBadge->layout();
    const int badgeOverhead = layAnn->contentsMargins().left() + layAnn->contentsMargins().right() + layAnn->spacing() + m_announceIcon->sizeHint().width() + 8;
    const int sepWidth = m_sepAnnounce->sizeHint().width() + layout()->spacing();
    int availForAnnounce = width() - baseWidth - sepWidth - layout()->spacing();

    if (availForAnnounce < badgeOverhead + fm.averageCharWidth() * 4)
    {
        m_announceBadge->hide();
        m_sepAnnounce->hide();
        return;
    }

    m_announceBadge->show();
    m_sepAnnounce->setVisible(hasQuota || hasExpiry || canShowInterval);

    const int naturalTextWidth = fm.horizontalAdvance(m_fullAnnounce);
    const int naturalPillWidth = naturalTextWidth + badgeOverhead;

    const int surplus = availForAnnounce - naturalPillWidth;
    if (surplus > fm.averageCharWidth() * 24)
    {
        if (hasWeb)
        {
            m_btnPortal->setText(tr("Portal"));
            m_btnPortal->setMinimumSize(0, btnHeight);
            m_btnPortal->setMaximumSize(QWIDGETSIZE_MAX, btnHeight);
        }
        if (hasSupport)
        {
            m_btnSupport->setText(tr("Support"));
            m_btnSupport->setMinimumSize(0, btnHeight);
            m_btnSupport->setMaximumSize(QWIDGETSIZE_MAX, btnHeight);
        }
        baseWidth = calculateBaseWidth();
        if (canShowInterval)
            baseWidth += intervalNeed;
        availForAnnounce = width() - baseWidth - sepWidth - layout()->spacing();
    }

    const int pillWidth = std::min(naturalPillWidth, availForAnnounce);
    const int textAvail = std::max(0, pillWidth - badgeOverhead);

    m_announceLabel->setText(fm.elidedText(m_fullAnnounce, Qt::ElideRight, textAvail));
    m_announceBadge->setMaximumWidth(pillWidth);
}

void SubscriptionInfoCard::updateData()
{
    if (!hasSubscription())
    {
        hide();
        emit cardVisibilityChanged();
        return;
    }

    auto sub = m_group->GetSubUserInfo();
    const auto &tk = themeManager()->tokens;
    const QPalette pal = qApp->palette();
    const bool isDark = pal.color(QPalette::Window).lightness() <= 128;
    const QColor textPrimary = pal.color(QPalette::WindowText);
    const QColor textSecondary = pal.color(QPalette::PlaceholderText).isValid()
                                     ? pal.color(QPalette::PlaceholderText)
                                     : (isDark ? QColor(156, 163, 175) : QColor(75, 85, 99));
    const QColor borderColor = pal.color(QPalette::Mid);
    const QColor trackColor = pal.color(QPalette::AlternateBase);

    QString title = !sub.title.isEmpty() ? sub.title : m_group->name;
    QFontMetrics titleFm(m_titleLabel->font());
    m_titleLabel->setText(titleFm.elidedText(title, Qt::ElideRight, m_titleLabel->maximumWidth()));

    QString tooltip = title.toHtmlEscaped();
    int effectiveInterval = m_group->sub_update_interval > 0 ? m_group->sub_update_interval : sub.server_interval;
    if (effectiveInterval > 0)
    {
        tooltip += QStringLiteral("\n") + tr("Auto-update: every %1 hours").arg(effectiveInterval);
    }
    if (m_group->sub_last_update > 0)
    {
        tooltip += QStringLiteral("\n") + tr("Last updated: %1").arg(DisplayTime(m_group->sub_last_update, QLocale::ShortFormat));
    }
    m_titleLabel->setToolTip(tooltip);

    const bool hasQuota = sub.has_quota;
    m_sepQuota->setVisible(hasQuota);
    m_progressBar->setVisible(hasQuota);
    m_quotaLabel->setVisible(hasQuota);

    if (hasQuota)
    {
        QString usedStr = ReadableSize(sub.used());
        QString totalStr = (sub.total > 0) ? ReadableSize(sub.total) : QString::fromUtf8("∞");
        double pct = sub.percentUsed();

        QColor barColor;
        if (sub.isExpired())
        {
            barColor = tk.danger;
        }
        else if (sub.total <= 0)
        {
            barColor = isDark ? QColor(0x16, 0xA3, 0x4A) : QColor(0x15, 0x80, 0x3D);
        }
        else
        {
            const double hue = std::clamp(120.0 * (1.0 - (pct / 100.0)), 0.0, 120.0);
            barColor = QColor::fromHsv(static_cast<int>(hue), 190, isDark ? 200 : 175);
        }

        const int minBarWidth = fontMetrics().averageCharWidth() * 18;
        const int maxBarWidth = fontMetrics().averageCharWidth() * 38;
        const int dynamicBarWidth = std::clamp(width() > 0 ? (width() / 7) : minBarWidth, minBarWidth, maxBarWidth);
        m_progressBar->setFixedWidth(dynamicBarWidth);
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(sub.total > 0 ? static_cast<int>(pct) : 100);

        if (sub.total > 0)
        {
            m_quotaLabel->setText(QStringLiteral("%1 / %2 (%3%)").arg(usedStr, totalStr, QString::number(static_cast<int>(pct))));
        }
        else
        {
            m_quotaLabel->setText(QStringLiteral("%1 / ∞").arg(usedStr));
        }

        m_quotaLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textSecondary.name()));

        const int radius = std::max(2, (fontMetrics().height() - 4) / 4);
        m_progressBar->setStyleSheet(QStringLiteral(
                                         "QProgressBar {"
                                         "  border: 1px solid %1;"
                                         "  border-radius: %2px;"
                                         "  background-color: %3;"
                                         "}"
                                         "QProgressBar::chunk {"
                                         "  background-color: %4;"
                                         "  border-radius: %5px;"
                                         "}")
                                         .arg(borderColor.name(), QString::number(radius), trackColor.name(), barColor.name(), QString::number(std::max(1, radius - 1))));
    }

    const bool hasExpiry = (sub.expire > 0);
    m_sepExpiry->setVisible(hasExpiry && hasQuota);

    if (hasExpiry)
    {
        qint64 now = QDateTime::currentSecsSinceEpoch();
        qint64 diffSecs = sub.expire - now;
        qint64 diffDays = diffSecs / 86400;

        QString expText;
        QColor badgeColor;

        if (sub.isExpired())
        {
            m_expiryIcon->setPixmap(renderVectorPixmap("warn", tk.danger, 12));
            expText = tr("Expired");
            badgeColor = tk.danger;
        }
        else if (diffSecs < 3600)
        {
            qint64 minutes = std::max<qint64>(1, diffSecs / 60);
            m_expiryIcon->setPixmap(renderVectorPixmap("warn", tk.danger, 12));
            expText = tr("%1m left").arg(minutes);
            badgeColor = tk.danger;
        }
        else if (diffSecs < 86400)
        {
            qint64 hours = std::max<qint64>(1, diffSecs / 3600);
            m_expiryIcon->setPixmap(renderVectorPixmap("warn", tk.danger, 12));
            expText = tr("%1h left").arg(hours);
            badgeColor = tk.danger;
        }
        else if (diffDays <= 3)
        {
            m_expiryIcon->setPixmap(renderVectorPixmap("warn", tk.danger, 12));
            expText = tr("%1d left").arg(diffDays);
            badgeColor = tk.danger;
        }
        else
        {
            m_expiryIcon->setPixmap(renderVectorPixmap("hourglass", textSecondary, 12));
            expText = tr("%1d left").arg(diffDays);
            badgeColor = textSecondary;
        }

        m_expiryLabel->setText(expText);
        m_expiryLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(badgeColor.name()));
        m_expiryBadge->setToolTip(tr("Expires: %1").arg(DisplayTime(sub.expire, QLocale::ShortFormat)));
        m_expiryBadge->show();
    }
    else
    {
        m_expiryBadge->hide();
    }

    const bool isManual = (m_group->sub_update_interval > 0);
    const int displayInterval = isManual ? m_group->sub_update_interval : sub.server_interval;
    const bool hasInterval = (displayInterval > 0);

    if (hasInterval)
    {
        if (isManual)
        {
            m_intervalLabel->setText(tr("Custom: %1h").arg(displayInterval));
            QString tip = tr("Manual update override: every %1 hours").arg(displayInterval);
            if (sub.server_interval > 0)
            {
                tip += QStringLiteral("\n") + tr("Server hint: %1h").arg(sub.server_interval);
            }
            m_intervalBadge->setToolTip(tip);
        }
        else
        {
            m_intervalLabel->setText(tr("Server: %1h").arg(displayInterval));
            m_intervalBadge->setToolTip(tr("Automatic update from server: every %1 hours").arg(displayInterval));
        }

        m_intervalIcon->setPixmap(renderVectorPixmap("sync", textSecondary, 12));
        m_intervalLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textSecondary.name()));
    }
    else
    {
        m_intervalBadge->hide();
        m_sepInterval->hide();
    }

    const bool hasWeb = !sub.web_url.isEmpty() && (sub.web_url.startsWith("http://", Qt::CaseInsensitive) || sub.web_url.startsWith("https://", Qt::CaseInsensitive));
    const bool hasSupport = !sub.support_url.isEmpty() && (sub.support_url.startsWith("http://", Qt::CaseInsensitive) || sub.support_url.startsWith("https://", Qt::CaseInsensitive) || sub.support_url.startsWith("tg://", Qt::CaseInsensitive));

    m_btnPortal->setVisible(hasWeb);
    if (hasWeb)
        m_btnPortal->setToolTip(tr("Website / Portal: %1").arg(sub.web_url.toHtmlEscaped()));

    m_btnSupport->setVisible(hasSupport);
    if (hasSupport)
        m_btnSupport->setToolTip(tr("Technical Support: %1").arg(sub.support_url.toHtmlEscaped()));

    m_sepActions->setVisible(hasWeb || hasSupport);

    const QString cleanAnnounce = sub.announce.trimmed();
    const bool hasAnnounce = !cleanAnnounce.isEmpty() && cleanAnnounce.compare("base64:", Qt::CaseInsensitive) != 0;

    if (hasAnnounce)
    {
        m_fullAnnounce = cleanAnnounce;
        m_announceIcon->setPixmap(renderVectorPixmap("info", tk.accent, 12));
        m_announceBadge->setToolTip(cleanAnnounce.toHtmlEscaped());
    }
    else
    {
        m_fullAnnounce.clear();
        m_announceBadge->hide();
        m_sepAnnounce->hide();
    }

    updateAnnouncementLayout();

    show();
    emit cardVisibilityChanged();
}

void SubscriptionInfoCard::applyTheme()
{
    const auto &tk = themeManager()->tokens;
    const QPalette pal = qApp->palette();
    const bool isDark = pal.color(QPalette::Window).lightness() <= 128;
    const QColor cardBg = pal.color(QPalette::Window);
    const QColor textPrimary = pal.color(QPalette::WindowText);
    const QColor chipBg = pal.color(QPalette::Button);
    const QColor border = pal.color(QPalette::Mid);
    const QString badgeBorder = isDark ? QStringLiteral("rgba(255, 255, 255, 0.18)") : QStringLiteral("rgba(0, 0, 0, 0.15)");

    setStyleSheet(QStringLiteral(
                      "QFrame#SubscriptionInfoCard {"
                      "  background-color: %1;"
                      "  border-bottom: 1px solid %2;"
                      "}"
                      "QLabel {"
                      "  color: %3;"
                      "}"
                      "QFrame[frameShape=\"5\"] {"
                      "  color: %2;"
                      "  background-color: %2;"
                      "  max-width: 1px;"
                      "}"
                      "QFrame#subExpiryBadge, QFrame#subAnnounceBadge, QFrame#subIntervalBadge {"
                      "  background-color: %4;"
                      "  border: 1px solid %5;"
                      "  border-radius: 4px;"
                      "}"
                      "QFrame#subAnnounceBadge:hover {"
                      "  border-color: %6;"
                      "}"
                      "QPushButton {"
                      "  background-color: %4;"
                      "  border: 1px solid %5;"
                      "  border-radius: 4px;"
                      "  padding: 2px 6px;"
                      "  color: %3;"
                      "}"
                      "QPushButton:hover {"
                      "  border-color: %6;"
                      "}")
                      .arg(cardBg.name(), border.name(), textPrimary.name(), chipBg.name(), badgeBorder, tk.accent.name()));

    m_announceIcon->setPixmap(renderVectorPixmap("info", tk.accent, 12));
    m_btnPortal->setIcon(QIcon(renderVectorPixmap("globe", tk.onSurface, 12)));
    m_btnSupport->setIcon(QIcon(renderVectorPixmap("chat", tk.onSurface, 12)));
}