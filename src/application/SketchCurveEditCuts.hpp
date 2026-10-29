// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/SketchGeometry.hpp>
#include <voelund/kernel/PlanarCurve.hpp>

#include <optional>
#include <vector>

// N14c: cuts for trim and extend where the edited curve or the cutting element is an ellipse,
// an elliptic arc or a B-spline. They come from the kernel's curve intersection; lines, arcs
// and circles among themselves keep their analytic cuts (SketchCurveEditPlan.cpp).
namespace voelund::application::detail {

struct CurveCut final {
    document::SketchPoint point;
    std::optional<document::SketchPointPosition> cutterEnd; // the cut is the cutter's end
    bool onObject; // the cutter can carry a point-on-object relation (a line or circle)
};

// N14c2: the sketch B-spline as the kernel's planar one, and back.
[[nodiscard]] kernel::BSpline2d planarSpline(const document::SketchBSpline& spline);
[[nodiscard]] document::SketchBSpline sketchSpline(const kernel::BSpline2d& spline);

// Whether cuts with this element need the kernel (an ellipse, elliptic arc or B-spline).
[[nodiscard]] bool needsCurveCuts(const document::SketchGeometry& geometry) noexcept;

// Where `support` (the edited curve's whole line, circle or ellipse) meets the element's curves
// within their bounds; `nullSegment` decides when a cut is the cutter's end.
[[nodiscard]] std::vector<CurveCut> curveCuts(const kernel::CurveDefinition2d& support,
                                              const document::SketchGeometry& cutter,
                                              double tolerance, double nullSegment);

} // namespace voelund::application::detail
