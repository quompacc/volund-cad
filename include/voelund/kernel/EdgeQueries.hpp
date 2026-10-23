// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax3.hxx>
#include <gp_Circ.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <memory>

class BRepBuilderAPI_Transform;

// K-GRENZE (2026-10-22): what the layers above the kernel ask of edges and small display shapes,
// without OCCT algorithms of their own.
namespace voelund::kernel {

struct ParameterRange final {
    double first;
    double last;
};
// The parameter range of the edge's curve as an adaptor sees it.
[[nodiscard]] ParameterRange curveRange(const TopoDS_Edge& edge);
// The parameter range stored on the edge (BRep_Tool::Range).
[[nodiscard]] ParameterRange storedRange(const TopoDS_Edge& edge);
// The point of the edge's curve at `parameter`.
[[nodiscard]] gp_Pnt pointOnEdge(const TopoDS_Edge& edge, double parameter);
// Both edges lie on the same curve object at the same place.
[[nodiscard]] bool sameUnderlyingCurve(const TopoDS_Edge& first, const TopoDS_Edge& second);
// An edge on the same curve (and place) as `edge` over another parameter range.
[[nodiscard]] TopoDS_Edge edgeOnSameCurve(const TopoDS_Edge& edge, double first, double last);

// A straight or circular edge as plain geometry; anything else is `other`.
struct EdgeCurve final {
    enum class Kind { line, circle, other };
    Kind kind{Kind::other};
    gp_Lin line{};
    gp_Circ circle{};
    ParameterRange range{};
    gp_Pnt start{};
    gp_Pnt end{};
};
[[nodiscard]] EdgeCurve describeEdge(const TopoDS_Edge& edge);

// Display shapes of construction geometry: a square patch of a plane and a straight segment.
[[nodiscard]] TopoDS_Face planePatch(const gp_Ax3& frame, double halfSize);
[[nodiscard]] TopoDS_Edge segmentEdge(const gp_Pnt& from, const gp_Pnt& to);

// A copy of a shape moved by a transformation that knows the image of each of its parts.
class MovedCopy final {
  public:
    MovedCopy(const TopoDS_Shape& shape, const gp_Trsf& transform);
    [[nodiscard]] const TopoDS_Shape& shape() const noexcept { return shape_; }
    // The moved part of the original, as OCCT's transformation maps it.
    [[nodiscard]] TopoDS_Shape image(const TopoDS_Shape& part) const;

  private:
    std::shared_ptr<BRepBuilderAPI_Transform> moved_;
    TopoDS_Shape shape_;
};

} // namespace voelund::kernel
