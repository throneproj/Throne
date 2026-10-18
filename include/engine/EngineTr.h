#pragma once

#include <QCoreApplication>

// Code moved out of a dialog keeps that dialog's translation context; -no-obsolete lupdate runs would drop it otherwise.
struct MainWindowTr {
    Q_DECLARE_TR_FUNCTIONS(MainWindow)
};

struct BasicSettingsTr {
    Q_DECLARE_TR_FUNCTIONS(DialogBasicSettings)
};
