// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <AIS_InteractiveObject.hxx>
#include <Quantity_Color.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <utility>
#include <vector>

namespace voelund::viewport {

// W12 (as in Fusion and FreeCAD): the lines and arrows of a dimension and where its value
// sits, in world coordinates.
struct DimensionDrawing final {
    struct Arrow final {
        gp_Pnt tip;
        gp_Dir direction; // the way the arrow points
    };
    std::vector<std::pair<gp_Pnt, gp_Pnt>> segments;
    std::vector<Arrow> arrows;
    double arrowLength{0.0};
    gp_Pnt text;
};

[[nodiscard]] DimensionDrawing drawDimension(const SceneDimension& dimension);

// The drawing as one display object; never picked (the value label is). Arrowheads are
// `pixelScale` times their size on a 100 % screen.
[[nodiscard]] occ::handle<AIS_InteractiveObject>
makeDimensionGraphic(const DimensionDrawing& drawing, const gp_Dir& normal,
                     const Quantity_Color& color, double pixelScale = 1.0);

} // namespace voelund::viewport
