#pragma once

#include <QDialog>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <functional>
#include <memory>

#include "include/database/entities/IpList.h"
#include "include/database/entities/IpScan.h"
#include "ui_dialog_scan_instance.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogScanInstance;
}
QT_END_NAMESPACE

class QPushButton;
class QSortFilterProxyModel;
class QTimer;
class IpEntriesModel;

namespace Validation {
    class Form;
}

class DialogScanInstance : public QDialog {
    Q_OBJECT

public:
    static void Open(int scanId);

    static void Discard(int scanId);

    // False when the edits are invalid; the user has already been told.
    static bool SaveIfDirty(int scanId);

    ~DialogScanInstance() override;

    void reject() override;

protected:
    void showEvent(QShowEvent *event) override;

    void hideEvent(QHideEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

    void changeEvent(QEvent *event) override;

private:
    DialogScanInstance(QWidget *parent, std::shared_ptr<Configs::IpScan> scan);

    static QHash<int, QPointer<DialogScanInstance>> &openWindows();

    Ui::DialogScanInstance *ui;
    std::shared_ptr<Configs::IpScan> scan;
    const int scanId;
    // Keeps the stored values the form does not show across a save.
    Configs::ScanConfig config;
    const Configs::ScanConfig defaults;

    bool dirty = false;
    bool loading = false;
    bool watching = false;
    bool discarding = false;
    bool settingsLocked = false;
    bool resultsStale = true;
    bool resultsLoading = false;
    bool resultsBusy = false;
    bool profilesStale = false;
    quint64 resultsGeneration = 0;
    int resultsLoadedListId = -1;
    int resultsLoadedCount = -1;
    int resultCount = 0;
    QHash<int, QString> baseListNames;
    QStringList logLines;

    Validation::Form *form = nullptr;

    IpEntriesModel *resultsModel = nullptr;
    QSortFilterProxyModel *resultsProxy = nullptr;
    QTimer *resultsReload = nullptr;
    QPushButton *useAsResolvers = nullptr;

    [[nodiscard]] bool isWarp() const;

    [[nodiscard]] bool isResolver() const;

    void setupSettings();

    void bindValidation();

    void setupResults();

    void setupRunPanel();

    void watchEdits(QWidget *root);

    void markDirty();

    void loadForm();

    bool collect(Configs::ScanConfig &out, int &baseListId, QString &name, QString *error) const;

    bool save(QString *error);

    bool saveIfDirty();

    void reloadBaseLists(int selectId);

    void reloadProfiles(int selectId);

    void matchWarpSeed();

    void reloadRow();

    void refreshRun();

    void applyLock(bool locked);

    void applyLogLines();

    void scheduleResultsReload();

    void reloadResults();

    void applyResults(int listId, int count, const QList<Configs::IpListEntry> &entries);

    void runResultsTask(const std::function<void()> &work, const std::function<void()> &done);

    [[nodiscard]] QList<Configs::IpListEntry> selectedResults() const;

    void saveResultsAsList();

    void useResultsAsResolvers();

    void copyResults();

    void exportResults();

    void removeSelectedResults();

    void startClicked();

    void startFromBeginning();

    void pauseClicked();

    void rescanClicked();
};
