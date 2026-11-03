#include "include/ui/widget/ResolverSourceWidget.h"

#include "include/database/DatabaseManager.h"
#include "include/database/IpListsRepo.h"
#include "include/global/Utils.hpp"
#include "include/ui/scanner/ScanItem.h"
#include "include/ui/utils/MyPlainTextEdit.h"
#include "include/ui/utils/NoWheelComboBox.h"
#include "include/ui/utils/ValidationForm.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {
    QString resolverSourceLineCheck(Configs::ResolverFlavor flavor, const QString &line) {
        switch (flavor) {
            case Configs::ResolverFlavor::Dnstt:
                return Configs::Checks::DnsttResolverLine(line);
            case Configs::ResolverFlavor::MasterDns:
                return Configs::Checks::MasterDnsResolverLine(line);
            case Configs::ResolverFlavor::Xdns:
                break;
        }
        return Configs::Checks::XdnsResolverLine(line);
    }

    bool resolverSourceListGone(int id) {
        if (id < 0 || Configs::dataManager == nullptr || Configs::dataManager->ipListsRepo == nullptr) return false;
        const auto list = Configs::dataManager->ipListsRepo->GetIpListHeader(id);
        return list == nullptr || list->IsHidden();
    }
}

ResolverSourceWidget::ResolverSourceWidget(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *lines = new MyPlainTextEdit(this);
    lines->setVisibleLines(4);
    lines->setTabChangesFocus(true);
    lines->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_lines = lines;
    layout->addWidget(m_lines);

    m_row = new QHBoxLayout;
    m_ipListLabel = new QLabel(tr("IP list"), this);
    m_ipList = new NoWheelComboBox(this);
    m_ipList->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_ipList->setMinimumContentsLength(16);
    m_ipListLabel->setBuddy(m_ipList);
    const auto hint = tr("Its addresses are used after the lines above; an entry without a port uses port 53.");
    m_ipListLabel->setToolTip(hint);
    m_ipList->setToolTip(hint);
    m_row->addWidget(m_ipListLabel);
    m_row->addWidget(m_ipList, 1);
    m_find = new QPushButton(tr("Find resolvers…"), this);
    m_find->setAutoDefault(false);
    m_row->addWidget(m_find);
    layout->addLayout(m_row);

    setFlavor(m_flavor);
    setIpList(-1);
    setProfileId(-1);
    connect(m_lines, &QPlainTextEdit::textChanged, this, &ResolverSourceWidget::changed);
    connect(m_ipList, &QComboBox::currentIndexChanged, this, &ResolverSourceWidget::changed);
    connect(m_find, &QPushButton::clicked, this, [this] { ScannerUi::FindResolvers(m_profileId); });
}

void ResolverSourceWidget::setFlavor(Configs::ResolverFlavor flavor) {
    m_flavor = flavor;
    m_lines->setPlaceholderText(flavor == Configs::ResolverFlavor::Xdns ? tr("One per line. Replaces the resolvers in Finalmask; empty keeps them.")
                                                                         : tr("One per line. Empty uses the system resolver."));
}

void ResolverSourceWidget::load(const Configs::ResolverSource &source) {
    m_lines->setPlainText(source.lines.join(u'\n'));
    setIpList(source.ipListId);
}

Configs::ResolverSource ResolverSourceWidget::save() const {
    Configs::ResolverSource source;
    source.lines = SplitAndTrim(m_lines->toPlainText(), "\n", false);
    const auto id = m_ipList->currentData();
    source.ipListId = id.isValid() ? id.toInt() : -1;
    return source;
}

void ResolverSourceWidget::setIpList(int id) {
    m_ipList->clear();
    m_ipList->addItem(tr("None"), -1);
    if (Configs::dataManager != nullptr && Configs::dataManager->ipListsRepo != nullptr) {
        for (const auto &list : Configs::dataManager->ipListsRepo->GetAllIpLists()) m_ipList->addItem(list->name, list->id);
    }
    // A gone list stays selected, so saving never points the profile at another list by itself.
    if (id >= 0 && m_ipList->findData(id) < 0) m_ipList->insertItem(1, tr("Missing list"), id);
    m_ipList->setCurrentIndex(std::max(0, m_ipList->findData(id)));
}

void ResolverSourceWidget::setProfileId(int id) {
    m_profileId = id;
    m_find->setEnabled(id >= 0);
    m_find->setToolTip(id >= 0 ? tr("Opens a scan that finds the resolvers relaying this profile's tunnel, as last saved.")
                               : tr("Save the profile first; the scan tests the saved profile."));
}

void ResolverSourceWidget::bindValidation(Validation::Form &form, QObject *owner) {
    if (owner == nullptr) owner = this;
    form.bindLines(m_lines, [this](const QString &line) { return resolverSourceLineCheck(m_flavor, line); }, owner).label(tr("Resolvers"));
    form.addRule({m_ipList}, [this] {
        return resolverSourceListGone(save().ipListId) ? tr("This IP list no longer exists, so it adds no resolvers.") : QString();
    }, owner, Validation::Severity::Warning, m_ipListLabel->text());
}
