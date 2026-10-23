// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include "FloatingCard.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QToolButton;

namespace voelund::desktop {

// N06 (user decision 2026-10-23): the properties of a selected body: its colour from ten
// swatches, "Eigene…" (a colour dialog) or "Standard" (the theme's colour). The card shows the
// body and reports a choice; it changes nothing itself. It takes the slot of the feature
// properties while a body row is selected.
class BodyColorCard final : public FloatingCard {
  public:
    using Choice = std::function<void(std::optional<std::uint32_t> color)>;

    explicit BodyColorCard(QWidget& host);

    [[nodiscard]] static const std::array<std::uint32_t, 10>& palette() noexcept;
    void setChoiceHandler(Choice handler) { choice_ = std::move(handler); }
    void display(const QString& bodyName, std::optional<std::uint32_t> color);

  private:
    void chooseCustom();

    std::vector<QToolButton*> swatches_;
    QToolButton* custom_{nullptr};
    QToolButton* standard_{nullptr};
    std::optional<std::uint32_t> color_;
    Choice choice_;
};

} // namespace voelund::desktop
