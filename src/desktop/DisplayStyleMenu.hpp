// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <QObject>

#include <array>

class QAction;
class QToolButton;
class QWidget;

namespace voelund::desktop {

// N06 (user decision 2026-10-23): the display style of the bodies (shaded, shaded with edges,
// wireframe, hidden edges) from a small button beside the view cube and from the command
// search. A view setting, kept per machine (QSettings "view/displayStyle"), never in the
// document: no undo step, no "modified".
class DisplayStyleMenu final : public QObject {
  public:
    DisplayStyleMenu(QWidget& window, viewport::CadViewport& viewport);

    [[nodiscard]] QToolButton& button() const noexcept { return *button_; }
    [[nodiscard]] QAction& action(viewport::DisplayStyle style) const;

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void choose(viewport::DisplayStyle style);
    void place();

    viewport::CadViewport& viewport_;
    QToolButton* button_{nullptr};
    std::array<QAction*, 4> actions_{};
};

} // namespace voelund::desktop
