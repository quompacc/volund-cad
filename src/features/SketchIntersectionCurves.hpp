// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <voelund/features/SketchContourGraph.hpp>
#include <voelund/kernel/PlanarCurve.hpp>
namespace voelund::features::detail {
// K-GRENZE: the curves themselves live in the kernel.
using PlanarCurve = kernel::PlanarCurve;
[[nodiscard]] kernel::CurveDefinition2d curveDefinition(const document::SketchGeometry& geometry);
[[nodiscard]] PlanarCurve planarCurve(const ContourPiece& piece);
[[nodiscard]] inline double parameterTolerance(const PlanarCurve& curve, double parameter,
                                               double distance) {
    return curve.parameterTolerance(parameter, distance);
}
[[nodiscard]] inline double domainParameter(const PlanarCurve& curve, double parameter,
                                            double distance) {
    return curve.domainParameter(parameter, distance);
}
} // namespace voelund::features::detail
