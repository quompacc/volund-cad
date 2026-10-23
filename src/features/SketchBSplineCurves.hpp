// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/ModelDocument.hpp>
#include <voelund/kernel/SketchShapes.hpp>

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <vector>

// S21: the curve of a sketch B-spline in space (kernel::SpaceCurve::bspline): cubic, clamped,
// uniform knots 0, 1, ..., n - degree, the same parameters as its planar curve.
namespace voelund::features::detail {

inline kernel::SpaceCurve bsplineCurve3d(const document::SketchBSpline& spline,
                                         const document::SketchPlane& plane) {
    const gp_Vec normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Vec x(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Vec y = normal.Crossed(x);
    const gp_Pnt origin(plane.originX, plane.originY, plane.originZ);
    std::vector<gp_Pnt> poles;
    for (const auto& pole : spline.poles)
        poles.push_back(origin.Translated(x * pole.x + y * pole.y));
    return kernel::SpaceCurve::bspline(poles);
}

} // namespace voelund::features::detail
