#pragma once

#include <QList>
#include <QString>

class QDialog;

// Searchable settings, read from the settings dialogs' .ui forms without running the dialogs.
namespace SettingsIndex {
    struct Item {
        // Class of the dialog that shows the setting, as metaObject()->className() reports it.
        QString dialogClass;
        // The widget to reveal in that dialog.
        QString objectName;
        QString label;
        // Tabs and group boxes inside the dialog: "Logging › Log Filtering".
        QString path;
        QString keywords;
    };

    QList<Item> Build();

    // Selects the tabs that hold objectName, then focuses the setting and briefly outlines it.
    void Reveal(QDialog *dialog, const QString &objectName);
}
