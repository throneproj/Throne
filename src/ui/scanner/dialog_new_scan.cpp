#include "include/ui/scanner/dialog_new_scan.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QScreen>
#include <QSet>
#include <QStyle>
#include <QVBoxLayout>

#include "include/database/DatabaseManager.h"
#include "include/database/IpScansRepo.h"
#include "include/global/Utils.hpp"
#include "include/scanner/DefaultIpLists.h"
#include "include/scanner/IpListUpdater.h"
#include "include/scanner/ScanManager.h"
#include "include/ui/utils/ValidationForm.h"

namespace {
    QString newScanDefaultName() {
        QSet<QString> taken;
        const auto scans = Configs::dataManager->ipScansRepo->GetAllIpScans();
        for (const auto &scan : scans) taken.insert(scan->name);
        for (qsizetype n = scans.size() + 1;; ++n) {
            const auto name = DialogNewScan::tr("Scan %1").arg(n);
            if (!taken.contains(name)) return name;
        }
    }

    QLabel *newScanDescription(const QString &text, QWidget *parent) {
        auto *label = new QLabel(text, parent);
        label->setWordWrap(true);
        label->setProperty("colorRole", QStringLiteral("muted"));
        return label;
    }
}

DialogNewScan::DialogNewScan(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("New scan"));
    form = new Validation::Form(this);

    auto *layout = new QVBoxLayout(this);

    auto *fields = new QFormLayout();
    nameEdit = new QLineEdit(newScanDefaultName(), this);
    nameEdit->selectAll();
    fields->addRow(tr("Name"), nameEdit);
    layout->addLayout(fields);
    form->bind(nameEdit, Validation::Rules::Required());

    auto *kinds = new QButtonGroup(this);
    genericRadio = new QRadioButton(tr("Generic scan"), this);
    warpRadio = new QRadioButton(tr("WARP scan"), this);
    kinds->addButton(genericRadio);
    kinds->addButton(warpRadio);
    genericRadio->setChecked(true);

    const int indent = style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth) +
                       style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing);
    const auto addKind = [&](QRadioButton *radio, const QString &description) {
        layout->addWidget(radio);
        auto *label = newScanDescription(description, this);
        label->setContentsMargins(indent, 0, 0, 0);
        layout->addWidget(label);
    };
    addKind(genericRadio, tr("Probe addresses with ping, TCP, TLS or HTTP, and optionally test them through one of your profiles."));
    addKind(warpRadio, tr("Find Cloudflare WARP endpoints that complete a WireGuard, AmneziaWG or MASQUE connection."));
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    form->attachStatus(buttons);

    nameEdit->setFocus();
    const auto *scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const QSize wanted = sizeHint().expandedTo(QSize(460, 0));
    resize(scr != nullptr ? wanted.boundedTo(scr->availableGeometry().size()) : wanted);
}

void DialogNewScan::accept() {
    if (!form->validateAll()) return;
    const auto name = nameEdit->text().trimmed();

    const auto kind = warpRadio->isChecked() ? Configs::IpScan::Kind::Warp : Configs::IpScan::Kind::Generic;
    auto scan = Configs::IpScansRepo::NewIpScan(kind);
    scan->name = name;
    if (kind == Configs::IpScan::Kind::Warp) {
        scan->base_list_id = Scanner::DefaultIpLists::Ensure(Scanner::DefaultIpLists::kWarpWireGuardName);
        if (scan->base_list_id >= 0) Scanner::IpListUpdater::instance()->NotifyListsChanged();
    }
    if (!Configs::dataManager->ipScansRepo->AddIpScan(scan)) {
        MessageBoxWarning(tr("New scan"), tr("Failed to store the scan."));
        return;
    }
    createdId = scan->id;
    Scanner::ScanManager::instance()->NotifyScansChanged();
    QDialog::accept();
}
