// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Wire.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

// N11c: places along a path wire for a pattern along a curve. Distances are arc lengths in mm
// from the wire's start; failures throw KernelError (KernelOperation::pattern).
namespace voelund::kernel {

struct PathFrame final {
    gp_Pnt point;
    gp_Dir tangent; // in the wire's direction
};

[[nodiscard]] double wireLength(const TopoDS_Wire& wire);
[[nodiscard]] bool isClosedWire(const TopoDS_Wire& wire);
// The point `distance` along the wire (0 to its length) and the tangent there.
[[nodiscard]] PathFrame wireFrameAt(const TopoDS_Wire& wire, double distance);

} // namespace voelund::kernel
