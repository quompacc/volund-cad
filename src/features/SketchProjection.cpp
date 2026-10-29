// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/FilletEdges.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchProjection.hpp>

#include <TopoDS.hxx>
#include <gp_Vec.hxx>
#include <voelund/kernel/EdgeQueries.hpp>
#include <voelund/kernel/PlaneSection.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace voelund::features {
namespace {

constexpr double tolerance = 1e-7;

struct Frame final {
    gp_Pnt origin;
    gp_Vec normal, x, y;
};

Frame frameOf(const document::SketchPlane& plane) {
    const gp_Vec normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Vec x(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    return {gp_Pnt(plane.originX, plane.originY, plane.originZ), normal, x, normal.Crossed(x)};
}

document::SketchPoint local(const Frame& frame, const gp_Pnt& point) {
    const gp_Vec offset(frame.origin, point);
    return {offset.Dot(frame.x), offset.Dot(frame.y)};
}

// N15a: an ellipse in the plane; seen from the other side its minor axis turns over, so the
// parameters change sign.
document::SketchGeometry sketchEllipse(const Frame& frame, const kernel::EdgeEllipse& curve) {
    const gp_Elips& e = curve.ellipse;
    const gp_Vec major(e.XAxis().Direction());
    const double rotation = std::atan2(major.Dot(frame.y), major.Dot(frame.x));
    const bool facing = gp_Vec(e.Axis().Direction()).Dot(frame.normal) > 0.0;
    const document::SketchEllipse ellipse{local(frame, e.Location()), e.MajorRadius(),
                                          e.MinorRadius(), rotation};
    if (curve.last - curve.first >= 2.0 * std::numbers::pi - 1e-9)
        return ellipse;
    const double start = facing ? curve.first : -curve.last;
    const double end = facing ? curve.last : -curve.first;
    return document::SketchEllipticArc{
        ellipse.center, ellipse.majorRadius, ellipse.minorRadius, ellipse.rotation, start, end};
}

// N15a: any other section curve as cubic B-splines in the plane; a closed one in two halves,
// since a sketch spline has two distinct ends.
void appendSplines(const Frame& frame, const TopoDS_Edge& edge,
                   std::vector<document::SketchGeometry>& curves) {
    const auto range = kernel::storedRange(edge);
    const bool closed =
        kernel::pointOnEdge(edge, range.first).Distance(kernel::pointOnEdge(edge, range.last)) <
        tolerance;
    const double middle = (range.first + range.last) / 2.0;
    const std::vector<TopoDS_Edge> pieces =
        closed ? std::vector{kernel::edgeOnSameCurve(edge, range.first, middle),
                             kernel::edgeOnSameCurve(edge, middle, range.last)}
               : std::vector{edge};
    for (const auto& piece : pieces) {
        const auto spline =
            kernel::cubicSplineOfEdge(piece, 1e-4, static_cast<int>(document::maximumBSplinePoles));
        if (!spline)
            throw GeometryEvaluationError("A section curve is too complex for a sketch spline",
                                          "section-unsupported");
        document::SketchBSpline curve;
        for (const auto& pole : spline->poles)
            curve.poles.push_back(local(frame, pole));
        curve.knots = spline->knots;
        curves.emplace_back(std::move(curve));
    }
}

} // namespace

std::vector<document::SketchGeometry> sectionSketchCurves(const document::SketchPlane& plane,
                                                          const TopoDS_Shape& body) {
    const Frame frame = frameOf(plane);
    std::vector<document::SketchGeometry> curves;
    const gp_Pln cut(frame.origin, gp_Dir(frame.normal));
    for (const auto& edge : kernel::planeSectionEdges(body, cut)) {
        if (auto projected = projectSketchEdge(plane, edge))
            curves.push_back(std::move(*projected));
        else if (const auto ellipse = kernel::edgeEllipse(edge))
            curves.push_back(sketchEllipse(frame, *ellipse));
        else
            appendSplines(frame, edge, curves);
    }
    return curves;
}

std::optional<document::SketchGeometry> projectSketchEdge(const document::SketchPlane& plane,
                                                          const TopoDS_Shape& edge) {
    if (edge.IsNull() || edge.ShapeType() != TopAbs_EDGE)
        return std::nullopt;
    const Frame frame = frameOf(plane);
    const auto curve = kernel::describeEdge(TopoDS::Edge(edge));
    const double first = curve.range.first, last = curve.range.last;
    if (curve.kind == kernel::EdgeCurve::Kind::line) {
        const auto start = local(frame, curve.start);
        const auto end = local(frame, curve.end);
        if (std::hypot(end.x - start.x, end.y - start.y) < tolerance)
            return std::nullopt; // the edge stands across the plane
        return document::SketchLine{start, end};
    }
    if (curve.kind != kernel::EdgeCurve::Kind::circle)
        return std::nullopt;
    const gp_Circ circle = curve.circle;
    const double along = gp_Vec(circle.Axis().Direction()).Dot(frame.normal);
    if (std::abs(std::abs(along) - 1.0) > 1e-9)
        return std::nullopt; // an ellipse in the sketch, which this projection does not make
    const auto center = local(frame, circle.Location());
    if (last - first >= 2.0 * std::numbers::pi - 1e-9)
        return document::SketchCircle{center, circle.Radius()};
    // The arc runs counterclockwise in the sketch: against it when the circle faces away.
    auto from = local(frame, curve.start);
    auto to = local(frame, curve.end);
    if (along < 0.0)
        std::swap(from, to);
    const double start = std::atan2(from.y - center.y, from.x - center.x);
    double end = std::atan2(to.y - center.y, to.x - center.x);
    while (end <= start)
        end += 2.0 * std::numbers::pi;
    return document::SketchArc{center, circle.Radius(), start, end};
}

namespace detail {

void refreshSketchProjections(document::SketchParameters& sketch,
                              const document::DocumentSnapshot& document,
                              document::FeatureId sketchId,
                              const std::vector<EvaluatedFeature>& outputs) {
    for (const auto& projection : sketch.projections) {
        const auto missing = [&](const char* message, const char* code) {
            throw GeometryEvaluationError(message, code, {projection.element.value()});
        };
        const auto producer = bodyProducerBefore(document, sketchId, projection.body);
        if (!producer)
            missing("The body of a projected edge has no solid before the sketch",
                    "projection-missing");
        const auto edges =
            filletEdgeShapes(findFeature(outputs, *producer).supportOrigins, projection.edge);
        if (edges.empty())
            missing("A projected edge is no longer on its body", "projection-missing");
        auto geometry = projectSketchEdge(sketch.plane, edges.front());
        if (!geometry)
            missing("A projected edge cannot be drawn in the sketch plane",
                    "projection-unsupported");
        const auto element = std::ranges::find(sketch.elements.values, projection.element,
                                               &document::SketchElement::id);
        element->geometry = std::move(*geometry);
    }
}

} // namespace detail
} // namespace voelund::features
