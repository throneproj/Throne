#pragma once

#include <QFrame>
#include <memory>

namespace Configs
{
    class Group;
}

class QLabel;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QEvent;

class SubscriptionInfoCard : public QFrame
{
    Q_OBJECT
public:
    explicit SubscriptionInfoCard(QWidget *parent = nullptr);
    ~SubscriptionInfoCard() override = default;

    void setGroup(const std::shared_ptr<Configs::Group> &group);
    void applyTheme();
    [[nodiscard]] bool hasSubscription() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void cardVisibilityChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void updateData();
    void updateAnnouncementLayout();

    std::shared_ptr<Configs::Group> m_group;
    QString m_fullAnnounce;

    QLabel *m_titleLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_quotaLabel = nullptr;

    QFrame *m_expiryBadge = nullptr;
    QLabel *m_expiryIcon = nullptr;
    QLabel *m_expiryLabel = nullptr;

    QFrame *m_intervalBadge = nullptr;
    QLabel *m_intervalIcon = nullptr;
    QLabel *m_intervalLabel = nullptr;

    QFrame *m_announceBadge = nullptr;
    QLabel *m_announceIcon = nullptr;
    QLabel *m_announceLabel = nullptr;

    QFrame *m_sepQuota = nullptr;
    QFrame *m_sepExpiry = nullptr;
    QFrame *m_sepInterval = nullptr;
    QFrame *m_sepAnnounce = nullptr;
    QFrame *m_sepActions = nullptr;

    QPushButton *m_btnPortal = nullptr;
    QPushButton *m_btnSupport = nullptr;
};