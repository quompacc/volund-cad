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
class ShellHeader final : public QWidget {
  public:
    explicit ShellHeader(QWidget* parent = nullptr);

    void setFileMenu(QMenu& menu);
    void setActions(QAction& undo, QAction& redo, QAction& theme);
    void display(const ShellState& state);
    void setTheme(ThemeMode mode);

  private:
    QLabel* brand_{nullptr};
    QLabel* documentName_{nullptr};
    QLabel* saveState_{nullptr};
    QToolButton* fileButton_{nullptr};
    QToolButton* undoButton_{nullptr};
    QToolButton* redoButton_{nullptr};
    QToolButton* themeButton_{nullptr};
};

} // namespace voelund::desktop
