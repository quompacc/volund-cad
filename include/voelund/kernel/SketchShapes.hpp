// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>

#include <memory>
#include <optional>
#include <span>

class BRepBuilderAPI_MakeWire;

// K-GRENZE (2026-10-22): edges, wires and faces of sketches for the evaluation above the
// kernel. A failing OCCT computation is reported as KernelError.
namespace voelund::kernel {

// A sketch curve in space with the parameters of its planar curve (PlanarCurve): a line by
// length from its start, conics by angle from their frame's x axis, a B-spline as in the sketch.
class SpaceCurve final {
  public:
    [[nodiscard]] static SpaceCurve line(const gp_Pnt& start, const gp_Pnt& through);
    [[nodiscard]] static SpaceCurve circle(const gp_Ax2& frame, double radius);
    [[nodiscard]] static SpaceCurve ellipse(const gp_Ax2& frame, double majorRadius,
                                            double minorRadius);
    [[nodiscard]] static SpaceCurve bspline(std::span<const gp_Pnt> poles);

    [[nodiscard]] gp_Pnt value(double parameter) const;
    [[nodiscard]] TopoDS_Edge edge() const; // the whole curve
    [[nodiscard]] TopoDS_Edge edge(double first, double last) const;
    // The edge over [first, last] between the given vertices; none when OCCT cannot make it.
    [[nodiscard]] std::optional<TopoDS_Edge>
    edge(const TopoDS_Vertex& start, const TopoDS_Vertex& end, double first, double last) const;

    struct Impl;

  private:
    explicit SpaceCurve(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}
    std::shared_ptr<const Impl> impl_;
};

// A full circle edge (OCCT's own circle parameterisation from the frame's x axis).
[[nodiscard]] TopoDS_Edge circleEdge(const gp_Ax2& frame, double radius);
// The vertex of a sketch point.
[[nodiscard]] TopoDS_Vertex pointVertex(const gp_Pnt& point);
// A shared junction vertex with an explicit tolerance.
[[nodiscard]] TopoDS_Vertex junctionVertex(const gp_Pnt& point, double tolerance);
// OCCT's confusion tolerance (Precision::Confusion).
[[nodiscard]] double confusionTolerance() noexcept;
// Widens the tolerance of every vertex of the shape to `tolerance`.
void widenVertexTolerance(const TopoDS_Shape& shape, double tolerance);

// Connects edges one after another into a wire (BRepBuilderAPI_MakeWire).
class WireBuilder final {
  public:
    WireBuilder();
    explicit WireBuilder(const TopoDS_Edge& edge);
    // Adds the edge; false when it does not connect.
    bool add(const TopoDS_Edge& edge);
    // The edge as the wire holds it (it may copy an edge to share its vertices).
    [[nodiscard]] TopoDS_Edge lastEdge() const;
    [[nodiscard]] TopoDS_Wire wire() const;

  private:
    std::shared_ptr<BRepBuilderAPI_MakeWire> builder_;
};

// The edges as one closed wire, in the given order and orientation.
[[nodiscard]] TopoDS_Wire closedWire(std::span<const TopoDS_Edge> edges);
// The wire chained from edges given in any order; none when they do not form one chain.
[[nodiscard]] std::optional<TopoDS_Wire> chainedWire(std::span<const TopoDS_Edge> edges);
// The face inside a wire (in its own plane, or in `plane`); none when OCCT cannot make it.
[[nodiscard]] std::optional<TopoDS_Face> faceFromWire(const TopoDS_Wire& wire);
[[nodiscard]] std::optional<TopoDS_Face> faceFromWire(const gp_Pln& plane, const TopoDS_Wire& wire);
// The face with the wires added as holes; none when OCCT cannot make it.
[[nodiscard]] std::optional<TopoDS_Face> faceWithHoles(const TopoDS_Face& face,
                                                       std::span<const TopoDS_Wire> holes);
// The outer face with the hole face's outer loop as an inner wire, oriented so that the result
// is valid and its area is exactly outer minus hole; the outer face's orientation is kept.
[[nodiscard]] std::optional<TopoDS_Face> faceWithHoleFace(const TopoDS_Face& outer,
                                                          const TopoDS_Face& hole);
[[nodiscard]] TopoDS_Wire outerWire(const TopoDS_Face& face);
[[nodiscard]] bool isValidShape(const TopoDS_Shape& shape);
[[nodiscard]] double faceArea(const TopoDS_Face& face);
// `actual` lies on the same curve object, at the same place and over the same stored range as
// `source` (a wire builder kept the analytic parameters).
[[nodiscard]] bool sameEdgeCurve(const TopoDS_Edge& source, const TopoDS_Edge& actual);

} // namespace voelund::kernel
