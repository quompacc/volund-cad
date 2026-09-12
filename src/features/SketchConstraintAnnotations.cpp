#include <voelund/features/SketchConstraintAnnotations.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <gp_Dir.hxx>
#include <gp_Vec.hxx>

namespace voelund::features {
namespace {

const document::SketchElement& findElement(const document::SketchParameters& sketch,
                                           document::SketchElementId id) {
    const auto match = std::ranges::find(sketch.elements.values, id, &document::SketchElement::id);
    if (match == sketch.elements.values.end())
        throw std::runtime_error("Sketch constraint references an unknown element");
    return *match;
}

document::SketchPoint referencePoint(const document::SketchParameters& sketch,
                                     const document::SketchGeometryReference& reference) {
    const auto& geometry = findElement(sketch, reference.element).geometry;
    if (const auto* line = std::get_if<document::SketchLine>(&geometry))
        return reference.point == document::SketchPointPosition::end ? line->end : line->start;
    if (const auto* circle = std::get_if<document::SketchCircle>(&geometry))
        return circle->center;
    if (const auto* arc = std::get_if<document::SketchArc>(&geometry)) {
        if (reference.point == document::SketchPointPosition::center)
            return arc->center;
        const double angle = reference.point == document::SketchPointPosition::start
                                 ? arc->startAngle
                                 : arc->endAngle;
        return {arc->center.x + arc->radius * std::cos(angle),
                arc->center.y + arc->radius * std::sin(angle)};
    }
    if (const auto* ellipse = std::get_if<document::SketchEllipse>(&geometry))
        return ellipse->center;
    const auto& rectangle = std::get<document::SketchRectangle>(geometry);
    if (reference.point == document::SketchPointPosition::center)
        return {(rectangle.first.x + rectangle.second.x) * 0.5,
                (rectangle.first.y + rectangle.second.y) * 0.5};
    return reference.point == document::SketchPointPosition::end ? rectangle.second
                                                                 : rectangle.first;
}

document::SketchPoint annotationPoint(const document::SketchParameters& sketch,
                                      const document::SketchConstraint& constraint) {
    if (constraint.kind == document::SketchConstraintKind::pointOnObject ||
        constraint.kind == document::SketchConstraintKind::midpoint)
        return referencePoint(sketch, constraint.first);
    if (constraint.kind == document::SketchConstraintKind::perpendicularBisector) {
        const auto& axis =
            std::get<document::SketchLine>(findElement(sketch, constraint.first.element).geometry);
        return {(axis.start.x + axis.end.x) * 0.5, (axis.start.y + axis.end.y) * 0.5};
    }
    const auto& geometry = findElement(sketch, constraint.first.element).geometry;
    if (!constraint.second.has_value()) {
        if (const auto* line = std::get_if<document::SketchLine>(&geometry))
            return {(line->start.x + line->end.x) * 0.5, (line->start.y + line->end.y) * 0.5};
        if (const auto* circle = std::get_if<document::SketchCircle>(&geometry))
            return {circle->center.x + circle->radius, circle->center.y};
        if (const auto* arc = std::get_if<document::SketchArc>(&geometry))
            return {arc->center.x + arc->radius * std::cos(arc->startAngle),
                    arc->center.y + arc->radius * std::sin(arc->startAngle)};
    }
    const document::SketchPoint first = referencePoint(sketch, constraint.first);
    if (!constraint.second.has_value())
        return first;
    const document::SketchPoint second = referencePoint(sketch, *constraint.second);
    return {(first.x + second.x) * 0.5, (first.y + second.y) * 0.5};
}

gp_Pnt worldPoint(const document::SketchPlane& plane, const document::SketchPoint& point) {
    const gp_Dir normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Dir xDirection(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Dir yDirection = normal.Crossed(xDirection);
    constexpr double displayOffset = 0.02;
    return gp_Pnt(plane.originX, plane.originY, plane.originZ)
        .Translated(gp_Vec(xDirection) * point.x + gp_Vec(yDirection) * point.y +
                    gp_Vec(normal) * displayOffset);
}

} // namespace

std::vector<SketchConstraintAnnotation>
evaluateSketchConstraintAnnotations(const document::SketchParameters& sketch) {
    std::vector<SketchConstraintAnnotation> result;
    result.reserve(sketch.constraints.values.size());
    for (const document::SketchConstraint& constraint : sketch.constraints.values) {
        result.push_back({constraint.id, constraint.kind,
                          worldPoint(sketch.plane, annotationPoint(sketch, constraint)),
                          constraint.value});
    }
    return result;
}

} // namespace voelund::features
