#pragma once

#include <QHeaderView>

class ProfilesTableModel;

// Vertical header that shows "✓" for the running row and "1  ", "2  ", ... for others.
class ProfilesTableVerticalHeader : public QHeaderView {
    Q_OBJECT
public:
    explicit ProfilesTableVerticalHeader(QWidget *parent = nullptr);

    void setProfilesModel(ProfilesTableModel *model);
    ProfilesTableModel *profilesModel() const { return m_model; }

    // Update header width to fit the widest row label (based on row count).
    void updateWidthFromRowCount();

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;

private:
    ProfilesTableModel *m_model = nullptr;
};
