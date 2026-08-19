#pragma once

#include <QList>
#include <QString>
#include <memory>

#include "include/database/entities/OtpProfile.h"
#include "include/ui/widget/TrayPopupFrame.hpp"

class QListWidgetItem;
class QTimer;

// A real window, not a tray submenu, for the reason given on TrayProfileSelector.
class TrayOtpCodes : public TrayPopupFrame {
    Q_OBJECT

public:
    explicit TrayOtpCodes(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

    void preparePopup() override;

private:
    void reload();

    void rebuild();

    // Without rebuilding, so the scroll position survives the once-a-second refresh.
    void refreshCodes() const;

    void copyCurrent(const QListWidgetItem *item);

    QTimer *ticker = nullptr;

    QList<std::shared_ptr<Configs::OtpProfile>> profiles;

    // Row index -> index into `profiles`, since the filter hides entries.
    QList<int> shown;
};
