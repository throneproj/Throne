#include "include/ui/widget/SubscriptionInfoCard.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QToolButton>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QTimer>
#include <QTableView>
#include <QHeaderView>
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

        p.end();
        return pix;
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

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(8);
    row->setAlignment(Qt::AlignVCenter);

    auto createButton = [this](const QString &tooltip) -> QToolButton *
    {
        auto *btn = new QToolButton(this);
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    // 1. Group Title (Locked to 105px to completely stop horizontal jumping)
    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(8);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setFixedWidth(105);
    m_titleLabel->setFixedHeight(18);
    row->addWidget(m_titleLabel);

    // 2. Hero Progress Bar (Stable 280px width)
    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(18);
    m_progressBar->setFixedWidth(280);
    m_progressBar->setTextVisible(true);
    m_progressBar->setAlignment(Qt::AlignCenter);
    row->addWidget(m_progressBar);

    // 3. Expiry Pill Badge (Locked 18px height)
    m_expiryBadge = new QFrame(this);
    m_expiryBadge->setObjectName(QStringLiteral("subExpiryBadge"));
    m_expiryBadge->setFixedHeight(18);
    auto *layExp = new QHBoxLayout(m_expiryBadge);
    layExp->setContentsMargins(6, 0, 6, 0);
    layExp->setSpacing(4);
    layExp->setAlignment(Qt::AlignVCenter);

    m_expiryIcon = new QLabel(m_expiryBadge);
    m_expiryIcon->setFixedSize(12, 12);
    m_expiryLabel = new QLabel(m_expiryBadge);
    QFont expFont = m_expiryLabel->font();
    expFont.setPointSize(8);
    m_expiryLabel->setFont(expFont);

    layExp->addWidget(m_expiryIcon);
    layExp->addWidget(m_expiryLabel);
    row->addWidget(m_expiryBadge);

    // 4. Clickable Announcement Pill Chip (Whole chip is clickable, no squished button inside)
    m_announceBadge = new QFrame(this);
    m_announceBadge->setObjectName(QStringLiteral("subAnnounceBadge"));
    m_announceBadge->setFixedHeight(18);
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
    QFont annFont = m_announceLabel->font();
    annFont.setPointSize(8);
    m_announceLabel->setFont(annFont);

    layAnn->addWidget(m_announceIcon);
    layAnn->addWidget(m_announceLabel);
    row->addWidget(m_announceBadge);

    // Spacer between data and right actions
    row->addStretch(1);

    // 5. Portal Button (Locked 18px height)
    m_btnPortal = createButton(tr("Website / Portal"));
    m_btnPortal->setFixedSize(22, 18);
    connect(m_btnPortal, &QToolButton::clicked, this, [this]
            {
        if (m_group) {
            auto sub = m_group->GetSubUserInfo();
            if (!sub.web_url.isEmpty()) QDesktopServices::openUrl(QUrl(sub.web_url));
        } });
    m_btnPortal->hide();
    row->addWidget(m_btnPortal);

    // 6. Support Button (Locked 18px height)
    m_btnSupport = createButton(tr("Technical Support"));
    m_btnSupport->setFixedSize(22, 18);
    connect(m_btnSupport, &QToolButton::clicked, this, [this]
            {
        if (m_group) {
            auto sub = m_group->GetSubUserInfo();
            if (!sub.support_url.isEmpty()) QDesktopServices::openUrl(QUrl(sub.support_url));
        } });
    m_btnSupport->hide();
    row->addWidget(m_btnSupport);
}

void SubscriptionInfoCard::setTableView(QTableView *table)
{
    m_tableView = table;
    if (!m_tableView)
        return;

    if (auto *vHeader = m_tableView->verticalHeader())
    {
        connect(vHeader, &QHeaderView::geometriesChanged, this, &SubscriptionInfoCard::syncTableOffset);
    }
    syncTableOffset();
}

void SubscriptionInfoCard::syncTableOffset()
{
    if (!m_tableView || !isVisible())
        return;

    int vhWidth = 0;
    if (auto *vHeader = m_tableView->verticalHeader())
    {
        if (vHeader->isVisible())
            vhWidth = vHeader->width();
    }
    layout()->setContentsMargins(vhWidth + 4, 4, 8, 4);
}

QSize SubscriptionInfoCard::sizeHint() const
{
    return {QFrame::sizeHint().width(), 26};
}

QSize SubscriptionInfoCard::minimumSizeHint() const
{
    return {0, 26};
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
    syncTableOffset();
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
    if (m_fullAnnounce.isEmpty())
    {
        m_announceBadge->hide();
        return;
    }

    m_announceBadge->show();

    // Dynamically calculate available width for the announcement chip
    int usedWidth = layout()->contentsMargins().left() + layout()->contentsMargins().right();
    usedWidth += m_titleLabel->width() + m_progressBar->width();
    if (m_expiryBadge->isVisible())
    {
        usedWidth += m_expiryBadge->sizeHint().width() + layout()->spacing();
    }
    if (m_btnPortal->isVisible())
    {
        usedWidth += m_btnPortal->width() + layout()->spacing();
    }
    if (m_btnSupport->isVisible())
    {
        usedWidth += m_btnSupport->width() + layout()->spacing();
    }
    usedWidth += 3 * layout()->spacing();

    const int avail = width() - usedWidth;
    if (avail <= 60)
    {
        m_announceBadge->hide();
        return;
    }

    // Cap the announcement pill up to 300px, but shrink it if the screen narrows
    const int maxPillWidth = qBound(80, avail, 300);
    const int textAvail = maxPillWidth - 12 - 12; // icon + padding

    QFontMetrics fm(m_announceLabel->font());
    m_announceLabel->setText(fm.elidedText(m_fullAnnounce, Qt::ElideRight, textAvail));
    m_announceBadge->setMaximumWidth(maxPillWidth);
}

void SubscriptionInfoCard::updateData()
{
    if (!hasSubscription())
    {
        hide();
        return;
    }

    auto sub = m_group->GetSubUserInfo();
    const auto &tk = themeManager()->tokens;
    const QColor winBg = qApp->palette().color(QPalette::Active, QPalette::Window);
    const bool isDark = (winBg.lightness() <= 128);

    const QString textPrimary = isDark ? QStringLiteral("#F3F4F6") : QStringLiteral("#111827");
    const QString textSecondary = isDark ? QStringLiteral("#9CA3AF") : QStringLiteral("#4B5563");
    const QString trackColor = isDark ? QStringLiteral("#1E2630") : QStringLiteral("#E2E8F0");
    const QString borderColor = isDark ? QStringLiteral("#3E4C5F") : QStringLiteral("#CBD5E1");

    // 1. Group Title (105px fixed width)
    QString title = !sub.title.isEmpty() ? sub.title : m_group->name;
    QFontMetrics titleFm(m_titleLabel->font());
    m_titleLabel->setText(titleFm.elidedText(title, Qt::ElideRight, m_titleLabel->width()));
    m_titleLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textPrimary));

    QString tooltip = title;
    if (m_group->sub_update_interval > 0)
    {
        tooltip += QStringLiteral("\n") + tr("Auto-update: every %1 hours").arg(m_group->sub_update_interval);
    }
    if (m_group->sub_last_update > 0)
    {
        tooltip += QStringLiteral("\n") + tr("Last updated: %1").arg(DisplayTime(m_group->sub_last_update, QLocale::ShortFormat));
    }
    m_titleLabel->setToolTip(tooltip);
    m_progressBar->setToolTip(tooltip);

    // 2. Hero Quota Progress Bar (280px fixed width)
    if (!sub.valid)
    {
        m_progressBar->hide();
    }
    else
    {
        m_progressBar->show();

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
            barColor = QColor::fromHsv(static_cast<int>(hue), 190, isDark ? 215 : 175);
        }

        if (sub.total > 0)
        {
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(static_cast<int>(pct));
            m_progressBar->setFormat(QStringLiteral("%1 / %2 (%3%)").arg(usedStr, totalStr, QString::number(static_cast<int>(pct))));
        }
        else
        {
            m_progressBar->setRange(0, 100);
            m_progressBar->setValue(100);
            m_progressBar->setFormat(QStringLiteral("%1 / ∞").arg(usedStr));
        }

        const QString textColor = (pct >= 45.0 || sub.total <= 0) ? QStringLiteral("#FFFFFF") : textPrimary;

        m_progressBar->setStyleSheet(QStringLiteral(
                                         "QProgressBar {"
                                         "  border: 1px solid %1;"
                                         "  border-radius: 3px;"
                                         "  text-align: center;"
                                         "  font-weight: bold;"
                                         "  font-size: 8pt;"
                                         "  line-height: 1;"
                                         "  color: %2;"
                                         "  background-color: %3;"
                                         "}"
                                         "QProgressBar::chunk {"
                                         "  background-color: %4;"
                                         "  border-radius: 2px;"
                                         "}")
                                         .arg(borderColor, textColor, trackColor, barColor.name()));
    }

    // 3. Expiry Badge
    if (sub.expire > 0)
    {
        qint64 now = QDateTime::currentSecsSinceEpoch();
        qint64 diffSecs = sub.expire - now;
        qint64 diffDays = diffSecs / 86400;

        QString expText;
        QColor badgeTextColor;
        QString badgeBg;
        QString badgeBorder;

        if (sub.isExpired())
        {
            m_expiryIcon->setPixmap(renderVectorPixmap("warn", tk.danger, 12));
            expText = tr("Expired");
            badgeTextColor = tk.danger;
            badgeBg = isDark ? QStringLiteral("rgba(239, 68, 68, 0.15)") : QStringLiteral("rgba(239, 68, 68, 0.10)");
            badgeBorder = isDark ? QStringLiteral("rgba(239, 68, 68, 0.4)") : QStringLiteral("rgba(239, 68, 68, 0.3)");
        }
        else if (diffSecs < 3600)
        {
            qint64 minutes = std::max<qint64>(1, diffSecs / 60);
            m_expiryIcon->setPixmap(renderVectorPixmap("warn", tk.danger, 12));
            expText = QStringLiteral("%1m left").arg(minutes);
            badgeTextColor = tk.danger;
            badgeBg = isDark ? QStringLiteral("rgba(239, 68, 68, 0.15)") : QStringLiteral("rgba(239, 68, 68, 0.10)");
            badgeBorder = isDark ? QStringLiteral("rgba(239, 68, 68, 0.4)") : QStringLiteral("rgba(239, 68, 68, 0.3)");
        }
        else if (diffSecs < 86400)
        {
            qint64 hours = std::max<qint64>(1, diffSecs / 3600);
            m_expiryIcon->setPixmap(renderVectorPixmap("warn", tk.danger, 12));
            expText = QStringLiteral("%1h left").arg(hours);
            badgeTextColor = tk.danger;
            badgeBg = isDark ? QStringLiteral("rgba(239, 68, 68, 0.15)") : QStringLiteral("rgba(239, 68, 68, 0.10)");
            badgeBorder = isDark ? QStringLiteral("rgba(239, 68, 68, 0.4)") : QStringLiteral("rgba(239, 68, 68, 0.3)");
        }
        else if (diffDays <= 3)
        {
            m_expiryIcon->setPixmap(renderVectorPixmap("warn", tk.danger, 12));
            expText = QStringLiteral("%1d left").arg(diffDays);
            badgeTextColor = tk.danger;
            badgeBg = isDark ? QStringLiteral("rgba(239, 68, 68, 0.12)") : QStringLiteral("rgba(239, 68, 68, 0.08)");
            badgeBorder = isDark ? QStringLiteral("rgba(239, 68, 68, 0.3)") : QStringLiteral("rgba(239, 68, 68, 0.2)");
        }
        else
        {
            m_expiryIcon->setPixmap(renderVectorPixmap("hourglass", QColor(textSecondary), 12));
            expText = QStringLiteral("%1d left").arg(diffDays);
            badgeTextColor = QColor(textSecondary);
            badgeBg = isDark ? QStringLiteral("rgba(255, 255, 255, 0.04)") : QStringLiteral("rgba(0, 0, 0, 0.03)");
            badgeBorder = borderColor;
        }

        m_expiryLabel->setText(expText);
        m_expiryLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold;").arg(badgeTextColor.name()));
        m_expiryBadge->setStyleSheet(QStringLiteral(
                                         "QFrame#subExpiryBadge {"
                                         "  background-color: %1;"
                                         "  border: 1px solid %2;"
                                         "  border-radius: 3px;"
                                         "}")
                                         .arg(badgeBg, badgeBorder));

        m_expiryBadge->setToolTip(tr("Expires: %1").arg(DisplayTime(sub.expire, QLocale::ShortFormat)));
        m_expiryBadge->show();
    }
    else
    {
        m_expiryBadge->hide();
    }

    // 4. Announcement Pill Badge
    const QString cleanAnnounce = sub.announce.trimmed();
    const bool hasAnnounce = !cleanAnnounce.isEmpty() && cleanAnnounce.compare("base64:", Qt::CaseInsensitive) != 0;

    if (hasAnnounce)
    {
        m_fullAnnounce = cleanAnnounce;
        m_announceBadge->setToolTip(cleanAnnounce);
        m_announceLabel->setStyleSheet(QStringLiteral("color: %1;").arg(textSecondary));
        updateAnnouncementLayout();
    }
    else
    {
        m_fullAnnounce.clear();
        m_announceBadge->hide();
    }

    // 5. Portal / Support Buttons
    const bool hasWeb = !sub.web_url.isEmpty() && (sub.web_url.startsWith("http://", Qt::CaseInsensitive) || sub.web_url.startsWith("https://", Qt::CaseInsensitive));
    const bool hasSupport = !sub.support_url.isEmpty() && (sub.support_url.startsWith("http://", Qt::CaseInsensitive) || sub.support_url.startsWith("https://", Qt::CaseInsensitive) || sub.support_url.startsWith("tg://", Qt::CaseInsensitive));

    m_btnPortal->setVisible(hasWeb);
    if (hasWeb)
        m_btnPortal->setToolTip(tr("Website / Portal: %1").arg(sub.web_url));

    m_btnSupport->setVisible(hasSupport);
    if (hasSupport)
        m_btnSupport->setToolTip(tr("Technical Support: %1").arg(sub.support_url));

    setFixedHeight(26);
    show();
    syncTableOffset();
}

void SubscriptionInfoCard::applyTheme()
{
    const auto &tk = themeManager()->tokens;
    const QColor winBg = qApp->palette().color(QPalette::Active, QPalette::Window);
    const bool isDark = (winBg.lightness() <= 128);

    const QString barBg = winBg.name();
    const QString chipBg = isDark ? QStringLiteral("#24303F") : QStringLiteral("#FFFFFF");
    const QString chipBorder = isDark ? QStringLiteral("#3E4C5F") : QStringLiteral("#CBD5E1");
    const QString hoverBg = isDark ? QStringLiteral("#2A374A") : QStringLiteral("#E2E8F0");

    setStyleSheet(QStringLiteral(
                      "QFrame#SubscriptionInfoCard {"
                      "  background-color: %1;"
                      "  border: none;"
                      "}"
                      "QFrame#subAnnounceBadge {"
                      "  background-color: %2;"
                      "  border: 1px solid %3;"
                      "  border-radius: 3px;"
                      "}"
                      "QFrame#subAnnounceBadge:hover {"
                      "  background-color: %4;"
                      "  border-color: %5;"
                      "}"
                      "QToolButton {"
                      "  background-color: %2;"
                      "  border: 1px solid %3;"
                      "  border-radius: 3px;"
                      "  padding: 1px 4px;"
                      "  color: %6;"
                      "}"
                      "QToolButton:hover {"
                      "  background-color: %4;"
                      "  border-color: %5;"
                      "}")
                      .arg(barBg, chipBg, chipBorder, hoverBg, tk.accent.name(), tk.onSurface.name()));

    m_announceIcon->setPixmap(renderVectorPixmap("info", tk.accent, 12));
    m_btnPortal->setIcon(QIcon(renderVectorPixmap("globe", tk.onSurface, 12)));
    m_btnSupport->setIcon(QIcon(renderVectorPixmap("chat", tk.onSurface, 12)));
}