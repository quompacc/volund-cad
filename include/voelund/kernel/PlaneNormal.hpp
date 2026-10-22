// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopAbs_Orientation.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>

namespace voelund::kernel {
// The normal of a planar face as it lies in its shape. M09: a mirrored plane has an indirect
// frame; its surface normal then points against the frame's main direction.
[[nodiscard]] inline gp_Dir planeNormal(const gp_Pln& plane, TopAbs_Orientation orientation) {
    gp_Dir normal = plane.Axis().Direction();
    if (!plane.Position().Direct())
        normal.Reverse();
    if (orientation == TopAbs_REVERSED)
        normal.Reverse();
    return normal;
}
} // namespace voelund::kernel
