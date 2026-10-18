#include "include/ui/scanner/dialog_edit_ip_list.h"

#include <QCompleter>
#include <QDateTime>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIntValidator>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <optional>

#include "include/database/DatabaseManager.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/global/Utils.hpp"
#include "include/scanner/IpListParse.h"
#include "include/scanner/IpListUpdater.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/utils/ValidationForm.h"

namespace {
    constexpr int kEditIpListEditableEntries = 50000;
    constexpr int kEditIpListPreviewEntries = 1000;
    constexpr int kEditIpListLiveParseLines = 20000;
    constexpr int kEditIpListMinInterval = 30;
    constexpr int kEditIpListMaxInterval = 525600;

    bool editIpListIsHttpUrl(const QString &text) {
        return text.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
               text.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
    }

    // open(), not exec(): accept() also runs from the fetch's posted callback, where a nested loop is unsafe (#1874).
    void editIpListWarning(QWidget *parent, const QString &title, const QString &text) {
        auto *box = new QMessageBox(QMessageBox::Warning, title, text, QMessageBox::Ok, parent);
        // Fetch errors and rejected samples are foreign text; AutoText would render a tag-like one as HTML.
        box->setTextFormat(Qt::PlainText);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->open();
    }

    bool editIpListConfirm(QWidget *parent, const QString &title, const QString &text) {
        QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, parent);
        box.setTextFormat(Qt::PlainText);
        return box.exec() == QMessageBox::Yes;
    }

    QString editIpListRejectedText(const int rejected, const QStringList &samples) {
        QString text = DialogEditIpList::tr("%Ln unreadable", nullptr, rejected);
        if (!samples.isEmpty()) text += QStringLiteral(" (") + samples.join(QStringLiteral(", ")) + QStringLiteral(")");
        return text;
    }

    std::optional<int> editIpListInterval(const QString &input) {
        const auto text = input.trimmed();
        if (text.isEmpty()) return Configs::IpList().update_interval;
        bool ok = false;
        // QLocale, not QString::toInt: the validator lets locale digits and group separators through.
        const int minutes = QLocale().toInt(text, &ok);
        if (!ok || minutes < kEditIpListMinInterval || minutes > kEditIpListMaxInterval) return std::nullopt;
        return minutes;
    }
}

DialogEditIpList::DialogEditIpList(QWidget *parent, const int listId, const Configs::IpList::SourceKind kind,
                                   const QString &prefillText)
    : QDialog(parent), ui(new Ui::DialogEditIpList) {
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);

    ui->entries->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    ui->managed_note->setTextFormat(Qt::PlainText);
    ui->managed_note->hide();
    ui->summary->setTextFormat(Qt::PlainText);
    ui->update_interval->setValidator(new QIntValidator(kEditIpListMinInterval, kEditIpListMaxInterval, ui->update_interval));
    ui->update_interval->setPlaceholderText(QString::number(Configs::IpList().update_interval));
    setupRuleSetPicker();
    form->bind(ui->url, [this](const QString &text) { return sourceError(Configs::IpList::SourceKind::Url, text); });
    form->bind(ui->rule_set, [this](const QString &text) { return sourceError(Configs::IpList::SourceKind::RuleSet, text); });
    form->bind(ui->update_interval, [](const QString &text) {
        if (editIpListInterval(text)) return QString();
        return tr("Must be from %1 to %2 minutes.").arg(kEditIpListMinInterval).arg(kEditIpListMaxInterval);
    }).label(ui->label_auto_update->text());

    summaryTimer = new QTimer(this);
    summaryTimer->setSingleShot(true);
    summaryTimer->setInterval(400);
    connect(summaryTimer, &QTimer::timeout, this, [this] { updateSummary(); });

    if (listId >= 0) {
        loadList(listId);
    } else {
        list = Configs::IpListsRepo::NewIpList();
        setWindowTitle(tr("New IP list"));
        ui->source_kind->setCurrentIndex(static_cast<int>(kind));
        ui->auto_update->setChecked(list->auto_update);
        ui->update_interval->setText(QString::number(std::max(kEditIpListMinInterval, list->update_interval)));
        if (kind == Configs::IpList::SourceKind::Manual && !prefillText.isEmpty()) {
            ui->entries->setPlainText(prefillText);
        }
    }

    connect(ui->source_kind, &QComboBox::currentIndexChanged, this, [this] {
        ui->name->setPlaceholderText(defaultName());
        updateControls();
        updateSummary();
    });
    connect(ui->url, &QLineEdit::textChanged, this, [this] {
        ui->name->setPlaceholderText(defaultName());
        summaryTimer->start();
    });
    connect(ui->rule_set, &QComboBox::currentTextChanged, this, [this] {
        ui->name->setPlaceholderText(defaultName());
        summaryTimer->start();
    });
    connect(ui->auto_update, &QCheckBox::toggled, this, [this] { updateControls(); });
    connect(ui->entries, &QPlainTextEdit::textChanged, this, [this] { summaryTimer->start(); });
    connect(ui->fetch, &QPushButton::clicked, this, [this] { startFetch(false); });
    connect(themeManager(), &ThemeManager::themeChanged, this, [this] { applyColors(); });

    ui->name->setPlaceholderText(defaultName());
    applyColors();
    updateControls();
    if (!missing) updateSummary();
    if (!scanOwned && !missing) form->markAll();

    if (isNew && kind == Configs::IpList::SourceKind::Url) ui->url->setFocus();
    else if (isNew && kind == Configs::IpList::SourceKind::RuleSet) ui->rule_set->setFocus();
    else ui->name->setFocus();

    const auto *scr = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
    if (scr != nullptr) resize(sizeHint().expandedTo(QSize(560, 480)).boundedTo(scr->availableGeometry().size()));
}

DialogEditIpList::~DialogEditIpList() {
    delete ui;
}

Configs::IpList::SourceKind DialogEditIpList::currentKind() const {
    return static_cast<Configs::IpList::SourceKind>(std::clamp(ui->source_kind->currentIndex(), 0, 2));
}

QString DialogEditIpList::currentSource() const {
    switch (currentKind()) {
        case Configs::IpList::SourceKind::Url:
            return ui->url->text().trimmed();
        case Configs::IpList::SourceKind::RuleSet:
            return ui->rule_set->currentText().trimmed();
        case Configs::IpList::SourceKind::Manual:
            break;
    }
    return {};
}

QString DialogEditIpList::sourceError(const Configs::IpList::SourceKind kind, const QString &source) const {
    if (kind == Configs::IpList::SourceKind::Url) {
        if (!editIpListIsHttpUrl(source) || QUrl(source).host().isEmpty()) return tr("Enter an http(s) URL.");
        return {};
    }
    if (kind == Configs::IpList::SourceKind::RuleSet) {
        if (source.isEmpty()) return tr("Choose a rule-set.");
        if (editIpListIsHttpUrl(source)) return QUrl(source).host().isEmpty() ? tr("Enter an http(s) URL.") : QString();
        if (Scanner::BuiltinRuleSetUrl(source).isEmpty())
            return tr("Unknown rule-set \"%1\". Pick a geoip-* name from the list or enter a rule-set URL.").arg(source);
    }
    return {};
}

QString DialogEditIpList::defaultName() const {
    const auto source = currentSource();
    QString name;
    if (!source.isEmpty()) {
        if (editIpListIsHttpUrl(source)) {
            const QUrl url(source);
            name = QFileInfo(url.fileName()).completeBaseName();
            if (name.isEmpty()) name = url.host();
        } else {
            name = source;
        }
    }
    return name.isEmpty() ? tr("IP list") : name;
}

bool DialogEditIpList::fetchMatchesInputs() const {
    return hasFetch && fetchedKind == currentKind() && fetchedSource == currentSource();
}

void DialogEditIpList::loadList(const int listId) {
    auto *repo = Configs::dataManager->ipListsRepo.get();
    for (const auto &header : repo->GetAllIpLists(true)) {
        if (header->id == listId) {
            list = header;
            break;
        }
    }
    setWindowTitle(tr("Edit IP list"));
    if (!list) {
        missing = true;
        list = Configs::IpListsRepo::NewIpList();
        setSummary(tr("This list no longer exists."), true);
        return;
    }

    isNew = false;
    scanOwned = list->role != Configs::IpList::Role::User;
    if (scanOwned) setWindowTitle(tr("IP list"));

    ui->name->setText(list->name);
    ui->source_kind->setCurrentIndex(static_cast<int>(list->source_kind));
    if (list->source_kind == Configs::IpList::SourceKind::Url) ui->url->setText(list->source);
    if (list->source_kind == Configs::IpList::SourceKind::RuleSet) ui->rule_set->setCurrentText(list->source);
    ui->auto_update->setChecked(list->auto_update);
    ui->update_interval->setText(QString::number(std::max(kEditIpListMinInterval, list->update_interval)));

    const int total = list->entryCount;
    const bool preview = total > kEditIpListEditableEntries;
    showEntries(repo->GetEntries(list->id, 0, preview ? kEditIpListPreviewEntries : -1), total, true);

    if (scanOwned) {
        QString owner;
        if (const auto scan = Configs::dataManager->ipScansRepo->GetIpScan(list->related_test_id)) owner = scan->name;
        ui->managed_note->setText(owner.isEmpty() ? tr("Managed by a scan that no longer exists.")
                                                  : tr("Managed by scan %1").arg(owner));
        ui->managed_note->show();
        ui->buttonBox->setStandardButtons(QDialogButtonBox::Close);
    }
}

void DialogEditIpList::setupRuleSetPicker() {
    const QSignalBlocker blocker(ui->rule_set);
    ui->rule_set->addItems(Scanner::BuiltinIpRuleSets());
    ui->rule_set->setMaxVisibleItems(15);

    auto *completer = new QCompleter(ui->rule_set->model(), this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    ui->rule_set->setCompleter(completer);

    ui->rule_set->setCurrentIndex(-1);
    ui->rule_set->lineEdit()->setPlaceholderText(tr("geoip-ir, or a rule-set URL"));
}

void DialogEditIpList::showEntries(const QList<Configs::IpListEntry> &entries, const int total, const bool fromDb) {
    const bool preview = total > kEditIpListEditableEntries;
    const auto shown = preview ? std::min<qsizetype>(kEditIpListPreviewEntries, entries.size()) : entries.size();

    QString text;
    text.reserve(shown * 20);
    for (qsizetype i = 0; i < shown; ++i) {
        text += Scanner::FormatEntry(entries[i]);
        text += QLatin1Char('\n');
    }
    if (!text.isEmpty()) text.chop(1);

    {
        const QSignalBlocker blocker(ui->entries);
        ui->entries->setPlainText(text);
        ui->entries->document()->setModified(false);
    }
    editorTotal = total;
    editorTruncated = preview;
    editorFromDb = fromDb;
}

void DialogEditIpList::updateControls() {
    const auto kind = currentKind();
    const bool remote = kind != Configs::IpList::SourceKind::Manual;
    const bool editable = !scanOwned && !missing;
    const bool inputsEnabled = editable && !fetching && !saving;

    ui->label_url->setVisible(kind == Configs::IpList::SourceKind::Url);
    ui->url->setVisible(kind == Configs::IpList::SourceKind::Url);
    ui->label_rule_set->setVisible(kind == Configs::IpList::SourceKind::RuleSet);
    ui->rule_set->setVisible(kind == Configs::IpList::SourceKind::RuleSet);
    ui->label_auto_update->setVisible(remote && editable);
    ui->auto_update_box->setVisible(remote && editable);
    ui->fetch->setVisible(remote && editable);

    ui->name->setReadOnly(!editable);
    ui->source_kind->setEnabled(inputsEnabled);
    ui->url->setReadOnly(!inputsEnabled);
    ui->rule_set->setEnabled(inputsEnabled);
    ui->update_interval->setEnabled(ui->auto_update->isChecked());
    ui->fetch->setEnabled(!fetching && !saving);
    ui->fetch->setText(fetching ? tr("Fetching…") : tr("Fetch now"));
    ui->entries->setReadOnly(!editable || remote || editorTruncated);

    if (auto *ok = ui->buttonBox->button(QDialogButtonBox::Ok)) ok->setEnabled(!fetching && !saving && !missing);
}

void DialogEditIpList::updateSummary() {
    if (missing || fetching) return;

    const auto kind = currentKind();
    if (!scanOwned && kind == Configs::IpList::SourceKind::Manual && !editorTruncated) {
        const auto *document = ui->entries->document();
        if (document->isEmpty()) {
            setSummary(tr("No entries yet."), false);
            return;
        }
        if (document->blockCount() > kEditIpListLiveParseLines) {
            setSummary(tr("%Ln lines", nullptr, document->blockCount()), false);
            return;
        }
        const auto parsed = Scanner::ParseIpListText(ui->entries->toPlainText().toUtf8());
        QStringList parts{tr("%Ln entries", nullptr, static_cast<int>(parsed.entries.size()))};
        if (parsed.duplicates > 0) parts << tr("%Ln duplicates", nullptr, parsed.duplicates);
        if (parsed.rejected > 0) parts << editIpListRejectedText(parsed.rejected, parsed.samples);
        setSummary(parts.join(QStringLiteral(" · ")), parsed.rejected > 0);
        return;
    }

    if (kind != Configs::IpList::SourceKind::Manual && fetchMatchesInputs()) {
        QStringList parts{tr("Fetched %Ln entries", nullptr, static_cast<int>(fetched.size()))};
        if (editorTruncated) parts << tr("showing the first %Ln", nullptr, kEditIpListPreviewEntries);
        if (fetchedRejected > 0) parts << editIpListRejectedText(fetchedRejected, fetchedSamples);
        setSummary(parts.join(QStringLiteral(" · ")), false);
        return;
    }

    const bool sourceChanged = isNew || kind != list->source_kind ||
                               (kind != Configs::IpList::SourceKind::Manual && currentSource() != list->source);
    if (kind != Configs::IpList::SourceKind::Manual && sourceChanged && !scanOwned) {
        setSummary(tr("The entries are fetched when you press Fetch now or OK."), false);
        return;
    }

    QStringList parts{tr("%Ln entries", nullptr, editorTotal)};
    if (editorTruncated) parts << tr("showing the first %Ln", nullptr, kEditIpListPreviewEntries);
    bool isError = false;
    if (list->IsRemote() && kind != Configs::IpList::SourceKind::Manual) {
        if (list->last_update > 0)
            parts << tr("updated %1").arg(QLocale().toString(QDateTime::fromSecsSinceEpoch(list->last_update),
                                                             QLocale::ShortFormat));
        if (!list->last_error.isEmpty()) {
            parts << tr("last update failed: %1").arg(list->last_error);
            isError = true;
        }
    }
    setSummary(parts.join(QStringLiteral(" · ")), isError);
}

void DialogEditIpList::setSummary(const QString &text, const bool isError) {
    summaryIsError = isError;
    ui->summary->setText(text);
    applyColors();
}

void DialogEditIpList::applyColors() {
    const auto &tokens = themeManager()->tokens;
    const auto summarySheet = QStringLiteral("color: %1;").arg((summaryIsError ? tokens.danger : tokens.muted).name());
    const auto noteSheet = QStringLiteral("color: %1;").arg(tokens.muted.name());
    // setStyleSheet repolishes even when unchanged.
    if (ui->summary->styleSheet() != summarySheet) ui->summary->setStyleSheet(summarySheet);
    if (ui->managed_note->styleSheet() != noteSheet) ui->managed_note->setStyleSheet(noteSheet);
}

void DialogEditIpList::startFetch(const bool thenAccept) {
    if (fetching) return;
    const auto kind = currentKind();
    const auto source = currentSource();
    if (kind == Configs::IpList::SourceKind::Manual) return;
    if (const auto error = sourceError(kind, source); !error.isEmpty()) {
        if (thenAccept) editIpListWarning(this, windowTitle(), error);
        setSummary(error, true);
        return;
    }

    fetching = true;
    acceptAfterFetch = thenAccept;
    updateControls();
    setSummary(tr("Fetching…"), false);

    QPointer<DialogEditIpList> self(this);
    runOnNewThread([self, kind, source] {
        const auto outcome = Scanner::FetchEntries(kind, source);
        runOnUiThread([self, kind, source, outcome] {
            if (self) self->onFetched(kind, source, outcome);
        });
    });
}

void DialogEditIpList::onFetched(const Configs::IpList::SourceKind kind, const QString &source,
                                 const Scanner::ImportOutcome &outcome) {
    fetching = false;
    const bool thenAccept = acceptAfterFetch;
    acceptAfterFetch = false;

    if (!outcome.error.isEmpty()) {
        updateControls();
        setSummary(tr("Fetch failed: %1").arg(outcome.error), true);
        if (thenAccept)
            editIpListWarning(this, windowTitle(),
                              tr("Could not fetch the list:\n%1").arg(outcome.error));
        return;
    }

    fetched = outcome.entries;
    fetchedRejected = outcome.rejected;
    fetchedSamples = outcome.samples;
    fetchedKind = kind;
    fetchedSource = source;
    hasFetch = true;
    showEntries(fetched, static_cast<int>(fetched.size()), false);
    updateControls();
    updateSummary();

    if (thenAccept && fetchMatchesInputs()) accept();
}

void DialogEditIpList::accept() {
    if (scanOwned || missing) {
        QDialog::reject();
        return;
    }
    if (fetching || saving) return;
    if (!form->validateAll()) return;

    const auto kind = currentKind();
    const bool remote = kind != Configs::IpList::SourceKind::Manual;
    const auto source = remote ? currentSource() : QString();
    const bool autoUpdate = remote && ui->auto_update->isChecked();
    const auto interval = editIpListInterval(ui->update_interval->text());
    auto *updater = Scanner::IpListUpdater::instance();
    if (!isNew && updater->IsRefreshing(list->id)) {
        editIpListWarning(this, windowTitle(),
                          tr("This list is being refreshed. Save again when the refresh has finished."));
        return;
    }

    const bool sourceChanged = isNew || kind != list->source_kind || source != list->source;
    QList<Configs::IpListEntry> entries;
    bool writeEntries = false;
    bool fromFetch = false;
    if (!remote) {
        if (!editorTruncated) {
            if (isNew || !editorFromDb || ui->entries->document()->isModified()) {
                const auto parsed = Scanner::ParseIpListText(ui->entries->toPlainText().toUtf8());
                if (parsed.rejected > 0) {
                    QString text = tr("%Ln entries could not be read and will be dropped.", nullptr, parsed.rejected);
                    if (!parsed.samples.isEmpty())
                        text += "\n" + tr("For example: %1").arg(parsed.samples.join(QStringLiteral(", ")));
                    text += "\n\n" + tr("Save the list without them?");
                    if (!editIpListConfirm(this, windowTitle(), text)) return;
                }
                entries = parsed.entries;
                writeEntries = true;
            }
        } else if (!editorFromDb && hasFetch) {
            entries = fetched;
            writeEntries = true;
        }
    } else if (fetchMatchesInputs()) {
        entries = fetched;
        writeEntries = true;
        fromFetch = true;
    } else if (sourceChanged) {
        startFetch(true);
        return;
    }

    auto candidate = std::make_shared<Configs::IpList>(*list);
    const auto typedName = ui->name->text().trimmed();
    candidate->name = typedName.isEmpty() ? defaultName() : typedName;
    if (sourceChanged) candidate->last_update = 0;
    candidate->source_kind = kind;
    candidate->source = source;
    candidate->auto_update = autoUpdate;
    if (interval) candidate->update_interval = *interval;
    if (fromFetch) {
        candidate->last_update = QDateTime::currentSecsSinceEpoch();
        candidate->last_error.clear();
    }
    if (!remote) candidate->last_error.clear();

    if (isNew) {
        candidate->entries = entries;
        candidate->entryCount = static_cast<int>(entries.size());
        candidate->entriesLoaded = true;
    }

    saving = true;
    updateControls();
    QPointer<DialogEditIpList> self(this);
    runOnNewThread([self, candidate, entries, writeEntries, isNewList = isNew]() mutable {
        auto *repo = Configs::dataManager->ipListsRepo.get();
        const bool stored = isNewList ? repo->AddIpList(candidate)
                                      : repo->SaveHeader(candidate) &&
                                            (!writeEntries || repo->ReplaceEntries(candidate->id, entries));
        if (stored) Scanner::IpListUpdater::instance()->NotifyListsChanged();
        runOnUiThread([self, candidate, stored] {
            if (self) self->finishSave(candidate, stored);
        });
    });
}

void DialogEditIpList::finishSave(const std::shared_ptr<Configs::IpList> &candidate, const bool stored) {
    saving = false;
    updateControls();
    if (!stored) {
        editIpListWarning(this, windowTitle(), tr("Failed to store the IP list."));
        return;
    }
    list = candidate;
    savedId = candidate->id;
    QDialog::accept();
}

void DialogEditIpList::reject() {
    if (saving) return;
    QDialog::reject();
}
