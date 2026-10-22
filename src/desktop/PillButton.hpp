// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QToolButton>

namespace voelund::desktop {

// U14 (user decision 2026-10-14): the pill of the Claude chip is the button look of the shell.
// A pill may show a painted chevron at its right edge: for its menu, or for a flyout it opens.
// The style's own arrow is switched off in the style sheet, so all chevrons look alike.
class PillButton : public QToolButton {
  public:
    explicit PillButton(QWidget* parent = nullptr);

    // Chevron without a menu (a group that opens a flyout).
    void setChevron(bool chevron);
    // Distance of the chevron's centre from the right edge.
    void setChevronInset(double inset);

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    bool chevron_{false};
    double inset_{13.0};
};

} // namespace voelund::desktop
