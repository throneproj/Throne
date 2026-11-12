#include "include/ui/setting/dialog_otp_manager.h"

#include <QBuffer>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>

#include "3rdparty/QrDecoder.h"
#include "3rdparty/qrcodegen.hpp"
#include "include/database/DatabaseManager.h"
#include "include/database/OtpProfilesRepo.h"
#include "include/global/Utils.hpp"
#include "include/ui/setting/OtpItem.h"
#include "include/ui/setting/OtpListWidget.h"
#include "include/ui/setting/dialog_edit_otp.h"
#include "include/ui/utils/ScreenQrScanner.h"
#include "include/ui/utils/NativeFileDialog.h"

namespace {
    constexpr qint64 MAX_IMPORT_FILE_SIZE = 50 * 1024 * 1024;

    QImage RenderQr(const QString &text, bool &ok) {
        constexpr int padding = 2;
        ok = false;
        try {
            const auto qr = qrcodegen::QrCode::encodeText(text.toUtf8().data(), qrcodegen::QrCode::Ecc::MEDIUM);
            const int size = qr.getSize();
            QImage image(size + padding * 2, size + padding * 2, QImage::Format_RGB32);
            image.fill(qRgb(255, 255, 255));
            for (int y = 0; y < size; ++y)
                for (int x = 0; x < size; ++x)
                    if (qr.getModule(x, y)) image.setPixel(x + padding, y + padding, qRgb(0, 0, 0));
            ok = true;
            return image;
        } catch (const std::exception &) {
            return {};
        }
    }

    void ShowQrDialog(QWidget *parent, const QString &title, const QString &text) {
        bool ok = false;
        const auto image = RenderQr(text, ok);

        QDialog dialog(parent);
        dialog.setWindowTitle(title);
        auto *layout = new QVBoxLayout(&dialog);

        if (ok) {
            auto *label = new QLabel(&dialog);
            label->setPixmap(QPixmap::fromImage(image.scaled(320, 320, Qt::KeepAspectRatio, Qt::FastTransformation)));
            label->setAlignment(Qt::AlignCenter);
            layout->addWidget(label);
        } else {
            auto *label = new QLabel(QObject::tr("Too much data to fit in a QR code."), &dialog);
            label->setWordWrap(true);
            layout->addWidget(label);
        }

        auto *view = new QPlainTextEdit(text, &dialog);
        view->setReadOnly(true);
        view->setMaximumHeight(120);
        layout->addWidget(view);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        auto *copy = buttons->addButton(QObject::tr("Copy"), QDialogButtonBox::ActionRole);
        QObject::connect(copy, &QPushButton::clicked, &dialog, [text] { QGuiApplication::clipboard()->setText(text); });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);

        dialog.exec();
    }

    QString AskForText(QWidget *parent, const QString &title, const QString &hint) {
        QDialog dialog(parent);
        dialog.setWindowTitle(title);
        dialog.resize(520, 300);
        auto *layout = new QVBoxLayout(&dialog);

        auto *label = new QLabel(hint, &dialog);
        label->setWordWrap(true);
        layout->addWidget(label);

        auto *edit = new QPlainTextEdit(&dialog);
        layout->addWidget(edit);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);

        if (dialog.exec() != QDialog::Accepted) return {};
        return edit->toPlainText();
    }

    QString otpManagerEntryLabel(const OTP::Entry &entry) {
        if (!entry.name.isEmpty()) return entry.name;
        if (!entry.issuer.isEmpty()) return entry.issuer;
        return DialogOtpManager::tr("Unnamed secret");
    }

    // Imported names are foreign text, so plain text only; the full list goes to the details.
    void otpManagerImportNotice(QWidget *parent, QMessageBox::Icon icon, const QString &summary, const QStringList &reasons) {
        constexpr int shown = 10;
        QString text = summary;
        if (!reasons.isEmpty()) {
            text += QStringLiteral("\n\n") + DialogOtpManager::tr("Not imported:") + QLatin1Char('\n') + reasons.mid(0, shown).join(QLatin1Char('\n'));
            if (reasons.size() > shown) text += QLatin1Char('\n') + DialogOtpManager::tr("…and %n more", nullptr, int(reasons.size() - shown));
        }
        auto *box = new QMessageBox(icon, DialogOtpManager::tr("Import OTP"), text, QMessageBox::Ok, parent);
        box->setTextFormat(Qt::PlainText);
        if (reasons.size() > shown) box->setDetailedText(reasons.join(QLatin1Char('\n')));
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->open();
    }
}

DialogOtpManager::DialogOtpManager(QWidget *parent) : QDialog(parent), ui(new Ui::DialogOtpManager) {
    ui->setupUi(this);
    setupOtpList();

    otpTimer = new QTimer(this);
    otpTimer->setInterval(1000);
    connect(otpTimer, &QTimer::timeout, this, [this] { refreshOtpCodes(); });
    otpTimer->start();
}

DialogOtpManager::~DialogOtpManager() {
    delete ui;
}

void DialogOtpManager::setupOtpList() {
    auto *pasteShortcut = new QShortcut(QKeySequence::Paste, this);
    pasteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteShortcut, &QShortcut::activated, this, [this] { importOtpFromClipboard(); });

    connect(ui->otp_list, &QListWidget::itemClicked, this, [this](const QListWidgetItem *item) {
        const int row = ui->otp_list->row(item);
        if (row >= 0 && row < otpProfiles.size()) copyOtpCode(otpProfiles[row]);
    });
    connect(ui->otp_list, &OtpListWidget::reorderRequested, this, &DialogOtpManager::moveOtpProfile);

    reloadOtpProfiles();
}

void DialogOtpManager::reloadOtpProfiles() {
    otpProfiles = Configs::dataManager->otpProfilesRepo->GetAllOtpProfiles();

    ui->otp_list->clear();
    for (const auto &profile : otpProfiles) {
        auto *item = new QListWidgetItem(ui->otp_list);
        auto *widget = new OtpItem(ui->otp_list, profile, item);
        connect(widget, &OtpItem::editRequested, this, [this, profile] { editOtpProfile(profile); });
        connect(widget, &OtpItem::exportRequested, this, [this, profile, widget] { showOtpExportMenu(profile, widget); });
        connect(widget, &OtpItem::deleteRequested, this, [this, profile] { deleteOtpProfile(profile); });
        ui->otp_list->setItemWidget(item, widget);
    }
}

void DialogOtpManager::refreshOtpCodes() const {
    for (int row = 0; row < ui->otp_list->count(); ++row) {
        if (auto *widget = qobject_cast<OtpItem *>(ui->otp_list->itemWidget(ui->otp_list->item(row))))
            widget->Refresh();
    }
}

void DialogOtpManager::copyOtpCode(const std::shared_ptr<Configs::OtpProfile> &profile) const {
    const auto code = profile->CurrentCode();
    if (code.isEmpty()) {
        MessageBoxWarning(tr("OTP"), tr("No usable code; check the secret."));
        return;
    }
    QGuiApplication::clipboard()->setText(code);
    QToolTip::showText(QCursor::pos(), tr("Copied"), ui->otp_list);
}

void DialogOtpManager::editOtpProfile(const std::shared_ptr<Configs::OtpProfile> &profile) {
    DialogEditOtp dialog(this, profile);
    if (dialog.exec() != QDialog::Accepted) return;

    Configs::dataManager->otpProfilesRepo->Save(profile);
    reloadOtpProfiles();
}

void DialogOtpManager::deleteOtpProfile(const std::shared_ptr<Configs::OtpProfile> &profile) {
    if (QMessageBox::question(this, tr("Confirmation"),
                              tr("Delete \"%1\"? Its secret cannot be recovered.").arg(profile->DisplayName()))
        != QMessageBox::StandardButton::Yes)
        return;

    Configs::dataManager->otpProfilesRepo->DeleteOtpProfile(profile->id);
    reloadOtpProfiles();
}

void DialogOtpManager::moveOtpProfile(int from, int to) {
    if (from < 0 || from >= otpProfiles.size() || to < 0 || to >= otpProfiles.size() || from == to) return;

    otpProfiles.move(from, to);
    QList<int> ids;
    ids.reserve(otpProfiles.size());
    for (const auto &profile : otpProfiles) ids.append(profile->id);
    Configs::dataManager->otpProfilesRepo->UpdateOtpProfilesOrder(ids);

    reloadOtpProfiles();
}

void DialogOtpManager::addOtpProfile() {
    auto profile = Configs::OtpProfilesRepo::NewOtpProfile();
    DialogEditOtp dialog(this, profile);
    if (dialog.exec() != QDialog::Accepted) return;

    if (!Configs::dataManager->otpProfilesRepo->AddOtpProfile(profile)) {
        MessageBoxWarning(tr("OTP"), tr("Failed to store the OTP profile."));
        return;
    }
    reloadOtpProfiles();
}

void DialogOtpManager::showOtpExportMenu(const std::shared_ptr<Configs::OtpProfile> &profile, QWidget *anchor) {
    const auto all = otpProfiles;

    QMenu menu(this);
    menu.addAction(tr("This one as otpauth:// link and QR"), this, [this, profile] { exportOtpAsLink(profile); });
    menu.addSeparator();
    menu.addAction(tr("All as otpauth-migration:// link and QR"), this, [this, all] { exportOtpAsMigration(all); });
    menu.addAction(tr("All as JSON file..."), this, [this, all] { exportOtpAsJson(all); });
    menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
}

void DialogOtpManager::on_otp_scan_clicked() {
    importOtpFromScreen();
}

void DialogOtpManager::on_otp_import_clicked() {
    QMenu menu(this);
    menu.addAction(tr("Add manually..."), this, [this] { addOtpProfile(); });
    menu.addSeparator();
    menu.addAction(tr("From link or text..."), this, [this] {
        const auto text = AskForText(this, tr("Import OTP"),
                                     tr("Paste otpauth:// links, an otpauth-migration:// link, a JSON export, "
                                        "or just a base32 secret."));
        if (!text.trimmed().isEmpty()) importOtpFromText(text);
    });
    menu.addAction(tr("From clipboard"), this, [this] { importOtpFromClipboard(); });
    menu.addAction(tr("From QR image file..."), this, [this] { importOtpFromFiles(); });
    menu.exec(ui->otp_import->mapToGlobal(QPoint(0, ui->otp_import->height())));
}

void DialogOtpManager::importOtpFromScreen() {
    bool captured = false;
    const auto texts = ScreenQr::ScanScreens(this, captured);

    if (!captured) {
        MessageBoxWarning(tr("Import OTP"), tr("Unable to capture screen"));
        return;
    }
    if (texts.isEmpty()) {
        MessageBoxWarning(tr("Import OTP"), tr("QR Code not found"));
        return;
    }
    importOtpFromText(texts.join("\n"));
}

void DialogOtpManager::importOtpFromText(const QString &text) {
    QStringList problems;
    const auto entries = OTP::ParseAny(text, &problems);
    importOtpEntries(entries, problems);
}

void DialogOtpManager::importOtpFromClipboard() {
    const auto *clipboard = QGuiApplication::clipboard();

    if (const auto image = clipboard->image(); !image.isNull()) {
        const auto texts = QrDecoder().decode(image.convertToFormat(QImage::Format_Grayscale8));
        if (texts.isEmpty()) {
            MessageBoxWarning(tr("Import OTP"), tr("No QR code found in the clipboard image."));
            return;
        }
        importOtpFromText(QStringList(texts.begin(), texts.end()).join("\n"));
        return;
    }

    const auto text = clipboard->text();
    if (text.trimmed().isEmpty()) {
        MessageBoxWarning(tr("Import OTP"), tr("Clipboard is empty."));
        return;
    }
    importOtpFromText(text);
}

void DialogOtpManager::importOtpFromFiles() {
    const auto paths = NativeFileDialog::getOpenFileNames(this, tr("Import OTP"), QDir::homePath(),
                                                          tr("QR images and exports (*.png *.jpg *.jpeg *.bmp *.gif *.json *.txt);;All files (*)"));
    if (paths.isEmpty()) return;

    QStringList payloads;
    QStringList problems;
    for (const auto &path : paths) {
        const QFileInfo info(path);
        QFile file(path);
        if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
            problems << tr("%1: cannot be opened").arg(info.fileName());
            continue;
        }
        if (file.size() > MAX_IMPORT_FILE_SIZE) {
            file.close();
            problems << tr("%1: larger than 50 MB, skipped").arg(info.fileName());
            continue;
        }
        const auto bytes = file.readAll();
        file.close();

        // By content, so a QR screenshot saved without a suffix still decodes.
        QBuffer buffer;
        buffer.setData(bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        reader.setDecideFormatFromContent(true);
        if (reader.canRead()) {
            const auto image = reader.read();
            const auto texts = image.isNull()
                                   ? QVector<QString>{}
                                   : QrDecoder().decode(image.convertToFormat(QImage::Format_Grayscale8));
            if (texts.isEmpty()) {
                problems << tr("%1: no QR code found").arg(info.fileName());
                continue;
            }
            for (const auto &text : texts) payloads << text.trimmed();
            continue;
        }

        payloads << QString::fromUtf8(bytes);
    }

    QList<OTP::Entry> entries;
    for (const auto &payload : payloads) entries += OTP::ParseAny(payload, &problems);
    importOtpEntries(entries, problems);
}

void DialogOtpManager::importOtpEntries(const QList<OTP::Entry> &entries, const QStringList &problems) {
    if (entries.isEmpty()) {
        otpManagerImportNotice(this, QMessageBox::Warning, problems.isEmpty() ? tr("Nothing to import.") : tr("Nothing could be imported."), problems);
        return;
    }

    const auto existing = Configs::dataManager->otpProfilesRepo->GetAllOtpProfiles();
    QStringList takenNames;
    for (const auto &other : existing) takenNames << other->name;

    int added = 0;
    QStringList reasons = problems;
    for (const auto &entry : entries) {
        if (const auto problem = OTP::Validate(entry); !problem.isEmpty()) {
            reasons << QStringLiteral("%1: %2").arg(otpManagerEntryLabel(entry), problem);
            continue;
        }
        // Same secret and name is the same account; a bare secret arrives unnamed, so secret alone decides.
        const bool duplicate = std::any_of(existing.begin(), existing.end(), [&entry](const auto &other) {
            return other->secret == entry.secret && (entry.name.isEmpty() || other->name == entry.name);
        });
        if (duplicate) {
            reasons << tr("%1: already in the list.").arg(otpManagerEntryLabel(entry));
            continue;
        }

        auto profile = std::make_shared<Configs::OtpProfile>(entry);
        if (profile->name.isEmpty()) {
            profile->name = tr("OTP");
            for (int n = 2; takenNames.contains(profile->name); ++n) profile->name = tr("OTP %1").arg(n);
        }
        takenNames << profile->name;
        if (Configs::dataManager->otpProfilesRepo->AddOtpProfile(profile)) ++added;
        else reasons << tr("%1: could not be stored.").arg(otpManagerEntryLabel(entry));
    }

    reloadOtpProfiles();
    otpManagerImportNotice(this, added > 0 ? QMessageBox::Information : QMessageBox::Warning,
                           tr("Imported %1 OTP profile(s).").arg(added), reasons);
}

void DialogOtpManager::exportOtpAsLink(const std::shared_ptr<Configs::OtpProfile> &profile) {
    ShowQrDialog(this, tr("Export OTP"), profile->ExportToLink());
}

void DialogOtpManager::exportOtpAsMigration(const QList<std::shared_ptr<Configs::OtpProfile>> &profiles) {
    QList<OTP::Entry> entries;
    for (const auto &profile : profiles) entries.append(*profile);

    const auto link = OTP::ExportToMigrationLink(entries);
    if (link.isEmpty()) {
        MessageBoxWarning(tr("Export OTP"), tr("None of the selected profiles could be exported."));
        return;
    }
    ShowQrDialog(this, tr("Export OTP"), link);
}

void DialogOtpManager::exportOtpAsJson(const QList<std::shared_ptr<Configs::OtpProfile>> &profiles) {
    const auto path = NativeFileDialog::getSaveFileName(this, tr("Export OTP"), QDir::homePath() + "/Throne-otp.json",
                                                        tr("JSON (*.json)"));
    if (path.isEmpty()) return;

    QList<OTP::Entry> entries;
    for (const auto &profile : profiles) entries.append(*profile);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        MessageBoxWarning(tr("Export OTP"), tr("Cannot write to: %1").arg(path));
        return;
    }
    file.write(OTP::ExportToJson(entries));
    file.close();

    MessageBoxInfo(tr("Export OTP"), tr("Exported %1 OTP profile(s) to:\n%2").arg(entries.size()).arg(path));
}
