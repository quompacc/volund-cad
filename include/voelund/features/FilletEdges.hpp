#pragma once

#include <voelund/document/ModelDocument.hpp>
#include <voelund/features/SupportProvenance.hpp>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

#include <optional>
#include <span>
#include <vector>

// M05/M06 (user decision 2026-10-12): an edge of a body is named by its two adjacent faces.
namespace voelund::features {

// The named edge at a picked body edge: the two named faces that contain it. None when the
// edge does not lie between two distinct named faces (e.g. a seam or an unnamed fillet face).
[[nodiscard]] std::optional<document::FilletEdge>
filletEdgeAt(std::span<const SupportProvenance> origins, const TopoDS_Shape& edge);

// The body edges where the two named faces meet now; empty when a face is gone or the faces
// no longer meet. A split face can meet the other in several pieces.
[[nodiscard]] std::vector<TopoDS_Edge> filletEdgeShapes(std::span<const SupportProvenance> origins,
                                                        const document::FilletEdge& edge);

// The same edge in either face order.
[[nodiscard]] bool sameFilletEdge(const document::FilletEdge& a,
                                  const document::FilletEdge& b) noexcept;

} // namespace voelund::features
