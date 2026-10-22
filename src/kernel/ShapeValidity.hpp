// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <BRepCheck_Analyzer.hxx>
#include <TopoDS_Shape.hxx>

namespace voelund::kernel::detail {

// L06: the full validity check (topology and geometry) of a result body, run in parallel over
// its subshapes. Same checks as the default sequential analyzer; on a body with many faces it
// was a third of a join's time (A-PERF-01).
[[nodiscard]] inline bool isValidShape(const TopoDS_Shape& shape) {
    return BRepCheck_Analyzer(shape, /*GeomControls=*/true, /*theIsParallel=*/true).IsValid();
}

} // namespace voelund::kernel::detail
