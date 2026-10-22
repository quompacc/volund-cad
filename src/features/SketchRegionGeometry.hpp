// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include "SketchIntersectionCurves.hpp"
#include <voelund/features/SketchRegions.hpp>
namespace voelund::features::detail {
[[nodiscard]] double boundaryArea(const SketchIntersectionResult& split,
                                  const std::vector<RegionHalfEdge>& boundary);
void classifyRegionContainment(SketchRegionResult& result, const std::vector<PlanarCurve>& curves);
} // namespace voelund::features::detail
