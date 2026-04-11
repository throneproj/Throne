#include <QStyle>
#include <QApplication>
#include <QFile>
#include <QPalette>

#include "include/ui/setting/ThemeManager.hpp"
#include "iostream"

ThemeManager *themeManager = new ThemeManager;

extern QString ReadFileText(const QString &path);

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->name();
    }

    if (this->current_theme == theme && !force) {
        return;
    }

    auto lowerTheme = theme.toLower();
    if (lowerTheme == "system") {
        qApp->setStyleSheet("");
        qApp->setStyle(system_style_name);
    } else if (lowerTheme == "qdarkstyle") {
        QFile f(":qdarkstyle/dark/darkstyle.qss");
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&f);
        qApp->setStyleSheet(ts.readAll());
    } else if (lowerTheme == "opusfusion") {
        QFile f(":/opusfusion/opusfusion_dark.qss"); // OpusFusion theme
        f.open(QFile::ReadOnly | QFile::Text);
        QTextStream ts(&f);
        qApp->setStyleSheet(ts.readAll());
    } else {
        qApp->setStyleSheet("");
        qApp->setStyle(theme);
    }

    current_theme = theme;

    emit themeChanged(theme);
}
