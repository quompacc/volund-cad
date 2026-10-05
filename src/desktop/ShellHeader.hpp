#pragma once

#include "ShellState.hpp"
#include "Theme.hpp"

#include <QWidget>

class QAction;
class QLabel;
class QMenu;
class QToolButton;

namespace voelund::desktop {

// U03: the slim header of ADR 0012 — brand, document name with saved state, file menu,
// undo/redo and the theme switch. It shows ShellState and triggers registered actions only.
// AI04c: state of the local Claude access (MCP).
enum class AssistantState { off, ready, connected };

class ShellHeader final : public QWidget {
  public:
    explicit ShellHeader(QWidget* parent = nullptr);

    void setFileMenu(QMenu& menu);
    void setActions(QAction& undo, QAction& redo, QAction& theme);
    // U04: command search field and the tool rail switch.
    void setViewActions(QAction& search, QAction& toolRail);
    void display(const ShellState& state);
    // AI04c: the Claude chip right of the theme switch and its menu.
    void setAssistantMenu(QMenu& menu);
    void showAssistant(AssistantState state);
    void flashAssistant();
    // Hidden while AI functions are switched off.
    void setAssistantVisible(bool visible);
    void setTheme(ThemeMode mode);

  private:
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
