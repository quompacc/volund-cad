// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/ShapeOperations.hpp>

#include <Geom_Surface.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Trsf.hxx>

#include <optional>
#include <span>
#include <vector>

// I05: faces of a solid on new surfaces with the topology kept: an edge becomes the intersection
// of its two faces' surfaces, a vertex the meeting point of its faces. This is what OCCT's draft
// does for tilted planes; it works on real parts where OCCT's offset gives up. It fails, with a
// KernelError, when the topology would have to change (a neighbour vanishes) or when two tangent
// faces no longer meet.
namespace voelund::kernel::detail {

struct FaceReplacement final {
    TopoDS_Face face;
    occ::handle<Geom_Surface> surface;
    // The faces moved as one rigid part share this transformation. Edges between them move with
    // them, even where the faces are tangent.
    std::optional<gp_Trsf> rigid{};
    int group{0};
};

[[nodiscard]] ShapeOperationResult replaceFaces(const TopoDS_Shape& input,
                                                std::span<const FaceReplacement> faces);

} // namespace voelund::kernel::detail
