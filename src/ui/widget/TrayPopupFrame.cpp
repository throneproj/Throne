#include "include/ui/widget/TrayPopupFrame.hpp"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPalette>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

TrayPopupFrame::TrayPopupFrame(QWidget *parent) : QFrame(parent) {
    // A transient, always-on-top tool window. Translucent so the inner rounded
    // "card" shows anti-aliased corners (a bitmap mask would be jagged).
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setFrameShape(QFrame::NoFrame);

    const QString bg = palette().color(QPalette::Window).name();
    const QString base = palette().color(QPalette::Base).name();
    const QString border = palette().color(QPalette::Mid).name();

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("trayCard"));
    m_card->setStyleSheet(QStringLiteral(
        "QFrame#trayCard { background-color:%1; border:1px solid %2; border-radius:10px; }")
        .arg(bg, border));
    outer->addWidget(m_card);

    m_cardLayout = new QVBoxLayout(m_card);
    m_cardLayout->setContentsMargins(10, 10, 10, 10);
    m_cardLayout->setSpacing(6);

    auto *searchRow = new QHBoxLayout();
    m_search = new QLineEdit(m_card);
    m_search->setObjectName(QStringLiteral("traySearch"));
    m_search->setPlaceholderText(tr("Search…"));
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    m_search->setStyleSheet(QStringLiteral(
        "QLineEdit#traySearch { border:1px solid %1; border-radius:8px; padding:5px 9px; background-color:%2; }")
        .arg(border, base));
    auto *closeBtn = new QPushButton(QStringLiteral("✕"), m_card);
    closeBtn->setFixedWidth(28);
    closeBtn->setToolTip(tr("Close"));
    searchRow->addWidget(m_search, 1);
    searchRow->addWidget(closeBtn);
    m_cardLayout->addLayout(searchRow);

    connect(closeBtn, &QPushButton::clicked, this, [this] { close(); });
}

void TrayPopupFrame::clearSearch() {
    m_search->blockSignals(true);
    m_search->clear();
    m_search->blockSignals(false);
}

void TrayPopupFrame::setListWidget(QListWidget *list) {
    m_list = list;
    if (m_list) m_list->installEventFilter(this);
}

void TrayPopupFrame::popupAt(const QPoint &globalPos) {
    preparePopup();
    adjustSize();

    QScreen *scr = QGuiApplication::screenAt(globalPos);
    if (!scr) scr = QGuiApplication::primaryScreen();
    const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1024, 768);
    const QSize sz = size();
    int x = globalPos.x();
    int y = globalPos.y();
    if (x + sz.width() > avail.right()) x = avail.right() - sz.width();
    if (y + sz.height() > avail.bottom()) y = avail.bottom() - sz.height();
    if (x < avail.left()) x = avail.left();
    if (y < avail.top()) y = avail.top();
    move(x, y);

    show();
    raise();
    activateWindow();
    m_search->setFocus();
    afterShow();
}

void TrayPopupFrame::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QFrame::keyPressEvent(event);
}

bool TrayPopupFrame::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            close();
            return true;
        }
        if (m_list && watched == m_search && key->key() == Qt::Key_Down && m_list->count() > 0) {
            m_list->setFocus();
            m_list->setCurrentRow(0);
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}
