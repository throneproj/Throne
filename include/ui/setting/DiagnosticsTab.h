#pragma once

#include <QWidget>

#include "ui_DiagnosticsTab.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DiagnosticsTab;
}
QT_END_NAMESPACE

class QColor;
class QLabel;
class QTimer;

class DiagnosticsTab : public QWidget {
    Q_OBJECT

public:
    explicit DiagnosticsTab(QWidget *parent = nullptr);
    ~DiagnosticsTab() override;

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void refresh();
    void refreshStart();
    void showProgress(qint64 elapsedMs);
    void saveLogs();
    static void setStatus(QLabel *label, const QString &text, const QColor &color);

    Ui::DiagnosticsTab *ui;
    QTimer *coreWatch_ = nullptr;
    QString logsStatus_;
    bool logsFailed_ = false;
};
