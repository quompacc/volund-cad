// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <span>
#include <vector>

// N14b (agent decision 2026-10-29): a spline through points. The sketch B-spline (S21: cubic,
// clamped, uniform knots 0, 1, ..., poles - 3) passes through fit point i at parameter i; two
// more poles make both ends natural (no curvature), as a drawn spline runs out straight.
namespace voelund::application::detail {

// The poles of the spline through `fit` (2 to maximumBSplinePoles - 2 points, neighbours
// apart); empty for fewer or more points, repeated neighbours or a singular system.
[[nodiscard]] std::vector<document::SketchPoint>
splineThroughPoints(std::span<const document::SketchPoint> fit);

// The spline's point (derivative 0) at `parameter` in [0, poles - 3].
[[nodiscard]] document::SketchPoint splinePoint(std::span<const document::SketchPoint> poles,
                                                double parameter);

} // namespace voelund::application::detail
