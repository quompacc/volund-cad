// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/ShapeOperations.hpp>

#include <span>

// N13a: direct edits of a solid's faces. Faces keep their history (HistoryOperand::first).
// A result that is no valid solid throws KernelError (KernelOperation::evaluation).
namespace voelund::kernel {

// The faces moved `distance` along their outward normals (negative: inwards); the faces around
// them follow (OCCT offset with a value per face).
[[nodiscard]] ShapeOperationResult offsetFacesWithHistory(const TopoDS_Shape& input,
                                                          std::span<const TopoDS_Shape> faces,
                                                          double distance);
// The faces removed and the gap closed by extending the faces around them (OCCT defeaturing):
// a chamfer or fillet heals back to its edge, a boss or hole disappears.
[[nodiscard]] ShapeOperationResult removeFacesWithHistory(const TopoDS_Shape& input,
                                                          std::span<const TopoDS_Shape> faces);

} // namespace voelund::kernel
