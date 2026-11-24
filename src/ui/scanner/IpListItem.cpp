#include "include/ui/scanner/IpListItem.h"

#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QTextDocument>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

#include "include/ui/setting/ThemeManager.hpp"

namespace {
    QIcon RecolorIpListItemIcon(const QString &path, const QColor &color) {
        QPixmap pixmap(path);
        if (pixmap.isNull()) return QIcon(path);
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();
        return QIcon(pixmap);
    }

    QString ipListIntervalText(const int minutes) {
        if (minutes % 1440 == 0) return IpListItem::tr("%1 d").arg(minutes / 1440);
        if (minutes % 60 == 0) return IpListItem::tr("%1 h").arg(minutes / 60);
        return IpListItem::tr("%1 min").arg(minutes);
    }

    QString ipListUpdatedText(const qint64 lastUpdate) {
        const qint64 age = QDateTime::currentSecsSinceEpoch() - lastUpdate;
        if (age < 60) return IpListItem::tr("updated just now");
        if (age < 3600) return IpListItem::tr("updated %1 min ago").arg(age / 60);
        if (age < 86400) return IpListItem::tr("updated %1 h ago").arg(age / 3600);
        return IpListItem::tr("updated %1 d ago").arg(age / 86400);
    }

    QString ipListSourceText(const Configs::IpList &list) {
        switch (list.source_kind) {
            case Configs::IpList::SourceKind::Url: {
                const QUrl url(list.source);
                return IpListItem::tr("URL %1").arg(url.host().isEmpty() ? list.source : url.host());
            }
            case Configs::IpList::SourceKind::Manual:
                break;
        }
        return {};
    }

    QLabel *makeIpListItemLabel(QWidget *parent) {
        auto *label = new QLabel(parent);
        label->setTextFormat(Qt::PlainText);
        // Ignored, so a long name elides instead of widening the row past the viewport.
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        return label;
    }

    QToolButton *makeIpListItemButton(QWidget *parent, const QString &toolTip) {
        auto *button = new QToolButton(parent);
        button->setToolTip(toolTip);
        button->setAutoRaise(true);
        auto policy = button->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        button->setSizePolicy(policy);
        return button;
    }
}

IpListItem::IpListItem(QWidget *parent, std::shared_ptr<Configs::IpList> list_, QListWidgetItem *item_,
                       const QString &ownerScanName)
    : QWidget(parent), list(std::move(list_)), item(item_) {
    setLayoutDirection(Qt::LeftToRight);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(6, 4, 6, 4);

    auto *texts = new QVBoxLayout;
    texts->setSpacing(1);
    nameLabel = makeIpListItemLabel(this);
    auto nameFont = nameLabel->font();
    nameFont.setWeight(QFont::DemiBold);
    nameLabel->setFont(nameFont);
    subtitleLabel = makeIpListItemLabel(this);
    errorLabel = makeIpListItemLabel(this);
    texts->addWidget(nameLabel);
    texts->addWidget(subtitleLabel);
    texts->addWidget(errorLabel);
    row->addLayout(texts, 1);

    busyLabel = new QLabel(tr("Refreshing…"), this);
    busyLabel->setTextFormat(Qt::PlainText);
    busyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    busyLabel->hide();
    row->addWidget(busyLabel);

    refreshButton = makeIpListItemButton(this, tr("Refresh now"));
    editButton = makeIpListItemButton(this, {});
    deleteButton = makeIpListItemButton(this, tr("Delete"));
    row->addWidget(refreshButton);
    row->addWidget(editButton);
    row->addWidget(deleteButton);

    connect(refreshButton, &QToolButton::clicked, this, [this] { emit refreshRequested(); });
    connect(editButton, &QToolButton::clicked, this, [this] { emit editRequested(); });
    connect(deleteButton, &QToolButton::clicked, this, [this] { emit deleteRequested(); });

    applyListTexts(ownerScanName);

    applyColors();
    applyIconColors();
    connect(themeManager(), &ThemeManager::themeChanged, this, [this] { applyColors(); });

    updateActions();
    updateElidedTexts();

    adjustSize();
    if (item != nullptr) item->setSizeHint(sizeHint());
}

void IpListItem::SetList(std::shared_ptr<Configs::IpList> list_, const QString &ownerScanName) {
    if (!list_) return;
    const auto oldRole = list->role;
    const bool hadError = !errorText.isEmpty();

    list = std::move(list_);
    applyListTexts(ownerScanName);
    if (list->role != oldRole) applyIconColors();
    updateActions();
    if (hadError != !errorText.isEmpty() && item != nullptr) {
        // Toggling a label invalidates only the top layout; activate() also re-reads the nested text column.
        layout()->activate();
        item->setSizeHint(sizeHint());
    }
    updateElidedTexts();
}

void IpListItem::applyListTexts(const QString &ownerScanName) {
    editButton->setToolTip(list->role == Configs::IpList::Role::User ? tr("Edit") : tr("View"));

    QStringList parts;
    parts << tr("%Ln entries", nullptr, list->entryCount);
    if (list->role == Configs::IpList::Role::ScanResult) {
        parts << (ownerScanName.isEmpty() ? tr("Result of a deleted scan") : tr("Result of scan %1").arg(ownerScanName));
    } else if (list->IsRemote()) {
        parts << ipListSourceText(*list);
        if (list->auto_update) parts << tr("auto-update every %1").arg(ipListIntervalText(std::max(30, list->update_interval)));
        parts << (list->last_update > 0 ? ipListUpdatedText(list->last_update) : tr("never updated"));
    }
    subtitleText = parts.join(QStringLiteral(" · "));
    errorText = list->last_error.isEmpty() ? QString() : tr("Last update failed: %1").arg(list->last_error);
    errorLabel->setVisible(!errorText.isEmpty());

    QString toolTip = list->name;
    if (list->IsRemote()) toolTip += "\n" + list->source;
    if (!errorText.isEmpty()) toolTip += "\n" + errorText;
    setToolTip(Qt::convertFromPlainText(toolTip));
}

void IpListItem::SetRefreshing(const bool refreshing_) {
    refreshing = refreshing_;
    busyLabel->setVisible(refreshing);
    updateActions();
}

void IpListItem::SetActionsVisible(const bool visible) {
    actionsVisible = visible;
    updateActions();
}

bool IpListItem::canRefresh() const {
    return list->IsRemote() && list->role == Configs::IpList::Role::User;
}

void IpListItem::updateActions() const {
    refreshButton->setVisible(actionsVisible && canRefresh() && !refreshing);
    editButton->setVisible(actionsVisible);
    deleteButton->setVisible(actionsVisible);
}

void IpListItem::applyColors() const {
    const auto &tokens = themeManager()->tokens;
    const auto mutedSheet = QStringLiteral("color: %1;").arg(tokens.muted.name());
    const auto dangerSheet = QStringLiteral("color: %1;").arg(tokens.danger.name());
    // setStyleSheet repolishes even when unchanged.
    if (subtitleLabel->styleSheet() != mutedSheet) subtitleLabel->setStyleSheet(mutedSheet);
    if (busyLabel->styleSheet() != mutedSheet) busyLabel->setStyleSheet(mutedSheet);
    if (errorLabel->styleSheet() != dangerSheet) errorLabel->setStyleSheet(dangerSheet);
}

void IpListItem::applyIconColors() const {
    const auto color = palette().color(QPalette::ButtonText);
    refreshButton->setIcon(RecolorIpListItemIcon(QStringLiteral(":/icon/material/history.png"), color));
    editButton->setIcon(RecolorIpListItemIcon(list->role == Configs::IpList::Role::User
                                                  ? QStringLiteral(":/icon/material/pencil-outline.png")
                                                  : QStringLiteral(":/icon/material/eye-outline.png"),
                                              color));
    deleteButton->setIcon(RecolorIpListItemIcon(QStringLiteral(":/icon/material/delete.png"), color));
}

void IpListItem::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange) applyIconColors();
    QWidget::changeEvent(event);
}

void IpListItem::updateElidedTexts() const {
    nameLabel->setText(nameLabel->fontMetrics().elidedText(list->name, Qt::ElideRight, nameLabel->width()));
    subtitleLabel->setText(subtitleLabel->fontMetrics().elidedText(subtitleText, Qt::ElideRight, subtitleLabel->width()));
    errorLabel->setText(errorLabel->fontMetrics().elidedText(errorText, Qt::ElideRight, errorLabel->width()));
}

void IpListItem::resizeEvent(QResizeEvent *event) {
    updateElidedTexts();
    QWidget::resizeEvent(event);
}
