#pragma once

#include <QPointer>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QMenu;
class QToolButton;

namespace Validation {
    class Form;
}

// One DNS server setting as reorderable rows, stored as a comma-separated list.
class DnsServerListEdit : public QWidget {
    Q_OBJECT

public:
    explicit DnsServerListEdit(QWidget *parent = nullptr);

    void setServers(const QString &commaList);

    [[nodiscard]] QString servers() const;

    void setPresets(const QStringList &presets);

    void setRecent(const QStringList &history);

    void setPrimaryNote(bool shown);

    void bindTo(Validation::Form *form, const QString &label);

signals:
    void changed();

protected:
    bool event(QEvent *event) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    class Rows;

    [[nodiscard]] QString problem() const;

    [[nodiscard]] int selectedRow() const;

    void rebuildRows(int select);

    void refreshMetrics();

    void queueMetrics();

    void syncSelection();

    void fillAddMenu();

    void addServers(const QStringList &entered);

    void removeAddress(const QString &address, bool selectNext);

    void moveServer(int from, int to);

    void editServer(int row);

    bool closeRowEditor(bool apply);

    void commitRowEdit(bool dropInvalid);

    void cancelRowEdit();

    void openCustom();

    void closeCustom();

    void commitCustom();

    void cancelCustom();

    void notifyChanged();

    void revalidate();

    QStringList addresses;
    QStringList presetServers;
    QStringList recentServers;
    bool primaryNote = false;
    Rows *rows = nullptr;
    QLineEdit *customEdit = nullptr;
    QToolButton *addButton = nullptr;
    QMenu *addMenu = nullptr;
    QLabel *raceHint = nullptr;
    QPointer<QLineEdit> rowEditor;
    int editedRow = -1;
    int emptyRowHeight = 0;
    bool metricsQueued = false;
    QPointer<Validation::Form> form;
};
