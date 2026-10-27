// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <voelund/viewport/CadViewport.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>

#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace voelund::viewport {
// M19: construction planes as translucent squares and construction axes as lines, in the
// construction color; pickable like base planes, hidden while a sketch is being drawn.
class DatumPresentation final {
  public:
    void show(const occ::handle<AIS_InteractiveContext>& context,
              std::span<const SceneDatum> datums, double halfSize, const ViewportTheme& theme);
    void hide(const occ::handle<AIS_InteractiveContext>& context);
    // W07: the selection filter takes them out of picking (they stay shown).
    void setPickable(const occ::handle<AIS_InteractiveContext>& context, bool pickable);
    [[nodiscard]] std::optional<std::uint64_t>
    datum(const occ::handle<AIS_InteractiveObject>& object) const;
    [[nodiscard]] std::size_t displayedCount() const noexcept { return shapes_.size(); }

  private:
    std::vector<std::pair<std::uint64_t, occ::handle<AIS_Shape>>> shapes_;
};
} // namespace voelund::viewport
