#include <QStyle>
#include <QApplication>
#include <QFile>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QMap>

#include <algorithm>
#include <cmath>

#include "include/ui/setting/ThemeManager.hpp"

#include <QGlobalStatic>

Q_GLOBAL_STATIC(ThemeManager, themeManagerInstance)

ThemeManager *themeManager() {
    return themeManagerInstance();
}

extern QString ReadFileText(const QString &path);

struct ThemeColors {
    QColor window, windowText;
    QColor base, alternateBase;
    QColor text;
    QColor button, buttonText;
    QColor brightText;
    QColor highlight, highlightedText;
    QColor link;            // paints the active/running config row
    QColor tooltipBase, tooltipText;
    QColor placeholder;
    QColor disabledText;
};

static QPalette buildThemePalette(const ThemeColors &c) {
    QPalette p;

    const auto setAll = [&](QPalette::ColorRole role, const QColor &col) {
        p.setColor(QPalette::Active, role, col);
        p.setColor(QPalette::Inactive, role, col);
        p.setColor(QPalette::Disabled, role, col);
    };

    setAll(QPalette::Window,          c.window);
    setAll(QPalette::WindowText,      c.windowText);
    setAll(QPalette::Base,            c.base);
    setAll(QPalette::AlternateBase,   c.alternateBase);
    setAll(QPalette::Text,            c.text);
    setAll(QPalette::Button,          c.button);
    setAll(QPalette::ButtonText,      c.buttonText);
    setAll(QPalette::BrightText,      c.brightText);
    setAll(QPalette::ToolTipBase,     c.tooltipBase);
    setAll(QPalette::ToolTipText,     c.tooltipText);
    setAll(QPalette::Highlight,       c.highlight);
    setAll(QPalette::HighlightedText, c.highlightedText);
    setAll(QPalette::Link,            c.link);
    setAll(QPalette::LinkVisited,     c.link);
    setAll(QPalette::PlaceholderText, c.placeholder);

    // Frames and bevels the stylesheet doesn't cover fall back to Qt's light defaults otherwise.
    setAll(QPalette::Light,    c.button.lighter(130));
    setAll(QPalette::Midlight, c.button.lighter(115));
    setAll(QPalette::Mid,      c.button.darker(130));
    setAll(QPalette::Dark,     c.button.darker(160));
    setAll(QPalette::Shadow,   c.window.darker(180));

    // Must follow setAll(), which wrote the Disabled group too.
    p.setColor(QPalette::Disabled, QPalette::WindowText,      c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::Text,            c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText,      c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, c.disabledText);
    p.setColor(QPalette::Disabled, QPalette::Link,            c.disabledText);

    return p;
}

// Lazy: a QPalette must not be constructed before QApplication exists. The keys also define "custom theme".
static const QMap<QString, QPalette> &customThemePalettes() {
    static const QMap<QString, QPalette> palettes = [] {
        QMap<QString, QPalette> m;

        m["flatgray"] = buildThemePalette({
            .window = "#FFFFFF", .windowText = "#57595B",
            .base = "#FFFFFF", .alternateBase = "#F6F6F6",
            .text = "#57595B",
            .button = "#F2F2F2", .buttonText = "#57595B",
            .brightText = "#FFFFFF",
            .highlight = "#D6D6D6", .highlightedText = "#2D2F31",
            .link = "#2A6CB0",
            .tooltipBase = "#FFFFFF", .tooltipText = "#57595B",
            .placeholder = "#9AA0A6", .disabledText = "#B0B0B0",
        });

        m["lightblue"] = buildThemePalette({
            .window = "#EAF7FF", .windowText = "#386487",
            .base = "#FFFFFF", .alternateBase = "#DAEFFF",
            .text = "#386487",
            .button = "#DEF0FE", .buttonText = "#386487",
            .brightText = "#FFFFFF",
            .highlight = "#C0DCF2", .highlightedText = "#1B3B57",
            .link = "#1D6FB8",
            .tooltipBase = "#EAF7FF", .tooltipText = "#386487",
            .placeholder = "#7F9DB5", .disabledText = "#A6BCCE",
        });

        m["softpink"] = buildThemePalette({
            .window = "#FFF0FB", .windowText = "#883983",
            .base = "#FFFFFF", .alternateBase = "#FBDDF5",
            .text = "#883983",
            .button = "#FCE1F6", .buttonText = "#883983",
            .brightText = "#FFFFFF",
            .highlight = "#F1C1E7", .highlightedText = "#5A2456",
            .link = "#B92BA6",
            .tooltipBase = "#FFF0FB", .tooltipText = "#883983",
            .placeholder = "#C08BBA", .disabledText = "#CBA6C6",
        });

        m["blacksoft"] = buildThemePalette({
            .window = "#444444", .windowText = "#DCDCDC",
            .base = "#444444", .alternateBase = "#525252",
            .text = "#DCDCDC",
            .button = "#484848", .buttonText = "#DCDCDC",
            .brightText = "#FFFFFF",
            .highlight = "#646464", .highlightedText = "#FFFFFF",
            .link = "#5AB0FF",
            .tooltipBase = "#484848", .tooltipText = "#DCDCDC",
            .placeholder = "#9A9A9A", .disabledText = "#808080",
        });

        // Mirrors the bundled darkstyle.qss.
        m["qdarkstyle"] = buildThemePalette({
            .window = "#19232D", .windowText = "#DFE1E2",
            .base = "#19232D", .alternateBase = "#37414F",
            .text = "#DFE1E2",
            .button = "#455364", .buttonText = "#DFE1E2",
            .brightText = "#FFFFFF",
            .highlight = "#346792", .highlightedText = "#DFE1E2",
            .link = "#6FC0FF",
            .tooltipBase = "#346792", .tooltipText = "#DFE1E2",
            .placeholder = "#9DA9B5", .disabledText = "#788D9C",
        });

        return m;
    }();
    return palettes;
}

static double relLuminance(const QColor &c) {
    const auto channel = [](double v) {
        v /= 255.0;
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c.red()) + 0.7152 * channel(c.green()) + 0.0722 * channel(c.blue());
}

static double contrastRatio(const QColor &a, const QColor &b) {
    const double la = relLuminance(a), lb = relLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

static QColor blendToward(const QColor &from, const QColor &to, double keep) {
    return QColor::fromRgbF(from.redF()   * keep + to.redF()   * (1 - keep),
                            from.greenF() * keep + to.greenF() * (1 - keep),
                            from.blueF()  * keep + to.blueF()  * (1 - keep));
}

// Walks HSL lightness away from `surface` until the ratio is met; hue and saturation survive.
static QColor separate(QColor c, const QColor &surface, double target) {
    const int dir = relLuminance(surface) > 0.5 ? -1 : 1;
    for (int i = 0; i < 24 && contrastRatio(c, surface) < target; ++i) {
        int h, s, l, a;
        c.getHsl(&h, &s, &l, &a);
        const int next = qBound(0, l + dir * 10, 255);
        if (next == l) break;
        c.setHsl(h < 0 ? 0 : h, s, next, a); // getHsl reports -1 for achromatic; s is 0 there anyway
    }
    return c;
}

static QColor readableOn(const QColor &bg) {
    return contrastRatio(Qt::white, bg) >= contrastRatio(Qt::black, bg) ? QColor(Qt::white) : QColor(Qt::black);
}

static QColor paletteAccent(const QPalette &pal) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    return pal.color(QPalette::Active, QPalette::Accent);
#else
    return pal.color(QPalette::Active, QPalette::Highlight);
#endif
}

// Prefers a surface the theme already defines, so the chip looks native to it; falls back to
// stepping the window itself and mixing in a trace of accent.
static QColor selectedFill(const QPalette &pal, const QColor &surface, const QColor &onSurface,
                           const QColor &accent) {
    for (const auto role : {QPalette::AlternateBase, QPalette::Base, QPalette::Button, QPalette::Midlight}) {
        const QColor c = pal.color(QPalette::Active, role);
        if (contrastRatio(c, surface) >= 1.35 && contrastRatio(onSurface, c) >= 4.5) return c;
    }
    return blendToward(accent, separate(surface, surface, 1.5), 0.22);
}

static ThemeTokens resolveTokens(const QPalette &pal) {
    ThemeTokens t;
    t.surface   = pal.color(QPalette::Active, QPalette::Window);
    t.onSurface = pal.color(QPalette::Active, QPalette::WindowText);

    t.accent       = separate(paletteAccent(pal), t.surface, 3.0);
    t.onAccent     = readableOn(t.accent);
    t.selectedFill = selectedFill(pal, t.surface, t.onSurface, t.accent);
    t.hoverFill    = separate(blendToward(t.accent, t.surface, 0.10), t.surface, 1.10);
    t.borderSubtle = separate(blendToward(t.onSurface, t.surface, 0.32), t.surface, 1.9);
    t.muted        = separate(blendToward(t.onSurface, t.surface, 0.62), t.surface, 4.0);
    t.tag          = separate(QColor(0xFB, 0x72, 0x99), t.surface, 4.0);
    t.danger       = separate(QColor(0xC6, 0x28, 0x28), t.surface, 4.5);
    t.success      = separate(QColor(0x2E, 0x7D, 0x32), t.surface, 4.5);
    t.info         = separate(QColor(0x32, 0x99, 0xFF), t.surface, 4.0);

    // Readability of onSurface on the chip outranks how far the chip sits from the window.
    for (int i = 0; i < 8 && contrastRatio(t.onSurface, t.selectedFill) < 4.5; ++i) {
        t.selectedFill = blendToward(t.selectedFill, t.surface, 0.6);
    }
    return t;
}

// Owns the tab chrome for every theme; literal hex only, so no rule here can resolve against
// the wrong palette or be served stale from QStyleSheetStyle's render-rule cache.
static QString overlayStyleSheet(const ThemeTokens &t) {
    const auto hex = [](const QColor &c) { return c.name(QColor::HexRgb); };
    return QStringLiteral(
        "QTabWidget::pane { margin-top: 1px; border: 1px solid %1; border-radius: 4px; }\n"
        "QTabWidget[documentMode=\"true\"]::pane { border: none; margin-top: 0px; }\n"
        "QTabWidget[documentMode=\"true\"]::tab-bar { left: 2px; }\n"
        "QTabBar { background: transparent; qproperty-drawBase: 0; }\n"
        "QTabBar::tab {\n"
        "    background: transparent;\n"
        "    color: %2;\n"
        "    border: 1px solid %1;\n"
        "    border-radius: 4px;\n"
        "    padding: 2px 4px;\n"
        "    margin-right: 1px;\n"
        "}\n"
        "QTabBar::tab:hover:!selected { background: %3; }\n"
        "QTabBar::tab:selected { background: %4; color: %2; border: 1px solid %5; }\n"
        "QTabBar::tab:disabled { color: %6; }\n"
        "*[colorRole=\"muted\"] { color: %6; }\n"
        "*[colorRole=\"tag\"] { color: %7; }\n"
        "*[colorRole=\"danger\"] { color: %8; }\n"
        "*[colorRole=\"success\"] { color: %9; }\n"
    ).arg(hex(t.borderSubtle), hex(t.onSurface), hex(t.hoverFill), hex(t.selectedFill),
          hex(t.accent), hex(t.muted), hex(t.tag), hex(t.danger), hex(t.success));
}

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->name();
        this->system_palette = qApp->palette();
    }

    if (this->current_theme == theme && !force) {
        return;
    }

    const auto lowerTheme = theme.toLower();
    const auto &palettes = customThemePalettes();
    const bool leavingCustom = palettes.contains(current_theme.toLower());
    const bool enteringCustom = palettes.contains(lowerTheme);

    QString themeSheet;

    if (enteringCustom) {
        // The whole palette goes on first, or a colour role leaks from Qt or the previous theme.
        qApp->setPalette(palettes.value(lowerTheme));
        themeSheet = lowerTheme == "qdarkstyle" ? ReadFileText(":/qdarkstyle/dark/darkstyle.qss")
                                                : ReadFileText(":/qss/" + lowerTheme + ".css");
    } else {
        if (leavingCustom) {
            // Drop the outgoing sheet before restyling, or its rules paint a frame against the
            // incoming palette. A QStyleFactory style owns its own palette.
            qApp->setStyleSheet("");
            qApp->setPalette(system_palette);
        }
        qApp->setStyle(lowerTheme == "system" ? system_style_name : theme);
    }

    // After setStyle(), which reinstalls the style's palette. Setting the sheet last is also
    // what clears the render-rule cache; a bare setPalette() does not.
    tokens = resolveTokens(qApp->palette());
    const auto sheet = themeSheet + overlayStyleSheet(tokens);
    qApp->setStyleSheet(sheet);

    // Every setStyle() above - setStyleSheet() runs one itself whenever it installs or drops the
    // proxy - refills Qt's per-class platform font table (QMenu/QAbstractItemView/QMessageBox...),
    // which outranks the app font. Re-asserting the font drops the table; the second sheet call is
    // a plain repolish that re-resolves the widgets the table already stamped (#1829).
    qApp->setFont(qApp->font());
    qApp->setStyleSheet(sheet);

    current_theme = theme;

    emit themeChanged(theme);
}
