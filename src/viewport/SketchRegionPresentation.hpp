// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <V3d_View.hxx>

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace voelund::viewport {

// User report 2026-10-25 (as in Fusion and FreeCAD): the faces of a visible sketch are shown
// lightly filled, the one under the pointer lights up and the chosen ones carry the selection
// colour, so the area to extrude (the floor, the wall ...) is seen before it is clicked. They
// take no clicks of their own: a click into an area goes the usual way (areaClickHandler).
class SketchRegionPresentation final {
  public:
    void show(const occ::handle<AIS_InteractiveContext>& context,
              std::span<const SceneSketchRegion> regions, const ViewportTheme& theme);
    void hide(const occ::handle<AIS_InteractiveContext>& context);
    // The region under the pointer (framebuffer pixels) lights up; true when that changed.
    bool hover(const occ::handle<AIS_InteractiveContext>& context,
               const occ::handle<V3d_View>& view, int x, int y);
    [[nodiscard]] std::size_t size() const noexcept { return shown_.size(); }
    [[nodiscard]] std::optional<std::string> hovered() const;

  private:
    struct Shown final {
        SceneSketchRegion region;
        occ::handle<AIS_Shape> shape;
    };
    void paint(Shown& shown, bool hovered) const;

    std::vector<Shown> shown_;
    std::optional<std::size_t> hovered_;
    ViewportTheme theme_;
};

} // namespace voelund::viewport
