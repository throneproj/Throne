#pragma once

#include <QCoreApplication>
#include <QFileDialog>
#include <QTimer>

// main.cpp keeps AA_DontUseNativeDialogs set so message boxes stay Qt-drawn; file pickers opt back in to the platform dialog.
namespace NativeFileDialog {
    namespace detail {
        template <typename Picker>
        auto run(Picker &&picker) {
            // Qt reads the attribute only until the picker is up, so it is restored on the picker loop's first turn.
            QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, false);
            QTimer::singleShot(0, qApp, [] { QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true); });
            auto result = picker();
            QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
            return result;
        }
    } // namespace detail

    inline QString getOpenFileName(QWidget *parent = nullptr, const QString &caption = {}, const QString &dir = {}, const QString &filter = {},
                                   QString *selectedFilter = nullptr, QFileDialog::Options options = {}) {
        return detail::run([&] { return QFileDialog::getOpenFileName(parent, caption, dir, filter, selectedFilter, options); });
    }

    inline QStringList getOpenFileNames(QWidget *parent = nullptr, const QString &caption = {}, const QString &dir = {}, const QString &filter = {},
                                        QString *selectedFilter = nullptr, QFileDialog::Options options = {}) {
        return detail::run([&] { return QFileDialog::getOpenFileNames(parent, caption, dir, filter, selectedFilter, options); });
    }

    inline QString getSaveFileName(QWidget *parent = nullptr, const QString &caption = {}, const QString &dir = {}, const QString &filter = {},
                                   QString *selectedFilter = nullptr, QFileDialog::Options options = {}) {
        return detail::run([&] { return QFileDialog::getSaveFileName(parent, caption, dir, filter, selectedFilter, options); });
    }
} // namespace NativeFileDialog
