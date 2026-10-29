// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/ModelDocument.hpp>

#include <TopoDS_Shape.hxx>

#include <optional>
#include <vector>

namespace voelund::features {

// S17: a body edge projected orthogonally onto the sketch plane, in sketch coordinates: a
// line, or a circle or arc when the edge's circle lies parallel to the plane. None for other
// curves and for a line that projects to a point.
[[nodiscard]] std::optional<document::SketchGeometry>
projectSketchEdge(const document::SketchPlane& plane, const TopoDS_Shape& edge);

// N15a: the curves where the body meets the sketch plane, in sketch coordinates: lines,
// circles and arcs as they are, ellipses and pieces of them as such, any other curve as a
// cubic B-spline within 1e-4 mm (a closed one as two halves).
[[nodiscard]] std::vector<document::SketchGeometry>
sectionSketchCurves(const document::SketchPlane& plane, const TopoDS_Shape& body);

} // namespace voelund::features
