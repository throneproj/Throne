#include "include/ui/profile/edit_tor.h"

#include "include/api/RPC.h"
#include "include/configs/outbounds/tor.h"
#include "include/engine/services/AssetUpdates.h"
#include "include/engine/services/TorBridges.h"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {
    // The bridge service has settings of its own for these.
    const QStringList kEditTorServiceCountries = {"by", "cn", "eg", "hk", "ir", "mm", "ru", "tm"};

    constexpr int kEditTorTypeRole = Qt::UserRole;
    constexpr int kEditTorBuiltinRole = Qt::UserRole + 1;
    constexpr int kEditTorLinesRole = Qt::UserRole + 2;

    struct EditTorChoice {
        QString type;
        bool builtin = false;
        QStringList bridges;
    };

    void editTorSelect(QComboBox *combo, const QString &value, const QString &fallbackText) {
        int index = combo->findData(value);
        if (index < 0) {
            combo->addItem(fallbackText, value);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index);
    }

    // Every territory with a two-letter code, by name; supranational codes left out.
    QList<QPair<QString, QString>> editTorCountries(const QStringList &skip) {
        static const QStringList notCountries = {"eu", "ez", "qo", "un"};
        QList<QPair<QString, QString>> countries;
        for (int value = QLocale::AnyTerritory + 1; value <= QLocale::LastTerritory; ++value) {
            const auto territory = static_cast<QLocale::Territory>(value);
            const auto code = QLocale::territoryToCode(territory).toLower();
            if (code.size() != 2 || !code[0].isLetter() || !code[1].isLetter()) continue;
            if (skip.contains(code) || notCountries.contains(code)) continue;
            countries.append({QLocale::territoryToString(territory), code});
        }
        std::sort(countries.begin(), countries.end(), [](const auto &a, const auto &b) { return a.first.localeAwareCompare(b.first) < 0; });
        return countries;
    }

    QLabel *editTorMutedLabel(QWidget *parent) {
        auto *label = new QLabel(parent);
        label->setProperty("colorRole", QStringLiteral("muted"));
        label->setWordWrap(true);
        return label;
    }

    void editTorShowAnswer(const libcore::TorMoatSettingsResponse &reply, bool automatic, QListWidget *settings, QLabel *status, QLabel *note) {
        status->clear();
        for (const auto &setting : reply.settings) {
            const auto type = QString::fromStdString(setting.type.value_or(""));
            const bool builtin = setting.source.value_or("") == "builtin";
            QStringList lines;
            for (const auto &line : setting.bridges) lines << QString::fromStdString(line);
            auto *item = new QListWidgetItem(Configs::TorSettingLabel(type, builtin, int(lines.size())), settings);
            item->setData(kEditTorTypeRole, type);
            item->setData(kEditTorBuiltinRole, builtin);
            item->setData(kEditTorLinesRole, lines);
        }
        if (settings->count() > 0) settings->setCurrentRow(0);
        else status->setText(EditTor::tr("The bridge service suggested nothing for this country."));

        QStringList notes;
        const auto answered = QString::fromStdString(reply.country.value_or(""));
        if (reply.offline.value_or(false)) {
            notes << (answered.isEmpty() ? EditTor::tr("The bridge service is unreachable; showing the built-in list.")
                                         : EditTor::tr("The bridge service is unreachable; showing the built-in list for %1.")
                                               .arg(Configs::TorCountryName(answered)));
        } else if (!answered.isEmpty()) {
            notes << EditTor::tr("Answered for %1.").arg(Configs::TorCountryName(answered));
        }
        if (automatic && reply.via_proxy.value_or(false))
            notes << EditTor::tr("Asked through the running proxy, so Automatic used the proxy's location.");
        note->setText(notes.join(u' '));
        note->setVisible(!notes.isEmpty());
    }

    // Window-modal and asynchronous: chosen runs once, on OK, with the selected setting.
    void editTorOpenBridgesDialog(QWidget *parent, const std::function<void(const EditTorChoice &)> &chosen) {
        auto *dialog = new QDialog(parent);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(EditTor::tr("Get bridges"));
        auto *layout = new QVBoxLayout(dialog);

        auto *country = new QComboBox(dialog);
        country->setToolTip(EditTor::tr("Where the bridges have to work; Automatic lets the service tell from your connection."));
        country->addItem(EditTor::tr("Automatic"), QString());
        for (const auto &code : kEditTorServiceCountries) country->addItem(Configs::TorCountryName(code), code);
        country->insertSeparator(country->count());
        for (const auto &[name, code] : editTorCountries(kEditTorServiceCountries)) country->addItem(name, code);
        auto *countryForm = new QFormLayout;
        auto *countryLabel = new QLabel(EditTor::tr("Country"), dialog);
        countryLabel->setBuddy(country);
        countryForm->addRow(countryLabel, country);
        layout->addLayout(countryForm);

        auto *requestRow = new QHBoxLayout;
        auto *request = new QPushButton(EditTor::tr("Request"), dialog);
        request->setToolTip(EditTor::tr("Ask the Tor Project's bridge service for bridges that work in this country."));
        auto *status = editTorMutedLabel(dialog);
        requestRow->addWidget(request);
        requestRow->addWidget(status, 1);
        layout->addLayout(requestRow);

        auto *settings = new QListWidget(dialog);
        layout->addWidget(settings);
        auto *note = editTorMutedLabel(dialog);
        note->hide();
        layout->addWidget(note);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
        auto *okButton = buttons->button(QDialogButtonBox::Ok);
        okButton->setEnabled(false);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        QObject::connect(settings, &QListWidget::currentRowChanged, okButton, [okButton](int row) { okButton->setEnabled(row >= 0); });
        QObject::connect(settings, &QListWidget::itemDoubleClicked, dialog, &QDialog::accept);
        QObject::connect(dialog, &QDialog::accepted, dialog, [settings, chosen] {
            const auto *item = settings->currentItem();
            if (item == nullptr) return;
            chosen({item->data(kEditTorTypeRole).toString(), item->data(kEditTorBuiltinRole).toBool(),
                    item->data(kEditTorLinesRole).toStringList()});
        });
        QObject::connect(request, &QPushButton::clicked, dialog, [=] {
            request->setEnabled(false);
            country->setEnabled(false);
            settings->clear();
            note->hide();
            status->setText(EditTor::tr("Asking the bridge service; this can take a minute."));
            const QString code = country->currentData().toString();
            const QPointer<QDialog> guard(dialog);
            runOnNewThread([=] {
                libcore::TorMoatSettingsRequest moatRequest;
                moatRequest.country = code.toStdString();
                moatRequest.bridges_document = Engine::TorBridges::NewerDocument().toStdString();
                bool rpcOK = false;
                QString coreError;
                libcore::TorMoatSettingsResponse reply;
                if (API::defaultClient != nullptr && API::defaultClient->IsConnected())
                    reply = API::defaultClient->TorMoatSettings(&rpcOK, moatRequest, &coreError);
                runOnUiThread([=] {
                    if (guard == nullptr) return;
                    request->setEnabled(true);
                    country->setEnabled(true);
                    if (!rpcOK) {
                        status->setText(EditTor::tr("The core did not answer: %1").arg(coreError.isEmpty() ? QStringLiteral("IPC error") : coreError));
                        return;
                    }
                    if (const auto error = QString::fromStdString(reply.error.value_or("")); !error.isEmpty()) {
                        status->setText(error);
                        return;
                    }
                    editTorShowAnswer(reply, code.isEmpty(), settings, status, note);
                });
            });
        });
        dialog->open();
    }
}

EditTor::EditTor(QWidget *parent) : QWidget(parent), ui(new Ui::EditTor), modeButtons(new QButtonGroup(this)) {
    ui->setupUi(this);

    // Ids are indexes into Configs::torBridgeModes.
    modeButtons->addButton(ui->bridges_none, 0);
    modeButtons->addButton(ui->bridges_builtin, 1);
    modeButtons->addButton(ui->bridges_custom, 2);
    for (const auto &type : Configs::torBuiltinTypes) ui->builtin_type->addItem(Configs::TorTypeLabel(type), type);
    ui->bridges->setVisibleLines(5);

    connect(modeButtons, &QButtonGroup::idToggled, this, [this](int, bool checked) {
        if (checked) updateBridgeRows();
    });
    connect(ui->get_bridges, &QPushButton::clicked, this, &EditTor::getBridges);
    connect(ui->copy_bridges, &QPushButton::clicked, this, &EditTor::copyBridges);
    connect(ui->download_tor, &QPushButton::clicked, this, &EditTor::downloadTor);

    Engine::TorBridges::RefreshInBackground();
}

EditTor::~EditTor() {
    delete ui;
}

void EditTor::FillExitCountries(QComboBox *combo, const QString &current) {
    combo->clear();
    combo->addItem(tr("Any"), QString());
    for (const auto &code : Configs::torExitCountries) combo->addItem(Configs::TorCountryName(code), code);
    editTorSelect(combo, current, Configs::TorCountryName(current));
}

QString EditTor::bridgeMode() const {
    return Configs::torBridgeModes.value(modeButtons->checkedId(), QStringLiteral("builtin"));
}

QStringList EditTor::customLines() const {
    return Configs::SplitTorBridgeText(ui->bridges->toPlainText());
}

void EditTor::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    const auto *outbound = this->ent->Tor();

    const auto mode = Configs::torBridgeModes.indexOf(outbound->bridge_mode);
    if (auto *button = modeButtons->button(mode >= 0 ? int(mode) : 1)) button->setChecked(true);
    editTorSelect(ui->builtin_type, outbound->builtin_type, outbound->builtin_type);
    ui->bridges->setPlainText(outbound->bridges.join(u'\n'));
    FillExitCountries(ui->exit_country, outbound->exit_country);
    showTorStatus();
    updateBridgeRows();
}

bool EditTor::onEnd() {
    auto *outbound = this->ent->Tor();
    outbound->bridge_mode = bridgeMode();
    outbound->builtin_type = ui->builtin_type->currentData().toString();
    outbound->bridges = customLines();
    outbound->exit_country = ui->exit_country->currentData().toString();
    return true;
}

void EditTor::bindValidation(Validation::Form &form) {
    // Hidden unless the mode is Custom, and the form skips hidden fields.
    form.bind(ui->bridges, Configs::Checks::TorBridgeText, this);
}

void EditTor::updateBridgeRows() {
    const auto mode = bridgeMode();
    ui->builtin_type->setVisible(mode == QLatin1String("builtin"));
    ui->bridges->setVisible(mode == QLatin1String("custom"));
    ui->copy_bridges->setEnabled(mode != QLatin1String("none"));
    if (editor_state_changed) editor_state_changed();
}

void EditTor::showTorStatus(const QString &text) {
    const bool installed = Engine::AssetUpdates::TorInstalled();
    ui->tor_status->setText(!text.isEmpty() ? text : installed ? tr("Tor is installed.") : tr("Tor is not downloaded yet."));
    ui->download_tor->setVisible(!installed);
}

void EditTor::getBridges() {
    const QPointer<EditTor> self(this);
    editTorOpenBridgesDialog(this, [self](const EditTorChoice &choice) {
        if (self == nullptr) return;
        if (choice.builtin && Configs::torBuiltinTypes.contains(choice.type)) {
            self->ui->bridges_builtin->setChecked(true);
            editTorSelect(self->ui->builtin_type, choice.type, Configs::TorTypeLabel(choice.type));
        } else {
            self->ui->bridges_custom->setChecked(true);
            self->ui->bridges->setPlainText(choice.bridges.join(u'\n'));
        }
        self->updateBridgeRows();
    });
}

void EditTor::copyBridges() {
    const auto mode = bridgeMode();
    QStringList lines;
    if (mode == QLatin1String("custom")) lines = customLines();
    else if (mode == QLatin1String("builtin")) lines = Engine::TorBridges::BuiltinLines(ui->builtin_type->currentData().toString());
    QString feedback;
    if (!lines.isEmpty()) {
        QApplication::clipboard()->setText(lines.join(u'\n'));
        feedback = tr("Copied %n bridge line(s).", nullptr, int(lines.size()));
    } else {
        feedback = mode == QLatin1String("builtin") ? Configs::TorBuiltinBridgesUnavailable() : tr("There are no bridge lines to copy.");
    }
    QToolTip::showText(ui->copy_bridges->mapToGlobal(QPoint(0, ui->copy_bridges->height())), feedback, ui->copy_bridges);
}

void EditTor::downloadTor() {
    const QPointer<EditTor> self(this);
    const bool started = Engine::AssetUpdates::InstallTor([self](const QString &error) {
        if (self == nullptr) return;
        self->ui->download_tor->setEnabled(true);
        self->showTorStatus(error.isEmpty() ? QString() : tr("Tor download failed: %1").arg(error));
        if (self->editor_state_changed) self->editor_state_changed();
    });
    if (!started) {
        showTorStatus(tr("A Tor download is already running."));
        return;
    }
    ui->download_tor->setEnabled(false);
    showTorStatus(tr("Downloading Tor."));
}
