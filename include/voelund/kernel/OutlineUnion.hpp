// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/PlanarCurve.hpp>

#include <gp_Pnt2d.hxx>

#include <array>
#include <functional>
#include <vector>

// N15c: closed outlines (a font's glyph contours) as the boundary of the area they fill. The
// contours split the plane into cells; `filled` decides for a point inside each cell (the font's
// fill rule), and the filled cells are merged. So overlapping contours and contours that cross
// themselves (as variable fonts draw letters) give one clean outline. A failing OCCT computation
// is reported as KernelError.
namespace voelund::kernel {

// One piece of a contour: a straight segment (its first two points) or a cubic Bézier segment
// (all four).
struct OutlineSegment final {
    bool straight;
    std::array<gp_Pnt2d, 4> points;
};
using OutlineContour = std::vector<OutlineSegment>;

// A piece of the boundary: a straight segment or a cubic B-spline (flat knots).
struct OutlinePiece final {
    bool straight;
    gp_Pnt2d start;
    gp_Pnt2d end;
    BSpline2d spline;
};

[[nodiscard]] std::vector<OutlinePiece>
mergedOutline(const std::vector<OutlineContour>& contours,
              const std::function<bool(const gp_Pnt2d&)>& filled);

} // namespace voelund::kernel
