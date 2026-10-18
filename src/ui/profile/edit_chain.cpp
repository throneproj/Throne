#include "include/ui/profile/edit_chain.h"


#include "include/database/ProfilesRepo.h"
#include "include/ui/mainwindow.h"
#include "include/ui/profile/ProxyItem.h"
#include "include/ui/utils/ValidationForm.h"

namespace {
    // generate.cpp chainScanError over the build order, which reads the list bottom-up.
    QString editChainScanError(const QList<std::shared_ptr<Configs::Profile>> &topFirst) {
        const auto count = topFirst.size();
        int extraCores = 0, extraCoreIndex = -1, xrayFullConfigs = 0, xrayFullConfigIndex = -1, xrayHops = 0, transitions = 0;
        bool customFullConfig = false, inXray = false;
        for (qsizetype k = 0; k < count; ++k) {
            const auto &hop = topFirst[count - 1 - k];
            const bool xray = hop->outbound->IsXray();
            if (xray != inXray) transitions++;
            inXray = xray;
            if (hop->outbound->IsExtraCore()) {
                extraCores++;
                extraCoreIndex = int(k);
            }
            if (hop->outbound->IsXrayFullConfig()) {
                xrayFullConfigs++;
                xrayFullConfigIndex = int(k);
            }
            if (xray) xrayHops++;
            if (hop->type == "custom" && hop->Custom() != nullptr && hop->Custom()->type == Configs::Custom::CustomFullConfig) customFullConfig = true;
        }
        if (customFullConfig) return EditChain::tr("A custom %1 config cannot be a hop; only custom outbounds can.").arg(software_core_name);
        if (extraCores > 1) return EditChain::tr("Only one Extra Core profile can be in a chain.");
        if (xrayFullConfigs > 1) return EditChain::tr("Only one custom Xray config can be in a chain.");
        if (extraCores > 0 && xrayFullConfigs > 0) return EditChain::tr("An Extra Core profile and a custom Xray config cannot be in the same chain.");
        if (xrayFullConfigs > 0 && xrayHops > 0) return EditChain::tr("A custom Xray config cannot share a chain with other Xray profiles.");
        // An extra core or a custom Xray config is reached through its local socks port, so nothing can come after it.
        if (extraCores == 1 && count > 1 && extraCoreIndex != count - 1) {
            return EditChain::tr("An Extra Core profile can only be the final hop; move it to the top of the list.");
        }
        if (xrayFullConfigs == 1 && count > 1 && xrayFullConfigIndex != count - 1) {
            return EditChain::tr("A custom Xray config can only be the final hop; move it to the top of the list.");
        }
        if (transitions > 2) {
            return EditChain::tr("The chain switches between %1 and Xray too often; keep the Xray hops together, between %1 hops.").arg(software_core_name);
        }
        return {};
    }
}

EditChain::EditChain(QWidget *parent) : QWidget(parent), ui(new Ui::EditChain) {
    ui->setupUi(this);
    // Adding, removing, replacing and dragging hops are the list's focus-out.
    const auto revalidate = [this] {
        if (form) form->revalidate(ui->listWidget);
    };
    auto *model = ui->listWidget->model();
    connect(model, &QAbstractItemModel::rowsInserted, this, revalidate);
    connect(model, &QAbstractItemModel::rowsRemoved, this, revalidate);
    connect(model, &QAbstractItemModel::rowsMoved, this, revalidate);
    connect(model, &QAbstractItemModel::dataChanged, this, revalidate);
}

EditChain::~EditChain() {
    delete ui;
}

void EditChain::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Chain();

    droppedHops = 0;
    for (auto id: outbound->list) {
        if (!AddProfileToListIfExist(id, false)) droppedHops++;
    }
}

bool EditChain::onEnd() {
    if (get_edit_text_name().isEmpty()) {
        MessageBoxWarning(software_name, tr("Name cannot be empty."));
        return false;
    }

    auto outbound = this->ent->Chain();

    QList<int> idList;
    for (int i = 0; i < ui->listWidget->count(); i++) {
        idList << ui->listWidget->item(i)->data(114514).toInt();
    }
    outbound->list = idList;

    return true;
}

void EditChain::bindValidation(Validation::Form &form) {
    this->form = &form;
    const auto label = tr("Hops");
    form.addRule({ui->listWidget}, [this] { return hopsError(); }, this, Validation::Severity::Error, label);
    form.addRule({ui->listWidget}, [this] {
        if (droppedHops == 0) return QString();
        return tr("%n hop(s) no longer exist or cannot be hops, so they were left out; saving removes them.", nullptr, droppedHops);
    }, this, Validation::Severity::Warning, label);
}

QString EditChain::hopsError() const {
    QList<std::shared_ptr<Configs::Profile>> hops;
    for (int i = 0; i < ui->listWidget->count(); i++) {
        auto hop = Configs::dataManager->profilesRepo->GetProfile(ui->listWidget->item(i)->data(114514).toInt());
        if (hop == nullptr || hop->outbound == nullptr) return tr("Hop %1 no longer exists; replace or remove it.").arg(i + 1);
        hops << hop;
    }
    if (hops.isEmpty()) return tr("Add at least one profile to the chain.");
    return editChainScanError(hops);
}

void EditChain::on_select_profile_clicked() {
    get_edit_dialog()->hide();
    GetMainWindow()->start_select_mode(this, [=,this](int id) {
        get_edit_dialog()->show();
        AddProfileToListIfExist(id);
    });
}

static bool acceptChainHop(const std::shared_ptr<Configs::Profile> &ent, bool explain) {
    if (ent == nullptr || ent->type == "chain") return false;
    if (ent->type == "autoselector") {
        if (explain) {
            MessageBoxWarning(software_name, QObject::tr("An auto selector cannot be a hop in a chain: it moves to a different "
                                                         "server on its own whenever one degrades."));
        }
        return false;
    }
    return true;
}

bool EditChain::AddProfileToListIfExist(int profileId, bool explain) {
    auto _ent = Configs::dataManager->profilesRepo->GetProfile(profileId);
    if (!acceptChainHop(_ent, explain)) return false;
    auto wI = new QListWidgetItem();
    wI->setData(114514, profileId);
    auto w = new ProxyItem(this, _ent, wI);
    ui->listWidget->addItem(wI);
    ui->listWidget->setItemWidget(wI, w);
    connect(w->get_change_button(), &QPushButton::clicked, w, [=,this] {
        get_edit_dialog()->hide();
        GetMainWindow()->start_select_mode(w, [=,this](int newId) {
            get_edit_dialog()->show();
            ReplaceProfile(w, newId);
        });
    });
    return true;
}

void EditChain::ReplaceProfile(ProxyItem *w, int profileId) {
    auto _ent = Configs::dataManager->profilesRepo->GetProfile(profileId);
    if (acceptChainHop(_ent, true)) {
        w->item->setData(114514, profileId);
        w->ent = _ent;
        w->refresh_data();
    }
}
