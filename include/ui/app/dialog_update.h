#pragma once

#include <QDialog>
#include <QUrl>

class DialogUpdate : public QDialog {
    Q_OBJECT

public:
    DialogUpdate(const QString &version, bool preRelease, const QString &notes, const QUrl &releaseUrl, bool canUpdate,
                 const QString &notice, QWidget *parent = nullptr);

signals:
    void updateRequested();
};
