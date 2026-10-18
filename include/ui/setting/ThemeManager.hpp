#pragma once

#include <QObject>
#include <QPalette>
#include <QColor>
#include <QHash>
#include <QIcon>
#include <QStyle>

#include <array>

// UI colours resolved from the installed palette, then contrast-repaired against `surface`.
// Use these rather than palette(...) in a QSS: stylesheet palette() resolves against
// QGuiApplication::palette() regardless of the widget, and only ever the Active group.
struct ThemeTokens {
    QColor surface;
    QColor onSurface;
    QColor accent;
    QColor muted;
    QColor tag;
    QColor danger;
    QColor success;
    QColor info;
    std::array<QColor, 5> chartSeries;
    QColor chartContext;
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    QString system_style_name = "";
    QPalette system_palette;     // snapshot of the OS palette, taken on first apply
    QString current_theme = "0"; // int: 0:system 1+:builtin string: QStyleFactory
    ThemeTokens tokens;          // rebuilt by every ApplyTheme(), before themeChanged fires

    void ApplyTheme(const QString &theme, bool force = false);

    // QStyle::standardIcon loads from the system on every call: about 9 ms for a message box icon on Windows.
    QIcon standardIcon(QStyle::StandardPixmap pixmap);
signals:
    void themeChanged(QString themeName);

private:
    QHash<int, QIcon> standardIcons; // the current style's; ApplyTheme() reloads the message box icons
};

ThemeManager *themeManager();
