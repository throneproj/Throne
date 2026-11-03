#pragma once

#include <QListWidgetItem>
#include <QWidget>
#include <memory>

#include "include/database/entities/IpScan.h"
#include "include/scanner/ScanManager.h"

class QLabel;
class QProgressBar;
class QToolButton;

namespace ScannerUi {
    // Ref-counted per scan: every Watch(id, true) needs a matching Watch(id, false).
    void Watch(int scanId, bool watched);

    [[nodiscard]] bool IsResumable(const Configs::IpScan &scan);

    [[nodiscard]] int ResultCount(const Configs::IpScan &scan);

    bool ConfirmWipe(QWidget *parent, const Configs::IpScan &scan);

    bool StartScan(QWidget *parent, int scanId, Scanner::StartMode mode);

    [[nodiscard]] Scanner::ScanLiveState RowState(const Configs::IpScan &scan);

    [[nodiscard]] QString StatusLine(const Configs::IpScan &scan, const Scanner::ScanLiveState &live, bool withStage,
                                     bool *isError);

    [[nodiscard]] QString KindText(Configs::IpScan::Kind kind);

    // Opens the newest DNS resolver scan of the profile, creating one when it has none.
    void FindResolvers(int profileId);
} // namespace ScannerUi

class ScanItem : public QWidget {
    Q_OBJECT

public:
    explicit ScanItem(QWidget *parent, std::shared_ptr<Configs::IpScan> scan, QListWidgetItem *item);

    void Refresh(bool reload);

    [[nodiscard]] int ScanId() const { return scanId; }

    // The owning list shows the actions of the hovered row only.
    void SetActionsVisible(bool visible);

signals:
    void startRequested();

    void pauseRequested();

    void openRequested();

    void deleteRequested();

protected:
    void changeEvent(QEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

private:
    std::shared_ptr<Configs::IpScan> scan;
    int scanId;
    QListWidgetItem *item;

    QLabel *nameLabel;
    QLabel *kindLabel;
    QLabel *statusLabel;
    QProgressBar *progressBar;
    QWidget *actions;
    QToolButton *runButton;
    QToolButton *openButton;
    QToolButton *removeButton;

    QString statusText;
    bool statusError = false;
    bool running = false;

    void updateElidedLabels() const;

    void applyStyle() const;

    void applyIcons() const;

    void updateRunButton() const;
};
