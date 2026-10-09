#include "SketchPlaneGuard.hpp"
#include <voelund/features/SketchConstraintAnnotations.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <stdexcept>

#include <gp_Dir.hxx>
#include <gp_Vec.hxx>

namespace voelund::features {
namespace {

using document::SketchConstraintKind;
using document::SketchPoint;
using Position = document::SketchPointPosition;

const document::SketchElement& findElement(const document::SketchParameters& sketch,
                                           document::SketchElementId id) {
    const auto match = std::ranges::find(sketch.elements.values, id, &document::SketchElement::id);
    if (match == sketch.elements.values.end())
        throw std::runtime_error("Sketch constraint references an unknown element");
    return *match;
}

SketchPoint onCircle(SketchPoint center, double radius, double angle) {
    return {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
}
SketchPoint middle(SketchPoint a, SketchPoint b) { return {(a.x + b.x) * 0.5, (a.y + b.y) * 0.5}; }

SketchPoint referencePoint(const document::SketchParameters& sketch,
                           const document::SketchGeometryReference& reference) {
    const auto& geometry = findElement(sketch, reference.element).geometry;
    if (const auto* line = std::get_if<document::SketchLine>(&geometry))
        return reference.point == Position::end ? line->end : line->start;
    if (const auto* circle = std::get_if<document::SketchCircle>(&geometry))
        return circle->center;
    if (const auto* arc = std::get_if<document::SketchArc>(&geometry)) {
        if (reference.point == Position::center)
            return arc->center;
        return onCircle(arc->center, arc->radius,
                        reference.point == Position::start ? arc->startAngle : arc->endAngle);
    }
    if (const auto* ellipse = std::get_if<document::SketchEllipse>(&geometry))
        return ellipse->center;
    const auto& rectangle = std::get<document::SketchRectangle>(geometry);
    if (reference.point == Position::center)
        return middle(rectangle.first, rectangle.second);
    return reference.point == Position::end ? rectangle.second : rectangle.first;
}

// Where a label for the element as a whole sits: on its curve, not at an end point.
SketchPoint elementPoint(const document::SketchParameters& sketch, document::SketchElementId id) {
    const auto& geometry = findElement(sketch, id).geometry;
    if (const auto* line = std::get_if<document::SketchLine>(&geometry))
        return middle(line->start, line->end);
    if (const auto* circle = std::get_if<document::SketchCircle>(&geometry))
        return onCircle(circle->center, circle->radius, std::numbers::pi / 4);
    if (const auto* arc = std::get_if<document::SketchArc>(&geometry))
        return onCircle(arc->center, arc->radius, (arc->startAngle + arc->endAngle) * 0.5);
    if (const auto* ellipse = std::get_if<document::SketchEllipse>(&geometry))
        return onCircle(ellipse->center, ellipse->majorRadius, ellipse->rotation);
    const auto& rectangle = std::get<document::SketchRectangle>(geometry);
    return middle(rectangle.first, rectangle.second);
}

std::optional<SketchPoint> centerOf(const document::SketchGeometry& geometry) {
    if (const auto* circle = std::get_if<document::SketchCircle>(&geometry))
        return circle->center;
    if (const auto* arc = std::get_if<document::SketchArc>(&geometry))
        return arc->center;
    return std::nullopt;
}

SketchPoint foot(const document::SketchLine& line, SketchPoint point) {
    const double dx = line.end.x - line.start.x;
    const double dy = line.end.y - line.start.y;
    const double squared = dx * dx + dy * dy;
    const double t = squared > 0.0
                         ? ((point.x - line.start.x) * dx + (point.y - line.start.y) * dy) / squared
                         : 0.0;
    return {line.start.x + dx * t, line.start.y + dy * t};
}

// The point two curves share through a coincident relation between their ends, if any.
std::optional<SketchPoint> sharedEnd(const document::SketchParameters& sketch,
                                     document::SketchElementId first,
                                     document::SketchElementId second) {
    for (const auto& constraint : sketch.constraints.values)
        if (constraint.kind == SketchConstraintKind::coincident &&
            constraint.first.point != Position::center &&
            constraint.second->point != Position::center &&
            ((constraint.first.element == first && constraint.second->element == second) ||
             (constraint.first.element == second && constraint.second->element == first)))
            return referencePoint(sketch, constraint.first);
    return std::nullopt;
}

SketchPoint tangentPoint(const document::SketchParameters& sketch,
                         const document::SketchConstraint& constraint) {
    const auto firstId = constraint.first.element;
    const auto secondId = constraint.second->element;
    if (const auto joint = sharedEnd(sketch, firstId, secondId))
        return *joint;
    const auto& first = findElement(sketch, firstId).geometry;
    const auto& second = findElement(sketch, secondId).geometry;
    if (const auto* line = std::get_if<document::SketchLine>(&first))
        return foot(*line, *centerOf(second));
    if (const auto* line = std::get_if<document::SketchLine>(&second))
        return foot(*line, *centerOf(first));
    // Two circles touch on the line through their centers.
    const SketchPoint a = *centerOf(first);
    const SketchPoint b = *centerOf(second);
    const double radius = std::holds_alternative<document::SketchCircle>(first)
                              ? std::get<document::SketchCircle>(first).radius
                              : std::get<document::SketchArc>(first).radius;
    const double apart = std::hypot(b.x - a.x, b.y - a.y);
    if (apart <= 0.0)
        return elementPoint(sketch, firstId);
    return {a.x + (b.x - a.x) / apart * radius, a.y + (b.y - a.y) / apart * radius};
}

SketchPoint annotationPoint(const document::SketchParameters& sketch,
                            const document::SketchConstraint& constraint) {
    switch (constraint.kind) {
    case SketchConstraintKind::horizontal:
    case SketchConstraintKind::vertical:
    case SketchConstraintKind::distance:
    case SketchConstraintKind::radius:
    case SketchConstraintKind::diameter:
    case SketchConstraintKind::fixed:
    case SketchConstraintKind::parallel:
    case SketchConstraintKind::perpendicular:
    case SketchConstraintKind::collinear:
    case SketchConstraintKind::equal:
    case SketchConstraintKind::perpendicularBisector:
        return elementPoint(sketch, constraint.first.element);
    case SketchConstraintKind::angle:
        return middle(elementPoint(sketch, constraint.first.element),
                      elementPoint(sketch, constraint.second->element));
    case SketchConstraintKind::tangent:
        return tangentPoint(sketch, constraint);
    case SketchConstraintKind::concentric:
        return *centerOf(findElement(sketch, constraint.first.element).geometry);
    case SketchConstraintKind::pointOnObject:
    case SketchConstraintKind::midpoint:
        return referencePoint(sketch, constraint.first);
    case SketchConstraintKind::pointLineDistance: {
        const SketchPoint point = referencePoint(sketch, constraint.first);
        const auto& line = std::get<document::SketchLine>(
            findElement(sketch, constraint.second->element).geometry);
        return middle(point, foot(line, point));
    }
    default:
        break;
    }
    const SketchPoint first = referencePoint(sketch, constraint.first);
    if (!constraint.second.has_value())
        return first;
    return middle(first, referencePoint(sketch, *constraint.second));
}

gp_Pnt worldPoint(const document::SketchPlane& plane, const SketchPoint& point) {
    const gp_Dir normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Dir xDirection(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Dir yDirection = normal.Crossed(xDirection);
    constexpr double displayOffset = 0.02;
    return gp_Pnt(plane.originX, plane.originY, plane.originZ)
        .Translated(gp_Vec(xDirection) * point.x + gp_Vec(yDirection) * point.y +
                    gp_Vec(normal) * displayOffset);
}

const char* symbol(SketchConstraintKind kind) {
    switch (kind) {
    case SketchConstraintKind::horizontal: return "[H]";
    case SketchConstraintKind::vertical: return "[V]";
    case SketchConstraintKind::coincident: return "[C]";
    case SketchConstraintKind::parallel: return "[||]";
    case SketchConstraintKind::perpendicular: return "[90]";
    case SketchConstraintKind::collinear: return "[CL]";
    case SketchConstraintKind::horizontalPoints: return "[HP]";
    case SketchConstraintKind::verticalPoints: return "[VP]";
    case SketchConstraintKind::fixed: return "[F]";
    case SketchConstraintKind::pointOnObject: return "[O]";
    case SketchConstraintKind::equal: return "[=]";
    case SketchConstraintKind::tangent: return "[T]";
    case SketchConstraintKind::symmetric: return "[S]";
    case SketchConstraintKind::midpoint: return "[M]";
    case SketchConstraintKind::perpendicularBisector: return "[PB]";
    case SketchConstraintKind::concentric: return "[CC]";
    case SketchConstraintKind::distance: return "L";
    case SketchConstraintKind::radius: return "R";
    case SketchConstraintKind::diameter: return "Ø";
    case SketchConstraintKind::pointDistance: return "D";
    case SketchConstraintKind::pointLineDistance: return "PL";
    case SketchConstraintKind::pointDistanceX: return "DX";
    case SketchConstraintKind::pointDistanceY: return "DY";
    case SketchConstraintKind::coordinateX: return "X";
    case SketchConstraintKind::coordinateY: return "Y";
    case SketchConstraintKind::angle: return "A";
    }
    return "?";
}

} // namespace

SketchAnnotationCategory sketchAnnotationCategory(SketchConstraintKind kind) noexcept {
    switch (kind) {
    case SketchConstraintKind::distance:
    case SketchConstraintKind::radius:
    case SketchConstraintKind::diameter:
    case SketchConstraintKind::pointDistance:
    case SketchConstraintKind::pointLineDistance:
    case SketchConstraintKind::pointDistanceX:
    case SketchConstraintKind::pointDistanceY:
    case SketchConstraintKind::coordinateX:
    case SketchConstraintKind::coordinateY:
    case SketchConstraintKind::angle:
        return SketchAnnotationCategory::dimension;
    default:
        return SketchAnnotationCategory::relation;
    }
}

bool isEditableDimension(const document::SketchConstraint& constraint) noexcept {
    return sketchAnnotationCategory(constraint.kind) == SketchAnnotationCategory::dimension &&
           !constraint.parameter.has_value() && !constraint.projectedEdge.has_value() &&
           constraint.auxiliaryValues.empty();
}

std::string sketchConstraintLabel(SketchConstraintKind kind, double value) {
    if (sketchAnnotationCategory(kind) == SketchAnnotationCategory::relation)
        return symbol(kind);
    std::ostringstream label;
    label << symbol(kind) << ' ' << std::fixed << std::setprecision(3);
    if (kind == SketchConstraintKind::angle)
        label << kernel::toDegrees(kernel::Radians(value)).value() << "°";
    else
        label << value << " mm";
    return label.str();
}

std::vector<SketchConstraintAnnotation>
evaluateSketchConstraintAnnotations(const document::SketchParameters& sketch) {
    detail::requireCanonicalPlane(sketch.plane);
    std::vector<SketchConstraintAnnotation> result;
    result.reserve(sketch.constraints.values.size());
    for (const document::SketchConstraint& constraint : sketch.constraints.values) {
        const auto category = sketchAnnotationCategory(constraint.kind);
        const double value = category == SketchAnnotationCategory::relation ? 0.0
                             : constraint.kind == SketchConstraintKind::angle
                                 ? constraint.angleValue.value()
                                 : constraint.value;
        result.push_back({constraint.id, constraint.kind,
                          worldPoint(sketch.plane, annotationPoint(sketch, constraint)), value,
                          category, isEditableDimension(constraint), constraint.parameter,
                          sketchConstraintLabel(constraint.kind, value)});
    }
    return result;
}

} // namespace voelund::features
