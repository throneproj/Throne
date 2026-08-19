#pragma once

#include <QObject>
#include <QPalette>
#include <QColor>

// UI colours resolved from the installed palette, then contrast-repaired against `surface`.
// Use these rather than palette(...) in a QSS: stylesheet palette() resolves against
// QGuiApplication::palette() regardless of the widget, and only ever the Active group.
struct ThemeTokens {
    QColor surface;
    QColor onSurface;
    QColor accent;       // never carries text; onAccent exists for callers that must
    QColor onAccent;
    QColor selectedFill;
    QColor hoverFill;
    QColor borderSubtle;
    QColor muted;
    QColor tag;
    QColor danger;
    QColor success;
    QColor info;
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    QString system_style_name = "";
    QPalette system_palette;     // snapshot of the OS palette, taken on first apply
    QString current_theme = "0"; // int: 0:system 1+:builtin string: QStyleFactory
    ThemeTokens tokens;          // rebuilt by every ApplyTheme(), before themeChanged fires

    void ApplyTheme(const QString &theme, bool force = false);
signals:
    void themeChanged(QString themeName);
};

ThemeManager *themeManager();
