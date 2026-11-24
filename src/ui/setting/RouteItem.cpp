#include "include/ui/setting/RouteItem.h"

#include "include/configs/common/Protocols.h"
#include "include/configs/generate.h"
#include "include/configs/sub/RouteUpdater.hpp"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"
#include "include/ui/setting/RouteDnsEditor.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/route/RouteEditorCommon.h"
#include "include/ui/setting/route/RouteQuickLists.h"
#include "include/ui/setting/route/RouteRulesPage.h"
#include "include/ui/utils/ValidationForm.h"

#include <algorithm>
#include <limits>

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSet>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

// User text and names from shared profiles must not render as rich text.
static void routeItemPlainBox(QWidget* parent, QMessageBox::Icon icon, const QString& title, const QString& text) {
    QMessageBox box(icon, title, text, QMessageBox::Ok, parent);
    box.setTextFormat(Qt::PlainText);
    box.exec();
}

static void routeItemTint(QLabel* label, bool danger) {
    const auto apply = [label, danger] {
        const auto& tokens = themeManager()->tokens;
        label->setStyleSheet(QStringLiteral("color: %1;").arg((danger ? tokens.danger : tokens.muted).name(QColor::HexArgb)));
    };
    apply();
    QObject::connect(themeManager(), &ThemeManager::themeChanged, label, apply);
}

static QList<QPair<QString, int>> routeItemEndpointCandidates() {
    const quint64 profilesRevision = Configs::dataManager->profilesRepo->Revision();
    static quint64 cachedRevision = std::numeric_limits<quint64>::max();
    static QList<QPair<QString, int>> cachedCandidates;

    if (profilesRevision == cachedRevision) return cachedCandidates;

    QList<QPair<QString, int>> candidates;
    QList<int> ids;
    for (const auto& type : Configs::Protocols::IdsWith(Configs::Protocols::VpnEndpoint)) {
        ids += Configs::dataManager->profilesRepo->GetProfileIdsByType(type);
    }
    ids += Configs::dataManager->profilesRepo->GetProfileIdsByType("chain");
    for (const int id : ids) {
        const auto ent = Configs::dataManager->profilesRepo->GetProfile(id);
        if (ent == nullptr || ent->outbound == nullptr) continue;
        if (!Configs::CanBeAuxEndpoint(ent)) continue;
        candidates.append({ent->outbound->DisplayTypeAndName(), id});
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        return QString::localeAwareCompare(a.first, b.first) < 0;
    });
    cachedCandidates = candidates;
    cachedRevision = profilesRevision;
    return cachedCandidates;
}

static bool routeItemIsEndpointRule(const std::shared_ptr<Configs::RouteRule>& rule) {
    return rule != nullptr && rule->type == Configs::endpointPreferredBy;
}

static QString routeItemEndpointName(int profileId) {
    const auto ent = Configs::dataManager->profilesRepo->GetProfile(profileId);
    if (ent == nullptr || ent->outbound == nullptr) return QString::number(profileId);
    return ent->outbound->DisplayName();
}

static QString routeItemUniqueRuleName(const QString& base, const QSet<QString>& taken) {
    QString name = base;
    for (int n = 2; taken.contains(name); n++) name = base + " (" + Int2String(n) + ")";
    return name;
}

static std::shared_ptr<Configs::RouteRule> routeItemMakeEndpointRule(int profileId) {
    auto rule = std::make_shared<Configs::RouteRule>();
    rule->type = Configs::endpointPreferredBy;
    rule->outboundID = profileId;
    // Nothing on it is user-editable; seeding attribute tabs from it would only offer edits.
    rule->uiAttributeTabsSeeded = true;
    return rule;
}

// Unnamed custom rules become "Rule N"; with dedupe, a repeated name also gets " (2)", " (3)".
static void routeItemNameRules(Configs::RouteProfile& profile, bool dedupe) {
    QSet<QString> taken;
    for (const auto& rule : profile.Rules) {
        if (!dedupe || rule->type != Configs::custom) taken.insert(rule->name);
    }
    int next = 1;
    for (const auto& rule : profile.Rules) {
        if (rule->type != Configs::custom) continue;
        if (rule->name.trimmed().isEmpty()) {
            while (taken.contains(RouteItem::tr("Rule %1").arg(next))) next++;
            rule->name = RouteItem::tr("Rule %1").arg(next);
        } else if (dedupe) {
            rule->name = routeItemUniqueRuleName(rule->name, taken);
        } else {
            continue;
        }
        taken.insert(rule->name);
    }
}

static QJsonArray routeItemIds(const QList<int>& ids) {
    QJsonArray array;
    for (const int id : ids) array.append(id);
    return array;
}

// to_share_json(), except for a rule to a deleted profile, which to_share_json() logs about and drops.
static QJsonObject routeItemRuleJson(Configs::RouteRule& rule) {
    const bool toProfile = rule.type != Configs::endpointPreferredBy && rule.outboundID >= 0 &&
                           (rule.action == QLatin1String("route") || rule.action == QLatin1String("bypass"));
    const auto ent = toProfile ? Configs::dataManager->profilesRepo->GetProfile(rule.outboundID) : nullptr;
    if (!toProfile || (ent != nullptr && ent->outbound != nullptr)) return rule.to_share_json();
    QJsonObject obj = rule.get_rule_json(false);
    obj["name"] = rule.name;
    obj["type"] = Configs::ruleTypeToToken(rule.type);
    obj["disabled"] = rule.disabled;
    if (rule.ignoreXrayRouting) obj["ignore_xray_routing"] = *rule.ignoreXrayRouting;
    return obj;
}

RouteItem::RouteItem(QWidget *parent, const std::shared_ptr<Configs::RouteProfile>& routeChain)
    : QDialog(parent), ui(new Ui::RouteItem) {
    ui->setupUi(this);

    chain = std::make_shared<Configs::RouteProfile>(*routeChain);
    // Not for a remote profile: a seeded rule would be local and outlive every update.
    if (chain->IsEmpty() && !chain->isRemote) {
        auto rule = std::make_shared<Configs::RouteRule>();
        rule->name = tr("DNS queries");
        rule->protocol = "dns";
        rule->action = "hijack-dns";
        chain->Rules << rule;
    }
    routeItemNameRules(*chain, true);
    QuickLists::EnsureLists(*chain);

    setupHeader();
    setupRemoteSection();
    setupEndpointsSection();
    syncEndpointRules();

    rulesPage = new RouteRulesPage(chain, ui->rulesTab);
    ui->rulesTabLayout->addWidget(rulesPage);
    connect(rulesPage, &RouteRulesPage::changed, this, &RouteItem::refreshEndpointSubtitles);
    connect(rulesPage, &RouteRulesPage::openEndpointsRequested, this, [this] { ui->tabWidget->setCurrentWidget(ui->endpointsTab); });
    // Deferred: both come from the page's own handlers, and syncing the endpoints reloads the page.
    connect(rulesPage, &RouteRulesPage::endpointRemovalRequested, this, [this](int profileId) {
        QTimer::singleShot(0, this, [this, profileId] { removeEndpointRow(profileId); });
    });
    connect(rulesPage, &RouteRulesPage::innerHopsOffRequested, this, [this](int ownerProfileId) {
        QTimer::singleShot(0, this, [this, ownerProfileId] { setEndpointRowInnerHops(ownerProfileId, false); });
    });
    connect(ui->defaultCombo, &QComboBox::currentIndexChanged, this, [this] { rulesPage->setDefaultOutbound(defaultOutbound()); });
    rulesPage->setDefaultOutbound(defaultOutbound());
    refreshEndpointSubtitles();

    dnsEditor = new RouteDnsEditor(*chain, ui->tabWidget);
    ui->tabWidget->addTab(dnsEditor, tr("DNS"));

    ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(ui->endpointsTab), !endpointRows.isEmpty() || !endpointCandidates.isEmpty());
    int visibleTabs = 0;
    for (int i = 0; i < ui->tabWidget->count(); i++) {
        if (ui->tabWidget->isTabVisible(i)) visibleTabs++;
    }
    ui->tabWidget->tabBar()->setVisible(visibleTabs > 1);
    ui->tabWidget->setCurrentWidget(ui->rulesTab);

    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);
    form->bind(ui->nameEdit, Validation::Rules::Required());
    if (chain->isRemote) {
        form->bind(ui->remoteUrlEdit, Validation::Rules::Url({"http", "https"}, Validation::Empty::Required)).label(ui->remoteUrlLabel->text());
    }
    form->addIssueSource([this] { return rulesPage->issues(); }, this);
    dnsEditor->bindTo(form);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &RouteItem::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &RouteItem::reject);

    form->markAll();

    const auto *scr = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
    QSize size = QSize(1000, 680).expandedTo(sizeHint());
    if (scr != nullptr) size = size.boundedTo(scr->availableGeometry().size());
    resize(size);

    openedFingerprint = fingerprint();
}

RouteItem::~RouteItem() {
    delete ui;
}

void RouteItem::setupHeader() {
    ui->nameEdit->setText(chain->name);
    for (const int id : {Configs::proxyID, Configs::directID, Configs::blockID, Configs::warpBypassID}) {
        ui->defaultCombo->addItem(RouteEditor::ThenLabel(RouteEditor::ThenOfOutbound(id)), id);
    }
    setDefaultOutbound(chain->defaultOutboundID);
    ui->ignoreXrayRoutingCheck->setChecked(chain->ignoreXrayRouting);
    connect(ui->ignoreXrayRoutingCheck, &QCheckBox::toggled, this, [this](bool on) {
        chain->ignoreXrayRouting = on;
        if (rulesPage) rulesPage->reload();
        if (form) form->refresh();
    });

    auto *info = ui->xrayRoutingInfo;
    auto *help = ui->xrayRoutingHelp;
    help->hide();
    const auto styleInfo = [info, help] {
        info->setIcon(themeManager()->standardIcon(QStyle::SP_MessageBoxInformation));
        help->setStyleSheet(QStringLiteral("color: %1;").arg(themeManager()->tokens.warning.name(QColor::HexArgb)));
    };
    styleInfo();
    connect(themeManager(), &ThemeManager::themeChanged, info, styleInfo);
    connect(info, &QToolButton::toggled, help, &QWidget::setVisible);
    for (QWidget *widget : {static_cast<QWidget *>(ui->ignoreXrayRoutingCheck), static_cast<QWidget *>(info)}) {
        widget->setAccessibleDescription(help->text());
        widget->setToolTip(help->text());
        widget->installEventFilter(this);
    }
}

bool RouteItem::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->ignoreXrayRoutingCheck || watched == ui->xrayRoutingInfo) {
        if (event->type() == QEvent::ToolTip) {
            // Use the normal hover delay, but keep the translated text inside the dialog.
            ui->xrayRoutingHelp->show();
            return true;
        }
        if (event->type() == QEvent::Leave && !ui->xrayRoutingInfo->isChecked())
            ui->xrayRoutingHelp->hide();
    }
    return QDialog::eventFilter(watched, event);
}

void RouteItem::setDefaultOutbound(int outboundID) {
    const int index = ui->defaultCombo->findData(outboundID);
    ui->defaultCombo->setCurrentIndex(index >= 0 ? index : 0);
}

int RouteItem::defaultOutbound() const {
    const QVariant data = ui->defaultCombo->currentData();
    return data.isValid() ? data.toInt() : static_cast<int>(Configs::proxyID);
}

void RouteItem::setupRemoteSection() {
    if (!chain->isRemote) {
        ui->remoteBox->hide();
        return;
    }
    QFont caption = ui->remoteCaption->font();
    caption.setBold(true);
    ui->remoteCaption->setFont(caption);
    routeItemTint(ui->remoteNote, false);
    ui->remoteUrlEdit->setText(chain->remoteURL);
    ui->autoUpdateCheck->setChecked(chain->autoUpdate);
    connect(ui->remotePreviewBtn, &QPushButton::clicked, this, [this] { fetchRemote(false); });
    connect(ui->remoteFetchBtn, &QPushButton::clicked, this, [this] { fetchRemote(true); });
}

void RouteItem::fetchRemote(bool applyToChain) {
    const QString url = ui->remoteUrlEdit->text().trimmed();
    if (!Validation::Url(url, {"http", "https"}, Validation::Empty::Required).isEmpty()) {
        form->revalidate(ui->remoteUrlEdit);
        MessageBoxWarning(tr("Invalid URL"), tr("Enter a valid http(s) URL first."));
        return;
    }
    const bool hasRemoteRules = std::any_of(chain->Rules.cbegin(), chain->Rules.cend(), [](const auto& rule) { return rule->remote; });
    if (applyToChain && hasRemoteRules) {
        if (QMessageBox::question(this, tr("Fetch from remote"),
                                  tr("This will replace the rules that came from the URL, edits to them included, with the ones fetched now. Rules added here are kept. Continue?"))
            != QMessageBox::StandardButton::Yes) {
            return;
        }
    }

    ui->remotePreviewBtn->setEnabled(false);
    ui->remoteFetchBtn->setEnabled(false);
    const QString origFetch = ui->remoteFetchBtn->text();
    const QString origPreview = ui->remotePreviewBtn->text();
    (applyToChain ? ui->remoteFetchBtn : ui->remotePreviewBtn)->setText(tr("Fetching..."));

    // The worker fills a copy. UpdateProfile fills an empty name from the remote one, so the typed name goes in first.
    auto target = std::make_shared<Configs::RouteProfile>(*chain);
    target->isRemote = true;
    target->remoteURL = url;
    target->name = ui->nameEdit->text();
    const QString typedName = target->name;
    const QPointer<RouteItem> self(this);

    runOnNewThread([=, this] {
        QString warnings;
        const QString err = RouteUpdate::UpdateProfile(target, &warnings);
        runOnUiThread([=, this] {
            if (self.isNull()) return;
            ui->remotePreviewBtn->setEnabled(true);
            ui->remoteFetchBtn->setEnabled(true);
            ui->remoteFetchBtn->setText(origFetch);
            ui->remotePreviewBtn->setText(origPreview);
            if (!err.isEmpty()) {
                MessageBoxWarning(tr("Could not fetch routing profile"), err);
                return;
            }
            const auto fetchedCount = std::count_if(target->Rules.cbegin(), target->Rules.cend(), [](const auto& rule) { return rule->remote; });
            if (applyToChain) {
                chain->Rules = target->Rules;
                chain->defaultOutboundID = target->defaultOutboundID;
                chain->remoteURL = target->remoteURL;
                chain->remoteLastUpdate = target->remoteLastUpdate;
                if (target->name != typedName) ui->nameEdit->setText(target->name);
                routeItemNameRules(*chain, true);
                QuickLists::EnsureLists(*chain);
                setDefaultOutbound(chain->defaultOutboundID);
                syncEndpointRules();
                form->refresh();
                const QString msg = tr("Loaded %1 rule(s) from the remote URL.").arg(fetchedCount);
                if (warnings.isEmpty()) MessageBoxInfo(tr("Fetched"), msg);
                else routeItemPlainBox(this, QMessageBox::Information, tr("Fetched with warnings"), msg + "\n\n" + warnings.trimmed());
                return;
            }
            // Only what the URL serves: the local rules and endpoints merged into target are not part of it.
            target->Rules.removeIf([](const auto& rule) { return !rule->remote; });
            target->endpointProfileIDs.clear();
            target->innerHopEndpointIDs.clear();
            auto* dlg = new QDialog(this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->setWindowTitle(tr("Remote Routing Profile Preview"));
            auto* lay = new QVBoxLayout(dlg);
            auto* header = new QLabel(tr("%1 — %2 rule(s)").arg(target->name.isEmpty() ? tr("(unnamed)") : target->name,
                                                               QString::number(fetchedCount)), dlg);
            header->setTextFormat(Qt::PlainText);
            lay->addWidget(header);
            if (!warnings.isEmpty()) {
                auto* warn = new QLabel(warnings.trimmed(), dlg);
                warn->setTextFormat(Qt::PlainText);
                warn->setWordWrap(true);
                routeItemTint(warn, true);
                lay->addWidget(warn);
            }
            auto* view = new QPlainTextEdit(dlg);
            view->setReadOnly(true);
            view->setPlainText(QJsonObject2QString(target->ToShareObject(), false));
            view->setLineWrapMode(QPlainTextEdit::NoWrap);
            lay->addWidget(view, 1);
            auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, dlg);
            connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
            connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
            lay->addWidget(buttons);
            dlg->resize(560, 460);
            dlg->show();
        });
    });
}

void RouteItem::setupEndpointsSection() {
    routeItemTint(ui->endpointsIntro, false);
    endpointCandidates = routeItemEndpointCandidates();
    QSet<int> listed;
    for (const int profileId : chain->endpointProfileIDs) {
        if (listed.contains(profileId)) continue;
        listed.insert(profileId);
        addEndpointRow(profileId, chain->innerHopEndpointIDs.contains(profileId));
    }

    connect(ui->endpointPicker, &QComboBox::currentIndexChanged, this, [this](int index) { ui->endpointAddBtn->setEnabled(index >= 0); });
    connect(ui->endpointAddBtn, &QPushButton::clicked, this, [this] {
        const int index = ui->endpointPicker->currentIndex();
        if (index < 0) return;
        addEndpointRow(ui->endpointPicker->itemData(index).toInt(), false);
        refreshEndpointCandidates();
        syncEndpointRules();
    });
    refreshEndpointCandidates();
}

void RouteItem::refreshEndpointCandidates() {
    QSet<int> used;
    for (const auto& row : std::as_const(endpointRows)) used.insert(row.profileId);
    {
        const QSignalBlocker blocker(ui->endpointPicker);
        ui->endpointPicker->clear();
        for (const auto& [label, id] : std::as_const(endpointCandidates)) {
            if (!used.contains(id)) ui->endpointPicker->addItem(label, id);
        }
        ui->endpointPicker->setCurrentIndex(-1);
    }
    ui->endpointPicker->setEnabled(ui->endpointPicker->count() > 0);
    ui->endpointAddBtn->setEnabled(false);
}

void RouteItem::addEndpointRow(int profileId, bool innerHops) {
    const auto ent = Configs::dataManager->profilesRepo->GetProfile(profileId);
    const bool alive = ent != nullptr && ent->outbound != nullptr;
    const bool hasInnerHops = alive && !Configs::AuxEndpointInnerHops(profileId).isEmpty();

    EndpointRow row;
    row.profileId = profileId;
    row.innerHops = innerHops && hasInnerHops;

    auto* frame = new QFrame(ui->endpointRowsHost);
    frame->setFrameShape(QFrame::StyledPanel);
    auto* layout = new QHBoxLayout(frame);
    auto* texts = new QVBoxLayout();
    auto* name = new QLabel(alive ? ent->outbound->DisplayName() : tr("Profile #%1 — deleted, dropped when you save").arg(profileId), frame);
    name->setTextFormat(Qt::PlainText);
    name->setWordWrap(true);
    QFont bold = name->font();
    bold.setBold(true);
    name->setFont(bold);
    if (!alive) routeItemTint(name, true);
    texts->addWidget(name);
    if (alive) {
        row.subtitle = new QLabel(frame);
        row.subtitle->setTextFormat(Qt::PlainText);
        routeItemTint(row.subtitle, false);
        texts->addWidget(row.subtitle);
    }
    layout->addLayout(texts, 1);
    if (hasInnerHops) {
        row.toggle = new QCheckBox(tr("Allow routing to inner hops"), frame);
        row.toggle->setChecked(row.innerHops);
        row.toggle->setToolTip(tr("Add a rule for each OpenVPN/OpenConnect hop behind this exit."));
        connect(row.toggle, &QCheckBox::toggled, this, [this, profileId](const bool on) {
            for (auto& listedRow : endpointRows) {
                if (listedRow.profileId == profileId) listedRow.innerHops = on;
            }
            syncEndpointRules();
        });
        layout->addWidget(row.toggle);
    }
    auto* remove = new QPushButton(tr("Remove"), frame);
    remove->setAutoDefault(false);
    connect(remove, &QPushButton::clicked, this, [this, profileId] { removeEndpointRow(profileId); });
    layout->addWidget(remove);

    row.widget = frame;
    // Before the spacer that keeps the rows at the top.
    ui->endpointRowsLayout->insertWidget(ui->endpointRowsLayout->count() - 1, frame);
    endpointRows << row;
}

void RouteItem::removeEndpointRow(int profileId) {
    for (int i = 0; i < endpointRows.size(); i++) {
        if (endpointRows.at(i).profileId != profileId) continue;
        QWidget* widget = endpointRows.at(i).widget;
        endpointRows.removeAt(i);
        // Later: this can run from the row's own Remove button.
        widget->hide();
        widget->deleteLater();
        break;
    }
    refreshEndpointCandidates();
    syncEndpointRules();
}

QList<int> RouteItem::listedEndpointIDs() const {
    QList<int> ids;
    for (const auto& row : endpointRows) {
        if (ids.contains(row.profileId)) continue;
        const auto ent = Configs::dataManager->profilesRepo->GetProfile(row.profileId);
        if (ent == nullptr || ent->outbound == nullptr) continue;
        ids << row.profileId;
    }
    return ids;
}

QList<int> RouteItem::listedInnerHopEndpointIDs() const {
    QList<int> ids;
    const QList<int> listed = listedEndpointIDs();
    for (const auto& row : endpointRows) {
        if (!row.innerHops || ids.contains(row.profileId) || !listed.contains(row.profileId)) continue;
        ids << row.profileId;
    }
    return ids;
}

void RouteItem::setEndpointRowInnerHops(int profileId, bool innerHops) {
    const auto row = std::find_if(endpointRows.begin(), endpointRows.end(), [profileId](const EndpointRow& r) { return r.profileId == profileId; });
    if (row == endpointRows.end()) return;
    // Its own toggled handler writes the row back and re-syncs.
    if (row->toggle != nullptr && row->toggle->isChecked() != innerHops) {
        row->toggle->setChecked(innerHops);
        return;
    }
    row->innerHops = innerHops;
    syncEndpointRules();
}

void RouteItem::syncEndpointRules() {
    chain->endpointProfileIDs = listedEndpointIDs();
    chain->innerHopEndpointIDs = listedInnerHopEndpointIDs();
    const QList<int> targets = chain->endpointRuleTargets();

    QSet<int> paired;
    QSet<QString> names;
    QList<std::shared_ptr<Configs::RouteRule>> kept;
    for (const auto& rule : chain->Rules) {
        if (routeItemIsEndpointRule(rule)) {
            if (!targets.contains(rule->outboundID) || paired.contains(rule->outboundID)) continue;
            paired.insert(rule->outboundID);
        } else {
            names.insert(rule->name);
        }
        kept << rule;
    }
    chain->Rules = kept;

    for (const int id : targets) {
        if (!paired.contains(id)) chain->Rules << routeItemMakeEndpointRule(id);
    }

    for (const auto& rule : chain->Rules) {
        if (!routeItemIsEndpointRule(rule)) continue;
        rule->name = routeItemUniqueRuleName(
            RouteItem::tr("%1 route prefer").arg(routeItemEndpointName(rule->outboundID)), names);
        names.insert(rule->name);
    }

    reloadRules();
    refreshEndpointSubtitles();
}

void RouteItem::refreshEndpointSubtitles() {
    if (endpointRows.isEmpty()) return;
    const auto rows = RouteEditor::BuildRows(*chain);
    for (const auto& row : std::as_const(endpointRows)) {
        if (row.subtitle == nullptr) continue;
        const auto ent = Configs::dataManager->profilesRepo->GetProfile(row.profileId);
        const QString type = ent != nullptr && ent->outbound != nullptr ? ent->outbound->DisplayType() : QString();
        int number = 0;
        for (const auto& rule : chain->Rules) {
            if (!routeItemIsEndpointRule(rule) || rule->outboundID != row.profileId) continue;
            number = RouteEditor::RowNumber(rows, *chain, rule);
            break;
        }
        row.subtitle->setText(number > 0 ? tr("%1 · rule %2 in the list").arg(type, QString::number(number)) : type);
    }
}

void RouteItem::reloadRules() {
    if (rulesPage != nullptr) rulesPage->reload();
}

QString RouteItem::fingerprint() const {
    Configs::RouteProfile copy(*chain);
    copy.name = ui->nameEdit->text().trimmed();
    if (copy.isRemote) {
        copy.remoteURL = ui->remoteUrlEdit->text().trimmed();
        copy.autoUpdate = ui->autoUpdateCheck->isChecked();
    }
    copy.defaultOutboundID = defaultOutbound();
    dnsEditor->writeTo(copy);

    QJsonArray rules;
    for (const auto& rule : copy.Rules) {
        if (RouteEditor::IsQuickListType(rule->type) && rule->isEmpty()) continue;
        rules.append(routeItemRuleJson(*rule));
    }
    const QJsonObject object{
        {"name", copy.name},
        {"defaultOutboundID", copy.defaultOutboundID},
        {"ignoreXrayRouting", ui->ignoreXrayRoutingCheck->isChecked()},
        {"isRemote", copy.isRemote},
        {"remoteURL", copy.remoteURL},
        {"autoUpdate", copy.autoUpdate},
        {"endpointProfileIDs", routeItemIds(copy.endpointProfileIDs)},
        {"innerHopEndpointIDs", routeItemIds(copy.innerHopEndpointIDs)},
        {"customDns", copy.customDns},
        {"dns", copy.dns.has_value() ? QJsonValue(copy.dns->ToJson()) : QJsonValue()},
        {"rules", rules},
    };
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

bool RouteItem::confirmDiscard() {
    if (fingerprint() == openedFingerprint) return true;
    QMessageBox box(QMessageBox::Question, tr("Unsaved changes"), tr("Discard the changes to this routing profile?"), QMessageBox::NoButton, this);
    auto* discard = box.addButton(tr("Discard"), QMessageBox::DestructiveRole);
    auto* keep = box.addButton(tr("Keep editing"), QMessageBox::RejectRole);
    box.setDefaultButton(keep);
    box.setEscapeButton(keep);
    box.exec();
    return box.clickedButton() == discard;
}

void RouteItem::accept() {
    // A condition's field commits on focus-out, which a Save taking no focus (a mnemonic, a click on macOS) never sends.
    if (QWidget* focused = focusWidget()) focused->clearFocus();
    if (!form->validateAll()) return;

    chain->name = ui->nameEdit->text().trimmed();
    if (chain->isRemote) {
        chain->remoteURL = ui->remoteUrlEdit->text().trimmed();
        chain->autoUpdate = ui->autoUpdateCheck->isChecked();
    }
    chain->defaultOutboundID = defaultOutbound();
    chain->ignoreXrayRouting = ui->ignoreXrayRoutingCheck->isChecked();

    // Disabled custom rules stay even when empty: they never block saving.
    QList<std::shared_ptr<Configs::RouteRule>> kept;
    for (const auto& rule : chain->Rules) {
        if (rule->isEmpty() && !(rule->type == Configs::custom && rule->disabled)) continue;
        kept << rule;
    }
    chain->Rules = kept;
    routeItemNameRules(*chain, false);

    // Endpoints whose profile is gone drop out here, and syncEndpointRules() drops their rules with them.
    const int missingEndpoints = static_cast<int>(endpointRows.size() - listedEndpointIDs().size());
    syncEndpointRules();

    // A remote profile may be saved before its first fetch; only plain profiles must be non-empty.
    if (!chain->isRemote && chain->IsEmpty()) {
        QuickLists::EnsureLists(*chain);
        reloadRules();
        MessageBoxInfo(tr("Empty routing profile"), tr("No valid rules are in the profile"));
        return;
    }

    if (missingEndpoints > 0) {
        MessageBoxInfo(tr("Endpoints"),
                       tr("%1 endpoint profile(s) no longer exist and were removed from this routing profile.")
                           .arg(missingEndpoints));
    }

    dnsEditor->applyTo(*chain);

    emit settingsChanged(chain);

    QDialog::accept();
}

void RouteItem::reject() {
    if (!discardConfirmed && !confirmDiscard()) return;
    discardConfirmed = true;
    QDialog::reject();
}

void RouteItem::closeEvent(QCloseEvent *event) {
    if (!discardConfirmed && isVisible() && !confirmDiscard()) {
        event->ignore();
        return;
    }
    // QDialog::closeEvent calls reject(), which must not ask again.
    discardConfirmed = true;
    QDialog::closeEvent(event);
}

void RouteItem::keyPressEvent(QKeyEvent *event) {
    // Enter ends typing in a field; only the Save button saves.
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}
