// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "OcctGuard.hpp"

#include <voelund/kernel/PlanarCurve.hpp>
#include <voelund/kernel/SketchShapes.hpp>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_Line.hxx>
#include <NCollection_Array1.hxx>
#include <Precision.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <gp_Circ.hxx>

#include <algorithm>
#include <cmath>
#include <vector>

namespace voelund::kernel {

struct SpaceCurve::Impl final {
    Handle(Geom_Curve) curve;
};

namespace {
constexpr auto operation = KernelOperation::sketchGeometry;

std::shared_ptr<const SpaceCurve::Impl> holding(Handle(Geom_Curve) curve) {
    auto impl = std::make_shared<SpaceCurve::Impl>();
    impl->curve = std::move(curve);
    return impl;
}
} // namespace

SpaceCurve SpaceCurve::line(const gp_Pnt& start, const gp_Pnt& through) {
    return detail::guarded(operation, [&] {
        return SpaceCurve(holding(new Geom_Line(gp_Ax1(start, gp_Dir(gp_Vec(start, through))))));
    });
}

SpaceCurve SpaceCurve::circle(const gp_Ax2& frame, double radius) {
    return detail::guarded(operation,
                           [&] { return SpaceCurve(holding(new Geom_Circle(frame, radius))); });
}

SpaceCurve SpaceCurve::ellipse(const gp_Ax2& frame, double majorRadius, double minorRadius) {
    return detail::guarded(operation, [&] {
        return SpaceCurve(holding(new Geom_Ellipse(frame, majorRadius, minorRadius)));
    });
}

SpaceCurve SpaceCurve::bspline(std::span<const gp_Pnt> poles) {
    return detail::guarded(operation, [&] {
        const int count = static_cast<int>(poles.size());
        const int degree = bsplineDegree(poles.size());
        NCollection_Array1<gp_Pnt> points(1, count);
        for (int index = 0; index < count; ++index)
            points.SetValue(index + 1, poles[static_cast<std::size_t>(index)]);
        const int spans = static_cast<int>(bsplineLastParameter(poles.size()));
        NCollection_Array1<double> knots(1, spans + 1);
        NCollection_Array1<int> multiplicities(1, spans + 1);
        for (int index = knots.Lower(); index <= knots.Upper(); ++index) {
            knots.SetValue(index, index - knots.Lower());
            multiplicities.SetValue(
                index, index == knots.Lower() || index == knots.Upper() ? degree + 1 : 1);
        }
        return SpaceCurve(holding(new Geom_BSplineCurve(points, knots, multiplicities, degree)));
    });
}

gp_Pnt SpaceCurve::value(double parameter) const {
    return detail::guarded(operation, [&] { return impl_->curve->Value(parameter); });
}

TopoDS_Edge SpaceCurve::edge() const {
    return detail::guarded(operation, [&] { return BRepBuilderAPI_MakeEdge(impl_->curve).Edge(); });
}

TopoDS_Edge SpaceCurve::edge(double first, double last) const {
    return detail::guarded(
        operation, [&] { return BRepBuilderAPI_MakeEdge(impl_->curve, first, last).Edge(); });
}

std::optional<TopoDS_Edge> SpaceCurve::edge(const TopoDS_Vertex& start, const TopoDS_Vertex& end,
                                            double first, double last) const {
    return detail::guarded(operation, [&]() -> std::optional<TopoDS_Edge> {
        BRepBuilderAPI_MakeEdge make(impl_->curve, start, end, first, last);
        if (!make.IsDone())
            return std::nullopt;
        return make.Edge();
    });
}

TopoDS_Edge circleEdge(const gp_Ax2& frame, double radius) {
    return detail::guarded(operation,
                           [&] { return BRepBuilderAPI_MakeEdge(gp_Circ(frame, radius)).Edge(); });
}

TopoDS_Vertex pointVertex(const gp_Pnt& point) {
    return detail::guarded(operation, [&] { return BRepBuilderAPI_MakeVertex(point).Vertex(); });
}

TopoDS_Vertex junctionVertex(const gp_Pnt& point, double tolerance) {
    return detail::guarded(operation, [&] {
        TopoDS_Vertex vertex;
        BRep_Builder().MakeVertex(vertex, point, tolerance);
        return vertex;
    });
}

double confusionTolerance() noexcept { return Precision::Confusion(); }

void widenVertexTolerance(const TopoDS_Shape& shape, double tolerance) {
    detail::guarded(operation, [&] {
        BRep_Builder builder;
        for (TopExp_Explorer vertex(shape, TopAbs_VERTEX); vertex.More(); vertex.Next())
            builder.UpdateVertex(TopoDS::Vertex(vertex.Current()), tolerance);
    });
}

WireBuilder::WireBuilder() : builder_(std::make_shared<BRepBuilderAPI_MakeWire>()) {}

WireBuilder::WireBuilder(const TopoDS_Edge& edge)
    : builder_(detail::guarded(operation,
                               [&] { return std::make_shared<BRepBuilderAPI_MakeWire>(edge); })) {}

bool WireBuilder::add(const TopoDS_Edge& edge) {
    return detail::guarded(operation, [&] {
        builder_->Add(edge);
        return builder_->IsDone();
    });
}

TopoDS_Edge WireBuilder::lastEdge() const {
    return detail::guarded(operation, [&] { return builder_->Edge(); });
}

TopoDS_Wire WireBuilder::wire() const {
    return detail::guarded(operation, [&] { return builder_->Wire(); });
}

TopoDS_Wire closedWire(std::span<const TopoDS_Edge> edges) {
    return detail::guarded(operation, [&] {
        BRep_Builder builder;
        TopoDS_Wire wire;
        builder.MakeWire(wire);
        for (const auto& edge : edges)
            builder.Add(wire, edge);
        wire.Closed(true);
        return wire;
    });
}

namespace {
// Adds every unused edge that touches the wire so far; true when one was added.
bool addTouching(BRepBuilderAPI_MakeWire& wire, std::span<const TopoDS_Edge> edges,
                 std::vector<bool>& used) {
    bool added = false;
    for (std::size_t index = 0; index < edges.size(); ++index) {
        if (used[index])
            continue;
        wire.Add(edges[index]);
        if (wire.Error() == BRepBuilderAPI_WireDone)
            used[index] = added = true;
    }
    return added;
}
} // namespace

std::optional<TopoDS_Wire> chainedWire(std::span<const TopoDS_Edge> edges) {
    return detail::guarded(operation, [&]() -> std::optional<TopoDS_Wire> {
        // The chain in any order: each pass adds the edges that touch the wire so far.
        BRepBuilderAPI_MakeWire wire;
        std::vector<bool> used(edges.size(), false);
        while (addTouching(wire, edges, used)) {
        }
        if (std::ranges::find(used, false) != used.end() || !wire.IsDone())
            return std::nullopt;
        return wire.Wire();
    });
}

std::optional<TopoDS_Face> faceFromWire(const TopoDS_Wire& wire) {
    return detail::guarded(operation, [&]() -> std::optional<TopoDS_Face> {
        BRepBuilderAPI_MakeFace face(wire);
        if (!face.IsDone())
            return std::nullopt;
        return face.Face();
    });
}

std::optional<TopoDS_Face> faceFromWire(const gp_Pln& plane, const TopoDS_Wire& wire) {
    return detail::guarded(operation, [&]() -> std::optional<TopoDS_Face> {
        BRepBuilderAPI_MakeFace face(plane, wire);
        if (!face.IsDone())
            return std::nullopt;
        return face.Face();
    });
}

std::optional<TopoDS_Face> faceWithHoles(const TopoDS_Face& face,
                                         std::span<const TopoDS_Wire> holes) {
    return detail::guarded(operation, [&]() -> std::optional<TopoDS_Face> {
        BRepBuilderAPI_MakeFace result(face);
        for (const auto& hole : holes)
            result.Add(hole);
        if (!result.IsDone())
            return std::nullopt;
        return result.Face();
    });
}

std::optional<TopoDS_Face> faceWithHoleFace(const TopoDS_Face& outer, const TopoDS_Face& hole) {
    return detail::guarded(operation, [&]() -> std::optional<TopoDS_Face> {
        const double expected = faceArea(outer) - faceArea(hole);
        const TopoDS_Wire wire = BRepTools::OuterWire(hole);
        for (const bool reversed : {true, false}) {
            BRepBuilderAPI_MakeFace face(outer);
            face.Add(reversed ? TopoDS::Wire(wire.Reversed()) : wire);
            if (!face.IsDone())
                continue;
            // MakeFace rebuilds a FORWARD face; keep the loop face's orientation for the prism.
            TopoDS_Face result = face.Face();
            result.Orientation(outer.Orientation());
            if (BRepCheck_Analyzer(result).IsValid() &&
                std::abs(faceArea(result) - expected) <= 1e-9 * std::max(1.0, faceArea(outer)))
                return result;
        }
        return std::nullopt;
    });
}

TopoDS_Wire outerWire(const TopoDS_Face& face) {
    return detail::guarded(operation, [&] { return BRepTools::OuterWire(face); });
}

bool isValidShape(const TopoDS_Shape& shape) {
    return detail::guarded(operation, [&] { return BRepCheck_Analyzer(shape).IsValid(); });
}

double faceArea(const TopoDS_Face& face) {
    return detail::guarded(operation, [&] {
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(face, properties);
        return properties.Mass();
    });
}

bool sameEdgeCurve(const TopoDS_Edge& source, const TopoDS_Edge& actual) {
    TopLoc_Location sourceLocation, actualLocation;
    double first = 0, last = 0, actualFirst = 0, actualLast = 0;
    const auto& curve = BRep_Tool::Curve(source, sourceLocation, first, last);
    const auto& mapped = BRep_Tool::Curve(actual, actualLocation, actualFirst, actualLast);
    return !curve.IsNull() && curve == mapped && sourceLocation.IsEqual(actualLocation) &&
           first == actualFirst && last == actualLast;
}

} // namespace voelund::kernel
