#include "include/ui/scanner/dialog_scan_instance.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QClipboard>
#include <QCursor>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIntValidator>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSet>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QToolTip>
#include <QVBoxLayout>

#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EditLocks.h"
#include "include/global/RunningProfiles.hpp"
#include "include/global/Utils.hpp"
#include "include/scanner/ScanManager.h"
#include "include/scanner/TuneProfiles.h"
#include "include/ui/scanner/ScanItem.h"
#include "include/ui/utils/NoWheelComboBox.h"
#include "include/ui/utils/ValidationForm.h"

namespace {
    constexpr int kScanInstanceTuneTrialRole = Qt::UserRole + 1;
    constexpr int kScanInstanceTuneSettingsColumn = 0;
    constexpr int kScanInstanceTunePassedColumn = 1;
    constexpr int kScanInstanceTuneLatencyColumn = 2;
    constexpr int kScanInstanceTuneNoteColumn = 3;
    constexpr int kScanInstanceTuneColumns = 4;

    QLineEdit *scanInstanceTuneNumberEdit(QWidget *parent, int minimum, int maximum, int defaultValue) {
        auto *edit = new QLineEdit(parent);
        edit->setValidator(new QIntValidator(minimum, maximum, edit));
        edit->setPlaceholderText(QString::number(defaultValue));
        return edit;
    }

    QLabel *scanInstanceTuneHint(const QString &text, QWidget *parent) {
        auto *label = new QLabel(text, parent);
        label->setWordWrap(true);
        label->setTextFormat(Qt::PlainText);
        label->setProperty("colorRole", QStringLiteral("muted"));
        return label;
    }

    void scanInstanceTuneNotice(QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &text) {
        auto *box = new QMessageBox(icon, title, text, QMessageBox::Ok, parent);
        box->setTextFormat(Qt::PlainText);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setWindowModality(Qt::NonModal);
        box->show();
    }

    QString scanInstanceTuneTsvCell(QString text) {
        for (auto &c : text) {
            if (c == QLatin1Char('\t') || c == QLatin1Char('\n') || c == QLatin1Char('\r')) c = QLatin1Char(' ');
        }
        return text;
    }

    QString scanInstanceTuneLatency(int ms) {
        return ms >= 0 ? DialogScanInstance::tr("%1 ms").arg(ms) : QStringLiteral("—");
    }

    // Saving the dialog can point the scan at another profile without running it again.
    bool scanInstanceTuneResultsMatch(const Configs::IpScan &scan) {
        const int profileId = scan.config.tune.profileId;
        return scan.tune_state.value(QLatin1String("profileId")).toInt(profileId) == profileId;
    }

    QString scanInstanceTuneOtherProfile() {
        return DialogScanInstance::tr("These results are for another profile; start the search to test the one chosen now.");
    }
}

void DialogScanInstance::buildTunePhase(QVBoxLayout *layout) {
    const auto &tuneDefaults = defaults.tune;

    tuneProfileBox = new QGroupBox(tr("Profile"), ui->phases_content);
    auto *profileForm = new QFormLayout(tuneProfileBox);
    profileForm->addRow(scanInstanceTuneHint(
        tr("Variants of this profile's anti-censorship settings are tested through the core, one attempt at a time. The profile "
           "itself changes only when you apply a result."),
        tuneProfileBox));
    tuneProfile = new NoWheelComboBox(tuneProfileBox);
    tuneProfileNames = new QStringListModel(this);
    setupProfilePicker(tuneProfile, tuneProfileNames);
    tuneProfile->lineEdit()->setPlaceholderText(tr("Select a profile…"));
    profileForm->addRow(tr("Profile"), tuneProfile);
    tuneProfileHint = scanInstanceTuneHint(QString(), tuneProfileBox);
    profileForm->addRow(static_cast<QWidget *>(nullptr), tuneProfileHint);
    layout->addWidget(tuneProfileBox);

    tuneTryBox = new QGroupBox(tr("What to try"), ui->phases_content);
    auto *tryForm = new QFormLayout(tuneTryBox);
    tryForm->addRow(scanInstanceTuneHint(
        tr("Each setting is first tried on its own, a switch flipped or a list value by value; the changes that help are then "
           "combined."),
        tuneTryBox));
    tuneFingerprints = new QCheckBox(tr("uTLS fingerprint"), tuneTryBox);
    tryForm->addRow(tuneFingerprints);
    tuneFingerprintList = new QLineEdit(tuneTryBox);
    tryForm->addRow(tr("Fingerprints to try"), tuneFingerprintList);
    tuneFragment = new QCheckBox(tr("TLS fragment"), tuneTryBox);
    tryForm->addRow(tuneFragment);
    tuneFragmentDelays = new QLineEdit(tuneTryBox);
    tryForm->addRow(tr("Fallback delays to try"), tuneFragmentDelays);
    tuneRecordFragment = new QCheckBox(tr("TLS record fragment"), tuneTryBox);
    tryForm->addRow(tuneRecordFragment);
    tuneMixedCaseSni = new QCheckBox(tr("Mixed-case SNI"), tuneTryBox);
    tryForm->addRow(tuneMixedCaseSni);
    tuneQuicOpener = new QCheckBox(tr("QUIC opener"), tuneTryBox);
    tryForm->addRow(tuneQuicOpener);
    tuneMtu = new QCheckBox(tr("MTU (WireGuard)"), tuneTryBox);
    tryForm->addRow(tuneMtu);
    tuneMtuMin = scanInstanceTuneNumberEdit(tuneTryBox, 576, 9000, tuneDefaults.mtuMin);
    tryForm->addRow(tr("Smallest MTU"), tuneMtuMin);
    tuneMtuMax = new QLineEdit(tuneTryBox);
    tuneMtuMax->setValidator(new QIntValidator(0, 9000, tuneMtuMax));
    tuneMtuMax->setPlaceholderText(tr("The profile's own"));
    tryForm->addRow(tr("Largest MTU"), tuneMtuMax);
    tuneMtuStep = scanInstanceTuneNumberEdit(tuneTryBox, 1, 1000, tuneDefaults.mtuStep);
    tryForm->addRow(tr("MTU step"), tuneMtuStep);
    tuneMtuUrl = new QLineEdit(tuneTryBox);
    tryForm->addRow(tr("Download URL"), tuneMtuUrl);
    tuneMtuBytes = scanInstanceTuneNumberEdit(tuneTryBox, 1000, 100000000, tuneDefaults.mtuBytes);
    tryForm->addRow(tr("Download size (bytes)"), tuneMtuBytes);
    layout->addWidget(tuneTryBox);

    tunePacingBox = new QGroupBox(tr("Pacing"), ui->phases_content);
    auto *pacingForm = new QFormLayout(tunePacingBox);
    tuneAttempts = scanInstanceTuneNumberEdit(tunePacingBox, 1, 50, tuneDefaults.attempts);
    tuneAttempts->setToolTip(tr("A censor that resets only some handshakes needs several tries to tell a working variant from a lucky one."));
    pacingForm->addRow(tr("Attempts per variant"), tuneAttempts);
    tunePerMinute = scanInstanceTuneNumberEdit(tunePacingBox, 1, 120, tuneDefaults.perMinute);
    tunePerMinute->setToolTip(tr("At most this many attempts start in any minute."));
    pacingForm->addRow(tr("Attempts per minute"), tunePerMinute);
    tuneMaxAttempts = scanInstanceTuneNumberEdit(tunePacingBox, 1, 5000, tuneDefaults.maxAttempts);
    tuneMaxAttempts->setToolTip(tr("The search stops after this many attempts in all."));
    pacingForm->addRow(tr("Attempt limit"), tuneMaxAttempts);
    tuneMaxMinutes = scanInstanceTuneNumberEdit(tunePacingBox, 1, 1440, tuneDefaults.maxMinutes);
    tuneMaxMinutes->setToolTip(tr("The search stops after this many minutes."));
    pacingForm->addRow(tr("Time limit (minutes)"), tuneMaxMinutes);
    tuneUrl = new QLineEdit(tunePacingBox);
    tuneUrl->setPlaceholderText(Configs::dataManager->settingsRepo->test_latency_url);
    tuneUrl->setToolTip(tr("Every attempt requests this URL through the profile. While the MTU is searched, attempts download from "
                           "the MTU download URL instead."));
    pacingForm->addRow(tr("Test URL"), tuneUrl);
    tuneTimeout = scanInstanceTuneNumberEdit(tunePacingBox, 100, 60000, tuneDefaults.timeoutMs);
    pacingForm->addRow(tr("Timeout (ms)"), tuneTimeout);
    layout->addWidget(tunePacingBox);

    connect(tuneProfile, &QComboBox::currentTextChanged, this, [this] {
        if (!loading) updateTuneControls();
    });
    for (auto *box : {tuneFingerprints, tuneFragment, tuneMtu}) connect(box, &QCheckBox::toggled, this, [this] { updateTuneControls(); });
    const auto updateMtuUrlHint = [this] {
        Configs::ScanTuneOptions options;
        bool ok = false;
        const int bytes = tuneMtuBytes->hasAcceptableInput() ? tuneMtuBytes->validator()->locale().toInt(tuneMtuBytes->text(), &ok) : 0;
        if (ok) options.mtuBytes = bytes;
        tuneMtuUrl->setPlaceholderText(Scanner::Tune::MtuTestUrl(options));
    };
    connect(tuneMtuBytes, &QLineEdit::textChanged, this, updateMtuUrlHint);
    updateMtuUrlHint();
}

void DialogScanInstance::bindTuneValidation() {
    form->bind(tuneFingerprintList, [](const QString &text) {
        QString error;
        (void) Scanner::Tune::ParseFingerprints(text, &error);
        return error;
    });
    form->bind(tuneFragmentDelays, [](const QString &text) {
        QString error;
        (void) Scanner::Tune::ParseDelays(text, &error);
        return error;
    });
    form->bind(tuneMtuUrl, Validation::Rules::Url({QStringLiteral("http"), QStringLiteral("https")}));
    form->bind(tuneUrl, Validation::Rules::Url({QStringLiteral("http"), QStringLiteral("https")}));
}

void DialogScanInstance::loadTuneForm() {
    const auto &tune = config.tune;
    reloadTuneProfiles(tune.profileId);
    tuneFingerprints->setChecked(tune.fingerprints);
    tuneFingerprintList->setText(tune.fingerprintList);
    tuneFragment->setChecked(tune.fragment);
    tuneFragmentDelays->setText(tune.fragmentDelays);
    tuneRecordFragment->setChecked(tune.recordFragment);
    tuneMixedCaseSni->setChecked(tune.mixedCaseSni);
    tuneQuicOpener->setChecked(tune.quicOpener);
    tuneMtu->setChecked(tune.mtu);
    tuneMtuMin->setText(QString::number(tune.mtuMin));
    tuneMtuMax->setText(tune.mtuMax > 0 ? QString::number(tune.mtuMax) : QString());
    tuneMtuStep->setText(QString::number(tune.mtuStep));
    tuneMtuUrl->setText(tune.mtuUrl);
    tuneMtuBytes->setText(QString::number(tune.mtuBytes));
    tuneAttempts->setText(QString::number(tune.attempts));
    tunePerMinute->setText(QString::number(tune.perMinute));
    tuneMaxAttempts->setText(QString::number(tune.maxAttempts));
    tuneMaxMinutes->setText(QString::number(tune.maxMinutes));
    tuneUrl->setText(tune.url);
    tuneTimeout->setText(QString::number(tune.timeoutMs));
    updateTuneControls();
}

void DialogScanInstance::reloadTuneProfiles(int selectId) {
    if (tuneProfile == nullptr) return;
    const QScopedValueRollback guard(loading, true);
    auto *profiles = Configs::dataManager->profilesRepo.get();
    if (const auto revision = profiles->Revision(); revision != tuneSupportedRevision) {
        tuneSupported.clear();
        tuneSupportedRevision = revision;
    }

    QHash<int, QString> idToName;
    QList<int> unknown;
    for (const auto &[id, profileName] : profiles->GetAllProfileIDNameMapped()) {
        idToName.insert(id, profileName);
        if (!tuneSupported.contains(id)) unknown << id;
    }
    for (const auto &profile : profiles->GetProfileBatch(unknown))
        tuneSupported.insert(profile->id, Scanner::Tune::Unsupported(*profile).isEmpty());

    tuneProfile->clear();
    tuneProfileIds.clear();
    QStringList names;
    QString selectedName;
    for (const int gid : Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
        const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (group == nullptr) continue;
        const QString prefix = QStringLiteral("[") + group->name + QStringLiteral("] ");
        for (const int id : group->profiles) {
            if (!tuneSupported.value(id, false)) continue;
            const auto found = idToName.constFind(id);
            if (found == idToName.constEnd()) continue;
            const QString display = prefix + found.value();
            names << display;
            if (!tuneProfileIds.contains(display)) tuneProfileIds.insert(display, id);
            if (id == selectId) selectedName = display;
            if (tuneProfile->count() < tuneProfile->maxCount()) tuneProfile->addItem(display, id);
        }
    }
    tuneProfileNames->setStringList(names);

    const int index = selectId >= 0 ? tuneProfile->findData(selectId) : -1;
    if (index >= 0) tuneProfile->setCurrentIndex(index);
    else if (!selectedName.isEmpty()) tuneProfile->setCurrentText(selectedName);
    else tuneProfile->setCurrentIndex(-1);
    updateTuneControls();
}

int DialogScanInstance::selectedTuneProfile() const {
    if (tuneProfile == nullptr) return -1;
    const QString text = tuneProfile->currentText();
    if (text.trimmed().isEmpty()) return -1;
    // The current item first: two profiles of one group can share a name.
    if (const int current = tuneProfile->currentIndex(); current >= 0 && tuneProfile->itemText(current) == text)
        return tuneProfile->itemData(current).toInt();
    if (const int index = tuneProfile->findText(text, Qt::MatchExactly); index >= 0) return tuneProfile->itemData(index).toInt();
    return tuneProfileIds.value(text, tuneProfileIds.value(text.trimmed(), -1));
}

void DialogScanInstance::updateTuneControls() {
    if (tuneTryBox == nullptr) return;
    using namespace Scanner::Tune;
    const int id = selectedTuneProfile();
    const auto profile = id >= 0 ? Configs::dataManager->profilesRepo->GetProfile(id) : nullptr;

    // Why a setting cannot apply to the selected profile; empty when it can.
    QString fingerprintWhy, fragmentWhy, delayWhy, recordFragmentWhy, mixedCaseSniWhy, quicOpenerWhy, mtuWhy;
    if (profile != nullptr && profile->outbound != nullptr) {
        Configs::ScanTuneOptions every;
        every.fingerprints = every.fragment = every.recordFragment = every.mixedCaseSni = every.quicOpener = every.mtu = true;
        const auto space = SpaceFor(*profile, every);
        QSet<QString> keys;
        for (const auto &knob : space.knobs) keys.insert(knob.key);
        const bool tls = keys.contains(QLatin1String(kFragment));
        const QString noTls = tr("This profile does not use TLS over TCP.");
        if (!keys.contains(QLatin1String(kFingerprint))) fingerprintWhy = tls ? tr("This type of profile has no uTLS fingerprint.") : noTls;
        if (!tls) fragmentWhy = noTls;
        if (space.delay.key.isEmpty())
            delayWhy = tls ? tr("Only the built-in fragment implementation uses a fallback delay.") : noTls;
        if (!keys.contains(QLatin1String(kRecordFragment))) recordFragmentWhy = noTls;
        if (!keys.contains(QLatin1String(kMixedCaseSni))) mixedCaseSniWhy = tls ? tr("Needs SNI turned on with a domain name, and no REALITY.") : noTls;
        if (!keys.contains(QLatin1String(kQuicOpener)))
            quicOpenerWhy = tr("Needs a profile that opens UDP flows to its server: QUIC, MASQUE over HTTP/3 or WireGuard.");
        if (space.mtuCurrent <= 0) mtuWhy = tr("Only WireGuard and AmneziaWG profiles have an MTU to search.");
    }

    // A disabled control keeps its value; the tooltip says why it is off.
    const auto mark = [](QWidget *control, const QString &why, const QString &tip, bool on) {
        control->setEnabled(why.isEmpty() && on);
        control->setToolTip(why.isEmpty() ? tip : why);
    };
    mark(tuneFingerprints, fingerprintWhy, {}, true);
    mark(tuneFingerprintList, fingerprintWhy, tr("Comma-separated uTLS fingerprints; \"off\" tries Go's own TLS."),
         tuneFingerprints->isChecked());
    mark(tuneFragment, fragmentWhy, {}, true);
    mark(tuneFragmentDelays, delayWhy,
         tr("Comma-separated durations such as 100ms, 500ms, 1s, tried on the best variant that fragments. Only the built-in "
            "fragment implementation uses them."),
         tuneFragment->isChecked());
    mark(tuneRecordFragment, recordFragmentWhy, {}, true);
    mark(tuneMixedCaseSni, mixedCaseSniWhy, {}, true);
    mark(tuneQuicOpener, quicOpenerWhy,
         tr("Sends one decoy packet before each new UDP flow, so networks that block QUIC by its first packet let the real "
            "handshake through."),
         true);
    mark(tuneMtu, mtuWhy, tr("Finds the largest MTU that still carries a bulk download through the tunnel."), true);
    const bool mtuOn = tuneMtu->isChecked();
    for (QLineEdit *edit : {tuneMtuMin, tuneMtuMax, tuneMtuStep}) mark(edit, mtuWhy, {}, mtuOn);
    mark(tuneMtuUrl, mtuWhy, tr("Downloaded through the tunnel to check each MTU. Empty: Cloudflare's speed test."), mtuOn);
    mark(tuneMtuBytes, mtuWhy, tr("How many bytes each MTU check downloads."), mtuOn);

    QString hint = tr("Choose the profile whose settings to search.");
    if (profile != nullptr) {
        const auto host = DialHost(profile);
        hint = host.isEmpty() ? tr("The profile has no server address to test.") : tr("Tests through %1").arg(host);
    } else if (id < 0 && config.tune.profileId >= 0) {
        const auto stored = Configs::dataManager->profilesRepo->GetProfile(config.tune.profileId);
        if (stored == nullptr) {
            hint = tr("The profile this search was made for no longer exists.");
        } else if (const auto reason = Unsupported(*stored); !reason.isEmpty()) {
            const QString storedName = stored->outbound != nullptr ? stored->outbound->name : stored->name;
            hint = tr("\"%1\" cannot be searched. %2.").arg(storedName, reason);
        }
    }
    tuneProfileHint->setText(hint);
}

void DialogScanInstance::setupTuneResults() {
    tuneModel = new QStandardItemModel(0, kScanInstanceTuneColumns, this);
    tuneModel->setHorizontalHeaderLabels({tr("Settings"), tr("Passed"), tr("Median latency"), tr("Note")});
    auto *view = ui->results_view;
    view->setSortingEnabled(false);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setModel(tuneModel);
    auto *header = view->horizontalHeader();
    header->setSectionResizeMode(kScanInstanceTuneSettingsColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(kScanInstanceTunePassedColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(kScanInstanceTuneLatencyColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(kScanInstanceTuneNoteColumn, QHeaderView::Stretch);
    ui->results_summary->setWordWrap(true);
    ui->results_summary->setTextFormat(Qt::PlainText);

    for (QWidget *listOnly : {static_cast<QWidget *>(ui->save_as_list), static_cast<QWidget *>(ui->export_results),
                              static_cast<QWidget *>(ui->remove_results)})
        listOnly->hide();
    tuneApply = new QPushButton(tr("Apply to profile"), ui->tab_results);
    tuneApply->setToolTip(tr("Writes the selected variant into the profile's own settings; global defaults stay as they are."));
    tuneSaveNew = new QPushButton(tr("Save as new profile"), ui->tab_results);
    tuneSaveNew->setToolTip(tr("Adds a copy of the profile with the selected variant to its group; the profile itself stays as it is."));
    ui->layout_results_buttons->insertWidget(0, tuneApply);
    ui->layout_results_buttons->insertWidget(1, tuneSaveNew);
    ui->copy_results->setToolTip(tr("Copies the table as tab-separated text."));

    connect(tuneApply, &QPushButton::clicked, this, [this] { applyTuneResult(); });
    connect(tuneSaveNew, &QPushButton::clicked, this, [this] { saveTuneResultAsProfile(); });
    connect(ui->copy_results, &QPushButton::clicked, this, [this] { copyTuneResults(); });
    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        if (!tuneFilling) tuneSelectionPinned = selectedTuneTrial() >= 0;
        updateTuneActions();
    });
}

void DialogScanInstance::reloadTuneResults() {
    if (tuneModel == nullptr) return;
    if (auto row = Configs::dataManager->ipScansRepo->GetIpScan(scanId)) scan = row;
    resultCount = ScannerUi::ResultCount(*scan);

    const int previous = selectedTuneTrial();
    const bool keep = tuneSelectionPinned && previous >= 0;
    const auto kept = keep ? tuneTrials.at(previous).assignment : Scanner::Tune::Assignment();

    const auto &state = scan->tune_state;
    tuneTrials = Scanner::Tune::TrialsFromJson(state.value(QLatin1String("trials")).toArray());
    const int best = state.value(QLatin1String("best")).toInt(-1);
    const int mtuWinner = state.value(QLatin1String("mtuWinner")).toInt(0);
    const auto order = Scanner::Tune::Rank(tuneTrials);

    {
        const QScopedValueRollback guard(tuneFilling, true);
        int selectRow = -1;
        int bestRow = -1;
        tuneModel->setRowCount(static_cast<int>(order.size()));
        for (int row = 0; row < order.size(); ++row) {
            const int index = order.at(row);
            const auto &trial = tuneTrials.at(index);
            QStringList notes;
            if (index == best && trial.successes > 0) notes << tr("Recommended");
            if (trial.assignment.isEmpty()) notes << tr("Profile as it is");
            if (trial.round > 1) notes << tr("Confirmed");
            if (trial.dropped && trial.Attempts() > 0) notes << tr("Dropped early");
            if (trial.dropped && trial.Attempts() == 0)
                notes << (trial.lastError.isEmpty() ? tr("Not usable") : tr("Not usable: %1").arg(trial.lastError));
            if (trial.stopOnSuccess) notes << tr("MTU probe");
            if (mtuWinner > 0 && trial.assignment.value(QLatin1String(Scanner::Tune::kMtu)) == QString::number(mtuWinner))
                notes << tr("Largest working MTU");
            const QString passed = trial.Attempts() > 0 ? tr("%1/%2").arg(trial.successes).arg(trial.Attempts()) : QStringLiteral("—");
            const QStringList cells = {Scanner::Tune::Describe(trial.assignment), passed,
                                       scanInstanceTuneLatency(trial.MedianLatencyMs()), notes.join(QStringLiteral(" · "))};
            for (int column = 0; column < kScanInstanceTuneColumns; ++column) {
                auto *item = tuneModel->item(row, column);
                if (item == nullptr) {
                    item = new QStandardItem();
                    item->setEditable(false);
                    if (column == kScanInstanceTunePassedColumn || column == kScanInstanceTuneLatencyColumn)
                        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    tuneModel->setItem(row, column, item);
                }
                item->setText(cells.at(column));
                item->setToolTip(trial.lastError);
            }
            tuneModel->item(row, kScanInstanceTuneSettingsColumn)->setData(index, kScanInstanceTuneTrialRole);
            if (index == best) bestRow = row;
            if (keep && trial.assignment == kept) selectRow = row;
        }
        if (selectRow < 0) {
            selectRow = bestRow;
            tuneSelectionPinned = false;
        }
        auto *selection = ui->results_view->selectionModel();
        if (selectRow >= 0) {
            selection->setCurrentIndex(tuneModel->index(selectRow, kScanInstanceTuneSettingsColumn),
                                       QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        } else {
            selection->clearSelection();
        }
    }

    QStringList lines;
    if (tuneTrials.isEmpty()) {
        lines << tr("No results yet.");
    } else if (best >= 0 && best < tuneTrials.size() && tuneTrials.at(best).successes > 0) {
        const auto &trial = tuneTrials.at(best);
        lines << tr("Best: %1 (passed %2/%3, median %4)")
                     .arg(Scanner::Tune::Describe(trial.assignment), QString::number(trial.successes),
                          QString::number(trial.Attempts()), scanInstanceTuneLatency(trial.MedianLatencyMs()));
    } else {
        const bool done = !Scanner::ScanManager::instance()->IsRunning(scanId) && scan->status == Configs::IpScan::Status::Completed;
        lines << (done ? tr("No variant passed.") : tr("No variant has passed yet."));
    }
    if (const auto note = state.value(QLatin1String("note")).toString(); !note.isEmpty()) lines << note;
    if (const auto host = state.value(QLatin1String("dialHost")).toString(); !host.isEmpty()) lines << tr("Tested through %1").arg(host);
    if (!tuneTrials.isEmpty() && !scanInstanceTuneResultsMatch(*scan)) lines << scanInstanceTuneOtherProfile();
    ui->results_summary->setText(lines.join(QLatin1Char('\n')));
    updateTuneActions();
}

void DialogScanInstance::updateTuneActions() {
    if (tuneModel == nullptr) return;
    const bool usable = !settingsLocked && selectedTuneTrial() >= 0;
    tuneApply->setEnabled(usable);
    tuneSaveNew->setEnabled(usable);
    ui->copy_results->setEnabled(tuneModel->rowCount() > 0);
}

int DialogScanInstance::selectedTuneTrial() const {
    if (tuneModel == nullptr || ui->results_view->selectionModel() == nullptr) return -1;
    const auto rows = ui->results_view->selectionModel()->selectedRows(kScanInstanceTuneSettingsColumn);
    if (rows.size() != 1) return -1;
    bool ok = false;
    const int index = rows.first().data(kScanInstanceTuneTrialRole).toInt(&ok);
    return ok && index >= 0 && index < tuneTrials.size() ? index : -1;
}

void DialogScanInstance::applyTuneResult() {
    const int index = selectedTuneTrial();
    if (index < 0 || Scanner::ScanManager::instance()->IsRunning(scanId)) return;
    const auto assignment = tuneTrials.at(index).assignment;
    const auto title = tr("Apply to profile");
    reloadRow();
    if (!scanInstanceTuneResultsMatch(*scan)) {
        scanInstanceTuneNotice(this, QMessageBox::Warning, title, scanInstanceTuneOtherProfile());
        return;
    }
    const int profileId = scan->config.tune.profileId;
    const auto profile = profileId >= 0 ? Configs::dataManager->profilesRepo->GetProfile(profileId) : nullptr;
    if (profile == nullptr || profile->outbound == nullptr) {
        scanInstanceTuneNotice(this, QMessageBox::Warning, title, tr("The profile of this search no longer exists."));
        return;
    }
    const QString profileName = profile->outbound->name;
    if (Engine::IsBeingEdited(Engine::EditTarget::Profile, profileId)) {
        scanInstanceTuneNotice(this, QMessageBox::Warning, title,
                               tr("\"%1\" is open in an editor; close the profile's editor first, then apply again.").arg(profileName));
        return;
    }
    if (assignment.isEmpty()) {
        scanInstanceTuneNotice(this, QMessageBox::Information, title, tr("\"%1\" already uses these settings.").arg(profileName));
        return;
    }

    QString error;
    // A dry run on a copy keeps a refused variant from half-changing the shared live profile.
    const auto probe = Scanner::Tune::Copy(*profile);
    if ((probe != nullptr && !Scanner::Tune::Apply(*probe, assignment, &error)) || !Scanner::Tune::Apply(*profile, assignment, &error)) {
        scanInstanceTuneNotice(this, QMessageBox::Warning, title,
                               tr("The settings could not be applied to \"%1\": %2.").arg(profileName, error));
        return;
    }
    if (!Configs::dataManager->profilesRepo->Save(profile)) {
        scanInstanceTuneNotice(this, QMessageBox::Warning, title, tr("Failed to store the profile."));
        return;
    }
    QStringList args;
    if (Configs::dataManager->settingsRepo->started_id == profileId || Configs::RunningUsesProfile(profileId))
        args << MwArg::RestartProxy;
    Configs::RepositionLater(QList<std::shared_ptr<Configs::Profile>>{profile}, Configs::SortInput::Config);
    MW_dialog_message(MwMessage::ProfileChanged, args);
    scanInstanceTuneNotice(this, QMessageBox::Information, title,
                           tr("\"%1\" now uses: %2.").arg(profileName, Scanner::Tune::Describe(assignment)));
}

void DialogScanInstance::saveTuneResultAsProfile() {
    const int index = selectedTuneTrial();
    if (index < 0 || Scanner::ScanManager::instance()->IsRunning(scanId)) return;
    const auto assignment = tuneTrials.at(index).assignment;
    const auto title = tr("Save as new profile");
    reloadRow();
    if (!scanInstanceTuneResultsMatch(*scan)) {
        scanInstanceTuneNotice(this, QMessageBox::Warning, title, scanInstanceTuneOtherProfile());
        return;
    }
    const int profileId = scan->config.tune.profileId;
    const auto profile = profileId >= 0 ? Configs::dataManager->profilesRepo->GetProfile(profileId) : nullptr;
    if (profile == nullptr || profile->outbound == nullptr) {
        scanInstanceTuneNotice(this, QMessageBox::Warning, title, tr("The profile of this search no longer exists."));
        return;
    }

    auto copy = Scanner::Tune::Copy(*profile);
    if (copy == nullptr) {
        scanInstanceTuneNotice(this, QMessageBox::Warning, title, tr("\"%1\" could not be copied.").arg(profile->outbound->name));
        return;
    }
    QString error;
    if (!Scanner::Tune::Apply(*copy, assignment, &error)) {
        scanInstanceTuneNotice(this, QMessageBox::Warning, title, tr("The settings could not be applied to the copy: %1.").arg(error));
        return;
    }
    const QString name = tr("%1 (tuned)").arg(profile->outbound->name);
    copy->name = name;
    copy->outbound->name = name;
    if (!Configs::dataManager->profilesRepo->AddProfile(copy, profile->gid)) {
        scanInstanceTuneNotice(this, QMessageBox::Warning, title, tr("Failed to store the profile."));
        return;
    }
    MW_dialog_message(MwMessage::ProfileChanged, {});
    scanInstanceTuneNotice(this, QMessageBox::Information, title,
                           tr("Saved \"%1\" with: %2.").arg(name, Scanner::Tune::Describe(assignment)));
}

void DialogScanInstance::copyTuneResults() {
    const int rows = tuneModel != nullptr ? tuneModel->rowCount() : 0;
    if (rows == 0) return;
    QStringList lines;
    QStringList header;
    for (int column = 0; column < kScanInstanceTuneColumns; ++column)
        header << scanInstanceTuneTsvCell(tuneModel->headerData(column, Qt::Horizontal).toString());
    lines << header.join(QLatin1Char('\t'));
    for (int row = 0; row < rows; ++row) {
        QStringList cells;
        for (int column = 0; column < kScanInstanceTuneColumns; ++column)
            cells << scanInstanceTuneTsvCell(tuneModel->index(row, column).data().toString());
        lines << cells.join(QLatin1Char('\t'));
    }
    QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    QToolTip::showText(QCursor::pos(), tr("Copied %n variant(s)", nullptr, rows), ui->copy_results);
}
