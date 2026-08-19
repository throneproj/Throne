#include "include/ui/setting/RawRouteItem.h"

#include "include/global/Configs.hpp"
#include "include/database/ProfilesRepo.h"
#include "include/database/GroupsRepo.h"
#include "include/ui/widget/json/JsonIssueList.h"
#include "include/ui/widget/json/SchemaStore.h"
#include "include/ui/setting/ThemeManager.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextBlock>

RawRouteEdit::RawRouteEdit(QWidget* parent) : JsonCodeEdit(parent) {
    completer = new QCompleter(this);
    completer->setModel(new QStringListModel(completer));
    completer->setWidget(this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    connect(completer, static_cast<void (QCompleter::*)(const QString&)>(&QCompleter::activated),
            this, &RawRouteEdit::insertOutboundCompletion);
}

void RawRouteEdit::setOutboundItems(const QList<QPair<QString, QString>>& items) {
    QStringList display;
    outboundIdByDisplay.clear();
    for (const auto& [text, id] : items) {
        display << text;
        outboundIdByDisplay.insert(text, id);
    }
    completer->setModel(new QStringListModel(display, completer));
}

bool RawRouteEdit::outboundContext(QString* partial) const {
    const QTextCursor tc = textCursor();
    const QString line = tc.block().text().left(tc.positionInBlock());
    // cursor is in a bare (unquoted) value right after "outbound": or "final":
    static const QRegularExpression re(QStringLiteral(R"rx("(?:outbound|final)"\s*:\s*([^",}\]\s]*)$)rx"));
    const auto m = re.match(line);
    if (!m.hasMatch()) return false;
    if (partial) *partial = m.captured(1);
    return true;
}

void RawRouteEdit::insertOutboundCompletion(const QString& completion) {
    // completion is the popup display text (e.g. "[Group] Name"); insert its mapped id.
    const QString id = outboundIdByDisplay.value(completion);
    if (id.isEmpty()) return;
    QTextCursor tc = textCursor();
    tc.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, completer->completionPrefix().length());
    tc.removeSelectedText();
    tc.insertText(id);
    setTextCursor(tc);
}

void RawRouteEdit::updateCompleter() {
    QString partial;
    if (!outboundContext(&partial)) {
        completer->popup()->hide();
        return;
    }
    if (partial != completer->completionPrefix()) {
        completer->setCompletionPrefix(partial);
        completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));
    }
    QRect cr = cursorRect();
    cr.setWidth(completer->popup()->sizeHintForColumn(0) + completer->popup()->verticalScrollBar()->sizeHint().width());
    completer->complete(cr);
}

void RawRouteEdit::keyPressEvent(QKeyEvent* e) {
    if (completer->popup()->isVisible()) {
        switch (e->key()) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Escape:
            case Qt::Key_Tab:
            case Qt::Key_Backtab:
                e->ignore();
                return;
            default:
                break;
        }
    }

    JsonCodeEdit::keyPressEvent(e);
    updateCompleter();
}

RawRouteItem::RawRouteItem(QWidget* parent, const std::shared_ptr<Configs::RouteProfile>& routeChain) : QDialog(parent) {
    setWindowTitle(tr("Raw routing profile"));
    chain = std::make_shared<Configs::RouteProfile>(*routeChain);
    chain->isRaw = true;

    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    nameEdit = new QLineEdit(chain->name, this);
    form->addRow(tr("Name"), nameEdit);
    layout->addLayout(form);

    preventCheck = new QCheckBox(tr("Prevent modifications"), this);
    preventCheck->setChecked(chain->preventModifications);
    preventCheck->setToolTip(tr("Use the route object exactly as written (outbound ids are still resolved to tags).\n"
                                "Throne will NOT add its DNS-hijack or xray bridge plumbing, so DNS, chained/xray\n"
                                "outbounds and other Throne features may break. For advanced users only."));
    layout->addWidget(preventCheck);

    jsonEdit = new RawRouteEdit(this);
    if (auto validator = JsonEdit::SingBoxValidator(JsonEdit::SingBox::Route)) {
        // Throne writes profile ids where sing-box writes outbound tags; TranslateRawOutbounds swaps them at build time.
        validator->AllowExtraType(QStringLiteral("outbound"), JsonEdit::ValueType::Number);
        validator->AllowExtraType(QStringLiteral("final"), JsonEdit::ValueType::Number);
        jsonEdit->setValidator(validator);
    }
    jsonEdit->setPlainText(chain->rawRoute.isEmpty()
        ? QStringLiteral("{\n"
                         "  \"rules\": [\n"
                         "    {\n"
                         "      \"domain_suffix\": [\".example.com\"],\n"
                         "      \"outbound\": -2\n"
                         "    }\n"
                         "  ],\n"
                         "  \"final\": -1\n"
                         "}")
        : chain->rawRoute);
    layout->addWidget(jsonEdit, 1);

    QList<QPair<QString, QString>> items;
    items.append({QStringLiteral("proxy"), QString::number(-1)});
    items.append({QStringLiteral("direct"), QString::number(-2)});
    items.append({QStringLiteral("warp-bypass"), QString::number(Configs::warpBypassID)});
    QMap<int, QString> idToName;
    for (const auto& [pid, pname] : Configs::dataManager->profilesRepo->GetAllProfileIDNameMapped())
        idToName.insert(pid, pname);
    for (int groupID : Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
        auto group = Configs::dataManager->groupsRepo->GetGroup(groupID);
        if (!group) continue;
        for (int profileID : group->profiles) {
            if (!idToName.contains(profileID)) continue;
            items.append({QString("[%1] %2").arg(group->name, idToName[profileID]), QString::number(profileID)});
        }
    }
    jsonEdit->setOutboundItems(items);

    issueList = new JsonEdit::JsonIssueList(this);
    issueList->attach(jsonEdit);
    layout->addWidget(issueList);

    validateLabel = new QLabel(this);
    layout->addWidget(validateLabel);
    const auto refreshStatus = [this] {
        validateLabel->setText(jsonEdit->statusText());
        const auto &tk = themeManager()->tokens;
        validateLabel->setStyleSheet(QStringLiteral("color: %1;")
                                         .arg((jsonEdit->hasErrors() ? tk.danger : tk.success).name()));
    };
    connect(jsonEdit, &JsonEdit::JsonCodeEdit::issuesChanged, this, refreshStatus);
    refreshStatus();

    auto* formatBtn = new QPushButton(tr("Format JSON"), this);
    connect(formatBtn, &QPushButton::clicked, this, [this] {
        if (!jsonEdit->formatDocument()) MessageBoxInfo(tr("Raw route"), tr("The route must be a valid JSON object"));
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    auto* bottom = new QHBoxLayout();
    bottom->addWidget(formatBtn);
    bottom->addStretch();
    bottom->addWidget(buttons);
    layout->addLayout(bottom);

    connect(buttons, &QDialogButtonBox::accepted, this, &RawRouteItem::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    resize(640, 520);
}

void RawRouteItem::accept() {
    chain->name = nameEdit->text().trimmed();
    if (chain->name.isEmpty()) {
        MessageBoxWarning(tr("Invalid operation"), tr("Cannot create Route Profile with empty name"));
        return;
    }
    const auto doc = QJsonDocument::fromJson(jsonEdit->toPlainText().toUtf8());
    if (!doc.isObject()) {
        MessageBoxWarning(tr("Invalid route"), tr("The route must be a valid JSON object"));
        return;
    }
    chain->isRaw = true;
    chain->preventModifications = preventCheck->isChecked();
    chain->rawRoute = QJsonObject2QString(doc.object(), false);
    emit settingsChanged(chain);
    QDialog::accept();
}
