#pragma once

#include <QFrame>

class QKeyEvent;
class QLineEdit;
class QListWidget;
class QVBoxLayout;

// Shared chrome for the system-tray popups (profile picker, OTP codes): a
// frameless always-on-top tool window, a rounded card, a search row with a
// close button, and screen-clamped positioning.
//
// Subclasses own the list (and any extra header/footer) and the data that
// fills it. Search/list keyboard handling that is common (Esc, Down from the
// search box) lives here; item activation stays in the subclass.
class TrayPopupFrame : public QFrame {
    Q_OBJECT

public:
    explicit TrayPopupFrame(QWidget *parent = nullptr);

    // Rebuild contents, size to fit, place near globalPos (kept fully on the
    // containing screen), then show and focus the search box.
    void popupAt(const QPoint &globalPos);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    virtual void preparePopup() = 0;
    virtual void afterShow() {}

    void clearSearch();
    void setListWidget(QListWidget *list);

    QFrame *m_card = nullptr;
    QVBoxLayout *m_cardLayout = nullptr;
    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
};
