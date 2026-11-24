#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QList>
#include <QString>
#include <memory>

#include "include/database/entities/IpList.h"

namespace Configs {
    class Profile;
}

class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;
class QTreeWidget;

// Searches for WARP endpoints that pass data on this network; the search starts with the dialog.
class DialogWarpFinder : public QDialog {
    Q_OBJECT

public:
    // overrideOrigin: the endpoint source the profile dials instead of its own address, empty when there is none.
    DialogWarpFinder(std::shared_ptr<Configs::Profile> base, const QString &overrideOrigin, QWidget *parent);

    ~DialogWarpFinder() override;

    // E.g. IP list "Foo"; empty when the profile dials its own address.
    [[nodiscard]] static QString OverrideOrigin(const std::shared_ptr<Configs::Profile> &profile);

    void done(int result) override;

signals:
    void endpointChosen(const Configs::IpListEntry &entry, bool junk, bool http2);

private:
    const std::shared_ptr<Configs::Profile> base;
    const QString overrideOrigin;
    int runId = 0;
    QString startError;
    QElapsedTimer clock;
    QList<Configs::IpListEntry> shownHits;
    bool selectionByUser = false;
    bool populating = false;

    QLabel *statusLabel;
    QLabel *detailLabel;
    QWidget *progressRow;
    QProgressBar *progressBar;
    QLabel *foundLabel;
    QLabel *noticeLabel;
    QTreeWidget *resultsView;
    QLabel *placeholderLabel;
    QPushButton *runButton;
    QPushButton *useButton;
    QTimer *refreshTimer;

    void start();

    void forgetRun();

    void scheduleRefresh();

    void refresh();

    void showHits(const QList<Configs::IpListEntry> &hits);

    void runClicked();

    void useSelected();
};
