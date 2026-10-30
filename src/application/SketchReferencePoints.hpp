// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <optional>
#include <vector>

// The points a relation can name on each kind of element, for picking and dragging them (S08;
// N14c adds the elliptic arc's ends and center).
namespace voelund::application::detail {

// The named point of `geometry` nearest to `position`; none for a rectangle (picked whole).
[[nodiscard]] std::optional<document::SketchPointPosition>
nearestReferencePoint(const document::SketchGeometry& geometry, document::SketchPoint position);

// VP01: every point a relation can name on `geometry`, the ones held while it is dragged whole.
[[nodiscard]] std::vector<document::SketchPointPosition>
dragPoints(const document::SketchGeometry& geometry);

// Moves the named point by (dx, dy): an end of a line or B-spline, a center, or an end of an
// arc or elliptic arc along its curve. False when the element has no such point.
[[nodiscard]] bool moveReferencePoint(document::SketchGeometry& geometry,
                                      document::SketchPointPosition point, double dx, double dy);

} // namespace voelund::application::detail
