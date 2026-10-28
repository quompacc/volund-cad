// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

// N12b: the material of a rib. A slab `thickness` thick, symmetric about the plane with
// `normal`, reaches from the line `start`-`end` (extended both ways) towards `side` within that
// plane; of it, the pieces the body encloses (that stay within its bounding box once the body is
// taken away) are the rib. Throws KernelError (KernelOperation::makePrism) when there is none.
namespace voelund::kernel {

struct RibLine final {
    gp_Pnt start;
    gp_Pnt end;
    gp_Dir normal; // of the sketch plane
    gp_Dir side;   // in the plane, across the line: where the rib grows
};

[[nodiscard]] TopoDS_Shape ribMaterial(const TopoDS_Shape& body, const RibLine& line,
                                       double thickness);

} // namespace voelund::kernel
