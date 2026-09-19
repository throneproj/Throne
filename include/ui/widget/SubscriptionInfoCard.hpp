#pragma once

#include <QFrame>
#include <memory>

namespace Configs
{
    class Group;
}

class QLabel;
class QProgressBar;
class QToolButton;
class QTableView;
class QResizeEvent;
class QEvent;

class SubscriptionInfoCard : public QFrame
{
    Q_OBJECT
public:
    explicit SubscriptionInfoCard(QWidget *parent = nullptr);
    ~SubscriptionInfoCard() override = default;

    void setTableView(QTableView *table);
    void setGroup(const std::shared_ptr<Configs::Group> &group);
    void applyTheme();
    [[nodiscard]] bool hasSubscription() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void syncTableOffset();

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
    QTableView *m_tableView = nullptr;

    QLabel *m_titleLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;

    QFrame *m_expiryBadge = nullptr;
    QLabel *m_expiryIcon = nullptr;
    QLabel *m_expiryLabel = nullptr;

    QFrame *m_announceBadge = nullptr;
    QLabel *m_announceIcon = nullptr;
    QLabel *m_announceLabel = nullptr;

    QToolButton *m_btnPortal = nullptr;
    QToolButton *m_btnSupport = nullptr;
};