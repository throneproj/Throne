#include "include/ui/scanner/dialog_scan_instance.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QClipboard>
#include <QCursor>
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

    void scanInstanceTuneNumber(QLineEdit *edit, int minimum, int maximum, int defaultValue) {
        edit->setValidator(new QIntValidator(minimum, maximum, edit));
        edit->setPlaceholderText(QString::number(defaultValue));
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
    tuneUi = new Ui::ScanTuneSettings;
    auto *page = new QWidget(ui->phases_content);
    tuneUi->setupUi(page);
    layout->addWidget(page);

    tuneProfileNames = new QStringListModel(this);
    setupProfilePicker(tuneUi->profile, tuneProfileNames);
    tuneUi->profile->lineEdit()->setPlaceholderText(tr("Select a profile…"));
    scanInstanceTuneNumber(tuneUi->mtu_min, 576, 9000, tuneDefaults.mtuMin);
    tuneUi->mtu_max->setValidator(new QIntValidator(0, 9000, tuneUi->mtu_max));
    scanInstanceTuneNumber(tuneUi->mtu_step, 1, 1000, tuneDefaults.mtuStep);
    scanInstanceTuneNumber(tuneUi->mtu_bytes, 1000, 100000000, tuneDefaults.mtuBytes);
    scanInstanceTuneNumber(tuneUi->attempts, 1, 50, tuneDefaults.attempts);
    scanInstanceTuneNumber(tuneUi->per_minute, 1, 120, tuneDefaults.perMinute);
    scanInstanceTuneNumber(tuneUi->max_attempts, 1, 5000, tuneDefaults.maxAttempts);
    scanInstanceTuneNumber(tuneUi->max_minutes, 1, 1440, tuneDefaults.maxMinutes);
    scanInstanceTuneNumber(tuneUi->timeout, 100, 60000, tuneDefaults.timeoutMs);
    tuneUi->url->setPlaceholderText(Configs::dataManager->settingsRepo->test_latency_url);

    connect(tuneUi->profile, &QComboBox::currentTextChanged, this, [this] {
        if (!loading) updateTuneControls();
    });
    for (auto *box : {tuneUi->fingerprints, tuneUi->fragment, tuneUi->mtu}) connect(box, &QCheckBox::toggled, this, [this] { updateTuneControls(); });
    const auto updateMtuUrlHint = [this] {
        Configs::ScanTuneOptions options;
        bool ok = false;
        const int bytes = tuneUi->mtu_bytes->hasAcceptableInput() ? tuneUi->mtu_bytes->validator()->locale().toInt(tuneUi->mtu_bytes->text(), &ok) : 0;
        if (ok) options.mtuBytes = bytes;
        tuneUi->mtu_url->setPlaceholderText(Scanner::Tune::MtuTestUrl(options));
    };
    connect(tuneUi->mtu_bytes, &QLineEdit::textChanged, this, updateMtuUrlHint);
    updateMtuUrlHint();
}

void DialogScanInstance::bindTuneValidation() {
    form->bind(tuneUi->fingerprint_list, [](const QString &text) {
        QString error;
        (void) Scanner::Tune::ParseFingerprints(text, &error);
        return error;
    });
    form->bind(tuneUi->fragment_delays, [](const QString &text) {
        QString error;
        (void) Scanner::Tune::ParseDelays(text, &error);
        return error;
    });
    form->bind(tuneUi->mtu_url, Validation::Rules::Url({QStringLiteral("http"), QStringLiteral("https")}));
    form->bind(tuneUi->url, Validation::Rules::Url({QStringLiteral("http"), QStringLiteral("https")}));
}

void DialogScanInstance::loadTuneForm() {
    const auto &tune = config.tune;
    reloadTuneProfiles(tune.profileId);
    tuneUi->fingerprints->setChecked(tune.fingerprints);
    tuneUi->fingerprint_list->setText(tune.fingerprintList);
    tuneUi->fragment->setChecked(tune.fragment);
    tuneUi->fragment_delays->setText(tune.fragmentDelays);
    tuneUi->record_fragment->setChecked(tune.recordFragment);
    tuneUi->mixed_case_sni->setChecked(tune.mixedCaseSni);
    tuneUi->quic_opener->setChecked(tune.quicOpener);
    tuneUi->mtu->setChecked(tune.mtu);
    tuneUi->mtu_min->setText(QString::number(tune.mtuMin));
    tuneUi->mtu_max->setText(tune.mtuMax > 0 ? QString::number(tune.mtuMax) : QString());
    tuneUi->mtu_step->setText(QString::number(tune.mtuStep));
    tuneUi->mtu_url->setText(tune.mtuUrl);
    tuneUi->mtu_bytes->setText(QString::number(tune.mtuBytes));
    tuneUi->attempts->setText(QString::number(tune.attempts));
    tuneUi->per_minute->setText(QString::number(tune.perMinute));
    tuneUi->max_attempts->setText(QString::number(tune.maxAttempts));
    tuneUi->max_minutes->setText(QString::number(tune.maxMinutes));
    tuneUi->url->setText(tune.url);
    tuneUi->timeout->setText(QString::number(tune.timeoutMs));
    updateTuneControls();
}

void DialogScanInstance::reloadTuneProfiles(int selectId) {
    if (tuneUi == nullptr) return;
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

    tuneUi->profile->clear();
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
            if (tuneUi->profile->count() < tuneUi->profile->maxCount()) tuneUi->profile->addItem(display, id);
        }
    }
    tuneProfileNames->setStringList(names);

    const int index = selectId >= 0 ? tuneUi->profile->findData(selectId) : -1;
    if (index >= 0) tuneUi->profile->setCurrentIndex(index);
    else if (!selectedName.isEmpty()) tuneUi->profile->setCurrentText(selectedName);
    else tuneUi->profile->setCurrentIndex(-1);
    updateTuneControls();
}

int DialogScanInstance::selectedTuneProfile() const {
    if (tuneUi == nullptr) return -1;
    const QString text = tuneUi->profile->currentText();
    if (text.trimmed().isEmpty()) return -1;
    // The current item first: two profiles of one group can share a name.
    if (const int current = tuneUi->profile->currentIndex(); current >= 0 && tuneUi->profile->itemText(current) == text)
        return tuneUi->profile->itemData(current).toInt();
    if (const int index = tuneUi->profile->findText(text, Qt::MatchExactly); index >= 0) return tuneUi->profile->itemData(index).toInt();
    return tuneProfileIds.value(text, tuneProfileIds.value(text.trimmed(), -1));
}

void DialogScanInstance::updateTuneControls() {
    if (tuneUi == nullptr) return;
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
            quicOpenerWhy = tr("Needs QUIC, MASQUE over HTTP/3 or WireGuard.");
        if (space.mtuCurrent <= 0) mtuWhy = tr("Only WireGuard and AmneziaWG profiles have an MTU to search.");
    }

    // A disabled control keeps its value; the tooltip says why it is off.
    const auto mark = [](QWidget *control, const QString &why, const QString &tip, bool on) {
        control->setEnabled(why.isEmpty() && on);
        control->setToolTip(why.isEmpty() ? tip : why);
    };
    mark(tuneUi->fingerprints, fingerprintWhy, {}, true);
    mark(tuneUi->fingerprint_list, fingerprintWhy, tr("Comma-separated uTLS fingerprints; \"off\" tries Go's own TLS."),
         tuneUi->fingerprints->isChecked());
    mark(tuneUi->fragment, fragmentWhy, {}, true);
    mark(tuneUi->fragment_delays, delayWhy, tr("Comma-separated, e.g. 100ms, 500ms, 1s."), tuneUi->fragment->isChecked());
    mark(tuneUi->record_fragment, recordFragmentWhy, {}, true);
    mark(tuneUi->mixed_case_sni, mixedCaseSniWhy, {}, true);
    mark(tuneUi->quic_opener, quicOpenerWhy, tr("Sends a decoy packet before each UDP flow to get past QUIC blocking."), true);
    mark(tuneUi->mtu, mtuWhy, tr("Finds the largest MTU that still works."), true);
    const bool mtuOn = tuneUi->mtu->isChecked();
    for (QLineEdit *edit : {tuneUi->mtu_min, tuneUi->mtu_max, tuneUi->mtu_step}) mark(edit, mtuWhy, {}, mtuOn);
    mark(tuneUi->mtu_url, mtuWhy, tr("Downloaded to check each MTU. Empty: Cloudflare's speed test."), mtuOn);
    mark(tuneUi->mtu_bytes, mtuWhy, tr("How many bytes each MTU check downloads."), mtuOn);

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
    tuneUi->profile_hint->setText(hint);
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
    ui->copy_results->setToolTip(tr("Copies the table as tab-separated text."));

    connect(ui->apply_result, &QPushButton::clicked, this, [this] { applyTuneResult(); });
    connect(ui->save_result_as_profile, &QPushButton::clicked, this, [this] { saveTuneResultAsProfile(); });
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
    ui->apply_result->setEnabled(usable);
    ui->save_result_as_profile->setEnabled(usable);
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
