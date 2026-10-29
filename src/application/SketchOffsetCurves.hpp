// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include "SketchOffset.hpp"

// N14d (agent decision 2026-10-29): the offset of an ellipse, elliptic arc or B-spline is no
// curve of the same kind. It becomes a cubic spline through offset points (SketchSplineFit),
// with more points until every point of the spline lies at the distance within
// `offsetCurveTolerance`. A whole ellipse grows outwards for positive distances and becomes two
// joined halves (a sketch spline has two distinct ends); open curves move to the left of their
// direction, as an open contour. A distance beyond the curvature (the offset would turn back)
// is refused.
namespace voelund::application::detail {

inline constexpr double offsetCurveTolerance = 1.0e-3; // millimetres

[[nodiscard]] bool offsetsAsSpline(const document::SketchGeometry& geometry) noexcept;

// Throws std::invalid_argument with the reason when the offset cannot be made.
[[nodiscard]] SketchOffsetResult offsetAsSpline(const document::SketchGeometry& geometry,
                                                double distance);

} // namespace voelund::application::detail
