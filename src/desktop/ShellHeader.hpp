#pragma once

#include "OverlayCard.hpp"
#include "ShellState.hpp"
#include "Theme.hpp"

#include <functional>

class QAction;
class QLabel;
class QMenu;
class QToolButton;

namespace voelund::desktop {

// U03: the slim header of ADR 0012 — brand, document name with saved state, file menu,
// undo/redo and the theme switch. It shows ShellState and triggers registered actions only.
// U14: two cards over the viewport — the project card top left (file menu, brand, document,
// saved state) and this header card top right (undo/redo, search, rail, theme, Claude).
// AI04c: state of the local Claude access (MCP).
enum class AssistantState { off, ready, connected };

class ShellHeader final : public OverlayCard {
  public:
    explicit ShellHeader(QWidget& host);

    void setFileMenu(QMenu& menu);
    void setActions(QAction& undo, QAction& redo, QAction& theme);
    // U04: command search field and the tool rail switch.
    void setViewActions(QAction& search, QAction& toolRail);
    void display(const ShellState& state);
    // AI04c: the Claude chip right of the theme switch and its menu.
    void setAssistantMenu(QMenu& menu);
    // AI03b: a click on the chip itself opens the Claude panel; the arrow opens the menu.
    void setAssistantClicked(std::function<void()> clicked);
    void showAssistant(AssistantState state);
    void flashAssistant();
    // Hidden while AI functions are switched off.
    void setAssistantVisible(bool visible);
    void setTheme(ThemeMode mode);

  private:
    void setCompact(bool compact);
    void showAssistantDot();

    bool compact_{false};
    ThemeMode mode_{ThemeMode::light};
    AssistantState assistant_{AssistantState::off};
    OverlayCard* project_{nullptr};
    QLabel* brand_{nullptr};
    QLabel* documentName_{nullptr};
    QLabel* saveState_{nullptr};
    QToolButton* fileButton_{nullptr};
    QToolButton* undoButton_{nullptr};
    QToolButton* redoButton_{nullptr};
    QToolButton* themeButton_{nullptr};
    QToolButton* searchButton_{nullptr};
    QToolButton* railButton_{nullptr};
    QToolButton* assistantButton_{nullptr};
};

} // namespace voelund::desktop
