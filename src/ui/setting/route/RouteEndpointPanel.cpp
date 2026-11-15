#include "include/ui/setting/route/RouteEndpointPanel.h"

#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/route/RouteEditorCommon.h"
#include "include/ui/setting/route/RouteEditorWidgets.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

static QLabel* endpointPanelLabel(QWidget* parent, bool muted) {
    auto* label = new QLabel(parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    if (muted) {
        const auto apply = [label] { label->setStyleSheet(QStringLiteral("color: %1;").arg(themeManager()->tokens.muted.name(QColor::HexArgb))); };
        apply();
        QObject::connect(themeManager(), &ThemeManager::themeChanged, label, apply);
    }
    return label;
}

EndpointRulePanel::EndpointRulePanel(std::shared_ptr<Configs::RouteProfile> profile, QWidget* parent)
    : QWidget(parent), profile(std::move(profile)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    title = endpointPanelLabel(this, false);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    if (titleFont.pointSizeF() > 0) titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
    title->setFont(titleFont);
    layout->addWidget(title);
    subtitle = endpointPanelLabel(this, true);
    layout->addWidget(subtitle);

    auto* note = endpointPanelLabel(this, true);
    note->setText(tr("The Endpoints tab adds and removes this rule. Here you can only move it, to choose which rules run before it."));
    layout->addWidget(note);

    text = endpointPanelLabel(this, false);
    layout->addWidget(text);

    auto* thenRow = new QHBoxLayout();
    auto* thenLabel = endpointPanelLabel(this, false);
    thenLabel->setWordWrap(false);
    thenLabel->setText(tr("Then"));
    pill = new RouteEditor::ThenPill(this);
    thenRow->addWidget(thenLabel);
    thenRow->addWidget(pill);
    thenRow->addStretch(1);
    layout->addLayout(thenRow);

    auto* open = new QPushButton(tr("Open the Endpoints tab"), this);
    open->setAutoDefault(false);
    connect(open, &QPushButton::clicked, this, &EndpointRulePanel::openEndpointsRequested);
    layout->addWidget(open, 0, Qt::AlignLeft);
    layout->addStretch(1);
}

EndpointRulePanel::~EndpointRulePanel() = default;

void EndpointRulePanel::setRule(const std::shared_ptr<Configs::RouteRule>& rule) {
    const int profileId = rule != nullptr ? rule->outboundID : -1;
    const auto ent = Configs::dataManager->profilesRepo->GetProfile(profileId);
    const bool alive = ent != nullptr && ent->outbound != nullptr;
    const QString name = alive ? ent->outbound->DisplayName() : RouteEditor::ProfileName(profileId);
    title->setText(name);
    subtitle->setText(alive ? tr("%1 endpoint, from the Endpoints tab").arg(ent->outbound->DisplayType()) : QString());
    subtitle->setVisible(alive);

    QString body = tr("Traffic to the networks this endpoint advertises goes into its tunnel.");
    if (const int owner = RouteEditor::InnerHopOwner(*profile, profileId); owner >= 0)
        body += QLatin1Char(' ') + tr("An inner hop of %1.").arg(RouteEditor::ProfileName(owner));
    text->setText(body);
    pill->setPill(name, alive ? RouteEditor::Tone::Profile : RouteEditor::Tone::Missing);
}
