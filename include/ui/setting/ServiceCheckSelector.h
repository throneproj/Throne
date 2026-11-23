#pragma once

#include <QList>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QGridLayout;
class QLabel;

namespace Engine::ServiceChecks {
    struct Definitions;
}

// Turns Service Check's services on and off, one row per category; the definitions load off the UI thread.
class ServiceCheckSelector : public QWidget {
    Q_OBJECT

public:
    explicit ServiceCheckSelector(const QStringList &disabledIds, QWidget *parent = nullptr);

    // The unchecked ids plus the given ones the loaded definitions do not list; the given list until they load.
    [[nodiscard]] QStringList disabledIds() const;

protected:
    void changeEvent(QEvent *event) override;

private:
    struct Entry {
        QCheckBox *box = nullptr;
        QString id;
        QString name;
    };

    void populate(const Engine::ServiceChecks::Definitions &definitions);
    void showMessage(const QString &text);
    void refreshIcons();

    QStringList disabled_;
    QGridLayout *grid_ = nullptr;
    QLabel *message_ = nullptr;
    QList<Entry> entries_;
    bool loaded_ = false;
};
