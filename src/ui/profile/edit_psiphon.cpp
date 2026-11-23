#include "include/ui/profile/edit_psiphon.h"

#include "include/api/RPC.h"
#include "include/configs/outbounds/psiphon.h"
#include "include/global/Utils.hpp"

#include <QComboBox>
#include <QPointer>

#include <algorithm>

namespace {
    // current stays selected, appended when the list lacks it.
    void editPsiphonFill(QComboBox *combo, const QStringList &regions, const QString &current) {
        QList<QPair<QString, QString>> named;
        for (const auto &code : regions) named.append({Configs::PsiphonRegionName(code), code});
        std::sort(named.begin(), named.end(), [](const auto &a, const auto &b) { return a.first.localeAwareCompare(b.first) < 0; });
        combo->clear();
        combo->addItem(Configs::PsiphonRegionName({}), QString());
        for (const auto &[name, code] : named) combo->addItem(name, code);
        int index = combo->findData(current);
        if (index < 0) {
            combo->addItem(Configs::PsiphonRegionName(current), current);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index);
    }

    // Off the UI thread; empty while the core is unreachable or no Psiphon outbound has connected yet.
    QStringList editPsiphonCoreRegions() {
        if (API::defaultClient == nullptr || !API::defaultClient->IsConnected()) return {};
        bool ok = false;
        const auto reply = API::defaultClient->PsiphonRegions(&ok);
        if (!ok) return {};
        QStringList regions;
        for (const auto &region : reply.regions) {
            const auto code = QString::fromStdString(region).trimmed().toUpper();
            if (!code.isEmpty() && Configs::Checks::PsiphonRegion(code).isEmpty() && !regions.contains(code)) regions << code;
        }
        return regions;
    }
}

EditPsiphon::EditPsiphon(QWidget *parent) : QWidget(parent), ui(new Ui::EditPsiphon) {
    ui->setupUi(this);
}

EditPsiphon::~EditPsiphon() {
    delete ui;
}

void EditPsiphon::FillRegions(QComboBox *combo, const QString &current) {
    editPsiphonFill(combo, Configs::psiphonRegions, current);
    const QPointer<QComboBox> guard(combo);
    runOnNewThread([guard] {
        const auto regions = editPsiphonCoreRegions();
        if (regions.isEmpty()) return;
        runOnUiThread([guard, regions] {
            if (guard == nullptr) return;
            editPsiphonFill(guard, regions, guard->currentData().toString());
        });
    });
}

void EditPsiphon::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    FillRegions(ui->egress_region, this->ent->Psiphon()->egress_region);
}

bool EditPsiphon::onEnd() {
    this->ent->Psiphon()->egress_region = ui->egress_region->currentData().toString();
    return true;
}
