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
#include "include/scanner/TunePlanner.h"
#include "ui_dialog_scan_instance.h"
#include "ui_scan_phases.h"
#include "ui_scan_tune_settings.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogScanInstance;
}
QT_END_NAMESPACE

class QComboBox;
class QLineEdit;
class QMenu;
class QPushButton;
class QSortFilterProxyModel;
class QStandardItemModel;
class QStringListModel;
class QTimer;
class QVBoxLayout;
class IpEntriesModel;

namespace Configs_network {
    struct WarpIdentity;
}

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
    // Keeps what the form does not show (the registered WARP device) across a save.
    Configs::ScanConfig config;
    const Configs::ScanConfig defaults;

    bool dirty = false;
    bool loading = false;
    bool watching = false;
    bool discarding = false;
    bool registering = false;
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

    // Null for a "Find working settings" scan; the boxes of phases the scan kind lacks stay hidden.
    Ui::ScanPhases *phasesUi = nullptr;
    QHash<QString, int> configProfileIds;
    QStringListModel *configProfileNames = nullptr;
    QList<QLineEdit *> warpPackets;
    QPushButton *useAsResolvers = nullptr;

    // Set only for a "Find working settings" scan.
    Ui::ScanTuneSettings *tuneUi = nullptr;
    QHash<QString, int> tuneProfileIds;
    QStringListModel *tuneProfileNames = nullptr;
    // Unsupported() verdicts by profile id, valid while ProfilesRepo::Revision() stays at tuneSupportedRevision.
    QHash<int, bool> tuneSupported;
    quint64 tuneSupportedRevision = 0;

    QStandardItemModel *tuneModel = nullptr;
    QList<Scanner::Tune::Trial> tuneTrials;
    // The user picked a row, so a rebuild keeps it rather than moving to the new best.
    bool tuneSelectionPinned = false;
    bool tuneFilling = false;

    [[nodiscard]] bool isGeneric() const;

    [[nodiscard]] bool isWarp() const;

    [[nodiscard]] bool isResolver() const;

    [[nodiscard]] bool isTune() const;

    void setupTargets();

    void buildPhases();

    void buildIcmpPhase();

    void buildTcpPhase();

    void buildHttpPhase();

    void buildConfigPhase();

    void buildWarpPhase();

    void buildResolverPhase();

    void buildTunePhase(QVBoxLayout *layout);

    void setupProfilePicker(QComboBox *combo, QStringListModel *names);

    void bindValidation();

    void bindTuneValidation();

    void setupResults();

    void setupRunPanel();

    void watchEdits(QWidget *root);

    void markDirty();

    void loadForm();

    bool collect(Configs::ScanConfig &out, int &baseListId, QString &name, QString *error) const;

    bool save(QString *error);

    bool saveIfDirty();

    void reloadBaseLists(int selectId);

    void reloadConfigProfiles(int selectId);

    [[nodiscard]] int selectedConfigProfile() const;

    void reloadWarpProfiles(int selectId);

    [[nodiscard]] QString currentWarpMode() const;

    void reloadResolverProfiles(int selectId);

    void reloadResolverLevels(const QString &selectLevel);

    [[nodiscard]] QString currentResolverProtocol() const;

    void updateResolverTimeoutHint();

    void loadTuneForm();

    void reloadTuneProfiles(int selectId);

    [[nodiscard]] int selectedTuneProfile() const;

    void updateTuneControls();

    void updateHttpControls();

    void updateWarpControls();

    void updateWarpIdentityStatus();

    void useBuiltinWarpRanges();

    void matchWarpTargets();

    void fillPortPresets(QMenu *menu);

    void refreshProfilePickers();

    void registerWarpIdentity();

    void applyWarpRegistration(const QString &transport, const std::shared_ptr<Configs_network::WarpIdentity> &identity,
                               const QString &error);

    void reloadRow();

    void refreshRun();

    void applyLock(bool locked);

    void applyLogLines();

    void scheduleResultsReload();

    void reloadResults();

    void applyResults(int listId, int count, const QList<Configs::IpListEntry> &entries);

    void applyResultColumns();

    void runResultsTask(const std::function<void()> &work, const std::function<void()> &done);

    [[nodiscard]] QList<Configs::IpListEntry> selectedResults() const;

    void saveResultsAsList();

    void useResultsAsResolvers();

    void copyResults();

    void exportResults();

    void removeSelectedResults();

    void setupTuneResults();

    void reloadTuneResults();

    void updateTuneActions();

    [[nodiscard]] int selectedTuneTrial() const;

    void applyTuneResult();

    void saveTuneResultAsProfile();

    void copyTuneResults();

    void startClicked();

    void startFromBeginning();

    void pauseClicked();

    void rescanClicked();
};
