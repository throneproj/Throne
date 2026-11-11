#include "include/ui/setting/ProcessPicker.h"

#include <algorithm>

#include <QAbstractTableModel>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QSet>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QVBoxLayout>

#include "include/api/RPC.h"
#include "include/configs/ProcessRules.h"
#include "include/database/DatabaseManager.h"
#include "include/database/TrafficStatsRepo.h"
#include "include/global/Utils.hpp"
#include "include/ui/utils/NativeFileDialog.h"
#ifdef Q_OS_WIN
#include "include/sys/windows/guihelper.h"
#endif

namespace ProcessRules = Configs::ProcessRules;

struct ProcessPickerProgram {
    QString name;
    QString path;
    QStringList services;
    // Why a browsed file may not be what connects.
    QString note;
    bool browsed = false;
    bool connectedNow = false;
    int instances = 0;
    // Epoch seconds; 0 = never seen on the network.
    qint64 lastSeen = 0;

    [[nodiscard]] bool systemOnly() const {
        if (browsed || connectedNow || lastSeen > 0) return false;
        return path.isEmpty() || !services.isEmpty() || ProcessRules::IsSystemProgram(path);
    }
};

namespace {
    struct ProcessPickerSnapshot {
        QList<ProcessPickerProgram> programs;
        QString problem;
    };

    QString processPickerAgo(qint64 epochSecs) {
        const qint64 ago = std::max<qint64>(0, QDateTime::currentSecsSinceEpoch() - epochSecs);
        if (ago < 60) return ProcessPicker::tr("just now");
        if (ago < 3600) return ProcessPicker::tr("%n min ago", nullptr, static_cast<int>(ago / 60));
        if (ago < 86400) return ProcessPicker::tr("%n h ago", nullptr, static_cast<int>(ago / 3600));
        return ProcessPicker::tr("%n d ago", nullptr, static_cast<int>(ago / 86400));
    }

    QString processPickerStatus(const ProcessPickerProgram& program) {
        QStringList parts;
        if (!program.services.isEmpty()) parts << ProcessPicker::tr("Service: %1").arg(program.services.join(QStringLiteral(", ")));
        if (program.browsed) parts << ProcessPicker::tr("Chosen file");
        if (program.connectedNow) parts << ProcessPicker::tr("Connected now");
        else if (program.lastSeen > 0) parts << ProcessPicker::tr("Seen %1").arg(processPickerAgo(program.lastSeen));
        if (program.instances > 1 && program.services.isEmpty()) parts << ProcessPicker::tr("Running (%1)").arg(program.instances);
        else if (program.instances > 0) parts << ProcessPicker::tr("Running");
        return parts.join(QStringLiteral(" · "));
    }

    // Runs off the UI thread.
    ProcessPickerSnapshot processPickerCollect() {
        QHash<QString, ProcessPickerProgram> byKey;
        const auto entry = [&byKey](const QString& name, const QString& path) -> ProcessPickerProgram& {
            auto& program = byKey[path.isEmpty() ? QStringLiteral("n:") + name : QStringLiteral("p:") + path];
            if (program.name.isEmpty()) {
                program.name = name;
                program.path = path;
            }
            return program;
        };

        ProcessPickerSnapshot snapshot;
        if (API::defaultClient == nullptr || !API::defaultClient->IsConnected()) {
            snapshot.problem = ProcessPicker::tr("The core is not connected, so running programs are not listed.");
        } else {
            const qint64 now = QDateTime::currentSecsSinceEpoch();
            const auto connections = API::defaultClient->QueryConnections();
            for (const auto& conn : connections.active) {
                const QString name = QString::fromStdString(conn.process.value_or(""));
                if (name.isEmpty()) continue;
                auto& program = entry(name, QString::fromStdString(conn.process_path.value_or("")));
                program.connectedNow = true;
                program.lastSeen = now;
            }
            for (const auto& conn : connections.closed) {
                const QString name = QString::fromStdString(conn.process.value_or(""));
                if (name.isEmpty()) continue;
                auto& program = entry(name, QString::fromStdString(conn.process_path.value_or("")));
                program.lastSeen = std::max<qint64>(program.lastSeen, conn.closed_at.value_or(0) / 1000);
            }

            bool rpcOK = false;
            const auto reply = API::defaultClient->ListProcesses(&rpcOK);
            if (!rpcOK) {
                snapshot.problem = ProcessPicker::tr("The core did not answer, so running programs are not listed.");
            } else {
                if (const QString error = QString::fromStdString(reply.error.value_or("")); !error.isEmpty())
                    snapshot.problem = ProcessPicker::tr("Some programs could not be listed: %1").arg(error);
                for (const auto& process : reply.processes) {
                    const QString name = QString::fromStdString(process.name.value_or(""));
                    if (name.isEmpty()) continue;
                    auto& program = entry(name, QString::fromStdString(process.path.value_or("")));
                    ++program.instances;
                    const QString service = QString::fromStdString(process.service.value_or(""));
                    if (!service.isEmpty() && !program.services.contains(service)) program.services << service;
                }
            }
        }

        if (Configs::dataManager != nullptr && Configs::dataManager->trafficStatsRepo != nullptr) {
            for (const auto& row : Configs::dataManager->trafficStatsRepo->GetAllAppMeta()) {
                if (row.process_name.isEmpty() || row.last_path.isEmpty()) continue;
                auto& program = entry(row.process_name, row.last_path);
                program.lastSeen = std::max(program.lastSeen, row.last_seen);
            }
        }

        snapshot.programs = byKey.values();
        std::sort(snapshot.programs.begin(), snapshot.programs.end(), [](const ProcessPickerProgram& a, const ProcessPickerProgram& b) {
            if (a.connectedNow != b.connectedNow) return a.connectedNow;
            if (a.lastSeen != b.lastSeen) return a.lastSeen > b.lastSeen;
            if (const int byName = a.name.compare(b.name, Qt::CaseInsensitive); byName != 0) return byName < 0;
            return a.path < b.path;
        });

        QStringList names;
        QStringList paths;
        for (const auto& program : std::as_const(snapshot.programs)) {
            names << program.name;
            paths << program.path;
        }
        ProcessRules::Learn(names, paths);
        return snapshot;
    }

    QString processPickerCanonical(QString file, QString* note) {
#ifdef Q_OS_MACOS
        if (file.endsWith(QStringLiteral(".app"), Qt::CaseInsensitive) && QFileInfo(file).isDir()) {
            const QSettings info(file + QStringLiteral("/Contents/Info.plist"), QSettings::NativeFormat);
            const QString executable = info.value(QStringLiteral("CFBundleExecutable")).toString();
            if (!executable.isEmpty()) {
                file += QStringLiteral("/Contents/MacOS/") + executable;
                *note = ProcessPicker::tr("Apps like %1 often connect from helper programs inside the bundle; match by app folder to catch them too.")
                            .arg(executable);
            }
        }
#endif
#ifdef Q_OS_WIN
        Q_UNUSED(note)
        if (const QString final = Windows_FinalPath(file); !final.isEmpty()) return final;
        return QDir::toNativeSeparators(QFileInfo(file).absoluteFilePath());
#else
        const QFileInfo info(file);
        const QString canonical = info.canonicalFilePath();
        QFile handle(canonical.isEmpty() ? file : canonical);
        if (handle.open(QIODevice::ReadOnly) && handle.read(2) == "#!")
            *note = ProcessPicker::tr("%1 is a script: what connects is the interpreter that runs it, which other scripts share.").arg(info.fileName());
        return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
#endif
    }

    QIcon processPickerIcon(const QString& path) {
        // Never destroyed: icons must not outlive the application object at exit.
        static auto* provider = new QFileIconProvider;
        static auto* cache = new QHash<QString, QIcon>;
        const auto cached = cache->constFind(path);
        if (cached != cache->cend()) return *cached;
        const QIcon found = path.isEmpty() ? provider->icon(QAbstractFileIconProvider::File) : provider->icon(QFileInfo(path));
        cache->insert(path, found);
        return found;
    }

    // UI thread only.
    struct ProcessCatalog {
        QList<ProcessPickerProgram> programs;
        QElapsedTimer age;
        bool loading = false;
        QList<QPointer<ProcessCompletionModel>> models;
    };

    ProcessCatalog& processCatalog() {
        static ProcessCatalog catalog;
        return catalog;
    }

    void processCatalogRequest() {
        auto& catalog = processCatalog();
        if (catalog.loading || (catalog.age.isValid() && catalog.age.elapsed() < 30000)) return;
        catalog.loading = true;
        runOnNewThread([] {
            auto snapshot = processPickerCollect();
            runOnUiThread([programs = std::move(snapshot.programs)] {
                auto& catalog = processCatalog();
                catalog.loading = false;
                catalog.age.start();
                catalog.programs = programs;
                catalog.models.removeIf([](const QPointer<ProcessCompletionModel>& model) { return model.isNull(); });
                for (const auto& model : std::as_const(catalog.models)) model->rebuild();
            });
        });
    }
}

class ProcessPickerModel : public QAbstractTableModel {
public:
    enum Column { ColProgram, ColStatus, ColPath, ColumnCount };

    using QAbstractTableModel::QAbstractTableModel;

    void setPrograms(QList<ProcessPickerProgram> programs) {
        for (const auto& row : std::as_const(rows)) {
            if (!row.browsed) continue;
            const auto same = std::find_if(programs.begin(), programs.end(), [&row](const ProcessPickerProgram& p) { return p.path == row.path; });
            if (same == programs.end()) {
                programs.prepend(row);
            } else {
                same->browsed = true;
                same->note = row.note;
            }
        }
        beginResetModel();
        rows = std::move(programs);
        endResetModel();
    }

    int addBrowsed(ProcessPickerProgram program) {
        for (int i = 0; i < rows.size(); ++i) {
            if (rows[i].path != program.path) continue;
            rows[i].browsed = true;
            rows[i].note = program.note;
            emit dataChanged(index(i, 0), index(i, ColumnCount - 1));
            return i;
        }
        beginInsertRows({}, 0, 0);
        rows.prepend(std::move(program));
        endInsertRows();
        return 0;
    }

    [[nodiscard]] const ProcessPickerProgram& at(int row) const { return rows.at(row); }

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override {
        return parent.isValid() ? 0 : static_cast<int>(rows.size());
    }

    [[nodiscard]] int columnCount(const QModelIndex& parent) const override {
        return parent.isValid() ? 0 : ColumnCount;
    }

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
        switch (section) {
            case ColProgram: return ProcessPicker::tr("Program");
            case ColStatus: return ProcessPicker::tr("Activity");
            case ColPath: return ProcessPicker::tr("Path");
            default: return {};
        }
    }

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= rows.size()) return {};
        const auto& program = rows.at(index.row());
        switch (role) {
            case Qt::DisplayRole:
                if (index.column() == ColProgram) return program.name;
                if (index.column() == ColStatus) return processPickerStatus(program);
                return program.path;
            case Qt::ToolTipRole:
                if (index.column() == ColStatus) return processPickerStatus(program);
                return program.path.isEmpty() ? ProcessPicker::tr("The core could not read where this program is.") : program.path;
            case Qt::DecorationRole:
                if (index.column() == ColProgram) return processPickerIcon(program.path);
                return {};
            default:
                return {};
        }
    }

private:
    QList<ProcessPickerProgram> rows;
};

class ProcessPickerFilter : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void set(bool everything, const QString& text) {
        showSystem = everything;
        needle = text.trimmed();
        invalidateRowsFilter();
    }

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex&) const override {
        const auto& program = static_cast<const ProcessPickerModel*>(sourceModel())->at(sourceRow);
        if (!showSystem && program.systemOnly()) return false;
        if (needle.isEmpty()) return true;
        return program.name.contains(needle, Qt::CaseInsensitive) || program.path.contains(needle, Qt::CaseInsensitive) ||
               std::any_of(program.services.begin(), program.services.end(), [this](const QString& s) { return s.contains(needle, Qt::CaseInsensitive); });
    }

private:
    bool showSystem = false;
    QString needle;
};

ProcessPicker::ProcessPicker(QWidget* parent, const QList<Match>& allowed) : ProcessPicker(parent, allowed, false) {}

ProcessPicker::ProcessPicker(QWidget* parent, const QList<Match>& allowed, bool oneProgram) : QDialog(parent), chooseOne(oneProgram) {
    setWindowTitle(chooseOne ? tr("Choose a Program") : tr("Add Programs"));

    model = new ProcessPickerModel(this);
    filter = new ProcessPickerFilter(this);
    filter->setSourceModel(model);

    search = new QLineEdit(this);
    search->setPlaceholderText(tr("Search by name or path"));
    search->setClearButtonEnabled(true);
    scope = new QComboBox(this);
    scope->addItem(tr("Programs"), false);
    scope->addItem(tr("Everything, with services and system programs"), true);
    refreshButton = new QPushButton(tr("Refresh"), this);

    view = new QTreeView(this);
    view->setModel(filter);
    view->setRootIsDecorated(false);
    view->setUniformRowHeights(true);
    view->setAllColumnsShowFocus(true);
    view->setSelectionMode(chooseOne ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setTextElideMode(Qt::ElideMiddle);
    view->header()->setStretchLastSection(true);
    view->header()->setSectionsMovable(false);

    status = new QLabel(this);
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    status->hide();

    matchGroup = new QButtonGroup(this);
    auto* matchRow = new QHBoxLayout;
    auto* matchLabel = new QLabel(tr("Match by:"), this);
    matchRow->addWidget(matchLabel);
    for (const Match kind : allowed) {
        QRadioButton* radio = nullptr;
        switch (kind) {
            case Match::Name:
                radio = new QRadioButton(tr("Program name"), this);
                radio->setToolTip(tr("Matches the program wherever it is installed (process_name)."));
                break;
            case Match::Path:
                radio = new QRadioButton(tr("Exact path"), this);
                radio->setToolTip(tr("Matches only this file (process_path)."));
                break;
            case Match::Folder:
                radio = new QRadioButton(tr("App folder"), this);
                radio->setToolTip(tr("Matches every program in the app's folder, helpers and later versions included (process_path_regex)."));
                break;
        }
        matchGroup->addButton(radio, static_cast<int>(kind));
        matchRow->addWidget(radio);
    }
    matchRow->addStretch(1);
    if (!allowed.isEmpty()) matchGroup->button(static_cast<int>(allowed.first()))->setChecked(true);
    if (allowed.size() < 2) {
        matchLabel->hide();
        for (auto* button : matchGroup->buttons()) button->hide();
    }

    summary = new QLabel(this);
    summary->setTextFormat(Qt::PlainText);
    summary->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    addButton = buttons->button(QDialogButtonBox::Ok);
    addButton->setText(chooseOne ? tr("Choose") : tr("Add"));
    auto* browseButton = buttons->addButton(tr("Browse…"), QDialogButtonBox::ActionRole);

    auto* topRow = new QHBoxLayout;
    topRow->addWidget(search, 1);
    topRow->addWidget(scope);
    topRow->addWidget(refreshButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(view, 1);
    layout->addWidget(status);
    layout->addLayout(matchRow);
    layout->addWidget(summary);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browseButton, &QPushButton::clicked, this, &ProcessPicker::browse);
    connect(refreshButton, &QPushButton::clicked, this, &ProcessPicker::reload);
    connect(search, &QLineEdit::textChanged, this, &ProcessPicker::applyFilter);
    connect(scope, &QComboBox::currentIndexChanged, this, &ProcessPicker::applyFilter);
    connect(matchGroup, &QButtonGroup::idClicked, this, &ProcessPicker::updateSummary);
    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ProcessPicker::updateSummary);
    connect(view, &QTreeView::doubleClicked, this, [this] {
        if (chooseOne ? !chosenProgram().isEmpty() : !values().isEmpty()) accept();
    });

    applyFilter();
    updateSummary();
    const auto* screen = parent != nullptr && parent->screen() != nullptr ? parent->screen() : QGuiApplication::primaryScreen();
    const QSize preferred(fontMetrics().averageCharWidth() * 110, fontMetrics().height() * 30);
    resize(screen != nullptr ? preferred.boundedTo(screen->availableGeometry().size() * 9 / 10) : preferred);
    reload();
}

ProcessPicker::Match ProcessPicker::match() const {
    return static_cast<Match>(matchGroup->checkedId());
}

QList<int> ProcessPicker::selectedRows() const {
    QList<int> proxyRows;
    for (const auto& index : view->selectionModel()->selectedRows()) proxyRows << index.row();
    std::sort(proxyRows.begin(), proxyRows.end());
    QList<int> rows;
    rows.reserve(proxyRows.size());
    for (const int row : std::as_const(proxyRows)) rows << filter->mapToSource(filter->index(row, 0)).row();
    return rows;
}

QStringList ProcessPicker::values() const {
    const Match kind = match();
    QStringList out;
    for (const int row : selectedRows()) {
        const auto& program = model->at(row);
        QString value;
        switch (kind) {
            case Match::Name: value = program.name; break;
            case Match::Path: value = program.path; break;
            case Match::Folder: value = ProcessRules::FolderPattern(program.path); break;
        }
        if (!value.isEmpty() && !out.contains(value)) out << value;
    }
    return out;
}

QString ProcessPicker::chosenProgram() const {
    const QList<int> rows = selectedRows();
    if (rows.isEmpty()) return {};
    const auto& program = model->at(rows.first());
    return program.path.isEmpty() ? program.name : program.path;
}

QStringList ProcessPicker::Pick(QWidget* parent, const QList<Match>& allowed, Match* match) {
    ProcessPicker dialog(parent, allowed);
    if (dialog.exec() != QDialog::Accepted) return {};
    if (match != nullptr) *match = dialog.match();
    return dialog.values();
}

QString ProcessPicker::ChooseProgram(QWidget* parent) {
    ProcessPicker dialog(parent, {Match::Path}, true);
    if (dialog.exec() != QDialog::Accepted) return {};
    return dialog.chosenProgram();
}

void ProcessPicker::reload() {
    const int token = ++generation;
    refreshButton->setEnabled(false);
    status->setText(tr("Looking for programs…"));
    status->show();
    QPointer<ProcessPicker> self(this);
    runOnNewThread([self, token] {
        auto snapshot = processPickerCollect();
        runOnUiThread([self, token, snapshot]() mutable {
            if (self == nullptr || token != self->generation) return;
            self->refreshButton->setEnabled(true);
            self->model->setPrograms(std::move(snapshot.programs));
            self->status->setText(snapshot.problem);
            self->status->setVisible(!snapshot.problem.isEmpty());
            self->fitColumns();
            self->updateSummary();
        });
    });
}

void ProcessPicker::browse() {
#if defined(Q_OS_WIN)
    const QString filters = tr("Programs (*.exe);;All files (*)");
    const QString start = qEnvironmentVariable("ProgramFiles");
#elif defined(Q_OS_MACOS)
    const QString filters = tr("Applications (*.app);;All files (*)");
    const QString start = QStringLiteral("/Applications");
#else
    const QString filters;
    const QString start = QStringLiteral("/usr/bin");
#endif
    const QString chosen = NativeFileDialog::getOpenFileName(this, tr("Choose a Program"), start, filters);
    if (chosen.isEmpty()) return;

    ProcessPickerProgram program;
    program.browsed = true;
    program.path = processPickerCanonical(chosen, &program.note);
    program.name = QFileInfo(program.path).fileName();
    const int row = model->addBrowsed(std::move(program));

    search->clear();
    const QModelIndex shown = filter->mapFromSource(model->index(row, 0));
    view->selectionModel()->select(shown, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    view->scrollTo(shown);
    updateSummary();
}

void ProcessPicker::applyFilter() {
    filter->set(scope->currentData().toBool(), search->text());
}

void ProcessPicker::fitColumns() {
    const QFontMetrics metrics(view->font());
    const int padding = metrics.averageCharWidth() * 3;
    int nameWidth = metrics.horizontalAdvance(model->headerData(ProcessPickerModel::ColProgram, Qt::Horizontal, Qt::DisplayRole).toString());
    int statusWidth = metrics.horizontalAdvance(model->headerData(ProcessPickerModel::ColStatus, Qt::Horizontal, Qt::DisplayRole).toString());
    const int measured = std::min(model->rowCount({}), 400);
    for (int row = 0; row < measured; ++row) {
        nameWidth = std::max(nameWidth, metrics.horizontalAdvance(model->at(row).name));
        statusWidth = std::max(statusWidth, metrics.horizontalAdvance(processPickerStatus(model->at(row))));
    }
    const int iconWidth = view->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, view);
    const int cap = std::max(view->viewport()->width() / 3, metrics.averageCharWidth() * 24);
    view->header()->resizeSection(ProcessPickerModel::ColProgram, std::min(nameWidth + iconWidth + padding, cap));
    view->header()->resizeSection(ProcessPickerModel::ColStatus, std::min(statusWidth + padding, cap));
}

void ProcessPicker::updateSummary() {
    if (chooseOne) {
        const QString chosen = chosenProgram();
        addButton->setEnabled(!chosen.isEmpty());
        summary->setText(chosen.isEmpty() ? tr("Select a program, or browse for one.") : chosen);
        return;
    }
    const Match kind = match();
    const QList<int> rows = selectedRows();
    const QStringList chosen = values();
    addButton->setEnabled(!chosen.isEmpty());

    QStringList lines;
    if (rows.isEmpty()) {
        lines << tr("Select one or more programs, or browse for one.");
    } else if (!chosen.isEmpty()) {
        constexpr int shown = 5;
        QString list = chosen.first(std::min<qsizetype>(shown, chosen.size())).join(QStringLiteral(", "));
        if (chosen.size() > shown) list += u' ' + tr("and %n more", nullptr, static_cast<int>(chosen.size() - shown));
        lines << tr("Adds: %1").arg(list);
    }

    QStringList warnings;
    for (const int row : rows) {
        const auto& program = model->at(row);
        if (!program.note.isEmpty()) warnings << program.note;
        if (ProcessRules::IsSharedHost(program.name))
            warnings << tr("%1 runs many unrelated programs, so the rule catches all of them.").arg(program.name);
        if (kind == Match::Name) continue;
        if (program.path.isEmpty()) {
            warnings << tr("The core could not read where %1 is, so it can only be matched by name.").arg(program.name);
        } else if (kind == Match::Path && ProcessRules::HasVersionedFolder(program.path)) {
            warnings << tr("The path of %1 has a version number in it, so the rule stops matching after an update.").arg(program.name);
        } else if (kind == Match::Folder) {
            QString folder;
            ProcessRules::FolderPattern(program.path, &folder);
            if (ProcessRules::IsSharedFolder(folder)) warnings << tr("%1 holds many other programs, and the rule catches all of them.").arg(folder);
        }
    }
    warnings.removeDuplicates();
    if (warnings.size() > 3) {
        warnings.resize(3);
        warnings << tr("More warnings for the other selected programs.");
    }
    summary->setText((lines + warnings).join(u'\n'));
}

ProcessCompletionModel::ProcessCompletionModel(Layout layout, QObject* parent) : QAbstractListModel(parent), config(std::move(layout)) {
    processCatalog().models << this;
    rebuild();
    processCatalogRequest();
}

int ProcessCompletionModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(config.head.size() + programs.size() + config.tail.size());
}

QVariant ProcessCompletionModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};
    qsizetype row = index.row();
    const bool text = role == Qt::DisplayRole || role == Qt::EditRole;
    if (row < config.head.size()) return text ? QVariant(config.head.at(row)) : QVariant();
    row -= config.head.size();
    if (row < programs.size()) {
        const auto& suggestion = programs.at(row);
        if (text) return suggestion.text;
        // Lazy file icon engines: sizing every row is cheap, the shell lookup happens only for painted rows.
        if (role == Qt::DecorationRole && !suggestion.path.isEmpty()) return processPickerIcon(suggestion.path);
        if (role == Qt::ToolTipRole && !suggestion.tip.isEmpty()) return suggestion.tip;
        return {};
    }
    row -= programs.size();
    if (row < config.tail.size()) return text ? QVariant(config.tail.at(row)) : QVariant();
    return {};
}

void ProcessCompletionModel::rebuild() {
    QList<Suggestion> next;
    QSet<QString> names;
    QSet<QString> paths;
    for (const auto& program : std::as_const(processCatalog().programs)) {
        if (program.systemOnly()) continue;
        if (config.names && !program.name.isEmpty() && !names.contains(program.name)) {
            names.insert(program.name);
            next << Suggestion{config.namePrefix + program.name, program.path, program.path};
        }
        if (config.paths && !program.path.isEmpty() && !paths.contains(program.path)) {
            paths.insert(program.path);
            next << Suggestion{config.pathPrefix + program.path, program.path, program.path};
        }
    }
    if (next == programs) return;
    beginResetModel();
    programs = std::move(next);
    endResetModel();
}
