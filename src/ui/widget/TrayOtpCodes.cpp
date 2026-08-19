#include "include/ui/widget/TrayOtpCodes.hpp"

#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>

#include "include/database/DatabaseManager.h"
#include "include/database/OtpProfilesRepo.h"
#include "include/ui/setting/OtpItem.h"

namespace {
    constexpr int TICK_MS = 1000;
    constexpr int LIST_MIN_HEIGHT = 280;
    constexpr int POPUP_MIN_WIDTH = 380;
}

TrayOtpCodes::TrayOtpCodes(QWidget *parent) : TrayPopupFrame(parent) {
    setMinimumWidth(POPUP_MIN_WIDTH);

    auto *list = new QListWidget(m_card);
    list->setUniformItemSizes(true);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    list->setMinimumHeight(LIST_MIN_HEIGHT);
    m_cardLayout->addWidget(list, 1);
    setListWidget(list);

    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuild(); });
    connect(m_search, &QLineEdit::returnPressed, this, [this] {
        const QListWidgetItem *item = m_list->currentItem();
        if (!item && m_list->count() > 0) item = m_list->item(0);
        copyCurrent(item);
    });
    // Deliberately stays open, so several codes can be taken in a row.
    connect(m_list, &QListWidget::itemClicked, this, [this](const QListWidgetItem *item) { copyCurrent(item); });

    ticker = new QTimer(this);
    ticker->setInterval(TICK_MS);
    connect(ticker, &QTimer::timeout, this, [this] { refreshCodes(); });
    ticker->start();
}

void TrayOtpCodes::reload() {
    profiles = Configs::dataManager->otpProfilesRepo->GetAllOtpProfiles();
}

void TrayOtpCodes::rebuild() {
    const auto query = m_search->text().trimmed().toLower();

    m_list->clear();
    shown.clear();
    for (int i = 0; i < profiles.size(); ++i) {
        const auto &profile = profiles[i];
        if (!query.isEmpty() && !profile->name.toLower().contains(query)
            && !profile->issuer.toLower().contains(query))
            continue;
        shown.append(i);
        auto *item = new QListWidgetItem(m_list);
        m_list->setItemWidget(item, new OtpItem(m_list, profile, item, OtpItem::Mode::ReadOnly));
    }

    if (m_list->count() == 0) {
        auto *empty = new QListWidgetItem(profiles.isEmpty() ? tr("No OTP profiles yet") : tr("No matches"));
        empty->setFlags(Qt::NoItemFlags);
        m_list->addItem(empty);
    }
    refreshCodes();
}

void TrayOtpCodes::refreshCodes() const {
    for (int row = 0; row < m_list->count(); ++row) {
        if (auto *widget = qobject_cast<OtpItem *>(m_list->itemWidget(m_list->item(row)))) widget->Refresh();
    }
}

void TrayOtpCodes::copyCurrent(const QListWidgetItem *item) {
    if (item == nullptr) return;
    const int row = m_list->row(item);
    if (row < 0 || row >= shown.size()) return;

    const auto &profile = profiles[shown[row]];
    const auto code = profile->CurrentCode();
    if (code.isEmpty()) {
        QToolTip::showText(QCursor::pos(), tr("Invalid secret"), m_list);
        return;
    }
    QGuiApplication::clipboard()->setText(code);
    QToolTip::showText(QCursor::pos(), tr("Copied"), m_list);
}

void TrayOtpCodes::preparePopup() {
    clearSearch();
    reload();
    rebuild();
}

bool TrayOtpCodes::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (watched == m_list && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
            copyCurrent(m_list->currentItem());
            return true;
        }
    }
    return TrayPopupFrame::eventFilter(watched, event);
}
