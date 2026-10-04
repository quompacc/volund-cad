#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <QColor>
#include <QString>

class QWidget;

namespace voelund::desktop {

// U03: design tokens of ADR 0012. Neutral surfaces, one accent (forge ember) for selection
// and the primary action, axis colors for the base planes. Widgets use the style sheet or
// these tokens; they never hard-code colors.
enum class ThemeMode { light, dark };

struct ThemeTokens final {
    QColor background; // viewport ground
    QColor panel;
    QColor ink;
    QColor muted;
    QColor faint;
    QColor line;
    QColor soft; // hover and secondary fills
    QColor field;
    QColor accent;
    QColor accentHover;
    QColor accentSoft;
    QColor accentInk;
    QColor onAccent;
    QColor error;
    QColor errorSoft;
    QColor cubeTop;
    QColor cubeLeft;
    QColor cubeRight;
    QColor cubeEdge;
};

[[nodiscard]] const ThemeTokens& themeTokens(ThemeMode mode);
[[nodiscard]] QString themeStyleSheet(const ThemeTokens& tokens);
[[nodiscard]] viewport::ViewportTheme viewportTheme(ThemeMode mode);

// Registers the embedded fonts once and returns the UI and value font families.
[[nodiscard]] QString uiFontFamily();
[[nodiscard]] QString valueFontFamily();

// The user's last choice (per user, not per document); light when nothing is stored.
[[nodiscard]] ThemeMode storedThemeMode();
void storeThemeMode(ThemeMode mode);

} // namespace voelund::desktop
