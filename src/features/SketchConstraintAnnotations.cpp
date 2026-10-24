// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchPlaneGuard.hpp"
#include <voelund/features/SketchConstraintAnnotations.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <optional>
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
    if (const auto* point = std::get_if<document::SketchPointGeometry>(&geometry))
        return point->position;                                               // S15
    if (const auto* spline = std::get_if<document::SketchBSpline>(&geometry)) // S21
        return reference.point == Position::end ? spline->poles.back() : spline->poles.front();
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
    if (const auto* point = std::get_if<document::SketchPointGeometry>(&geometry))
        return point->position;
    if (const auto* spline = std::get_if<document::SketchBSpline>(&geometry))
        return spline->poles[spline->poles.size() / 2];
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
    case SketchConstraintKind::horizontal:
        return "[H]";
    case SketchConstraintKind::vertical:
        return "[V]";
    case SketchConstraintKind::coincident:
        return "[C]";
    case SketchConstraintKind::parallel:
        return "[||]";
    case SketchConstraintKind::perpendicular:
        return "[90]";
    case SketchConstraintKind::collinear:
        return "[CL]";
    case SketchConstraintKind::horizontalPoints:
        return "[HP]";
    case SketchConstraintKind::verticalPoints:
        return "[VP]";
    case SketchConstraintKind::fixed:
        return "[F]";
    case SketchConstraintKind::pointOnObject:
        return "[O]";
    case SketchConstraintKind::equal:
        return "[=]";
    case SketchConstraintKind::tangent:
        return "[T]";
    case SketchConstraintKind::symmetric:
        return "[S]";
    case SketchConstraintKind::midpoint:
        return "[M]";
    case SketchConstraintKind::perpendicularBisector:
        return "[PB]";
    case SketchConstraintKind::concentric:
        return "[CC]";
    case SketchConstraintKind::distance:
        return "L";
    case SketchConstraintKind::radius:
        return "R";
    case SketchConstraintKind::diameter:
        return "Ø";
    case SketchConstraintKind::pointDistance:
        return "D";
    case SketchConstraintKind::pointLineDistance:
        return "PL";
    case SketchConstraintKind::pointDistanceX:
        return "DX";
    case SketchConstraintKind::pointDistanceY:
        return "DY";
    case SketchConstraintKind::coordinateX:
        return "X";
    case SketchConstraintKind::coordinateY:
        return "Y";
    case SketchConstraintKind::angle:
        return "A";
    }
    return "?";
}

// W12: a dimension's value as a drafter writes it: up to two decimals, no trailing zeros.
std::string compactNumber(double value) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(2) << value;
    std::string result = text.str();
    while (result.ends_with('0'))
        result.pop_back();
    if (result.ends_with('.'))
        result.pop_back();
    return result == "-0" ? "0" : result;
}

// W12: how far the dimension lines stand off the geometry: a share of the sketch's size.
double dimensionOffset(const document::SketchParameters& sketch) {
    double low[2] = {0.0, 0.0}, high[2] = {0.0, 0.0};
    for (const auto& element : sketch.elements.values) {
        for (const auto position : {Position::start, Position::end, Position::center}) {
            const SketchPoint point = referencePoint(sketch, {element.id, position});
            low[0] = std::min(low[0], point.x), high[0] = std::max(high[0], point.x);
            low[1] = std::min(low[1], point.y), high[1] = std::max(high[1], point.y);
        }
    }
    return std::clamp(0.08 * std::hypot(high[0] - low[0], high[1] - low[1]), 1.5, 12.0);
}

// The middle of the sketch's geometry; dimensions stand off away from it.
SketchPoint sketchMiddle(const document::SketchParameters& sketch) {
    SketchPoint sum{0.0, 0.0};
    int count = 0;
    for (const auto& element : sketch.elements.values) {
        const SketchPoint point = elementPoint(sketch, element.id);
        sum.x += point.x, sum.y += point.y;
        ++count;
    }
    return count == 0 ? sum : SketchPoint{sum.x / count, sum.y / count};
}

gp_Dir worldDirection(const document::SketchPlane& plane, double x, double y) {
    const gp_Dir normal(plane.normalX, plane.normalY, plane.normalZ);
    const gp_Dir xDirection(plane.xDirectionX, plane.xDirectionY, plane.xDirectionZ);
    const gp_Dir yDirection = normal.Crossed(xDirection);
    return gp_Dir(gp_Vec(xDirection) * x + gp_Vec(yDirection) * y);
}

struct Measured final {
    SketchPoint first;
    SketchPoint second;
    std::optional<SketchPoint> axis; // measured along this direction; else first to second
};

std::optional<Measured> lineMeasure(const document::SketchParameters& sketch,
                                    const document::SketchConstraint& constraint) {
    const auto& geometry = findElement(sketch, constraint.first.element).geometry;
    if (const auto* line = std::get_if<document::SketchLine>(&geometry))
        return Measured{line->start, line->end, std::nullopt};
    return std::nullopt;
}

// Two points apart along the sketch's X (true) or Y.
Measured axisMeasure(const document::SketchParameters& sketch,
                     const document::SketchConstraint& constraint, bool x) {
    return Measured{referencePoint(sketch, constraint.first),
                    referencePoint(sketch, *constraint.second),
                    x ? SketchPoint{1.0, 0.0} : SketchPoint{0.0, 1.0}};
}

Measured footMeasure(const document::SketchParameters& sketch,
                     const document::SketchConstraint& constraint) {
    const SketchPoint point = referencePoint(sketch, constraint.first);
    const auto& line =
        std::get<document::SketchLine>(findElement(sketch, constraint.second->element).geometry);
    return Measured{point, foot(line, point), std::nullopt};
}

// A coordinate: from the axis to the point, along X (true) or Y.
Measured coordinateMeasure(const document::SketchParameters& sketch,
                           const document::SketchConstraint& constraint, bool x) {
    const SketchPoint point = referencePoint(sketch, constraint.first);
    return Measured{x ? SketchPoint{0.0, point.y} : SketchPoint{point.x, 0.0}, point,
                    x ? SketchPoint{1.0, 0.0} : SketchPoint{0.0, 1.0}};
}

// The two points a linear dimension measures between.
std::optional<Measured> measuredPoints(const document::SketchParameters& sketch,
                                       const document::SketchConstraint& constraint) {
    switch (constraint.kind) {
    case SketchConstraintKind::distance:
        return lineMeasure(sketch, constraint);
    case SketchConstraintKind::pointDistance:
        return Measured{referencePoint(sketch, constraint.first),
                        referencePoint(sketch, *constraint.second), std::nullopt};
    case SketchConstraintKind::pointDistanceX:
    case SketchConstraintKind::pointDistanceY:
        return axisMeasure(sketch, constraint,
                           constraint.kind == SketchConstraintKind::pointDistanceX);
    case SketchConstraintKind::pointLineDistance:
        return footMeasure(sketch, constraint);
    case SketchConstraintKind::coordinateX:
    case SketchConstraintKind::coordinateY:
        return coordinateMeasure(sketch, constraint,
                                 constraint.kind == SketchConstraintKind::coordinateX);
    default:
        return std::nullopt;
    }
}

std::optional<SketchDimensionShape> linearShape(const document::SketchParameters& sketch,
                                                const Measured& measured, double offset) {
    SketchPoint along = measured.axis.value_or(
        SketchPoint{measured.second.x - measured.first.x, measured.second.y - measured.first.y});
    const double length = std::hypot(along.x, along.y);
    if (length < 1e-9)
        return std::nullopt;
    along = {along.x / length, along.y / length};
    SketchPoint outward{-along.y, along.x};
    const SketchPoint centre = middle(measured.first, measured.second);
    const SketchPoint inside = sketchMiddle(sketch);
    if ((centre.x - inside.x) * outward.x + (centre.y - inside.y) * outward.y < 0.0)
        outward = {-outward.x, -outward.y};
    const auto& plane = sketch.plane;
    const int beside = std::abs(along.y) > std::abs(along.x) ? (outward.x < 0 ? -1 : 1) : 0;
    return SketchDimensionShape{SketchDimensionStyle::linear,
                                worldPoint(plane, measured.first),
                                worldPoint(plane, measured.second),
                                worldPoint(plane, centre),
                                worldDirection(plane, along.x, along.y),
                                worldDirection(plane, outward.x, outward.y),
                                gp_Dir(plane.normalX, plane.normalY, plane.normalZ),
                                offset,
                                {},
                                beside};
}

std::optional<SketchDimensionShape> radialShape(const document::SketchParameters& sketch,
                                                const document::SketchConstraint& constraint,
                                                double offset) {
    const auto& geometry = findElement(sketch, constraint.first.element).geometry;
    const auto centre = centerOf(geometry);
    if (!centre)
        return std::nullopt;
    const SketchPoint onCurve = elementPoint(sketch, constraint.first.element);
    const auto& plane = sketch.plane;
    const bool diameter = constraint.kind == SketchConstraintKind::diameter;
    const SketchPoint across{2 * centre->x - onCurve.x, 2 * centre->y - onCurve.y};
    const double dx = onCurve.x - centre->x, dy = onCurve.y - centre->y;
    const double radius = std::max(std::hypot(dx, dy), 1e-9);
    return SketchDimensionShape{diameter ? SketchDimensionStyle::diameter
                                         : SketchDimensionStyle::radius,
                                worldPoint(plane, diameter ? across : *centre),
                                worldPoint(plane, onCurve),
                                worldPoint(plane, *centre),
                                worldDirection(plane, dx / radius, dy / radius),
                                worldDirection(plane, dx / radius, dy / radius),
                                gp_Dir(plane.normalX, plane.normalY, plane.normalZ),
                                offset,
                                {}};
}

// The legs of an angle: where the two lines meet and a point on each, away from the meeting.
std::optional<SketchDimensionShape> angleShape(const document::SketchParameters& sketch,
                                               const document::SketchConstraint& constraint,
                                               double offset) {
    const auto* a =
        std::get_if<document::SketchLine>(&findElement(sketch, constraint.first.element).geometry);
    const auto* b = std::get_if<document::SketchLine>(
        &findElement(sketch, constraint.second->element).geometry);
    if (a == nullptr || b == nullptr)
        return std::nullopt;
    const double ax = a->end.x - a->start.x, ay = a->end.y - a->start.y;
    const double bx = b->end.x - b->start.x, by = b->end.y - b->start.y;
    const double cross = ax * by - ay * bx;
    if (std::abs(cross) < 1e-12)
        return std::nullopt;
    const double t = ((b->start.x - a->start.x) * by - (b->start.y - a->start.y) * bx) / cross;
    const SketchPoint vertex{a->start.x + ax * t, a->start.y + ay * t};
    // Each leg points from the meeting to the line's farther end.
    const auto leg = [&](const document::SketchLine& line) {
        const SketchPoint far = std::hypot(line.start.x - vertex.x, line.start.y - vertex.y) >
                                        std::hypot(line.end.x - vertex.x, line.end.y - vertex.y)
                                    ? line.start
                                    : line.end;
        const double length = std::max(std::hypot(far.x - vertex.x, far.y - vertex.y), 1e-9);
        const double radius = 2.5 * offset;
        return SketchPoint{vertex.x + (far.x - vertex.x) / length * radius,
                           vertex.y + (far.y - vertex.y) / length * radius};
    };
    const auto& plane = sketch.plane;
    return SketchDimensionShape{SketchDimensionStyle::angle,
                                worldPoint(plane, leg(*a)),
                                worldPoint(plane, leg(*b)),
                                worldPoint(plane, vertex),
                                worldDirection(plane, 1.0, 0.0),
                                worldDirection(plane, 0.0, 1.0),
                                gp_Dir(plane.normalX, plane.normalY, plane.normalZ),
                                2.5 * offset,
                                {}};
}

std::string dimensionText(SketchConstraintKind kind, double value) {
    switch (kind) {
    case SketchConstraintKind::angle:
        return compactNumber(std::abs(kernel::toDegrees(kernel::Radians(value)).value())) + "°";
    case SketchConstraintKind::radius:
        return "R " + compactNumber(value) + " mm";
    case SketchConstraintKind::diameter:
        return "Ø " + compactNumber(value) + " mm";
    default:
        return compactNumber(std::abs(value)) + " mm";
    }
}

// A point on an axis keeps its short label; a dimension line of length zero shows nothing.
bool onAxis(const document::SketchConstraint& constraint, double value) {
    return (constraint.kind == SketchConstraintKind::coordinateX ||
            constraint.kind == SketchConstraintKind::coordinateY) &&
           std::abs(value) < 1e-9;
}

// W12: how the dimension is drawn; none for a relation or one without a drawing.
std::optional<SketchDimensionShape>
sketchDimensionShape(const document::SketchParameters& sketch,
                     const document::SketchConstraint& constraint, double value) {
    if (sketchAnnotationCategory(constraint.kind) != SketchAnnotationCategory::dimension)
        return std::nullopt;
    if (onAxis(constraint, value))
        return std::nullopt;
    const double offset = dimensionOffset(sketch);
    std::optional<SketchDimensionShape> shape;
    if (constraint.kind == SketchConstraintKind::angle)
        shape = angleShape(sketch, constraint, offset);
    else if (constraint.kind == SketchConstraintKind::radius ||
             constraint.kind == SketchConstraintKind::diameter)
        shape = radialShape(sketch, constraint, offset);
    else if (const auto measured = measuredPoints(sketch, constraint))
        shape = linearShape(sketch, *measured, offset);
    if (shape) {
        shape->text = dimensionText(constraint.kind, value);
        if (constraint.reference)
            shape->text = "(" + shape->text + ")";
    }
    return shape;
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
           constraint.auxiliaryValues.empty() && !constraint.reference;
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
                          constraint.reference // S18: a reference in parentheses
                              ? "(" + sketchConstraintLabel(constraint.kind, value) + ")"
                              : sketchConstraintLabel(constraint.kind, value),
                          sketchDimensionShape(sketch, constraint, value)});
    }
    return result;
}

} // namespace voelund::features
