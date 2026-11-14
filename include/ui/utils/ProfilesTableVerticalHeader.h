#pragma once

#include <QHeaderView>

class ProfilesTableModel;
class ProfilesFilterProxyModel;

class ProfilesTableVerticalHeader : public QHeaderView {
    Q_OBJECT
public:
    explicit ProfilesTableVerticalHeader(QWidget *parent = nullptr);

    // Sections are numbered in `proxy`'s row space (may be null); labels come from `model`.
    void setProfilesModel(ProfilesTableModel *model, ProfilesFilterProxyModel *proxy = nullptr);
    ProfilesTableModel *profilesModel() const { return m_model; }

    void updateWidthFromRowCount();
    bool setPinnedVisible(bool visible);

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;

private:
    ProfilesTableModel *m_model = nullptr;
    ProfilesFilterProxyModel *m_proxy = nullptr;
    bool m_pinnedVisible = false;
};
