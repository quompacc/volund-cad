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

} // namespace

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
