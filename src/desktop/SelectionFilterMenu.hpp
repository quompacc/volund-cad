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

// W07 (as Fusion's selection filter): what a click or box picks outside a tool - bodies,
// faces, edges, planes and axes, sketch regions - from a small button beside the display
// style and from the command search. A view setting, kept per machine (QSettings
// "view/selectionFilter"), never in the document. While a tool is open everything stays
// pickable: the tool says what it takes.
class SelectionFilterMenu final : public QObject {
  public:
    SelectionFilterMenu(QWidget& window, viewport::CadViewport& viewport);

    [[nodiscard]] QToolButton& button() const noexcept { return *button_; }
    // Applies the chosen filter, or none while a tool is open.
    void sync(bool toolOpen);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    [[nodiscard]] viewport::SelectionFilter chosen() const;
    void changed();
    void showFiltered(bool filtered);
    void place();

    viewport::CadViewport& viewport_;
    QToolButton* button_{nullptr};
    std::array<QAction*, 5> actions_{};
    bool toolOpen_{false};
};

} // namespace voelund::desktop
