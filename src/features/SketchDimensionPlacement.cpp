// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/features/SketchConstraintAnnotations.hpp>

#include <algorithm>

namespace voelund::features {

std::optional<double> sketchDimensionPositionAt(document::SketchParameters sketch,
                                                document::SketchConstraintId id,
                                                const document::SketchPoint& pointer) {
    // Measured against the automatic place: the dimension's own drag must not move its frame.
    auto& constraints = sketch.constraints.values;
    const auto constraint = std::ranges::find(constraints, id, &document::SketchConstraint::id);
    if (constraint == constraints.end())
        return std::nullopt;
    constraint->dimensionPosition.reset();
    const auto annotations = evaluateSketchConstraintAnnotations(sketch);
    const auto annotation = std::ranges::find(annotations, id, &SketchConstraintAnnotation::id);
    if (annotation == annotations.end() || !annotation->dimension)
        return std::nullopt;
    const auto& shape = *annotation->dimension;
    const auto& plane = sketch.plane;
    const gp_Dir normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Dir x(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Pnt at =
        gp_Pnt(plane.originX, plane.originY, plane.originZ)
            .Translated(gp_Vec(x) * pointer.x + gp_Vec(normal.Crossed(x)) * pointer.y);
    const gp_Vec fromVertex(shape.vertex, at);
    switch (shape.style) {
    case SketchDimensionStyle::linear:
        return fromVertex.Dot(gp_Vec(shape.outward)); // the vertex is the measured middle
    case SketchDimensionStyle::angle:
        return std::max(fromVertex.Magnitude(), 1e-3);
    default:
        return std::nullopt; // radius and diameter keep their place
    }
}

} // namespace voelund::features
