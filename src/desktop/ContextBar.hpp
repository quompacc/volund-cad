// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include "ContextCommands.hpp"

#include <QFrame>

#include <functional>
#include <string_view>
#include <vector>

class QAction;
class QHBoxLayout;
class QLabel;
class QToolButton;

namespace voelund::viewport {
class CadViewport;
}

namespace voelund::desktop {

// U04: the floating bar at the bottom of the viewport (ADR 0012). It shows the commands of
// ContextCommands as buttons on the window's own actions; the primary step is highlighted.
class ContextBar final : public QFrame {
  public:
    using ActionLookup = std::function<QAction*(std::string_view)>;

    ContextBar(viewport::CadViewport& viewport, ActionLookup lookup);

    void display(const ContextCommands& content);
    // Container for the Move/Rotate snap fields; shown only while that tool is active.
    [[nodiscard]] QWidget& transformSettings() const noexcept { return *transformSettings_; }

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* event) override;

  private:
    void place();
    // L08 (A-VIS-01): a bar wider than the view shows its buttons as symbols, names in the
    // tooltips, so no command is cut off at the window edge.
    void setCompact(bool compact);

    viewport::CadViewport& viewport_;
    ActionLookup lookup_;
    QHBoxLayout* buttons_{nullptr};
    QLabel* title_{nullptr};
    QWidget* transformSettings_{nullptr};
    std::vector<QToolButton*> current_;
    int fullWidth_{0};
    bool compact_{false};
};

} // namespace voelund::desktop
