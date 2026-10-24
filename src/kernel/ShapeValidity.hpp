// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <BRepCheck_Analyzer.hxx>
#include <TopoDS_Shape.hxx>

namespace voelund::kernel::detail {

// The full validity check (topology and geometry) of a result body. Sequential again
// (2026-10-24): L06 ran it in parallel over the subshapes, but OCCT's parallel analyzer
// corrupts the heap now and then (BRepCheck_Vertex::InContext; application.fillet aborted in 4
// of 20 Release runs, CI 725 and 750; 0 of 20 sequential).
[[nodiscard]] inline bool isValidShape(const TopoDS_Shape& shape) {
    return BRepCheck_Analyzer(shape, /*GeomControls=*/true, /*theIsParallel=*/false).IsValid();
}

} // namespace voelund::kernel::detail
